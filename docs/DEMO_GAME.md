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
 └── demo_main.cpp        ← entry point (boot menu + windowed loop + headless CI)
     └── OpenWorld         ← state machine (BOOT_MENU → LOADING → PLAYING)
         ├── DemoQuestManager ← main quest + 4 side activities (M-DG9/10)
         └── GameRuntime   ← all M8 gameplay systems (combat, AI, quests, …)
             └── D3D11Renderer ← existing rendering backend (PBR, shadows, …)
```

`OpenWorld` is a pure C++17 class with no Win32 or D3D11 dependency.  It is
compiled into both `demo_game` (primary) and `engine_sandbox` (for the
`--scene demo_world` CI headless test).

`DemoQuestManager` is also pure C++17 with no platform dependency.  It tracks
the demo main quest and three side activities and exposes event hooks that
`OpenWorld` calls on station visits, enemy kills, and item pickups.

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
[DemoQuestManager] Initialised — main quest "Tour of the Engine" (3 objectives), 4 side activities.
[OK] demo_world/1: Init — 12 stations registered.
[OK] demo_world/2: Initial state = BOOT_MENU.
[OK] demo_world/3: All biomes visited; headless done at frame 124.
[OK] demo_world/4: 12 teaching stations registered; all have non-empty id, displayName, and sceneHint.
[OK] demo_world/5: Teleport to rendering_pbr → biome = GRASSLAND.
[OK] demo_world/6: JSON fallback safe — 12 stations retained after missing-file load.
[OK] demo_world/7: DemoQuestManager — 5 defined (1 main quest, 4 activities); quest completes on station visits; Scanner advances.
[OK] demo_world/8: lesson data valid — combat lesson has text + 3 code pointer(s) + combo challenge (3 keys: 1 → 3).
[OK] demo_world/9: combo challenge — Teleport+Interact+NotifyChallengePassed → COMBO_PRACTICE complete.
[PASS] demo_world: 9 acceptance tests passed (init, boot_menu, biome_cycle, stations, teleport, json_fallback, quest_manager_interact, lesson_data, combo_challenge).
```

---

## Boot Menu

When `demo_game.exe` launches it shows a title screen overlay in BOOT_MENU state.

| Key | Action |
|-----|--------|
| **↑ / ↓** | Navigate menu items |
| **Enter** | Select highlighted item |

Menu items:
- **New Game** — transitions to LOADING → PLAYING state
- **Continue** — (stub; coming in future milestone)
- **Settings** — (stub; coming in future milestone)
- **Quit** — cleanly exits the application

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

`OpenWorld::Init()` first registers the canonical stations in C++ via
`RegisterDefaultStations()`, then calls `TryLoadStationsFromJSON()` to
optionally override them from
`samples/vertical_slice_project/Content/World/teaching_stations.json`
(requires `ENGINE_ENABLE_JSON` / nlohmann-json via vcpkg).  When the JSON
file is missing or malformed, the C++ defaults are used — the game always
boots regardless of content-file status.

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

## Quest & Activity Gameplay Loop (M-DG9, updated)

### Main Quest — "Tour of the Engine"

The main quest is a **tour quest** designed for teaching.  The player must
physically interact with each station by pressing **E** (Interact) — not just
teleport to it.  Pressing E shows the station's lesson panel and advances the
objective.

| Step | Objective | Station | Engine Feature |
|------|-----------|---------|---------------|
| 1 | Press E at the Quests & Dialogue station | `quests_dialogue` | QuestSystem + DialogueSystem |
| 2 | Press E at the Action Combat station | `combat` | ComboSystem FSM |
| 3 | Press E at the PBR Rendering station | `rendering_pbr` | Cook-Torrance BRDF + IBL |

**Reward**: 500 XP · 3,000 Gil (via QuestSystem when GameRuntime is active)

**Teleport is navigation-only**: The F1 menu teleports the player to a station
for convenience (especially useful for a classroom demo).  Teleporting does NOT
advance quest objectives — only pressing E at the station does.

### Side Activities (teaching-oriented)

All three interact-based activities are triggered by pressing **E** (Interact) at
teaching stations.  The fourth activity (Combo Challenger) additionally requires
completing the interactive combo mini-game:

| Activity | Goal | How to complete |
|----------|------|----------------|
| **Lesson Reader** | Interact at 3 distinct stations | Press E at any 3 different stations |
| **Code Explorer: Combat** | Read the Combat lesson | Press E at the Action Combat station |
| **Code Explorer: Rendering** | Read the Rendering lesson | Press E at the PBR Rendering station |
| **Combo Challenger** | Complete the combo practice at Combat station | Read the combat lesson, press **[Space]**, then input the 3-key combo sequence |

