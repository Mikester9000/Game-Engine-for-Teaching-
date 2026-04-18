#!/usr/bin/env python3
"""
cook_assets.py — Vertical Slice Asset Cook Script
==================================================

TEACHING NOTE — What is "Cooking"?
====================================
In a game engine pipeline, "cooking" means transforming raw source assets
(PNG textures, WAV audio, FBX meshes, JSON animation data) into
**runtime-ready** binary or optimised formats that the engine loads quickly.

The cook step:
  1. Reads raw assets from Content/
  2. Processes them (compress, convert, package)
  3. Writes cooked assets to Cooked/
  4. Updates AssetRegistry.json with hashes and paths

This script is a STUB that demonstrates the pipeline structure.
As you implement the real cook steps (Milestone 2), replace the stubs with
real processing calls from:
  - tools/audio_authoring/audio_engine  (for audio)
  - tools/anim_authoring/animation_engine  (for skeletons + clips)

Usage:
    cd samples/vertical_slice_project
    python cook_assets.py

    # Or from repo root:
    python samples/vertical_slice_project/cook_assets.py

TEACHING NOTE — Why a single cook script?
Every engine (Unreal, Unity, Godot) has a cook/export step.  Keeping it as
a simple Python script makes it:
  • Easy to understand and modify
  • Runnable from CI pipelines (GitHub Actions, Jenkins)
  • Debuggable with standard Python tools
"""

from __future__ import annotations

import json
import hashlib
import shutil
import sys
import uuid
from datetime import datetime, timezone
from pathlib import Path

# ---------------------------------------------------------------------------
# TEACHING NOTE — Optional Tool Integration
# ---------------------------------------------------------------------------
# We try to import the Python authoring tools that live in tools/.  If they
# are installed (e.g. via  pip install -e tools/audio_authoring)  the cook
# step will use them for real audio/animation processing.  If they are NOT
# installed (e.g. on a machine that only has the raw C++ build tools) we fall
# back to simple file-copy stubs so the pipeline still works end-to-end.
# ---------------------------------------------------------------------------
try:
    from animation_engine.integration import AnimAssetPipeline  # type: ignore
    _HAS_ANIM_ENGINE = True
except ImportError:
    _HAS_ANIM_ENGINE = False

try:
    from audio_engine.integration import AssetPipeline as _AudioAssetPipeline  # noqa: F401
    _HAS_AUDIO_ENGINE = True
except ImportError:
    _HAS_AUDIO_ENGINE = False

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

SCRIPT_DIR   = Path(__file__).parent
CONTENT_DIR  = SCRIPT_DIR / "Content"
COOKED_DIR   = SCRIPT_DIR / "Cooked"
PROJECT_FILE = SCRIPT_DIR / "Project.json"
REGISTRY_FILE = SCRIPT_DIR / "AssetRegistry.json"

# ---------------------------------------------------------------------------
# Utilities
# ---------------------------------------------------------------------------

def sha256_file(path: Path) -> str:
    """Compute SHA-256 hex digest of a file.

    TEACHING NOTE — Hashing
    We store a hash of each source asset in the registry.  On the next cook
    run we compare the current hash to the stored one.  If they match, the
    asset is unchanged and we skip it (incremental rebuild).
    """
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def new_guid() -> str:
    """Generate a stable UUID v4 string."""
    return str(uuid.uuid4())


def ensure_dir(path: Path) -> None:
    """Create a directory and all parents if they don't already exist."""
    path.mkdir(parents=True, exist_ok=True)


# ---------------------------------------------------------------------------
# Cook steps (stubs)
# ---------------------------------------------------------------------------

