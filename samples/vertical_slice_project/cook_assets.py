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
import struct
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

try:
    from PIL import Image as _PILImage  # type: ignore
    _HAS_PILLOW = True
except ImportError:
    _HAS_PILLOW = False

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

SCRIPT_DIR   = Path(__file__).parent
CONTENT_DIR  = SCRIPT_DIR / "Content"
COOKED_DIR   = SCRIPT_DIR / "Cooked"
PROJECT_FILE = SCRIPT_DIR / "Project.json"
REGISTRY_FILE = SCRIPT_DIR / "AssetRegistry.json"

# ---------------------------------------------------------------------------
# TEACHING NOTE — Importing creation_engine from the tools directory
# ---------------------------------------------------------------------------
# The creation_engine.py baker functions (bake_collision, bake_font,
# bake_road) live in tools/.  We add the repo root to sys.path so they
# can be imported without installation.  This mirrors how the CI workflow
# runs the bakers: from the repo root with python tools/creation_engine.py.
# ---------------------------------------------------------------------------
_REPO_ROOT = SCRIPT_DIR.parent.parent
_TOOLS_DIR = _REPO_ROOT / "tools"
if str(_TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(_TOOLS_DIR))

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


# ---------------------------------------------------------------------------
# Stable-GUID helpers
# ---------------------------------------------------------------------------

# Module-level cache: source-relative-path → existing GUID.
# Populated by load_existing_guids() early in main() so that every cook
# step can call stable_guid() and reuse the GUID the asset already has.
_EXISTING_GUIDS: dict[str, str] = {}
_EXISTING_ASSETS: dict[str, dict] = {}


def load_existing_guids() -> None:
    """Load source → GUID mappings from the on-disk AssetRegistry.json.

    TEACHING NOTE — Stable GUIDs across cook runs
    ──────────────────────────────────────────────
    A GUID is *stable* if it never changes once assigned — even when the cook
    tool is re-run, files are moved, or the engine is rebuilt.  Stable GUIDs
    are essential because C++ runtime code (game_runtime.cpp, main.cpp) and the
    golden-file contract test all hardcode the GUID of specific assets.

    cook.exe (the C++ production cooker) preserves stability by reading the
    existing AssetRegistry.json and reusing any 'id' field it finds for a
    given source path.  cook_assets.py must do the same:

      1. Read AssetRegistry.json if it exists (first cook: file absent → skip).
      2. Build a { source_rel: id } dict.
      3. In each cook function, call stable_guid(source_rel) instead of
         new_guid() directly — it returns the existing ID if known, or a
         fresh UUID4 for truly new assets.
    """
    global _EXISTING_GUIDS, _EXISTING_ASSETS
    # TEACHING NOTE — Explicit reset for re-entrant safety
    # This function currently runs once from main(), but we clear caches first
    # so future callers (tests/tools that may invoke multiple cook passes in one
    # process) never retain stale registry state between runs.
    _EXISTING_GUIDS = {}
    _EXISTING_ASSETS = {}
    if not REGISTRY_FILE.exists():
        return
    try:
        data = json.loads(REGISTRY_FILE.read_text(encoding="utf-8"))
        for entry in data.get("assets", []):
            src = entry.get("source", "")
            gid = entry.get("id", "")
            if src and gid:
                _EXISTING_GUIDS[src] = gid
                _EXISTING_ASSETS[src] = entry
    except Exception:
        pass  # If the file is malformed, ignore and generate fresh GUIDs.


def stable_guid(source_rel: str) -> str:
    """Return the existing GUID for *source_rel* if known; else a fresh UUID4.

    TEACHING NOTE — Incremental cook / GUID stability
    ──────────────────────────────────────────────────
    Use this instead of new_guid() whenever you register an asset entry that
    has a 'source' path.  The lookup is O(1) (dict) and handles the case where
    the registry does not yet contain an entry for the asset (first cook run).

    Args:
        source_rel: Relative source path as it appears in AssetRegistry.json,
                    e.g. "Maps/MainTown.scene.json" or "Levels/cell_0_0.cell.json".

    Returns:
        The existing UUID v4 string for this asset, or a new UUID4.
    """
    return _EXISTING_GUIDS.get(source_rel) or new_guid()


