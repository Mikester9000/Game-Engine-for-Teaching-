# Copilot Continuation Instructions
# Game Engine for Teaching — FFXV-Style Action RPG Monorepo

## Overview
This is a **Windows-first teaching monorepo** that implements a complete game engine
toolchain inspired by Final Fantasy XV. Every piece of code is written to be read,
studied, and extended. Copilot continuations should follow these rules strictly.

---

## Current Development Status

> **IMPORTANT — Keep this section updated.**
> Every time a subsystem changes status (from ⬜ to 🔨, or 🔨 to ✅), update
> the entry here so the next Copilot session starts with an accurate picture.
> Status legend: ✅ complete · 🔨 in progress / partial · ⬜ not started

---

### Infrastructure & Tooling

| Area | Status | Notes |
|------|--------|-------|
| Monorepo folder layout | ✅ | All dirs: `src/`, `editor/`, `tools/`, `shared/`, `samples/`, `scripts/`, `shaders/`, `docs/` |
| Root `CMakeLists.txt` | ✅ | `ENGINE_ENABLE_D3D11` (default ON), `ENGINE_ENABLE_VULKAN` (optional), `ENGINE_ENABLE_TERMINAL`, `BUILD_EDITOR` options; `engine_sandbox` + `game` targets |
| `CMakePresets.json` | ✅ | `windows-debug`, `windows-debug-engine-only` (D3D11, no SDK required), `windows-debug-vulkan` presets defined |
| Shared JSON schemas (7 formats) | ✅ | `shared/schemas/`: project, asset_registry, scene, audio_bank, skeleton, anim_clip, anim_graph |
| Shared runtime headers | ✅ | `shared/runtime/`: `Guid.hpp`, `VersionedFile.hpp`, `Log.hpp` |
| CI — Linux build + Python tests | ✅ | `.github/workflows/build-linux.yml`: builds terminal game, runs 32+11 pytest |
| CI — asset manifest validation | ✅ | `.github/workflows/validate-assets.yml` |
| CI — Windows build + headless | ✅ | `.github/workflows/build-windows.yml`: MSVC x64 + D3D11 (no Vulkan SDK); builds `engine_sandbox` + `cook`; runs `--headless` (WARP) + `--validate-project`; optional Vulkan job |
| CI — contract / golden-file tests | ✅ | `.github/workflows/contract-tests.yml`: runs `cook`, diffs against `tests/golden/assetdb_expected.json`; pytest cook pipeline (13 tests); TEACHING NOTE audit |
| CI — Architecture Lint | ✅ | `.github/workflows/architecture-lint.yml`: runs `check_architecture.py` (file-size + layer rules) and `extract_teaching_notes.py`; fails if `CURRICULUM_INDEX.md` is stale |
| `vcpkg.json` | ✅ | Repo root; `nlohmann-json` dependency added for M2 cook pipeline |
| Vertical slice sample skeleton | ✅ | `samples/vertical_slice_project/`: `cook_assets.py` (stubs), `Project.json`, `AssetRegistry.json`, `Content/`, `Cooked/` |

---

### Rendering (D3D11 default + Vulkan optional, Windows)

| Area | Status | Notes |
|------|--------|-------|
| `IRenderer` abstract interface | ✅ | `src/engine/rendering/IRenderer.hpp` — backend-agnostic Init/DrawFrame/Shutdown |
| `RendererFactory` | ✅ | `src/engine/rendering/RendererFactory.hpp` — creates D3D11Renderer or VulkanRenderer by `--renderer` flag |
| D3D11 renderer (default) | ✅ | `src/engine/rendering/d3d11/D3D11Renderer.hpp/.cpp`; D3D_FEATURE_LEVEL_10_0 minimum; WARP headless mode; GT610-compatible |
| Win32 window + headless mode | ✅ | `src/engine/platform/win32/Win32Window.hpp/.cpp`; `--headless` arg in `src/sandbox/main.cpp` |
| Vulkan bootstrap (optional, M0) | ✅ | Instance, device, swapchain, render pass, framebuffers, command buffers, sync primitives |
| SPIR-V pipeline + colored triangle (M1, Vulkan) | ✅ | `VulkanPipeline`, `VulkanMesh`, `VulkanBuffer`; `shaders/triangle.vert/.frag` |
| D3D11 depth buffer | ⬜ | Needed for any 3D geometry |
| Vulkan depth buffer | ⬜ | Needed for any 3D geometry |
| D3D11 textures (DDS/BC7) | ⬜ | `src/engine/rendering/d3d11/d3d11_texture.hpp/.cpp` |
| Vulkan textures (DDS/BC7) | ⬜ | `src/engine/rendering/vulkan/vulkan_texture.hpp/.cpp` |
| Vulkan descriptor sets | ⬜ | `src/engine/rendering/vulkan/vulkan_descriptor.hpp/.cpp` |
| Textured quad scene | ⬜ | Shaders: `shaders/textured_quad.vert/.frag` |
| PBR shading (IBL + directional light) | ⬜ | `src/engine/rendering/vulkan/pbr_pipeline.hpp/.cpp` |
| Directional shadow map | ⬜ | Shadow pass render target + shadow sampling in PBR shader |
| Post-processing (bloom + tonemap) | ⬜ | Full-screen pass pipeline |
| Dynamic sky / procedural time-of-day | ⬜ | `src/engine/rendering/sky_renderer.hpp/.cpp` |
| Weather VFX (rain, fog) | ⬜ | `src/engine/rendering/weather_fx.hpp/.cpp` |
| GPU skinned mesh pass | ⬜ | `shaders/skinned_mesh.vert/.frag`; joint matrix UBO |
| Swapchain resize handling | ✅ | `VulkanRenderer::RecreateSwapchain()` |
| Headless frame recording | ✅ | `VulkanRenderer::RecordHeadlessFrame()` |

