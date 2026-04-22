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

> **⚠️ ACTIVE POLICY — D3D11 Only (until explicitly stated otherwise)**
> Vulkan work is **DEFERRED**. All new rendering code targets **D3D11 only**.
> Do **NOT** implement any Vulkan-specific features unless explicitly instructed.
> The Vulkan backend already compiles and the existing Vulkan files are kept as-is.
> New rendering features (textures, skinning, PBR, shadow maps, etc.) are implemented
> on the D3D11 path first. See the **"Vulkan — Deferred Requirements Reference"**
> section near the bottom for a full list of what Vulkan will eventually require.

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
| CI — Windows build + headless | ✅ | `.github/workflows/build-windows.yml`: MSVC x64 + D3D11 (no Vulkan SDK); builds `engine_sandbox` + `cook`; runs `--headless` (WARP) + `--validate-project`; M7 streaming tests (`streaming_load`, `streaming_evict`, `streaming_async`); `build-windows-physics` job (Jolt, classic-mode vcpkg, `--scene physics_test`, `--scene vehicle_test`) |
| CI — contract / golden-file tests | ✅ | `.github/workflows/contract-tests.yml`: runs `cook`, diffs against `tests/golden/assetdb_expected.json`; pytest cook pipeline (13 tests); TEACHING NOTE audit |
| CI — Architecture Lint | ✅ | `.github/workflows/architecture-lint.yml`: runs `check_architecture.py` (file-size + layer rules) and `extract_teaching_notes.py`; fails if `CURRICULUM_INDEX.md` is stale |
| `vcpkg.json` | ✅ | Repo root; `nlohmann-json` (M2), `directxtex` (M3 texture), `imgui[docking,dx11-binding,win32-binding]` (editor), `joltphysics` (M5; CI physics job installs via classic-mode vcpkg separately) |
| Vertical slice sample skeleton | ✅ | `samples/vertical_slice_project/`: `cook_assets.py` (stubs), `Project.json`, `AssetRegistry.json`, `Content/`, `Cooked/` |

---

### Rendering (D3D11 default + Vulkan optional, Windows)

> **Active path: D3D11 only.** Vulkan items are labelled **(DEFERRED)**. See "Vulkan — Deferred Requirements Reference" section.

| Area | Status | Notes |
|------|--------|-------|
| `IRenderer` abstract interface | ✅ | `src/engine/rendering/IRenderer.hpp` — backend-agnostic Init/DrawFrame/Shutdown |
| `RendererFactory` | ✅ | `src/engine/rendering/RendererFactory.hpp` — creates D3D11Renderer or VulkanRenderer by `--renderer` flag |
| D3D11 renderer (default) | ✅ | `src/engine/rendering/d3d11/D3D11Renderer.hpp/.cpp`; D3D_FEATURE_LEVEL_10_0 minimum; WARP headless mode; GT610-compatible |
| Win32 window + headless mode | ✅ | `src/engine/platform/win32/Win32Window.hpp/.cpp`; `--headless` arg in `src/sandbox/main.cpp` |
| Vulkan bootstrap (optional, M0) | ✅ | Instance, device, swapchain, render pass, framebuffers, command buffers, sync primitives |
| SPIR-V pipeline + colored triangle (M1, Vulkan) | ✅ | `VulkanPipeline`, `VulkanMesh`, `VulkanBuffer`; `shaders/triangle.vert/.frag` |
| D3D11 depth buffer | ✅ | D24_UNORM_S8_UINT DSV + DepthStencilState; wired into DrawFrame (ClearDepthStencilView + OMSetRenderTargets); recreated on resize (M16) |
| **Vulkan depth buffer** | **(DEFERRED)** | Needed for 3D geometry in the Vulkan path — implement when Vulkan work resumes |
| D3D11 textures (DDS/BC7) | ✅ | `src/engine/rendering/d3d11/d3d11_texture.hpp/.cpp` — self-contained DDS loader (RGBA8, BC1, BC3, BC7/DX10); no directxtex needed |
| **Vulkan textures (DDS/BC7)** | **(DEFERRED)** | `src/engine/rendering/vulkan/vulkan_texture.hpp/.cpp` — implement when Vulkan work resumes |
| **Vulkan descriptor sets** | **(DEFERRED)** | `src/engine/rendering/vulkan/vulkan_descriptor.hpp/.cpp` — implement when Vulkan work resumes |
| D3D11 textured quad scene | ✅ | HLSL shaders `shaders/textured_quad.vs.hlsl` + `textured_quad.ps.hlsl`; `LoadScene("textured_quad")` + `DrawTexturedQuad()` + `UnloadScene()` in D3D11Renderer; 1×1 white fallback if no DDS |
| D3D11 PBR rendering (M9) | ✅ | `shaders/pbr_mesh.vs.hlsl` + `pbr_mesh.ps.hlsl` (SM 4.0); GGX NDF + Smith G + Schlick F + Reinhard tonemap + gamma; `LoadScene("pbr_mesh")`; UV sphere 16×16; 3 CBs (perFrameCB/lightCB/materialCB); `--headless --scene pbr_mesh` CI |
| **PBR: IBL (M16 complete)** | ✅ | BRDF LUT (64×64 RG8, Hammersley+GGX integration), irradiance cubemap (16×16×6 cosine-weighted hemisphere avg), prefiltered env cubemap (16×16×6×5mip GGX importance sampling), all CPU-generated from procedural sky; `--headless --scene pbr_ibl` CI |
| **PBR: shadow maps + bloom** | ✅ | Directional shadow map (512×512 D32, PCF 3×3 in `shadow_lit.ps.hlsl`), LDR bloom demo (bright-pass + separable Gaussian blur in RGBA8 offscreen RTs + composite with Reinhard tonemap); `--headless --scene shadow_test` + `bloom_test` CI |
| **Directional shadow map** | ✅ | `shadow.vs.hlsl` depth-only pass, `shadow_lit.vs/ps.hlsl` PCF lit pass; `ShadowScene` struct; depth bias rasterizer state; `SamplerComparisonState` |
| **Post-processing (bloom + tonemap)** | ✅ | `bloom_bright/blur/composite.ps.hlsl`; 4× RGBA8 256×256 offscreen RTs; `BloomScene` struct; 5-tap Gaussian (ping-pong); Reinhard tonemap in composite |
| Dynamic sky / procedural time-of-day (M10) | ✅ | `src/engine/rendering/sky_renderer.hpp/.cpp` — time-of-day clock, sun direction, zenith/horizon colour phases, sunset/sunrise tint, cloud darkening; `--headless --scene dynamic_sky` CI |
| Weather VFX — fog + rain (M10) | ✅ | `src/engine/rendering/weather_fx.hpp/.cpp` — fog density, rain intensity, cloud cover smooth lerp; `shaders/sky.vs.hlsl` (SV_VertexID full-screen triangle) + `sky.ps.hlsl` (gradient + sun disc + fog + weather + Reinhard + gamma) |
| D3D11 GPU skinned mesh pass | ✅ | `shaders/skinned_mesh.vs.hlsl` + `skinned_mesh.ps.hlsl`; `GpuSkinningBuffer` (64 × Mat4 CB); D3D11Renderer `skinned_mesh` scene |
| **Vulkan GPU skinned mesh pass** | **(DEFERRED)** | `shaders/skinned_mesh.vert/.frag`; joint matrix UBO |
| D3D11 swapchain resize handling | ✅ | `D3D11Renderer::RecreateSwapchain()` |
| D3D11 headless frame recording | ✅ | `D3D11Renderer::RecordHeadlessFrame()` |
| Vulkan swapchain resize handling | ✅ | `VulkanRenderer::RecreateSwapchain()` — existing, kept as-is |
| Vulkan headless frame recording | ✅ | `VulkanRenderer::RecordHeadlessFrame()` — existing, kept as-is |

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
They must be re-wired to the **D3D11 runtime** at **Milestone M8** (Vulkan wiring is deferred).

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
| Behaviour tree AI | ✅ | `src/engine/ai/behaviour_tree.hpp/.cpp` — BtStatus, BtBlackboard, BtNode, BtSequence, BtSelector, BtCondition, BtAction, BtTree; `--scene bt_test` 4-test headless CI |
| Formation system | ✅ | `src/engine/ai/formation_system.hpp/.cpp` — FormationSystem (LINE / V_SHAPE / CIRCLE), slot offset generation, greedy assignment, world-space transform |
| Nav-mesh baker | ✅ | `src/engine/ai/nav_mesh.hpp/.cpp` — grid NavMesh, BakeFromGrid / BakeEmpty / SetWalkable, A* FindPath (4-dir + diagonal, obstacle routing) |
| Vehicle physics | ✅ | `src/engine/vehicle/vehicle_system.hpp/.cpp` — `VehicleSystem` (wheel-ray suspension + spring-damper + Ackermann steer + fuel); `WheelState` × 4 in `VehicleComponent`; vehicle chase camera in `CameraSystem`; `--scene vehicle_test` headless CI |

