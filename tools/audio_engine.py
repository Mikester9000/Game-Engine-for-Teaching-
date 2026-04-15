#!/usr/bin/env python3
"""
audio_engine.py — Audio Engine Asset Manager with Manifest Support
==================================================================

TEACHING NOTE — Audio Pipeline Integration
-------------------------------------------
Real-world audio engines (Wwise, FMOD, in-house tools) maintain a central
registry of every audio clip.  When the build pipeline runs, that registry is
exported as a machine-readable manifest so the game engine, CI/CD system, and
QA tools all share exactly the same source of truth.

This module demonstrates two integration patterns for the shared asset manifest
format (see assets/schema/asset-manifest.schema.json):

  emit_manifest  — The Audio Engine serialises its internal registry in the
                   shared manifest format.  Any downstream tool (the asset
                   loader, the CI validator, another engine module) reads this
                   file rather than querying the audio tool directly.

  consume_manifest — The Audio Engine reads a manifest produced by another tool
                     (e.g. a DAW export script or a sound designer's authoring
                     tool) and populates its own registry from it, skipping
                     manual re-entry of audio metadata.

Both operations are *optional*: the engine runs fine without them.  You opt in
by passing --emit-manifest or --consume-manifest on the command line (or by
calling the corresponding methods programmatically).

Usage
-----
  # Register a clip and emit a manifest in one step:
  python3 tools/audio_engine.py register \\
      --id audio-main-theme --source audio/music/main_theme.ogg \\
      --format ogg --sample-rate 44100 --channels 2 --duration 180.5 \\
      --category music --tags music main-theme \\
      --emit-manifest build/audio-manifest.json

  # Load (consume) an existing manifest and list what was loaded:
  python3 tools/audio_engine.py consume \\
      --manifest assets/examples/audio-manifest.json --list

  # Emit the built-in demo manifest (no real files required):
  python3 tools/audio_engine.py emit \\
      --manifest /tmp/demo-audio-manifest.json

Exit codes
----------
  0 — success
  1 — one or more errors (bad arguments, missing manifest, …)
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
class AudioAsset:
    """
    TEACHING NOTE — Data Class as Value Object
    -------------------------------------------
    We use a dataclass here as a pure *value object*: it holds data and
    nothing else.  The engine methods that operate on this data live on
    AudioEngine, keeping concerns separated.
    """
    id: str
    source: str
    format: str          # wav / ogg / mp3 / flac / opus
    sample_rate: int     # Hz
    channels: int        # 1 = mono, 2 = stereo, …
    duration_seconds: float
    version: str = "1.0.0"
    category: Optional[str] = None   # music / sfx / ambience / voice / ui
    loop_start: Optional[float] = None
    loop_end: Optional[float] = None
    tags: dataclasses.field(default_factory=list) = dataclasses.field(
        default_factory=list
    )
    dependencies: dataclasses.field(default_factory=list) = dataclasses.field(
        default_factory=list
    )
    hash: Optional[str] = None  # computed from source file when available

    def to_manifest_entry(self) -> Dict[str, Any]:
        """Convert this asset to the canonical manifest entry dict."""
        audio_ext: Dict[str, Any] = {
            "format": self.format,
            "sampleRate": self.sample_rate,
            "channels": self.channels,
            "durationSeconds": self.duration_seconds,
        }
        if self.category is not None:
            audio_ext["category"] = self.category
        if self.loop_start is not None:
            audio_ext["loopStart"] = self.loop_start
        if self.loop_end is not None:
            audio_ext["loopEnd"] = self.loop_end

        entry: Dict[str, Any] = {
            "id": self.id,
            "version": self.version,
            "type": "audio",
            "source": self.source,
            "hash": self.hash or ("0" * 64),
            "dependencies": list(self.dependencies),
            "tags": list(self.tags),
            "audio": audio_ext,
        }
        return entry

    @staticmethod
    def from_manifest_entry(entry: Dict[str, Any]) -> "AudioAsset":
        """
        TEACHING NOTE — Factory Method
        --------------------------------
        Rather than a complex __init__ with many optional params, we use a
        static factory that knows how to parse the manifest dict shape.
        Separating construction from data holding keeps the dataclass lean.
        """
        audio = entry.get("audio", {})
        return AudioAsset(
            id=entry["id"],
            source=entry["source"],
            format=audio.get("format", ""),
            sample_rate=audio.get("sampleRate", 0),
            channels=audio.get("channels", 0),
            duration_seconds=audio.get("durationSeconds", 0.0),
            version=entry.get("version", "1.0.0"),
            category=audio.get("category"),
            loop_start=audio.get("loopStart"),
            loop_end=audio.get("loopEnd"),
            tags=list(entry.get("tags", [])),
            dependencies=list(entry.get("dependencies", [])),
            hash=entry.get("hash"),
        )


# ---------------------------------------------------------------------------
# Engine
# ---------------------------------------------------------------------------

class AudioEngine:
    """
    TEACHING NOTE — Service Class / Facade
    ----------------------------------------
    AudioEngine is a *service* that owns the audio asset registry.  It
    exposes a small, stable API:

      register(...)         — add an asset to the in-memory registry
      emit_manifest(path)   — serialise the registry to a manifest file
      consume_manifest(path)— load the registry from a manifest file
      get_asset(id)         — look up a single asset by its stable ID
      all_assets            — read-only view of the full registry

    The manifest methods are intentionally *optional*.  Callers that only
    want an in-memory registry never need to call them.
    """

    def __init__(self) -> None:
        self._assets: Dict[str, AudioAsset] = {}

    # ------------------------------------------------------------------
    # Registration
    # ------------------------------------------------------------------

    def register(
        self,
        asset_id: str,
        source: str,
        fmt: str,
        sample_rate: int,
        channels: int,
        duration_seconds: float,
        *,
        version: str = "1.0.0",
        category: Optional[str] = None,
        loop_start: Optional[float] = None,
        loop_end: Optional[float] = None,
        tags: Optional[List[str]] = None,
        dependencies: Optional[List[str]] = None,
        compute_hash: bool = True,
    ) -> AudioAsset:
        """
        Register an audio asset in the engine's registry.

        Parameters
        ----------
        asset_id        : Stable, globally-unique identifier (slug or GUID).
        source          : Path to the audio file (relative to the manifest
                          when the manifest is written, or an absolute path).
        fmt             : Audio format: wav / ogg / mp3 / flac / opus.
        sample_rate     : Sample rate in Hz (e.g. 44100).
        channels        : Channel count (1 = mono, 2 = stereo).
        duration_seconds: Total playback length in seconds.
        version         : Asset revision (SemVer, default '1.0.0').
        category        : Mixer category: music / sfx / ambience / voice / ui.
        loop_start      : Optional loop start point in seconds.
        loop_end        : Optional loop end point in seconds.
        tags            : Free-form labels for pipeline filtering.
        dependencies    : IDs of other assets this clip depends on.
        compute_hash    : If True and the file exists, compute SHA-256 now.

        Returns the registered AudioAsset.
        """
        file_hash: Optional[str] = None
        if compute_hash:
            file_hash = _sha256_if_exists(source)

        asset = AudioAsset(
            id=asset_id,
            source=source,
            format=fmt,
            sample_rate=sample_rate,
            channels=channels,
            duration_seconds=duration_seconds,
            version=version,
            category=category,
            loop_start=loop_start,
            loop_end=loop_end,
            tags=list(tags or []),
            dependencies=list(dependencies or []),
            hash=file_hash,
        )
        self._assets[asset_id] = asset
        return asset

    # ------------------------------------------------------------------
    # Manifest — emit
    # ------------------------------------------------------------------

    def emit_manifest(
        self, output_path: Path | str, *, indent: int = 2
    ) -> None:
        """
        TEACHING NOTE — Manifest Emission
        -----------------------------------
        Serialising the registry to the shared manifest format decouples the
        Audio Engine from all its consumers.  The manifest is a *snapshot*:
        it captures the exact state of every registered asset at the moment of
        export.  Downstream tools can validate, cache, or import this snapshot
        without knowing anything about how the Audio Engine stores its data
        internally.

        Parameters
        ----------
        output_path : File to write.  Parent directories are created if
                      they do not already exist.
        indent      : JSON indentation width (default 2).
        """
        output_path = Path(output_path)
        output_path.parent.mkdir(parents=True, exist_ok=True)

        manifest: Dict[str, Any] = {
            "manifestVersion": _MANIFEST_VERSION,
            "assets": [a.to_manifest_entry() for a in self._assets.values()],
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
        audio_only: bool = True,
        overwrite: bool = False,
    ) -> List[str]:
        """
        TEACHING NOTE — Manifest Consumption
        --------------------------------------
        Reading a manifest lets the Audio Engine import asset metadata that
        was produced by a *different* tool (e.g. a DAW batch-export script)
        without requiring that tool to know about the Audio Engine's internals.
        Both tools share only the manifest format — a stable contract.

        Parameters
        ----------
        input_path  : Manifest file to read.
        audio_only  : If True (default), silently skip non-audio entries.
                      Set to False to raise an error on unexpected types.
        overwrite   : If False (default), skip assets whose ID is already
                      registered; set to True to overwrite them.

        Returns
        -------
        List of warning strings (empty == no issues).
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
            if entry_type != "audio":
                if audio_only:
                    warnings.append(
                        f"Skipping non-audio asset '{entry.get('id', '?')}'"
                        f" (type='{entry_type}')"
                    )
                    continue
                else:
                    raise ValueError(
                        f"Unexpected asset type '{entry_type}' in audio manifest"
                    )

            try:
                asset = AudioAsset.from_manifest_entry(entry)
            except KeyError as exc:
                warnings.append(
                    f"Skipping asset '{entry.get('id', '?')}': missing field {exc}"
                )
                continue

            if asset.id in self._assets and not overwrite:
                warnings.append(
                    f"Asset '{asset.id}' already registered — skipping "
                    f"(pass overwrite=True to replace)"
                )
                continue

            self._assets[asset.id] = asset
            loaded += 1

        return warnings

    # ------------------------------------------------------------------
    # Queries
    # ------------------------------------------------------------------

    def get_asset(self, asset_id: str) -> Optional[AudioAsset]:
        """Return the registered asset for *asset_id*, or None."""
        return self._assets.get(asset_id)

    @property
    def all_assets(self) -> List[AudioAsset]:
        """Return a snapshot list of all registered assets."""
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

