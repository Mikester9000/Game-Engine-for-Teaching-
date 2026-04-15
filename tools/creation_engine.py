#!/usr/bin/env python3
"""
creation_engine.py — Creation Engine Asset Manager with Manifest Support
========================================================================

TEACHING NOTE — Creation Pipeline Integration
-----------------------------------------------
A "Creation Engine" (sometimes called an authoring tool, editor, or content
pipeline) is the tool chain that *produces* game assets: tilemaps, textures,
3-D models, scripts, audio, etc.  When the pipeline bakes assets it should
also write a manifest so every other part of the system — the game engine, the
CI validator, the audio engine, the QA team — all share the same source of
truth about what assets exist and where they live.

This module demonstrates both integration directions:

  emit_manifest  — After creating or registering assets, serialise the
                   creation engine's registry to the shared manifest format.
                   Downstream tools read this file instead of querying the
                   creation tool directly.

  consume_manifest — Import asset metadata from a manifest written by *another*
                     tool (e.g. the audio_engine, a DAW export script, or a
                     3-D modelling tool's export pipeline).  The creation engine
                     now knows about those assets without duplicating their
                     definitions.

Both operations are *optional*: pass --emit-manifest or --consume-manifest on
the command line to opt in, or call the corresponding Python methods directly.

Usage
-----
  # Register assets from the command line and emit a manifest:
  python3 tools/creation_engine.py register \\
      --id tex-noctis-albedo --type texture --source textures/noctis_alb.png \\
      --texture-format png --width 2048 --height 2048 --usage albedo \\
      --tags character noctis \\
      --emit-manifest build/texture-manifest.json

  # Consume an existing audio manifest and re-emit as part of a combined
  # creation manifest:
  python3 tools/creation_engine.py consume \\
      --manifest assets/examples/audio-manifest.json \\
      --emit-manifest build/combined-manifest.json

  # Emit the built-in demo manifest (no real files required):
  python3 tools/creation_engine.py emit \\
      --manifest /tmp/demo-creation-manifest.json

  # List all assets registered in the demo seed:
  python3 tools/creation_engine.py list

Exit codes
----------
  0 — success
  1 — one or more errors
  2 — usage / filesystem error
"""

from __future__ import annotations

import argparse
import dataclasses
import hashlib
import json
import os
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

_TOOLS_DIR = Path(__file__).resolve().parent
_REPO_ROOT = _TOOLS_DIR.parent
_SCHEMA_PATH = _REPO_ROOT / "assets" / "schema" / "asset-manifest.schema.json"

_MANIFEST_VERSION = "1.0.0"

_VALID_TYPES = {"audio", "texture", "tilemap", "model", "script", "material"}

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
# Data model
# ---------------------------------------------------------------------------

@dataclasses.dataclass
class AssetRecord:
    """
    TEACHING NOTE — Generic Asset Record
    --------------------------------------
    Each asset in the creation engine is stored as an AssetRecord with the
    common fields that every asset type shares, plus a *type_extension* dict
    that holds whatever extra metadata the asset type requires (e.g. sample
    rate for audio, pixel dimensions for textures).

    This mirrors the manifest schema's two-level layout:
      common fields  →  id / version / type / source / hash / dependencies / tags
      extension block → "audio": {…} / "texture": {…} / "tilemap": {…} / …
    """
    id: str
    asset_type: str        # one of _VALID_TYPES
    source: str
    version: str = "1.0.0"
    tags: dataclasses.field(default_factory=list) = dataclasses.field(
        default_factory=list
    )
    dependencies: dataclasses.field(default_factory=list) = dataclasses.field(
        default_factory=list
    )
    hash: Optional[str] = None
    type_extension: Dict[str, Any] = dataclasses.field(default_factory=dict)

    def to_manifest_entry(self) -> Dict[str, Any]:
        """Serialise this record to the canonical manifest entry format."""
        entry: Dict[str, Any] = {
            "id": self.id,
            "version": self.version,
            "type": self.asset_type,
            "source": self.source,
            "hash": self.hash or ("0" * 64),
            "dependencies": list(self.dependencies),
            "tags": list(self.tags),
        }
        if self.type_extension:
            entry[self.asset_type] = dict(self.type_extension)
        return entry

    @staticmethod
    def from_manifest_entry(entry: Dict[str, Any]) -> "AssetRecord":
        """Parse a manifest entry dict into an AssetRecord."""
        asset_type = entry.get("type", "")
        extension = dict(entry.get(asset_type, {})) if asset_type else {}
        return AssetRecord(
            id=entry["id"],
            asset_type=asset_type,
            source=entry["source"],
            version=entry.get("version", "1.0.0"),
            tags=list(entry.get("tags", [])),
            dependencies=list(entry.get("dependencies", [])),
            hash=entry.get("hash"),
            type_extension=extension,
        )


