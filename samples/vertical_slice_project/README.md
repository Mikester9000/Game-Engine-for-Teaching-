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
│   ├── Textures/         ← PNG placeholder textures
│   ├── Audio/            ← WAV placeholder audio
│   ├── Maps/             ← Scene JSON files (MainTown, Dungeon, etc.)
│   └── Animations/       ← Source animation JSON
├── Cooked/               ← Generated at cook time (gitignored)
└── Saved/Logs/           ← Engine log output (gitignored)
```

## How to cook

```bat
:: From the repo root:
cd samples\vertical_slice_project
python cook_assets.py
```

Expected output:
```
============================================================
 Vertical Slice Asset Cook
============================================================
 Project : .../samples/vertical_slice_project/Project.json
 Content : .../Content
 Cooked  : .../Cooked

--- Textures ---
  (no textures found in Content/Textures/)

--- Audio ---
  (no WAV files found in Content/Audio/)

--- Scenes / Maps ---
  [MAP] Maps/MainTown.scene.json → Cooked/Maps/MainTown.scene.json

--- Animations ---
  (no JSON files found in Content/Animations/)

--- Registry ---
  Registry written: AssetRegistry.json  (1 assets)

============================================================
 Cook complete: 1 assets processed.
============================================================
```

## How to add real assets

1. **Textures**: Drop PNG/JPG files into `Content/Textures/`. Run cook. The
   cook step copies them to `Cooked/Textures/*.tex` (stub for now; Milestone
   2 adds real texture compression).

2. **Audio**: Drop WAV files into `Content/Audio/`. Run cook. They are
   packaged into `Cooked/Audio/VerticalSliceBank.bank.json`.

3. **Scenes**: Open the Creation Suite Editor, load this project, edit the
   scene in the scene editor, and save to `Content/Maps/*.scene.json`. The
   cook step copies scenes to `Cooked/Maps/`.

4. **Animations**: Use `tools/anim_authoring/` to export animation clips as
   JSON to `Content/Animations/`. Run cook. They appear in `Cooked/Anim/`.

## Next steps (Milestone 2)

- Replace texture copy stub with real PNG → BC7 compression
- Replace audio copy stub with WAV → OGG + loop-point encoding
- Replace animation copy stub with JSON → compact binary `.animc` format
- Wire up the cooked asset registry to the engine's asset loader