---

### Audio (C++ Runtime)

| Area | Status | Notes |
|------|--------|-------|
| Python `audio_authoring` tool | ✅ | `tools/audio_authoring/audio_engine/` — AI generation, DSP, OGG/WAV export (32 tests passing) |
| `audio_engine.py` CLI | ✅ | `tools/audio_engine.py` — register/emit/consume/list |
| XAudio2 backend (C++) | ✅ | `src/engine/audio/xaudio2_backend.hpp/.cpp` — device init, master voice, 16-slot voice pool, WAV parser, `Play`/`Stop`/`SetSlotVolume` |
| Audio system (C++) | ✅ | `src/engine/audio/audio_system.hpp/.cpp` — ECS AudioSystem, music FSM (EXPLORATION/BATTLE/VICTORY/MENU) with real crossfade, event-driven play/stop |
| X3DAudio 3D positional audio | ✅ | `X3DAudioInitialize` after mastering voice; `SetListenerPosition`; `Compute3DVolume` (X3DAudio DSP path + linear fallback); `Apply3DAttenuation`; per-frame `AudioSystem::Update` reads `TransformComponent`; `--headless --scene audio_3d_test` 3-test CI |

**M3 audio is ✅ complete. M3 D3D11 textured quad is ✅ complete.** M4b (IK solver + D3D11 GPU skinning) is ✅ complete. M5 (Jolt Physics) is ✅ complete. M6 (Editor) is ✅ complete. M7 (World Streaming, all sub-milestones M7.1–M7.5) is ✅ complete. M8 (Gameplay Integration, all sub-milestones M8.1–M8.9) is ✅ complete. **M10 (Dynamic Sky + weather VFX, all sub-milestones M10.1–M10.7) is ✅ complete.** **Post-M10 Vehicle Physics is ✅ complete.** **Post-M10 Behaviour Tree AI is ✅ complete:** `BtTree`/`BtSequence`/`BtSelector`/`BtCondition`/`BtAction`/`BtBlackboard` (engine/ai/behaviour_tree); `FormationSystem` (LINE/V_SHAPE/CIRCLE slot layouts, greedy assignment, world-space transform); grid `NavMesh` (BakeFromGrid, A* FindPath, obstacle routing); `--scene bt_test` 4-test headless CI. **Post-M10 SDF Font Renderer is ✅ complete:** `FontRenderer` (SDF atlas, R8_UNORM D3D11 texture, sdf_text.vs/ps.hlsl, `--scene font_test` CI). **`pak.exe` PAK1 packager is ✅ complete:** `src/tools/pak/pak_main.cpp` (PAK1 format, `--input`/`--list`/`--extract`, CI test). **Post-M10 Cinematics runtime is ✅ complete:** `CinematicSequencer` + `CameraRig` + `CameraComponent.cinematicOverride` + `--scene cinematic_test` 3-test headless CI. Cut-scene baker tool + editor panel remain ⬜ (M22). **M16 (D3D11 depth buffer + IBL) is ✅ complete:** D24_UNORM_S8_UINT depth buffer wired into DrawFrame; CPU-generated BRDF LUT + irradiance cubemap + prefiltered env cubemap (Hammersley, GGX importance sampling); `pbr_ibl.vs/ps.hlsl`; `--scene pbr_ibl` 4-test headless CI. **M17 (D3D11 shadow maps + bloom) is ✅ complete:** directional shadow map (512×512 D32_FLOAT, depth-bias rasterizer, 3×3 PCF via `SamplerComparisonState`, `shadow.vs.hlsl` + `shadow_lit.vs/ps.hlsl`); LDR bloom demo (4× 256×256 RGBA8 offscreen RTs, 5-tap separable Gaussian blur ping-pong, Reinhard composite, `bloom_bright/blur/composite.ps.hlsl`); `--headless --scene shadow_test` + `bloom_test` 3-test CI each. **M18 (X3DAudio 3D positional audio) is ✅ complete:** `X3DAudioInitialize` after mastering voice; `SetListenerPosition`/`Compute3DVolume`/`Apply3DAttenuation` on `XAudio2Backend` (X3DAudio DSP path + linear fallback); per-frame attenuation in `AudioSystem::Update` reads `TransformComponent`; `--headless --scene audio_3d_test` 3-test CI. Next: M19 action combat completion.

---

### Animation (C++ Runtime)