# ---------------------------------------------------------------------------
# Engine
# ---------------------------------------------------------------------------

class CreationEngine:
    """
    TEACHING NOTE — Creation Engine as a Service Facade
    -----------------------------------------------------
    The CreationEngine orchestrates all asset types.  In a real tool this
    class would wrap the 3-D viewport, the tilemap editor, the audio importer,
    etc.  Here it is intentionally minimal — its value is in demonstrating
    the manifest integration pattern, not in being a fully-featured editor.

    API surface:
      register(...)         — add any asset type to the registry
      emit_manifest(path)   — write the registry to a manifest file (optional)
      consume_manifest(path)— load a manifest into the registry (optional)
      get_asset(id)         — look up a registered asset by stable ID
      all_assets            — read-only snapshot of the registry
    """

    def __init__(self) -> None:
        self._assets: Dict[str, AssetRecord] = {}

    # ------------------------------------------------------------------
    # Registration
    # ------------------------------------------------------------------

    def register(
        self,
        asset_id: str,
        asset_type: str,
        source: str,
        type_extension: Optional[Dict[str, Any]] = None,
        *,
        version: str = "1.0.0",
        tags: Optional[List[str]] = None,
        dependencies: Optional[List[str]] = None,
        compute_hash: bool = True,
    ) -> AssetRecord:
        """
        Register any asset type in the engine's registry.

        Parameters
        ----------
        asset_id       : Stable, globally-unique identifier.
        asset_type     : One of audio / texture / tilemap / model / script /
                         material.
        source         : Path to the source asset file.
        type_extension : Dict of type-specific metadata (e.g. for audio:
                         {"format": "ogg", "sampleRate": 44100, …}).
        version        : Asset revision (SemVer, default '1.0.0').
        tags           : Free-form pipeline labels.
        dependencies   : IDs of assets this asset depends on.
        compute_hash   : If True and the file exists, compute SHA-256 now.
        """
        if asset_type not in _VALID_TYPES:
            raise ValueError(
                f"Unknown asset type '{asset_type}'. "
                f"Valid types: {sorted(_VALID_TYPES)}"
            )

        file_hash: Optional[str] = None
        if compute_hash:
            file_hash = _sha256_if_exists(source)

        record = AssetRecord(
            id=asset_id,
            asset_type=asset_type,
            source=source,
            version=version,
            tags=list(tags or []),
            dependencies=list(dependencies or []),
            hash=file_hash,
            type_extension=dict(type_extension or {}),
        )
        self._assets[asset_id] = record
        return record

    # ------------------------------------------------------------------
    # Manifest — emit
    # ------------------------------------------------------------------

    def emit_manifest(
        self, output_path: Path | str, *, indent: int = 2
    ) -> None:
        """
        TEACHING NOTE — Manifest Emission from the Creation Engine
        -----------------------------------------------------------
        When the creation engine finishes a bake / export step it calls
        emit_manifest to publish a snapshot of every asset it produced.
        Downstream systems (the game engine's asset loader, the CI validator,
        the audio engine) import this manifest instead of duplicating asset
        metadata.

        The manifest is written atomically: we build the full dict in memory
        first, then write it to disk in one call.  This avoids leaving a
        half-written file if the process is interrupted.
        """
        output_path = Path(output_path)
        output_path.parent.mkdir(parents=True, exist_ok=True)

        manifest: Dict[str, Any] = {
            "manifestVersion": _MANIFEST_VERSION,
            "assets": [r.to_manifest_entry() for r in self._assets.values()],
        }

        with output_path.open("w", encoding="utf-8") as fh:
            json.dump(manifest, fh, indent=indent)
            fh.write("\n")

    # ------------------------------------------------------------------
    # Manifest — consume
    # ------------------------------------------------------------------

    def consume_manifest(
        self,
        input_path: Path | str,
        *,
        type_filter: Optional[List[str]] = None,
        overwrite: bool = False,
    ) -> List[str]:
        """
        TEACHING NOTE — Manifest Consumption in the Creation Engine
        ------------------------------------------------------------
        A creation tool may import assets from many upstream sources: an audio
        team's DAW export, a texture artist's Substance Painter export, a
        modeller's glTF batch output.  Rather than hard-coding connectors for
        each tool, the creation engine consumes their shared manifest files.
        Only the schema matters — not the producer.

        Parameters
        ----------
        input_path  : Manifest file to load.
        type_filter : If given, only import asset types in this list
                      (e.g. ["audio", "texture"]).  None = import all types.
        overwrite   : If False (default), skip assets whose ID is already
                      registered.

        Returns
        -------
        List of warning strings (empty == clean import).
        """
        input_path = Path(input_path)
        warnings: List[str] = []

        try:
            with input_path.open(encoding="utf-8") as fh:
                manifest = json.load(fh)
        except json.JSONDecodeError as exc:
            raise ValueError(f"JSON parse error in {input_path}: {exc}") from exc
        except OSError as exc:
            raise FileNotFoundError(
                f"Cannot read manifest {input_path}: {exc}"
            ) from exc

        if not isinstance(manifest, dict):
            raise ValueError(f"{input_path}: manifest root must be a JSON object")

        entries = manifest.get("assets", [])
        if not isinstance(entries, list):
            raise ValueError(f"{input_path}: 'assets' must be an array")

        loaded = 0
        for entry in entries:
            if not isinstance(entry, dict):
                warnings.append("Skipping non-object entry in manifest")
                continue

            entry_type = entry.get("type", "")

            if type_filter is not None and entry_type not in type_filter:
                warnings.append(
                    f"Skipping asset '{entry.get('id', '?')}'"
                    f" (type='{entry_type}' not in filter)"
                )
                continue

            if entry_type not in _VALID_TYPES:
                warnings.append(
                    f"Skipping asset '{entry.get('id', '?')}'"
                    f": unknown type '{entry_type}'"
                )
                continue

            try:
                record = AssetRecord.from_manifest_entry(entry)
            except KeyError as exc:
                warnings.append(
                    f"Skipping asset '{entry.get('id', '?')}': missing field {exc}"
                )
                continue

            if record.id in self._assets and not overwrite:
                warnings.append(
                    f"Asset '{record.id}' already registered — skipping "
                    f"(pass overwrite=True to replace)"
                )
                continue

            self._assets[record.id] = record
            loaded += 1

        return warnings

    # ------------------------------------------------------------------
    # Queries
    # ------------------------------------------------------------------

    def get_asset(self, asset_id: str) -> Optional[AssetRecord]:
        """Return the registered record for *asset_id*, or None."""
        return self._assets.get(asset_id)

    @property
    def all_assets(self) -> List[AssetRecord]:
        """Return a snapshot list of all registered records."""
        return list(self._assets.values())

    def __len__(self) -> int:
        return len(self._assets)