def _print_asset(asset: AudioAsset) -> None:
    """Pretty-print a single audio asset."""
    print(f"  {_bold(asset.id)}")
    print(f"    source   : {asset.source}")
    print(f"    format   : {asset.format}  {asset.sample_rate} Hz  "
          f"{asset.channels}ch  {asset.duration_seconds:.2f}s")
    if asset.category:
        print(f"    category : {asset.category}")
    if asset.loop_start is not None and asset.loop_end is not None:
        print(f"    loop     : {asset.loop_start}s → {asset.loop_end}s")
    if asset.tags:
        print(f"    tags     : {', '.join(asset.tags)}")
    hash_display = asset.hash[:16] + "…" if asset.hash else _yellow("(none)")
    print(f"    hash     : {hash_display}")
    print()


# ---------------------------------------------------------------------------
# Sub-command handlers
# ---------------------------------------------------------------------------

def _cmd_register(args: argparse.Namespace) -> int:
    engine = AudioEngine()

    tags = args.tags or []
    dependencies = args.dependencies or []

    engine.register(
        asset_id=args.id,
        source=args.source,
        fmt=args.format,
        sample_rate=args.sample_rate,
        channels=args.channels,
        duration_seconds=args.duration,
        version=args.version,
        category=args.category,
        loop_start=args.loop_start,
        loop_end=args.loop_end,
        tags=tags,
        dependencies=dependencies,
        compute_hash=True,
    )

    asset = engine.get_asset(args.id)
    print(_bold("\n=== audio_engine — registered asset ===\n"))
    _print_asset(asset)  # type: ignore[arg-type]

    if args.emit_manifest:
        _do_emit(engine, Path(args.emit_manifest))

    return 0