| Area | Status | Notes |
|------|--------|-------|
| Python `anim_authoring` tool | ✅ | `tools/anim_authoring/animation_engine/` — skeleton, clips, IK, glTF, blend trees (11 tests passing) |
| Skeleton runtime (C++) | ✅ | `src/engine/animation/skeleton.hpp/.cpp` — joint hierarchy, bind pose, world-transform computation |
| Anim clip evaluation (C++) | ✅ | `src/engine/animation/anim_clip.hpp/.cpp` — keyframe channels, lerp/slerp evaluation |
| Blend tree (C++) | ✅ | `src/engine/animation/blend_tree.hpp/.cpp` — clip nodes + linear blend |
| IK solver (C++) | ✅ | `src/engine/animation/ik_solver.hpp/.cpp` — Two-Bone analytical IK (law of cosines) + FABRIK iterative N-joint IK |
| D3D11 GPU skinning (C++) | ✅ | `src/engine/animation/gpu_skinning.hpp/.cpp`; `GpuSkinningBuffer` (64 × Mat4 DYNAMIC CB); `shaders/skinned_mesh.vs.hlsl` + `skinned_mesh.ps.hlsl`; wired into D3D11Renderer `skinned_mesh` scene |
| **Vulkan GPU skinning (C++)** | **(DEFERRED)** | Upload joint matrices to Vulkan UBO — implement when Vulkan work resumes |
| Animation system (C++) | ✅ | `src/engine/animation/animation_system.hpp/.cpp` — ECS update, advance time, evaluate, update `AnimatorComponent` |

**M4 is ✅ complete.** CPU core (skeleton, anim clip, blend tree, animation system) + IK solver (Two-Bone + FABRIK) + D3D11 GPU skinning (constant buffer upload, skinned_mesh.vs.hlsl, skinned_mesh scene in engine_sandbox) are all done.

---

### Physics

| Area | Status | Notes |
|------|--------|-------|
| Jolt Physics integration | ✅ | `vcpkg.json` has `joltphysics`; `ENGINE_ENABLE_PHYSICS` CMake option (default ON); `find_package(Jolt CONFIG QUIET)` |
| `PhysicsWorld` wrapper | ✅ | `src/engine/physics/physics_world.hpp/.cpp` — pImpl facade; `Init`, `Step`, `Shutdown`; `CreateBox/Sphere/Capsule`; `Raycast` |
| Rigid body ECS component | ✅ | `RigidBodyComponent` (component 22) + `RigidBodyCreator` in `src/engine/physics/rigid_body.hpp/.cpp` — `Create`, `Destroy`, sync helpers |
| Character capsule controller | ✅ | `src/engine/physics/character_controller.hpp/.cpp` — `JPH::CharacterVirtual`; gravity, step-up, slope-slide, jump; `IsGrounded()` |
| Raycast interface | ✅ | `src/engine/physics/raycast.hpp/.cpp` — `CastRay`, `CastRayDown`, `CastSphere`; `ShapeCastHit` |
| Hit volumes (combat) | ✅ | `src/engine/physics/hit_volume.hpp/.cpp` — `HitVolumeManager`; AABB Attack/Hurt volume register/query |
| `ColliderComponent` (ECS) | ✅ | Component 23 in `ECS.hpp` — `shapeType` (Box/Sphere/Capsule), `halfExtents`, `radius`, `isTrigger` |
| `physics_test` CI scene | ✅ | `--scene physics_test` in `main.cpp`; 3 acceptance tests (drop_sphere, step_ledge, raycast); `build-windows-physics` CI job |
| `physics_impl.hpp` | ✅ | Internal pImpl header; `PhysicsWorldImpl` struct + Jolt layer filters; never included outside `physics/` |

**M5 is ✅ complete.** Jolt Physics integrated: `PhysicsWorld`, `CharacterController`, `Raycast`, `HitVolumeManager`, `RigidBodyComponent` + `ColliderComponent` in ECS, `physics_test` headless acceptance scene, CI job. **M6 is ✅ complete.** `SceneHierarchyPanel`, `InspectorPanel`, `SceneSerialiser`, `Play-in-Engine`, headless editor CLI, asset drag-drop (ContentBrowser→SceneEditor), `build-windows-editor` CI job. **M7 is ✅ complete.** `WorldStreamingManager`, `WorldPartition`, `AsyncLoader`; 3 headless acceptance scenes (`streaming_load`, `streaming_evict`, `streaming_async`) in CI. Next: M8 Gameplay Integration.

---

### Asset Pipeline (C++ runtime + cook tool)

| Area | Status | Notes |
|------|--------|-------|
| `tools/creation_engine.py` CLI | ✅ | Multi-type asset emit/consume |
| `tools/validate-assets.py` | ✅ | Schema validation against `assets/schema/asset-manifest.schema.json` |
| `cook_assets.py` (Python stubs) | 🔨 | Copies files; texture/audio/anim cook steps are stubs |
| `cook.exe` (C++ standalone cooker) | ✅ | `src/tools/cook/cook_main.cpp` (504 lines); reads `AssetRegistry.json`, copies source → `Cooked/`, writes `Cooked/assetdb.json`; uses nlohmann/json via vcpkg |
| `pak.exe` (C++ PAK1 packager) | ✅ | `src/tools/pak/pak_main.cpp`; PAK1 binary format (magic+TOC+blobs); `--input`/`--list`/`--extract`; CI acceptance test in build-windows.yml |
| `AssetDB` runtime (C++) | ✅ | `src/engine/assets/asset_db.hpp/.cpp` (268 lines); GUID → absolute cooked path; `Load()`, `GetCookedPath()`, `Has()`, `All()` |
| `AssetLoader` (C++) | ✅ | `src/engine/assets/asset_loader.hpp/.cpp`; synchronous `LoadRaw(id)` → `std::vector<uint8_t>`; async deferred to M7 |
| Golden files + contract tests | ✅ | `tests/golden/assetdb_expected.json`; `tests/cook/test_cook_pipeline.py` (13 pytest); `.github/workflows/contract-tests.yml` |

**M2 is complete.** `engine_sandbox --validate-project samples/vertical_slice_project` loads AssetDB and all cooked assets, exits 0.

**Next step:** See "Next Milestone — What to Work On Now" table above.

---

### World Streaming