def cook_textures(registry: list[dict]) -> int:
    """STUB: Copy PNG/JPG textures from Content/ to Cooked/Textures/.

    TEACHING NOTE — Texture Cooking (Stub)
    A real cook step would:
      1. Decode the PNG with Pillow or stb_image.
      2. Generate mip levels (halve dimensions until 1×1).
      3. Compress to BC1/BC3/BC7 using ispc_texcomp or DirectXTex.
      4. Write a custom .tex binary header + compressed mip data.
    For now we just copy the file to Cooked/ to show the pipeline structure.
    """
    texture_src = CONTENT_DIR / "Textures"
    texture_dst = COOKED_DIR  / "Textures"
    ensure_dir(texture_dst)

    count = 0
    for src in sorted(list(texture_src.glob("**/*.png")) + list(texture_src.glob("**/*.jpg"))):
        rel    = src.relative_to(texture_src)   # relative to Textures/
        dst    = texture_dst / rel.with_suffix(".tex")  # rename extension
        dst.parent.mkdir(parents=True, exist_ok=True)

        # STUB: just copy; real cook would compress
        shutil.copy2(src, dst)

        registry.append({
            "id":     new_guid(),
            "type":   "texture",
            "name":   src.stem,
            "source": "Textures/" + str(rel),
            "cooked": str(dst.relative_to(SCRIPT_DIR)),
            "hash":   sha256_file(src),
            "dependencies": [],
            "tags":   ["texture"],
        })
        count += 1
        print(f"  [TEX] {rel} → {dst.relative_to(SCRIPT_DIR)}")

    return count


def cook_audio(registry: list[dict]) -> int:
    """Cook WAV audio from Content/Audio/ to Cooked/Audio/.

    TEACHING NOTE — Audio Cooking
    When the  tools/audio_authoring  package is installed, this function uses
    the  audio_engine.dsp  module to normalise each WAV file (target LUFS,
    true-peak ceiling) before writing to Cooked/Audio/.  It also writes a
    .bank JSON manifest that the C++ runtime reads at startup.

    When the package is NOT installed (stub mode) we simply copy WAV files
    without any DSP processing.  The pipeline still works end-to-end.

    TEACHING NOTE — Why normalise at cook time?
    Normalising audio during the cook step (not at runtime) means:
      1. The runtime doesn't waste CPU cycles on DSP during gameplay.
      2. All audio has consistent loudness regardless of how the source WAVs
         were recorded.
      3. The cooked asset is the final, verified form — what ships in the game.
    """
    audio_src  = CONTENT_DIR / "Audio"
    audio_dst  = COOKED_DIR  / "Audio"
    ensure_dir(audio_dst)

    clips: list[dict] = []
    source_wavs = sorted(audio_src.glob("**/*.wav"))

    if not source_wavs:
        return 0

    for src in source_wavs:
        rel = src.relative_to(audio_src)   # relative to Audio/
        dst = audio_dst / rel
        dst.parent.mkdir(parents=True, exist_ok=True)

        # ------------------------------------------------------------------
        # Path A: use audio_engine DSP to normalise (if package installed)
        # ------------------------------------------------------------------
        processed = False
        if _HAS_AUDIO_ENGINE:
            try:
                from audio_engine.export.audio_exporter import AudioExporter  # type: ignore
                from audio_engine.render.offline_bounce import OfflineBounce   # type: ignore
                import numpy as np  # type: ignore

                # Read the source WAV via the exporter's importer helper
                from audio_engine.dsp.normaliser import Normaliser  # type: ignore
                normaliser = Normaliser(target_lufs=-16.0, ceiling_db=-1.0)
                import scipy.io.wavfile as sio_wav  # type: ignore
                sr, data = sio_wav.read(str(src))
                audio = data.astype(np.float32) / 32768.0
                if audio.ndim > 1:
                    audio = audio.mean(axis=1)
                normalised = normaliser.process(audio)
                AudioExporter(sample_rate=sr, bit_depth=16).export(normalised, dst, fmt="wav")
                processed = True
            except Exception as exc:
                print(f"  [WARN] audio_engine DSP failed for {src.name}: {exc} — falling back to copy")

        if not processed:
            # Path B: simple file copy (stub / fallback)
            shutil.copy2(src, dst)

        clip_id = new_guid()
        clips.append({
            "id":      clip_id,
            "name":    src.stem,
            "source":  "Audio/" + str(rel),
            "cooked":  str(dst.relative_to(SCRIPT_DIR)),
            "volume":  1.0,
            "loopable": False,
            "is3D":    False,
            "tags":    [],
        })
        print(f"  [AUD] {rel} → {dst.relative_to(SCRIPT_DIR)}")

    if clips:
        bank_id = new_guid()
        bank = {
            "$schema": "../../../shared/schemas/audio_bank.schema.json",
            "version":  "1.0.0",
            "bankId":   bank_id,
            "bankName": "VerticalSliceBank",
            "sampleRate": 44100,
            "clips":    clips,
        }
        bank_path = audio_dst / "VerticalSliceBank.bank.json"
        bank_path.write_text(json.dumps(bank, indent=2), encoding="utf-8")

        registry.append({
            "id":           bank_id,
            "type":         "audio_bank",
            "name":         "VerticalSliceBank",
            "source":       "Content/Audio",
            "cooked":       str(bank_path.relative_to(SCRIPT_DIR)),
            "hash":         "",
            "dependencies": [],
            "tags":         ["audio_bank"],
        })

    return len(clips)