---

### Terminal Renderer (Linux/ncurses)

| Area | Status | Notes |
|------|--------|-------|
| `TerminalRenderer` (ncurses) | ✅ | `src/engine/rendering/Renderer.hpp/.cpp` |
| `InputSystem` (ncurses) | ✅ | `src/engine/input/InputSystem.hpp/.cpp` |

---

### Engine Core

| Area | Status | Notes |
|------|--------|-------|
| `EventBus<T>` | ✅ | `src/engine/core/EventBus.hpp` — type-safe pub/sub |
| `Logger` | ✅ | `src/engine/core/Logger.hpp/.cpp` — `LOG_INFO/WARN/ERROR/DEBUG` |
| `Types.hpp` | ✅ | All enums, typedefs, `EntityID`, `NULL_ENTITY`, `TileCoord`, etc. |
| `ECS.hpp` (~2000 lines) | ✅ | Full ECS: `World`, `ComponentStorage`, all components |
| `LuaEngine` (Lua 5.4) | ✅ | `src/engine/scripting/LuaEngine.hpp/.cpp`; `engine_log`/`game_log` registered |
| Lua scripts | ✅ | `scripts/main.lua`, `enemies.lua`, `quests.lua` |

---

### Gameplay Systems (Terminal Game)

All systems below run in the Linux ncurses terminal game (`src/game/`).
They must be re-wired to the Vulkan runtime at **Milestone M8**.

| System | Status | Notes |
|--------|--------|-------|
| `CombatSystem` | ✅ | ATB, warp-strike, link-strikes, elemental damage, status effects, crit, dodge, flee |
| `AISystem` | ✅ | FSM (IDLE/WANDERING/CHASING/ATTACKING/FLEEING/DEAD) + A* pathfinding on tile grid |
| `WeatherSystem` | ✅ | Day/night cycle (60× compression), probabilistic weather FSM, EventBus broadcast |
| `QuestSystem` | ✅ | Objective tracking, rewards, prerequisites |
| `InventorySystem` | ✅ | Items, equipment, stack management |
| `MagicSystem` | ✅ | MP-cost spells, elemental types |
| `ShopSystem` | ✅ | Buy/sell; currency via `CurrencyComponent::gil` |
| `CampSystem` | ✅ | Rest, HP/MP restore, triggers auto-save hook |
| `Zone` | ✅ | Zone lifecycle (Load/Unload/Update), spawn points, respawn timers |
| `TileMap` / `WorldMap` | ✅ | Tile-based 2D world |
| `Game` (main loop) | ✅ | All systems wired; game state machine; `ncurses` rendering |
| `SaveGame`/`LoadGame` | 🔨 | Implemented in `Game.cpp` (text key=value: HP/MP/Level/Gil + WorldMap tile data); wired to CampSystem auto-save; no `src/engine/save/` production system with versioning/migration yet |
| Dialogue system | ⬜ | `src/game/systems/dialogue_system.hpp/.cpp` — referenced but not created |
| Behaviour tree AI | ⬜ | `src/engine/ai/behaviour_tree.hpp/.cpp` — FSM exists; BT not implemented |
| Formation system | ⬜ | `src/engine/ai/formation_system.hpp/.cpp` |
| Vehicle physics | ⬜ | State enum only; `src/game/systems/vehicle_system.hpp/.cpp` not yet complete |

---

### Audio (C++ Runtime)

| Area | Status | Notes |
|------|--------|-------|
| Python `audio_authoring` tool | ✅ | `tools/audio_authoring/audio_engine/` — AI generation, DSP, OGG/WAV export (32 tests passing) |
| `audio_engine.py` CLI | ✅ | `tools/audio_engine.py` — register/emit/consume/list |
| XAudio2 backend (C++) | ⬜ | `src/engine/audio/xaudio2_backend.hpp/.cpp` |
| Audio system (C++) | ⬜ | `src/engine/audio/audio_system.hpp/.cpp` — event-driven play/stop, music layer FSM, 3D emitters |

**Next step:** Create `src/engine/audio/xaudio2_backend.hpp/.cpp` using Windows SDK XAudio2.
Device init → master voice → source voice pool → `Play(clipId)` / `Stop()`.
Follow the design in `docs/FF15_REQUIREMENTS_BLUEPRINT.md §8`.

---

### Animation (C++ Runtime)