| Area | Status | Notes |
|------|--------|-------|
| Tile-based `Zone` / `TileMap` | ✅ | `src/game/world/Zone.hpp/.cpp`, `TileMap.hpp/.cpp` |
| `WorldMap` (multi-zone container) | ✅ | `src/game/world/WorldMap.hpp/.cpp` |
| `world_streaming` (proximity-based) | ✅ | `src/engine/world/world_streaming.hpp/.cpp` — delta load/evict coordinator; `WorldStreamingManager`; `SetMaxCompletionsPerFrame(n)` frame-budget cap; `DrawDebugOverlay(ImDrawList*)` minimap |
| `world_partition` (spatial grid) | ✅ | `src/engine/world/world_partition.hpp/.cpp` — `CellCoord`, `CellIdFromCoord`, `GetCellsNearPosition` |
| `async_loader` (worker thread) | ✅ | `src/engine/world/async_loader.hpp/.cpp` — single worker thread + job queue; `CancelJob(cellId)` cancellation token (`shared_ptr<atomic<bool>>`); `PumpMainThreadCompletions(maxCount)` |
| `cell_data.hpp` | ✅ | `src/engine/world/cell_data.hpp` — `CellData` + `SpawnEntry` POD structs; worker↔main transfer type |
| `GameStreamingManager` (Zone wiring) | ✅ | `src/game/world/GameStreamingManager.hpp/.cpp` — game-layer subclass; `OnLoadCell` calls `AssetLoader::LoadRaw` + deserialises JSON; `OnCellLoaded` calls `Zone::SpawnEnemies/SpawnNPCs`; `OnEvictCell` calls `Zone::Unload` |
| Sample `.level` cooked data | ✅ | `samples/vertical_slice_project/Content/Levels/cell_0_0.cell.json` + `Cooked/Levels/cell_0_0.level`; `cook_assets.py` gains `cook_levels()` |
| ImGui streaming debug overlay | ✅ | `WorldStreamingManager::DrawDebugOverlay` — colour-coded cell grid (grey/yellow/green/red + white camera outline); editor View menu toggle |

**M7 is ✅ complete (all sub-milestones M7.1–M7.5).** `WorldStreamingManager` + `WorldPartition` + `AsyncLoader` + Zone lifecycle wiring + AssetLoader `.level` integration + cancellation tokens + frame-budget cap + ImGui debug overlay. Three CI acceptance scenes (`streaming_load`, `streaming_evict`, `streaming_async`). **Next step (M8):** Wire all gameplay systems (combat, AI, quests, etc.) into the D3D11 runtime.

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

### UI System (D3D11 ImGui Overlay)

| Area | Status | Notes |
|------|--------|-------|
| Terminal UI (ncurses) | ✅ | Part of `TerminalRenderer` + `Game` render methods |
| D3D11 ImGui HUD / menu stack | ✅ | `src/engine/ui/hud.hpp/.cpp`, `menu_stack.hpp/.cpp` — HUD (HP/MP/ATB), menu push/pop stack, `--scene menu_stack_test` CI |
| **Vulkan HUD / menu stack** | **(DEFERRED)** | Vulkan imgui binding — implement when Vulkan work resumes |
| Font renderer | ✅ | `src/engine/ui/font_renderer.hpp/.cpp` — SDF atlas (96 glyphs, 8×8 bitmap→SDF), R8_UNORM D3D11 texture, sdf_text.vs/ps.hlsl, `--scene font_test` CI |

---

### Cinematics

| Area | Status | Notes |
|------|--------|-------|
| Cinematic sequencer | ✅ | `src/engine/cinematics/cinematic_sequencer.hpp/.cpp` — shot/cut, `ApplyToCamera()`, `OnShotChanged`/`OnComplete` callbacks; `--scene cinematic_test` CI |
| Camera rig | ✅ | `src/engine/cinematics/camera_rig.hpp/.cpp` — keyframe Lerp, binary-search bracket |
| Cinematic baker tool | ⬜ | `tools/creation_engine.py` `bake_cinematic()` — timeline JSON → cooked `.cinematic` (M22) |
| Cinematic editor panel | ⬜ | `editor/src/panels/CinematicEditorPanel.hpp/.cpp` — shot timeline, keyframe edit (M22) |

---

### Editor (Dear ImGui)

| Area | Status | Notes |
|------|--------|-------|
| `EditorApp` (DockSpace, menu bar, status bar) | ✅ | `editor/src/EditorApp.hpp/.cpp`; M6: Play-in-Engine, Load Scene, panel wiring |
| `ContentBrowserPanel` | ✅ | `editor/src/ContentBrowserPanel.hpp/.cpp` — file tree via std::filesystem |
| `SceneEditorPanel` canvas | ✅ | `editor/src/SceneEditorPanel.hpp/.cpp` — ImGui DrawList canvas + JSON I/O; M6: z+components, shared-state accessors |
| Entity inspector / property editor | ✅ | `editor/src/panels/InspectorPanel.hpp/.cpp` — table-driven (kComponentDefs); DragFloat/DragInt/Checkbox; Add Component popup |
| Scene hierarchy panel | ✅ | `editor/src/panels/SceneHierarchyPanel.hpp/.cpp` — entity list; add/rename/duplicate/delete; context menu |
| Scene ECS serialization | ✅ | `src/engine/scene/scene_serialiser.hpp/.cpp` — JSON ↔ ECS World; SaveScene/LoadScene/CountEntities; ENGINE_ENABLE_JSON gated |
| Play-in-Engine button | ✅ | `EditorApp::LaunchPlayInEngine()` — saves temp scene to %TEMP%, ShellExecuteExW engine_sandbox.exe |
| Headless editor CLI | ✅ | `editor/src/main.cpp` — `--headless`, `--create-scene`, `--load-scene --validate`; AttachConsole_(); exit 0/1 |
| Asset drag-drop into scene | ✅ | Drag from ContentBrowser → SceneEditor; CONTENT_ASSET payload; auto-creates entity with RenderComponent or AudioSourceComponent |

---

### Next Milestone — What to Work On Now

> **Current position: M18 (X3DAudio 3D positional audio) ✅ complete. Cut-scene baker tool + editor panel ⬜. Next: M19 action combat completion → M20–M22 quest/tool gaps → M14 Vulkan.**

Recommended implementation order to reach project completion (D3D11-first policy — see Active Policy box above):