def cook_scenes(registry: list[dict]) -> int:
    """Copy scene JSON files to Cooked/Maps/ and register them.

    TEACHING NOTE — Scene Cooking
    For simple JSON scenes, cooking is mostly a copy + validation step.
    A real cook might:
      1. Validate against scene.schema.json.
      2. Resolve asset references (replace names with GUIDs from the registry).
      3. Write a compact binary version for faster runtime loading.
    """
    maps_src = CONTENT_DIR / "Maps"
    maps_dst = COOKED_DIR  / "Maps"
    ensure_dir(maps_dst)

    count = 0
    for src in sorted(maps_src.glob("**/*.json")):
        rel = src.relative_to(maps_src)   # relative to Maps/, not Content/
        dst = maps_dst / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)

        registry.append({
            "id":     new_guid(),
            "type":   "scene",
            "name":   src.stem.replace(".scene", ""),
            "source": "Maps/" + str(rel),
            "cooked": str(dst.relative_to(SCRIPT_DIR)),
            "hash":   sha256_file(src),
            "dependencies": [],
            "tags":   ["scene", "map"],
        })
        count += 1
        print(f"  [MAP] {rel} → {dst.relative_to(SCRIPT_DIR)}")

    return count


def cook_animations(registry: list[dict]) -> int:
    """Cook animation JSON files to Cooked/Anim/ using anim_engine when available.

    TEACHING NOTE — Animation Cooking
    When the  tools/anim_authoring  package is installed, this function uses
    the  AnimAssetPipeline  class from  animation_engine.integration  to
    deserialise skeletons and clips, apply key-frame reduction and then write
    the cooked .skelc / .animc files.

    When the package is NOT installed (stub mode) we copy JSON files directly
    (renamed to .animc) so the rest of the pipeline still functions.
    """
    anim_src = CONTENT_DIR / "Animations"
    anim_dst = COOKED_DIR  / "Anim"
    ensure_dir(anim_dst)

    count = 0

    # ------------------------------------------------------------------
    # Path A: use the real animation_engine pipeline (installed package)
    # ------------------------------------------------------------------
    if _HAS_ANIM_ENGINE:
        print("  [INFO] animation_engine package found — using AnimAssetPipeline")
        pipeline = AnimAssetPipeline(skip_existing=False)
        manifest = pipeline.cook_all(
            content_dir=str(anim_src),
            cooked_dir=str(anim_dst),
        )
        print(f"  {manifest.summary()}")
        for entry in manifest.skeletons:
            cooked_path = Path(entry["cooked"])
            source_path = Path(entry["source"])
            registry.append({
                "id":     new_guid(),
                "type":   "skeleton",
                "name":   source_path.stem,
                "source": _try_relative_to(source_path, SCRIPT_DIR),
                "cooked": _try_relative_to(cooked_path, SCRIPT_DIR),
                "hash":   sha256_file(source_path) if source_path.exists() else "",
                "dependencies": [],
                "tags":   ["skeleton"],
            })
            count += 1
        for entry in manifest.clips:
            cooked_path = Path(entry["cooked"])
            source_path = Path(entry["source"])
            registry.append({
                "id":     new_guid(),
                "type":   "anim_clip",
                "name":   source_path.stem,
                "source": _try_relative_to(source_path, SCRIPT_DIR),
                "cooked": _try_relative_to(cooked_path, SCRIPT_DIR),
                "hash":   sha256_file(source_path) if source_path.exists() else "",
                "dependencies": [],
                "tags":   ["animation"],
            })
            count += 1
    else:
        # ------------------------------------------------------------------
        # Path B: stub — copy JSON → .animc
        # ------------------------------------------------------------------
        for src in sorted(anim_src.glob("**/*.json")):
            rel = src.relative_to(anim_src)     # relative to Animations/
            dst = anim_dst / rel.with_suffix(".animc")
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)  # STUB: copy; real cook converts to binary

            registry.append({
                "id":     new_guid(),
                "type":   "anim_clip",
                "name":   src.stem,
                "source": "Animations/" + str(rel),
                "cooked": str(dst.relative_to(SCRIPT_DIR)),
                "hash":   sha256_file(src),
                "dependencies": [],
                "tags":   ["animation"],
            })
            count += 1
            print(f"  [ANI] {rel} → {dst.relative_to(SCRIPT_DIR)}")

    return count


