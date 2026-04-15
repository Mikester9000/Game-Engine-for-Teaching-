# Asset Manifest Schema — Game Engine for Teaching

> **Teaching context:** This document mirrors the asset manifest systems used
> in AAA studios.  *Final Fantasy XV* ships thousands of asset files; a
> manifest layer tells the engine *what* every file is, *who* depends on it,
> and *whether* it has changed since last cook.  This schema is a simplified,
> learnable version of that concept.

---

## Table of Contents

1. [Overview](#overview)
2. [Schema Reference](#schema-reference)
   - [Manifest root](#manifest-root)
   - [Asset entry (common fields)](#asset-entry-common-fields)
   - [Audio extension](#audio-extension)
   - [Texture extension](#texture-extension)
   - [Tilemap extension](#tilemap-extension)
   - [Model extension](#model-extension)
3. [Validate-Assets CLI](#validate-assets-cli)
4. [Example Manifests](#example-manifests)
5. [Integration Guide](#integration-guide)
6. [CI / Lint Pipeline](#ci--lint-pipeline)

---

## Overview

Every asset consumed or produced by the engine (audio clips, textures, tile
maps, 3-D models, scripts, materials) is described by a **manifest file**.

```
assets/
├── schema/
│   └── asset-manifest.schema.json   ← canonical JSON Schema (draft-07)
├── examples/
│   ├── audio-manifest.json
│   ├── texture-manifest.json
│   ├── tilemap-manifest.json
│   └── model-manifest.json
```

A manifest is a plain JSON file with two top-level keys:

| Key | Type | Required | Description |
|---|---|---|---|
| `manifestVersion` | string | ✅ | SemVer of the manifest format |
| `assets` | array | ✅ | List of asset entries |

---

## Schema Reference

The complete machine-readable schema lives at
`assets/schema/asset-manifest.schema.json` (JSON Schema draft-07).

### Manifest root

```json
{
  "manifestVersion": "1.0.0",
  "assets": [ ... ]
}
```

`manifestVersion` must match `X.Y.Z` (SemVer).

---

### Asset entry (common fields)

Every asset, regardless of type, **must** include these fields:

| Field | Type | Required | Description |
|---|---|---|---|
| `id` | string | ✅ | Stable, globally-unique identifier (GUID or slug) |
| `version` | string | ✅ | Asset revision, SemVer `X.Y.Z` |
| `type` | string | ✅ | One of `audio`, `texture`, `tilemap`, `model`, `script`, `material` |
| `source` | string | ✅ | Relative path to the source file (from the manifest) |
| `hash` | string | ✅ | 64-character SHA-256 hex digest of the source file |
| `dependencies` | string[] | — | IDs of other assets this asset depends on |
| `tags` | string[] | — | Free-form labels for filtering (`["combat", "boss"]`) |

In addition, a type-specific **extension block** (e.g. `"audio": {...}`) must
be present for the four primary asset types.

---

### Audio extension

Present when `"type": "audio"`.

| Field | Type | Required | Description |
|---|---|---|---|
| `format` | string | ✅ | `wav` / `ogg` / `mp3` / `flac` / `opus` |
| `sampleRate` | integer | ✅ | Hz, e.g. `44100` or `48000` |
| `channels` | integer | ✅ | `1` = mono, `2` = stereo, `6` = 5.1 |
| `durationSeconds` | number | ✅ | Total playback length |
| `loopStart` | number | — | Loop start point in seconds |
| `loopEnd` | number | — | Loop end point (must be > `loopStart`) |
| `category` | string | — | `music` / `sfx` / `ambience` / `voice` / `ui` |

**Example:**

```json
{
  "id": "audio-prelude-of-light",
  "version": "1.0.0",
  "type": "audio",
  "source": "../audio/music/prelude_of_light.ogg",
  "hash": "a3f1c2b4d5e6f7a8b9c0d1e2f3a4b5c6d7e8f9a0b1c2d3e4f5a6b7c8d9e0f1a2",
  "tags": ["music", "main-theme"],
  "audio": {
    "format": "ogg",
    "sampleRate": 44100,
    "channels": 2,
    "durationSeconds": 180.5,
    "loopStart": 4.2,
    "loopEnd": 176.3,
    "category": "music"
  }
}
```

---

### Texture extension

Present when `"type": "texture"`.

| Field | Type | Required | Description |
|---|---|---|---|
| `format` | string | ✅ | `png` / `jpeg` / `rgba8` / `bc1` / `bc3` / `bc5` / `bc7` / `astc4x4` |
| `width` | integer | ✅ | Width in pixels |
| `height` | integer | ✅ | Height in pixels |
| `mipLevels` | integer | — | Number of mip-map levels (1 = no mips) |
| `sRGB` | boolean | — | `true` for colour data, `false` for linear (normals, roughness) |
| `usage` | string | — | PBR slot: `albedo` / `normal` / `roughness` / `metalness` / `ao` / `emissive` / `ui` / `heightmap` |

---

### Tilemap extension

Present when `"type": "tilemap"`.

| Field | Type | Required | Description |
|---|---|---|---|
| `width` | integer | ✅ | Map width in tiles |
| `height` | integer | ✅ | Map height in tiles |
| `tileWidth` | integer | ✅ | Single tile width in pixels |
| `tileHeight` | integer | ✅ | Single tile height in pixels |
| `layers` | integer | — | Number of layers (ground / objects / collision) |
| `tilesetId` | string | — | Asset `id` of the texture used as the tileset |

---

### Model extension

Present when `"type": "model"`.

| Field | Type | Required | Description |
|---|---|---|---|
| `format` | string | ✅ | `gltf` / `glb` / `fbx` / `obj` / `dae` |
| `vertexCount` | integer | ✅ | Total vertex count across all meshes |
| `triangleCount` | integer | ✅ | Total triangle count across all meshes |
| `hasSkeleton` | boolean | — | `true` if the model has a skeletal rig |
| `animationCount` | integer | — | Number of embedded animation clips |
| `lodLevels` | integer | — | Number of Level-of-Detail meshes |

---

## Validate-Assets CLI

The `tools/validate-assets.py` script validates any manifest file or directory
of manifest files against the schema.

### Install (optional enhanced validation)

```bash
pip install jsonschema       # optional — enables full JSON Schema draft-07 checks
```

Without `jsonschema`, the validator uses a built-in structural checker that
covers all required fields.

### Usage

```bash
# Validate all examples (default when no args given)
python3 tools/validate-assets.py

# Validate a single file
python3 tools/validate-assets.py assets/examples/audio-manifest.json

# Validate a whole directory (recursive)
python3 tools/validate-assets.py assets/

# Validate multiple paths
python3 tools/validate-assets.py assets/examples/ path/to/other/manifest.json

# Verbose output (show errors even for passing files)
python3 tools/validate-assets.py --verbose

# Use a custom schema
python3 tools/validate-assets.py --schema path/to/custom.schema.json assets/
```

### Exit codes

| Code | Meaning |
|---|---|
| `0` | All manifests passed |
| `1` | One or more validation errors |
| `2` | Usage or filesystem error |

### Example output

```
=== validate-assets — validator: jsonschema (full) ===
Schema : assets/schema/asset-manifest.schema.json
Files  : 4

  [PASS] assets/examples/audio-manifest.json
  [PASS] assets/examples/model-manifest.json
  [PASS] assets/examples/texture-manifest.json
  [PASS] assets/examples/tilemap-manifest.json

=== Summary ===
  Passed   : 4
  Failed   : 0
  Warnings : 0

Result: PASS — all manifests are valid.
```

---

## Example Manifests

Four complete example manifests are provided in `assets/examples/`.

| File | Type | Assets |
|---|---|---|
| `audio-manifest.json` | audio | Main theme (looped OGG), sword clash SFX, campfire ambience |
| `texture-manifest.json` | texture | Noctis albedo + normal maps, Regalia albedo, UI HUD atlas |
| `tilemap-manifest.json` | tilemap | Lucis Capital, Insomnia dungeon, Duscae overworld |
| `model-manifest.json` | model | Noctis (rigged, 32 anims), Regalia car, Behemoth boss |

All examples are kept in sync with the schema and pass the CLI validator.

---

## Integration Guide

### Creating a manifest for a new asset directory

1. Copy an appropriate example from `assets/examples/`.
2. Update `id`, `source`, `hash`, and the type-specific extension block.
3. Generate the SHA-256 hash:

   ```bash
   # Linux / macOS
   sha256sum path/to/asset.ogg

   # Python (cross-platform)
   python3 -c "import hashlib, sys; print(hashlib.sha256(open(sys.argv[1],'rb').read()).hexdigest())" path/to/asset.ogg
   ```

4. Run the validator:

   ```bash
   python3 tools/validate-assets.py your-manifest.json
   ```

### Using the manifest from C++

```cpp
// TEACHING NOTE — In a production engine, this manifest data would be parsed
// at load time so the asset manager can resolve dependencies, verify hashes,
// and stream only what is needed.
//
// For this educational engine the manifest is consumed by the CLI validator
// and by future asset manager code in src/engine/assets/ (planned).
//
// Example pseudo-code:
#include <fstream>
#include "nlohmann/json.hpp"   // or any JSON library

void LoadManifest(const std::string& path) {
    std::ifstream f(path);
    auto manifest = nlohmann::json::parse(f);

    for (auto& asset : manifest["assets"]) {
        std::string id   = asset["id"];
        std::string type = asset["type"];
        std::string src  = asset["source"];
        // ... register with AssetManager
    }
}
```

### Emitting a manifest from a creation tool

Any tool that produces game assets should write (or append to) a manifest:

```python
import json, hashlib, pathlib

def make_asset_entry(asset_id, asset_type, source_path, extra):
    data = pathlib.Path(source_path).read_bytes()
    return {
        "id": asset_id,
        "version": "1.0.0",
        "type": asset_type,
        "source": source_path,
        "hash": hashlib.sha256(data).hexdigest(),
        "dependencies": [],
        "tags": [],
        **{asset_type: extra},
    }

manifest = {
    "manifestVersion": "1.0.0",
    "assets": [
        make_asset_entry(
            "audio-battle-fanfare",
            "audio",
            "audio/sfx/battle_fanfare.wav",
            {"format": "wav", "sampleRate": 44100,
             "channels": 1, "durationSeconds": 3.2, "category": "sfx"},
        )
    ],
}
with open("my-manifest.json", "w") as f:
    json.dump(manifest, f, indent=2)
```

---

## CI / Lint Pipeline

A GitHub Actions workflow (`.github/workflows/validate-assets.yml`) runs the
validator automatically on every push and pull request that touches asset or
schema files.

```yaml
on:
  push:
    paths:
      - 'assets/**'
      - 'tools/validate-assets.py'
  pull_request:
    paths:
      - 'assets/**'
      - 'tools/validate-assets.py'
```

The job fails if any manifest file does not conform to the schema, preventing
broken assets from landing in the main branch.
