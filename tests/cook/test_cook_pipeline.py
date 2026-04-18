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
            "script", "font", "tilemap",
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
