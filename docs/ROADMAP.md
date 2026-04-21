# ROADMAP — Game Engine for Teaching (FFXV-Style Action RPG)

> **North Star:** Build a complete, teachable Final Fantasy XV-inspired open-world
> action RPG toolchain — with every subsystem documented for learning.
> "Prefer simple-but-real over fancy-but-incomplete."

---

## Milestone 1 — Monorepo Foundation ✅ *(complete)*

**Goal:** All repos under one roof with a working Windows build and clear teaching structure.

| Item | Status |
|------|--------|
| Monorepo layout (`engine/`, `editor/`, `tools/`, `shared/`, `samples/`) | ✅ Done |
| Root CMake superbuild + `CMakePresets.json` (Windows) | ✅ Done |
| `.github/copilot-instructions.md` | ✅ Done |
| Shared JSON schemas (7 formats) | ✅ Done |
| Shared runtime headers (`Guid.hpp`, `VersionedFile.hpp`) | ✅ Done |
| Dear ImGui editor scaffold (project browser + content browser + scene editor) — *Qt6 was replaced by Dear ImGui (MIT license, zero extra install)* | ✅ Done |
| Audio authoring tool vendored under `tools/audio_authoring/` | ✅ Done |
| Animation authoring tool vendored under `tools/anim_authoring/` | ✅ Done |
| Vertical slice sample project skeleton + cook script | ✅ Done |
| `docs/ARCHITECTURE.md` + `docs/ROADMAP.md` | ✅ Done |
| Updated `README.md` with monorepo build instructions | ✅ Done |
| Vulkan bootstrap (M0) + colored triangle (M1) | ✅ Done |

---

## Milestone 1.5 — D3D11 Baseline Renderer ✅ *(complete)*

**Goal:** Switch the default Windows renderer to D3D11 so the engine runs on GT610-era
hardware and CI runners without a Vulkan SDK.  Vulkan remains as an optional high-end
backend.

| Item | Status |
|------|--------|
| `IRenderer` abstract interface (`src/engine/rendering/IRenderer.hpp`) | ✅ Done |
| `D3D11Renderer` backend (`src/engine/rendering/d3d11/D3D11Renderer.hpp/.cpp`) | ✅ Done |
| `RendererFactory` factory (`src/engine/rendering/RendererFactory.hpp`) | ✅ Done |
| `--renderer d3d11\|vulkan` runtime flag in `engine_sandbox`; default D3D11 | ✅ Done |
| D3D11 WARP headless mode (no GPU/driver needed in CI) | ✅ Done |
| `ENGINE_ENABLE_D3D11` CMake option (ON by default on Windows) | ✅ Done |
| Vulkan `find_package` changed to QUIET / optional (no SDK = auto-disable) | ✅ Done |
| `windows-debug-engine-only` preset: D3D11 only, no Vulkan SDK required | ✅ Done |
| `windows-debug-vulkan` preset: D3D11 + Vulkan (SDK required) | ✅ Done |
| CI: `build-windows.yml` primary job uses D3D11 (no Vulkan SDK step) | ✅ Done |
| CI: `build-windows-vulkan` optional job validates Vulkan backend | ✅ Done |
| Hardware baseline: D3D_FEATURE_LEVEL_10_0 minimum (GT610 = FL 11_0) | ✅ Done |
| `README.md` updated — D3D11 default, Vulkan optional high-end | ✅ Done |
| `ROADMAP.md` updated with Milestone 1.5 | ✅ Done |

---

## Milestone 2 — Import & Cook Pipeline ✅ *(complete)*

**Goal:** A complete round-trip: import source asset → cook → engine loads it.

| Item | Status |
|------|--------|
| `cook.exe` C++ standalone cooker (`src/tools/cook/cook_main.cpp`) | ✅ Done |
| `AssetDB` runtime (`src/engine/assets/asset_db.hpp/.cpp`) | ✅ Done |
| `AssetLoader` runtime (`src/engine/assets/asset_loader.hpp/.cpp`) | ✅ Done |
| `engine_sandbox --validate-project` exits 0 | ✅ Done |
| `vcpkg.json` with `nlohmann-json` dependency | ✅ Done |
| Golden-file tests (`tests/golden/`) + 13 pytest cook pipeline tests | ✅ Done |
| CI: Windows build + headless (`.github/workflows/build-windows.yml`) | ✅ Done |
| CI: Contract tests (`.github/workflows/contract-tests.yml`) | ✅ Done |
| CI: Architecture lint (`.github/workflows/architecture-lint.yml`) | ✅ Done |

---

## Milestone 3 — Vulkan Textures + XAudio2 ✅ *(complete)*

**Goal:** First textured 3D surface rendered on screen; audio plays from cooked assets.