def should_recook(source_rel: str, source_hash: str, require_dds: bool = False) -> bool:
    """Return True when the source asset should be re-cooked.

    TEACHING NOTE — Incremental cook
    We use the existing registry to skip redundant work:
      1. If the source hash changed, re-cook.
      2. If the cooked file is missing, re-cook.
      3. If *require_dds* is True and the cooked file lacks the DDS magic
         bytes ('DDS '), re-cook.  This catches the case where a previous
         run wrote a raw PNG copy instead of a proper DDS RGBA8 file.
      4. Otherwise, keep the previous cooked output and just refresh the
         in-memory registry entry for this run.
    """
    if not source_hash:
        # TEACHING NOTE — Defensive fallback
        # A missing hash means we cannot verify content identity safely.
        # Re-cook to avoid reusing potentially stale cooked outputs.
        return True

    existing = _EXISTING_ASSETS.get(source_rel)
    if not existing:
        return True
    if existing.get("hash") != source_hash:
        return True
    cooked_rel = existing.get("cooked", "")
    if not cooked_rel:
        return True
    cooked_path = SCRIPT_DIR / cooked_rel
    if not cooked_path.exists():
        return True
    if require_dds:
        # Re-cook if the file was previously saved as a raw PNG copy.
        try:
            header = cooked_path.read_bytes()[:4]
            if header != b"DDS ":
                return True
        except OSError:
            return True
    return False


def ensure_dir(path: Path) -> None:
    """Create a directory and all parents if they don't already exist."""
    path.mkdir(parents=True, exist_ok=True)


# ---------------------------------------------------------------------------
# DDS helper
# ---------------------------------------------------------------------------


def _png_to_dds_rgba8(src_path: Path) -> bytes:
    """Decode a PNG/JPG and write an uncompressed DDS RGBA8 binary blob.

    TEACHING NOTE — Why DDS Instead of Raw PNG?
    ============================================
    The D3D11 texture loader (d3d11_texture.cpp) expects either a DDS file
    (detected by the 4-byte magic 'DDS ') or a raw RGBA8 stream.  PNG files
    are *not* understood by the GPU-facing loader, so shipping PNG files as
    cooked textures silently causes every authored slot to fall back to the
    1×1 white SRV.

    DDS (DirectDraw Surface) is a container format that carries:
      - A compact binary header (magic + DDS_HEADER = 128 bytes).
      - The raw pixel data — no compression needed for the teaching build.

    The format used here is the legacy uncompressed variant:
      • DDPF_RGB | DDPF_ALPHAPIXELS flags in DDS_PIXELFORMAT.
      • 32-bit pixel layout: R8 G8 B8 A8 (matching DXGI_FORMAT_R8G8B8A8_UNORM).
      • Single mip level — no mip chain; fine for teaching purposes.

    A shipping engine would compress to BC7 (GPU block-compressed) to halve
    VRAM usage.  That step requires the ispc_texcomp library or DirectXTex
    and is beyond the scope of this teaching cook script.

    DDS_HEADER field breakdown (124 bytes total):
      dwSize            = 124          (fixed by spec)
      dwFlags           = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH |
                          DDSD_PITCH | DDSD_PIXELFORMAT
      dwHeight/Width    = image dimensions in pixels
      dwPitchOrLinearSize = width * 4  (row stride in bytes for RGBA8)
      dwDepth           = 0            (2D texture)
      dwMipMapCount     = 1            (no mip chain)
      dwReserved1[11]   = 0s           (unused)
      DDS_PIXELFORMAT:
        dwSize          = 32
        dwFlags         = DDPF_RGB | DDPF_ALPHAPIXELS
        dwFourCC        = 0            (0 → not block-compressed)
        dwRGBBitCount   = 32
        dwRBitMask      = 0x000000FF  (R in byte 0)
        dwGBitMask      = 0x0000FF00  (G in byte 1)
        dwBBitMask      = 0x00FF0000  (B in byte 2)
        dwABitMask      = 0xFF000000  (A in byte 3)
      dwCaps            = DDSCAPS_TEXTURE
      dwCaps2/3/4       = 0
      dwReserved2       = 0

    Parameters
    ----------
    src_path : Path
        Path to the source PNG or JPG image file.

    Returns
    -------
    bytes
        Complete DDS binary blob (magic + header + RGBA8 pixels).

    Raises
    ------
    OSError, ValueError
        Propagated from PIL if the source file cannot be opened or decoded.
    """
    try:
        img = _PILImage.open(src_path)
        # Validate that PIL recognised the format before converting.
        img.verify()
        img = _PILImage.open(src_path).convert("RGBA")
    except Exception as exc:
        raise ValueError(
            f"_png_to_dds_rgba8: cannot decode '{src_path}': {exc}"
        ) from exc

    width, height = img.size
    rgba_bytes: bytes = img.tobytes()  # R,G,B,A,R,G,B,A,... row-major

    # --- DDS_PIXELFORMAT (32 bytes) ---
    DDPF_ALPHAPIXELS = 0x00000001
    DDPF_RGB         = 0x00000040
    pf = struct.pack(
        "<IIIIIIII",
        32,                            # dwSize (must equal 32)
        DDPF_RGB | DDPF_ALPHAPIXELS,   # dwFlags
        0,                             # dwFourCC  (0 for uncompressed)
        32,                            # dwRGBBitCount
        0x000000FF,                    # dwRBitMask  (R in byte 0 of pixel)
        0x0000FF00,                    # dwGBitMask  (G in byte 1)
        0x00FF0000,                    # dwBBitMask  (B in byte 2)
        0xFF000000,                    # dwABitMask  (A in byte 3)
    )  # 8 × 4 = 32 bytes ✓

    # --- DDS_HEADER (124 bytes) ---
    DDSD_CAPS        = 0x00000001
    DDSD_HEIGHT      = 0x00000002
    DDSD_WIDTH       = 0x00000004
    DDSD_PITCH       = 0x00000008
    DDSD_PIXELFORMAT = 0x00001000
    DDSCAPS_TEXTURE  = 0x00001000
    pitch = width * 4  # 4 bytes per RGBA8 pixel

    hdr = struct.pack(
        "<IIIIIII",
        124,                                                          # dwSize
        DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PITCH | DDSD_PIXELFORMAT,
        height,                                                       # dwHeight
        width,                                                        # dwWidth
        pitch,                                                        # dwPitchOrLinearSize
        0,                                                            # dwDepth
        1,                                                            # dwMipMapCount
    )  # 7 × 4 = 28 bytes
    hdr += b"\x00" * 44          # dwReserved1[11] — 11 × 4 = 44 bytes
    hdr += pf                    # DDS_PIXELFORMAT  — 32 bytes
    hdr += struct.pack(
        "<IIIII",
        DDSCAPS_TEXTURE,         # dwCaps
        0,                       # dwCaps2
        0,                       # dwCaps3
        0,                       # dwCaps4
        0,                       # dwReserved2
    )  # 5 × 4 = 20 bytes
    # Total: 28 + 44 + 32 + 20 = 124 bytes ✓

    return b"DDS " + hdr + rgba_bytes