| Priority | Milestone | Key deliverables |
|----------|-----------|-----------------|
| ~~**1**~~ | ~~**M6: Editor**~~ | ✅ done — SceneHierarchyPanel, InspectorPanel, SceneSerialiser, Play-in-Engine, asset drag-drop |
| ~~**2**~~ | ~~**M7: World streaming**~~ | ✅ done — WorldStreamingManager + Zone wiring (M7.1), AssetLoader `.level` (M7.2), cancellation tokens (M7.3), frame-budget cap (M7.4), ImGui debug overlay (M7.5) |
| ~~**3**~~ | ~~**M8: Gameplay integration**~~ | ✅ done — GameRuntime, InputMapper, CameraSystem, HUD, DialogueSystem, Zone streaming → D3D11 (M8.7), SaveSystem, m8_gameplay + m8_streaming CI scenes |
| ~~**4**~~ | ~~**M9: PBR rendering**~~ | ✅ done — Cook-Torrance BRDF (GGX NDF + Smith G + Schlick F), UV sphere, 3 CBs, Reinhard tonemap, gamma, `pbr_mesh` headless CI |
| ~~**1 — Now**~~ | ~~**M10: Dynamic sky**~~ | ✅ done — SkyRenderer + WeatherFx + sky.vs.hlsl + sky.ps.hlsl + D3D11 SkyScene + 3 headless acceptance tests |
| ~~**1 — Now**~~ | ~~**Post-M10: Vehicle physics**~~ | ✅ done — `VehicleSystem` (wheel-ray suspension, spring-damper, Ackermann steering), `WheelState` × 4, vehicle chase camera in CameraSystem, `--scene vehicle_test` CI |
| ~~**1 — Now**~~ | ~~**Post-M10: Behaviour tree AI**~~ | ✅ done — `BtTree`/`BtSequence`/`BtSelector`/`BtCondition`/`BtAction`/`BtBlackboard`; `FormationSystem` (LINE/V_SHAPE/CIRCLE); grid `NavMesh` (BakeFromGrid, A* with obstacle routing); `--scene bt_test` CI |
| ~~**1 — Now**~~ | ~~**Post-M10: SDF Font Renderer + pak.exe**~~ | ✅ done — `FontRenderer` (SDF atlas, R8_UNORM D3D11 texture, sdf_text.vs/ps.hlsl, `--scene font_test` CI); `pak.exe` (PAK1 format, `--input`/`--list`/`--extract`, CI test) |
| ~~**1 — Now**~~ | ~~**Post-M10: Cinematics**~~ | ✅ done (runtime + tests) — `CinematicSequencer` + `CameraRig` + `CameraComponent.cinematicOverride` + `--scene cinematic_test` 3-test headless CI; cut-scene baker tool + editor panel remain ⬜ (M22) |
| ~~**1 — Now**~~ | ~~**M16: D3D11 depth buffer + IBL**~~ | ✅ done — D24 DSV + DepthStencilState wired into DrawFrame; CPU-generated BRDF LUT (64×64) + irradiance cube (16×16×6) + prefiltered env cube (16×16×6×5mip); `pbr_ibl.vs/ps.hlsl`; `--scene pbr_ibl` 4-test CI |
| ~~**1 — Now**~~ | ~~**M17: D3D11 shadow maps + bloom**~~ | ✅ done — Shadow pass (512×512 D32, depth bias, PCF 3×3 SamplerComparisonState) + LDR bloom demo (4× 256×256 RGBA8 RT ping-pong, Gaussian blur, Reinhard composite); `--scene shadow_test` + `bloom_test` CI |
| ~~**1 — Now**~~ | ~~**M18: X3DAudio 3D positional audio**~~ | ✅ done — `X3DAudioInitialize` after mastering voice; `SetListenerPosition`/`Compute3DVolume`/`Apply3DAttenuation` on `XAudio2Backend`; per-frame attenuation in `AudioSystem::Update` via `TransformComponent`; `--headless --scene audio_3d_test` 3-test CI |
| **1 — Now** | **M19: Action combat completion** | `combo_system.hpp/.cpp`; `combat_config.json` loader; `--headless --scene combat_test` CI |
| **5** | **M20: Quest/dialogue tools + tests** | Quest baker; `--headless --scene quest_test` + `dialogue_test` CI |
| **6** | **M21: Tool stubs** | Nav-mesh baker; tod.lut baker; pytest for each |
| **7** | **M22: Cinematic baker + editor panel** | `bake_cinematic()`; `CinematicEditorPanel` |
| **Future** | **M14: Vulkan catch-up** | Resume Vulkan work: vulkan_texture, vulkan_descriptor, Vulkan PBR, Vulkan skinning — implement all Vulkan DEFERRED items |

---

### M8.0 — Gameplay Integration Plan (D3D11 Runtime)

> **Goal:** Wire all terminal-only gameplay systems into the D3D11 `engine_sandbox` runtime so the vertical-slice project runs as a real-time 3D game on Windows.

Implementation order within M8:

| Sub-milestone | Deliverable | Key files |
|---------------|-------------|-----------|
| **M8.1 — D3D11 game loop** ✅ | Replace terminal `Game` main-loop driver with a D3D11-backed `GameRuntime` that ticks all ECS systems (Combat, AI, Quest, Weather, Magic, Shop, Camp) from `engine_sandbox` | `src/sandbox/game_runtime.hpp/.cpp`; wire into `main.cpp --scene game` |
| **M8.2 — Player entity + input** ✅ | Spawn player entity with `TransformComponent` + `HealthComponent` + `StatsComponent` + `MovementComponent` + `CombatComponent`; map Win32 keyboard events via `InputMapper` to component state | `src/game/systems/input_mapper.hpp/.cpp`; `Win32Window` key callback wired to `InputMapper` |
| **M8.3 — Camera system** ✅ | Third-person follow camera: `CameraComponent` + `CameraSystem`; outputs view/proj matrices consumed by `D3D11Renderer`; supports manual orbit via mouse drag | `src/engine/rendering/camera_system.hpp/.cpp`; `CameraComponent` added to ECS |
| **M8.4 — Enemy AI in 3D** ✅ | Port `AISystem` (FSM + A*) to work on 3D world positions (not 2D tile coords); adapt `TileMap` pathfinding grid to be generated from `WorldPartition` cell data | `src/game/systems/AISystem.cpp` — replace `TileCoord` with `Vec3` nav grid |
| **M8.5 — Combat HUD (ImGui)** ✅ | D3D11 ImGui overlay showing HP/MP bars, ATB gauge, equipped spell, party member icons; driven by ECS component reads on the player + party | `src/engine/ui/hud.hpp/.cpp`; wired in `D3D11Renderer::DrawFrame` after scene pass |
| **M8.6 — Quest + dialogue** ✅ | Implement `DialogueSystem` (missing stub); wire `QuestSystem` objective complete → HUD notification; add sample quest + NPC to vertical-slice cell | `src/game/systems/dialogue_system.hpp/.cpp`; update `cell_0_0.cell.json` |
| **M8.7 — Zone streaming → D3D11** ✅ | Replace stub `GameStreamingManager` test data with real vertical-slice cells; `GameStreamingManager` spawns `AnimatorComponent`-bearing NPCs + enemies whose meshes are rendered by D3D11 skinned-mesh pass | Update `AssetRegistry.json`; wire `GameStreamingManager` into `game_runtime` |
| **M8.8 — Save/load (15 slots)** ✅ | Production `SaveSystem`: JSON serialise full ECS `World` state, 15 numbered slots + auto-save slot; migration via `"version"` field; wired to CampSystem auto-save hook | `src/engine/save/save_system.hpp/.cpp`, `save_schema.hpp` |
| **M8.9 — CI acceptance scene** ✅ | `--scene m8_gameplay` headless run: spawn player + 3 enemies; tick 60 frames; assert player HP unchanged (no bugs), at least 1 AI state transition, quest objective registered | `main.cpp --scene m8_gameplay`; `build-windows.yml` job |