| Area | Status | Notes |
|------|--------|-------|
| Python `anim_authoring` tool | ✅ | `tools/anim_authoring/animation_engine/` — skeleton, clips, IK, glTF, blend trees (11 tests passing) |
| Skeleton runtime (C++) | ⬜ | `src/engine/animation/skeleton.hpp/.cpp` |
| Anim clip evaluation (C++) | ⬜ | `src/engine/animation/anim_clip.hpp/.cpp` |
| Blend tree (C++) | ⬜ | `src/engine/animation/blend_tree.hpp/.cpp` |
| IK solver (C++) | ⬜ | `src/engine/animation/ik_solver.hpp/.cpp` |
| GPU skinning (C++) | ⬜ | `src/engine/animation/gpu_skinning.hpp/.cpp`; upload joint matrices to Vulkan UBO |
| Animation system (C++) | ⬜ | `src/engine/animation/animation_system.hpp/.cpp` — ECS update, advance time, evaluate, upload |

**Next step (M4):** Start with `skeleton.hpp/.cpp` (joint hierarchy, bind pose), then `anim_clip.hpp/.cpp`
(sampled keyframe evaluation). The Python tool's `animation_engine.model.Bone` and
`animation_engine.animation.Clip` are the reference data model.

---

### Physics

| Area | Status | Notes |
|------|--------|-------|
| Jolt Physics integration | ⬜ | Nothing exists in `src/engine/physics/` |
| `PhysicsWorld` wrapper | ⬜ | `src/engine/physics/physics_world.hpp/.cpp` |
| Rigid body ECS component | ⬜ | `src/engine/physics/rigid_body.hpp/.cpp` |
| Character capsule controller | ⬜ | `src/engine/physics/character_controller.hpp/.cpp` |
| Raycast interface | ⬜ | `src/engine/physics/raycast.hpp/.cpp` |
| Hit volumes (combat) | ⬜ | `src/engine/physics/hit_volume.hpp/.cpp` |

**Next step (M5):** Add Jolt Physics via vcpkg (`vcpkg install jolt-physics`).
Add `ENGINE_ENABLE_PHYSICS` CMake option. Create `physics_world.hpp/.cpp` wrapping
`JPH::PhysicsSystem`. See `docs/FF15_REQUIREMENTS_BLUEPRINT.md §10` for acceptance tests.

---

### Asset Pipeline (C++ runtime + cook tool)

| Area | Status | Notes |
|------|--------|-------|
| `tools/creation_engine.py` CLI | ✅ | Multi-type asset emit/consume |
| `tools/validate-assets.py` | ✅ | Schema validation against `assets/schema/asset-manifest.schema.json` |
| `cook_assets.py` (Python stubs) | 🔨 | Copies files; texture/audio/anim cook steps are stubs |
| `cook.exe` (C++ standalone cooker) | ✅ | `src/tools/cook/cook_main.cpp` (504 lines); reads `AssetRegistry.json`, copies source → `Cooked/`, writes `Cooked/assetdb.json`; uses nlohmann/json via vcpkg |
| `AssetDB` runtime (C++) | ✅ | `src/engine/assets/asset_db.hpp/.cpp` (268 lines); GUID → absolute cooked path; `Load()`, `GetCookedPath()`, `Has()`, `All()` |
| `AssetLoader` (C++) | ✅ | `src/engine/assets/asset_loader.hpp/.cpp`; synchronous `LoadRaw(id)` → `std::vector<uint8_t>`; async deferred to M7 |
| Golden files + contract tests | ✅ | `tests/golden/assetdb_expected.json`; `tests/cook/test_cook_pipeline.py` (13 pytest); `.github/workflows/contract-tests.yml` |

**M2 is complete.** `engine_sandbox --validate-project samples/vertical_slice_project` loads AssetDB and all cooked assets, exits 0.

**Next step (M3):** See M3 Bootstrap Guide below.

---

### World Streaming

| Area | Status | Notes |
|------|--------|-------|
| Tile-based `Zone` / `TileMap` | ✅ | `src/game/world/Zone.hpp/.cpp`, `TileMap.hpp/.cpp` |
| `WorldMap` (multi-zone container) | ✅ | `src/game/world/WorldMap.hpp/.cpp` |
| `world_streaming` (proximity-based) | ⬜ | `src/engine/world/world_streaming.hpp/.cpp` |
| `world_partition` (spatial grid) | ⬜ | `src/engine/world/world_partition.hpp/.cpp` |
| `async_loader` (worker thread) | ⬜ | `src/engine/world/async_loader.hpp/.cpp` |

**Next step (M7):** Create `src/engine/world/world_streaming.hpp/.cpp`. Cell manager
with a proximity-based load/evict loop. Integrate the existing `Zone` class as the
streaming cell type. Use `std::thread` + a lock-free queue for the async loader.

---

### Save System

| Area | Status | Notes |
|------|--------|-------|
| `SaveGame`/`LoadGame` (basic) | 🔨 | Implemented in `src/game/Game.cpp` — text key=value format (HP/MP/Level/Gil + WorldMap); called by CampSystem auto-save; no versioning or migration |
| `SaveSystem` / `SaveSchema` | ⬜ | `src/engine/save/save_system.hpp/.cpp`, `save_schema.hpp` — production system with 15 slots + auto-save + migration not yet started |

**Next step:** Create `src/engine/save/save_system.hpp/.cpp`. Serialize ECS `World` state
to JSON (all components per entity). Include a `"version"` field with migration support.
Acceptance: save → load → component data matches byte-for-byte.

---

### UI System (Vulkan HUD)

