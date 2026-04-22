# ARCHITECTURE — Game Engine for Teaching

> This document explains the end-to-end pipeline: how authoring tools feed data
> into the engine, how the engine's internal systems relate to each other, and
> where each piece of code lives.  Study this before reading individual source files.
>
> **Status note (2026-04-22):** This file was reconciled against
> `docs/ASSESSMENT_2026-04-22.md`.

---

## Big Picture

```
┌─────────────────────────────────────────────────────────┐
│                  AUTHORING (tools)                       │
│                                                          │
│  Creation Suite Editor (Dear ImGui + D3D11)              │
│  ├─ EditorApp         ← DockSpace, menu bar, status bar  │
│  ├─ ContentBrowser    ← shows Content/ raw assets        │
│  ├─ SceneEditor       ← 2D canvas; entity placement      │
│  ├─ SceneHierarchy    ← entity list; add/rename/delete   │  ← M6
│  ├─ InspectorPanel    ← component property editor        │  ← M6
│  └─ SceneSerialiser   ← JSON ↔ ECS World round-trip      │  ← M6
│                                                          │
│  tools/audio_authoring/ (Python)                         │
│  ├─ AudioEngine.generate_track()                         │
│  └─ exports  →  Cooked/Audio/*.bank                      │
│                                                          │
│  tools/anim_authoring/ (Python)                          │
│  ├─ animation_engine.io.Exporter                         │
│  └─ exports  →  Cooked/Anim/*.skelc, *.animc             │
│                                                          │
│  tools/creation_engine.py (Python) — asset manifest     │
│  src/tools/cook/cook_main.cpp — cook.exe                 │
│  src/tools/pak/pak_main.cpp  — pak.exe (PAK1)   ← M15   │
└─────────────────────────────────────────────────────────┘
           │                    │
           ▼  cook step         ▼  cook step
┌─────────────────────────────────────────────────────────┐
│                  COOKED DATA                             │
│                                                          │
│  Cooked/                                                 │
│  ├─ Audio/*.bank          (packed audio clips)           │
│  ├─ Anim/*.skelc, *.animc (cooked skeleton + clips)      │
│  ├─ Textures/*.tex        (compressed textures)          │
│  └─ Maps/*.scene.json     (entity data, loaded by Zone)  │
│                                                          │
│  AssetRegistry.json  ← updated by every cook run        │
└─────────────────────────────────────────────────────────┘
           │
           ▼  engine loads from Cooked/
┌─────────────────────────────────────────────────────────┐
│                  RUNTIME ENGINE (C++17)                  │
│                                                          │
│  main()                                                  │
│  └─ engine_sandbox (D3D11 Windows)                       │
│      ├─ World (ECS)                                      │
│      │   ├─ EntityManager                                │
│      │   └─ ComponentPool<T>[N]                          │
│      ├─ D3D11Renderer        ← IRenderer interface       │
│      │   ├─ PBR pipeline     ← M9: Cook-Torrance BRDF    │
│      │   ├─ SkyRenderer      ← M10: time-of-day sky      │
│      │   ├─ WeatherFx        ← M10: fog/rain/cloud       │
│      │   ├─ FontRenderer     ← Post-M10: SDF text        │
│      │   └─ [depth buffer ✅; IBL ✅; shadows ✅; bloom ✅]│
│      ├─ GameRuntime (M8)     ← drives all gameplay       │
│      │   ├─ CombatSystem     ← ATB + damage + combos     │
│      │   ├─ AISystem         ← FSM + A* pathfinding      │
│      │   ├─ WeatherSystem    ← day/night cycle           │
│      │   ├─ QuestSystem      ← objective tracking        │
│      │   ├─ DialogueSystem   ← NPC interactions (M8.6)   │
│      │   ├─ InputMapper      ← keyboard → ECS (M8.2)     │
│      │   ├─ CameraSystem     ← third-person orbit (M8.3) │
│      │   ├─ SaveSystem       ← 15 slots + auto-save      │
│      │   └─ GameStreamingMgr ← live cell streaming (M8.7)│
│      ├─ AnimationSystem      ← M4 CPU + GPU skinning     │
│      ├─ PhysicsWorld         ← M5 Jolt Physics           │
│      ├─ AudioSystem          ← M3 + M18 (X3DAudio 3D)    │
│      ├─ VehicleSystem        ← M11 wheel-ray physics     │
│      ├─ BehaviourTree        ← M12 BtTree/Seq/Sel        │
│      ├─ FormationSystem      ← M12 LINE/V/CIRCLE         │
│      ├─ NavMesh (grid A*)    ← M12 pathfinding           │
│      ├─ CinematicSequencer   ← M13 shot/cut runtime      │
│      ├─ CameraRig            ← M13 keyframe Lerp          │
│      ├─ Hud + MenuStack      ← M8.5 + Post-M10           │
│      ├─ LuaEngine            ← Lua 5.4 scripting         │
│      └─ EventBus<T>          ← Combat/Quest/World/UI     │
└─────────────────────────────────────────────────────────┘
```