### Interact Flow

```
Player teleports to station (F1 → Enter)   ← navigation only
         ↓
"[E] Interact — <Station Name>" prompt appears
         ↓
Player presses E
         ↓
  • Lesson panel opens (lessonTitle + lessonText + codePointers)
  • Quest objective advances (if station matches current objective)
  • STATION_INTERACT activities advance
  • If station has a combo challenge → "[Space] Start Combo Challenge" shown
         ↓
Player presses ESC or Enter → lesson panel closes
         OR
Player presses Space (when challenge available) → combo challenge mini-game starts
```

---

### Combo Challenge Mini-Game

The **Combo Challenger** is an optional gameplay loop available at stations that
define a `challengeTitle` and `challengeKeys` in `station_lessons.json`.  It
makes the ComboSystem lesson interactive rather than purely text-based.

**How to play:**

1. Press **E** at the **Action Combat** station to open the lesson panel.
2. Read the lesson about the ComboSystem FSM (IDLE → BUILDING → COOLDOWN).
3. Press **[Space]** to start the Combo Challenge.
4. The challenge overlay shows:
   - The challenge title.
   - The full key sequence (e.g., `[1] → [2] → [3]`).
   - Which key to press next (highlighted).
5. Press **[1]**, **[2]**, **[3]** in order.
6. On success: "Combo Complete! BUILDING state fired" is shown for 2 seconds.
7. The **Combo Challenger** side activity is marked complete.

**What it teaches:**

Each keypress corresponds to a combo input arriving at the `ComboSystem`'s
`BUILDING` state.  The student physically walks the FSM:
- [1] = first input arrives → state IDLE → BUILDING.
- [2] = second input; prefix still matches → stay BUILDING.
- [3] = third input; exact match fires → combo executes → COOLDOWN.

```
ComboSystem (src/engine/combat/combo_system.hpp)
  IDLE ──[1]──► BUILDING ──[2]──► BUILDING ──[3]──► COOLDOWN ──► IDLE
```

**Controls during challenge:**

| Key | Action |
|-----|--------|
| **[1] [2] [3]** | Input combo keys (press in order shown) |
| **ESC** | Exit challenge (activity not awarded) |

### Quest HUD Overlay

While in the **PLAYING** state (and not in the F1 menu, lesson panel, or
challenge overlay), the bottom-left of the screen shows a persistent quest
progress panel:

```
┌──────────────────────────────────────────────────────────────┐
│ MAIN [1/3]: Press E at the Quests & Dialogue station         │
│   [ ] Lesson Reader              [0/3]                       │
│   [ ] Code Explorer: Combat      [0/1]                       │
│   [ ] Code Explorer: Rendering   [0/1]                       │
│   [ ] Combo Challenger           [0/1]                       │
│   Activities: 0/4 complete                                   │
│   Last lesson: Action Combat — ComboSystem FSM & Damage…     │
└──────────────────────────────────────────────────────────────┘
```

- Gold text = active main quest objective
- Green text = completed item/activity
- Grey text = in-progress activity
- Dim text = **Last lesson** hint (shows the title of the most recently read lesson for teaching continuity)

### Lesson Panel

When E is pressed at a station the lesson panel opens in the centre of the
screen and shows:

```
┌──────────────────────────────────────────────────────────────────┐
│  PBR Rendering — Cook-Torrance BRDF                              │
│                                                                  │
│  This station demonstrates Physically Based Rendering (PBR).    │
│  The engine uses the Cook-Torrance BRDF with three terms:        │
│    • GGX Normal Distribution Function …                          │
│    • Smith Geometry Function …                                   │
│    • Schlick Fresnel …                                           │
│                                                                  │
│  Key files / classes to inspect:                                 │
│    + shaders/pbr_mesh.vs.hlsl / pbr_mesh.ps.hlsl                │
│    + shaders/pbr_ibl.vs.hlsl / pbr_ibl.ps.hlsl                  │
│    + src/engine/rendering/d3d11/D3D11Renderer.cpp               │
│                                                                  │
│  Press ESC or ENTER to close                                     │
└──────────────────────────────────────────────────────────────────┘
```

When the station has a **combo challenge** defined, an additional prompt appears:

```
│  [Space] Start Combo Challenge  |  ESC or ENTER to close        │
```

### Data Files

| File | Purpose | Schema |
|------|---------|--------|
| `Content/World/demo_activities.json` | Main quest + activities (loaded by `DemoQuestManager::TryLoadFromJSON`). Uses string `stationID` + `specificStationID` fields. | `shared/schemas/demo_activities.schema.json` |
| `Content/World/station_lessons.json` | Per-station lesson content (loaded by `OpenWorld::TryLoadLessonsFromJSON`). Fields: `lessonTitle`, `lessonText`, `codePointers`, `relatedStations`, optional `challengeTitle` + `challengeKeys`. | `shared/schemas/station_lessons.schema.json` |
| `Content/quest_bank.json` | QuestSystem definitions (IDs 10–13: Tour, Lesson Reader, Code Explorer activities). | `shared/schemas/quest_bank.schema.json` |

> **Note — Two quest systems:** `quest_bank.json` uses numeric `targetID` values (QuestSystem schema) while `demo_activities.json` uses string `stationID` values (DemoQuestManager). See `src/demo_game/demo_quest_manager.hpp` for the dual-system design rationale.

### Controls for Quest Activities

| Key | Action |
|-----|--------|
| **E** | Interact at nearest teaching station (advances quest, opens lesson panel) |
| **Space** | Start combo challenge (when lesson panel is open at a challenge station) |
| **[1] [2] [3]** | Combo challenge inputs (while challenge overlay is active) |
| **F1** | Open station selector overlay (navigation aid — does NOT advance quest) |
| **↑ / ↓** | Move selection in F1 overlay |
| **Enter** | Teleport to selected station (in F1 menu); dismiss lesson panel (in lesson panel) |
| **ESC** | Close F1 overlay; dismiss lesson panel; exit challenge |
| **D** | Toggle biome + FPS debug strip |

---

## Lesson Authoring Guide

Station lessons are data-driven: you can add or update them without recompiling.

### JSON format (`station_lessons.json`)

```json
{
  "$schema": "../../shared/schemas/station_lessons.schema.json",
  "version": "1.1.0",
  "lessons": [
    {
      "stationID":   "my_station_id",
      "lessonTitle": "Short panel heading",
      "lessonText":  "Multi-line explanation.\nUse \\n for line breaks.\nBe concrete and concise.",
      "codePointers": [
        "src/engine/my_subsystem/my_class.hpp",
        "shaders/my_shader.hlsl  (key function: MyFunction)"
      ],
      "relatedStations": ["other_station_id"]
    }
  ]
}
```

### Adding a combo challenge to a lesson

Add `challengeTitle` and `challengeKeys` to the lesson entry:

```json
{
  "stationID":     "my_station_id",
  "lessonTitle":   "My Subsystem — Key Concepts",
  "lessonText":    "... explanation ...",
  "codePointers":  ["src/engine/my_subsystem/my_class.hpp"],
  "challengeTitle": "Challenge — Name three FSM states",
  "challengeKeys":  ["1", "2", "3"]
}
```

- `challengeTitle`: heading shown at the top of the challenge overlay.
- `challengeKeys`: ordered sequence of single character keys the player must
  press in order.  Currently **[1], [2], [3]** are the recognized inputs.
  Keep the list to 2–4 keys for a smooth classroom demo.

When both fields are present, the lesson panel shows:

```
│  [Space] Start Combo Challenge  |  ESC or ENTER to close        │
```

And pressing **Space** transitions to the challenge overlay.

### Authoring tips

1. **lessonText** — aim for 5–10 lines max.  Use bullet points (•) for lists.
   Separate concerns with blank lines (`\n\n`).
2. **codePointers** — list file paths relative to the repo root.  Optionally
   append `  (Class::Method)` to point at specific symbols.
3. **relatedStations** — helps students navigate between related topics.
   Not currently rendered but reserved for future cross-link UI.
4. **Safe fallback** — if `station_lessons.json` is missing or malformed, the
   game shows a stub message and continues normally.  No crash.
5. **$schema** — keep the `"$schema"` field pointing at
   `shared/schemas/station_lessons.schema.json` so editors like VS Code can
   validate your JSON in-place.

### Extending activities in `demo_activities.json`

To add a new activity, append to the `"activities"` array:

```json
{
  "id":               "my_new_activity",
  "title":            "My Activity",
  "description":      "Complete this optional teaching task.",
  "type":             "combo_practice",
  "specificStationID": "my_station_id",
  "required":          1
}
```