| Area | Status | Notes |
|------|--------|-------|
| Terminal UI (ncurses) | ✅ | Part of `TerminalRenderer` + `Game` render methods |
| Vulkan HUD / menu stack | ⬜ | `src/engine/ui/ui_system.hpp/.cpp`, `hud.hpp/.cpp`, `menu_stack.hpp/.cpp` |
| Font renderer | ⬜ | `src/engine/ui/font_renderer.hpp/.cpp` — SDF font atlas |

---

### Cinematics

| Area | Status | Notes |
|------|--------|-------|
| Cinematic sequencer | ⬜ | `src/engine/cinematics/cinematic_sequencer.hpp/.cpp` |
| Camera rig | ⬜ | `src/engine/cinematics/camera_rig.hpp/.cpp` |

---

### Editor (Qt 6)

| Area | Status | Notes |
|------|--------|-------|
| `MainWindow` (menus, toolbar, docks, status bar) | ✅ | `editor/src/MainWindow.hpp/.cpp` |
| `ContentBrowser` panel | ✅ | `editor/src/ContentBrowser.hpp/.cpp` — file tree |
| `SceneEditor` canvas | ✅ | `editor/src/SceneEditor.hpp/.cpp` — placeholder canvas |
| Entity inspector / property editor | ⬜ | `editor/src/panels/inspector.hpp/.cpp` |
| Scene hierarchy panel | ⬜ | `editor/src/panels/scene_hierarchy.hpp/.cpp` |
| Scene ECS serialization | ⬜ | `src/engine/scene/scene_serialiser.hpp/.cpp` — JSON ↔ ECS snapshot |
| Play-in-Engine button | ⬜ | Launch `engine_sandbox.exe` with current scene file |
| Asset drag-drop into scene | ⬜ | Drag from ContentBrowser → SceneEditor |

---

### Next Milestone — What to Work On Now

> **Current position: M2 complete (AssetDB + cook.exe + contract CI), M3 is next.**

Recommended implementation order to reach project completion:

| Priority | Milestone | Key deliverables |
|----------|-----------|-----------------|
| **1 — Now** | **M3: Texture + Audio** | Vulkan texture (DDS/BC7) via `directxtex`; `vulkan_texture.hpp/.cpp`; XAudio2 backend; `audio_system.hpp/.cpp`; textured quad renders; cooked audio plays; `AudioSourceComponent` added to ECS |
| **2** | **M4: Animation runtime** | C++ skeleton + clip evaluation + blend tree; GPU skinning UBO; `AnimatorComponent` added to ECS; animated character on screen |
| **3** | **M5: Physics** | Jolt Physics via vcpkg; character capsule falls + steps; raycasts work; `RigidBodyComponent` + `ColliderComponent` added to ECS |
| **4** | **M6: Editor** | Entity inspector; scene ECS serialization; Play-in-Engine |
| **5** | **M7: World streaming** | Async cell load/evict; no frame spikes during load |
| **6** | **M8: Gameplay integration** | All gameplay systems (combat, AI, quests, etc.) wired into Vulkan runtime |
| **7+** | **Post-M8** | Cinematics, vehicle physics, Vulkan HUD, PBR, dynamic sky, production save system (15 slots), PAK packager, behaviour tree, nav-mesh, dialogue |

---

## Repository Layout & Folder Responsibilities

```
Game-Engine-for-Teaching-/
├── engine/             # Engine subdirectory README; source lives in src/
├── src/                # C++17 engine & game source code (existing, working)
│   ├── engine/         # Platform-independent engine kernel
│   │   ├── core/       # Logger, EventBus, Types
│   │   ├── ecs/        # Entity-Component-System (ECS.hpp — 2 000 lines)
│   │   ├── input/      # Input system (ncurses on Linux)
│   │   ├── platform/   # Win32 window, message pump
│   │   ├── rendering/  # ncurses renderer + Vulkan renderer
│   │   └── scripting/  # Lua 5.4 embedding
│   ├── game/           # FFXV-style gameplay systems
│   │   ├── systems/    # Combat, AI, Camp, Inventory, Magic, Quest, Shop, Weather
│   │   └── world/      # TileMap, Zone, WorldMap
│   ├── sandbox/        # Windows Vulkan clear-screen demo
│   └── main.cpp        # Terminal game entry point
├── editor/             # Qt 6 Widgets editor (Creation Suite)
│   └── src/            # C++ Qt source code
├── tools/
│   ├── audio_authoring/    # Python audio authoring tool (from Audio-Engine)
│   └── anim_authoring/     # Python animation authoring tool (from Animation-Engine)
├── shared/
│   ├── schemas/        # JSON Schema (draft-07) for all shared data formats
│   └── runtime/        # Shared C++ header utilities (GUID, VersionedFile, Log)
├── samples/
│   └── vertical_slice_project/  # One end-to-end sample project
│       ├── Content/    # Raw source assets (textures, audio, maps, animations)
│       ├── Cooked/     # Runtime-ready cooked assets (generated by cook script)
│       └── Saved/      # Logs and editor state
├── assets/             # Asset manifests and JSON schemas (existing)
├── scripts/            # Lua 5.4 game scripts
└── docs/               # Architecture, roadmap, and feature docs
```

---

## Coding Conventions

### C++ (Engine + Editor)
- **Standard**: C++17 minimum. Use `if constexpr`, structured bindings, `std::optional`.
- **Naming**: `PascalCase` for types/classes, `camelCase` for local variables,
  `UPPER_SNAKE` for macros, `snake_case` for file names.
