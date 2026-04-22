"""
tests/test_quest_baker.py — Pytest suite for QuestBaker and DialogueBaker.

TEACHING NOTE — Testing a baker
=================================
A baker is tested by feeding it:
  1. Valid inputs — expect success=True, correct counts.
  2. Invalid inputs — expect success=False, specific error messages.

We use pytest's built-in tmp_path fixture to write temporary JSON files
without polluting the repository.  This is the standard pattern for testing
file-processing tools without side effects.
"""

import json
import tempfile
from pathlib import Path
from typing import Any, Dict, List

import pytest

from quest_baker import QuestBaker, DialogueBaker, BakeResult


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _write_json(path: Path, data: Any) -> Path:
    """Write *data* as JSON to *path* and return *path*."""
    path.write_text(json.dumps(data, indent=2), encoding="utf-8")
    return path


def _minimal_quest(
    qid: int = 1,
    title: str = "Test Quest",
    obj_type: str = "kill_enemy",
) -> Dict[str, Any]:
    """Return a minimal valid quest dict."""
    return {
        "id": qid,
        "title": title,
        "description": "A test quest.",
        "isMainQuest": False,
        "objectives": [
            {
                "type": obj_type,
                "description": "Do the thing.",
                "targetID": 1,
                "requiredCount": 3,
            }
        ],
        "xpReward": 50,
        "gilReward": 100,
        "prereqQuestIDs": [],
    }


def _minimal_quest_bank(quests: List[Dict[str, Any]]) -> Dict[str, Any]:
    """Wrap *quests* in a valid quest bank envelope."""
    return {"version": "1.0.0", "quests": quests}


def _minimal_node(nid: int = 0, is_terminal: bool = True) -> Dict[str, Any]:
    """Return a minimal valid terminal dialogue node."""
    return {
        "id": nid,
        "speakerName": "NPC",
        "text": "Hello there.",
        "isTerminal": is_terminal,
    }


def _minimal_dialogue_tree(nodes: List[Dict[str, Any]]) -> Dict[str, Any]:
    """Wrap *nodes* in a valid dialogue tree envelope."""
    return {
        "version": "1.0.0",
        "id": "test_tree",
        "nodes": nodes,
    }


# ---------------------------------------------------------------------------
# QuestBaker tests
# ---------------------------------------------------------------------------


class TestQuestBaker:

    def test_bake_valid_quest_bank(self, tmp_path: Path) -> None:
        """A valid quest bank bakes successfully and produces a cooked file."""
        src = _write_json(tmp_path / "quest_bank.json",
                          _minimal_quest_bank([_minimal_quest()]))
        out = tmp_path / "Cooked" / "quest_bank.cooked.json"
        baker = QuestBaker()
        result = baker.bake(src, out)

        assert result.success, result.errors
        assert result.quest_count == 1
        assert out.exists()

    def test_cooked_output_has_quest_index(self, tmp_path: Path) -> None:
        """Cooked output includes a quest_index for O(1) runtime lookup."""
        quests = [_minimal_quest(1), _minimal_quest(2, "Second Quest")]
        src = _write_json(tmp_path / "q.json", _minimal_quest_bank(quests))
        out = tmp_path / "q.cooked.json"
        baker = QuestBaker()
        baker.bake(src, out)

        cooked = json.loads(out.read_text())
        assert "quest_index" in cooked
        assert cooked["quest_index"]["1"] == 0
        assert cooked["quest_index"]["2"] == 1

    def test_bake_missing_quests_field(self, tmp_path: Path) -> None:
        """A bank with no 'quests' field is rejected with an error."""
        src = _write_json(tmp_path / "bad.json", {"version": "1.0.0"})
        out = tmp_path / "bad.cooked.json"
        baker = QuestBaker()
        result = baker.bake(src, out)

        assert not result.success
        assert any("quests" in e for e in result.errors)

    def test_bake_duplicate_quest_ids(self, tmp_path: Path) -> None:
        """Duplicate quest IDs are detected and reported."""
        quests = [_minimal_quest(1), _minimal_quest(1, "Dupe")]
        src = _write_json(tmp_path / "dup.json", _minimal_quest_bank(quests))
        out = tmp_path / "dup.cooked.json"
        baker = QuestBaker()
        result = baker.bake(src, out)

        assert not result.success
        assert any("Duplicate" in e for e in result.errors)

    def test_bake_invalid_objective_type(self, tmp_path: Path) -> None:
        """An objective with an unknown type is rejected."""
        quest = _minimal_quest()
        quest["objectives"][0]["type"] = "dance_on_moon"
        src = _write_json(tmp_path / "bad_obj.json",
                          _minimal_quest_bank([quest]))
        out = tmp_path / "bad_obj.cooked.json"
        baker = QuestBaker()
        result = baker.bake(src, out)

        assert not result.success
        assert any("dance_on_moon" in e for e in result.errors)

    def test_bake_bad_prereq_reference(self, tmp_path: Path) -> None:
        """A prerequisite ID that doesn't exist in the bank is an error."""
        quest = _minimal_quest(1)
        quest["prereqQuestIDs"] = [99]  # ID 99 does not exist
        src = _write_json(tmp_path / "prereq.json",
                          _minimal_quest_bank([quest]))
        out = tmp_path / "prereq.cooked.json"
        baker = QuestBaker()
        result = baker.bake(src, out)

        assert not result.success
        assert any("99" in e for e in result.errors)

    def test_validate_only_no_file_written(self, tmp_path: Path) -> None:
        """validate() does not write a cooked file on success."""
        src = _write_json(tmp_path / "vq.json",
                          _minimal_quest_bank([_minimal_quest()]))
        baker = QuestBaker()
        result = baker.validate(src)

        assert result.success
        # Only the source file should exist in tmp_path.
        assert list(tmp_path.glob("*.cooked.json")) == []

    def test_bake_multiple_objectives(self, tmp_path: Path) -> None:
        """A quest with multiple objectives bakes successfully."""
        quest = _minimal_quest(1)
        quest["objectives"].append({
            "type": "collect_item",
            "description": "Pick up the relic.",
            "targetID": 5,
            "requiredCount": 1,
        })
        src = _write_json(tmp_path / "multi.json",
                          _minimal_quest_bank([quest]))
        out = tmp_path / "multi.cooked.json"
        baker = QuestBaker()
        result = baker.bake(src, out)

        assert result.success, result.errors