---

## Folder Responsibilities

| Folder | Language | Responsibility |
|--------|----------|----------------|
| `src/` | C++17 | Engine runtime + game systems |
| `engine/` | — | Engine README and documentation |
| `editor/` | C++17 / Dear ImGui | Creation Suite editor app |
| `tools/audio_authoring/` | Python 3.9+ | Audio synthesis + bank cooking |
| `tools/anim_authoring/` | Python 3.9+ | Skeletal animation authoring + cooking |
| `shared/schemas/` | JSON Schema | Canonical data format definitions |
| `shared/runtime/` | C++17 headers | Cross-project utilities (Guid, VersionedFile) |
| `samples/vertical_slice_project/` | Mixed | End-to-end sample proving the pipeline |
| `scripts/` | Lua 5.4 | Gameplay scripts (hot-reloadable) |
| `assets/` | JSON | Asset manifests and existing schemas |
| `docs/` | Markdown | Architecture, roadmap, feature docs |

---

## Engine Subsystems (C++)

### ECS — Entity Component System (`src/engine/ecs/ECS.hpp`)

```
World
 ├─ EntityManager    — free-list of EntityIDs (uint32_t)
 │                     living-set bitset
 │                     signature bitset per entity
 └─ ComponentPool<T> — one per component type (max 64 types)
     ├─ sparse[] — maps EntityID → dense index
     └─ dense[]  — contiguous component storage (cache-friendly)

SystemBase — Update(float dt) iterates all entities with matching signature
```

**Why ECS?** Data-oriented design: components are packed in arrays, so the CPU
cache is hot when a system iterates them.  Unreal uses UActorComponent/UObject;
Unity uses ECS (DOTS) and MonoBehaviour.  Both are ECS variants.

### EventBus (`src/engine/core/EventBus.hpp`)

Typed publish-subscribe bus.  Four buses:
- `CombatEvent` — hit, kill, status effect
- `QuestEvent` — start, objective, complete
- `WorldEvent` — zone load, weather change
- `UIEvent` — show notification, open menu

**Why EventBus?** Decouples systems.  CombatSystem doesn't need a pointer to
QuestSystem; it just emits a CombatEvent and QuestSystem subscribes.

### Renderer

- **Windows (default):** D3D11 (`src/engine/rendering/d3d11/D3D11Renderer.hpp/.cpp`).
  Win32 window + D3D11 device + swapchain + per-frame clear.  Runs on GT610-era GPUs.
  Uses D3D11 WARP (CPU software rasteriser) in headless CI mode — no GPU driver needed.
  **Active scenes:** `textured_quad`, `skinned_mesh`, `pbr_mesh` (M9 Cook-Torrance BRDF),
  `dynamic_sky` (M10), `vehicle_test` (M11), `bt_test` (M12), `cinematic_test` (M13),
  `font_test` (M15), `menu_stack_test` (Post-M10), `m8_gameplay`, `m8_streaming`,
  `streaming_load`, `streaming_evict`, `streaming_async`, `physics_test`.
  > **Rendering status note:** D3D11 depth buffer, IBL, directional shadow maps, and bloom
  > are all implemented and CI-covered (`pbr_ibl`, `shadow_test`, `bloom_test` scenes).
  > The dominant remaining rendering gap is runtime ingestion of authored mesh/material
  > content from sample cooked assets into the D3D11 draw path.
- **Windows (optional):** Vulkan (`src/engine/rendering/vulkan/VulkanRenderer.hpp/.cpp`).
  High-end modern API; requires Vulkan SDK.  Select with `--renderer vulkan`.
- **Linux:** ncurses ASCII renderer (`src/engine/rendering/Renderer.hpp`).

Both Windows backends implement `IRenderer` (`src/engine/rendering/IRenderer.hpp`).
`RendererFactory` (`src/engine/rendering/RendererFactory.hpp`) selects the backend via
the `--renderer d3d11|vulkan` runtime flag.

### SkyRenderer + WeatherFx (M10)