def _cmd_emit(args: argparse.Namespace) -> int:
    """
    Emit a demo manifest with one pre-baked example asset.

    TEACHING NOTE: In production you would populate the engine from your
    project database, a config file, or a DAW export.  Here we seed the
    engine with one example so you can see the manifest format without
    needing real audio files.
    """
    engine = AudioEngine()

    # Seed with an example (mirrors assets/examples/audio-manifest.json)
    engine.register(
        asset_id="audio-prelude-of-light",
        source="../audio/music/prelude_of_light.ogg",
        fmt="ogg",
        sample_rate=44100,
        channels=2,
        duration_seconds=180.5,
        category="music",
        loop_start=4.2,
        loop_end=176.3,
        tags=["music", "main-theme", "ambient", "ff15-style"],
        compute_hash=False,
    )
    # Supply the hash from the example manifest so the output is deterministic
    engine.all_assets[0].hash = (
        "a3f1c2b4d5e6f7a8b9c0d1e2f3a4b5c6d7e8f9a0b1c2d3e4f5a6b7c8d9e0f1a2"
    )

    _do_emit(engine, Path(args.manifest))
    return 0


def _cmd_consume(args: argparse.Namespace) -> int:
    engine = AudioEngine()

    manifest_path = Path(args.manifest)
    if not manifest_path.exists():
        _die(f"Manifest not found: {manifest_path}")

    try:
        warnings = engine.consume_manifest(
            manifest_path,
            audio_only=not args.strict,
            overwrite=args.overwrite,
        )
    except (ValueError, FileNotFoundError) as exc:
        _die(str(exc), code=1)

    print(_bold(f"\n=== audio_engine — consumed manifest: {manifest_path} ===\n"))

    for w in warnings:
        print(f"  {_yellow('WARN')} {w}")

    if warnings:
        print()

    print(f"  Loaded {_green(str(len(engine)))} audio asset(s).\n")

    if args.list:
        for asset in engine.all_assets:
            _print_asset(asset)

    if args.emit_manifest:
        _do_emit(engine, Path(args.emit_manifest))

    return 0