Valid `"type"` values:

| Type | Triggered by |
|------|-------------|
| `station_interact` | Pressing E at a station (`NotifyStationVisited`) |
| `combo_practice`   | Completing the combo challenge (`NotifyChallengePassed`) |
| `combat_challenge` | Enemy kill (`NotifyEnemyKilled`) — future use |
| `item_collection`  | Item pickup (`NotifyItemCollected`) — future use |

After adding a new activity, also update `DemoQuestManager::kExpectedActivities`
in `src/demo_game/demo_quest_manager.hpp` and add the matching C++ default in
`RegisterDefaults()` so the headless tests still pass.

### Extending stations in C++ (no JSON required)

If `ENGINE_ENABLE_JSON` is OFF, edit `OpenWorld::RegisterDefaultStations()` in
`src/demo_game/open_world.cpp` and add lesson entries programmatically:

```cpp
StationLesson l;
l.stationID       = "my_station_id";
l.lessonTitle     = "My Lesson";
l.lessonText      = "My explanation.\nSecond line.";
l.codePointers.push_back("src/engine/my_module/my_file.hpp");
// Optional challenge:
l.challengeTitle  = "Practice the FSM";
l.challengeKeys   = {"1", "2", "3"};
m_lessons[l.stationID] = l;
```

---

## F1 Debug / Teaching Station Menu

Press **F1** at any time while in the **PLAYING** state to open the
developer station overlay.

### What the overlay shows

- A panel listing every teaching station registered in `OpenWorld::GetStations()`,
  with its `displayName` and `sceneHint`.
- The currently selected station is highlighted in blue.
- An optional debug-info strip (toggled with **D**) showing the current biome
  name and live FPS.

### Controls

| Key | Action |
|-----|--------|
| **F1** | Open overlay (from PLAYING state) |
| **↑ / ↓** | Move station selection |
| **Enter** | Teleport to selected station (`OpenWorld::TeleportToStation`) |
| **F1 / ESC** | Close the overlay |
| **D** | Toggle biome-name + FPS debug strip (works outside F1 overlay too) |

### Implementation notes

The overlay is implemented using Windows **GDI** (`GetDC` / `DrawText` /
`ReleaseDC`) drawn after the D3D11 `DrawFrame` call.  In windowed D3D11 mode
the Desktop Window Manager (DWM) composites both surfaces, so the text appears
on top of the rendered scene without requiring an extra shader or ImGui
dependency.

> **For shipping games**: replace GDI with the engine's SDF `FontRenderer`
> (`src/engine/ui/font_renderer.hpp`) for GPU-accelerated anti-aliased text.

---

## Extending the Demo

### Add a new teaching station

1. Edit `samples/vertical_slice_project/Content/World/teaching_stations.json`
   — add a new entry following the existing format.  If `ENGINE_ENABLE_JSON`
   is active the runtime picks it up automatically on next boot.
2. If the JSON runtime path is not active (no vcpkg / ENGINE_ENABLE_JSON),
   also append a matching entry to `RegisterDefaultStations()` in
   `src/demo_game/open_world.cpp`.
3. Rebuild `demo_game`.

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

Edit `samples/vertical_slice_project/Content/quest_bank.json`.
Add a new quest entry following the schema (see `shared/schemas/quest_bank.schema.json`).
The `QuestSystem` (M8/M20) will automatically pick it up via `GameRuntime::Init()`.

To wire it into the `DemoQuestManager` quest HUD:
1. Add a new main quest objective in `demo_activities.json` (or extend an
   existing one), **or**
2. Add a new side activity entry to `demo_activities.json` with the appropriate
   `type` (`station_interact`, `combo_practice`, `combat_challenge`, or
   `item_collection`).
3. The built-in C++ defaults in `DemoQuestManager::RegisterDefaults()` remain
   as the fallback; update them if you want the quest visible in headless mode.
4. Update `DemoQuestManager::kExpectedActivities` and the headless test count.

---

## CI Integration

The `demo_world` scene is validated in every CI run as part of the primary
`build-windows` job:

```yaml
- name: Run Demo_Game acceptance test (demo_world — OpenWorld state machine)
  run: .\build\windows-ninja-debug-engine-only\engine_sandbox.exe --headless --scene demo_world
```

A dedicated `build-windows-demo` CI job builds `demo_game.exe` and runs its
headless path (9 acceptance tests):

