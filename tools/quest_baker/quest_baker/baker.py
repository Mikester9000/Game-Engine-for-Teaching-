"""
baker.py — QuestBaker and DialogueBaker implementation for M20.

TEACHING NOTE — What is a "baker"?
====================================
A *baker* is a tool that converts *source* data (human-authored JSON) into
*cooked* data (validated, runtime-ready JSON or binary).  This mirrors the
asset pipeline used in commercial engines:

  Unreal Engine  — DerivedDataCache (DDC) bakes meshes, textures, blueprints.
  Unity          — Import pipeline bakes FBX → Mesh, PNG → Texture2D.
  This engine    — quest_baker bakes quest_bank.json → quest_bank.cooked.json.

Baking catches data errors *at cook time*, not at runtime.  If a quest
references a prerequisite quest that does not exist in the bank, the baker
raises a BakeError before the game ever runs.

TEACHING NOTE — Separation of Validation and Transformation
=============================================================
Every baker follows this pattern:

  1. Load  → read raw JSON from disk.
  2. Validate  → check schema, references, and business rules.
  3. Transform → reorganise data for efficient runtime access.
  4. Write  → serialise cooked JSON to the Cooked/ directory.

We keep validation (steps 1–2) separate from transformation (step 3) so that
a failed validation short-circuits before any writes occur.
"""

from __future__ import annotations

import json
import hashlib
import os
import tempfile
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional


# ---------------------------------------------------------------------------
# BakeResult — returned by every baker method
# ---------------------------------------------------------------------------

@dataclass
class BakeResult:
    """
    Summary returned after a baking operation.

    TEACHING NOTE — Result objects vs. exceptions
    ===============================================
    We use a BakeResult *in addition to* exceptions.  Exceptions propagate
    hard failures (malformed JSON, missing files).  BakeResult.errors is a
    list of *soft* validation failures — things that are structurally valid
    JSON but break game rules (e.g. a quest with no objectives).

    This separation lets callers decide whether to abort on any warning
    or only on errors.

    Attributes
    ----------
    source_path : Path
        Absolute path of the source JSON that was baked.
    cooked_path : Path
        Absolute path where the cooked JSON was written.
    quest_count : int
        Number of quests processed (QuestBaker) or 0 (DialogueBaker).
    node_count : int
        Number of dialogue nodes processed (DialogueBaker) or 0 (QuestBaker).
    errors : list of str
        Validation errors that prevented baking.
    warnings : list of str
        Non-fatal issues found during validation.
    success : bool
        True iff errors is empty and the cooked file was written.
    """

    source_path: Path = field(default_factory=Path)
    cooked_path: Path = field(default_factory=Path)
    quest_count: int = 0
    node_count: int = 0
    errors: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)
    success: bool = False


# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------

_VALID_OBJECTIVE_TYPES = {"kill_enemy", "collect_item", "reach_location"}

# TEACHING NOTE — Why compute a SHA-256 hash of the source?
# The engine uses this hash to detect *stale caches*: if the source hash in
# assetdb.json differs from the cooked file's "source_hash" field, the engine
# knows it must re-cook the asset before loading it.
def _sha256_file(path: Path) -> str:
    """Return the hex SHA-256 digest of the file at *path*."""
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


# ---------------------------------------------------------------------------
# QuestBaker
# ---------------------------------------------------------------------------

