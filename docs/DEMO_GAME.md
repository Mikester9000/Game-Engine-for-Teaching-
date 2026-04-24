# Demo_Game — Open World Teaching & Playable Demo

## Overview

`Demo_Game` is the standalone player-facing executable built on the teaching
engine.  It launches directly into a multi-biome open world and serves two
simultaneous purposes:

1. **Playable beta slice** — a genuinely explorable, quest-rich game area at
   the scale and polish of a small commercial vertical slice.
2. **Teaching tool** — every engine feature is represented as an in-world
   "teaching station" reachable by exploration or via the F1 debug overlay.

---

## Architecture

```
demo_game.exe
 └── demo_main.cpp       ← entry point (boot menu + windowed loop + headless CI)
     └── OpenWorld        ← state machine (BOOT_MENU → LOADING → PLAYING)
         └── GameRuntime  ← all M8 gameplay systems (combat, AI, quests, …)
             └── D3D11Renderer ← existing rendering backend (PBR, shadows, …)
```

`OpenWorld` is a pure C++17 class with no Win32 or D3D11 dependency.  It is
compiled into both `demo_game` (primary) and `engine_sandbox` (for the
`--scene demo_world` CI headless test).

---

## State Machine

```
BOOT_MENU ──(New Game)──► LOADING ──(ready)──► PLAYING
PLAYING   ──(ESC)──► PAUSED ──(resume)──► PLAYING
PLAYING   ──(F1)──► DEBUG_MENU ──(F1)──► PLAYING
```

All transitions are logged to stdout, making them observable in CI and
in a terminal session.

---

## Building

### Option 1 — Dedicated demo preset (recommended)

```bat
cmake --preset windows-ninja-debug-demo
cmake --build --preset windows-ninja-debug-demo
.\build\windows-ninja-debug-demo\demo_game.exe
```

### Option 2 — Add to any existing preset

```bat
cmake --preset windows-debug-engine-only -DBUILD_DEMO_GAME=ON
cmake --build --preset windows-debug-engine-only
.\build\windows-debug-engine-only\demo_game.exe
```

### Headless CI (no GPU, no window)

```bat
.\build\windows-ninja-debug-demo\demo_game.exe --headless
```

Expected output:

```
[demo_game] Starting headless validation ...
[OK] demo_world/1: Init — 12 stations registered.
[OK] demo_world/2: Initial state = BOOT_MENU.
[OK] demo_world/3: All biomes visited; headless done at frame 124.
[OK] demo_world/4: Teleport → rendering_pbr = GRASSLAND.
[OK] demo_world/5: All 12 stations have non-empty id + displayName + sceneHint.
[PASS] demo_world: 5 acceptance tests passed ...
```

---

## Biomes

| ID          | Display Name        | Area (world-units)           | Music Track          |
|-------------|--------------------|-----------------------------|----------------------|
| `grassland` | Lucis Plains        | (0,0) – (512,512)           | exploration_plains   |
| `forest`    | Vesperpool Edge     | (0,512) – (512,1024)        | exploration_forest   |
| `snow`      | Ghorovas Rift       | (512,512) – (1024,1024)     | exploration_highland |
| `desert`    | Leide Badlands      | (512,0) – (1024,512)        | exploration_desert   |
| `coast`     | Cape Caem Shore     | (384,640) – (640,1024)      | exploration_coast    |

Biome sky colours are defined in `OpenWorld::GetClearColour()` in
`src/demo_game/open_world.cpp`.  The file
`samples/vertical_slice_project/Content/World/open_world.json` is the
planned authoring format for a future data-driven biome loader, but it is
not consumed by the current runtime yet.

---

## Teaching Stations

The current `OpenWorld` runtime registers stations in C++ inside
`RegisterDefaultStations()` (`src/demo_game/open_world.cpp`).  The file
`samples/vertical_slice_project/Content/World/teaching_stations.json` is
the planned authoring format for a future data-driven version of the station
list.  Until that runtime loading path is implemented, adding or changing
stations still requires a C++ edit.

| ID                  | Station Name               | Engine Feature                    |
|---------------------|---------------------------|-----------------------------------|
| `rendering_pbr`     | PBR Rendering              | Cook-Torrance BRDF, IBL, tonemap  |
| `rendering_shadows` | Shadows & Bloom            | Shadow map PCF, Gaussian bloom    |
| `sky_weather`       | Dynamic Sky & Weather      | SkyRenderer, WeatherFx            |
| `physics_jolt`      | Jolt Physics               | Rigid body, capsule, raycast      |
| `audio_3d`          | 3D Positional Audio        | X3DAudio DSP, attenuation         |
| `animation_skinning`| GPU Skeletal Animation     | GpuSkinningBuffer, IK solver      |
| `ai_formation`      | Behaviour Trees & Formation| BtTree, FormationSystem, NavMesh  |
| `quests_dialogue`   | Quests & Dialogue          | QuestSystem, DialogueSystem       |
| `world_streaming`   | World Streaming            | WorldStreamingManager, AsyncLoader|
| `terrain`           | Terrain Renderer           | TerrainRenderer, TRN1 format      |
| `cinematics`        | Cinematics                 | CinematicSequencer, CameraRig     |
| `combat`            | Action Combat              | ComboSystem FSM, damage formula   |

