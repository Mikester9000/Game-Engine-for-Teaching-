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
│   ├── Textures/         ← PBR PNG textures (character + props + terrain)  ← M24
│   │   ├── character/    ← hero, hero_rogue, hero_mage PBR sets (4 maps each)
│   │   ├── props/        ← crate_metal, barrel_wood, stone_pillar PBR sets
│   │   └── terrain/      ← grass, desert, rocky, snow PBR sets
│   ├── Audio/            ← WAV audio clips (music, SFX, ambient)            ← M24
│   │   ├── music/        ← battle/exploration/town/dungeon/boss/menu loops + fanfare
│   │   ├── sfx/          ← footsteps, UI, combat hits, items, traversal, world events
│   │   └── ambient/      ← wind, rain, forest day/night, cave, town, market beds
│   ├── Animations/       ← Skeleton + animation clip JSON                   ← M24
│   │   ├── hero_skeleton.json         ← 21-bone humanoid rig
│   │   ├── hero_* clips               ← idle/walk/run/attack + jump/dodge/cast/etc.
│   │   ├── enemy_goblin_skeleton.json ← enemy rig for combat encounters
│   │   └── enemy_goblin_* clips       ← idle/walk/attack/hit/death
│   ├── Maps/             ← Scene JSON files (MainTown, etc.)
│   ├── Materials/        ← PBR material descriptors
│   └── Levels/           ← Streaming cell descriptors
├── Cooked/               ← Generated at cook time (gitignored)
└── Saved/Logs/           ← Engine log output (gitignored)
```

### Asset budget (M24)

| Category | Files | Total size |
|----------|-------|-----------|
| Textures (source PNG) | 40 PBR maps | ~3.5 MB |
| Audio (WAV) | 32 clips | ~9.0 MB |
| Animations (JSON) | 2 skeletons + 18 clips | ~120 KB |
| **Total source** | **95+ files** | **~12.32 MB (0.012 GiB)** |

All source assets are well under the stricter 0.5 GiB target.

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
  ... (40 texture maps total)

--- Audio ---
  [AUD] ambient/wind_ambient.wav → Cooked/Audio/ambient/wind_ambient.wav
  [AUD] music/battle_theme_loop.wav → ...
  ... (32 WAV clips total, packed into VerticalSliceBank)

--- Scenes / Maps ---
  [MAP] MainTown.scene.json → Cooked/Maps/MainTown.scene.json

--- Animations ---
  [SKL] hero_skeleton.json → Cooked/Anim/hero_skeleton.skelc
  [ANI] hero_idle.json → Cooked/Anim/hero_idle.animc
  ... (18 clips + 2 skeletons)

--- Registry ---
  Registry written: AssetRegistry.json  (66 assets)

============================================================
 Cook complete: 97 assets processed.
============================================================
```

## Content included (M24)

The `Content/` directories ship with real assets ready to use:

| Directory | What's inside |
|-----------|--------------|
| `Content/Textures/character/` | 3 character PBR sets (`hero`, `hero_rogue`, `hero_mage`) with albedo/normal/metallic-roughness/AO maps |
| `Content/Textures/props/` | 3 prop PBR sets (`crate_metal`, `barrel_wood`, `stone_pillar`) |
| `Content/Textures/terrain/` | 4 terrain PBR sets (`grass`, `desert`, `rocky`, `snow`) |
| `Content/Audio/music/` | 7 tracks: battle, exploration, town, dungeon, boss, menu loops + victory fanfare |
| `Content/Audio/sfx/` | 19 event clips: footsteps, UI, weapon impacts, enemy, item, movement, and interact sounds |
| `Content/Audio/ambient/` | 7 ambience beds: wind, rain, forest day/night, cave, town crowd, market |
| `Content/Animations/` | 2 skeletons + 18 clips (hero locomotion/combat/utility and goblin enemy combat set) |

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
