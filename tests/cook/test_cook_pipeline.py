"""
tests/cook/test_cook_pipeline.py — Contract tests for the cook_assets.py pipeline.

TEACHING NOTE — Contract Tests
================================
Contract tests validate that the OUTPUT of a pipeline matches an agreed-upon
contract (the "golden file").  They differ from unit tests in that they test
integration across the whole pipeline rather than a single function.

Here we verify:
  1. cook_assets.py exits with code 0.
  2. AssetRegistry.json is written and validates against the JSON Schema.
  3. Every registered "cooked" path actually exists on disk.
  4. The registry contains the mandatory scene asset (MainTown).
  5. The registry structure matches the asset_registry.schema.json.
  6. cells_manifest.json — if cook.exe has been run, verify its format and
     that each entry's GUID matches a level asset in AssetRegistry.json.

Run with:
    pytest tests/cook/ -v
    # Or from repo root:
    python -m pytest tests/cook/ -v
"""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

import pytest

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
REPO_ROOT    = Path(__file__).parent.parent.parent
SAMPLE_DIR   = REPO_ROOT / "samples" / "vertical_slice_project"
REGISTRY_FILE = SAMPLE_DIR / "AssetRegistry.json"
COOKED_DIR   = SAMPLE_DIR / "Cooked"
COOK_SCRIPT  = SAMPLE_DIR / "cook_assets.py"
SCHEMA_FILE  = REPO_ROOT / "shared" / "schemas" / "asset_registry.schema.json"
CELLS_MANIFEST_FILE = COOKED_DIR / "Levels" / "cells_manifest.json"
CELLS_MANIFEST_GOLDEN = REPO_ROOT / "tests" / "golden" / "cells_manifest_expected.json"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def run_cook() -> subprocess.CompletedProcess:
    """Run cook_assets.py as a subprocess and return the result."""
    return subprocess.run(
        [sys.executable, str(COOK_SCRIPT)],
        capture_output=True,
        text=True,
        cwd=str(SAMPLE_DIR),
    )


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture(scope="module")
def cook_result() -> subprocess.CompletedProcess:
    """Run the cook pipeline once per test module.

    TEACHING NOTE — scope="module"
    Using module-level scope means the cook is only executed once even when
    multiple tests in this file each request this fixture.  This speeds up
    the test suite considerably.
    """
    return run_cook()


@pytest.fixture(scope="module")
def registry(cook_result: subprocess.CompletedProcess) -> dict:
    """Parse the AssetRegistry.json after a successful cook."""
    assert cook_result.returncode == 0, (
        f"cook_assets.py failed (exit {cook_result.returncode}).\n"
        f"stdout:\n{cook_result.stdout}\n"
        f"stderr:\n{cook_result.stderr}"
    )
    return json.loads(REGISTRY_FILE.read_text(encoding="utf-8"))


# ---------------------------------------------------------------------------
# Tests — cook script invocation
# ---------------------------------------------------------------------------

class TestCookInvocation:
    """Verify that the cook script can be invoked and exits cleanly."""

    def test_cook_exits_zero(self, cook_result: subprocess.CompletedProcess) -> None:
        """Cook script must exit with code 0 on the sample project."""
        assert cook_result.returncode == 0, (
            f"cook_assets.py exited with {cook_result.returncode}.\n"
            f"stderr: {cook_result.stderr}"
        )

    def test_cook_produces_registry_file(self, cook_result: subprocess.CompletedProcess) -> None:
        """cook_assets.py must write AssetRegistry.json."""
        assert cook_result.returncode == 0
        assert REGISTRY_FILE.exists(), "AssetRegistry.json was not written"

    def test_cook_produces_cooked_dir(self, cook_result: subprocess.CompletedProcess) -> None:
        """cook_assets.py must create the Cooked/ directory."""
        assert cook_result.returncode == 0
        assert COOKED_DIR.exists(), "Cooked/ directory was not created"


# ---------------------------------------------------------------------------
# Tests — registry structure
# ---------------------------------------------------------------------------

