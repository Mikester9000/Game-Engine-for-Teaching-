#!/usr/bin/env python3
"""
validate-assets.py — Asset Manifest Validator CLI
==================================================

TEACHING NOTE — Asset Pipeline Validation
------------------------------------------
In a professional game engine every asset that ships in the final package must
pass through a validation gate.  This script is that gate.  It reads one or
more manifest files (JSON), checks them against the shared JSON schema, and
reports per-asset errors so that artists and engineers get actionable feedback
immediately — before a broken asset can stall a build.

Usage
-----
  # Validate a single manifest file
  python3 tools/validate-assets.py assets/examples/audio-manifest.json

  # Validate every manifest in a directory (recursive)
  python3 tools/validate-assets.py assets/examples/

  # Validate multiple paths in one go
  python3 tools/validate-assets.py assets/examples/ path/to/other/manifest.json

  # Show this help message
  python3 tools/validate-assets.py --help

Exit codes
----------
  0 — all manifests passed
  1 — one or more validation errors were found
  2 — usage / filesystem error

Dependencies
------------
The validator works out-of-the-box with Python 3.8+ stdlib only.
If the optional `jsonschema` package is available it is used for full
JSON-Schema draft-07 validation.  Without it the validator falls back to a
built-in structural check that covers all required fields.

  pip install jsonschema          # optional but recommended
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from pathlib import Path
from typing import Any, Dict, List, Tuple

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

_TOOLS_DIR = Path(__file__).resolve().parent
_REPO_ROOT = _TOOLS_DIR.parent
_SCHEMA_PATH = _REPO_ROOT / "assets" / "schema" / "asset-manifest.schema.json"
_ASSET_REGISTRY_SCHEMA_PATH = _REPO_ROOT / "shared" / "schemas" / "asset_registry.schema.json"

# ---------------------------------------------------------------------------
# Colour helpers (ANSI, disabled on Windows without VT mode)
# ---------------------------------------------------------------------------

_USE_COLOUR = sys.stdout.isatty() and os.name != "nt"


def _c(text: str, code: str) -> str:
    return f"\033[{code}m{text}\033[0m" if _USE_COLOUR else text


def _green(t: str) -> str:
    return _c(t, "32")


def _yellow(t: str) -> str:
    return _c(t, "33")


def _red(t: str) -> str:
    return _c(t, "31")


def _bold(t: str) -> str:
    return _c(t, "1")


# ---------------------------------------------------------------------------
# Schema loader
# ---------------------------------------------------------------------------

def _load_schema(path: Path | None = None) -> Dict[str, Any]:
    """Load the canonical asset manifest schema from disk."""
    schema_path = path or _SCHEMA_PATH
    if not schema_path.exists():
        _die(
            f"Schema file not found: {schema_path}\n"
            "Make sure you run this tool from the repository root or that the\n"
            "assets/schema/asset-manifest.schema.json file exists."
        )
    with schema_path.open(encoding="utf-8") as fh:
        return json.load(fh)


def _try_load_json_object(path: Path) -> Dict[str, Any] | None:
    """Read a JSON file and return its root object, or None on parse/read error."""
    try:
        with path.open(encoding="utf-8") as fh:
            data = json.load(fh)
    except (OSError, json.JSONDecodeError):
        return None
    return data if isinstance(data, dict) else None


def _is_asset_registry_manifest(manifest: Dict[str, Any]) -> bool:
    """Heuristic detection for AssetRegistry.json-style documents.

    TEACHING NOTE — Backward-compatible manifest detection
    ------------------------------------------------------
    The repo currently has two JSON families:
      1) assets/schema/asset-manifest.schema.json   (legacy manifestVersion root)
      2) shared/schemas/asset_registry.schema.json  (runtime registry root)

    Contributors often run this tool directly on AssetRegistry.json.  Auto-
    detecting the shape here prevents false failures caused by validating a
    registry document against the legacy manifest schema.
    """
    schema_ref = manifest.get("$schema")
    if isinstance(schema_ref, str) and "asset_registry.schema.json" in schema_ref:
        return True

    # Fallback shape-based detection for documents that omit $schema.
    has_assets = isinstance(manifest.get("assets"), list)
    has_version = isinstance(manifest.get("version"), str)
    has_manifest_version = isinstance(manifest.get("manifestVersion"), str)
    return has_assets and has_version and not has_manifest_version


def _is_asset_registry_schema(schema: Dict[str, Any]) -> bool:
    """Detect whether a loaded schema describes the AssetRegistry format."""
    req = schema.get("required")
    if isinstance(req, list):
        req_set = set(x for x in req if isinstance(x, str))
        if "manifestVersion" in req_set:
            return False
        if {"version", "assets"}.issubset(req_set):
            return True

    props = schema.get("properties")
    if isinstance(props, dict):
        if "manifestVersion" in props:
            return False
        return "version" in props and "assets" in props

    return False


def _resolve_schema_for_file(
    manifest_path: Path,
    explicit_schema_path: Path | None,
    schema_cache: Dict[Path, Dict[str, Any]],
) -> tuple[Path, Dict[str, Any]]:
    """Resolve the schema to use for one manifest file.

    If --schema is provided, always use it.
    Otherwise auto-detect based on the file's JSON root shape.
    """
    if explicit_schema_path is not None:
        schema_path = explicit_schema_path
    else:
        manifest = _try_load_json_object(manifest_path)
        if manifest is not None and _is_asset_registry_manifest(manifest):
            schema_path = _ASSET_REGISTRY_SCHEMA_PATH
        else:
            schema_path = _SCHEMA_PATH

    if schema_path not in schema_cache:
        schema_cache[schema_path] = _load_schema(schema_path)
    return schema_path, schema_cache[schema_path]


# ---------------------------------------------------------------------------
# Validation — jsonschema path (full draft-07)
# ---------------------------------------------------------------------------

def _validate_with_jsonschema(
    manifest: Dict[str, Any], schema: Dict[str, Any]
) -> List[str]:
    """Return a list of error message strings (empty == valid)."""
    try:
        import jsonschema  # type: ignore
    except ImportError:
        return []  # signal: fall back to built-in checker

    validator_cls = jsonschema.Draft7Validator
    errors: List[str] = []
    for err in sorted(validator_cls(schema).iter_errors(manifest), key=str):
        path = " → ".join(str(p) for p in err.absolute_path) or "(root)"
        errors.append(f"{path}: {err.message}")
    return errors


# ---------------------------------------------------------------------------
# Validation — built-in fallback (no external deps)
# ---------------------------------------------------------------------------

_SEMVER_RE = re.compile(r"^\d+\.\d+\.\d+$")
_SHA256_RE = re.compile(r"^[0-9a-fA-F]{64}$")

_VALID_TYPES = {"audio", "texture", "tilemap", "model", "script", "material"}

_AUDIO_FORMATS = {"wav", "ogg", "mp3", "flac", "opus"}
_AUDIO_CATEGORIES = {"music", "sfx", "ambience", "voice", "ui"}

_TEXTURE_FORMATS = {
    "rgba8", "rgb8", "bc1", "bc3", "bc5", "bc7", "astc4x4", "png", "jpeg"
}
_TEXTURE_USAGES = {
    "albedo", "normal", "roughness", "metalness", "ao",
    "emissive", "ui", "heightmap"
}

_MODEL_FORMATS = {"gltf", "glb", "fbx", "obj", "dae"}


def _check_asset_builtin(asset: Any, idx: int) -> List[str]:
    """Structural check for a single asset entry (no jsonschema required)."""
    errors: List[str] = []
    prefix = f"assets[{idx}]"

    if not isinstance(asset, dict):
        return [f"{prefix}: expected an object, got {type(asset).__name__}"]

    # --- required scalar fields ---
    for field in ("id", "version", "type", "source", "hash"):
        if field not in asset:
            errors.append(f"{prefix}: missing required field '{field}'")
        elif not isinstance(asset[field], str) or not asset[field]:
            errors.append(f"{prefix}.{field}: must be a non-empty string")

    if errors:
        return errors  # skip deeper checks if basics are broken

    asset_id = asset.get("id", f"<index {idx}>")
    prefix = f"asset '{asset_id}'"

    # version format
    if not _SEMVER_RE.match(asset.get("version", "")):
        errors.append(f"{prefix}.version: must match SemVer (X.Y.Z)")

    # type enum
    asset_type = asset.get("type", "")
    if asset_type not in _VALID_TYPES:
        errors.append(
            f"{prefix}.type: '{asset_type}' is not one of {sorted(_VALID_TYPES)}"
        )

    # hash format
    if not _SHA256_RE.match(asset.get("hash", "")):
        errors.append(
            f"{prefix}.hash: must be a 64-character lowercase SHA-256 hex string"
        )

    # optional arrays
    for field in ("dependencies", "tags"):
        val = asset.get(field)
        if val is not None:
            if not isinstance(val, list):
                errors.append(f"{prefix}.{field}: must be an array")
            elif not all(isinstance(x, str) and x for x in val):
                errors.append(f"{prefix}.{field}: every item must be a non-empty string")

    # --- type-specific extension checks ---
    errors.extend(_check_extension_builtin(asset, prefix, asset_type))

    return errors


def _check_extension_builtin(
    asset: Dict[str, Any], prefix: str, asset_type: str
) -> List[str]:
    errors: List[str] = []
    ext = asset.get(asset_type)

    # TEACHING NOTE — type narrowing: if an extension block exists we validate
    # it; we only *warn* (not error) if the extension is absent, because some
    # pipeline steps add it lazily.
    if asset_type == "audio":
        if ext is None:
            return [f"{prefix}: missing 'audio' extension block (required for type='audio')"]
        for f in ("format", "sampleRate", "channels", "durationSeconds"):
            if f not in ext:
                errors.append(f"{prefix}.audio: missing required field '{f}'")
        if ext.get("format") not in _AUDIO_FORMATS:
            errors.append(
                f"{prefix}.audio.format: '{ext.get('format')}' not in {sorted(_AUDIO_FORMATS)}"
            )
        for num_field in ("sampleRate", "channels"):
            v = ext.get(num_field)
            if v is not None and (not isinstance(v, int) or v < 1):
                errors.append(f"{prefix}.audio.{num_field}: must be a positive integer")
        v = ext.get("durationSeconds")
        if v is not None and (not isinstance(v, (int, float)) or v < 0):
            errors.append(f"{prefix}.audio.durationSeconds: must be >= 0")
        cat = ext.get("category")
        if cat is not None and cat not in _AUDIO_CATEGORIES:
            errors.append(
                f"{prefix}.audio.category: '{cat}' not in {sorted(_AUDIO_CATEGORIES)}"
            )
        # loop point consistency
        loop_start = ext.get("loopStart")
        loop_end = ext.get("loopEnd")
        if loop_start is not None and loop_end is not None:
            if loop_end <= loop_start:
                errors.append(
                    f"{prefix}.audio: loopEnd ({loop_end}) must be > loopStart ({loop_start})"
                )

    elif asset_type == "texture":
        if ext is None:
            return [f"{prefix}: missing 'texture' extension block (required for type='texture')"]
        for f in ("format", "width", "height"):
            if f not in ext:
                errors.append(f"{prefix}.texture: missing required field '{f}'")
        if ext.get("format") not in _TEXTURE_FORMATS:
            errors.append(
                f"{prefix}.texture.format: '{ext.get('format')}' not in {sorted(_TEXTURE_FORMATS)}"
            )
        for dim in ("width", "height"):
            v = ext.get(dim)
            if v is not None and (not isinstance(v, int) or v < 1):
                errors.append(f"{prefix}.texture.{dim}: must be a positive integer")
        usage = ext.get("usage")
        if usage is not None and usage not in _TEXTURE_USAGES:
            errors.append(
                f"{prefix}.texture.usage: '{usage}' not in {sorted(_TEXTURE_USAGES)}"
            )

    elif asset_type == "tilemap":
        if ext is None:
            return [f"{prefix}: missing 'tilemap' extension block (required for type='tilemap')"]
        for f in ("width", "height", "tileWidth", "tileHeight"):
            if f not in ext:
                errors.append(f"{prefix}.tilemap: missing required field '{f}'")
        for dim in ("width", "height", "tileWidth", "tileHeight"):
            v = ext.get(dim)
            if v is not None and (not isinstance(v, int) or v < 1):
                errors.append(f"{prefix}.tilemap.{dim}: must be a positive integer")

    elif asset_type == "model":
        if ext is None:
            return [f"{prefix}: missing 'model' extension block (required for type='model')"]
        for f in ("format", "vertexCount", "triangleCount"):
            if f not in ext:
                errors.append(f"{prefix}.model: missing required field '{f}'")
        if ext.get("format") not in _MODEL_FORMATS:
            errors.append(
                f"{prefix}.model.format: '{ext.get('format')}' not in {sorted(_MODEL_FORMATS)}"
            )
        for cnt in ("vertexCount", "triangleCount"):
            v = ext.get(cnt)
            if v is not None and (not isinstance(v, int) or v < 0):
                errors.append(f"{prefix}.model.{cnt}: must be a non-negative integer")

    return errors


def _validate_manifest_builtin(
    manifest: Dict[str, Any], schema: Dict[str, Any]  # noqa: ARG001 — unused in fallback
) -> List[str]:
    errors: List[str] = []

    mv = manifest.get("manifestVersion")
    if not isinstance(mv, str) or not _SEMVER_RE.match(mv):
        errors.append("manifestVersion: missing or not a valid SemVer string (X.Y.Z)")

    assets = manifest.get("assets")
    if not isinstance(assets, list):
        errors.append("assets: missing or not an array")
        return errors
    if len(assets) == 0:
        errors.append("assets: must contain at least one entry")

    for idx, asset in enumerate(assets):
        errors.extend(_check_asset_builtin(asset, idx))

    return errors


_REGISTRY_ASSET_VALID_TYPES = {
    "texture", "mesh", "material", "audio", "audio_bank",
    "scene", "skeleton", "anim_clip", "anim_graph",
    "script", "font", "tilemap", "level",
}
_UUID_V4_PATTERN = r"^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$"
_UUID_V4_RE = re.compile(_UUID_V4_PATTERN)


def _validate_registry_builtin(
    manifest: Dict[str, Any], schema: Dict[str, Any]  # noqa: ARG001 — unused in fallback
) -> List[str]:
    """Built-in structural validation for shared/schemas/asset_registry.schema.json."""
    errors: List[str] = []

    version = manifest.get("version")
    if not isinstance(version, str) or not _SEMVER_RE.match(version):
        errors.append("version: missing or not a valid SemVer string (X.Y.Z)")

    generated_at = manifest.get("generatedAt")
    if generated_at is not None and not isinstance(generated_at, str):
        errors.append("generatedAt: must be a string when present")

    assets = manifest.get("assets")
    if not isinstance(assets, list):
        errors.append("assets: missing or not an array")
        return errors

    for idx, asset in enumerate(assets):
        prefix = f"assets[{idx}]"
        if not isinstance(asset, dict):
            errors.append(f"{prefix}: expected an object, got {type(asset).__name__}")
            continue

        for field in ("id", "type", "source"):
            if field not in asset:
                errors.append(f"{prefix}: missing required field '{field}'")
            elif not isinstance(asset[field], str) or not asset[field]:
                errors.append(f"{prefix}.{field}: must be a non-empty string")

        aid = asset.get("id")
        if isinstance(aid, str) and aid and not _UUID_V4_RE.match(aid):
            errors.append(f"{prefix}.id: must be a valid UUID v4 string")

        asset_type = asset.get("type")
        if isinstance(asset_type, str) and asset_type not in _REGISTRY_ASSET_VALID_TYPES:
            errors.append(
                f"{prefix}.type: '{asset_type}' is not one of "
                f"{sorted(_REGISTRY_ASSET_VALID_TYPES)}"
            )

        hash_value = asset.get("hash")
        if hash_value is not None:
            if not isinstance(hash_value, str):
                errors.append(f"{prefix}.hash: must be a string")
            elif hash_value and not _SHA256_RE.match(hash_value):
                errors.append(
                    f"{prefix}.hash: must be a 64-character lowercase SHA-256 hex string"
                )

        for opt_field in ("cooked", "name"):
            value = asset.get(opt_field)
            if value is not None and not isinstance(value, str):
                errors.append(f"{prefix}.{opt_field}: must be a string when present")

        for list_field in ("dependencies", "tags"):
            value = asset.get(list_field)
            if value is not None:
                if not isinstance(value, list):
                    errors.append(f"{prefix}.{list_field}: must be an array when present")
                elif not all(isinstance(x, str) for x in value):
                    errors.append(f"{prefix}.{list_field}: every item must be a string")

    return errors


# ---------------------------------------------------------------------------
# Duplicate ID detection
# ---------------------------------------------------------------------------

def _check_duplicate_ids(manifest: Dict[str, Any]) -> List[str]:
    """Warn if two assets share the same id within the same manifest."""
    seen: Dict[str, int] = {}
    warnings: List[str] = []
    for idx, asset in enumerate(manifest.get("assets", [])):
        if not isinstance(asset, dict):
            continue
        aid = asset.get("id")
        if aid is None:
            continue
        if aid in seen:
            warnings.append(
                f"Duplicate asset id '{aid}' found at index {seen[aid]} and {idx}"
            )
        else:
            seen[aid] = idx
    return warnings


# ---------------------------------------------------------------------------
# Per-file entry point
# ---------------------------------------------------------------------------

_HAS_JSONSCHEMA: bool | None = None


def _has_jsonschema() -> bool:
    global _HAS_JSONSCHEMA
    if _HAS_JSONSCHEMA is None:
        try:
            import jsonschema  # type: ignore  # noqa: F401
            _HAS_JSONSCHEMA = True
        except ImportError:
            _HAS_JSONSCHEMA = False
    return _HAS_JSONSCHEMA


def validate_file(
    path: Path, schema: Dict[str, Any]
) -> Tuple[bool, List[str], List[str]]:
    """
    Validate a single manifest file.

    Returns
    -------
    (passed, errors, warnings)
    """
    # --- parse JSON ---
    try:
        with path.open(encoding="utf-8") as fh:
            manifest = json.load(fh)
    except json.JSONDecodeError as exc:
        return False, [f"JSON parse error: {exc}"], []
    except OSError as exc:
        return False, [f"File read error: {exc}"], []

    if not isinstance(manifest, dict):
        return False, ["Manifest root must be a JSON object"], []

    # --- schema validation ---
    if _has_jsonschema():
        errors = _validate_with_jsonschema(manifest, schema)
        if not errors:
            # jsonschema returned nothing — means valid
            pass
    else:
        if _is_asset_registry_schema(schema):
            errors = _validate_registry_builtin(manifest, schema)
        else:
            errors = _validate_manifest_builtin(manifest, schema)

    # --- duplicate id check (always run) ---
    warnings = _check_duplicate_ids(manifest)

    return len(errors) == 0, errors, warnings


# ---------------------------------------------------------------------------
# Discovery helpers
# ---------------------------------------------------------------------------

def _collect_manifests(path: Path) -> List[Path]:
    """Return all manifest JSON files under *path* (recursive if directory).

    Files whose names end with '.schema.json' are skipped because they are
    schema definitions, not asset manifests.
    """
    if path.is_file():
        return [path]
    if path.is_dir():
        return sorted(
            f for f in path.rglob("*.json")
            if (
                not f.name.endswith(".schema.json")
                and (
                    "manifest" in (name_lower := f.name.lower())
                    or name_lower == "assetregistry.json"
                )
            )
        )
    return []


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------

def _report(
    path: Path,
    passed: bool,
    errors: List[str],
    warnings: List[str],
    verbose: bool,
) -> None:
    status = _green("PASS") if passed else _red("FAIL")
    print(f"  [{status}] {path}")
    for w in warnings:
        print(f"        {_yellow('WARN')} {w}")
    if not passed or verbose:
        for e in errors:
            print(f"        {_red('ERR ')} {e}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def _die(msg: str, code: int = 2) -> None:
    print(_red(f"ERROR: {msg}"), file=sys.stderr)
    sys.exit(code)


def main(argv: List[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="validate-assets",
        description=(
            "Validate Game Engine asset documents against canonical JSON schemas.\n"
            "By default the tool auto-detects either legacy asset manifests\n"
            "(assets/schema/asset-manifest.schema.json) or runtime registries\n"
            "(shared/schemas/asset_registry.schema.json).\n\n"
            "TEACHING NOTE: This is the asset pipeline's lint step.  Run it\n"
            "before packaging, importing, or shipping any assets."
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "paths",
        metavar="PATH",
        nargs="*",
        help=(
            "One or more manifest .json files or directories to scan recursively.\n"
            "Defaults to assets/examples/ relative to the repo root."
        ),
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Print all errors even for passing files.",
    )
    parser.add_argument(
        "--schema",
        metavar="FILE",
        help=(
            "Override schema file path. If omitted, schema is auto-detected per file:\n"
            "asset-manifest.schema.json for legacy manifests, or\n"
            "asset_registry.schema.json for AssetRegistry documents."
        ),
    )
    args = parser.parse_args(argv)
    explicit_schema_path = Path(args.schema) if args.schema else None

    # --- resolve paths ---
    if not args.paths:
        default_dir = _REPO_ROOT / "assets" / "examples"
        if not default_dir.exists():
            _die(
                f"No paths provided and default directory not found: {default_dir}"
            )
        scan_paths = [default_dir]
    else:
        scan_paths = [Path(p) for p in args.paths]

    manifests: List[Path] = []
    for p in scan_paths:
        if not p.exists():
            _die(f"Path does not exist: {p}")
        manifests.extend(_collect_manifests(p))

    if not manifests:
        print(_yellow("WARNING: No manifest files found."))
        return 0

    # --- header ---
    using = "jsonschema (full)" if _has_jsonschema() else "built-in (structural)"
    schema_label = str(explicit_schema_path) if explicit_schema_path else "auto (per-file)"
    print(_bold(f"\n=== validate-assets — validator: {using} ==="))
    print(f"Schema : {schema_label}")
    print(f"Files  : {len(manifests)}\n")

    total_passed = 0
    total_failed = 0
    total_warnings = 0
    schema_cache: Dict[Path, Dict[str, Any]] = {}

    for mf in manifests:
        _, schema = _resolve_schema_for_file(mf, explicit_schema_path, schema_cache)
        passed, errors, warnings = validate_file(mf, schema)
        _report(mf, passed, errors, warnings, args.verbose)
        if passed:
            total_passed += 1
        else:
            total_failed += 1
        total_warnings += len(warnings)

    # --- summary ---
    print()
    print(_bold("=== Summary ==="))
    print(f"  Passed   : {_green(str(total_passed))}")
    print(f"  Failed   : {(_red if total_failed else _green)(str(total_failed))}")
    print(f"  Warnings : {(_yellow if total_warnings else _green)(str(total_warnings))}")
    print()

    if total_failed:
        print(_red("Result: FAIL — fix the errors listed above and re-run."))
        return 1

    print(_green("Result: PASS — all manifests are valid."))
    return 0


if __name__ == "__main__":
    sys.exit(main())