---

## Repository Layout & Folder Responsibilities

```
Game-Engine-for-Teaching-/
├── engine/             # Engine subdirectory README; source lives in src/
├── src/                # C++17 engine & game source code (existing, working)
│   ├── engine/         # Platform-independent engine kernel
│   │   ├── core/       # Logger, EventBus, Types
│   │   ├── ecs/        # Entity-Component-System (ECS.hpp — 2 000 lines)
│   │   ├── animation/  # M4 runtime: skeleton, anim_clip, blend_tree, animation_system, IK solver, GPU skinning
│   │   ├── math/       # math_types.hpp (Vec3, Quat, Mat4 — row-major D3D11)
│   │   ├── input/      # Input system (ncurses on Linux)
│   │   ├── physics/    # M5 runtime: physics_world, character_controller, raycast, hit_volume, rigid_body
│   │   ├── platform/   # Win32 window, message pump
│   │   ├── rendering/  # ncurses renderer + D3D11 renderer (+ Vulkan, optional)
│   │   └── scripting/  # Lua 5.4 embedding
│   ├── game/           # FFXV-style gameplay systems
│   │   ├── systems/    # Combat, AI, Camp, Inventory, Magic, Quest, Shop, Weather
│   │   └── world/      # TileMap, Zone, WorldMap
│   ├── sandbox/        # Windows D3D11 sandbox / test harness
│   └── main.cpp        # Terminal game entry point
├── editor/             # Dear ImGui editor (Creation Suite)
│   └── src/            # C++ source: EditorApp, ContentBrowserPanel, SceneEditorPanel
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
| `AudioSourceComponent` | Audio system (M3) | Already ✅ added |
| `AnimatorComponent` | Animation system (M4) | Already ✅ added — `skeletonID`, `currentClipID`, `blendTreeID`, `playbackSpeed`, `currentTime`, `jointMatrices[MAX_JOINTS]` |
| `RigidBodyComponent` | Physics system (M5) | Already ✅ added — component 22: `mass`, `isStatic`, `useGravity`, `linearDamping`, `bodyID` (opaque Jolt handle, uint32_t) |
| `ColliderComponent` | Physics system (M5) | Already ✅ added — component 23: `shapeType` (Box/Sphere/Capsule), `halfExtents`, `radius`, `isTrigger` |
| `VehicleComponent` | Vehicle system (Post-M10) | Already ✅ expanded — `throttleInput`, `brakeInput`, `steerInput`, `velocity`, `yaw`, `wheelStates[4]` (WheelState with spring compression/damping), `physicsBodyID`, full suspension tuning |

---

## CMake Integration Pattern

When adding a new `.cpp` file to the engine, follow this exact pattern:

**1. For a new engine subsystem (goes into `engine_sandbox`):**

```cmake
# In CMakeLists.txt — inside the if(WIN32 AND ...) block,
# add your file to SANDBOX_SOURCES (D3D11 block or unconditional):
list(APPEND SANDBOX_SOURCES
    src/engine/animation/skeleton.cpp      # ← add here (renderer-agnostic)
    src/engine/animation/anim_clip.cpp     # ← add here
)
```

**2. For a new D3D11-specific subsystem:**

```cmake
# Inside if(ENGINE_ENABLE_D3D11) — add to the D3D11 block:
list(APPEND SANDBOX_SOURCES
    src/engine/rendering/d3d11/d3d11_texture.cpp   # ← already present
)
# Also add d3dcompiler.lib if your file uses D3DCompile:
target_link_libraries(engine_sandbox PRIVATE d3d11.lib dxgi.lib d3dcompiler.lib)
```

**3. For a new gameplay system (goes into the terminal `game` target):**

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

**4. For a new D3D11 HLSL shader:**

```cmake
# In CMakeLists.txt — inside if(ENGINE_ENABLE_D3D11), add a POST_BUILD copy:
add_custom_command(TARGET engine_sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/shaders/textured_quad.vs.hlsl"
        "$<TARGET_FILE_DIR:engine_sandbox>/shaders/textured_quad.vs.hlsl"
    COMMENT "Copying HLSL shader to output"
)
# D3D11Renderer compiles HLSL at runtime via D3DCompileFromFile
# (requires d3dcompiler.lib which ships with the Windows SDK).
```

**4b. For a new Vulkan GLSL → SPIR-V shader (DEFERRED — do not add until Vulkan work resumes):**

```cmake
# In the GLSL_SHADERS list inside if(ENGINE_ENABLE_VULKAN):
set(GLSL_SHADERS
    "${SHADER_SOURCE_DIR}/triangle.vert"
    "${SHADER_SOURCE_DIR}/triangle.frag"
    # Add Vulkan shaders here when Vulkan work resumes
)
```

**5. For a new CMake option — pattern used for `ENGINE_ENABLE_PHYSICS` (M5, already done):**

```cmake
# ✅ Already implemented — shown here as the reference pattern for future options.
# Find the package quietly so missing SDK does not hard-fail the configure step:
option(ENGINE_ENABLE_PHYSICS "Build Jolt Physics integration (M5)" ON)
if(ENGINE_ENABLE_PHYSICS)
    find_package(Jolt CONFIG QUIET)   # ← Jolt is the CMake package name for joltphysics@5+
    if(Jolt_FOUND)
        message(STATUS "Jolt (JoltPhysics) found: physics subsystem ENABLED.")
    else()
        message(STATUS "Jolt NOT found — ENGINE_ENABLE_PHYSICS will be OFF.")
        set(ENGINE_ENABLE_PHYSICS OFF CACHE BOOL "" FORCE)
    endif()
