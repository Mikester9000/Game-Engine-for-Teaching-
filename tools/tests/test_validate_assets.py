#!/usr/bin/env python3
"""
test_validate_assets.py — Unit tests for tools/validate-assets.py
==================================================================

TEACHING NOTE — Testing the Asset Pipeline Validator
------------------------------------------------------
Every lint / validation tool needs its own test suite.  These tests exercise
validate-assets.py with a set of known-good (fixtures/valid/) and known-bad
(fixtures/invalid/) sample manifests so that:

  1. Contributors can confirm the validator works before shipping a change.
  2. CI catches regressions — if someone accidentally loosens a check, a test
     will fail before the broken validator reaches production.
  3. The test fixtures serve as living documentation of what the schema allows
     and forbids.

Run locally:
  pip install pytest
  python -m pytest tools/tests/ -v

Or without pytest (stdlib unittest):
  python -m unittest discover -s tools/tests -p "test_*.py"
"""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

# ---------------------------------------------------------------------------
# Make sure the tools/ directory is importable even when running from the
# repo root or from inside tools/tests/.
# ---------------------------------------------------------------------------

_TESTS_DIR = Path(__file__).resolve().parent
_TOOLS_DIR = _TESTS_DIR.parent
_REPO_ROOT = _TOOLS_DIR.parent
_FIXTURES_VALID = _TESTS_DIR / "fixtures" / "valid"
_FIXTURES_INVALID = _TESTS_DIR / "fixtures" / "invalid"

import importlib.util as _ilu

# TEACHING NOTE — importing a module whose filename contains a hyphen
# Python identifiers cannot contain hyphens, so the normal `import` statement
# does not work for validate-assets.py.  importlib.util.spec_from_file_location
# loads the file directly and registers it under an alias.
_spec = _ilu.spec_from_file_location(
    "validate_assets", _TOOLS_DIR / "validate-assets.py"
)
va = _ilu.module_from_spec(_spec)  # type: ignore[arg-type]
_spec.loader.exec_module(va)  # type: ignore[union-attr]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _load_schema() -> dict:
    """Load the canonical schema from the repository."""
    schema_path = _REPO_ROOT / "assets" / "schema" / "asset-manifest.schema.json"
    with schema_path.open(encoding="utf-8") as fh:
        return json.load(fh)


def _load_json(path: Path) -> dict:
    with path.open(encoding="utf-8") as fh:
        return json.load(fh)


# ---------------------------------------------------------------------------
# Tests — valid sample manifests must all pass
# ---------------------------------------------------------------------------

class TestValidSampleManifests(unittest.TestCase):
    """Each fixture in fixtures/valid/ must pass validation without errors."""

    def setUp(self) -> None:
        self.schema = _load_schema()

    def _assert_passes(self, fixture_path: Path) -> None:
        self.assertTrue(fixture_path.exists(), f"Fixture not found: {fixture_path}")
        passed, errors, _warnings = va.validate_file(fixture_path, self.schema)
        self.assertTrue(
            passed,
            f"{fixture_path.name} should be valid but got errors:\n"
            + "\n".join(errors),
        )
        self.assertEqual(
            errors, [],
            f"{fixture_path.name} produced unexpected errors: {errors}",
        )

    def test_valid_audio_manifest(self) -> None:
        self._assert_passes(_FIXTURES_VALID / "audio-manifest.json")

    def test_valid_texture_manifest(self) -> None:
        self._assert_passes(_FIXTURES_VALID / "texture-manifest.json")

    def test_valid_tilemap_manifest(self) -> None:
        self._assert_passes(_FIXTURES_VALID / "tilemap-manifest.json")

    def test_valid_model_manifest(self) -> None:
        self._assert_passes(_FIXTURES_VALID / "model-manifest.json")

    def test_example_audio_manifest(self) -> None:
        """The bundled example audio manifest must also be valid."""
        self._assert_passes(
            _REPO_ROOT / "assets" / "examples" / "audio-manifest.json"
        )

    def test_example_texture_manifest(self) -> None:
        self._assert_passes(
            _REPO_ROOT / "assets" / "examples" / "texture-manifest.json"
        )

    def test_example_tilemap_manifest(self) -> None:
        self._assert_passes(
            _REPO_ROOT / "assets" / "examples" / "tilemap-manifest.json"
        )

    def test_example_model_manifest(self) -> None:
        self._assert_passes(
            _REPO_ROOT / "assets" / "examples" / "model-manifest.json"
        )


# ---------------------------------------------------------------------------
# Tests — invalid sample manifests must be rejected
# ---------------------------------------------------------------------------