# ---------------------------------------------------------------------------
# Utilities
# ---------------------------------------------------------------------------

def _sha256_if_exists(path: str | Path) -> Optional[str]:
    """Compute the SHA-256 hex digest of *path* if the file exists."""
    p = Path(path)
    if p.is_file():
        return hashlib.sha256(p.read_bytes()).hexdigest()
    return None


def _die(msg: str, code: int = 2) -> None:
    print(_red(f"ERROR: {msg}"), file=sys.stderr)
    sys.exit(code)


# ---------------------------------------------------------------------------
# CLI helpers
# ---------------------------------------------------------------------------

def _print_record(record: AssetRecord) -> None:
    """Pretty-print a single asset record."""
    type_colour = {
        "audio": "36", "texture": "35", "tilemap": "33",
        "model": "34", "script": "32", "material": "31",
    }
    colour = type_colour.get(record.asset_type, "0")
    print(f"  {_bold(record.id)}  {_c('[' + record.asset_type + ']', colour)}")
    print(f"    source   : {record.source}")
    if record.type_extension:
        ext_summary = ", ".join(
            f"{k}={v}" for k, v in list(record.type_extension.items())[:4]
        )
        print(f"    ext      : {ext_summary}")
    if record.tags:
        print(f"    tags     : {', '.join(record.tags)}")
    hash_display = record.hash[:16] + "…" if record.hash else _yellow("(none)")
    print(f"    hash     : {hash_display}")
    print()