---

## F1 Debug / Teaching Station Menu

Press **F1** at any time while playing to toggle the demo's debug-menu state.
In the current implementation `demo_main.cpp` flips `debugMenuOpen` and emits
a log line so the input path can be verified during development.

> **Current status:** the full in-game ImGui teaching-station overlay is **not
> yet implemented** in `demo_main.cpp`.  The F1 path is a stub / hook for
> the future overlay.

Planned expansion for this menu:

- List all teaching stations (from `OpenWorld::GetStations()`).
- Teleport instantly to a selected station (classroom demo mode).
- Toggle debug overlays (AI nav-mesh, streaming cell grid, audio zones).

---

## Extending the Demo

### Add a new teaching station

1. Open `src/demo_game/open_world.cpp` and append an entry to
   `RegisterDefaultStations()`.
2. Add a matching entry in
   `samples/vertical_slice_project/Content/World/teaching_stations.json`
   so the authoring record stays in sync with the C++ data.
3. Rebuild `demo_game`.

> **Future:** once a JSON runtime loader is added, step 1 will be replaced
> by editing the JSON file only — no C++ change needed.

### Add a new biome

1. Add the new value to the `BiomeType` enum in `src/demo_game/open_world.hpp`.
2. Add its sky colour to `OpenWorld::GetClearColour()` in
   `src/demo_game/open_world.cpp`.
3. Add matching stations in `RegisterDefaultStations()` if needed.
4. Add a corresponding entry in
   `samples/vertical_slice_project/Content/World/open_world.json` to keep
   the authoring record in sync.
5. Rebuild `demo_game`.

### Add a new quest

`samples/vertical_slice_project/Content/quest_bank.json`.
Add a new quest entry following the schema (see `shared/schemas/quest_bank.schema.json`).
The `QuestSystem` (M8/M20) will automatically pick it up.

---

## CI Integration

The `demo_world` scene is validated in every CI run as part of the primary
`build-windows` job:

```yaml
- name: Run Demo_Game acceptance test (demo_world — OpenWorld state machine)
  run: .\build\windows-ninja-debug-engine-only\engine_sandbox.exe --headless --scene demo_world
```

A dedicated `build-windows-demo` CI job builds `demo_game.exe` and runs its
headless path:

```yaml
- name: Run Demo_Game headless acceptance test (demo_main path)
  run: .\build\windows-ninja-debug-demo\demo_game.exe --headless
```

---

## Milestones Checklist

| Milestone | Description | Status |
|-----------|-------------|--------|
| **M-DG1** | `demo_game` executable + CMake target + `BUILD_DEMO_GAME` option | ✅ Done |
| **M-DG2** | `OpenWorld` state machine (BOOT_MENU → LOADING → PLAYING) | ✅ Done |
| **M-DG3** | Biome definitions JSON + teaching station definitions JSON | ✅ Done |
| **M-DG4** | `--scene demo_world` headless CI test (5 acceptance tests) | ✅ Done |
| **M-DG5** | `build-windows-demo` CI job (builds + validates `demo_game.exe`) | ✅ Done |
| **M-DG6** | F1 debug overlay (station list, teleport, overlays) | 🔨 Windowed only (F1 key detected; ImGui pass pending) |
| **M-DG7** | Full open-world biome traversal with real streaming cells | ⬜ Pending content population |
| **M-DG8** | NPC vendors, interactable camp sites, side activities | ⬜ Pending |
| **M-DG9** | Settings menu (resolution, volume, key bindings) | ⬜ Pending |
| **M-DG10**| Performance / LOD pass (Intel HD 4000 baseline) | ⬜ Pending |

---

## See Also

- `src/demo_game/open_world.hpp/.cpp` — OpenWorld state machine
- `src/demo_game/demo_main.cpp` — Standalone entry point
- `src/sandbox/game_runtime.hpp/.cpp` — Reused M8 gameplay systems
- `samples/vertical_slice_project/Content/World/` — Content JSON files
- `docs/FF15_REQUIREMENTS_BLUEPRINT.md` — Full requirements reference