`src/engine/rendering/sky_renderer.hpp/.cpp` — procedural time-of-day sky:
- Drives a 24-hour clock (60× compressed for demo use).
- Computes sun direction from `sin/cos` on a shifted arc.
- Outputs `SkyShaderConstants` (zenith colour, horizon colour, sun direction, fog, time).
- Delegates weather state to `WeatherFx`.

`src/engine/rendering/weather_fx.hpp/.cpp` — weather effects:
- `WeatherType` enum: `Clear / Cloudy / Rain / Storm`.
- Smooth lerp of fog density, rain intensity, and cloud cover across transitions.
- `WeatherFxState` struct maps directly to the sky PS cbuffer fog/rain fields.

### LuaEngine (`src/engine/scripting/LuaEngine.hpp`)

Embeds Lua 5.4.  C++ functions are registered as Lua globals:
`engine_log`, `game_get_player_hp`, `game_heal_player`, `game_add_item`, …
Scripts in `scripts/` define hook functions called by C++:
`on_combat_start`, `on_camp_rest`, `on_level_up`, `on_explore_update`.

---

### Post-M10 Subsystems (all ✅)

#### Vehicle Physics (M11)

`src/engine/vehicle/vehicle_system.hpp/.cpp` — `VehicleSystem`:
- Per-wheel ray cast suspension; spring-damper forces; Ackermann steering correction.
- `VehicleComponent` with `WheelState`×4 (spring compression, damping, contact point).
- Fuel drain; velocity damping; ECS integration.
- Vehicle chase camera wired in `src/engine/rendering/camera_system.hpp/.cpp`.

#### Behaviour Tree AI (M12)

`src/engine/ai/behaviour_tree.hpp/.cpp`:
- `BtBlackboard` — type-erased key/value store for inter-node communication.
- `BtNode` base; `BtSequence` (AND); `BtSelector` (OR); `BtCondition`; `BtAction`.
- `BtTree` — owns the root node, drives `Tick(blackboard)` each frame.

`src/engine/ai/formation_system.hpp/.cpp` — `FormationSystem`:
- Supports LINE, V_SHAPE, CIRCLE formation types.
- `AssignFormation(entityIDs, leader, type)` → greedy slot assignment.
- `GetWorldOffset(slot, leaderTransform)` → world-space position per member.

`src/engine/ai/nav_mesh.hpp/.cpp` — grid `NavMesh`:
- `BakeFromGrid(walkabilityGrid, width, height)` / `BakeEmpty(w, h)` / `SetWalkable(x, y, v)`.
- `FindPath(start, goal)` → `std::vector<Vec3>`; A\* with 4-dir + diagonal movement; obstacle routing.

#### Cinematics (M13/M22 — runtime + tooling ✅)

`src/engine/cinematics/camera_rig.hpp/.cpp` — `CameraRig`:
- Stores a sorted list of `{time, eyePos, lookAtPos}` keyframes.
- `Evaluate(t)` — binary-search bracket + linear Lerp for smooth interpolation.
- `Duration()`, `KeyframeCount()`.

`src/engine/cinematics/cinematic_sequencer.hpp/.cpp` — `CinematicSequencer`:
- Owns a sequence of shots, each referencing a `CameraRig` + optional audio event.
- `Play()` / `Stop()` / `Tick(dt)` — advances time across shot boundaries.
- `ApplyToCamera(world, entityID)` — writes `cinematicEyePos` / `cinematicLookAt` into the
  entity's `CameraComponent`; sets `cinematicOverride = true` so `CameraSystem` bypasses orbit math.
- `OnShotChanged` and `OnComplete` callbacks for game-layer notification.

ECS integration: `CameraComponent` has `cinematicOverride` (bool), `cinematicEyePos` (Vec3),
`cinematicLookAt` (Vec3) fields.  `CameraSystem::Update` checks `cinematicOverride` first.

#### UI Systems (Post-M10)

`src/engine/ui/menu_stack.hpp/.cpp` — `MenuStack`:
- Push/pop/PopToBase navigation; `Contains(screen)` predicate; `OnScreenChanged` callback.
- `MenuScreen` enum: 9 screen types (NONE/MAIN/INVENTORY/EQUIPMENT/MAP/QUEST/SHOP/CAMP/PAUSE).