class TestInvalidSampleManifests(unittest.TestCase):
    """Each fixture in fixtures/invalid/ must fail validation."""

    def setUp(self) -> None:
        self.schema = _load_schema()

    def _assert_fails(self, fixture_path: Path) -> list[str]:
        """Assert the fixture fails and return the error list for further checks."""
        self.assertTrue(fixture_path.exists(), f"Fixture not found: {fixture_path}")
        passed, errors, _warnings = va.validate_file(fixture_path, self.schema)
        self.assertFalse(
            passed,
            f"{fixture_path.name} should be invalid but passed validation",
        )
        self.assertGreater(
            len(errors),
            0,
            f"{fixture_path.name} failed but produced no error messages",
        )
        return errors

    def test_missing_manifest_version(self) -> None:
        errors = self._assert_fails(
            _FIXTURES_INVALID / "missing-manifest-version.json"
        )
        self.assertTrue(
            any("manifestVersion" in e for e in errors),
            f"Expected error about 'manifestVersion', got: {errors}",
        )

    def test_bad_hash(self) -> None:
        errors = self._assert_fails(_FIXTURES_INVALID / "bad-hash.json")
        self.assertTrue(
            any("hash" in e for e in errors),
            f"Expected error about 'hash', got: {errors}",
        )

    def test_unknown_asset_type(self) -> None:
        errors = self._assert_fails(_FIXTURES_INVALID / "unknown-asset-type.json")
        self.assertTrue(
            any("type" in e for e in errors),
            f"Expected error about 'type', got: {errors}",
        )

    def test_missing_audio_extension(self) -> None:
        errors = self._assert_fails(_FIXTURES_INVALID / "missing-audio-extension.json")
        self.assertTrue(
            any("audio" in e for e in errors),
            f"Expected error about missing 'audio' block, got: {errors}",
        )

    def test_bad_semver(self) -> None:
        errors = self._assert_fails(_FIXTURES_INVALID / "bad-semver.json")
        self.assertTrue(
            any("version" in e or "manifestVersion" in e for e in errors),
            f"Expected a SemVer error, got: {errors}",
        )

    def test_bad_loop_points(self) -> None:
        # TEACHING NOTE — JSON Schema draft-07 cannot express cross-property
        # numeric comparisons (loopEnd > loopStart).  That constraint lives in
        # the built-in structural checker.  We call it directly here so the test
        # is deterministic regardless of whether jsonschema is installed.
        manifest = _load_json(_FIXTURES_INVALID / "bad-loop-points.json")
        schema = _load_schema()
        errors = va._validate_manifest_builtin(manifest, schema)
        self.assertGreater(
            len(errors), 0,
            "bad-loop-points manifest should produce built-in validation errors",
        )
        self.assertTrue(
            any("loop" in e.lower() for e in errors),
            f"Expected loop-point error from built-in checker, got: {errors}",
        )

    def test_missing_required_fields(self) -> None:
        errors = self._assert_fails(_FIXTURES_INVALID / "missing-required-fields.json")
        self.assertTrue(
            any("id" in e or "source" in e for e in errors),
            f"Expected error about missing required fields, got: {errors}",
        )


# ---------------------------------------------------------------------------
# Tests — duplicate ID detection (warning, not a hard failure)
# ---------------------------------------------------------------------------

class TestDuplicateIdDetection(unittest.TestCase):
    """Duplicate asset IDs produce warnings without failing validation."""

    def setUp(self) -> None:
        self.schema = _load_schema()

    def test_duplicate_ids_produce_warning(self) -> None:
        fixture = _FIXTURES_INVALID / "duplicate-ids.json"
        self.assertTrue(fixture.exists(), f"Fixture not found: {fixture}")
        passed, errors, warnings = va.validate_file(fixture, self.schema)
        self.assertTrue(
            passed,
            f"Duplicate-ID manifest should pass (warnings are not errors), "
            f"but got errors: {errors}",
        )
        self.assertEqual(errors, [], f"Unexpected errors: {errors}")
        self.assertGreater(
            len(warnings),
            0,
            "Expected at least one duplicate-id warning, got none",
        )
        self.assertTrue(
            any("Duplicate" in w or "duplicate" in w for w in warnings),
            f"Expected 'Duplicate' in warning text, got: {warnings}",
        )


# ---------------------------------------------------------------------------
# Tests — CLI entry point (main())
# ---------------------------------------------------------------------------