- **Comments**: Every non-obvious decision MUST have a `// TEACHING NOTE —` comment
  explaining *why*, not just *what*.
- **Headers**: Use `#pragma once`. Keep implementation out of headers unless template/inline.
- **RAII**: Prefer RAII for resource management. No raw `new`/`delete` in new code.
- **Errors**: Use `LOG_ERROR` + return-code or exceptions consistently per subsystem.
- **Windows**: All new C++ targets must compile clean with MSVC `/W4` on Windows.

### Python (Authoring Tools)
- **Version**: Python 3.9+.
- **Style**: Follow PEP 8. Type hints on all public APIs.
- **Docstrings**: NumPy-style docstrings on all public classes and functions.
- **Testing**: `pytest` for all new functionality.
- **Output**: Tools produce cooked artifacts in `Cooked/` (see Shared Schemas).

### CMake
- **Minimum version**: 3.16.
- **Targets**: Use `target_*` commands (not global `include_directories`).
- **Presets**: Use `CMakePresets.json` for Windows Debug/Release builds.
- **Includes**: Never use `include_directories()` globally; always `target_include_directories()`.
- **Comments**: Every CMake block must have a `# TEACHING NOTE` explaining the pattern.

---

## Shared Schemas & Versioning Rules

All data files shared between the editor, tools, and runtime engine MUST:

1. **Live under `shared/schemas/`** as JSON Schema (draft-07) definitions.
2. **Include a `version` field** (SemVer string, e.g. `"1.0.0"`).
3. **Include a `$schema` reference** pointing to the local schema file.
4. **Be versioned**: any breaking field change bumps the major version.
5. **Use stable GUIDs** for asset identifiers (UUID v4 format string).

### Canonical schemas (current version):
| Schema file                    | Version | Used by                        |
|-------------------------------|---------|-------------------------------|
| `project.schema.json`          | 1.0.0   | Editor (project open/save)     |
| `asset_registry.schema.json`   | 1.0.0   | Editor + Engine (asset lookup) |
| `scene.schema.json`            | 1.0.0   | Editor + Engine (scene load)   |
| `audio_bank.schema.json`       | 1.0.0   | Audio tool → Engine            |
| `skeleton.schema.json`         | 1.0.0   | Anim tool → Engine             |
| `anim_clip.schema.json`        | 1.0.0   | Anim tool → Engine             |
| `anim_graph.schema.json`       | 1.0.0   | Editor + Engine                |

### Asset Registry format summary:
```json
{
  "id": "<uuid-v4>",
  "version": "1.0.0",
  "type": "texture|mesh|audio|anim_clip|skeleton|scene|material",
  "source": "Content/relative/path.png",
  "cooked": "Cooked/relative/path.bin",
  "hash": "<sha256-hex>",
  "dependencies": ["<uuid>"]
}
```

---

## ECS Component Reference

All components live in `src/engine/ecs/ECS.hpp`.  Before adding any new system,
check this table — the component you need may already exist.

| Component | Key fields | Used by |
|-----------|-----------|---------|
| `TransformComponent` | `position`, `rotation`, `scale`, `velocity`, `isDirty`, `Forward()`, `Translate()` | Every visible entity |
| `HealthComponent` | `hp`, `maxHp`, `mp`, `maxMp`, `isDead`, `isDowned`, `regenRate`, `HPFraction()`, `Heal()`, `SpendMP()` | Combat, Camp, Magic |
| `StatsComponent` | `strength`, `defence`, `magic`, `spirit`, `speed`, `luck`, `critRate`, `fireResist`, `iceResist`, `lightningResist` | Combat damage formula |
| `NameComponent` | `name`, `internalID`, `title` | UI, Dialogue, Save |
| `RenderComponent` | `spriteSheet`, `sourceRect`, `tint`, `zOrder`, `isVisible`, `symbol`, `colorPair` (ncurses) | Terminal renderer |
| `MovementComponent` | `moveSpeed`, `sprintSpeed`, `jumpForce`, `isGrounded`, `facingDir`, `dashCooldown` | Player input, Vehicle |
| `CombatComponent` | `isInCombat`, `attackCooldown`, `attackRate`, `xpReward`, `gilReward`, `canWarpStrike`, `warpCooldown`, `currentTarget`, `attackElement` | CombatSystem |
| `InventoryComponent` | `slots[MAX_INV_SLOTS]` (ItemStack), `FindItem()`, `HasItem()` | InventorySystem, Shop |
| `QuestComponent` | `quests[MAX_QUESTS]` (QuestEntry), `activeCount` | QuestSystem |
| `DialogueComponent` | `dialogueTreeID`, `interactRange`, `isInteractable`, `currentNodeID`, `portraitAsset` | (DialogueSystem — not built yet) |
| `AIComponent` | `currentState` (IDLE/PATROL/ALERT/CHASE/ATTACK/FLEE/STUNNED/DEAD), `sightRange`, `hearRange`, `attackRange`, `aggroTarget`, `waypoints`, `isNocturnal` | AISystem |
| `PartyComponent` | `members[MAX_PARTY_SIZE]`, `leaderID`, `formationID`, `AddMember()`, `RemoveMember()` | Game main loop |
| `MagicComponent` | `knownSpells`, `equippedSpell`, `isCasting`, `activeElement`, flask quantities | MagicSystem |
| `EquipmentComponent` | `weaponID`, `offhandID`, `headID`, `bodyID`, `legsID`, `accessory1/2`, `bonusStrength/Defence/Magic/HP/MP` | InventorySystem |
| `StatusEffectsComponent` | `active[MAX_STATUS]` (ActiveStatusEntry), `bitmask`, `Apply()`, `Remove()`, `Has()` | CombatSystem |
| `LevelComponent` | `level`, `currentXP`, `pendingXP`, `pendingLevelUp`, `GainXP()`, `ApplyBankedXP()` | CombatSystem, Camp |
| `CurrencyComponent` | `gil` (uint64_t), `crownTokens`, `SpendGil()`, `EarnGil()` | ShopSystem |
| `SkillsComponent` | `skills[MAX_SKILLS]` (SkillEntry), `equippedSkills[4]` | CombatSystem |
| `CampComponent` | `isCamping`, `currentMealID`, `mealDuration`, meal stat bonuses | CampSystem |