endif()
# Link: target_link_libraries(engine_sandbox PRIVATE Jolt::Jolt)
# Define: target_compile_definitions(engine_sandbox PRIVATE ENGINE_ENABLE_PHYSICS)
```

---

## M5 Bootstrap Guide (Completed Milestone — Jolt Physics)

**M5 is ✅ complete.** All steps listed below are done. This section is kept as a reference for how the physics subsystem was integrated.

### ✅ Already Done (do NOT redo these)

- **Step 1 — `joltphysics` in vcpkg.json** ✅  
  `joltphysics` added to `vcpkg.json`. CI physics job installs via classic-mode vcpkg (no manifest) to avoid imgui docking conflict.

- **Step 2 — `ENGINE_ENABLE_PHYSICS` CMake option** ✅  
  `CMakeLists.txt`: `option(ENGINE_ENABLE_PHYSICS ... ON)` + `find_package(Jolt CONFIG QUIET)`. Physics .cpp files compiled only when `ENGINE_ENABLE_PHYSICS AND Jolt_FOUND`. CMake package name is `Jolt` (not `JoltPhysics`); target is `Jolt::Jolt`.

- **Step 3 — `physics_impl.hpp` pImpl header** ✅  
  `src/engine/physics/physics_impl.hpp` — `PhysicsWorldImpl` struct + Jolt layer filter classes. Only included by `physics_world.cpp`. Keeps Jolt headers out of all public APIs.

- **Step 4 — `PhysicsWorld` wrapper** ✅  
  `src/engine/physics/physics_world.hpp/.cpp` — `Init/Step/Shutdown`, `CreateBox/Sphere/Capsule`, `GetPosition/SetPosition/GetLinearVelocity/SetLinearVelocity`, `Raycast`.

- **Step 5 — `CharacterController`** ✅  
  `src/engine/physics/character_controller.hpp/.cpp` — `JPH::CharacterVirtual`; gravity, step-up (0.4 m), slope-slide (50°), jump impulse (5 m/s), `IsGrounded()`.

- **Step 6 — Raycast + ShapeCast helpers** ✅  
  `src/engine/physics/raycast.hpp/.cpp` — `CastRay`, `CastRayDown`, `CastSphere`; `ShapeCastHit` output struct.

- **Step 7 — `HitVolumeManager`** ✅  
  `src/engine/physics/hit_volume.hpp/.cpp` — AABB Attack/Hurt volumes; `Register/Unregister/SetActive/Update/QueryOverlaps`.

- **Step 8 — `RigidBodyCreator` + ECS components** ✅  
  `src/engine/physics/rigid_body.hpp/.cpp` — `RigidBodyCreator::Create/Destroy/SyncPositionFromPhysics/PushPositionToPhysics`. `RigidBodyComponent` (component 22) + `ColliderComponent` (component 23) added to `ECS.hpp`.

- **Step 9 — `physics_test` acceptance scene** ✅  
  `main.cpp`: `--scene physics_test` runs 3 tests (drop_sphere, step_ledge, raycast); exits `[PASS]` or `[FAIL]`.

- **Step 10 — CI job** ✅  
  `build-windows.yml` → `build-windows-physics` job: classic-mode vcpkg install, `windows-ninja-debug-physics` preset, `--scene physics_test` run.

---

## M3 Bootstrap Guide (Completed Milestone — D3D11 Texture + XAudio2)

### ✅ Already Done (do NOT redo these)

- **Step 1 — `directxtex` in vcpkg.json** ✅  
  `directxtex` and `imgui[docking,dx11-binding,win32-binding]` already present in `vcpkg.json`.  
  *Note: `directxtex` is in vcpkg.json for the future Vulkan path; D3D11 uses its own self-contained DDS parser.*

- **Step 2 — D3D11 texture loader** ✅  
  `src/engine/rendering/d3d11/d3d11_texture.hpp/.cpp` — self-contained DDS parser (RGBA8, BC1, BC3, BC7/DX10 header). No `directxtex` vcpkg dep needed. Wired into `CMakeLists.txt`.

- **Step 3 — XAudio2 backend** ✅  
  `src/engine/audio/xaudio2_backend.hpp/.cpp` — device init, master voice, 16-slot source voice pool, WAV parser, `Play`/`Stop`/`SetSlotVolume`. Linked via `xaudio2.lib` + `ole32.lib`.

- **Step 4 — Audio ECS system** ✅  
  `src/engine/audio/audio_system.hpp/.cpp` — ECS system iterating `AudioSourceComponent`, music FSM with real `SetVolume` crossfade. `AudioSourceComponent` added to `ECS.hpp`.

- **Step 5 — D3D11 textured quad HLSL shaders** ✅  
  `shaders/textured_quad.vs.hlsl` + `shaders/textured_quad.ps.hlsl` — SM4.0 HLSL vertex/pixel shaders. Copied to output by CMake POST_BUILD.

- **Step 6 — D3D11 textured quad scene** ✅  
  `D3D11Renderer::LoadScene("textured_quad", shaderDir)` — compiles HLSL at runtime via `D3DCompileFromFile`, creates quad VB/IB, loads DDS texture (or 1×1 white fallback), creates sampler. `DrawFrame` renders the quad; `RecordHeadlessFrame` validates the pipeline in headless CI mode. `d3dcompiler.lib` wired in `CMakeLists.txt`.

### ⬜ Deferred Vulkan Steps (do NOT implement until instructed)

The following were the original M3 Vulkan steps — they are recorded here for future reference only.

- **[DEFERRED] Vulkan texture** — `src/engine/rendering/vulkan/vulkan_texture.hpp/.cpp`  
  Load a DDS file via DirectXTex into `VkImage` + `VkImageView`. Support BC7 (`VK_FORMAT_BC7_UNORM_BLOCK`).

- **[DEFERRED] Vulkan descriptor sets** — `src/engine/rendering/vulkan/vulkan_descriptor.hpp/.cpp`  
  Wrap `VkDescriptorPool` + `VkDescriptorSetLayout` + `VkDescriptorSet`. Bind texture sampler to binding 0.

- **[DEFERRED] Vulkan textured quad shaders** — `shaders/textured_quad.vert/.frag`  
  GLSL pass-through UV vertex shader + `sampler2D` fragment shader. Add to `GLSL_SHADERS` in CMakeLists.

- **[DEFERRED] Vulkan `LoadScene("textured_quad")`** in `VulkanRenderer`  
  Create `VulkanTexture` + `VulkanDescriptor`, use textured quad pipeline.

- **[DEFERRED] Vulkan CI wiring**  
  `build-windows.yml` Vulkan job: `engine_sandbox.exe --renderer vulkan --headless --scene textured_quad`.

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

`vcpkg.json` exists in the repository root with all M2–M5 dependencies:

```json
// vcpkg.json — current state (M5 complete)
{
  "name": "educational-game-engine",
  "version-string": "1.0.0",
  "dependencies": [
    "nlohmann-json",          // M2 — cook.exe JSON parsing ✅
    "directxtex",             // Reserved for future Vulkan texture path (DEFERRED); D3D11 uses self-contained DDS parser
    {
      "name": "imgui",
      "features": ["docking", "dx11-binding", "win32-binding"]  // Editor ✅
    },
    "joltphysics"             // M5 — Jolt Physics ✅ (CI physics job uses classic-mode vcpkg install)
  ]
}
```

Planned additions:
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

# Build engine only (no Qt/external SDK required beyond Windows SDK):
cmake --preset windows-debug-engine-only
cmake --build --preset windows-debug-engine-only

# Build engine with Jolt Physics (requires joltphysics via vcpkg):
cmake --preset windows-ninja-debug-physics -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build --preset windows-ninja-debug-physics

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
| M3 | D3D11 texture + XAudio2 + D3D11 textured quad | ✅ |
| M4 | Animation runtime (C++) — CPU core | ✅ |
| M4b | IK solver + D3D11 GPU skinning CB | ✅ |
| M5 | Jolt Physics | ✅ |
| M6 | Editor inspector + Play-in-Engine | ✅ |
| M7 | World streaming (WorldStreamingManager, WorldPartition, AsyncLoader) | ✅ |
| M7.1 | Zone ↔ WorldStreamingManager wiring (`GameStreamingManager`) | ✅ |
| M7.2 | AssetLoader `.level` cooked cell integration | ✅ |
| M7.3 | Cancellation token in `AsyncLoader` (`CancelJob`) | ✅ |
| M7.4 | Frame-budget cap (`SetMaxCompletionsPerFrame`) | ✅ |
| M7.5 | ImGui streaming debug overlay + editor View menu toggle | ✅ |
| M8 | Wire all gameplay into D3D11 runtime (see M8.0 plan above) | ✅ |
| M9 | PBR Cook-Torrance BRDF (D3D11: GGX NDF, Smith G, Schlick F, Reinhard, UV sphere, headless CI) | ✅ |
| M10 | Dynamic sky + weather VFX (SkyRenderer, WeatherFx, sky.vs.hlsl, sky.ps.hlsl, 3-test headless CI) | ✅ |
| Post-M10 | Vehicle physics | ✅ |
| Post-M10 | Behaviour tree AI (BtTree, FormationSystem, NavMesh, bt_test CI) | ✅ |
| Post-M10 | SDF Font Renderer (FontRenderer, sdf_text.vs/ps.hlsl, font_test CI) + pak.exe (PAK1 packager) | ✅ |
| Post-M10 | Cinematics runtime (CinematicSequencer + CameraRig + cinematic_test CI) | ✅ |
| M13 partial | Cinematics tool (cut-scene baker) + editor panel | ⬜ (M22) |
| M16 | D3D11 depth buffer + IBL | ✅ |
| M17 | D3D11 shadow maps + bloom | ✅ |
| M18 | X3DAudio 3D positional audio | ✅ |
| M19 | Action combat completion (combo FSM + config + tests) | ⬜ |
| M20 | Quest/dialogue tools + tests | ⬜ |
| M21 | Nav-mesh baker + tod.lut baker | ⬜ |
| M22 | Cut-scene baker + cinematic editor panel | ⬜ |
| M23 | Vulkan catch-up | ⬜ Deferred — merged into M14, no separate M23 |

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
| **Tools** | Asset cooker (`cook.exe`); texture / mesh / audio / animation import pipeline; Dear ImGui scene editor with Play-in-Engine; Python authoring tools for audio and animation |
| **Teaching** | Every non-trivial pattern has a `// TEACHING NOTE` block; subsystem docs in `docs/`; `samples/vertical_slice_project/` demonstrates each subsystem end-to-end |