| Item | Status | Priority |
|------|--------|----------|
| D3D11 texture loader: DDS → `ID3D11ShaderResourceView` (`d3d11_texture.hpp/.cpp`) | ✅ Done | HIGH |
| XAudio2 backend (`src/engine/audio/xaudio2_backend.hpp/.cpp`) | ✅ Done | HIGH |
| Audio system (`src/engine/audio/audio_system.hpp/.cpp`) — ECS-driven play/stop + music FSM | ✅ Done | HIGH |
| `AudioSourceComponent` added to `ECS.hpp` | ✅ Done | HIGH |
| `directxtex` + `imgui` added to `vcpkg.json` | ✅ Done | HIGH |
| D3D11 textured quad shaders (`shaders/textured_quad.vs.hlsl` / `textured_quad.ps.hlsl`) | ✅ Done | HIGH |
| D3D11 textured quad scene (`LoadScene("textured_quad")` in `D3D11Renderer`) | ✅ Done | HIGH |
| CI: headless `--scene textured_quad` validation | ✅ Done | MEDIUM |
| Vulkan texture loader (`vulkan_texture.hpp/.cpp`) | ⬜ **DEFERRED** | — |
| Vulkan descriptor set (`vulkan_descriptor.hpp/.cpp`) | ⬜ **DEFERRED** | — |
| Vulkan textured quad shaders (`shaders/textured_quad.vert/.frag`) | ⬜ **DEFERRED** | — |

---

## Milestone 4 — Animation Runtime ✅ *(complete)*

**Goal:** GPU-skinned skeletal animation running on screen.

| Item | Status |
|------|--------|
| C++ skeleton runtime (`src/engine/animation/skeleton.hpp/.cpp`) | ✅ Done |
| C++ anim clip evaluation (`src/engine/animation/anim_clip.hpp/.cpp`) | ✅ Done |
| C++ blend tree (`src/engine/animation/blend_tree.hpp/.cpp`) | ✅ Done |
| GPU skinning CB upload (`src/engine/animation/gpu_skinning.hpp/.cpp`) — D3D11 path | ✅ Done |
| IK solver (`src/engine/animation/ik_solver.hpp/.cpp`) — TwoBone + FABRIK | ✅ Done |
| `AnimatorComponent` added to `ECS.hpp` | ✅ Done |
| D3D11 skinned mesh shaders (`shaders/skinned_mesh.vs.hlsl` / `skinned_mesh.ps.hlsl`) | ✅ Done |
| D3D11 `skinned_mesh` scene in `D3D11Renderer` | ✅ Done |
| Animation system ECS update (`src/engine/animation/animation_system.hpp/.cpp`) | ✅ Done |
| CI: headless `--scene skinned_mesh` validation | ✅ Done |
| Vulkan GPU skinning (`shaders/skinned_mesh.vert/.frag`) | ⬜ **DEFERRED** | 

---

## Milestone 5 — Physics ✅ *(complete)*

**Goal:** Real rigid-body physics and character controller.

| Item | Status |
|------|--------|
| Jolt Physics via vcpkg (`joltphysics`) | ✅ Done |
| `PhysicsWorld` wrapper (`src/engine/physics/physics_world.hpp/.cpp`) | ✅ Done |
| Character capsule controller (`src/engine/physics/character_controller.hpp/.cpp`) | ✅ Done |
| `RigidBodyComponent` + `ColliderComponent` added to `ECS.hpp` | ✅ Done |
| Hit volumes for combat (`src/engine/physics/hit_volume.hpp/.cpp`) | ✅ Done |
| Raycast interface (`src/engine/physics/raycast.hpp/.cpp`) | ✅ Done |
| `physics_test` headless acceptance scene (3 tests: drop_sphere, step_ledge, raycast) | ✅ Done |
| CI: `build-windows-physics` job (classic-mode vcpkg, `--scene physics_test`) | ✅ Done |

---

## Milestone 6 — Editor ✅ *(complete)*

**Goal:** Save a scene in the editor and immediately run it in the engine.

| Item | Status | Priority |
|------|--------|----------|
| Entity inspector / property editor (`editor/src/panels/InspectorPanel.hpp/.cpp`) | ✅ Done | HIGH |
| Scene hierarchy panel (`editor/src/panels/SceneHierarchyPanel.hpp/.cpp`) | ✅ Done | HIGH |
| Scene ECS serialization (`src/engine/scene/scene_serialiser.hpp/.cpp`) | ✅ Done | HIGH |
| "Play In Engine" button — saves temp scene + launches `engine_sandbox.exe` | ✅ Done | HIGH |
| Headless editor CLI (`--headless`, `--create-scene`, `--load-scene --validate`) | ✅ Done | HIGH |
| Load Scene from menu (`File > Load Scene...`) | ✅ Done | MEDIUM |
| Asset drag-drop from ContentBrowser → SceneEditor canvas | ✅ Done | MEDIUM |
| Undo/redo stack in scene editor | ⬜ | LOW |