### Missing components — must be added to ECS.hpp for new systems

| Component | Needed for | Key fields to add |
|-----------|-----------|-------------------|
| `AudioSourceComponent` | Audio system (M3) | `clipID` (string), `is3D` (bool), `volume` (float), `isLooping` (bool), `isPlaying` (bool), `maxDistance` (float) |
| `AnimatorComponent` | Animation system (M4) | `skeletonID` (string), `currentClipID` (string), `blendTreeID` (string), `playbackSpeed` (float), `currentTime` (float), joint matrix array ptr |
| `RigidBodyComponent` | Physics system (M5) | `mass` (float), `isStatic` (bool), `useGravity` (bool), `linearDamping` (float), Jolt body ID (opaque handle) |
| `ColliderComponent` | Physics system (M5) | `shapeType` (enum: sphere/capsule/box/mesh), `radius/halfExtents`, `isTrigger` (bool), physics layer mask |
| `VehicleComponent` | Vehicle system (post-M8) | `throttle`, `brake`, `steerAngle`, wheel suspension state (4 wheels), `currentSpeed` |

---

## CMake Integration Pattern

When adding a new `.cpp` file to the engine, follow this exact pattern:

**1. For a new engine subsystem (goes into `engine_sandbox`):**

```cmake
# In CMakeLists.txt — inside the if(ENGINE_ENABLE_VULKAN) block,
# add your file to SANDBOX_SOURCES:
set(SANDBOX_SOURCES
    ...existing files...
    src/engine/audio/xaudio2_backend.cpp   # ← add here
    src/engine/audio/audio_system.cpp      # ← add here
)
```

**2. For a new gameplay system (goes into the terminal `game` target):**

```cmake
# In CMakeLists.txt — inside the if(ENGINE_ENABLE_TERMINAL) block,
# add your file to GAME_SOURCES:
set(GAME_SOURCES
    ...existing files...
    src/game/systems/dialogue_system.cpp   # ← add here
)
```

**3. For a new standalone tool (`cook.exe`, `pak.exe`):**

```cmake
# After the existing if(ENGINE_ENABLE_VULKAN) block, add a new target:
add_executable(cook
    src/tools/cook/cook_main.cpp
    src/engine/core/Logger.cpp
)
target_include_directories(cook PRIVATE src/)
```

**4. For a new shader (GLSL → SPIR-V):**

```cmake
# In the GLSL_SHADERS list inside if(ENGINE_ENABLE_VULKAN):
set(GLSL_SHADERS
    "${SHADER_SOURCE_DIR}/triangle.vert"
    "${SHADER_SOURCE_DIR}/triangle.frag"
    "${SHADER_SOURCE_DIR}/textured_quad.vert"   # ← add here
    "${SHADER_SOURCE_DIR}/textured_quad.frag"   # ← add here
)
```

**5. For a new CMake option (e.g. `ENGINE_ENABLE_PHYSICS`):**

```cmake
# Add near the other option() calls at the top of CMakeLists.txt:
option(ENGINE_ENABLE_PHYSICS "Build Jolt Physics integration" OFF)
if(ENGINE_ENABLE_PHYSICS)
    find_package(JoltPhysics REQUIRED)  # via vcpkg
    target_compile_definitions(engine_sandbox PRIVATE ENGINE_ENABLE_PHYSICS)
    target_link_libraries(engine_sandbox PRIVATE JoltPhysics)
endif()
```

---

## M3 Bootstrap Guide (Active Milestone)

M2 is complete. M3 is the Vulkan Texture + XAudio2 Audio milestone. Build in this order:

### Step 1 — Add `directxtex` to vcpkg.json
```json
{
  "dependencies": [
    "nlohmann-json",
    "directxtex"
  ]
}
```
This provides `DirectXTex` for DDS/BC7 texture compression on Windows.

### Step 2 — `src/engine/rendering/vulkan/vulkan_texture.hpp/.cpp`
- Load a DDS file (using DirectXTex) into a `VkImage` + `VkImageView`.
- Support BC7 compressed format (`VK_FORMAT_BC7_UNORM_BLOCK`).
- Expose `VulkanTexture::Load(device, physicalDevice, commandPool, queue, path)`.
- Add a `Sampler()` accessor returning a `VkSampler`.