# ---------------------------------------------------------------------------
# Demo seed data
# ---------------------------------------------------------------------------

def _seed_demo_engine() -> CreationEngine:
    """
    Return a CreationEngine pre-loaded with one asset of each type.

    TEACHING NOTE: In a real tool the engine would be populated from a project
    database, a config file, or the UI.  Here we seed it with synthetic data
    so every CLI demo subcommand works without needing real asset files.
    """
    engine = CreationEngine()

    engine.register(
        "audio-main-theme",
        "audio",
        "../audio/music/main_theme.ogg",
        {
            "format": "ogg", "sampleRate": 44100, "channels": 2,
            "durationSeconds": 180.5, "loopStart": 4.2, "loopEnd": 176.3,
            "category": "music",
        },
        tags=["music", "main-theme"],
        compute_hash=False,
    )
    engine.all_assets[-1].hash = (
        "a3f1c2b4d5e6f7a8b9c0d1e2f3a4b5c6d7e8f9a0b1c2d3e4f5a6b7c8d9e0f1a2"
    )

    engine.register(
        "tex-noctis-albedo",
        "texture",
        "../textures/characters/noctis_albedo.png",
        {"format": "png", "width": 2048, "height": 2048, "sRGB": True, "usage": "albedo"},
        tags=["character", "noctis", "albedo"],
        compute_hash=False,
    )
    engine.all_assets[-1].hash = (
        "b5c6d7e8f9a0b1c2d3e4f5a6b7c8d9e0f1a2b3c4d5e6f7a8b9c0d1e2f3a4b5c6"
    )

    engine.register(
        "tilemap-lucis-capital",
        "tilemap",
        "../maps/lucis_capital.tmx",
        {"width": 128, "height": 128, "tileWidth": 32, "tileHeight": 32, "layers": 3},
        tags=["city", "lucis"],
        compute_hash=False,
    )
    engine.all_assets[-1].hash = (
        "c6d7e8f9a0b1c2d3e4f5a6b7c8d9e0f1a2b3c4d5e6f7a8b9c0d1e2f3a4b5c6d7"
    )

    engine.register(
        "model-noctis",
        "model",
        "../models/characters/noctis.glb",
        {
            "format": "glb", "vertexCount": 12480, "triangleCount": 22560,
            "hasSkeleton": True, "animationCount": 32,
        },
        tags=["character", "noctis", "rigged"],
        compute_hash=False,
    )
    engine.all_assets[-1].hash = (
        "d7e8f9a0b1c2d3e4f5a6b7c8d9e0f1a2b3c4d5e6f7a8b9c0d1e2f3a4b5c6d7e8"
    )

    return engine


# ---------------------------------------------------------------------------
# Sub-command handlers
# ---------------------------------------------------------------------------

def _cmd_register(args: argparse.Namespace) -> int:
    engine = CreationEngine()

    # Build the type-extension dict from the flat CLI flags
    ext: Dict[str, Any] = {}
    if args.asset_type == "audio":
        _require_audio_flags(args)
        ext = {
            "format": args.audio_format,
            "sampleRate": args.sample_rate,
            "channels": args.channels,
            "durationSeconds": args.duration,
        }
        if args.category:
            ext["category"] = args.category
        if args.loop_start is not None:
            ext["loopStart"] = args.loop_start
        if args.loop_end is not None:
            ext["loopEnd"] = args.loop_end

    elif args.asset_type == "texture":
        _require_texture_flags(args)
        ext = {
            "format": args.texture_format,
            "width": args.width,
            "height": args.height,
        }
        if args.usage:
            ext["usage"] = args.usage
        if args.mip_levels is not None:
            ext["mipLevels"] = args.mip_levels
        if args.srgb is not None:
            ext["sRGB"] = args.srgb

    elif args.asset_type == "tilemap":
        _require_tilemap_flags(args)
        ext = {
            "width": args.map_width,
            "height": args.map_height,
            "tileWidth": args.tile_width,
            "tileHeight": args.tile_height,
        }

    elif args.asset_type == "model":
        _require_model_flags(args)
        ext = {
            "format": args.model_format,
            "vertexCount": args.vertex_count,
            "triangleCount": args.triangle_count,
        }
        if args.has_skeleton is not None:
            ext["hasSkeleton"] = args.has_skeleton
        if args.animation_count is not None:
            ext["animationCount"] = args.animation_count

    engine.register(
        asset_id=args.id,
        asset_type=args.asset_type,
        source=args.source,
        type_extension=ext,
        version=args.version,
        tags=args.tags or [],
        dependencies=args.dependencies or [],
        compute_hash=True,
    )

    record = engine.get_asset(args.id)
    print(_bold("\n=== creation_engine — registered asset ===\n"))
    _print_record(record)  # type: ignore[arg-type]

    if args.emit_manifest:
        _do_emit(engine, Path(args.emit_manifest))

    return 0


