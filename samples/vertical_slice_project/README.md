# Vertical Slice Project

This sample project demonstrates the complete end-to-end pipeline for the
**Game Engine for Teaching** monorepo.

## What's here

```
vertical_slice_project/
├── Project.json          ← Project descriptor (see shared/schemas/project.schema.json)
├── AssetRegistry.json    ← Asset catalog (updated by cook_assets.py)
├── cook_assets.py        ← One-command asset cook script
├── Content/              ← Raw source assets (version-controlled)
│   ├── AI/               ← Sample OBJ geometry for nav-mesh baking (M21)
│   ├── environment/      ← Sample time-of-day curve JSON (M21)
│   ├── Textures/         ← PBR PNG textures (character + terrain sets)  ← M24
│   │   ├── character/    ← hero_albedo/normal/metallic_roughness/ao.png
│   │   └── terrain/      ← terrain_grass_albedo/normal/metallic_roughness/ao.png
│   ├── Audio/            ← WAV audio clips (music, SFX, ambient)        ← M24
│   │   ├── music/        ← battle_theme_loop.wav, exploration_loop.wav
│   │   ├── sfx/          ← footstep_stone/grass, ui_click, sword_swing, magic_cast
│   │   └── ambient/      ← wind_ambient.wav
│   ├── Animations/       ← Skeleton + animation clip JSON               ← M24
│   │   ├── hero_skeleton.json   ← 21-bone humanoid rig
│   │   ├── hero_idle.json       ← 2s looping idle (breathing + head sway)
│   │   ├── hero_walk.json       ← 1.2s looping walk cycle (root-motion)
│   │   ├── hero_run.json        ← 0.7s looping run cycle (root-motion)
│   │   └── hero_attack.json     ← 0.8s sword slash (hit_frame event)
│   ├── Maps/             ← Scene JSON files (MainTown, etc.)
│   ├── Materials/        ← PBR material descriptors
│   └── Levels/           ← Streaming cell descriptors
├── Cooked/               ← Generated at cook time (gitignored)
└── Saved/Logs/           ← Engine log output (gitignored)
```

### Asset budget (M24)

| Category | Files | Total size |
|----------|-------|-----------|
| Textures (source PNG) | 8 × 512×512 | ~332 KB |
| Audio (WAV) | 8 clips | ~1.3 MB |
| Animations (JSON) | 1 skeleton + 4 clips | ~52 KB |
| **Total source** | **21 new files** | **< 2 MB** |

All source assets are well under the 3 GiB repository budget.

## How to cook

```bat
:: From the repo root:
cd samples\vertical_slice_project
python cook_assets.py
```

Expected output (M24 — full content):
```
============================================================
 Vertical Slice Asset Cook
============================================================
 Project : .../samples/vertical_slice_project/Project.json
 Content : .../Content
 Cooked  : .../Cooked

--- Textures ---
  [TEX] character/hero_albedo.png → Cooked/Textures/character/hero_albedo.tex
  [TEX] character/hero_ao.png → ...
  ... (8 textures total)

--- Audio ---
  [AUD] ambient/wind_ambient.wav → Cooked/Audio/ambient/wind_ambient.wav
  [AUD] music/battle_theme_loop.wav → ...
  ... (8 WAV clips total, packed into VerticalSliceBank)

--- Scenes / Maps ---
  [MAP] MainTown.scene.json → Cooked/Maps/MainTown.scene.json

--- Animations ---
  [SKL] hero_skeleton.json → Cooked/Anim/hero_skeleton.skelc
  [ANI] hero_idle.json → Cooked/Anim/hero_idle.animc
  ... (4 clips + 1 skeleton)

--- Registry ---
  Registry written: AssetRegistry.json  (20 assets)

============================================================
 Cook complete: 27 assets processed.
============================================================
```

## Content included (M24)

The `Content/` directories ship with real assets ready to use:

| Directory | What's inside |
|-----------|--------------|
| `Content/Textures/character/` | 4-map PBR set for the hero: albedo, normal, metallic-roughness, AO (512×512 PNG) |
| `Content/Textures/terrain/` | 4-map PBR set for grass terrain: albedo, normal, metallic-roughness, AO (512×512 PNG) |
| `Content/Audio/music/` | `battle_theme_loop.wav` — driving 140 BPM battle music; `exploration_loop.wav` — calm 90 BPM exploration music |
| `Content/Audio/sfx/` | `footstep_stone.wav`, `footstep_grass.wav`, `ui_click.wav`, `sword_swing.wav`, `magic_cast.wav` |
| `Content/Audio/ambient/` | `wind_ambient.wav` — 4-second loopable wind atmosphere |
| `Content/Animations/` | `hero_skeleton.json` (21-bone humanoid rig) + `hero_idle/walk/run/attack.json` clips |

## How to add more assets

1. **Textures**: Drop PNG/JPG files into `Content/Textures/`. Run cook. The
   cook step copies them to `Cooked/Textures/*.tex` (stub for now; a future
   milestone adds real BC7 compression via DirectXTex).

2. **Audio**: Drop WAV files into `Content/Audio/`. Run cook. They are
   packaged into `Cooked/Audio/VerticalSliceBank.bank.json`.

3. **Scenes**: Open the Creation Suite Editor, load this project, edit the
   scene in the scene editor, and save to `Content/Maps/*.scene.json`. The
   cook step copies scenes to `Cooked/Maps/`.

4. **Animations**: Follow the existing skeleton/clip JSON schema in
   `Content/Animations/` or use `tools/anim_authoring/` to export clips.
   Run cook — clips appear in `Cooked/Anim/`.

5. **Nav-mesh (M21)**: Bake OBJ geometry to a cooked nav-mesh:
   ```bash
   python ../../tools/creation_engine.py bake-navmesh \
       --input Content/AI/arena_plane.obj \
       --output Cooked/AI/arena_plane.navmesh
   ```

6. **Time-of-day LUT (M21)**: Bake ToD curves to `tod.lut`:
   ```bash
   python ../../tools/creation_engine.py bake-tod \
       --input Content/environment/tod.json \
       --output Cooked/environment/tod.lut
   ```

## Next steps (Milestone 2)

- Replace texture copy stub with real PNG → BC7 compression
- Replace audio copy stub with WAV → OGG + loop-point encoding
- Replace animation copy stub with JSON → compact binary `.animc` format
- Wire up the cooked asset registry to the engine's asset loader