class TestRegistryStructure:
    """Verify the schema and mandatory fields in AssetRegistry.json."""

    def test_registry_has_version(self, registry: dict) -> None:
        """Registry must have a 'version' field."""
        assert "version" in registry, "Missing 'version' key"

    def test_registry_version_is_semver(self, registry: dict) -> None:
        """Registry version must be a SemVer string (e.g. '1.0.0')."""
        import re
        version = registry.get("version", "")
        assert re.match(r"^\d+\.\d+\.\d+$", version), f"version '{version}' is not SemVer"

    def test_registry_has_assets_list(self, registry: dict) -> None:
        """Registry must have an 'assets' array."""
        assert "assets" in registry
        assert isinstance(registry["assets"], list)

    def test_registry_validates_against_schema(self, registry: dict) -> None:
        """Registry must validate against asset_registry.schema.json.

        TEACHING NOTE — JSON Schema Validation
        We use the  jsonschema  library to validate the registry programmatically.
        This is the same check that  tools/validate-assets.py  performs; having
        it here means a failing cook is caught immediately in pytest output.
        """
        try:
            import jsonschema  # type: ignore
        except ImportError:
            pytest.skip("jsonschema not installed — skipping schema validation")

        schema = json.loads(SCHEMA_FILE.read_text(encoding="utf-8"))
        # Remove the $schema field from both documents before validation
        # because jsonschema draft-07 does not download the meta-schema
        schema_for_validate = {k: v for k, v in schema.items() if k != "$schema"}
        registry_for_validate = {k: v for k, v in registry.items() if k != "$schema"}
        jsonschema.validate(instance=registry_for_validate, schema=schema_for_validate)


# ---------------------------------------------------------------------------
# Tests — asset entries
# ---------------------------------------------------------------------------

class TestAssetEntries:
    """Verify the content of individual asset entries."""

    def test_each_asset_has_required_fields(self, registry: dict) -> None:
        """Every asset entry must have id, type, and source."""
        for asset in registry.get("assets", []):
            assert "id"     in asset, f"Asset missing 'id': {asset}"
            assert "type"   in asset, f"Asset missing 'type': {asset}"
            assert "source" in asset, f"Asset missing 'source': {asset}"

    def test_asset_ids_are_valid_uuids(self, registry: dict) -> None:
        """Asset IDs must be valid UUID v4 strings."""
        import re
        uuid_pattern = re.compile(
            r"^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$"
        )
        for asset in registry.get("assets", []):
            asset_id = asset.get("id", "")
            assert uuid_pattern.match(asset_id), (
                f"Asset id '{asset_id}' is not a valid UUID v4"
            )

    def test_asset_ids_are_unique(self, registry: dict) -> None:
        """All asset GUIDs must be unique within a registry."""
        ids = [a["id"] for a in registry.get("assets", [])]
        assert len(ids) == len(set(ids)), "Duplicate asset IDs detected"

    def test_asset_types_are_valid(self, registry: dict) -> None:
        """Asset types must be from the allowed enum in the schema."""
        valid_types = {
            "texture", "mesh", "material", "audio", "audio_bank",
            "scene", "skeleton", "anim_clip", "anim_graph",
            "script", "font", "tilemap", "level",
            "collision", "road",
        }
        for asset in registry.get("assets", []):
            assert asset.get("type") in valid_types, (
                f"Unknown asset type: {asset.get('type')}"
            )

    def test_cooked_paths_exist(self, registry: dict) -> None:
        """Every asset with a 'cooked' field must have the file present on disk.

        TEACHING NOTE — Golden Path Verification
        This test embodies the main contract: if the cook script says an asset
        is at a cooked path, that file MUST exist.  A missing cooked file means
        the registry is out of sync with the file system — a fatal engine error.
        """
        for asset in registry.get("assets", []):
            cooked = asset.get("cooked")
            if cooked:
                full_path = SAMPLE_DIR / cooked
                assert full_path.exists(), (
                    f"Asset '{asset.get('name', asset['id'])}' cooked path "
                    f"'{cooked}' does not exist at {full_path}"
                )

    def test_mandatory_scene_asset_present(self, registry: dict) -> None:
        """The sample project must include the MainTown scene asset.

        TEACHING NOTE — Golden File
        This is the minimal "golden" contract for the vertical slice: at least
        one scene (MainTown) must be cooked and registered.  Add more golden
        assertions here as the project grows.
        """
        scene_assets = [
            a for a in registry.get("assets", [])
            if a.get("type") == "scene" and "MainTown" in a.get("name", "")
        ]
        assert len(scene_assets) >= 1, (
            "Expected at least one 'scene' asset named MainTown in the registry"
        )

    def test_mandatory_material_asset_present(self, registry: dict) -> None:
        """The sample project must include at least one cooked material asset."""
        material_assets = [
            a for a in registry.get("assets", [])
            if a.get("type") == "material"
        ]
        assert len(material_assets) >= 1, (
            "Expected at least one 'material' asset in the registry"
        )
        for asset in material_assets:
            cooked = asset.get("cooked", "")
            assert cooked.endswith(".material"), (
                f"Material asset cooked path should end with .material, got '{cooked}'"
            )