### Step 3 — `src/engine/rendering/vulkan/vulkan_descriptor.hpp/.cpp`
- Wrap `VkDescriptorPool` + `VkDescriptorSetLayout` + `VkDescriptorSet`.
- Bind the texture sampler to binding 0 (fragment shader).

### Step 4 — Textured quad shaders (`shaders/textured_quad.vert/.frag`)
- `.vert`: pass UV coordinates through to fragment stage.
- `.frag`: sample from a `sampler2D` at binding 0.
- Add both to `GLSL_SHADERS` list in `CMakeLists.txt`.

### Step 5 — `LoadScene("textured_quad", ...)` in `VulkanRenderer`
- Create `VulkanTexture` from a cooked DDS in `Cooked/`.
- Create `VulkanDescriptor` binding the texture sampler.
- Use the textured quad pipeline + a quad mesh.
- Acceptance: `engine_sandbox.exe --headless --scene textured_quad` exits 0.

### Step 6 — `src/engine/audio/xaudio2_backend.hpp/.cpp`
- XAudio2 device init → `IXAudio2MasteringVoice`.
- Source voice pool: pre-allocate N `IXAudio2SourceVoice` instances.
- `Play(clipId)` → resolve cooked `.wav` via `AssetDB`, submit to source voice.
- `Stop(clipId)` → stop the matching source voice.
- Add `AudioSourceComponent` to `ECS.hpp`.
- Add `CMakeLists.txt` entry: link `xaudio2.lib` on Windows only.

### Step 7 — `src/engine/audio/audio_system.hpp/.cpp`
- ECS system: each frame, iterate `AudioSourceComponent`; call `Play`/`Stop` on backend.
- Music layer FSM: EXPLORATION / BATTLE / VICTORY / MENU states, crossfade on transition.
- Event-driven triggers via `EventBus`.

### Step 8 — Wire into CI
- `build-windows.yml` step: `engine_sandbox.exe --headless --scene textured_quad`
- Contract test: cook a 1×1 DDS texture stub; verify `AssetDB` resolves it.

---

## Lua Hook Reference

The C++ engine calls into Lua scripts at these points (via `LuaEngine::CallFunction`).
When a new system is added, wire in Lua hooks following the same pattern.

| C++ call site | Lua function called | Arguments | Where called |
|--------------|--------------------|-----------|----|
| `CombatSystem::StartCombat()` | `on_combat_start(enemyID)` | `EntityID` | `CombatSystem.cpp` |
| `CombatSystem::CheckDeaths()` | `on_entity_died(entityID)` | `EntityID` | `CombatSystem.cpp` |
| `QuestSystem` objective complete | `on_quest_complete(questID)` | `uint32_t` | `QuestSystem.cpp` |
| `Game::LoadScripts()` | `on_game_start()` | none | `Game.cpp` |
| `CampSystem::Rest()` | `on_camp_rest()` | none | `CampSystem.cpp` |
| `Zone::SpawnEnemies()` | `on_zone_loaded(zoneName)` | `string` | `Zone.cpp` |

**Pattern to add a new Lua hook:**
```cpp
// In the relevant system .cpp file:
auto& lua = LuaEngine::Get();
lua.CallFunction("on_my_new_event", arg1, arg2);  // safe — does nothing if undefined
```

Lua scripts should define the function as:
```lua
function on_my_new_event(arg1, arg2)
    engine_log("my_new_event fired: " .. tostring(arg1))
end
```

---

## vcpkg Setup

`vcpkg.json` already exists in the repository root with `nlohmann-json` (added for M2).
Add new dependencies as milestones progress by editing `vcpkg.json`:

```json
// vcpkg.json — current state (M2 complete)
{
  "name": "educational-game-engine",
  "version-string": "1.0.0",
  "dependencies": [
    "nlohmann-json",          // M2 — cook.exe JSON parsing ✅
    "directxtex"              // M3 — DDS/BC7 texture compression (add now)
  ]
}
```

Planned additions:
- **M5** (physics): `"joltphysics"`
- **M5+** (glTF mesh loading): `"tinygltf"`

Integrate with CMake (in `CMakeLists.txt`):
```cmake
# TEACHING NOTE — vcpkg toolchain file
# If VCPKG_ROOT is set, the toolchain file automatically finds all
# vcpkg packages. Users run:
#   cmake --preset windows-debug -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
# Or add to CMakePresets.json userSettings.
```

---

## Test Directory Structure

The `tests/` directory exists and has the following layout (created for M2):

```
tests/
├── __init__.py
├── cook/                      # Python pytest: cook pipeline validation
│   ├── __init__.py
│   └── test_cook_pipeline.py  # 13 tests: cook output, schema compliance, cooked paths
└── golden/                    # Golden-file reference outputs for contract tests
    ├── assetdb_expected.json  # Expected cook.exe output for sample project
    └── README.md
```

**Still needed for M3+:**
```
tests/
└── unit/                      # C++ unit tests (one file per system) — NOT YET CREATED
    ├── test_asset_db.cpp      # Tests for AssetDB::Load, GetCookedPath
    ├── test_combat.cpp        # Tests for CalculateDamage, status effects
    └── CMakeLists.txt
```

When C++ unit tests are added, also add to root `CMakeLists.txt`:
```cmake
enable_testing()
add_subdirectory(tests)
```