def _cmd_emit(args: argparse.Namespace) -> int:
    """Emit a manifest for the built-in demo assets."""
    engine = _seed_demo_engine()
    _do_emit(engine, Path(args.manifest))
    return 0


def _cmd_consume(args: argparse.Namespace) -> int:
    engine = CreationEngine()

    manifest_path = Path(args.manifest)
    if not manifest_path.exists():
        _die(f"Manifest not found: {manifest_path}")

    type_filter = args.types or None

    try:
        warnings = engine.consume_manifest(
            manifest_path,
            type_filter=type_filter,
            overwrite=args.overwrite,
        )
    except (ValueError, FileNotFoundError) as exc:
        _die(str(exc), code=1)

    print(_bold(f"\n=== creation_engine — consumed manifest: {manifest_path} ===\n"))

    for w in warnings:
        print(f"  {_yellow('WARN')} {w}")

    if warnings:
        print()

    print(f"  Loaded {_green(str(len(engine)))} asset(s).\n")

    if args.list:
        for record in engine.all_assets:
            _print_record(record)

    if args.emit_manifest:
        _do_emit(engine, Path(args.emit_manifest))

    return 0


def _cmd_list(_args: argparse.Namespace) -> int:
    engine = _seed_demo_engine()
    print(_bold("\n=== creation_engine — registered assets ===\n"))
    for record in engine.all_assets:
        _print_record(record)
    print(f"  Total: {len(engine)} asset(s)\n")
    return 0


# ---------------------------------------------------------------------------
# Shared helpers
# ---------------------------------------------------------------------------

def _do_emit(engine: CreationEngine, output_path: Path) -> None:
    if len(engine) == 0:
        print(_yellow("WARNING: No assets registered — manifest will be empty."))
        return
    engine.emit_manifest(output_path)
    print(
        f"  {_green('Manifest written:')} {output_path}  "
        f"({len(engine)} asset(s))"
    )


def _require_audio_flags(args: argparse.Namespace) -> None:
    for flag in ("audio_format", "sample_rate", "channels", "duration"):
        if getattr(args, flag, None) is None:
            _die(f"--{flag.replace('_', '-')} is required for type=audio", code=1)


def _require_texture_flags(args: argparse.Namespace) -> None:
    for flag in ("texture_format", "width", "height"):
        if getattr(args, flag, None) is None:
            _die(f"--{flag.replace('_', '-')} is required for type=texture", code=1)


def _require_tilemap_flags(args: argparse.Namespace) -> None:
    for flag in ("map_width", "map_height", "tile_width", "tile_height"):
        if getattr(args, flag, None) is None:
            _die(f"--{flag.replace('_', '-')} is required for type=tilemap", code=1)


def _require_model_flags(args: argparse.Namespace) -> None:
    for flag in ("model_format", "vertex_count", "triangle_count"):
        if getattr(args, flag, None) is None:
            _die(f"--{flag.replace('_', '-')} is required for type=model", code=1)


# ---------------------------------------------------------------------------
# Argument parser
# ---------------------------------------------------------------------------

