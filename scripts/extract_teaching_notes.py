#!/usr/bin/env python3
"""
extract_teaching_notes.py — Curriculum Index Generator
=======================================================

TEACHING NOTE — Automated Documentation Extraction
----------------------------------------------------
AAA studios maintain "living documentation" that is generated directly from
source code comments, not maintained separately.  When docs live in .md files
alone they drift from reality; when they are co-located with code as structured
comments (TEACHING NOTE blocks), they stay accurate because every code reviewer
sees them.

This script implements the extraction side: it walks the entire repository,
finds every ``TEACHING NOTE`` block in C++, Python, CMake, YAML, and Lua
source files, and emits ``docs/CURRICULUM_INDEX.md`` — a searchable, browsable
index that links every lesson back to the source file where it was written.

Usage
-----
    python scripts/extract_teaching_notes.py [--repo-root <path>] [--output <path>]

    Defaults:
        --repo-root   directory containing this script's parent (repo root)
        --output      docs/CURRICULUM_INDEX.md  (relative to repo root)

Exit codes
----------
    0   Index written successfully.
    1   Fatal error (I/O failure, bad arguments, etc.).
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path


# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

# File extensions that may contain TEACHING NOTE blocks.
SCANNABLE_EXTENSIONS: set[str] = {
    ".cpp", ".hpp", ".c", ".h",   # C / C++
    ".py",                         # Python
    ".cmake", ".txt",              # CMake (CMakeLists.txt, *.cmake)
    ".yml", ".yaml",               # GitHub Actions / CI
    ".lua",                        # Lua scripts
}

# Directories that are never interesting (build artefacts, dependencies).
SKIP_DIRS: set[str] = {
    "build", "build-test", ".git", "node_modules",
    "__pycache__", ".venv", "venv",
}

# The marker that starts a teaching block (case-sensitive per project standard).
TEACHING_NOTE_MARKER = "TEACHING NOTE"

# Maximum lines to capture per teaching block before we stop (safety valve).
MAX_BLOCK_LINES = 80


# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------

@dataclass
class TeachingNote:
    """A single extracted TEACHING NOTE block.

    Parameters
    ----------
    title : str
        The short title that appears after the ``—`` on the TEACHING NOTE line.
    file : Path
        Repository-relative path to the source file.
    line : int
        1-based line number of the ``TEACHING NOTE`` marker.
    body : str
        Cleaned text of the block (leading comment characters stripped).
    subsystem : str
        Derived from the file path – e.g. ``engine/core``, ``game/systems``.
    """

    title: str
    file: Path
    line: int
    body: str
    subsystem: str


@dataclass
class SubsystemGroup:
    """All notes that belong to the same subsystem path."""

    name: str
    notes: list[TeachingNote] = field(default_factory=list)


# ---------------------------------------------------------------------------
# Extraction helpers
# ---------------------------------------------------------------------------

# Matches the TEACHING NOTE title line in all supported comment styles:
#   //  TEACHING NOTE — Title
#   /*  TEACHING NOTE — Title
#    *  TEACHING NOTE — Title
#   #   TEACHING NOTE — Title       (Python / CMake / YAML / Lua)
_TITLE_RE = re.compile(
    r"""
    ^                              # start of line
    [ \t]*                         # optional leading whitespace
    (?://|/\*|\*|[#]|--)?          # optional comment opener (# in [] avoids comment parse)
    [ \t]*                         # optional whitespace after opener
    TEACHING\ NOTE                 # the marker (literal space)
    (?:\ *[—–\-]+\ *(.*))?         # optional " — Title" part (capture group 1)
    $
    """,
    re.VERBOSE,
)

# Matches a "continuation" comment line (strip the comment glyph prefix).
_COMMENT_LINE_RE = re.compile(
    r"""
    ^[ \t]*                        # leading whitespace
    (?://|/\*|\*/?|[#]|--)?        # comment glyph(s)
    [ \t]?                         # single optional space after glyph
    (.*)                           # rest of line (capture group 1)
    $
    """,
    re.VERBOSE,
)

# A line that signals the block has ended.
_BLOCK_END_RE = re.compile(
    r"""
    ^[ \t]*                        # leading whitespace
    (?:
        ={5,}                      # ===== separator
      | -{5,}                      # ----- separator
      | \*+/                       # end of /* … */ comment
    )
    """,
    re.VERBOSE,
)


def _strip_comment(line: str) -> str:
    """Remove leading comment glyphs from *line* and return the payload."""
    m = _COMMENT_LINE_RE.match(line)
    return m.group(1) if m else line


def _derive_subsystem(rel_path: Path) -> str:
    """Convert a repository-relative path to a human-readable subsystem label.

    Examples
    --------
    ``src/engine/core/Logger.cpp``  →  ``engine/core``
    ``src/game/systems/AISystem.cpp`` → ``game/systems``
    ``tools/audio_authoring/…``     →  ``tools/audio_authoring``
    ``.github/workflows/…``          →  ``ci/workflows``
    """
    parts = rel_path.parts
    if len(parts) >= 3 and parts[0] == "src":
        return "/".join(parts[1:3])
    if len(parts) >= 2 and parts[0] == ".github":
        return "ci/" + parts[1]
    if len(parts) >= 2:
        return "/".join(parts[:2])
    return parts[0] if parts else "misc"


def extract_notes_from_file(abs_path: Path, repo_root: Path) -> list[TeachingNote]:
    """Parse *abs_path* and return all ``TEACHING NOTE`` blocks found.

    Parameters
    ----------
    abs_path : Path
        Absolute path of the file to parse.
    repo_root : Path
        Absolute path of the repository root (used to compute relative paths).

    Returns
    -------
    list[TeachingNote]
        Possibly empty list of extracted notes.
    """
    try:
        text = abs_path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return []

    rel_path = abs_path.relative_to(repo_root)
    subsystem = _derive_subsystem(rel_path)
    lines = text.splitlines()
    notes: list[TeachingNote] = []

    i = 0
    while i < len(lines):
        m = _TITLE_RE.match(lines[i])
        if m:
            title = (m.group(1) or "").strip()
            start_line = i + 1  # 1-based
            body_lines: list[str] = []

            # Capture the body of the block (up to MAX_BLOCK_LINES).
            j = i + 1
            while j < len(lines) and len(body_lines) < MAX_BLOCK_LINES:
                raw = lines[j]
                # Stop on a blank line that follows at least one body line.
                if not raw.strip() and body_lines:
                    break
                # Stop on a section separator.
                if _BLOCK_END_RE.match(raw):
                    break
                # Stop if we hit another TEACHING NOTE marker.
                if _TITLE_RE.match(raw):
                    break
                body_lines.append(_strip_comment(raw))
                j += 1

            body = "\n".join(body_lines).strip()
            notes.append(TeachingNote(
                title=title or "(untitled)",
                file=rel_path,
                line=start_line,
                body=body,
                subsystem=subsystem,
            ))
            i = j  # skip the block we just consumed
        else:
            i += 1

    return notes


# ---------------------------------------------------------------------------
# Repository walk
# ---------------------------------------------------------------------------

def collect_all_notes(repo_root: Path) -> list[TeachingNote]:
    """Walk *repo_root* and collect every TEACHING NOTE block.

    Parameters
    ----------
    repo_root : Path
        Root directory of the repository.

    Returns
    -------
    list[TeachingNote]
        All notes found, ordered by (subsystem, file, line).
    """
    all_notes: list[TeachingNote] = []

    for path in sorted(repo_root.rglob("*")):
        # Skip hidden directories and known build/dependency directories.
        if any(part in SKIP_DIRS for part in path.parts):
            continue
        if not path.is_file():
            continue
        if path.suffix.lower() not in SCANNABLE_EXTENSIONS:
            # CMakeLists.txt has no extension suffix; handle by name too.
            if path.name != "CMakeLists.txt":
                continue

        all_notes.extend(extract_notes_from_file(path, repo_root))

    # Sort: primary = subsystem, secondary = file, tertiary = line number.
    all_notes.sort(key=lambda n: (n.subsystem, str(n.file), n.line))
    return all_notes


# ---------------------------------------------------------------------------
# Markdown generation
# ---------------------------------------------------------------------------

def _subsystem_anchor(name: str) -> str:
    """Generate a GitHub-Markdown-compatible anchor for *name*."""
    return name.replace("/", "").replace(" ", "-").lower()


def build_markdown(notes: list[TeachingNote], repo_root: Path) -> str:
    """Render *notes* as a Markdown curriculum index document.

    Parameters
    ----------
    notes : list[TeachingNote]
        All teaching notes to include.
    repo_root : Path
        Repository root (used to sanity-check relative paths).

    Returns
    -------
    str
        Complete Markdown document content.
    """
    # Group by subsystem.
    groups: dict[str, SubsystemGroup] = {}
    for note in notes:
        if note.subsystem not in groups:
            groups[note.subsystem] = SubsystemGroup(name=note.subsystem)
        groups[note.subsystem].notes.append(note)

    lines: list[str] = []

    lines.append("<!-- AUTO-GENERATED — do not edit by hand.")
    lines.append("     Regenerate with:  python scripts/extract_teaching_notes.py")
    lines.append("-->")
    lines.append("")
    lines.append("# Curriculum Index")
    lines.append("")
    lines.append(
        "This index is **automatically generated** from every `TEACHING NOTE` block "
        "in the repository source code.  Each entry links back to the exact line "
        "where the lesson was written."
    )
    lines.append("")
    lines.append(
        f"**Total lessons:** {len(notes)} across {len(groups)} subsystems."
    )
    lines.append("")
    lines.append("---")
    lines.append("")

    # Table of contents.
    lines.append("## Table of Contents")
    lines.append("")
    for name in sorted(groups):
        anchor = _subsystem_anchor(name)
        count = len(groups[name].notes)
        lines.append(f"- [{name}](#{anchor}) ({count} lesson{'s' if count != 1 else ''})")
    lines.append("")
    lines.append("---")
    lines.append("")

    # Per-subsystem sections.
    for name in sorted(groups):
        group = groups[name]
        lines.append(f"## {name}")
        lines.append("")

        for note in group.notes:
            # Link to the file on GitHub (works when the index is viewed on github.com).
            file_link = f"[`{note.file}`]({note.file}#L{note.line})"
            lines.append(f"### {note.title}")
            lines.append("")
            lines.append(f"**Source:** {file_link} (line {note.line})")
            lines.append("")
            if note.body:
                lines.append(note.body)
                lines.append("")

        lines.append("---")
        lines.append("")

    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main(argv: list[str] | None = None) -> int:
    """CLI entry point.

    Parameters
    ----------
    argv : list[str] | None
        Command-line arguments (defaults to ``sys.argv[1:]``).

    Returns
    -------
    int
        Exit code (0 = success, 1 = failure).
    """
    parser = argparse.ArgumentParser(
        description="Extract TEACHING NOTE blocks and emit docs/CURRICULUM_INDEX.md",
    )
    parser.add_argument(
        "--repo-root",
        default=None,
        help="Path to the repository root.  Defaults to the parent of this script.",
    )
    parser.add_argument(
        "--output",
        default=None,
        help="Output path for the Markdown index.  "
             "Defaults to <repo-root>/docs/CURRICULUM_INDEX.md",
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Suppress progress output.",
    )

    args = parser.parse_args(argv)

    # Resolve repo root.
    script_dir = Path(__file__).resolve().parent
    repo_root = Path(args.repo_root).resolve() if args.repo_root else script_dir.parent
    if not repo_root.is_dir():
        print(f"ERROR: repo-root '{repo_root}' is not a directory.", file=sys.stderr)
        return 1

    # Resolve output path.
    output_path = (
        Path(args.output).resolve()
        if args.output
        else repo_root / "docs" / "CURRICULUM_INDEX.md"
    )

    if not args.quiet:
        print(f"Scanning repository: {repo_root}")

    notes = collect_all_notes(repo_root)

    if not args.quiet:
        print(f"Found {len(notes)} TEACHING NOTE blocks.")

    markdown = build_markdown(notes, repo_root)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(markdown, encoding="utf-8")

    if not args.quiet:
        print(f"Curriculum index written to: {output_path}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