`src/engine/ui/font_renderer.hpp/.cpp` — `FontRenderer`:
- Generates an 8×8 bitmap SDF atlas covering 96 printable ASCII glyphs at construction time.
- Uploads as `DXGI_FORMAT_R8_UNORM` D3D11 texture (single-channel, mipmapped).
- `RenderText(device, context, text, x, y, scale, rgba)` — builds a dynamic VB of quads (6 verts each).
- Uses `shaders/sdf_text.vs.hlsl` + `shaders/sdf_text.ps.hlsl` (SM 4.0).
- Alpha-blending state (`D3D11_BLEND_SRC_ALPHA` / `D3D11_BLEND_INV_SRC_ALPHA`).

#### PAK Packager (M15)

`src/tools/pak/pak_main.cpp` — `pak.exe`:
- PAK1 binary format: 4-byte magic `PAK1` + 4-byte entry count + per-entry (32-byte path hash, 8-byte offset, 8-byte size) TOC + blobs.
- `--input <dir> --output <file>` — packs an entire directory tree.
- `--list <pakfile>` — lists all entries with path + size.
- `--extract <pakfile> --output <dir>` — extracts with path traversal protection.

#### Save System (M8.8)

`src/engine/save/save_system.hpp/.cpp` — `SaveSystem`:
- 15 numbered save slots + auto-save slot; JSON ECS snapshot format.
- Serialises all ECS component state per-entity via `SceneSerialiser`.
- `"version"` field supports forward migration for future schema changes.
- Wired to `CampSystem` auto-save hook in `GameRuntime`.

## Dear ImGui Editor Architecture

> **Note:** Qt6 was replaced by Dear ImGui (MIT license) as the editor framework.
> Dear ImGui is the industry-standard immediate-mode GUI used by game engine tooling
> at virtually every AAA studio (Unity, Unreal, Godot, and most in-house engines).

```
Win32 + D3D11 host window
└─ EditorApp::Run() — ImGui DockSpace, menu bar, status bar
    ├─ ContentBrowserPanel
    │   Uses std::filesystem to browse Content/ tree.
    │   Signals: double-click opens asset; **drag file → CONTENT_ASSET payload**.
    ├─ SceneEditorPanel (ImGui DrawList canvas)
    │   Draws: grid, entity boxes.
    │   Handles: left-click = place entity, Delete = remove.
    │   **Drop target: CONTENT_ASSET → create entity with pre-filled component.**
    │   saveScene() → JSON via nlohmann/json.
    │   loadScene() ← JSON via nlohmann/json.
    ├─ SceneHierarchyPanel  (M6 ✅)
    │   Entity list with add/rename/duplicate/delete + context menu.
    └─ InspectorPanel       (M6 ✅)
        Table-driven component property editor (DragFloat/DragInt/Checkbox/InputText).
        Add Component popup lists all known component types.
```

**How to add a new editor panel:**
1. Create `editor/src/panels/MyPanel.hpp` / `.cpp`.
2. Inherit from no base class — panels are plain C++ objects with a `Draw()` method.
3. In `EditorApp::Render()`, call `MyPanel::Draw()` inside an `ImGui::Begin()`/`End()` block.
4. Add the `.cpp` to `EDITOR_SOURCES` in `editor/CMakeLists.txt`.

---

## Audio Pipeline

```
tools/audio_authoring/audio_engine/
├─ engine.py     — AudioEngine façade
├─ ai/           — MusicGen, SFXGen, VoiceGen (prompt → audio)
├─ composer/     — Sequencer + Note (procedural)
├─ synthesizer/  — InstrumentLibrary (wavetable synth)
├─ dsp/          — filters, reverb, compressor
├─ export/       — AudioExporter (WAV, OGG)
└─ qa/           — LoudnessMeter, ClippingDetector

Cook step (stub, see samples/vertical_slice_project/cook_assets.py):
  AudioEngine.generate_track("battle") → "Cooked/Audio/battle.bank"

Engine loads:  Cooked/Audio/*.bank → XAudio2Backend + AudioSystem (C++, ✅ implemented in M3)
```

**Schema:** `shared/schemas/audio_bank.schema.json`

---

## Animation Pipeline

```
tools/anim_authoring/animation_engine/
├─ model/       — Vertex, Mesh, Material, Bone, Skeleton, Model
├─ animation/   — Keyframe, Channel, Clip, BlendTree, IKSolver
├─ io/          — Exporter / Importer (.anim JSON and GLTF 2.0)
├─ runtime/     — Animator (evaluates clip at time t), CPU skinning
├─ math_utils/  — Vector, Quaternion, Matrix, Transform
└─ editor/      — Tkinter timeline editor (standalone tool)

Cook step:
  animation_engine.io.Exporter.export_clip(clip, "Cooked/Anim/Walk.animc")
  animation_engine.io.Exporter.export_skeleton(skel, "Cooked/Anim/Human.skelc")

Engine loads:  Cooked/Anim/*.skelc + *.animc → AnimationSystem (C++, ✅ M4 complete)
```