# ---------------------------------------------------------------------------
# DialogueBaker tests
# ---------------------------------------------------------------------------


class TestDialogueBaker:

    def test_bake_valid_single_terminal(self, tmp_path: Path) -> None:
        """A dialogue tree with a single terminal root node bakes OK."""
        src = _write_json(tmp_path / "dlg.json",
                          _minimal_dialogue_tree([_minimal_node(0, is_terminal=True)]))
        out = tmp_path / "dlg.cooked.json"
        baker = DialogueBaker()
        result = baker.bake(src, out)

        assert result.success, result.errors
        assert result.node_count == 1
        assert out.exists()

    def test_bake_multi_node_tree(self, tmp_path: Path) -> None:
        """A tree with root → body → terminal bakes successfully."""
        nodes = [
            {"id": 0, "speakerName": "NPC", "text": "Greetings.",
             "nextNodeID": 1, "isTerminal": False},
            {"id": 1, "speakerName": "NPC", "text": "Farewell.",
             "isTerminal": True},
        ]
        src = _write_json(tmp_path / "multi.json",
                          _minimal_dialogue_tree(nodes))
        out = tmp_path / "multi.cooked.json"
        baker = DialogueBaker()
        result = baker.bake(src, out)

        assert result.success, result.errors
        assert result.node_count == 2

    def test_cooked_output_has_node_index(self, tmp_path: Path) -> None:
        """Cooked output includes a node_index for O(1) lookup."""
        nodes = [
            {"id": 0, "speakerName": "A", "text": "Hi", "nextNodeID": 1,
             "isTerminal": False},
            {"id": 1, "speakerName": "A", "text": "Bye", "isTerminal": True},
        ]
        src = _write_json(tmp_path / "idx.json",
                          _minimal_dialogue_tree(nodes))
        out = tmp_path / "idx.cooked.json"
        baker = DialogueBaker()
        baker.bake(src, out)

        cooked = json.loads(out.read_text())
        assert cooked["node_index"]["0"] == 0
        assert cooked["node_index"]["1"] == 1

    def test_bake_missing_root_node(self, tmp_path: Path) -> None:
        """A tree without a node id=0 is rejected."""
        nodes = [{"id": 1, "speakerName": "NPC", "text": "No root.",
                  "isTerminal": True}]
        src = _write_json(tmp_path / "noroot.json",
                          _minimal_dialogue_tree(nodes))
        out = tmp_path / "noroot.cooked.json"
        baker = DialogueBaker()
        result = baker.bake(src, out)

        assert not result.success
        assert any("root node" in e.lower() or "id=0" in e for e in result.errors)

    def test_bake_dangling_next_node(self, tmp_path: Path) -> None:
        """A nextNodeID that references a non-existent node is an error."""
        nodes = [{"id": 0, "speakerName": "NPC", "text": "Hello.",
                  "nextNodeID": 999, "isTerminal": False}]
        src = _write_json(tmp_path / "dangle.json",
                          _minimal_dialogue_tree(nodes))
        out = tmp_path / "dangle.cooked.json"
        baker = DialogueBaker()
        result = baker.bake(src, out)

        assert not result.success
        assert any("999" in e for e in result.errors)

    def test_bake_no_terminal_node(self, tmp_path: Path) -> None:
        """A tree with no terminal node is rejected."""
        nodes = [{"id": 0, "speakerName": "NPC", "text": "Loop.",
                  "nextNodeID": 0, "isTerminal": False}]
        src = _write_json(tmp_path / "noterm.json",
                          _minimal_dialogue_tree(nodes))
        out = tmp_path / "noterm.cooked.json"
        baker = DialogueBaker()
        result = baker.bake(src, out)

        assert not result.success
        assert any("terminal" in e.lower() for e in result.errors)

    def test_bake_branching_choices(self, tmp_path: Path) -> None:
        """A tree with branching choices bakes successfully."""
        nodes = [
            {"id": 0, "speakerName": "NPC",
             "text": "Choose wisely.",
             "choices": [
                 {"label": "Yes.", "targetNodeID": 1},
                 {"label": "No.", "targetNodeID": 2},
             ],
             "isTerminal": False},
            {"id": 1, "speakerName": "NPC", "text": "You chose yes.",
             "isTerminal": True},
            {"id": 2, "speakerName": "NPC", "text": "You chose no.",
             "isTerminal": True},
        ]
        src = _write_json(tmp_path / "branch.json",
                          _minimal_dialogue_tree(nodes))
        out = tmp_path / "branch.cooked.json"
        baker = DialogueBaker()
        result = baker.bake(src, out)

        assert result.success, result.errors
        assert result.node_count == 3