> **Note on content vs. quality:** The goal is not a content-complete copy of
> FF15.  It *is* a toolchain and engine where each technology category (visuals,
> physics, sound, gameplay) is implemented at the same *class* of solution FF15
> uses — real PBR, real physics, real positional audio, real action combat — so
> that a student studying this code is studying the same patterns a professional
> AAA studio uses.  Stubs or toy implementations do not satisfy this bar.

---

## Vulkan — Deferred Requirements Reference

> **Do NOT implement any of these items until explicitly instructed.**
> This section is a reference list for when Vulkan work resumes (Post-M8).
> All existing Vulkan code (`VulkanRenderer.cpp`, `vulkan_pipeline.cpp`, etc.) is kept as-is.

| Item | Files needed | Notes |
|------|-------------|-------|
| Vulkan texture (DDS/BC7) | `src/engine/rendering/vulkan/vulkan_texture.hpp/.cpp` | Use DirectXTex to load DDS; upload to `VkImage`; BC7 = `VK_FORMAT_BC7_UNORM_BLOCK` |
| Vulkan descriptor sets | `src/engine/rendering/vulkan/vulkan_descriptor.hpp/.cpp` | Wrap pool + layout + set; bind texture sampler at binding 0 |
| Vulkan textured quad shaders | `shaders/textured_quad.vert/.frag` | GLSL pass-through UV vertex + `sampler2D` fragment; add to `GLSL_SHADERS` |
| Vulkan textured quad scene | `VulkanRenderer::LoadScene("textured_quad", ...)` | Use vulkan_texture + vulkan_descriptor + textured quad pipeline |
| Vulkan depth buffer | In `VulkanRenderer::Init` | `VK_FORMAT_D32_SFLOAT`, `VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL` |
| Vulkan PBR pipeline | `src/engine/rendering/vulkan/pbr_pipeline.hpp/.cpp` | IBL + directional light; metallic-roughness workflow |
| Vulkan shadow map | Shadow render pass + shadow CB | `VK_FORMAT_D32_SFLOAT` offscreen; PCF shadow sampling in PBR shader |
| Vulkan post-processing | Full-screen pass pipeline | Bloom (extract + blur + composite) + Reinhard/ACES tonemap |
| Vulkan dynamic sky | Reuse `src/engine/rendering/sky_renderer.hpp/.cpp` + `weather_fx.hpp/.cpp` (already ✅ D3D11). Add `shaders/sky.vert/.frag` GLSL + Vulkan sky pipeline in `VulkanRenderer` | D3D11 CPU side is done; only Vulkan GPU pipeline deferred |
| Vulkan weather VFX | GPU particle rain + fog pass; reuse `WeatherFxState` from `weather_fx.hpp` (already ✅ D3D11) | CPU state done; Vulkan particle pipeline deferred |
| Vulkan GPU skinning | `shaders/skinned_mesh.vert/.frag` + UBO | joint matrix UBO; `VulkanRenderer::UploadJointMatrices()` |
| Vulkan HUD / menu stack | `src/engine/ui/` (Vulkan imgui binding) | imgui Vulkan backend; bind to Vulkan render pass |
| Vulkan `M8` gameplay wiring | All gameplay systems → `VulkanRenderer` | Wire combat, AI, quests, etc. into Vulkan render loop |
| Vulkan CI wiring | `.github/workflows/build-windows.yml` | Optional `build-windows-vulkan` job: `--renderer vulkan --headless --scene textured_quad` |

---

## North Star

> Build a complete, teachable FFXV-style open-world action RPG toolchain.
> Every commit must move the vertical slice forward.
> Prefer simple-but-real over fancy-but-incomplete.
> The project is finished when all 13 subsystems ship at FF15-comparable quality
> and every one can be fully taught from source code alone.
