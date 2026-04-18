#!/usr/bin/env python3
"""
check_architecture.py — Architecture Lint Tool
===============================================

TEACHING NOTE — Why Architecture Enforcement Matters
-----------------------------------------------------
In a teaching engine every rule exists to be *understood*, not just obeyed.
This script enforces three of the most important structural rules:

1. **File-size limit (500 lines)**
   Large files are harder to read, harder to test, and harder to reason about.
   When a file exceeds ~500 lines it almost always contains more than one
   concept — a sign that it should be split into smaller, more focused units.
   Exceptions (``ECS.hpp``, ``Types.hpp``, etc.) are deliberate teaching
   monoliths whose size is justified by their pedagogical purpose.

2. **Layer-boundary violations**
   An engine is divided into layers so that higher-level code can be changed
   without touching lower-level code.  The forbidden-include table below
   matches ``docs/COPILOT_CONTINUATION.md § 3.3`` exactly.  Violating a layer
   boundary is a build error in production studios; here it is a CI error.

3. **TEACHING NOTE presence**
   Every new C++ source file must contain at least one ``TEACHING NOTE`` block.
   This rule keeps the codebase self-documenting and ensures that a student
   reading any file for the first time always has an entry point for learning.

Usage
-----
    python scripts/check_architecture.py [--repo-root <path>] [--strict]

    --strict   Exit 1 on warnings as well as errors (default: only errors).

Exit codes
----------
    0   No violations found.
    1   One or more violations found.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path



# ---------------------------------------------------------------------------
# Configuration — layer-boundary rules
# ---------------------------------------------------------------------------

# TEACHING NOTE — Forbidden-Include Table
# ----------------------------------------
# Each entry in LAYER_RULES maps a *source layer* (a glob-like path prefix
# relative to ``src/``) to the set of *forbidden* path substrings that must
# NOT appear in its ``#include`` directives.
#
# The table is taken directly from COPILOT_CONTINUATION.md § 3.3:
#
#   Layer              May depend on       Must NOT depend on
#   engine/core/       (nothing else)      everything below
#   engine/ecs/        core/               game/, tools
#   engine/rendering/  core/, ecs/         game/
#   engine/audio/      core/, ecs/         game/, rendering/
#   engine/animation/  core/, ecs/         game/
#   engine/physics/    core/, ecs/         game/
#   game/systems/      engine/             tools/, editor/
#   game/world/        engine/, systems/   tools/, editor/
#   tools/             engine/core/        game/
#
# Implementation note: we match on the *normalised* include path string
# (forward slashes, relative to the repo root ``src/`` directory).

LAYER_RULES: list[tuple[str, list[str]]] = [
    # (source_prefix,  [forbidden_include_substrings])
    ("engine/core",       ["engine/ecs", "engine/rendering", "engine/audio",
                           "engine/animation", "engine/physics", "game/"]),
    ("engine/ecs",        ["engine/rendering", "engine/audio",
                           "engine/animation", "engine/physics",
                           "game/", "tools/"]),
    ("engine/rendering",  ["game/", "tools/"]),
    ("engine/audio",      ["game/", "engine/rendering", "tools/"]),
    ("engine/animation",  ["game/", "tools/"]),
    ("engine/physics",    ["game/", "tools/"]),
    ("game/systems",      ["tools/", "editor/"]),
    ("game/world",        ["tools/", "editor/"]),
]

# ---------------------------------------------------------------------------
# Configuration — file-size exceptions
# ---------------------------------------------------------------------------

# TEACHING NOTE — Teaching Exceptions to the 500-Line Rule
# ----------------------------------------------------------
# These files intentionally exceed 500 lines because their size is
# *pedagogically justified*: the ECS is studied as a whole, and splitting
# it would obscure the relationships between components.  Every exception
# must be listed here with a rationale comment.
#
# Files listed here are exempt from the line-count violation check but are
# still subject to all other checks (layer boundaries, TEACHING NOTEs).

FILE_SIZE_EXCEPTIONS: dict[str, str] = {
    # ECS is a deliberate teaching monolith (COPILOT_CONTINUATION.md § 3.5).
    "src/engine/ecs/ECS.hpp":                 "deliberate teaching monolith — ECS studied as a whole",
    # GameData aggregates all shared game constants in one place for discoverability.
    "src/game/GameData.hpp":                  "deliberate teaching monolith — shared game constants",
    # Types.hpp centralises all engine-wide enums and type aliases.
    "src/engine/core/Types.hpp":              "deliberate teaching monolith — all engine enums/types",
    # EventBus.hpp is a heavily-commented deep-dive into the observer pattern.
    "src/engine/core/EventBus.hpp":           "deliberate teaching monolith — observer pattern deep-dive",
    # Renderer.hpp/.cpp: large because ncurses rendering is inherently verbose.
    "src/engine/rendering/Renderer.hpp":      "ncurses terminal renderer — inherently large",
    "src/engine/rendering/Renderer.cpp":      "ncurses terminal renderer — inherently large",
    # LuaEngine: the Lua binding layer is intentionally comprehensive.
    "src/engine/scripting/LuaEngine.hpp":     "Lua binding layer — comprehensive public API",
    "src/engine/scripting/LuaEngine.cpp":     "Lua binding layer — comprehensive implementation",
    # TileMap: large because it covers several tile rendering sub-problems.
    "src/game/world/TileMap.hpp":             "tilemap subsystem — several inter-related helpers",
    "src/game/world/TileMap.cpp":             "tilemap subsystem — several inter-related helpers",
    # VulkanRenderer.cpp: Vulkan requires substantial boilerplate by design.
    "src/engine/rendering/vulkan/VulkanRenderer.cpp": "Vulkan boilerplate — unavoidably large",
    # D3D11Renderer.cpp: comprehensive inline teaching notes on the D3D11 API.
    "src/engine/rendering/d3d11/D3D11Renderer.cpp":   "D3D11 renderer — extensive inline TEACHING NOTEs cover COM, WARP, swap chain, viewport, off-screen rendering",
    # cook_main.cpp: standalone asset cooker — verbose for teaching purposes.
    "src/tools/cook/cook_main.cpp":                   "cook tool — single-file teaching tool; teaches JSON parsing, asset pipeline, and filesystem ops",
    # InputSystem: covers keyboard, mouse, and gamepad in one teaching file.
    "src/engine/input/InputSystem.hpp":       "input system — multiple device types in one file",
    # CombatSystem and Zone are large but structured subsystems.
    "src/game/systems/CombatSystem.cpp":      "combat subsystem — core gameplay logic",
    "src/game/systems/CombatSystem.hpp":      "combat subsystem — comprehensive public API",
    "src/game/world/Zone.cpp":                "zone subsystem — structured lifecycle logic",
    "src/game/world/Zone.hpp":                "zone subsystem — comprehensive public API",
    "src/game/Game.cpp":                      "main game loop — wires all subsystems together",
    "src/game/systems/AISystem.cpp":          "AI subsystem — FSM + A* pathfinding",
    "src/game/systems/AISystem.hpp":          "AI subsystem — comprehensive public API",
    # AudioSystem.hpp is part of the Python audio_authoring tool's C++ integration
    # example, not the engine's own src/ tree.  Its size is justified by the
    # comprehensive API it demonstrates for students studying XAudio2 integration.
    "tools/audio_authoring/audio_engine/integration/cpp/AudioSystem.hpp":
        "audio tool C++ integration example — comprehensive XAudio2 API demonstration",
    # xaudio2_backend.cpp: M3 XAudio2 backend — extensive teaching notes cover COM
    # init, RIFF/WAVE parsing, source voice pools, and XAudio2 buffer submission.
    "src/engine/audio/xaudio2_backend.cpp":
        "XAudio2 backend — M3 audio milestone; TEACHING NOTEs cover COM, RIFF/WAV, voice pool, buffer submission",
    # d3d11_texture.cpp: M3 D3D11 DDS texture loader — teaching notes cover DDS
    # format, block compression (BC1/BC3/BC7), mip maps, and D3D11 resource upload.
    "src/engine/rendering/d3d11/d3d11_texture.cpp":
        "D3D11 DDS texture loader — M3 texture milestone; TEACHING NOTEs cover DDS format, BC7 compression, mip map upload",
}

# ---------------------------------------------------------------------------
# Configuration — known layer-boundary exceptions
# ---------------------------------------------------------------------------

# TEACHING NOTE — Suppressing Known Pre-existing Violations
# ----------------------------------------------------------
# A freshly introduced lint rule will almost always find violations in existing
# code.  The correct workflow is:
#
#   1. List all *pre-existing* violations here with a clear rationale and a
#      TODO comment noting the refactor needed to remove them.
#   2. New violations that are NOT in this list immediately fail CI.
#   3. Work through the list over time, removing entries as the refactors land.
#
# Key: (repo-relative file path, forbidden include substring)
# Value: human-readable rationale for allowing the exception.

LAYER_EXCEPTIONS: dict[tuple[str, str], str] = {
    # The ncurses terminal renderer (Renderer.cpp) is a game-specific component
    # that renders game tiles.  It couples to game/ types (TileType, GameData)
    # by design — it IS the game's terminal view.  The Vulkan renderer
    # (VulkanRenderer.cpp) is correctly separated.
    #
    # TODO M8: When the gameplay systems are wired into the Vulkan runtime the
    # terminal renderer should be removed or placed under game/ rather than
    # engine/rendering/, resolving this violation.
    ("src/engine/rendering/Renderer.cpp", "game/"): (
        "TerminalRenderer couples to TileType/GameData by design (ncurses game view). "
        "TODO: move under game/ when Vulkan runtime wiring is complete (M8)."
    ),
}

# Maximum lines before a violation is reported.
FILE_SIZE_LIMIT = 500

# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------

LEVEL_ERROR = "ERROR"
LEVEL_WARN = "WARN"


@dataclass
class Violation:
    """A single architecture violation."""

    level: str       # LEVEL_ERROR or LEVEL_WARN
    file: Path
    line: int | None
    rule: str
    message: str


@dataclass
class CheckResult:
    """Aggregate result of all architecture checks."""

    violations: list[Violation] = field(default_factory=list)

    @property
    def errors(self) -> list[Violation]:
        return [v for v in self.violations if v.level == LEVEL_ERROR]

    @property
    def warnings(self) -> list[Violation]:
        return [v for v in self.violations if v.level == LEVEL_WARN]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

_INCLUDE_RE = re.compile(r'^\s*#\s*include\s+[<"]([^>"]+)[>"]')
_TEACHING_NOTE_RE = re.compile(r"TEACHING NOTE")


def _rel(path: Path, repo_root: Path) -> str:
    """Return *path* as a forward-slash string relative to *repo_root*."""
    try:
        return path.relative_to(repo_root).as_posix()
    except ValueError:
        return str(path)


def _file_lines(path: Path) -> list[str]:
    """Read *path* returning lines (empty list on error)."""
    try:
        return path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return []


# ---------------------------------------------------------------------------
# Check 1 — File size
# ---------------------------------------------------------------------------

def check_file_size(
    cpp_files: list[Path],
    repo_root: Path,
    result: CheckResult,
) -> None:
    """Emit a WARN for every C++ file exceeding FILE_SIZE_LIMIT lines.

    Files listed in FILE_SIZE_EXCEPTIONS are skipped.

    TEACHING NOTE — Why 500 Lines?
    --------------------------------
    500 lines fits in a single screen with reasonable font size (roughly
    40 lines × 12 visible at once = several screenfuls, which is the
    cognitive limit for keeping a whole file in working memory at once).
    Files longer than this almost always contain more than one concept and
    benefit from being split.  The number is a *guideline*, not a hard
    physical limit — hence the list of explicit teaching exceptions above.
    """
    exceptions: set[str] = set(FILE_SIZE_EXCEPTIONS.keys())

    for path in cpp_files:
        rel = _rel(path, repo_root)
        if rel in exceptions:
            continue
        lines = _file_lines(path)
        if len(lines) > FILE_SIZE_LIMIT:
            result.violations.append(Violation(
                level=LEVEL_WARN,
                file=path,
                line=None,
                rule="file-size",
                message=(
                    f"{rel}: {len(lines)} lines exceeds the {FILE_SIZE_LIMIT}-line "
                    f"soft limit.  Consider splitting into smaller translation units. "
                    f"If this is intentional, add it to FILE_SIZE_EXCEPTIONS in "
                    f"scripts/check_architecture.py with a rationale."
                ),
            ))


# ---------------------------------------------------------------------------
# Check 2 — Layer boundaries
# ---------------------------------------------------------------------------

def check_layer_boundaries(
    cpp_files: list[Path],
    repo_root: Path,
    result: CheckResult,
) -> None:
    """Detect forbidden ``#include`` directives that cross layer boundaries.

    TEACHING NOTE — Include-Based Layer Checking
    ---------------------------------------------
    The simplest way to enforce layer boundaries in C++ is to scan ``#include``
    directives and reject any that cross the forbidden edges in the dependency
    graph.  A more sophisticated approach (used by large studios) involves
    building a full include-dependency graph and running graph algorithms on it.
    For a teaching engine, regex-based scanning is clear, fast, and sufficient.
    """
    src_root = repo_root / "src"

    for path in cpp_files:
        # Determine which layer this file belongs to.
        try:
            rel_to_src = path.relative_to(src_root).as_posix()
        except ValueError:
            continue  # file is not under src/ — skip layer checks

        source_layer: str | None = None
        forbidden: list[str] = []
        for layer_prefix, layer_forbidden in LAYER_RULES:
            if rel_to_src.startswith(layer_prefix):
                source_layer = layer_prefix
                forbidden = layer_forbidden
                break

        if source_layer is None:
            continue  # not a regulated layer

        lines = _file_lines(path)
        for lineno, raw in enumerate(lines, start=1):
            m = _INCLUDE_RE.match(raw)
            if not m:
                continue
            include_path = m.group(1).replace("\\", "/")
            for bad in forbidden:
                if bad in include_path:
                    # Check whether this specific (file, pattern) pair is a
                    # documented known exception.
                    exception_key = (_rel(path, repo_root), bad)
                    if exception_key in LAYER_EXCEPTIONS:
                        # Known exception — emit a WARN instead of an ERROR so
                        # it is visible in CI output but does not fail the build.
                        result.violations.append(Violation(
                            level=LEVEL_WARN,
                            file=path,
                            line=lineno,
                            rule="layer-boundary-known-exception",
                            message=(
                                f"{_rel(path, repo_root)}:{lineno}: "
                                f"known layer exception '{bad}' in '{include_path}'. "
                                f"Rationale: {LAYER_EXCEPTIONS[exception_key]}"
                            ),
                        ))
                    else:
                        result.violations.append(Violation(
                            level=LEVEL_ERROR,
                            file=path,
                            line=lineno,
                            rule="layer-boundary",
                            message=(
                                f"{_rel(path, repo_root)}:{lineno}: "
                                f"layer '{source_layer}' must NOT include '{include_path}' "
                                f"(forbidden pattern: '{bad}').  "
                                f"See docs/COPILOT_CONTINUATION.md § 3.3 for the full dependency table."
                            ),
                        ))
                    break  # one violation per include line is enough


# ---------------------------------------------------------------------------
# Check 3 — TEACHING NOTE presence
# ---------------------------------------------------------------------------

def check_teaching_notes(
    cpp_files: list[Path],
    repo_root: Path,
    result: CheckResult,
) -> None:
    """Warn on any C++ source file that contains no TEACHING NOTE block.

    TEACHING NOTE — Documentation as a First-Class Requirement
    -----------------------------------------------------------
    In this project documentation is not optional.  Every C++ source file
    must contain at least one ``TEACHING NOTE`` block that explains *why*
    the code exists and what pattern it demonstrates.  Files without any
    teaching notes are flagged as incomplete — they cannot teach a student
    anything beyond what the code itself says.

    The threshold is deliberately low (≥ 1 note per file) because many
    small utility files genuinely need only a single explanatory block.
    """
    for path in cpp_files:
        rel = _rel(path, repo_root)
        # Only check files under src/ to avoid flagging generated headers etc.
        if not rel.startswith("src/"):
            continue
        content = path.read_text(encoding="utf-8", errors="replace")
        if not _TEACHING_NOTE_RE.search(content):
            result.violations.append(Violation(
                level=LEVEL_WARN,
                file=path,
                line=None,
                rule="teaching-note-missing",
                message=(
                    f"{rel}: no TEACHING NOTE block found.  "
                    f"Add at least one // TEACHING NOTE — <title> block "
                    f"that explains what this file teaches."
                ),
            ))


# ---------------------------------------------------------------------------
# Repository walk
# ---------------------------------------------------------------------------

def collect_cpp_files(repo_root: Path) -> list[Path]:
    """Return all ``.cpp`` and ``.hpp`` files under *repo_root*, sorted."""
    skip_dirs = {"build", "build-test", ".git", "__pycache__", "node_modules"}
    result: list[Path] = []
    for path in sorted(repo_root.rglob("*")):
        if any(part in skip_dirs for part in path.parts):
            continue
        if path.suffix in (".cpp", ".hpp", ".h", ".c"):
            result.append(path)
    return result


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------

def print_report(check_result: CheckResult, strict: bool) -> int:
    """Print the violation report and return the process exit code.

    Parameters
    ----------
    check_result : CheckResult
    strict : bool
        If True, treat warnings as errors for the exit code.

    Returns
    -------
    int
        0 = clean, 1 = violations present.
    """
    errors = check_result.errors
    warnings = check_result.warnings

    if not check_result.violations:
        print("✓  Architecture checks passed — no violations found.")
        return 0

    if warnings:
        print(f"\n{'='*70}")
        print(f"WARNINGS ({len(warnings)})")
        print('='*70)
        for v in warnings:
            loc = f":{v.line}" if v.line else ""
            print(f"  [WARN]  {v.file}{loc}")
            print(f"          {v.message}")
            print()

    if errors:
        print(f"\n{'='*70}")
        print(f"ERRORS ({len(errors)})")
        print('='*70)
        for v in errors:
            loc = f":{v.line}" if v.line else ""
            print(f"  [ERROR] {v.file}{loc}")
            print(f"          {v.message}")
            print()

    print(f"\nSummary: {len(errors)} error(s), {len(warnings)} warning(s)")

    has_failure = bool(errors) or (strict and bool(warnings))
    return 1 if has_failure else 0


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main(argv: list[str] | None = None) -> int:
    """CLI entry point."""
    parser = argparse.ArgumentParser(
        description="Enforce architecture rules: file-size, layer boundaries, teaching notes.",
    )
    parser.add_argument(
        "--repo-root",
        default=None,
        help="Repository root directory.  Defaults to the parent of this script.",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Exit 1 on warnings as well as errors.",
    )
    parser.add_argument(
        "--skip-teaching-notes",
        action="store_true",
        help="Skip the TEACHING NOTE presence check (useful for WIP branches).",
    )

    args = parser.parse_args(argv)

    script_dir = Path(__file__).resolve().parent
    repo_root = Path(args.repo_root).resolve() if args.repo_root else script_dir.parent

    if not repo_root.is_dir():
        print(f"ERROR: '{repo_root}' is not a directory.", file=sys.stderr)
        return 1

    print(f"Architecture lint — repo root: {repo_root}\n")

    cpp_files = collect_cpp_files(repo_root)
    print(f"Scanning {len(cpp_files)} C/C++ source files …")

    result = CheckResult()
    check_file_size(cpp_files, repo_root, result)
    check_layer_boundaries(cpp_files, repo_root, result)
    if not args.skip_teaching_notes:
        check_teaching_notes(cpp_files, repo_root, result)

    return print_report(result, strict=args.strict)


if __name__ == "__main__":
    sys.exit(main())