# ---------------------------------------------------------------------------
# Registry update
# ---------------------------------------------------------------------------

def _try_relative_to(path: Path, base: Path) -> str:
    """Return path relative to base as a string, or the original string if not relative.

    TEACHING NOTE — Python 3.9 compatibility
    Path.is_relative_to() was added in Python 3.9.  We use a try/except
    approach so the code also runs on Python 3.8 (the minimum for some CI
    runners).  When the path is not under base we return the full string.
    """
    try:
        return str(path.relative_to(base))
    except ValueError:
        return str(path)


def update_registry(registry: list[dict]) -> None:
    """Write the updated AssetRegistry.json.

    TEACHING NOTE — Asset Registry
    The registry is the single source of truth for all cooked assets.
    It maps stable GUIDs → file paths + hashes.  The engine reads it at
    startup to build an in-memory lookup table.
    """
    data = {
        "$schema":     "../../shared/schemas/asset_registry.schema.json",
        "version":     "1.0.0",
        "generatedAt": datetime.now(tz=timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "assets":      registry,
    }
    REGISTRY_FILE.write_text(json.dumps(data, indent=2), encoding="utf-8")
    print(f"\n  Registry written: {REGISTRY_FILE.name}  ({len(registry)} assets)")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    print("=" * 60)
    print(" Vertical Slice Asset Cook")
    print("=" * 60)
    print(f" Project : {PROJECT_FILE}")
    print(f" Content : {CONTENT_DIR}")
    print(f" Cooked  : {COOKED_DIR}")
    print()

    # Ensure Cooked/ exists
    ensure_dir(COOKED_DIR)

    registry: list[dict] = []
    total = 0

    print("--- Textures ---")
    n = cook_textures(registry)
    total += n
    if n == 0:
        print("  (no textures found in Content/Textures/)")

    print("\n--- Audio ---")
    n = cook_audio(registry)
    total += n
    if n == 0:
        print("  (no WAV files found in Content/Audio/)")

    print("\n--- Scenes / Maps ---")
    n = cook_scenes(registry)
    total += n
    if n == 0:
        print("  (no JSON files found in Content/Maps/)")

    print("\n--- Animations ---")
    n = cook_animations(registry)
    total += n
    if n == 0:
        print("  (no JSON files found in Content/Animations/)")

    print("\n--- Registry ---")
    update_registry(registry)

    print()
    print("=" * 60)
    print(f" Cook complete: {total} assets processed.")
    print("=" * 60)
    return 0


if __name__ == "__main__":
    sys.exit(main())