class QuestBaker:
    """
    Bake a quest_bank.json source file into a cooked JSON file.

    TEACHING NOTE — Quest Bank Layout
    ===================================
    A *quest bank* is a collection of quest definitions authored by designers.
    Each quest has:

      - A unique integer ID.
      - A title, description, and main/side flag.
      - One or more ordered objectives (kill N, collect N, reach location).
      - XP and Gil rewards.
      - Optional item rewards (list of item IDs).
      - Optional prerequisite quest IDs that must be completed first.

    The baker validates:

      1. No duplicate quest IDs.
      2. Every objective has a valid type ("kill_enemy", "collect_item",
         "reach_location") and requiredCount >= 1.
      3. Every prerequisite quest ID references a quest that also exists in
         the bank (forward references within one file are allowed; the baker
         does a two-pass check).
      4. At least one quest is defined.

    The cooked output adds:
      - "source_hash"  — SHA-256 of the source file.
      - "baked_at"     — ISO-8601 UTC timestamp of when bake() ran.
      - "quest_index"  — dict mapping str(quest_id) → list index for O(1)
                         runtime lookup by ID.

    Examples
    --------
    >>> from pathlib import Path
    >>> baker = QuestBaker()
    >>> result = baker.bake(Path("Content/quest_bank.json"),
    ...                     Path("Cooked/quest_bank.cooked.json"))
    >>> assert result.success
    >>> assert result.quest_count >= 1
    """

    def bake(self, source: Path, output: Path) -> BakeResult:
        """
        Validate and cook *source* quest bank JSON, writing to *output*.

        Parameters
        ----------
        source : Path
            Path to the source quest_bank.json file.
        output : Path
            Path where the cooked JSON will be written.  Parent directories
            are created automatically.

        Returns
        -------
        BakeResult
            Result summary.  Check result.success and result.errors.

        Raises
        ------
        FileNotFoundError
            If *source* does not exist.
        json.JSONDecodeError
            If *source* is not valid JSON.
        """
        result = BakeResult(source_path=source, cooked_path=output)

        # ------------------------------------------------------------------
        # Step 1 — Load
        # ------------------------------------------------------------------
        with open(source, "r", encoding="utf-8") as fh:
            data: Dict[str, Any] = json.load(fh)

        # ------------------------------------------------------------------
        # Step 2 — Validate top-level structure
        # ------------------------------------------------------------------
        if "quests" not in data:
            result.errors.append("Missing required top-level field 'quests'.")
            return result

        quests: List[Dict[str, Any]] = data["quests"]

        if not isinstance(quests, list) or len(quests) == 0:
            result.errors.append("'quests' must be a non-empty array.")
            return result

        # TEACHING NOTE — Two-pass validation for cross-references.
        # Pass 1: collect all quest IDs so we can validate prerequisites.
        # Pass 2: validate objectives and prerequisites against the full set.
        quest_ids: set = set()
        for i, quest in enumerate(quests):
            qid = quest.get("id")
            if not isinstance(qid, int) or qid < 1:
                result.errors.append(
                    f"Quest at index {i}: 'id' must be a positive integer."
                )
            elif qid in quest_ids:
                result.errors.append(f"Duplicate quest ID {qid}.")
            else:
                quest_ids.add(qid)

        if result.errors:
            return result

        # Pass 2 — full validation
        for quest in quests:
            qid = quest["id"]
            prefix = f"Quest {qid} ({quest.get('title', '?')})"

            if not quest.get("title"):
                result.errors.append(f"{prefix}: 'title' is required.")

            # Objectives must be a non-empty list.
            objectives = quest.get("objectives", [])
            if not isinstance(objectives, list) or len(objectives) == 0:
                result.errors.append(
                    f"{prefix}: 'objectives' must be a non-empty array."
                )
            else:
                for j, obj in enumerate(objectives):
                    obj_prefix = f"{prefix} objective[{j}]"
                    obj_type = obj.get("type", "")
                    if obj_type not in _VALID_OBJECTIVE_TYPES:
                        result.errors.append(
                            f"{obj_prefix}: invalid type '{obj_type}'. "
                            f"Must be one of {sorted(_VALID_OBJECTIVE_TYPES)}."
                        )
                    req = obj.get("requiredCount", 0)
                    if not isinstance(req, int) or req < 1:
                        result.errors.append(
                            f"{obj_prefix}: 'requiredCount' must be >= 1."
                        )
                    if not obj.get("description"):
                        result.warnings.append(
                            f"{obj_prefix}: missing 'description' (optional but recommended)."
                        )

            # Prerequisite quest IDs must reference quests in this bank.
            for prereq_id in quest.get("prereqQuestIDs", []):
                if prereq_id not in quest_ids:
                    result.errors.append(
                        f"{prefix}: prerequisite quest ID {prereq_id} not "
                        f"found in this quest bank."
                    )

            # Rewards must be non-negative.
            for field_name in ("xpReward", "gilReward"):
                val = quest.get(field_name, 0)
                if not isinstance(val, int) or val < 0:
                    result.errors.append(
                        f"{prefix}: '{field_name}' must be a non-negative integer."
                    )

        if result.errors:
            return result

        # ------------------------------------------------------------------
        # Step 3 — Transform: build the cooked payload
        # ------------------------------------------------------------------
        # TEACHING NOTE — The quest_index lets the runtime do O(1) ID lookups.
        # Without it the runtime would scan the entire quests array each time
        # it needs to find a quest by ID — O(N) for every combat kill, item
        # pickup, etc.  A dict avoids this.
        quest_index: Dict[str, int] = {
            str(q["id"]): i for i, q in enumerate(quests)
        }

        cooked: Dict[str, Any] = {
            "$schema": "quest_bank.schema.json",
            "version": data.get("version", "1.0.0"),
            "source_hash": _sha256_file(source),
            "baked_at": datetime.now(timezone.utc).isoformat(),
            "quest_count": len(quests),
            "quest_index": quest_index,
            "quests": quests,
        }

        # ------------------------------------------------------------------
        # Step 4 — Write
        # ------------------------------------------------------------------
        output.parent.mkdir(parents=True, exist_ok=True)
        with open(output, "w", encoding="utf-8") as fh:
            json.dump(cooked, fh, indent=2)

        result.quest_count = len(quests)
        result.success = True
        return result

    def validate(self, source: Path) -> BakeResult:
        """
        Validate *source* without writing any output.

        This is useful in CI pipelines that want to check data integrity
        without producing build artifacts.

        Parameters
        ----------
        source : Path
            Path to the source quest_bank.json file.

        Returns
        -------
        BakeResult
            result.success is True if the source is valid.
        """
        # TEACHING NOTE — Use NamedTemporaryFile instead of mktemp().
        # tempfile.mktemp() is race-prone: it reserves a name without creating
        # the file, so another process (or attacker) could create that path
        # before bake() opens it.  NamedTemporaryFile(delete=False) atomically
        # creates the file, giving bake() a real on-disk path to write to.
        with tempfile.NamedTemporaryFile(suffix=".cooked.json", delete=False) as fh:
            tmp = Path(fh.name)
        try:
            result = self.bake(source, tmp)
        finally:
            if tmp.exists():
                os.unlink(tmp)
        return result


