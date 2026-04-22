"""Repository-wide pytest configuration for tool package imports."""

from __future__ import annotations

import sys
from pathlib import Path


# TEACHING NOTE — Root pytest import bootstrap
# --------------------------------------------
# CI executes Python tool tests from each tool directory after `pip install -e`,
# but many contributors run `python -m pytest` once from the repository root.
# In that mode, `audio_engine`, `animation_engine`, and `quest_baker` are not on
# sys.path by default, so collection fails before tests even start.
#
# We prepend each tool root to sys.path so the full suite can run from the repo
# root without requiring per-tool editable installs.
REPO_ROOT = Path(__file__).resolve().parent
TOOL_IMPORT_ROOTS = (
    REPO_ROOT / "tools" / "audio_authoring",
    REPO_ROOT / "tools" / "anim_authoring",
    REPO_ROOT / "tools" / "quest_baker",
)

for import_root in TOOL_IMPORT_ROOTS:
    root_str = str(import_root)
    if root_str not in sys.path:
        sys.path.insert(0, root_str)
