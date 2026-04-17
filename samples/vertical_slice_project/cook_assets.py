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
    """STUB: Copy WAV audio to Cooked/Audio/.

    TEACHING NOTE — Audio Cooking (Stub)
    A real cook step would:
      1. Import the AudioEngine from tools/audio_authoring/.
      2. Call AudioEngine.generate_track() or load the source WAV.
      3. Apply DSP (normalise, compress, add loop points).
      4. Export as OGG Vorbis (smaller than WAV) into a .bank container.
    For now we copy WAV files and write a minimal .bank JSON manifest.
    """
    audio_src  = CONTENT_DIR / "Audio"
    audio_dst  = COOKED_DIR  / "Audio"
    ensure_dir(audio_dst)

    clips: list[dict] = []
    for src in sorted(audio_src.glob("**/*.wav")):
        rel = src.relative_to(audio_src)   # relative to Audio/
        dst = audio_dst / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
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
    """STUB: Cook animation JSON files to Cooked/Anim/.

    TEACHING NOTE — Animation Cooking (Stub)
    A real cook step would:
      1. Load the source .anim JSON using animation_engine.io.Importer.
      2. Apply key-frame reduction (remove redundant keys within tolerance).
      3. Write a compact .animc binary (header + packed float channels).
      4. Do the same for skeletons (.skelc).
    """
    anim_src = CONTENT_DIR / "Animations"
    anim_dst = COOKED_DIR  / "Anim"
    ensure_dir(anim_dst)

    count = 0
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