# ---------------------------------------------------------------------------
# Tests — cells_manifest.json (generated by cook.exe, not cook_assets.py)
# ---------------------------------------------------------------------------

class TestCellsManifest:
    """Validate the cells_manifest.json produced by cook.exe.

    TEACHING NOTE — cook.exe vs cook_assets.py
    cells_manifest.json is generated by the C++ cook.exe tool, NOT by the
    Python cook_assets.py script.  These tests skip gracefully if cook.exe
    has not been run (e.g. in the Linux contract-tests CI job which uses
    only cook_assets.py).  The authoritative validation of cells_manifest.json
    is performed in the build-windows.yml CI step 6b.
    """

    @pytest.fixture(scope="class")
    def manifest(self) -> dict:
        """Load cells_manifest.json; skip if not present (cook.exe not run)."""
        if not CELLS_MANIFEST_FILE.exists():
            pytest.skip(
                "cells_manifest.json not found — run cook.exe first "
                "(cmake --build <preset> --target cook_samples)"
            )
        return json.loads(CELLS_MANIFEST_FILE.read_text(encoding="utf-8"))

    def test_manifest_has_version(self, manifest: dict) -> None:
        """Manifest must have a 'version' field equal to 1."""
        assert "version" in manifest, "cells_manifest.json missing 'version'"
        assert manifest["version"] == 1, (
            f"Expected version 1, got {manifest['version']}"
        )

    def test_manifest_has_cells_list(self, manifest: dict) -> None:
        """Manifest must have a 'cells' array."""
        assert "cells" in manifest, "cells_manifest.json missing 'cells'"
        assert isinstance(manifest["cells"], list), "'cells' must be a list"

    def test_manifest_cells_not_empty(self, manifest: dict) -> None:
        """The vertical slice project must have at least one cooked level cell."""
        assert len(manifest["cells"]) >= 1, (
            "cells_manifest.json has no entries — expected ≥1 level cell from "
            "AssetRegistry.json"
        )

    def test_manifest_cell_entries_have_required_fields(self, manifest: dict) -> None:
        """Each cell entry must have 'cx', 'cz', and 'guid' fields."""
        for entry in manifest.get("cells", []):
            assert "cx"   in entry, f"Cell entry missing 'cx': {entry}"
            assert "cz"   in entry, f"Cell entry missing 'cz': {entry}"
            assert "guid" in entry, f"Cell entry missing 'guid': {entry}"

    def test_manifest_cell_guids_match_registry(self, manifest: dict) -> None:
        """Each cell GUID must appear in AssetRegistry.json as a level asset.

        TEACHING NOTE — Data-driven GUID verification
        This test confirms that cook.exe derived the GUIDs from the actual
        registry (not from a hardcoded list).  If AssetRegistry.json GUIDs
        change and cook.exe is rerun, this test automatically passes with
        the new values.
        """
        import re
        uuid_pattern = re.compile(
            r"^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
            re.IGNORECASE,
        )

        registry = json.loads(REGISTRY_FILE.read_text(encoding="utf-8"))
        level_guids = {
            a["id"]
            for a in registry.get("assets", [])
            if a.get("type") == "level"
        }

        for entry in manifest.get("cells", []):
            guid = entry.get("guid", "")
            assert uuid_pattern.match(guid), (
                f"Cell guid '{guid}' is not a valid UUID"
            )
            assert guid in level_guids, (
                f"Cell guid '{guid}' (cx={entry.get('cx')}, cz={entry.get('cz')}) "
                f"not found in AssetRegistry.json level assets"
            )

    def test_manifest_cell_coords_are_non_negative(self, manifest: dict) -> None:
        """All cell coordinates must be non-negative integers."""
        for entry in manifest.get("cells", []):
            cx = entry.get("cx")
            cz = entry.get("cz")
            assert isinstance(cx, int) and cx >= 0, (
                f"Cell 'cx' must be a non-negative int, got {cx!r}"
            )
            assert isinstance(cz, int) and cz >= 0, (
                f"Cell 'cz' must be a non-negative int, got {cz!r}"
            )

    def test_manifest_matches_golden_file(self, manifest: dict) -> None:
        """cells_manifest.json must match tests/golden/cells_manifest_expected.json.

        TEACHING NOTE — Golden File Comparison
        The golden file (tests/golden/cells_manifest_expected.json) records the
        expected set of level-cell GUIDs for the vertical-slice project.  This
        test catches accidental GUID changes: if AssetRegistry.json is modified
        (e.g. an asset is re-imported with a new GUID) the golden file must be
        updated deliberately and reviewed in the PR — preventing silent breakage
        of the streaming pipeline.

        The golden file is committed to the repository and must always be present.
        A missing golden file is a repository integrity regression, not a skip.

        Comparison is order-independent: we sort both sides by (cx, cz) so that
        cook.exe output order does not cause false failures.
        """
        # TEACHING NOTE — Committed golden files must always exist.
        # Using assert rather than pytest.skip() ensures that accidental deletion
        # of the golden file fails loudly in CI instead of silently passing.
        assert CELLS_MANIFEST_GOLDEN.exists(), (
            f"Golden file not found: {CELLS_MANIFEST_GOLDEN} — "
            "this file is committed to the repository and must not be deleted. "
            "Restore it from version control or regenerate by running cook.exe."
        )

        golden = json.loads(CELLS_MANIFEST_GOLDEN.read_text(encoding="utf-8"))

        # Check top-level version matches.
        assert manifest.get("version") == golden.get("version"), (
            f"cells_manifest.json version {manifest.get('version')!r} does not match "
            f"golden version {golden.get('version')!r}.  "
            f"Update tests/golden/cells_manifest_expected.json if a version bump is intentional."
        )

        def sort_cells(cells: list) -> list:
            return sorted(cells, key=lambda c: (c.get("cx", 0), c.get("cz", 0)))

        actual_cells = sort_cells(manifest.get("cells", []))
        golden_cells = sort_cells(golden.get("cells", []))

        assert len(actual_cells) == len(golden_cells), (
            f"cells_manifest.json has {len(actual_cells)} cell(s); "
            f"golden file has {len(golden_cells)}.  "
            f"Update tests/golden/cells_manifest_expected.json if intentional."
        )

        for actual, expected in zip(actual_cells, golden_cells):
            assert actual.get("cx") == expected.get("cx"), (
                f"Cell cx mismatch: actual={actual.get('cx')} expected={expected.get('cx')} "
                f"(entry: {actual}).  Update tests/golden/cells_manifest_expected.json if intentional."
            )
            assert actual.get("cz") == expected.get("cz"), (
                f"Cell cz mismatch: actual={actual.get('cz')} expected={expected.get('cz')} "
                f"(entry: {actual}).  Update tests/golden/cells_manifest_expected.json if intentional."
            )
            assert actual.get("guid") == expected.get("guid"), (
                f"Cell guid mismatch: actual={actual.get('guid')!r} expected={expected.get('guid')!r} "
                f"at ({actual.get('cx')},{actual.get('cz')}).  "
                f"Update tests/golden/cells_manifest_expected.json if intentional."
            )