And create `tests/CMakeLists.txt`:
```cmake
add_executable(unit_tests
    unit/test_asset_db.cpp
    unit/test_combat.cpp
)
target_include_directories(unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME UnitTests COMMAND unit_tests)
```

---

## Pipeline Rules

```
Editor / Tools (authoring)
       |
       v  writes source assets
  Content/   (raw: .png, .wav, .fbx, .blend, ...)
       |
       v  cook step (Python scripts or editor export)
   Cooked/   (runtime-ready: .tex, .bank, .skelc, .animc, ...)
       |
       v  engine loads
  Runtime Engine
```

- **Editors write to `Content/`** — never directly to `Cooked/`.
- **Cook scripts write to `Cooked/`** — never modify `Content/`.
- **Engine loads from `Cooked/`** in shipping mode, `Content/` in dev mode.
- **Asset registry** (`AssetRegistry.json`) is updated by cook step.

---

## Per-Feature Checklist

When adding any new feature, Copilot MUST:
1. Add the feature implementation with `// TEACHING NOTE` comments.
2. Add or update the relevant `shared/schemas/` JSON schema if new data format.
3. Add a sample usage in `samples/vertical_slice_project/`.
4. Add or update documentation in `docs/`.
5. Add tests (C++: test target; Python: pytest).

---

## Build Entry Points (Windows)

```bat
# Configure and build everything (engine + editor):
cmake --preset windows-debug
cmake --build --preset windows-debug

# Build engine only (no Qt required):
cmake --preset windows-debug-engine-only
cmake --build --preset windows-debug-engine-only

# Cook vertical slice assets:
cd samples/vertical_slice_project
python cook_assets.py
```

---

## Milestone Ladder

See the **"Next Milestone — What to Work On Now"** table in the "Current Development Status" section above for the authoritative, up-to-date roadmap.  The table below is a condensed reminder only.

| Milestone | Goal | Status |
|-----------|------|--------|
| M0 | D3D11 window + clear screen (default); Vulkan optional | ✅ |
| M1 | Colored triangle (Vulkan SPIR-V pipeline) | ✅ |
| M1.5 | D3D11 baseline renderer + IRenderer abstraction + CI fix | ✅ |
| M2 | AssetDB + `cook.exe` + contract CI | ✅ |
| M3 | D3D11/Vulkan texture + XAudio2 | ⬜ **active** |
| M4 | Animation runtime (C++) | ⬜ |
| M5 | Jolt Physics | ⬜ |
| M6 | Editor inspector + Play-in-Engine | ⬜ |
| M7 | World streaming | ⬜ |
| M8 | Wire all gameplay into D3D11/Vulkan runtime | ⬜ |

---

## Definition of Done — Project Completion Criteria

The project is considered **complete** when every subsystem listed in
`docs/FF15_REQUIREMENTS_BLUEPRINT.md` reaches ✅ in the Runtime, Tool, and
Tests columns of the Completion Matrix **and** satisfies the quality bar below.

A student must be able to:
1. **Run** a demonstration at FF15-comparable fidelity across visuals, physics,
   sound, and gameplay — the engine and tools can *produce* a game at that level.
2. **Read** every subsystem's source code and understand *why* each design
   decision was made (all code annotated with `// TEACHING NOTE` blocks).
3. **Extend** any subsystem by following the established patterns without
   breaking other systems.

### Quality bar per domain

| Domain | Minimum "complete" quality |
|--------|---------------------------|
| **Visuals** | PBR rendering (IBL + directional shadows + bloom + tonemapping); GPU-skinned skeletal meshes; dynamic sky (procedural time-of-day + weather FX); Vulkan ≥ 1.3 pipeline on Windows |
| **Physics** | Rigid-body simulation (Jolt Physics); character capsule controller with step-up and slopes; vehicle wheel-ray physics; physics-based hit volumes for combat |
| **Sound** | XAudio2 backend; positional 3D audio with distance attenuation; layered music system (battle / exploration / idle blend); event-driven SFX triggers |
| **Gameplay** | Real-time action combat (warp-strike, link-strike, combo chains, ATB); open-world zone streaming without loading screens; party AI (behaviour tree + formation); quest system with dialogue; save/load (15 slots + auto-save at camp) |
| **Tools** | Asset cooker (`cook.exe`); texture / mesh / audio / animation import pipeline; Qt 6 scene editor with Play-in-Engine; Python authoring tools for audio and animation |
| **Teaching** | Every non-trivial pattern has a `// TEACHING NOTE` block; subsystem docs in `docs/`; `samples/vertical_slice_project/` demonstrates each subsystem end-to-end |

> **Note on content vs. quality:** The goal is not a content-complete copy of
> FF15.  It *is* a toolchain and engine where each technology category (visuals,
> physics, sound, gameplay) is implemented at the same *class* of solution FF15
> uses — real PBR, real physics, real positional audio, real action combat — so
> that a student studying this code is studying the same patterns a professional
> AAA studio uses.  Stubs or toy implementations do not satisfy this bar.

---

## North Star

> Build a complete, teachable FFXV-style open-world action RPG toolchain.
> Every commit must move the vertical slice forward.
> Prefer simple-but-real over fancy-but-incomplete.
> The project is finished when all 13 subsystems ship at FF15-comparable quality
> and every one can be fully taught from source code alone.
