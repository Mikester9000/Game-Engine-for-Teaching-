#!/usr/bin/env python3
"""
audit_teaching_notes.py — TEACHING NOTE coverage audit tool.
=============================================================

TEACHING NOTE — Why Automate Code-Quality Audits?
====================================================
This project uses ``// TEACHING NOTE`` comments to explain every
non-obvious design decision.  As the codebase grows, it becomes impossible
to manually verify that every important file has adequate coverage.

Automated auditing solves this by:
  1. Running on every CI push — catches regressions before they're merged.
  2. Reporting per-file metrics — shows exactly where coverage is missing.
  3. Enforcing a minimum threshold — prevents comment debt from accumulating.

This mirrors the way professional studios use code-coverage tools (gcov,
lcov, LLVM-cov) to track test coverage: the same discipline applied to
documentation coverage.

Usage
-----
From the repository root::

    python tools/audit_teaching_notes.py [options]

Options:
  --min-density FLOAT  Minimum TEACHING NOTEs per 100 lines (default: 0.5).
  --fail-below FLOAT   Exit with code 1 if overall density is below this
                       value (default: 0.3).  Use 0 to never fail.
  --paths PATH ...     Directories / glob patterns to scan (default: src/).
  --ext .cpp .hpp ...  File extensions to check (default: .cpp .hpp .h).
  --exclude DIR ...    Sub-path fragments to exclude (e.g. build/ .git/).
  --report FORMAT      Output format: "text" (default) or "json".
  --verbose, -v        Show per-file results even when above threshold.

Exit codes:
  0  — All files at or above threshold (or --fail-below=0).
  1  — One or more files below threshold.
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass, field
from pathlib import Path


# ===========================================================================
# TEACHING NOTE — Python dataclasses (PEP 557)
# ===========================================================================
# @dataclass auto-generates __init__, __repr__, and __eq__ from annotated
# class fields.  This removes boilerplate and makes the code self-documenting:
# a reader can see all fields at a glance without searching through __init__.
#
# Comparison to namedtuple:
#   dataclass  — mutable, supports default values, inheritable.
#   namedtuple — immutable, slightly faster, tuple-compatible.
# For result aggregation we prefer dataclass for its mutability.
# ===========================================================================


@dataclass
class FileResult:
    """Coverage result for a single source file."""

    path: str
    total_lines: int
    code_lines: int          # non-blank, non-comment lines
    teaching_note_count: int
    density: float           # teaching notes per 100 code lines
    below_threshold: bool


@dataclass
class AuditResult:
    """Aggregate result for the full audit run."""

    files: list[FileResult] = field(default_factory=list)
    total_files: int = 0
    total_code_lines: int = 0
    total_notes: int = 0
    overall_density: float = 0.0
    files_below_threshold: int = 0


# ===========================================================================
# Core analysis
# ===========================================================================


def count_teaching_notes(path: Path) -> tuple[int, int, int]:
    """Count teaching notes in a C++ source file.

    TEACHING NOTE — Why count distinct note *blocks*, not note *lines*?
    A teaching note often spans multiple lines::

        // ============================================================
        // TEACHING NOTE — Title
        // ============================================================
        // Explanation line 1
        // Explanation line 2

    Counting every line that says ``TEACHING NOTE`` would count only line 2
    of the block above (the line with the actual phrase).  Instead, we count
    the number of *distinct blocks* — each one starts on the line that first
    contains the phrase "TEACHING NOTE".

    Parameters
    ----------
    path : Path
        Absolute path to the C++ source file.

    Returns
    -------
    tuple[int, int, int]
        (total_lines, code_lines, teaching_note_count)
    """
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return 0, 0, 0

    lines = text.splitlines()
    total_lines = len(lines)
    code_lines = 0
    teaching_note_count = 0

    prev_had_note = False  # de-duplicate consecutive note lines in one block

    for line in lines:
        stripped = line.strip()

        # Blank line — not a code line.
        if not stripped:
            prev_had_note = False
            continue

        # Pure comment line (starts with //).
        is_comment = stripped.startswith("//") or stripped.startswith("/*") or stripped.startswith("*")

        if not is_comment:
            code_lines += 1

        # TEACHING NOTE — Counting distinct note blocks.
        # We increment the counter when we encounter "TEACHING NOTE" for the
        # first time after a non-note line.  This means a multi-line block
        # counts as ONE note, not N lines.
        has_note_marker = "TEACHING NOTE" in stripped
        if has_note_marker and not prev_had_note:
            teaching_note_count += 1
            prev_had_note = True
        elif not has_note_marker:
            prev_had_note = False

    return total_lines, code_lines, teaching_note_count


def audit_file(
    path: Path,
    min_density: float,
) -> FileResult:
    """Produce a FileResult for one source file.

    Parameters
    ----------
    path : Path
        Absolute path to the file.
    min_density : float
        Minimum notes per 100 code lines to be considered passing.

    Returns
    -------
    FileResult
    """
    total_lines, code_lines, note_count = count_teaching_notes(path)

    # TEACHING NOTE — Division-by-zero guard
    # A file could have zero code lines (e.g., a header with only macros or
    # blank lines).  We treat such files as passing to avoid false alarms.
    density = (note_count / code_lines * 100.0) if code_lines > 0 else 100.0

    return FileResult(
        path=str(path),
        total_lines=total_lines,
        code_lines=code_lines,
        teaching_note_count=note_count,
        density=density,
        below_threshold=(code_lines > 0 and density < min_density),
    )


def run_audit(
    scan_paths: list[Path],
    extensions: list[str],
    exclude_fragments: list[str],
    min_density: float,
) -> AuditResult:
    """Scan all matching files and return an AuditResult.

    TEACHING NOTE — Generator-based file discovery with Path.rglob()
    ``Path.rglob("*.cpp")`` recursively yields all .cpp files under a
    directory.  We filter out excluded paths with a simple substring check.
    Using generators (yield-based) keeps memory usage low — no need to
    materialise the full list of thousands of files.

    Parameters
    ----------
    scan_paths : list[Path]
        Root directories to scan.
    extensions : list[str]
        File extensions to include (e.g. [".cpp", ".hpp"]).
    exclude_fragments : list[str]
        Path sub-strings that cause a file to be skipped.
    min_density : float
        Minimum notes per 100 code lines.

    Returns
    -------
    AuditResult
    """
    result = AuditResult()

    for root in scan_paths:
        if not root.exists():
            continue

        # Collect files matching any of the requested extensions.
        candidate_files: list[Path] = []
        for ext in extensions:
            candidate_files.extend(root.rglob(f"*{ext}"))

        for filepath in sorted(candidate_files):
            # Apply exclusion filters.
            filepath_str = str(filepath).replace("\\", "/")
            if any(excl in filepath_str for excl in exclude_fragments):
                continue

            file_result = audit_file(filepath, min_density)
            result.files.append(file_result)
            result.total_files += 1
            result.total_code_lines += file_result.code_lines
            result.total_notes += file_result.teaching_note_count
            if file_result.below_threshold:
                result.files_below_threshold += 1

    # Overall density across ALL code lines in the scan.
    result.overall_density = (
        result.total_notes / result.total_code_lines * 100.0
        if result.total_code_lines > 0
        else 0.0
    )

    return result


# ===========================================================================
# Output formatters
# ===========================================================================


def print_text_report(
    result: AuditResult,
    min_density: float,
    verbose: bool,
) -> None:
    """Print a human-readable text report to stdout.

    TEACHING NOTE — Console output formatting
    We use fixed-width columns aligned with ljust/rjust to make the output
    easy to scan in a terminal.  The same technique is used by tools like
    pytest, eslint, and clang-tidy for their human-readable output.
    """
    bar = "=" * 72
    print(bar)
    print("  TEACHING NOTE Coverage Audit")
    print(bar)
    print(f"  Minimum density  : {min_density:.1f} notes per 100 code lines")
    print(f"  Files scanned    : {result.total_files}")
    print(f"  Total code lines : {result.total_code_lines}")
    print(f"  Total notes      : {result.total_notes}")
    print(f"  Overall density  : {result.overall_density:.2f}")
    print()

    # Print per-file results — only failing files by default.
    if result.files_below_threshold == 0:
        print("  [PASS] All files meet the density threshold.\n")
    else:
        print(f"  [FAIL] {result.files_below_threshold} file(s) below threshold:\n")

    col_w = 60
    print(f"  {'File':<{col_w}}  {'Notes':>5}  {'Lines':>6}  {'Density':>8}")
    print("  " + "-" * (col_w + 25))

    for fr in result.files:
        if not verbose and not fr.below_threshold:
            continue
        flag = "FAIL" if fr.below_threshold else "    "
        # Shorten absolute path for readability.
        short = fr.path
        for prefix in ["src/", "editor/", "tools/"]:
            idx = fr.path.replace("\\", "/").find(prefix)
            if idx != -1:
                short = fr.path[idx:]
                break
        print(
            f"  [{flag}] {short:<{col_w - 6}}"
            f"  {fr.teaching_note_count:>5}"
            f"  {fr.code_lines:>6}"
            f"  {fr.density:>7.2f}"
        )

    if verbose or result.files_below_threshold > 0:
        print()

    print(bar)
    overall_pass = result.files_below_threshold == 0
    status = "PASS" if overall_pass else "WARN"
    print(f"  Per-file: [{status}]  {result.files_below_threshold} file(s) below per-file threshold")
    print(f"  Overall:  density={result.overall_density:.2f}  (fail-below threshold printed by caller)")
    print(bar)


def print_json_report(result: AuditResult) -> None:
    """Print a machine-readable JSON report to stdout."""
    data = {
        "summary": {
            "total_files": result.total_files,
            "total_code_lines": result.total_code_lines,
            "total_notes": result.total_notes,
            "overall_density": round(result.overall_density, 4),
            "files_below_threshold": result.files_below_threshold,
        },
        "files": [
            {
                "path": fr.path,
                "total_lines": fr.total_lines,
                "code_lines": fr.code_lines,
                "teaching_note_count": fr.teaching_note_count,
                "density": round(fr.density, 4),
                "below_threshold": fr.below_threshold,
            }
            for fr in result.files
        ],
    }
    print(json.dumps(data, indent=2))


# ===========================================================================
# CLI entry point
# ===========================================================================


def build_arg_parser() -> argparse.ArgumentParser:
    """Build and return the argument parser.

    TEACHING NOTE — argparse
    argparse is Python's standard command-line argument library.  It auto-
    generates --help text from the descriptions you provide and handles
    type conversion, defaults, and validation.  This is preferable to
    manually parsing sys.argv because it is robust and self-documenting.
    """
    p = argparse.ArgumentParser(
        prog="audit_teaching_notes.py",
        description="Audit TEACHING NOTE comment coverage in C++ source files.",
    )
    p.add_argument(
        "--min-density",
        type=float,
        default=0.5,
        metavar="FLOAT",
        help="Minimum TEACHING NOTEs per 100 code lines per file (default: 0.5).",
    )
    p.add_argument(
        "--fail-below",
        type=float,
        default=0.3,
        metavar="FLOAT",
        help=(
            "Exit with code 1 if the overall project density drops below "
            "this value (default: 0.3).  Use 0 to never fail."
        ),
    )
    p.add_argument(
        "--paths",
        nargs="+",
        default=["src", "editor"],
        metavar="PATH",
        help="Directories to scan (default: src editor).",
    )
    p.add_argument(
        "--ext",
        nargs="+",
        default=[".cpp", ".hpp", ".h"],
        metavar="EXT",
        help="File extensions to include (default: .cpp .hpp .h).",
    )
    p.add_argument(
        "--exclude",
        nargs="*",
        default=["build/", "build-", ".git/", "third_party/", "vcpkg/"],
        metavar="FRAGMENT",
        help="Path sub-strings that cause a file to be skipped.",
    )
    p.add_argument(
        "--report",
        choices=["text", "json"],
        default="text",
        help="Output format: 'text' (default) or 'json'.",
    )
    p.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Show per-file results for all files, not just failures.",
    )
    return p


def main() -> int:
    """Entry point.

    Returns
    -------
    int
        Exit code: 0 = pass, 1 = failures found.
    """
    parser = build_arg_parser()
    args = parser.parse_args()

    # Resolve scan paths relative to the script's parent directory (repo root).
    repo_root = Path(__file__).resolve().parent.parent
    scan_paths = [repo_root / p for p in args.paths]

    result = run_audit(
        scan_paths=scan_paths,
        extensions=args.ext,
        exclude_fragments=args.exclude,
        min_density=args.min_density,
    )

    if args.report == "json":
        print_json_report(result)
    else:
        print_text_report(result, args.min_density, args.verbose)

    # TEACHING NOTE — Exit code conventions
    # Unix convention: 0 = success, non-zero = failure.
    # CI tools (GitHub Actions, Jenkins) check the exit code automatically:
    # a non-zero exit marks the step as failed and stops the pipeline.
    if args.fail_below > 0 and result.overall_density < args.fail_below:
        if args.report == "text":
            print(
                f"\n[FAIL] Overall density {result.overall_density:.2f} is below "
                f"the required minimum {args.fail_below:.2f}."
            )
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