**Schemas:** `shared/schemas/skeleton.schema.json`, `anim_clip.schema.json`,
             `anim_graph.schema.json`

---

## Data Format Versioning

Every shared file has:
```json
{
  "$schema": "../../shared/schemas/scene.schema.json",
  "version": "1.0.0",
  ...
}
```

When you change a format:
- **PATCH** bump: bug fix, no consumer changes needed.
- **MINOR** bump: added optional field, old readers ignore it safely.
- **MAJOR** bump: breaking change; add a migration path in `shared/runtime/VersionedFile.hpp`.

---

## Vertical Slice Pipeline (end-to-end)

```
samples/vertical_slice_project/
├─ Project.json             ← project descriptor
├─ AssetRegistry.json       ← updated by cook step; 5 entries (1 scene + 4 level cells)
├─ Content/                 ← raw source assets (committed)
│   ├─ Textures/            ← .png placeholder textures
│   ├─ Audio/               ← .wav source audio
│   ├─ Maps/                ← .scene.json maps
│   ├─ Animations/          ← .json source animation data
│   └─ Levels/              ← .cell.json streaming cells (cell_0_0 … cell_1_1)
├─ Cooked/                  ← generated at cook time (gitignored)
└─ cook_assets.py           ← one-command cook script

Run: cd samples/vertical_slice_project && python cook_assets.py
```

The cook script:
1. Reads `Project.json` → knows source + cooked paths.
2. Runs audio authoring → writes `Cooked/Audio/*.bank`.
3. Runs animation authoring → writes `Cooked/Anim/*.skelc`, `*.animc`.
4. Updates `AssetRegistry.json`.
5. Prints a summary of cooked files.

---

## Learning Path

Study files in this order:

1. `shared/schemas/*.schema.json` — understand the data model
2. `src/engine/core/Types.hpp` — vocabulary types
3. `src/engine/core/Logger.hpp` — singleton, thread safety, macros
4. `src/engine/ecs/ECS.hpp §1-4` — ECS basics (ComponentPool, sparse-set)
5. `src/engine/ecs/ECS.hpp §5-7` — World, SystemBase
6. `src/engine/core/EventBus.hpp` — Observer pattern, pub/sub
7. `src/game/GameData.hpp` — Flyweight pattern, static databases
8. `src/game/systems/CombatSystem.*` — ATB combat
9. `src/game/systems/AISystem.*` — FSM + A*
10. `src/engine/scripting/LuaEngine.*` — scripting integration
11. `editor/src/EditorApp.*` — Dear ImGui DockSpace, Win32+D3D11 host
12. `editor/src/ContentBrowserPanel.*` — std::filesystem tree browser + drag-drop source
13. `editor/src/SceneEditorPanel.*` — ImGui DrawList canvas, JSON I/O, drag-drop target
14. `editor/src/panels/SceneHierarchyPanel.*` — M6: entity list, context menus, rename
15. `editor/src/panels/InspectorPanel.*` — M6: table-driven property editor, Add Component
16. `src/engine/world/world_streaming.hpp` — M7: streaming manager + cell state machine
17. `src/engine/world/world_partition.hpp` — M7: spatial grid (CellCoord / CellId)
18. `src/engine/world/async_loader.hpp` — M7: worker-thread job queue + cancellation tokens
19. `src/game/world/GameStreamingManager.*` — M8.7: Zone lifecycle wired into streaming
20. `src/sandbox/game_runtime.*` — M8: D3D11 gameplay integration (all systems wired)
21. `shaders/pbr_mesh.vs.hlsl` + `pbr_mesh.ps.hlsl` — M9: Cook-Torrance BRDF (GGX NDF, Smith G, Schlick F)
22. `src/engine/rendering/sky_renderer.hpp/.cpp` — M10: time-of-day clock, sun direction, colour phases
23. `src/engine/rendering/weather_fx.hpp/.cpp` — M10: WeatherType FSM, fog/rain/cloud lerp transitions
24. `shaders/sky.vs.hlsl` + `sky.ps.hlsl` — M10: SV_VertexID full-screen triangle; exponential sky gradient
25. `tools/audio_authoring/audio_engine/engine.py` — Python façade pattern
26. `tools/anim_authoring/animation_engine/` — animation data model
27. `samples/vertical_slice_project/cook_assets.py` — scripted pipeline