# ---------------------------------------------------------------------------
# DialogueBaker
# ---------------------------------------------------------------------------

class DialogueBaker:
    """
    Bake a dialogue_tree.json source file into a cooked JSON file.

    TEACHING NOTE — Dialogue Tree Structure
    =========================================
    A *dialogue tree* is a directed graph of *nodes*.  Each node represents
    one exchange in a conversation:

      - id          — unique integer within this tree (0 = root node).
      - speakerName — who is talking.
      - text        — what they say.
      - choices     — optional list of (label, targetNodeID) branches.
      - nextNodeID  — auto-advance target when choices is empty.
      - isTerminal  — true for end-of-conversation nodes.

    The baker validates:

      1. No duplicate node IDs.
      2. A root node with id=0 exists.
      3. All nextNodeID and choices.targetNodeID references point to nodes
         that exist in this tree.
      4. At least one terminal node exists.
      5. No node that is neither terminal nor has a next/choice target.

    The cooked output adds:
      - "source_hash" — SHA-256 of the source.
      - "node_index"  — dict mapping str(node_id) → list index for O(1) lookup.

    Examples
    --------
    >>> baker = DialogueBaker()
    >>> result = baker.bake(
    ...     Path("Content/dialogue_tree.json"),
    ...     Path("Cooked/dialogue_tree.cooked.json"))
    >>> assert result.success
    >>> assert result.node_count >= 2
    """

    def bake(self, source: Path, output: Path) -> BakeResult:
        """
        Validate and cook *source* dialogue tree JSON, writing to *output*.

        Parameters
        ----------
        source : Path
            Path to the source dialogue_tree.json file.
        output : Path
            Path where the cooked JSON will be written.

        Returns
        -------
        BakeResult
            Result summary.

        Raises
        ------
        FileNotFoundError, json.JSONDecodeError
        """
        result = BakeResult(source_path=source, cooked_path=output)

        with open(source, "r", encoding="utf-8") as fh:
            data: Dict[str, Any] = json.load(fh)

        nodes: List[Dict[str, Any]] = data.get("nodes", [])
        if not isinstance(nodes, list) or len(nodes) == 0:
            result.errors.append("'nodes' must be a non-empty array.")
            return result

        # ------------------------------------------------------------------
        # Pass 1 — collect node IDs
        # ------------------------------------------------------------------
        node_ids: set = set()
        for i, node in enumerate(nodes):
            nid = node.get("id")
            if not isinstance(nid, int) or nid < 0:
                result.errors.append(
                    f"Node at index {i}: 'id' must be a non-negative integer."
                )
            elif nid in node_ids:
                result.errors.append(f"Duplicate node ID {nid}.")
            else:
                node_ids.add(nid)

        if result.errors:
            return result

        # TEACHING NOTE — Root node requirement.
        # DialogueSystem always starts at node ID 0.  Requiring a root node
        # means the tree has a deterministic entry point regardless of array
        # ordering in the JSON file.
        if 0 not in node_ids:
            result.errors.append(
                "No root node found (a node with id=0 is required as the "
                "entry point for DialogueSystem)."
            )
            return result

        # ------------------------------------------------------------------
        # Pass 2 — validate references
        # ------------------------------------------------------------------
        terminal_count = 0
        for node in nodes:
            nid = node["id"]
            prefix = f"Node {nid}"

            if not node.get("speakerName"):
                result.errors.append(f"{prefix}: 'speakerName' is required.")
            if "text" not in node or not isinstance(node["text"], str):
                result.errors.append(f"{prefix}: 'text' (string) is required.")

            is_terminal = node.get("isTerminal", False)
            choices = node.get("choices", [])
            next_id = node.get("nextNodeID", None)

            if is_terminal:
                terminal_count += 1
            elif choices:
                # Validate choice targets.
                for choice in choices:
                    target = choice.get("targetNodeID")
                    if not isinstance(target, int) or target not in node_ids:
                        result.errors.append(
                            f"{prefix} choice '{choice.get('label', '?')}': "
                            f"targetNodeID {target} does not exist in this tree."
                        )
                    if not choice.get("label"):
                        result.warnings.append(
                            f"{prefix}: a choice is missing a 'label'."
                        )
            elif next_id is not None:
                if not isinstance(next_id, int) or next_id not in node_ids:
                    result.errors.append(
                        f"{prefix}: nextNodeID {next_id} does not exist in "
                        f"this tree."
                    )
            else:
                result.errors.append(
                    f"{prefix}: non-terminal node has no 'choices' and no "
                    f"'nextNodeID' — conversation would be stuck."
                )

        if terminal_count == 0:
            result.errors.append(
                "Dialogue tree has no terminal node (isTerminal=true). "
                "DialogueSystem would loop forever."
            )

        if result.errors:
            return result

        # ------------------------------------------------------------------
        # Step 3 — Transform
        # ------------------------------------------------------------------
        node_index: Dict[str, int] = {
            str(n["id"]): i for i, n in enumerate(nodes)
        }

        cooked: Dict[str, Any] = {
            "$schema": "dialogue_tree.schema.json",
            "version": data.get("version", "1.0.0"),
            "id": data.get("id", "unknown"),
            "source_hash": _sha256_file(source),
            "baked_at": datetime.now(timezone.utc).isoformat(),
            "node_count": len(nodes),
            "node_index": node_index,
            "nodes": nodes,
        }

        # ------------------------------------------------------------------
        # Step 4 — Write
        # ------------------------------------------------------------------
        output.parent.mkdir(parents=True, exist_ok=True)
        with open(output, "w", encoding="utf-8") as fh:
            json.dump(cooked, fh, indent=2)

        result.node_count = len(nodes)
        result.success = True
        return result

    def validate(self, source: Path) -> BakeResult:
        """
        Validate *source* without writing any output.

        Parameters
        ----------
        source : Path
            Path to the source dialogue_tree.json file.

        Returns
        -------
        BakeResult
        """
        # TEACHING NOTE — Use NamedTemporaryFile instead of mktemp().
        # tempfile.mktemp() is race-prone: it reserves a name without creating
        # the file, so another process (or attacker) could create that path
        # before bake() opens it.  NamedTemporaryFile(delete=False) atomically
        # creates the file, giving bake() a real on-disk path to write to.
        with tempfile.NamedTemporaryFile(suffix=".cooked.json", delete=False) as fh:
            tmp = Path(fh.name)
        try:
            result = self.bake(source, tmp)
        finally:
            if tmp.exists():
                os.unlink(tmp)
        return result