---

## Milestone 7 — World Streaming ✅ *(complete)*

**Goal:** Open-world zone streaming without loading screens.

| Item | Status |
|------|--------|
| `world_streaming` proximity-based loader (`src/engine/world/world_streaming.hpp/.cpp`) | ✅ Done |
| `world_partition` spatial grid (`src/engine/world/world_partition.hpp/.cpp`) | ✅ Done |
| Async loader worker thread (`src/engine/world/async_loader.hpp/.cpp`) | ✅ Done |
| Frame-budget cap (`SetMaxCompletionsPerFrame`) | ✅ Done |
| `GameStreamingManager` — Zone lifecycle wiring (`src/game/world/`) | ✅ Done |
| AssetLoader `.level` cooked cell integration (M7.2) | ✅ Done |
| Cancellation token in `AsyncLoader` (`CancelJob`) (M7.3) | ✅ Done |
| ImGui streaming debug overlay + editor View menu toggle (M7.5) | ✅ Done |
| `streaming_load` / `streaming_evict` / `streaming_async` headless CI scenes | ✅ Done |

---

## Milestone 8 — Gameplay Integration ✅ *(complete)*

**Goal:** Wire all terminal gameplay systems into the D3D11 runtime (Vulkan catch-up is Post-M8).

| Item | Status |
|------|--------|
| `GameRuntime` D3D11 game-loop driver (M8.1) | ✅ Done |
| `InputMapper`: Win32 key events → ECS state (M8.2) | ✅ Done |
| `CameraSystem`: third-person follow + mouse orbit (M8.3) | ✅ Done |
| D3D11 ImGui HUD: HP/MP bars, ATB, equipped spell (M8.5) | ✅ Done |
| `DialogueSystem` + QuestSystem HUD notifications + NPC sample cell (M8.6) | ✅ Done |
| `GameStreamingManager` → D3D11; `AnimatorComponent` on streamed entities; 3 new cells (M8.7) | ✅ Done |
| `SaveSystem`: 15 slots + auto-save + `"version"` migration field (M8.8) | ✅ Done |
| `--scene m8_gameplay` headless CI acceptance test (M8.9) | ✅ Done |
| `--scene m8_streaming` headless CI acceptance test (M8.7) | ✅ Done |

---

## Post-M8 Work

- PBR rendering (IBL + directional shadows + bloom + tonemap) — D3D11 first, then Vulkan
- Dynamic sky / procedural time-of-day + weather VFX
- Vehicle physics (`VehicleComponent` + wheel-ray suspension)
- Behaviour tree AI (`src/engine/ai/behaviour_tree.hpp/.cpp`)
- Formation system (`src/engine/ai/formation_system.hpp/.cpp`)
- Cinematic sequencer + camera rig
- Vulkan catch-up: all DEFERRED items (vulkan_texture, vulkan_descriptor, Vulkan PBR, Vulkan skinning)
- Nav-mesh generation + runtime pathfinding
- PAK file packager

---

## Ongoing Work (every milestone)

- Add `// TEACHING NOTE` comments to every new subsystem
- Update `docs/ARCHITECTURE.md` when new systems are added
- Keep `samples/vertical_slice_project/` buildable and runnable
- Write tests: C++ test targets + Python pytest
- Keep all CI green: Architecture Lint + Linux Build + Windows Build + Contract Tests + Asset Validation

---

## Decisions Log

| Decision | Rationale |
|----------|-----------|
| **D3D11 as default Windows renderer** | GT610-era hardware compatibility; ships with Windows (no SDK install); WARP software rasteriser enables CI on any runner without a GPU driver |
| **Vulkan as optional high-end backend** | Modern explicit API for students learning low-level GPU programming; kept in codebase alongside D3D11 |
| **D3D_FEATURE_LEVEL_10_0 as hard minimum** | Covers GPUs from 2006+; GeForce GT 610 supports FL 11_0 |
| Windows-first | D3D11 + D3D11 WARP + Visual Studio is the primary teaching target |
| **Dear ImGui for editor** (replaced Qt6) | Dear ImGui is MIT-licensed, ships as source, has zero extra install, and is the industry standard for game engine tooling (used by Unity, Unreal, and virtually every AAA engine). Qt6 was originally chosen for its rich widget set but was replaced because it requires a separate SDK installation and its LGPL license adds complexity. |
| Python for authoring tools | Fast iteration, rich ecosystem (NumPy, SciPy) |
| JSON for all shared formats | Human-readable, no extra library on Windows |
| Shared schemas in `shared/` | Single source of truth for all data formats |
| Keep `src/` in place | Avoid breaking existing Linux terminal build |
| nlohmann-json via vcpkg | Header-only, widely used, easy to integrate |
| Synchronous AssetLoader first | Correct before concurrent; async deferred to M7 |