# ---------------------------------------------------------------------------
# Cook steps
# ---------------------------------------------------------------------------

def cook_textures(registry: list[dict]) -> int:
    """Cook PNG/JPG textures from Content/ to Cooked/Textures/ as DDS RGBA8.

    TEACHING NOTE — Texture Cooking (DDS RGBA8)
    ============================================
    When Pillow is available the cook step converts each source PNG/JPG into
    an uncompressed DDS RGBA8 file that the D3D11 texture loader can parse.
    The DDS format is detected by the 4-byte magic 'DDS ' at the start of
    every file; the runtime loader (d3d11_texture.cpp::LoadFromMemory) checks
    this magic before reading the header — so a raw PNG copy would always
    fail to load and silently fall back to the 1×1 white SRV.

    Output format: uncompressed DDS RGBA8 (DXGI_FORMAT_R8G8B8A8_UNORM),
    stored with a .tex extension so the AssetRegistry path conventions are
    preserved.  The D3D11Renderer resolves .tex paths via
    TryResolveAuthoredTexturePath which checks the actual file content (DDS
    magic) rather than the extension.

    When Pillow is NOT installed the step falls back to a PNG copy so the
    pipeline still runs end-to-end even without the imaging library.

    A production cook step would also:
      1. Generate a full mip chain (halve dimensions until 1×1).
      2. Block-compress to BC7 using ispc_texcomp or DirectXTex.
      3. Validate output size and GPU upload via d3dcompiler offline.
    """
    texture_src = CONTENT_DIR / "Textures"
    texture_dst = COOKED_DIR  / "Textures"
    ensure_dir(texture_dst)

    count = 0
    for src in sorted(list(texture_src.glob("**/*.png")) + list(texture_src.glob("**/*.jpg"))):
        rel    = src.relative_to(texture_src)   # relative to Textures/
        source_rel = "Textures/" + str(rel)
        source_hash = sha256_file(src)
        dst    = texture_dst / rel.with_suffix(".tex")  # rename extension
        dst.parent.mkdir(parents=True, exist_ok=True)

        if should_recook(source_rel, source_hash, require_dds=_HAS_PILLOW):
            if _HAS_PILLOW:
                # Convert PNG → DDS RGBA8 so d3d11_texture.cpp can load it.
                try:
                    dds_bytes = _png_to_dds_rgba8(src)
                    dst.write_bytes(dds_bytes)
                    action = "TEX-DDS"
                except (OSError, ValueError) as exc:
                    print(f"  [WARN] Could not convert {src.name} to DDS: {exc}. Falling back to copy.")
                    shutil.copy2(src, dst)
                    action = "TEX-COPY-ERR"
            else:
                # Fallback: raw copy (loader will fail to parse but pipeline works).
                shutil.copy2(src, dst)
                action = "TEX-COPY"
        else:
            action = "SKIP-TEX"

        registry.append({
            "id":     stable_guid(source_rel),
            "type":   "texture",
            "name":   src.stem,
            "source": source_rel,
            "cooked": str(dst.relative_to(SCRIPT_DIR)),
            "hash":   source_hash,
            "dependencies": [],
            "tags":   ["texture"],
        })
        count += 1
        print(f"  [{action}] {rel} → {dst.relative_to(SCRIPT_DIR)}")

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
        source_rel = "Audio/" + str(rel)
        source_hash = sha256_file(src)
        dst = audio_dst / rel
        dst.parent.mkdir(parents=True, exist_ok=True)

        if should_recook(source_rel, source_hash):
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
            action = "AUD"
        else:
            action = "SKIP-AUD"

        clip_id = stable_guid(source_rel)
        clips.append({
            "id":      clip_id,
            "name":    src.stem,
            "source":  source_rel,
            "cooked":  str(dst.relative_to(SCRIPT_DIR)),
            "volume":  1.0,
            "loopable": False,
            "is3D":    False,
            "tags":    [],
        })
        print(f"  [{action}] {rel} → {dst.relative_to(SCRIPT_DIR)}")

    if clips:
        bank_id = stable_guid("Content/Audio")
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

        # TEACHING NOTE — Hash for aggregate assets
        # An audio bank is assembled from multiple WAV source files, so there
        # is no single source file to hash.  We hash the cooked bank JSON
        # (written just above) so any change to the clip list or clip metadata
        # is captured and will trigger a re-cook on the next pass.
        bank_hash = sha256_file(bank_path)

        registry.append({
            "id":           bank_id,
            "type":         "audio_bank",
            "name":         "VerticalSliceBank",
            "source":       "Audio/",
            "cooked":       str(bank_path.relative_to(SCRIPT_DIR)),
            "hash":         bank_hash,
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
        source_rel = "Maps/" + str(rel)
        source_hash = sha256_file(src)
        dst = maps_dst / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        if should_recook(source_rel, source_hash):
            shutil.copy2(src, dst)
            action = "MAP"
        else:
            action = "SKIP-MAP"

        registry.append({
            "id":     stable_guid(source_rel),
            "type":   "scene",
            "name":   src.stem.replace(".scene", ""),
            "source": source_rel,
            "cooked": str(dst.relative_to(SCRIPT_DIR)),
            "hash":   source_hash,
            "dependencies": [],
            "tags":   ["scene", "map"],
        })
        count += 1
        print(f"  [{action}] {rel} → {dst.relative_to(SCRIPT_DIR)}")

    return count


def _is_skeleton_file(raw: dict, src: Path) -> bool:
    """Return True when *src* is an animation skeleton rather than a clip.

    TEACHING NOTE — Type detection heuristic
    The ``AnimAssetPipeline`` (anim_engine Path A) identifies skeletons by
    checking whether the ``$schema`` field contains the word ``"skeleton"``
    *or* whether the file stem ends with ``"_skeleton"``.  We replicate that
    exact heuristic here so stub mode (Path B) produces registry entries with
    the same ``type`` and cooked file extension as the real pipeline would.

    Args:
        raw: Parsed JSON dict from the source file.
        src: Path to the source JSON file (used for filename-suffix check).

    Returns:
        ``True`` for skeleton assets, ``False`` for animation clips.
    """
    schema_ref = str(raw.get("$schema", "")).lower()
    return "skeleton" in schema_ref or src.stem.lower().endswith("_skeleton")


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
            src_rel = _try_relative_to(source_path, SCRIPT_DIR)
            registry.append({
                "id":     stable_guid(src_rel),
                "type":   "skeleton",
                "name":   source_path.stem,
                "source": src_rel,
                "cooked": _try_relative_to(cooked_path, SCRIPT_DIR),
                "hash":   sha256_file(source_path) if source_path.exists() else "",
                "dependencies": [],
                "tags":   ["skeleton"],
            })
            count += 1
        for entry in manifest.clips:
            cooked_path = Path(entry["cooked"])
            source_path = Path(entry["source"])
            src_rel = _try_relative_to(source_path, SCRIPT_DIR)
            registry.append({
                "id":     stable_guid(src_rel),
                "type":   "anim_clip",
                "name":   source_path.stem,
                "source": src_rel,
                "cooked": _try_relative_to(cooked_path, SCRIPT_DIR),
                "hash":   sha256_file(source_path) if source_path.exists() else "",
                "dependencies": [],
                "tags":   ["animation"],
            })
            count += 1
    else:
        # ------------------------------------------------------------------
        # Path B: stub — copy JSON → .skelc / .animc
        # ------------------------------------------------------------------
        # TEACHING NOTE — Type detection in stub mode
        # The AnimAssetPipeline (Path A) distinguishes skeletons from clips
        # via the "$schema" field.  We replicate that logic here so that stub
        # mode produces the same registry entry types and cooked extensions.
        for src in sorted(anim_src.glob("**/*.json")):
            rel = src.relative_to(anim_src)     # relative to Animations/
            source_rel = "Animations/" + str(rel)
            source_bytes = src.read_bytes()
            source_hash = hashlib.sha256(source_bytes).hexdigest()

            # Detect asset type: skeleton files reference the skeleton schema
            # or use the "_skeleton" filename suffix — see _is_skeleton_file().
            try:
                raw = json.loads(source_bytes.decode("utf-8"))
            except json.JSONDecodeError as exc:
                # TEACHING NOTE — Surface malformed content during stub cooking
                # Silent fallback makes it hard for content creators to notice
                # that a file is invalid JSON and may be misclassified.  We log
                # a warning and continue with an empty object so the cook pass
                # remains resilient while still reporting the issue.
                print(
                    f"  [WARN] {src.name}: invalid animation JSON at "
                    f"line {exc.lineno}, column {exc.colno} ({exc.msg})"
                )
                raw = {}
            is_skeleton = _is_skeleton_file(raw, src)

            if is_skeleton:
                asset_type = "skeleton"
                cooked_suffix = ".skelc"
                tags = ["skeleton"]
            else:
                asset_type = "anim_clip"
                cooked_suffix = ".animc"
                tags = ["animation"]

            dst = anim_dst / rel.with_suffix(cooked_suffix)
            dst.parent.mkdir(parents=True, exist_ok=True)
            if should_recook(source_rel, source_hash):
                shutil.copy2(src, dst)  # STUB: copy; real cook converts to binary
                action = "SKL" if is_skeleton else "ANI"
            else:
                action = "SKIP-SKL" if is_skeleton else "SKIP-ANI"

            registry.append({
                "id":     stable_guid(source_rel),
                "type":   asset_type,
                "name":   src.stem,
                "source": source_rel,
                "cooked": str(dst.relative_to(SCRIPT_DIR)),
                "hash":   source_hash,
                "dependencies": [],
                "tags":   tags,
            })
            count += 1
            print(f"  [{action}] {rel} → {dst.relative_to(SCRIPT_DIR)}")

    return count


def cook_materials(registry: list[dict]) -> int:
    """Cook material JSON files to Cooked/Materials/ as runtime .material files.

    TEACHING NOTE — M23 groundwork: authored material ingestion
    For M23 we start by treating authored materials as first-class cooked assets.
    Designers author *.material.json files in Content/Materials/.  The cook step
    validates that they parse as JSON, then copies them to Cooked/Materials/ with
    a .material extension so runtime loaders can distinguish cooked material data
    from editable source JSON.
    """
    materials_src = CONTENT_DIR / "Materials"
    materials_dst = COOKED_DIR / "Materials"
    ensure_dir(materials_dst)

    count = 0
    for src in sorted(materials_src.glob("**/*.material.json")):
        rel = src.relative_to(materials_src)
        source_rel = "Materials/" + str(rel)
        source_hash = sha256_file(src)
        cooked_name = src.name[: -len(".material.json")] + ".material"
        dst = materials_dst / rel.parent / cooked_name
        dst.parent.mkdir(parents=True, exist_ok=True)

        # TEACHING NOTE — Parse-check before cook output
        # We fail fast on malformed material JSON so bad content never reaches
        # Cooked/ where the runtime would otherwise fail much later.
        try:
            json.loads(src.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            print(
                "  [WARN] "
                f"{src.name}: invalid material JSON syntax at line {exc.lineno}, "
                f"column {exc.colno} ({exc.msg}) — skipping. "
                "Hint: validate against shared/schemas/material.schema.json."
            )
            continue
        except OSError as exc:
            print(
                f"  [WARN] {src.name}: failed to read material file ({exc}) — skipping"
            )
            continue

        if should_recook(source_rel, source_hash):
            shutil.copy2(src, dst)
            action = "MAT"
        else:
            action = "SKIP-MAT"

        registry.append({
            "id":     stable_guid(source_rel),
            "type":   "material",
            "name":   src.name[: -len(".material.json")],
            "source": source_rel,
            "cooked": str(dst.relative_to(SCRIPT_DIR)),
            "hash":   source_hash,
            "dependencies": [],
            "tags":   ["material", "pbr"],
        })
        count += 1
        print(f"  [{action}] {rel} → {dst.relative_to(SCRIPT_DIR)}")

    return count


def cook_levels(registry: list[dict]) -> int:
    """Cook streaming cell descriptor files (.cell.json → .level) from Content/Levels/.

    TEACHING NOTE — M7.2: Level / Streaming Cell Cooking
    ======================================================
    A "level" asset represents one streaming cell in the open world.  Each
    cell descriptor JSON (Content/Levels/*.cell.json) contains:
      - Zone name, tilemap dimensions, danger rating
      - Enemy spawn points (tile coordinates + enemy data IDs)
      - NPC and shop ID lists

    The cook step:
      1. Reads the source .cell.json.
      2. Validates the required fields are present.
      3. Copies it to Cooked/Levels/ with the .level extension.
         (A future cook step could convert to a compact binary format.)

    The cooked .level file is loaded at runtime by GameStreamingManager::
    OnLoadCell() via AssetLoader::LoadRaw(guid).  The GUID is registered in
    AssetRegistry.json and looked up at runtime using RegisterCellGuid().

    TEACHING NOTE — Why .level instead of keeping .cell.json?
    Renaming to .level makes it explicit that this is a COOKED, runtime-ready
    file — not a raw source file.  The extension signals the content pipeline
    stage: .cell.json is edited by designers, .level is consumed by the engine.
    """
    levels_src = CONTENT_DIR / "Levels"
    levels_dst = COOKED_DIR  / "Levels"
    ensure_dir(levels_dst)

    count = 0
    for src in sorted(levels_src.glob("**/*.cell.json")):
        rel = src.relative_to(levels_src)            # relative to Levels/
        source_rel = "Levels/" + str(rel)
        source_hash = sha256_file(src)
        # TEACHING NOTE — Strip double extension: "cell_0_0.cell.json" → "cell_0_0.level"
        # Path.with_suffix() only removes the last suffix (e.g. ".json" → ".level"),
        # leaving ".cell" behind.  We strip the full ".cell.json" suffix explicitly.
        # All files matched by **/*.cell.json are guaranteed to end in ".cell.json".
        cooked_name = src.name[: -len(".cell.json")] + ".level"
        dst = levels_dst / rel.parent / cooked_name
        dst.parent.mkdir(parents=True, exist_ok=True)

        # Validate the source JSON has required fields before cooking.
        try:
            with src.open(encoding="utf-8") as f:
                data = json.load(f)
            # Required: zoneName (or a sensible fallback)
            if "tileWidth" not in data or "tileHeight" not in data:
                print(f"  [WARN] {src.name}: missing tileWidth/tileHeight — cooking anyway")
        except Exception as exc:
            print(f"  [WARN] {src.name}: JSON parse failed ({exc}) — skipping")
            continue

        if should_recook(source_rel, source_hash):
            # STUB: copy file as-is (real cook could convert to binary for faster loading)
            shutil.copy2(src, dst)
            action = "LVL"
        else:
            action = "SKIP-LVL"

        registry.append({
            "id":     stable_guid(source_rel),
            "type":   "level",
            "name":   src.name[: -len(".cell.json")],  # "cell_0_0.cell.json" → "cell_0_0"
            "source": source_rel,
            "cooked": str(dst.relative_to(SCRIPT_DIR)),
            "hash":   source_hash,
            "dependencies": [],
            "tags":   ["level", "streaming-cell"],
        })
        count += 1
        print(f"  [{action}] {rel} → {dst.relative_to(SCRIPT_DIR)}")

    return count




def cook_physics(registry: list[dict]) -> int:
    """Cook OBJ collision meshes (.obj → .phys) from Content/AI/ and Content/Physics/.

    TEACHING NOTE — M27: Collision Mesh Cooking (PHY1 Baker)
    ==========================================================
    A "collision mesh" asset is a baked representation of a level's static
    geometry used by the physics engine (Jolt) for raycasting and rigid-body
    simulation.  The cook step:
      1. Reads OBJ files from Content/AI/ (arena geometry) and Content/Physics/.
      2. Calls bake_collision() to produce a compact PHY1 binary.
      3. Registers the cooked .phys file in AssetRegistry.json.

    TEACHING NOTE — Why not ship raw OBJ to the runtime?
    OBJ is a text format designed for 3D DCC tools (Blender, Maya).  Parsing
    text at runtime is slow and the format includes data the physics engine
    does not need (normals, UVs, comments, material refs).  PHY1 is a flat
    array of float32 vertices + uint32 indices — a single memcpy away from
    Jolt's TriangleMesh shape constructor.
    """
    try:
        import creation_engine as ce
        _HAS_CE = True
    except ImportError:
        _HAS_CE = False

    # Search for .obj files in both AI/ and Physics/ content directories.
    ai_dir      = CONTENT_DIR / "AI"
    physics_dir = CONTENT_DIR / "Physics"
    phys_dst    = COOKED_DIR  / "Physics"
    ensure_dir(phys_dst)

    count = 0
    for src_dir in [ai_dir, physics_dir]:
        if not src_dir.exists():
            continue
        for src in sorted(src_dir.glob("**/*.obj")):
            rel = src.relative_to(CONTENT_DIR)
            source_rel  = str(rel)
            source_hash = sha256_file(src)
            cooked_name = src.stem + ".phys"
            dst = phys_dst / cooked_name

            if should_recook(source_rel, source_hash) and _HAS_CE:
                try:
                    import creation_engine as ce
                    ce.bake_collision(src, dst)
                    action = "PHY"
                except Exception as exc:
                    print(f"  [WARN] {src.name}: bake_collision failed ({exc}) — skipping")
                    continue
            else:
                action = "SKIP-PHY"
                if not dst.exists():
                    # Generate stub binary so the registry entry is valid.
                    dst.parent.mkdir(parents=True, exist_ok=True)
                    if _HAS_CE:
                        import creation_engine as ce
                        ce.bake_collision(src, dst)
                    action = "PHY"

            registry.append({
                "id":     stable_guid(source_rel),
                "type":   "collision",
                "name":   src.stem,
                "source": source_rel,
                "cooked": str(dst.relative_to(SCRIPT_DIR)),
                "hash":   source_hash,
                "dependencies": [],
                "tags":   ["collision", "physics"],
            })
            count += 1
            print(f"  [{action}] {rel} → {dst.relative_to(SCRIPT_DIR)}")

    return count


def cook_fonts(registry: list[dict]) -> int:
    """Cook font descriptor JSON files (.font.json → .font) from Content/UI/Fonts/.

    TEACHING NOTE — M27: Font Atlas Cooking (FNT1 Baker)
    ======================================================
    The runtime FontRenderer generates its SDF atlas at startup by rasterising
    an embedded 8×8 bitmap.  The cook step bakes the atlas offline so the
    runtime can load a pre-computed binary instead, reducing startup cost.

    The .font.json descriptor specifies glyph dimensions and ASCII char range.
    bake_font() generates:
      • UV table:  per-glyph u0/v0/u1/v1/advance (5 floats each)
      • SDF atlas: W×H R8_UNORM pixels (signed distance field)

    TEACHING NOTE — Why teach offline SDF generation?
    Signed Distance Fields are the standard technique for resolution-independent
    GPU font rendering (used in FFXV HUD, Unity TextMeshPro, Valve's Dota 2 HUD).
    Generating the SDF offline demonstrates the core principle without the
    complexity of FreeType/HarfBuzz integration.  Students can extend the baker
    to accept real TTF input as a self-study exercise.
    """
    try:
        import creation_engine as ce
        _HAS_CE = True
    except ImportError:
        _HAS_CE = False

    fonts_src = CONTENT_DIR / "UI" / "Fonts"
    fonts_dst = COOKED_DIR  / "UI" / "Fonts"
    ensure_dir(fonts_dst)

    count = 0
    for src in sorted(fonts_src.glob("**/*.font.json")):
        rel = src.relative_to(CONTENT_DIR)
        source_rel  = str(rel)
        source_hash = sha256_file(src)
        cooked_name = src.name[: -len(".font.json")] + ".font"
        dst = fonts_dst / cooked_name

        if should_recook(source_rel, source_hash) and _HAS_CE:
            try:
                import creation_engine as ce
                ce.bake_font(src, dst)
                action = "FNT"
            except Exception as exc:
                print(f"  [WARN] {src.name}: bake_font failed ({exc}) — skipping")
                continue
        else:
            action = "SKIP-FNT"
            if not dst.exists() and _HAS_CE:
                import creation_engine as ce
                ce.bake_font(src, dst)
                action = "FNT"

        registry.append({
            "id":     stable_guid(source_rel),
            "type":   "font",
            "name":   src.name[: -len(".font.json")],
            "source": source_rel,
            "cooked": str(dst.relative_to(SCRIPT_DIR)),
            "hash":   source_hash,
            "dependencies": [],
            "tags":   ["font", "ui"],
        })
        count += 1
        print(f"  [{action}] {rel} → {dst.relative_to(SCRIPT_DIR)}")

    return count


def cook_roads(registry: list[dict]) -> int:
    """Cook road spline JSON files (.road.json → .road) from Content/Roads/.

    TEACHING NOTE — M27: Road Spline Cooking (RD01 Baker)
    =======================================================
    The VehicleSystem needs road geometry at runtime to:
      • Find the nearest road segment for automatic lane-keeping.
      • Apply Ackermann steering corrections along the spline.
      • Constrain the Regalia's movement to road boundaries.

    The .road.json format specifies a series of waypoints (x, y, z) in world
    space.  bake_road() serialises these to a compact RD01 binary (10-byte
    header + N×12 bytes), dropping the JSON overhead.

    TEACHING NOTE — Road waypoints vs Bezier curves
    A production road system would upsample waypoints into a Bezier or
    Catmull-Rom spline for smooth camera and steering transitions.  For
    teaching we use piecewise linear segments, which are trivial to implement
    and still demonstrate the key concept: runtime nearest-segment queries.
    """
    try:
        import creation_engine as ce
        _HAS_CE = True
    except ImportError:
        _HAS_CE = False

    roads_src = CONTENT_DIR / "Roads"
    roads_dst = COOKED_DIR  / "Roads"
    ensure_dir(roads_dst)

    count = 0
    for src in sorted(roads_src.glob("**/*.road.json")):
        rel = src.relative_to(CONTENT_DIR)
        source_rel  = str(rel)
        source_hash = sha256_file(src)
        cooked_name = src.name[: -len(".road.json")] + ".road"
        dst = roads_dst / cooked_name

        if should_recook(source_rel, source_hash) and _HAS_CE:
            try:
                import creation_engine as ce
                ce.bake_road(src, dst)
                action = "ROAD"
            except Exception as exc:
                print(f"  [WARN] {src.name}: bake_road failed ({exc}) — skipping")
                continue
        else:
            action = "SKIP-ROAD"
            if not dst.exists() and _HAS_CE:
                import creation_engine as ce
                ce.bake_road(src, dst)
                action = "ROAD"

        registry.append({
            "id":     stable_guid(source_rel),
            "type":   "road",
            "name":   src.name[: -len(".road.json")],
            "source": source_rel,
            "cooked": str(dst.relative_to(SCRIPT_DIR)),
            "hash":   source_hash,
            "dependencies": [],
            "tags":   ["road", "vehicle", "navigation"],
        })
        count += 1
        print(f"  [{action}] {rel} → {dst.relative_to(SCRIPT_DIR)}")

    return count


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

    # Load existing GUIDs from AssetRegistry.json so cook steps can reuse them.
    # This ensures GUIDs remain stable across multiple cook runs (the C++ runtime
    # and CI tests hardcode specific GUIDs for known assets).
    load_existing_guids()

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

    print("\n--- Materials ---")
    n = cook_materials(registry)
    total += n
    if n == 0:
        print("  (no .material.json files found in Content/Materials/)")

    print("\n--- Streaming Levels ---")
    n = cook_levels(registry)
    total += n
    if n == 0:
        print("  (no .cell.json files found in Content/Levels/)")

    print("\n--- Physics Collision Meshes ---")
    n = cook_physics(registry)
    total += n
    if n == 0:
        print("  (no .obj files found in Content/AI/ or Content/Physics/)")

    print("\n--- Font Atlases ---")
    n = cook_fonts(registry)
    total += n
    if n == 0:
        print("  (no .font.json files found in Content/UI/Fonts/)")

    print("\n--- Road Splines ---")
    n = cook_roads(registry)
    total += n
    if n == 0:
        print("  (no .road.json files found in Content/Roads/)")

    print("\n--- Registry ---")
    update_registry(registry)

    print()
    print("=" * 60)
    print(f" Cook complete: {total} assets processed.")
    print("=" * 60)
    return 0


if __name__ == "__main__":
    sys.exit(main())
