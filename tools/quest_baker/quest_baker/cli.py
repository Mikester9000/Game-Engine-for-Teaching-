"""
cli.py — Command-line interface for the quest-baker tool.

TEACHING NOTE — Console script entry points
============================================
pyproject.toml declares:

    [project.scripts]
    quest-baker = "quest_baker.cli:main"

When the package is installed (pip install .), setuptools generates a thin
wrapper script that calls ``main()`` in this module.  This is the standard
Python packaging pattern for CLI tools — the same approach used by popular
tools like black, pytest, and mypy.

Usage:
    quest-baker quest    bake     <source> <output>
    quest-baker quest    validate <source>
    quest-baker dialogue bake     <source> <output>
    quest-baker dialogue validate <source>
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from quest_baker.baker import QuestBaker, DialogueBaker


def main() -> None:
    """Entry point for the ``quest-baker`` console script."""
    parser = argparse.ArgumentParser(
        prog="quest-baker",
        description="Quest and dialogue content baking tool (M20).",
    )
    sub = parser.add_subparsers(dest="baker", metavar="BAKER",
                                help="Which baker to use")
    sub.required = True

    # ------------------------------------------------------------------
    # quest sub-command
    # ------------------------------------------------------------------
    quest_p = sub.add_parser("quest", help="Bake or validate a quest_bank.json")
    quest_sub = quest_p.add_subparsers(dest="action", metavar="ACTION",
                                       help="bake | validate")
    quest_sub.required = True

    q_bake = quest_sub.add_parser("bake", help="Bake source → cooked")
    q_bake.add_argument("source", type=Path, help="Path to quest_bank.json")
    q_bake.add_argument("output", type=Path,
                        help="Path for cooked JSON output")

    q_val = quest_sub.add_parser("validate",
                                  help="Validate source without writing output")
    q_val.add_argument("source", type=Path, help="Path to quest_bank.json")

    # ------------------------------------------------------------------
    # dialogue sub-command
    # ------------------------------------------------------------------
    dlg_p = sub.add_parser("dialogue",
                            help="Bake or validate a dialogue_tree.json")
    dlg_sub = dlg_p.add_subparsers(dest="action", metavar="ACTION",
                                    help="bake | validate")
    dlg_sub.required = True

    d_bake = dlg_sub.add_parser("bake", help="Bake source → cooked")
    d_bake.add_argument("source", type=Path, help="Path to dialogue_tree.json")
    d_bake.add_argument("output", type=Path,
                        help="Path for cooked JSON output")

    d_val = dlg_sub.add_parser("validate",
                                help="Validate source without writing output")
    d_val.add_argument("source", type=Path,
                       help="Path to dialogue_tree.json")

    args = parser.parse_args()

    # ------------------------------------------------------------------
    # Dispatch
    # ------------------------------------------------------------------
    if args.baker == "quest":
        baker = QuestBaker()
        if args.action == "bake":
            result = baker.bake(args.source, args.output)
        else:
            result = baker.validate(args.source)
    else:  # dialogue
        baker = DialogueBaker()  # type: ignore[assignment]
        if args.action == "bake":
            result = baker.bake(args.source, args.output)
        else:
            result = baker.validate(args.source)

    # ------------------------------------------------------------------
    # Report
    # ------------------------------------------------------------------
    for warning in result.warnings:
        print(f"WARNING: {warning}")
    for error in result.errors:
        print(f"ERROR:   {error}", file=sys.stderr)

    if result.success:
        print(f"OK — {result}")
        sys.exit(0)
    else:
        sys.exit(1)


if __name__ == "__main__":
    main()
