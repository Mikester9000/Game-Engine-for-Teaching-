#!/usr/bin/env python3
"""
tools/ci/check_teaching_standards.py
=====================================
Teaching Standards Enforcement Script

TEACHING NOTE — Why this script exists
---------------------------------------
This project is built to be learned from. Two rules keep it teachable even as
the codebase grows, and this script enforces both automatically:

  1. FILE SIZE LIMIT (500 lines)
     Small files are easier to read, understand, and modify. A Copilot session
     that only needs to read one focused 200-line file is far more effective
     than one that must ingest a 2 000-line monolith. The limit applies to
     new and changed files only; it is not applied retroactively to deliberate
     teaching exceptions like ECS.hpp.

  2. TEACHING NOTE REQUIREMENT
     Every non-trivial C++ or Python file must contain at least one block that
     starts with "// TEACHING NOTE" (C++) or "# TEACHING NOTE" (Python). This
     ensures the reasoning behind every design decision is captured in source
     code where students will actually read it, not buried in a separate doc.

USAGE
-----
  # Check specific files (e.g., passed by a CI workflow):
  python3 tools/ci/check_teaching_standards.py src/engine/core/Logger.cpp

  # Check all files changed since a git base ref (recommended for CI):
  python3 tools/ci/check_teaching_standards.py --git-diff origin/main

  # Check all files changed since the previous commit (useful on direct push):
  python3 tools/ci/check_teaching_standards.py --git-diff HEAD~1

  # Run a full audit of every tracked file (useful for periodic audits):
  python3 tools/ci/check_teaching_standards.py --all

ALLOWLISTS
----------
Files listed in ALLOWLIST_SIZE are exempt from the 500-line limit. They are
deliberate teaching exceptions where studying the whole file as one unit is
part of the lesson (e.g. ECS.hpp teaches the complete ECS pattern at once).

Files listed in ALLOWLIST_TEACHING_NOTE are exempt from the teaching-note
requirement. These are auto-generated files, trivial glue code, pure data
files, or infrastructure that has no teaching content to annotate.

To add a new exception, add its repo-relative path to the appropriate
allowlist constant below and add a short comment explaining why it is exempt.

EXIT CODE
---------
  0 — all checks passed (or no relevant files were found)
  1 — one or more checks failed (CI job will be marked as failed)
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# TEACHING NOTE — Configuration as Named Constants
# ---------------------------------------------------------------------------
# All tunable values live here as module-level named constants. This is a
# Python best practice: "magic numbers" buried inside functions are opaque,
# but named constants at the top of the file are immediately discoverable and
# easy to change without reading through the logic.
# ---------------------------------------------------------------------------

# Maximum number of lines allowed in a single source file.
MAX_FILE_LINES: int = 500

# Minimum number of lines a file must have before we require a TEACHING NOTE.
# Very short files (e.g. a 10-line forward-declaration header or an empty
# __init__.py) have nothing substantial to annotate.
MIN_LINES_FOR_TEACHING_NOTE: int = 30

# File extensions subject to the teaching-note requirement.
TEACHING_NOTE_EXTENSIONS: frozenset = frozenset({".cpp", ".hpp", ".h", ".py"})

# File extensions subject to the line-count limit.
SIZE_CHECK_EXTENSIONS: frozenset = frozenset(
    {".cpp", ".hpp", ".h", ".py", ".cmake", ".yml", ".yaml"}
)

# ---------------------------------------------------------------------------
# TEACHING NOTE — TEACHING NOTE Detection Pattern
# ---------------------------------------------------------------------------
# We pre-compile the regex at module level (rather than inside check_teaching_note)
# so it is compiled exactly once, regardless of how many files are checked.
#
# The pattern matches lines where "TEACHING NOTE" appears after a comment
# prefix.  We allow optional leading whitespace and a "//" or "#" followed by
# optional whitespace before the marker.  This correctly matches:
#   // TEACHING NOTE — …       (C++)
#   # TEACHING NOTE — …        (Python / YAML / CMake)
#   //TEACHING NOTE             (no space — also accepted)
#
# The pattern does NOT match:
#   str = "TEACHING NOTE"       (string literal — no comment prefix)
#   teaching_note_required = … (variable name — not a comment prefix)
# ---------------------------------------------------------------------------
_TEACHING_NOTE_RE: re.Pattern = re.compile(
    r"^\s*(?://|#)\s*TEACHING NOTE", re.MULTILINE
)

# ---------------------------------------------------------------------------
# TEACHING NOTE — Allowlist Design
# ---------------------------------------------------------------------------
# Some files are intentional exceptions to the rules above. We use an
# explicit allowlist (rather than disabling the check globally or using a
# .gitignore-style pattern file) so that:
#   a) New exceptions must be deliberately added here (not silently skipped).
#   b) The allowlist is version-controlled and visible in PRs.
#   c) Students can see which files are "special" and why.
#
# Each entry uses the repo-relative path (no leading slash).
# ---------------------------------------------------------------------------

# Files exempt from the 500-line size limit.
ALLOWLIST_SIZE: frozenset = frozenset(
    {
        # ECS.hpp is ~2 000 lines by design — the full Entity-Component-System
        # implementation is studied as a single teaching unit. Splitting it
        # would destroy the lesson.
        "src/engine/ecs/ECS.hpp",
        # The asset validator is a standalone CLI tool; its size is driven by
        # the breadth of schema validation logic, not by poor modularity.
        "tools/validate-assets.py",
        # The audio engine core bundles synthesis, DSP, and export in one
        # module that is taught as a complete audio pipeline.
        "tools/audio_authoring/audio_engine/audio_engine.py",
        # The animation engine core bundles skeleton, IK, and blend-tree
        # concepts in one file taught as a complete animation pipeline.
        "tools/anim_authoring/animation_engine/animation_engine.py",
        # The creation engine CLI is a multi-command tool that grows with
        # each new asset type; it is exempt until it can be split by type.
        "tools/creation_engine.py",
        # The audio engine CLI wraps the authoring library; size driven by
        # number of sub-commands, not by coupling.
        "tools/audio_engine.py",
        # The CI teaching-standards script itself is comprehensive by design —
        # it includes a full docstring, thorough TEACHING NOTE blocks, and
        # detailed allowlists that make it an educational example of a CI tool.
        "tools/ci/check_teaching_standards.py",
    }
)

# Files exempt from the TEACHING NOTE requirement.
# These are auto-generated files, trivial glue code, pure data files, or
# infrastructure configuration that has no teachable design to annotate.
ALLOWLIST_TEACHING_NOTE: frozenset = frozenset(
    {
        "__init__.py",   # Package namespace files (usually empty)
        "setup.py",      # Packaging boilerplate
        "conftest.py",   # pytest fixture configuration
    }
)

# Path substrings: if a file's repo-relative path contains any of these
# prefixes the file is exempt from the teaching-note check.
ALLOWLIST_TEACHING_NOTE_PATHS: tuple = (
    "build/",           # CMake build artifacts
    "build-test/",      # CMake build artifacts
    ".git/",            # Git internals
    "__pycache__/",     # Python bytecode cache
    "samples/",         # Sample data files (teaching is in src/, not samples)
    "assets/examples/", # Example manifest data (JSON, not code)
    "shared/schemas/",  # JSON Schema definitions (not executable code)
    "tools/tests/",     # Pytest fixtures/data files
)


# ---------------------------------------------------------------------------
# TEACHING NOTE — Helper: resolve repo root
# ---------------------------------------------------------------------------
# We compute the repository root once at startup so that all repo-relative
# path calculations are consistent, regardless of the working directory the
# script is invoked from.
# ---------------------------------------------------------------------------
def _repo_root() -> Path:
    """Return the absolute path to the repository root."""
    result = subprocess.run(
        ["git", "rev-parse", "--show-toplevel"],
        capture_output=True,
        text=True,
        check=True,
    )
    return Path(result.stdout.strip())


# ---------------------------------------------------------------------------
# TEACHING NOTE — Collecting Changed Files
# ---------------------------------------------------------------------------
# In CI the script is typically told "check the files that changed in this
# PR". We collect that list by running `git diff --name-only <base>..HEAD`.
# The `--diff-filter=ACMR` flag limits output to Added, Copied, Modified, and
# Renamed files; deleted files are excluded because they no longer exist on
# disk and therefore cannot violate the standards.
# ---------------------------------------------------------------------------
def _get_changed_files(base_ref: str, repo_root: Path) -> list:
    """
    Return a list of Path objects for files changed between *base_ref* and HEAD.

    Parameters
    ----------
    base_ref : str
        A git ref (branch name, tag, or commit SHA) to diff against.
    repo_root : Path
        Absolute path to the repository root.

    Returns
    -------
    list[Path]
        Absolute paths of changed (added/modified/renamed) files.
    """
    result = subprocess.run(
        ["git", "diff", "--name-only", "--diff-filter=ACMR", f"{base_ref}...HEAD"],
        capture_output=True,
        text=True,
        cwd=str(repo_root),
    )
    if result.returncode != 0:
        # TEACHING NOTE — Fallback diff syntax
        # We prefer the three-dot syntax (A...B) for PRs: it finds the
        # merge-base between A and B and computes the diff from there, which
        # correctly shows only the commits unique to the PR branch.
        #
        # If three-dot fails (e.g. A is a short ref like HEAD~1 that git
        # cannot resolve with the merge-base algorithm), we fall back to the
        # two-dot syntax (A..B), which diffs directly between two commit
        # objects. For a linear branch history (HEAD~1 is always a direct
        # ancestor of HEAD) both produce identical output.
        result = subprocess.run(
            ["git", "diff", "--name-only", "--diff-filter=ACMR", f"{base_ref}..HEAD"],
            capture_output=True,
            text=True,
            cwd=str(repo_root),
        )
    paths = []
    for line in result.stdout.splitlines():
        line = line.strip()
        if line:
            paths.append(repo_root / line)
    return paths


def _get_all_tracked_files(repo_root: Path) -> list:
    """
    Return a list of Path objects for every file tracked by git.

    Parameters
    ----------
    repo_root : Path
        Absolute path to the repository root.

    Returns
    -------
    list[Path]
        Absolute paths of all tracked files.
    """
    result = subprocess.run(
        ["git", "ls-files"],
        capture_output=True,
        text=True,
        cwd=str(repo_root),
        check=True,
    )
    paths = []
    for line in result.stdout.splitlines():
        line = line.strip()
        if line:
            paths.append(repo_root / line)
    return paths


# ---------------------------------------------------------------------------
# TEACHING NOTE — The Two Core Checks
# ---------------------------------------------------------------------------
# Each check is a pure function that receives a Path and returns either None
# (pass) or a human-readable error string (fail). Pure functions are easy to
# unit-test and easy to reason about in isolation.
# ---------------------------------------------------------------------------

def check_file_size(path: Path, repo_relative: str) -> str | None:
    """
    Fail if *path* exceeds MAX_FILE_LINES lines.

    Parameters
    ----------
    path : Path
        Absolute path to the file.
    repo_relative : str
        Repo-relative path string used for allowlist lookup and messages.

    Returns
    -------
    str | None
        An error message, or None if the file passes.
    """
    if path.suffix not in SIZE_CHECK_EXTENSIONS:
        return None  # Not a file type we size-check.

    if repo_relative in ALLOWLIST_SIZE:
        return None  # Deliberate teaching exception.

    try:
        line_count = sum(1 for _ in path.open(encoding="utf-8", errors="replace"))
    except OSError:
        return None  # File not readable; skip silently.

    if line_count > MAX_FILE_LINES:
        return (
            f"  SIZE  {repo_relative} — {line_count} lines "
            f"(limit: {MAX_FILE_LINES}). Split into smaller files or add to "
            f"ALLOWLIST_SIZE in tools/ci/check_teaching_standards.py."
        )
    return None


def check_teaching_note(path: Path, repo_relative: str) -> str | None:
    """
    Fail if *path* is a non-trivial C++/Python file with no TEACHING NOTE block.

    Parameters
    ----------
    path : Path
        Absolute path to the file.
    repo_relative : str
        Repo-relative path string used for allowlist lookup and messages.

    Returns
    -------
    str | None
        An error message, or None if the file passes.
    """
    if path.suffix not in TEACHING_NOTE_EXTENSIONS:
        return None  # Not a file type we check for teaching notes.

    # Check filename-based allowlist (exact filename match).
    if path.name in ALLOWLIST_TEACHING_NOTE:
        return None

    # Check path-prefix allowlist.
    for prefix in ALLOWLIST_TEACHING_NOTE_PATHS:
        if prefix in repo_relative:
            return None

    try:
        content = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return None  # File not readable; skip silently.

    lines = content.splitlines()
    if len(lines) < MIN_LINES_FOR_TEACHING_NOTE:
        return None  # Too short to require a teaching note.

    # TEACHING NOTE — Detection Pattern
    # We look for the canonical marker strings used throughout the project:
    #   C++ : "// TEACHING NOTE"
    #   Python : "# TEACHING NOTE"
    # A case-sensitive match is intentional — contributors must spell the
    # marker exactly as defined in the coding standards.
    # The pre-compiled _TEACHING_NOTE_RE pattern (defined at module level)
    # only matches the marker when it appears after a "//" or "#" comment
    # prefix, preventing false positives from string literals or variable names.
    if not _TEACHING_NOTE_RE.search(content):
        return (
            f"  NOTE  {repo_relative} — no TEACHING NOTE block found "
            f"({len(lines)} lines). Add at least one '// TEACHING NOTE' "
            f"(C++) or '# TEACHING NOTE' (Python) comment explaining the "
            f"design intent. See docs/COPILOT_CONTINUATION.md §3.1."
        )
    return None


# ---------------------------------------------------------------------------
# TEACHING NOTE — Main Entry Point
# ---------------------------------------------------------------------------
# The main() function wires together: argument parsing → file collection →
# running both checks on each file → reporting results → exit code. Keeping
# all orchestration here (rather than at module level) allows the module to
# be imported by tests without side-effects.
# ---------------------------------------------------------------------------

def main(argv: list | None = None) -> int:
    """
    Run teaching-standards checks and return an exit code.

    Parameters
    ----------
    argv : list | None
        Argument list for testing. If None, sys.argv[1:] is used.

    Returns
    -------
    int
        0 if all checks passed, 1 if any check failed.
    """
    parser = argparse.ArgumentParser(
        description="Enforce teaching standards (file size + TEACHING NOTE).",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "files",
        nargs="*",
        help="Explicit list of files to check (repo-relative or absolute).",
    )
    parser.add_argument(
        "--git-diff",
        metavar="BASE_REF",
        help=(
            "Check files changed between BASE_REF and HEAD "
            "(e.g. --git-diff origin/main or --git-diff HEAD~1)."
        ),
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Check every file tracked by git (full audit mode).",
    )
    parser.add_argument(
        "--no-size-check",
        action="store_true",
        help="Skip the file-size check (useful when only auditing notes).",
    )
    parser.add_argument(
        "--no-note-check",
        action="store_true",
        help="Skip the teaching-note check (useful when only auditing size).",
    )

    args = parser.parse_args(argv)

    # Resolve the repository root once.
    try:
        repo_root = _repo_root()
    except subprocess.CalledProcessError:
        print("ERROR: Could not determine git repository root.", file=sys.stderr)
        print("       Run this script from inside the repository.", file=sys.stderr)
        return 1

    # ---------------------------------------------------------------------------
    # Collect the list of files to check.
    # ---------------------------------------------------------------------------
    paths_to_check: list[Path] = []

    if args.all:
        paths_to_check = _get_all_tracked_files(repo_root)
    elif args.git_diff:
        paths_to_check = _get_changed_files(args.git_diff, repo_root)
    elif args.files:
        for f in args.files:
            p = Path(f)
            if not p.is_absolute():
                p = Path.cwd() / p
            paths_to_check.append(p.resolve())
    else:
        # No input provided — default to changed files vs. HEAD~1.
        print(
            "INFO: No files or --git-diff specified; "
            "defaulting to --git-diff HEAD~1"
        )
        paths_to_check = _get_changed_files("HEAD~1", repo_root)

    if not paths_to_check:
        print("No relevant files to check. All checks passed.")
        return 0

    # ---------------------------------------------------------------------------
    # Run checks on each file and collect failures.
    # ---------------------------------------------------------------------------
    failures: list[str] = []

    for path in paths_to_check:
        if not path.exists():
            continue  # Deleted files; nothing to check.

        # Compute the repo-relative path for display and allowlist matching.
        try:
            rel = str(path.relative_to(repo_root))
        except ValueError:
            rel = str(path)  # Fallback if outside repo (unusual).

        # Normalise path separators to forward-slash so allowlist entries work
        # cross-platform (Windows uses backslash internally).
        rel = rel.replace("\\", "/")

        if not args.no_size_check:
            err = check_file_size(path, rel)
            if err:
                failures.append(err)

        if not args.no_note_check:
            err = check_teaching_note(path, rel)
            if err:
                failures.append(err)

    # ---------------------------------------------------------------------------
    # Report results.
    # ---------------------------------------------------------------------------
    checked_count = len(paths_to_check)
    print(f"Checked {checked_count} file(s).")

    if failures:
        print(f"\n{'='*70}")
        print(f"TEACHING STANDARDS — {len(failures)} violation(s) found:\n")
        for msg in failures:
            print(msg)
        print(f"\n{'='*70}")
        print(
            "\nHow to fix:\n"
            "  SIZE violations  — split the file into smaller focused modules,\n"
            "                     or add it to ALLOWLIST_SIZE with a comment.\n"
            "  NOTE violations  — add a '// TEACHING NOTE' (C++) or\n"
            "                     '# TEACHING NOTE' (Python) comment block\n"
            "                     explaining the design intent of the file.\n"
            "\nSee docs/COPILOT_CONTINUATION.md §3 for full coding standards."
        )
        return 1

    print("All teaching-standards checks passed. ✓")
    return 0


if __name__ == "__main__":
    sys.exit(main())
