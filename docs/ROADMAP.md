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
| Qt 6 editor scaffold (project browser + content browser + scene editor) | ✅ Done |
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

## Milestone 3 — Vulkan Textures + XAudio2 *(next — active)*

**Goal:** First textured 3D surface rendered on screen; audio plays from cooked assets.

| Item | Priority |
|------|----------|
| Vulkan texture loader: cooked DDS → `VkImage` + `VkImageView` (DirectXTex via vcpkg) | HIGH |
| Vulkan descriptor set: bind texture sampler to fragment shader | HIGH |
| Textured quad shaders (`shaders/textured_quad.vert/.frag`) | HIGH |
| `LoadScene("textured_quad")` in `VulkanRenderer` | HIGH |
| XAudio2 backend (`src/engine/audio/xaudio2_backend.hpp/.cpp`) | HIGH |
| Audio system (`src/engine/audio/audio_system.hpp/.cpp`) — ECS-driven play/stop | HIGH |
| `AudioSourceComponent` added to `ECS.hpp` | HIGH |
| Cook pipeline: texture cook stub → cooked DDS; audio cook stub → cooked WAV | MEDIUM |
| CI: headless `--scene textured_quad` validation | MEDIUM |

---

## Milestone 4 — Animation Runtime *(near-term)*

**Goal:** GPU-skinned skeletal animation running on screen.

| Item | Priority |
|------|----------|
| C++ skeleton runtime (`src/engine/animation/skeleton.hpp/.cpp`) | HIGH |
| C++ anim clip evaluation (`src/engine/animation/anim_clip.hpp/.cpp`) | HIGH |
| C++ blend tree (`src/engine/animation/blend_tree.hpp/.cpp`) | HIGH |
| GPU skinning UBO upload (`src/engine/animation/gpu_skinning.hpp/.cpp`) | HIGH |
| `AnimatorComponent` added to `ECS.hpp` | HIGH |
| Skinned mesh shaders (`shaders/skinned_mesh.vert/.frag`) | HIGH |
| Animation system ECS update (`src/engine/animation/animation_system.hpp/.cpp`) | MEDIUM |
| IK solver (`src/engine/animation/ik_solver.hpp/.cpp`) | LOW |

---

## Milestone 5 — Physics *(medium-term)*

**Goal:** Real rigid-body physics and character controller.

| Item | Priority |
|------|----------|
| Jolt Physics via vcpkg (`joltphysics`) | HIGH |
| `PhysicsWorld` wrapper (`src/engine/physics/physics_world.hpp/.cpp`) | HIGH |
| Character capsule controller (`src/engine/physics/character_controller.hpp/.cpp`) | HIGH |
| `RigidBodyComponent` + `ColliderComponent` added to `ECS.hpp` | HIGH |
| Hit volumes for combat (`src/engine/physics/hit_volume.hpp/.cpp`) | MEDIUM |
| Raycast interface (`src/engine/physics/raycast.hpp/.cpp`) | MEDIUM |

---

## Milestone 6 — Editor *(medium-term)*

**Goal:** Save a scene in the editor and immediately run it in the engine.

| Item | Priority |
|------|----------|
| Entity inspector / property editor (`editor/src/panels/inspector.hpp/.cpp`) | HIGH |
| Scene hierarchy panel (`editor/src/panels/scene_hierarchy.hpp/.cpp`) | HIGH |
| Scene ECS serialization (`src/engine/scene/scene_serialiser.hpp/.cpp`) | HIGH |
| "Play In Engine" button — launches `engine_sandbox.exe` with current scene | HIGH |
| Mesh placement in scene editor (drag from content browser) | MEDIUM |
| Undo/redo stack in scene editor | LOW |

---

## Milestone 7 — World Streaming *(medium-term)*

**Goal:** Open-world zone streaming without loading screens.

| Item | Priority |
|------|----------|
| `world_streaming` proximity-based loader (`src/engine/world/world_streaming.hpp/.cpp`) | HIGH |
| `world_partition` spatial grid (`src/engine/world/world_partition.hpp/.cpp`) | HIGH |
| Async loader worker thread (`src/engine/world/async_loader.hpp/.cpp`) | HIGH |
| No frame spikes during zone transition | HIGH |

---

## Milestone 8 — Gameplay Integration *(long-term)*

**Goal:** Wire all terminal gameplay systems into the Vulkan runtime.

| Item | Priority |
|------|----------|
| CombatSystem → Vulkan (replace ncurses rendering) | HIGH |
| AISystem → Vulkan + physics raycasts | HIGH |
| QuestSystem → Vulkan HUD | HIGH |
| WeatherSystem → sky renderer + weather VFX | MEDIUM |
| Dialogue system (`src/game/systems/dialogue_system.hpp/.cpp`) | MEDIUM |
| Production save system: 15 slots + auto-save + migration (`src/engine/save/`) | MEDIUM |

---

## Post-M8 Work

- PBR rendering (IBL + directional shadows + bloom + tonemap)
- Dynamic sky / procedural time-of-day
- Vehicle physics
- Behaviour tree AI (`src/engine/ai/behaviour_tree.hpp/.cpp`)
- Formation system (`src/engine/ai/formation_system.hpp/.cpp`)
- Cinematic sequencer + camera rig
- Vulkan HUD / menu stack
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
| Qt 6 for editor | Mature, cross-platform, rich tooling APIs |
| Python for authoring tools | Fast iteration, rich ecosystem (NumPy, SciPy) |
| JSON for all shared formats | Human-readable, no extra library on Windows |
| Shared schemas in `shared/` | Single source of truth for all data formats |
| Keep `src/` in place | Avoid breaking existing Linux terminal build |
| nlohmann-json via vcpkg | Header-only, widely used, easy to integrate |
| Synchronous AssetLoader first | Correct before concurrent; async deferred to M7 |