def _cmd_list(args: argparse.Namespace) -> int:  # noqa: ARG001
    """
    List all assets registered in the demo engine seed.

    TEACHING NOTE: In a real tool this command would read from a project
    database or config file.  Here we just show the built-in example to
    illustrate what a 'list' command looks like.
    """
    engine = AudioEngine()
    engine.register(
        asset_id="audio-prelude-of-light",
        source="../audio/music/prelude_of_light.ogg",
        fmt="ogg",
        sample_rate=44100,
        channels=2,
        duration_seconds=180.5,
        category="music",
        loop_start=4.2,
        loop_end=176.3,
        tags=["music", "main-theme"],
        compute_hash=False,
    )
    print(_bold("\n=== audio_engine — registered assets ===\n"))
    for asset in engine.all_assets:
        _print_asset(asset)
    return 0


# ---------------------------------------------------------------------------
# Shared helper
# ---------------------------------------------------------------------------

def _do_emit(engine: AudioEngine, output_path: Path) -> None:
    """Write manifest and print a confirmation line."""
    if len(engine) == 0:
        print(_yellow("WARNING: No assets registered — manifest will be empty."))
        return

    engine.emit_manifest(output_path)
    print(
        f"  {_green('Manifest written:')} {output_path}  "
        f"({len(engine)} asset(s))"
    )


# ---------------------------------------------------------------------------
# Argument parser
# ---------------------------------------------------------------------------

def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="audio_engine",
        description=(
            "Audio Engine asset manager with optional manifest emit/consume.\n\n"
            "TEACHING NOTE: This tool demonstrates how an audio subsystem\n"
            "integrates with the shared asset manifest format defined in\n"
            "assets/schema/asset-manifest.schema.json."
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    sub = parser.add_subparsers(dest="command", required=True)

    # --- register ---
    reg = sub.add_parser(
        "register",
        help="Register a single audio asset and optionally emit a manifest.",
    )
    reg.add_argument("--id", required=True, help="Stable asset ID (slug or GUID).")
    reg.add_argument("--source", required=True, help="Path to the audio file.")
    reg.add_argument(
        "--format", required=True, choices=["wav", "ogg", "mp3", "flac", "opus"],
        help="Audio format."
    )
    reg.add_argument("--sample-rate", type=int, required=True, help="Sample rate in Hz.")
    reg.add_argument("--channels", type=int, required=True, help="Channel count.")
    reg.add_argument("--duration", type=float, required=True,
                     help="Duration in seconds.")
    reg.add_argument("--version", default="1.0.0", help="Asset version (SemVer).")
    reg.add_argument(
        "--category", choices=["music", "sfx", "ambience", "voice", "ui"],
        help="Mixer category."
    )
    reg.add_argument("--loop-start", type=float, help="Loop start point (seconds).")
    reg.add_argument("--loop-end", type=float, help="Loop end point (seconds).")
    reg.add_argument("--tags", nargs="*", metavar="TAG",
                     help="Space-separated list of tags.")
    reg.add_argument("--dependencies", nargs="*", metavar="DEP_ID",
                     help="IDs of assets this clip depends on.")
    reg.add_argument(
        "--emit-manifest", metavar="OUTPUT",
        help="Path to write the manifest JSON after registering.",
    )

    # --- emit ---
    emit_p = sub.add_parser(
        "emit",
        help="Emit a manifest for the built-in demo audio assets.",
    )
    emit_p.add_argument(
        "--manifest", required=True, metavar="OUTPUT",
        help="Path to write the manifest JSON.",
    )

    # --- consume ---
    cons = sub.add_parser(
        "consume",
        help="Load audio assets from an existing manifest.",
    )
    cons.add_argument(
        "--manifest", required=True, metavar="INPUT",
        help="Manifest file to read.",
    )
    cons.add_argument(
        "--list", action="store_true",
        help="Print loaded assets after consuming.",
    )
    cons.add_argument(
        "--strict", action="store_true",
        help="Error on non-audio asset types instead of skipping them.",
    )
    cons.add_argument(
        "--overwrite", action="store_true",
        help="Replace already-registered assets when IDs collide.",
    )
    cons.add_argument(
        "--emit-manifest", metavar="OUTPUT",
        help="Re-emit the consumed registry to a new manifest file.",
    )

    # --- list ---
    sub.add_parser(
        "list",
        help="List the built-in demo audio assets.",
    )

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