```yaml
- name: Run Demo_Game headless acceptance test (demo_main path)
  run: .\build\windows-ninja-debug-demo\demo_game.exe --headless
```

The 9 acceptance tests are:

| Test | Name | What is verified |
|------|------|-----------------|
| 1 | `init` | `OpenWorld::Init()` returns true; ≥ 12 stations + lessons registered |
| 2 | `boot_menu` | Initial state is `BOOT_MENU` |
| 3 | `biome_cycle` | All 5 biomes visited before `IsHeadlessDone()` |
| 4 | `stations` | Every station has non-empty id, displayName, sceneHint |
| 5 | `teleport` | `TeleportToStation("rendering_pbr")` sets biome=GRASSLAND + nearestStationID (navigation only) |
| 6 | `json_fallback` | Missing JSON file leaves station list intact |
| 7 | `quest_manager_interact` | `DemoQuestManager`: 5 defined (1 main + 4 activities); quest completes on Teleport+Interact; Lesson Reader advances on unique interacts |
| 8 | `lesson_data` | Combat station lesson has non-empty text + code pointers + a combo challenge defined (soft-OK when JSON absent) |
| 9 | `combo_challenge` | Deterministic simulation: `TeleportToStation + InteractAtStation + NotifyChallengePassed` → `COMBO_PRACTICE` activity complete; `GetLastLessonTitle()` non-empty |

The same tests 8 and 9 are also run as part of `--scene demo_world` in the
`engine_sandbox` (`build-windows` CI job).

---

## Milestones Checklist

| Milestone | Description | Status |
|-----------|-------------|--------|
| **M-DG1** | `demo_game` executable + CMake target + `BUILD_DEMO_GAME` option | ✅ Done |
| **M-DG2** | `OpenWorld` state machine (BOOT_MENU → LOADING → PLAYING) | ✅ Done |
| **M-DG3** | Biome definitions JSON + teaching station definitions JSON | ✅ Done |
| **M-DG4** | `--scene demo_world` headless CI test (acceptance tests) | ✅ Done |
| **M-DG5** | `build-windows-demo` CI job (builds + validates `demo_game.exe`) | ✅ Done |
| **M-DG6** | Boot menu UI (keyboard navigation, New Game / Quit) | ✅ Done (GDI overlay) |
| **M-DG7** | F1 debug overlay (station list, teleport, debug info toggle) | ✅ Done (GDI overlay) |
| **M-DG8** | Data-driven station loading from `teaching_stations.json` | ✅ Done (`TryLoadStationsFromJSON`, ENGINE_ENABLE_JSON gated) |
| **M-DG9** | Quest & activity skeleton (tour main quest + 3 teaching activities + interact-based HUD + lesson panel) | ✅ Done (interact-first redesign: E key, lesson panel, station_lessons.json) |
| **M-DG10-ext** | Combo-practice mini-game, 4th activity (Combo Challenger), schema files, HUD last-lesson hint, headless tests 8–9 | ✅ Done |
| **M-DG11**| Full open-world biome traversal with real streaming cells | ⬜ Pending content population |
| **M-DG11**| NPC vendors, interactable camp sites, extended activities | ⬜ Pending |
| **M-DG12**| Settings menu (resolution, volume, key bindings) | ⬜ Pending |
| **M-DG13**| Performance / LOD pass (Intel HD 4000 baseline) | ⬜ Pending |

---

## See Also

- `src/demo_game/open_world.hpp/.cpp` — OpenWorld state machine + JSON loading + InteractAtStation
- `src/demo_game/demo_quest_manager.hpp/.cpp` — DemoQuestManager (M-DG9)
- `src/demo_game/demo_main.cpp` — Standalone entry point, GDI overlays (lesson panel, interact prompt)
- `src/sandbox/game_runtime.hpp/.cpp` — Reused M8 gameplay systems
- `samples/vertical_slice_project/Content/World/` — Content JSON files
- `samples/vertical_slice_project/Content/World/station_lessons.json` — Per-station lesson content (+ challenge definitions)
- `samples/vertical_slice_project/Content/World/demo_activities.json` — Quest + activity authoring (+ combo_practice type)
- `shared/schemas/station_lessons.schema.json` — JSON Schema for station lessons (draft-07)
- `shared/schemas/demo_activities.schema.json` — JSON Schema for demo activities (draft-07)
- `samples/vertical_slice_project/Content/quest_bank.json` — Quest definitions (IDs 1–13)
- `docs/FF15_REQUIREMENTS_BLUEPRINT.md` — Full requirements reference

