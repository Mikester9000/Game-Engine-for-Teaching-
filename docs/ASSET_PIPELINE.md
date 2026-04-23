# Asset Pipeline — Game Engine for Teaching

This document describes how raw source assets (PNG images, WAV audio, JSON data)
are cooked into engine-ready binary formats and loaded at runtime.

---

## End-to-End Flow

```
┌─────────────────────────────────────────────────────────────────────┐
│  Authoring (Editor / Tools)                                         │
│  ─────────────────────────                                          │
│  Content/Textures/hero.png         (raw PNG, 2048×2048)            │
│  Content/Audio/battle.wav          (raw WAV)                        │
│  Content/Animations/run.anim.json  (JSON keyframe data)             │
│  Content/Terrain/world.terrain.json (heightmap description)         │
└──────────────┬──────────────────────────────────────────────────────┘
               │  cook_assets.py  (Python)
               ▼
┌─────────────────────────────────────────────────────────────────────┐
│  Cooked/                                                            │
│  ──────                                                             │
│  Cooked/Textures/hero.tex          (DDS RGBA8 or BC7)              │
│  Cooked/Audio/battle_bank.json     (audio bank JSON)                │
│  Cooked/Animations/run.animc       (binary animation clip)          │
│  Cooked/Terrain/world.trn          (TRN1 binary heightmap)          │
│  Cooked/assetdb.json               (runtime asset database)         │
└──────────────┬──────────────────────────────────────────────────────┘
               │  engine_sandbox.exe (runtime)
               ▼
┌─────────────────────────────────────────────────────────────────────┐
│  Runtime                                                            │
│  ───────                                                            │
│  AssetDB::Load("Cooked/assetdb.json")                               │
│  AssetLoader::LoadRaw(guid)  →  std::vector<uint8_t>               │
│  D3D11Texture::Load(bytes)   →  ID3D11ShaderResourceView*           │
└─────────────────────────────────────────────────────────────────────┘
```

**Rule:** The engine **always** loads from `Cooked/`.  Never load directly from `Content/` in shipping code.

---

## Running the Cook

From the project root:

```bash
# Cook all assets in the vertical slice project:
python samples/vertical_slice_project/cook_assets.py

# Or cook just the C++ AssetDB side (reads AssetRegistry.json, copies → Cooked/):
.\build\windows-ninja-debug-engine-only\cook.exe \
    --project samples\vertical_slice_project
```

### What cook_assets.py does

| Source type | Cook step | Output format |
|-------------|-----------|---------------|
| `.png` | `_png_to_dds_rgba8()` via Pillow | `.tex` (DDS RGBA8, `DDS ` magic) |
| `.wav` | copy | `.wav` |
| `.anim.json` | stub (planned: binary serialisation) | `.animc` |
| `.skeleton.json` | stub | `.skelc` |
| `.terrain.json` | `bake-terrain` via `creation_engine.py` | `.trn` (TRN1 format) |
| `.cell.json` | copy | `.level` |
| Audio bank | hash + metadata | audio bank `.json` |

---

## DDS Format Rationale

GPU textures are stored in **DDS** (DirectDraw Surface) format because:

1. **GPU-native** — DDS is loaded directly into GPU memory without conversion
2. **Block compression** — DDS supports BC1/BC3/BC7 which reduce VRAM 4–8×
3. **Mip-maps** — DDS can embed a full mip chain; the GPU uses the right level
   automatically based on screen-space coverage (eliminates aliasing)
4. **Fast load** — no CPU decode step; the raw bytes go straight to `CreateTexture2D`

PNG requires CPU decode (libjpeg-turbo or Pillow) and upload — 3× slower at runtime.

### Current format: RGBA8 Uncompressed

`cook_assets.py` currently produces **RGBA8 uncompressed** DDS (`DXGI_FORMAT_R8G8B8A8_UNORM`):
- 4 bytes per pixel
- Works everywhere; no texconv tool needed
- Good for development; suboptimal for shipping

### Planned: BC7 Block Compression (AP-1)

When `texconv.exe` (from DirectXTex) is available:

```python
# cook_assets.py — planned BC7 path:
subprocess.run([texconv, "-f", "BC7_UNORM", "-y", "-o", cooked_dir, src_png], check=True)
```

BC7 achieves 0.5–1 bytes/pixel (4–8× reduction).  The D3D11 texture loader
in `d3d11_texture.cpp` already handles BC7 via the DX10-extended header.

### Mip-maps (AP-2)

Texconv generates a full mip chain by default (omit `-m 1`).  The D3D11 loader
reads `dwMipMapCount` when `DDSD_MIPMAPCOUNT` is set in `dwFlags` and creates
the texture with `MipLevels = mipCount`.

---

## PAK Packaging

For distribution, cooked assets are bundled into a single **PAK1** archive:

```bat
:: Pack the entire Cooked/ directory:
.\build\windows-ninja-debug-engine-only\pak.exe ^
    --input samples\vertical_slice_project\Cooked\ ^
    --output samples\vertical_slice_project\game.pak

:: List contents:
.\build\windows-ninja-debug-engine-only\pak.exe --list samples\vertical_slice_project\game.pak

:: Extract:
.\build\windows-ninja-debug-engine-only\pak.exe ^
    --extract samples\vertical_slice_project\game.pak ^
    --output %TEMP%\extracted
```

### PAK1 Binary Format

```
Offset 0  : "PAK1" (4 bytes magic)
Offset 4  : uint32  number of entries
Offset 8  : Table of contents (per entry):
              char[256] path (null-terminated)
              uint64    offset
              uint64    size
After TOC : Entry data blobs (concatenated)
```

---

## Validating Assets

### 1. DDS magic check (authored_content scene)

Confirms every `.tex` file in `Cooked/` has the DDS magic bytes (`DDS `) and a
valid `DDS_HEADER` (dwSize == 124):

```bat
engine_sandbox.exe --headless --scene authored_content
set ENGINE_PROJECT_ROOT=samples\vertical_slice_project
```

### 2. AssetDB round-trip (--validate-project)

Loads `Cooked/assetdb.json` and tries to open every cooked file path:

```bat
engine_sandbox.exe --validate-project samples\vertical_slice_project
```

### 3. PAK round-trip (CI-3)

Pack → extract → validate:

```bat
pak.exe --input Cooked\ --output game.pak
pak.exe --extract game.pak --output %TEMP%\extracted
engine_sandbox.exe --validate-project %TEMP%\extracted
```

---

## Adding a New Asset Type

### Step 1 — Define the schema

Add a JSON Schema under `shared/schemas/`:

```json
// shared/schemas/my_asset.schema.json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "required": ["version", "type"],
  "properties": {
    "version": { "type": "string" },
    "type":    { "const": "my_asset" }
  }
}
```

### Step 2 — Add to AssetRegistry.json

```json
{
  "id": "11111111-...",
  "type": "my_asset",
  "source": "Content/MyAssets/example.myasset.json",
  "cooked": "Cooked/MyAssets/example.myc",
  "hash": "",
  "dependencies": []
}
```

### Step 3 — Add a cook step

In `cook_assets.py`, add a `cook_my_assets()` function called from `cook_all()`:

```python
def cook_my_assets() -> None:
    """Cook .myasset.json files to .myc binary format."""
    src_dir   = CONTENT_DIR / "MyAssets"
    cooked_dir = COOKED_DIR / "MyAssets"
    cooked_dir.mkdir(parents=True, exist_ok=True)
    for src in src_dir.glob("*.myasset.json"):
        out = cooked_dir / src.with_suffix(".myc").name
        # ... bake logic ...
```

### Step 4 — Add a runtime loader

In `src/engine/assets/`, add a `my_asset_loader.hpp/.cpp` that reads the binary
format and returns a `MyAsset` struct.

### Step 5 — Add a headless test

In `src/sandbox/main.cpp`, add `--scene my_asset_test` that:
1. Calls `AssetLoader::LoadRaw(guid)` for a my_asset entry
2. Checks the binary magic bytes
3. Prints `[PASS] my_asset_test` or `[FAIL]`