class TestCLI(unittest.TestCase):
    """Smoke-test the main() entry point with the fixture directories."""

    def test_cli_passes_valid_directory(self) -> None:
        rc = va.main([str(_FIXTURES_VALID)])
        self.assertEqual(rc, 0, "CLI should exit 0 for all-valid fixture directory")

    def test_cli_fails_on_invalid_manifest(self) -> None:
        # Pick a single clearly-invalid fixture that must always fail
        bad_file = str(_FIXTURES_INVALID / "bad-hash.json")
        rc = va.main([bad_file])
        self.assertEqual(rc, 1, "CLI should exit 1 when a manifest is invalid")

    def test_cli_verbose_flag_accepted(self) -> None:
        rc = va.main([str(_FIXTURES_VALID), "--verbose"])
        self.assertEqual(rc, 0, "CLI with --verbose should still exit 0 for valid files")

    def test_cli_custom_schema_flag(self) -> None:
        schema_path = str(
            _REPO_ROOT / "assets" / "schema" / "asset-manifest.schema.json"
        )
        rc = va.main([str(_FIXTURES_VALID), "--schema", schema_path])
        self.assertEqual(rc, 0, "CLI with explicit --schema should exit 0 for valid files")

    def test_cli_empty_directory_exits_zero(self) -> None:
        """Passing a directory with no manifests should warn and exit 0."""
        import tempfile, os
        with tempfile.TemporaryDirectory() as tmp:
            rc = va.main([tmp])
        self.assertEqual(rc, 0, "CLI should exit 0 (warning only) for empty directory")

    def test_cli_example_manifests(self) -> None:
        """The canonical example manifests in assets/examples/ must all pass."""
        rc = va.main([str(_REPO_ROOT / "assets" / "examples")])
        self.assertEqual(
            rc, 0,
            "assets/examples/ manifests should all pass the validator",
        )


# ---------------------------------------------------------------------------
# Tests — internal helpers
# ---------------------------------------------------------------------------

class TestBuiltinChecks(unittest.TestCase):
    """Unit tests for the lower-level built-in validation helpers."""

    def test_check_asset_builtin_valid(self) -> None:
        asset = {
            "id": "audio-test",
            "version": "1.0.0",
            "type": "audio",
            "source": "audio/sfx/test.wav",
            "hash": "a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1b2",
            "audio": {
                "format": "wav",
                "sampleRate": 44100,
                "channels": 1,
                "durationSeconds": 1.0,
                "category": "sfx",
            },
        }
        errors = va._check_asset_builtin(asset, 0)
        self.assertEqual(errors, [], f"Unexpected errors: {errors}")

    def test_check_asset_builtin_missing_id(self) -> None:
        asset = {
            "version": "1.0.0",
            "type": "audio",
            "source": "audio/sfx/test.wav",
            "hash": "a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1b2",
            "audio": {
                "format": "wav",
                "sampleRate": 44100,
                "channels": 1,
                "durationSeconds": 1.0,
            },
        }
        errors = va._check_asset_builtin(asset, 0)
        self.assertTrue(any("id" in e for e in errors), f"Expected 'id' error: {errors}")

    def test_check_asset_builtin_bad_audio_format(self) -> None:
        asset = {
            "id": "audio-test",
            "version": "1.0.0",
            "type": "audio",
            "source": "audio/sfx/test.wav",
            "hash": "a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1b2",
            "audio": {
                "format": "mp4",
                "sampleRate": 44100,
                "channels": 1,
                "durationSeconds": 1.0,
            },
        }
        errors = va._check_asset_builtin(asset, 0)
        self.assertTrue(
            any("format" in e for e in errors),
            f"Expected 'format' error: {errors}",
        )

    def test_check_duplicate_ids_none(self) -> None:
        manifest = {
            "manifestVersion": "1.0.0",
            "assets": [
                {"id": "a"},
                {"id": "b"},
                {"id": "c"},
            ],
        }
        warnings = va._check_duplicate_ids(manifest)
        self.assertEqual(warnings, [])

    def test_check_duplicate_ids_found(self) -> None:
        manifest = {
            "manifestVersion": "1.0.0",
            "assets": [
                {"id": "same-id"},
                {"id": "other"},
                {"id": "same-id"},
            ],
        }
        warnings = va._check_duplicate_ids(manifest)
        self.assertGreater(len(warnings), 0)
        self.assertTrue(any("same-id" in w for w in warnings))

    def test_collect_manifests_file(self) -> None:
        fixture = _FIXTURES_VALID / "audio-manifest.json"
        result = va._collect_manifests(fixture)
        self.assertEqual(result, [fixture])

    def test_collect_manifests_directory(self) -> None:
        result = va._collect_manifests(_FIXTURES_VALID)
        self.assertGreater(len(result), 0)
        for p in result:
            self.assertTrue(p.name.endswith(".json"))
            self.assertIn("manifest", p.name)

    def test_collect_manifests_skips_schema_files(self) -> None:
        """Files ending with .schema.json must not be collected."""
        result = va._collect_manifests(
            _REPO_ROOT / "assets" / "schema"
        )
        for p in result:
            self.assertFalse(
                p.name.endswith(".schema.json"),
                f"Schema file should be excluded: {p}",
            )


if __name__ == "__main__":
    unittest.main()