def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="creation_engine",
        description=(
            "Creation Engine asset manager with optional manifest emit/consume.\n\n"
            "TEACHING NOTE: This tool demonstrates how a creation/authoring\n"
            "tool integrates with the shared asset manifest format defined in\n"
            "assets/schema/asset-manifest.schema.json."
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    sub = parser.add_subparsers(dest="command", required=True)

    # ------------------------------------------------------------------ register
    reg = sub.add_parser(
        "register",
        help="Register a single asset and optionally emit a manifest.",
    )
    reg.add_argument("--id", required=True, help="Stable asset ID.")
    reg.add_argument("--type", dest="asset_type", required=True,
                     choices=sorted(_VALID_TYPES), help="Asset type.")
    reg.add_argument("--source", required=True, help="Path to the asset file.")
    reg.add_argument("--version", default="1.0.0", help="Asset version (SemVer).")
    reg.add_argument("--tags", nargs="*", metavar="TAG")
    reg.add_argument("--dependencies", nargs="*", metavar="DEP_ID")
    reg.add_argument("--emit-manifest", metavar="OUTPUT",
                     help="Write a manifest after registering.")

    # audio flags
    ag = reg.add_argument_group("audio")
    ag.add_argument("--audio-format", dest="audio_format",
                    choices=["wav", "ogg", "mp3", "flac", "opus"])
    ag.add_argument("--sample-rate", type=int)
    ag.add_argument("--channels", type=int)
    ag.add_argument("--duration", type=float)
    ag.add_argument("--category",
                    choices=["music", "sfx", "ambience", "voice", "ui"])
    ag.add_argument("--loop-start", type=float)
    ag.add_argument("--loop-end", type=float)

    # texture flags
    tg = reg.add_argument_group("texture")
    tg.add_argument("--texture-format", dest="texture_format",
                    choices=["rgba8", "rgb8", "bc1", "bc3", "bc5", "bc7",
                             "astc4x4", "png", "jpeg"])
    tg.add_argument("--width", type=int)
    tg.add_argument("--height", type=int)
    tg.add_argument("--usage",
                    choices=["albedo", "normal", "roughness", "metalness",
                             "ao", "emissive", "ui", "heightmap"])
    tg.add_argument("--mip-levels", type=int)
    tg.add_argument("--srgb", type=lambda x: x.lower() == "true",
                    help="true or false")

    # tilemap flags
    tmg = reg.add_argument_group("tilemap")
    tmg.add_argument("--map-width", type=int)
    tmg.add_argument("--map-height", type=int)
    tmg.add_argument("--tile-width", type=int)
    tmg.add_argument("--tile-height", type=int)

    # model flags
    mg = reg.add_argument_group("model")
    mg.add_argument("--model-format", dest="model_format",
                    choices=["gltf", "glb", "fbx", "obj", "dae"])
    mg.add_argument("--vertex-count", type=int)
    mg.add_argument("--triangle-count", type=int)
    mg.add_argument("--has-skeleton", type=lambda x: x.lower() == "true",
                    help="true or false")
    mg.add_argument("--animation-count", type=int)

    # ------------------------------------------------------------------ emit
    emit_p = sub.add_parser(
        "emit",
        help="Emit a manifest for the built-in demo assets.",
    )
    emit_p.add_argument("--manifest", required=True, metavar="OUTPUT",
                        help="Path to write the manifest JSON.")

    # ------------------------------------------------------------------ consume
    cons = sub.add_parser(
        "consume",
        help="Load assets from an existing manifest.",
    )
    cons.add_argument("--manifest", required=True, metavar="INPUT",
                      help="Manifest file to read.")
    cons.add_argument("--types", nargs="*", metavar="TYPE",
                      choices=sorted(_VALID_TYPES),
                      help="Only import these asset types.")
    cons.add_argument("--list", action="store_true",
                      help="Print loaded assets after consuming.")
    cons.add_argument("--overwrite", action="store_true",
                      help="Replace already-registered assets on ID collision.")
    cons.add_argument("--emit-manifest", metavar="OUTPUT",
                      help="Re-emit the consumed registry to a new file.")

    # ------------------------------------------------------------------ list
    sub.add_parser("list", help="List the built-in demo assets.")

    return parser


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main(argv: List[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)

    dispatch = {
        "register": _cmd_register,
        "emit":     _cmd_emit,
        "consume":  _cmd_consume,
        "list":     _cmd_list,
    }
    handler = dispatch.get(args.command)
    if handler is None:
        _die(f"Unknown command: {args.command}")
    return handler(args)  # type: ignore[misc]


if __name__ == "__main__":
    sys.exit(main())
