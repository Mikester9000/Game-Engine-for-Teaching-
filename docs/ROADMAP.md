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

## Post-M8 Work — Completed

- ✅ PBR rendering (Cook-Torrance BRDF, UV sphere, 3 CBs, Reinhard tonemap) — D3D11 (M9)
- ✅ Dynamic sky + weather VFX (SkyRenderer, WeatherFx, sky.vs.hlsl, sky.ps.hlsl) — D3D11 (M10)

## Post-M10 Work — Completed

- ✅ **M11** Vehicle physics (`VehicleComponent` + wheel-ray suspension + Ackermann steer + fuel + vehicle chase camera + `--scene vehicle_test` CI)
- ✅ **M12** Behaviour tree AI (`BtTree`/`BtSequence`/`BtSelector`/`BtCondition`/`BtAction`/`BtBlackboard`) + `FormationSystem` (LINE/V_SHAPE/CIRCLE) + grid `NavMesh` A* + `--scene bt_test` CI
- ✅ **M13-partial** Cinematics — `CinematicSequencer` + `CameraRig` + `--scene cinematic_test` CI; **cut-scene baker tool + editor panel remain ⬜**
- ✅ **M15** PAK Packager — `src/tools/pak/pak_main.cpp`; PAK1 binary format; `--input`/`--list`/`--extract`; CI test
- ✅ SDF Font Renderer — `FontRenderer` (SDF atlas, R8_UNORM D3D11 texture, `sdf_text.vs/ps.hlsl`, `--scene font_test` CI)

## Remaining Work — Required for Definition of Done

The following items are needed to satisfy the "Project Completion Definition" in
`docs/FF15_REQUIREMENTS_BLUEPRINT.md`.  All D3D11-first per active policy.

| Milestone | Feature | Key deliverables |
|-----------|---------|-----------------|
| **M16** | D3D11 depth buffer + IBL | DSV + DepthStencilState; PBR texture maps; irradiance cubemap + prefiltered env + BRDF LUT; `--headless --scene pbr_ibl` CI |
| **M17** | D3D11 shadow maps + bloom | Shadow pass render target; PCF shadow sampling in `pbr_mesh.ps.hlsl`; bright-pass + Gaussian blur + composite pass; `--headless --scene shadow_test` + `--scene bloom_test` CI |
| **M18** | X3DAudio 3D positional audio | X3DAudio init in `XAudio2Backend`; `Update3DListener()` + per-voice emitter; distance rolloff applied to voice volume; `--headless --scene audio_3d_test` CI |
| **M19** | Action combat completion | `combo_system.hpp/.cpp` input-driven combo FSM; `combat_config.json` external data loader; `--headless --scene combat_test` CI (1 000 damage samples + warp-strike + link-strike) |
| **M20** | Quest/dialogue tools + tests | `creation_engine.py` quest baker; `--headless --scene quest_test` CI (start→complete→reward cycle; prereq guard) |
| **M21** | Missing tool stubs | Nav-mesh baker (`.obj` → `.navmesh`); tod.lut baker (`tod.json` → `cooked/environment/tod.lut`) |
| **M22** | Cinematic tool + editor | Cut-scene baker (timeline JSON → cooked `.cinematic`); cinematic editor panel in Dear ImGui editor |
| **M14** | Vulkan catch-up | All DEFERRED Vulkan items: textures, descriptors, depth buffer, PBR, skinning, sky, HUD (no separate M23) |

---

## Milestone 9 — PBR Rendering (Cook-Torrance BRDF) ✅ *(complete)*

**Goal:** Physically-based rendering pipeline for D3D11 — the same Cook-Torrance
BRDF class used in Final Fantasy XV's Luminous engine.

| Item | Status |
|------|--------|
| `shaders/pbr_mesh.vs.hlsl` — SM 4.0 vertex shader (model→world→clip, inverse-transpose normal) | ✅ Done |
| `shaders/pbr_mesh.ps.hlsl` — GGX NDF + Smith geometry + Schlick Fresnel + Reinhard tonemap + gamma | ✅ Done |
| `D3D11Renderer::PBRScene` struct + `LoadScene("pbr_mesh")` + `DrawPBRMesh()` | ✅ Done |
| UV sphere geometry (16×16 stacks/slices, 289 verts, 1536 indices) | ✅ Done |
| 3 constant buffers: perFrameCB (b0), lightCB (b1), materialCB (b2) | ✅ Done |
| `--scene pbr_mesh` in `engine_sandbox`; `--headless --scene pbr_mesh` CI | ✅ Done |
| CI: `build-windows.yml` WARP headless smoke test | ✅ Done |

---

## Milestone 10 — Dynamic Sky + Weather VFX ✅ *(complete)*

**Goal:** Procedural time-of-day sky with weather effects (fog, rain, cloud cover)
matching the visual fidelity of FF15's Duscae region outdoor lighting.

| Item | Status |
|------|--------|
| `src/engine/rendering/sky_renderer.hpp/.cpp` — time-of-day clock, sun direction, zenith/horizon colour phases, sunset/sunrise tint, cloud darkening | ✅ Done |
| `src/engine/rendering/weather_fx.hpp/.cpp` — `WeatherType` (Clear/Cloudy/Rain/Storm), `WeatherFxState`, smooth lerp transitions | ✅ Done |
| `shaders/sky.vs.hlsl` — SV_VertexID full-screen triangle (no VB needed) | ✅ Done |
| `shaders/sky.ps.hlsl` — exponential gradient + sun disc + glow + fog + rain darkening + Reinhard + gamma (SM 4.0) | ✅ Done |
| `D3D11Renderer::SkyScene` + `LoadSkyScene()` + `DrawSky()` + `RecordHeadlessFrame()` sky path | ✅ Done |
| `--scene dynamic_sky` in `engine_sandbox`; 3-test headless validation | ✅ Done |
| CI: `build-windows.yml` — `dynamic_sky` WARP headless acceptance (Test 1: GPU pipeline; Test 2: sun elevation; Test 3: fog delta) | ✅ Done |

---

## Milestone 11 — Vehicle Physics ✅ *(complete — Post-M10)*

**Goal:** The Regalia car traverses roads at speed with wheel-ray physics and a spring-arm chase camera.

| Item | Status |
|------|--------|
| `VehicleComponent` — throttle, brake, steerAngle, 4-wheel `WheelState` (spring compression, damping, contact), currentSpeed, fuel | ✅ |
| `src/engine/vehicle/vehicle_system.hpp/.cpp` — input → torque/steer; wheel-ray suspension; spring-damper; Ackermann steering; fuel drain; ECS integration | ✅ |
| Vehicle chase camera in `src/engine/rendering/camera_system.hpp/.cpp` | ✅ |
| `--scene vehicle_test` headless CI: apply throttle 5 s; assert velocity > 0; position advances | ✅ |
| Road spline baker tool (`tools/creation_engine.py` bake spline JSON → cooked road) | ⬜ (M21) |

---

## Milestone 12 — Behaviour Tree AI ✅ *(complete — Post-M10)*

**Goal:** Replace/augment the FSM-based `AISystem` with a proper behaviour tree so
party members and bosses can execute complex multi-step strategies.

| Item | Status |
|------|--------|
| `src/engine/ai/behaviour_tree.hpp/.cpp` — `BtTree`, `BtSequence`, `BtSelector`, `BtCondition`, `BtAction`, `BtBlackboard` | ✅ |
| `src/engine/ai/formation_system.hpp/.cpp` — LINE/V_SHAPE/CIRCLE slot layouts, greedy assignment, world-space transform | ✅ |
| `src/engine/ai/nav_mesh.hpp/.cpp` — grid `NavMesh`, `BakeFromGrid`, A\* `FindPath` (4-dir + diagonal, obstacle routing) | ✅ |
| `--scene bt_test` headless CI: 4 tests — BT sequence/selector, blackboard, formation offsets, nav-mesh A\* | ✅ |
| Nav-mesh baker tool (`tools/creation_engine.py` bake `.obj` → cooked `.navmesh`) | ⬜ (M21) |

---

## Milestone 13 — Cinematics 🔨 *(runtime + tests complete; tool + editor panel pending)*

**Goal:** In-engine cut-scenes with camera choreography, character animation, and timed audio.

| Item | Status |
|------|--------|
| `src/engine/cinematics/cinematic_sequencer.hpp/.cpp` — timeline, shot/cut orchestrator, `ApplyToCamera()` ECS write, `OnShotChanged`/`OnComplete` callbacks | ✅ |
| `src/engine/cinematics/camera_rig.hpp/.cpp` — keyframed camera path, binary-search Lerp, `Duration()` / `Evaluate()` | ✅ |
| `CameraComponent.cinematicOverride` + `cinematicEyePos` / `cinematicLookAt` in `ECS.hpp` | ✅ |
| `--scene cinematic_test` headless CI: 3 tests (camera position accuracy, shot advance, callback) | ✅ |
| `tools/creation_engine.py` — cut-scene baker (timeline JSON → cooked `.cinematic`) | ⬜ (M22) |
| Cinematic editor panel in `editor/` (shot timeline, add/remove keyframes) | ⬜ (M22) |

---

## Milestone 14 — Vulkan Catch-up ⬜ *(not started)*

**Goal:** Bring the Vulkan backend to parity with D3D11 (textures, PBR, GPU skinning, sky).

| Item | Status |
|------|--------|
| `src/engine/rendering/vulkan/vulkan_texture.hpp/.cpp` — DDS/BC7 → `VkImage` (DirectXTex) | ⬜ |
| `src/engine/rendering/vulkan/vulkan_descriptor.hpp/.cpp` — pool + layout + set; sampler at binding 0 | ⬜ |
| `shaders/textured_quad.vert/.frag` — GLSL UV quad shaders; add to `GLSL_SHADERS` | ⬜ |
| Vulkan depth buffer — `VK_FORMAT_D32_SFLOAT` in `VulkanRenderer::Init` | ⬜ |
| Vulkan PBR pipeline — `src/engine/rendering/vulkan/pbr_pipeline.hpp/.cpp` | ⬜ |
| `shaders/skinned_mesh.vert/.frag` — GLSL skinning shaders; Vulkan UBO for joint matrices | ⬜ |
| `shaders/sky.vert/.frag` — GLSL sky shaders; Vulkan sky pipeline in `VulkanRenderer` | ⬜ |
| Vulkan HUD — imgui Vulkan backend bound to Vulkan render pass | ⬜ |
| `build-windows-vulkan` CI job: `--renderer vulkan --headless --scene textured_quad` | ⬜ |

---

## Milestone 15 — PAK Packager + Release ✅ *(complete — Post-M10)*

**Goal:** One-command distribution build: all cooked assets packed into a `.pak` archive
with the engine executable; `--validate-project` exits 0 on the packed output.

| Item | Status |
|------|--------|
| `src/tools/pak/pak_main.cpp` — PAK1 packager (directory → `.pak` archive with index; PAK1 magic + TOC + blobs) | ✅ |
| `--input`/`--output`/`--list`/`--extract` CLI modes; path traversal protection | ✅ |
| CI acceptance test in `build-windows.yml` | ✅ |
| `AssetDB` PAK mount mode — transparent path resolution from `.pak` at runtime | ⬜ (future) |

---

## Milestone 16 — D3D11 Depth Buffer + IBL ⬜ *(not started — required for Definition of Done)*

**Goal:** Add a proper depth buffer to D3D11Renderer and implement Image-Based Lighting
(IBL) so the PBR pipeline has physically correct ambient lighting from an environment map.
This is required by the "visuals quality bar" in the Project Completion Definition.

> **D3D11 policy:** This is D3D11-only. Vulkan IBL is deferred to M14.

| Item | Status |
|------|--------|
| D3D11 depth buffer — create `ID3D11Texture2D` + `ID3D11DepthStencilView` + `ID3D11DepthStencilState`; bind DSV in `DrawFrame`; resolve in `RecreateSwapchain` | ⬜ |
| Extend `src/engine/rendering/d3d11/d3d11_texture.hpp/.cpp` DDS support to include BC5 (`DXGI_FORMAT_BC5_UNORM`) and R8 (`DXGI_FORMAT_R8_UNORM`) so M16 PBR textures match the runtime loader's supported formats | ⬜ |
| PBR texture maps — albedo (BC1/BC7), metallic-roughness (BC5), normal (BC5), AO (R8); create D3D11 sampler + SRVs in `PBRScene`; update `pbr_mesh.ps.hlsl` to sample textures after BC5/R8 loader support is in place | ⬜ |
| IBL irradiance cubemap — precomputed diffuse irradiance; load DDS cubemap array; `shaders/ibl_irradiance.cs.hlsl` offline bake tool or pre-baked DDS asset | ⬜ |
| IBL prefiltered environment map — split-sum approximation (Epic Games method); roughness-mipped specular cube | ⬜ |
| IBL BRDF LUT — precomputed `IntegrateBRDF` table; R16G16 texture; use in `pbr_mesh.ps.hlsl` | ⬜ |
| Update `pbr_mesh.ps.hlsl` — combine direct PBR with IBL ambient terms; bind irradiance cube + prefiltered cube + BRDF LUT at t3/t4/t5 | ⬜ |
| `--headless --scene pbr_ibl` CI: validate IBL SRVs bound; no D3D11 validation errors | ⬜ |

**CI validation command (target):**
```
engine_sandbox.exe --headless --scene pbr_ibl
```
Expected: `[PASS] pbr_ibl: IBL pipeline OK (WARP headless).`

---

## Milestone 17 — D3D11 Shadow Maps + Bloom ⬜ *(not started — required for Definition of Done)*

**Goal:** Add directional shadow mapping (PCF) and a bloom post-processing pass.
Both are required by the "visuals quality bar" in the Project Completion Definition.

> **D3D11 policy:** D3D11-only. Requires depth buffer from M16.

| Item | Status |
|------|--------|
| Shadow map render target — `ID3D11Texture2D` (1024×1024 `DXGI_FORMAT_R32_TYPELESS`) + DSV + SRV; `shaders/shadow_pass.vs.hlsl`; shadow CB (light view/proj) | ⬜ |
| Shadow sampling in `pbr_mesh.ps.hlsl` — PCF 3×3 tap; `ShadowMap` SRV at t6; compare sampler (`D3D11_COMPARISON_LESS`) | ⬜ |
| `D3D11Renderer::DrawShadowPass()` — render opaque geometry from light POV each frame | ⬜ |
| Bloom bright-pass — threshold extract to `ID3D11Texture2D` offscreen RT; `shaders/bloom_bright.ps.hlsl` | ⬜ |
| Bloom Gaussian blur — horizontal + vertical ping-pong passes; `shaders/bloom_blur.ps.hlsl` | ⬜ |
| Bloom composite — additive blend of bloom texture over scene; `shaders/bloom_composite.ps.hlsl` | ⬜ |
| `--headless --scene shadow_test` CI: render sphere + floor; sample shadow map; assert shadow factor < 1.0 in umbra | ⬜ |
| `--headless --scene bloom_test` CI: render bright quad; verify bloom output pixels > 0 in blur region | ⬜ |

**CI validation commands (target):**
```
engine_sandbox.exe --headless --scene shadow_test
engine_sandbox.exe --headless --scene bloom_test
```

---

## Milestone 18 — X3DAudio Positional 3D Audio ⬜ *(not started — required for Definition of Done)*

**Goal:** Implement true 3D positional audio using X3DAudio (distance rolloff + stereo panning).
The `AudioSourceComponent.is3D` field and `maxDistance` are already scaffolded in the ECS;
this milestone wires them into X3DAudio calculations.

| Item | Status |
|------|--------|
| X3DAudio init in `XAudio2Backend::Init` — `X3DAudioInitialize()` with speed-of-sound | ⬜ |
| Listener state update — `XAudio2Backend::Update3DListener(Vec3 pos, Vec3 forward, Vec3 up)` | ⬜ |
| Per-voice emitter — `X3DAUDIO_EMITTER` struct per `AudioSourceComponent` with `is3D=true` | ⬜ |
| DSP settings — `X3DCalculate()` → `DSPSettings`; apply to source voice via `SetOutputMatrix` | ⬜ |
| Distance rolloff — linear or inverse-square falloff clamped to `maxDistance` | ⬜ |
| `AudioSystem::Update` — pass listener position from `CameraComponent`; iterate 3D sources | ⬜ |
| `--headless --scene audio_3d_test` CI: place emitter at `maxDistance`; assert volume ≤ 0.05 | ⬜ |

**CI validation command (target):**
```
engine_sandbox.exe --headless --scene audio_3d_test
```

---

## Milestone 19 — Action Combat Completion ⬜ *(not started — required for Definition of Done)*

**Goal:** Complete the action combat runtime: add the missing combo state machine, externalise
combat tuning data, and add headless acceptance tests.

| Item | Status |
|------|--------|
| `src/game/systems/combo_system.hpp/.cpp` — input-driven combo FSM; defines valid button sequences (e.g. ▲▲▲, ▲△▲△); integrates with `InputMapper` and `CombatSystem` | ⬜ |
| `combat_config.json` — externalized damage formulae, ATB base speed, per-combo hit count, status effect durations | ⬜ |
| `CombatSystem` loader — `LoadConfig(path)` reads `combat_config.json` at startup via `AssetLoader` | ⬜ |
| Physics hit detection wiring — `HitVolumeManager::QueryOverlaps` called from `ComboSystem` on each combo swing frame | ⬜ |
| `--headless --scene combat_test` CI: 1 000 damage samples within expected range; warp-strike position offset ✓; link-strike trigger ✓ | ⬜ |

---

## Milestone 20 — Quest / Dialogue Authoring Tools + Tests ⬜ *(not started — required for Definition of Done)*

**Goal:** Add the missing "Tool" column items for the Quests & Objectives subsystem: a baker that
produces cooked quest data and a headless acceptance test validating quest lifecycle.

| Item | Status |
|------|--------|
| `shared/schemas/quest.schema.json` — JSON Schema for `quests/*.quest.json` source format | ⬜ |
| `tools/creation_engine.py` — `bake_quest(path)` converts `*.quest.json` → `cooked/quests/<id>.quest` | ⬜ |
| `--headless --scene quest_test` CI: start quest → complete objective → assert reward applied; prereq guard | ⬜ |
| `--headless --scene dialogue_test` CI: walk dialogue tree to terminal node; assert NPC state flag | ⬜ |

---

## Milestone 21 — Tool Stubs: Nav-Mesh Baker + ToD LUT Baker ⬜ *(not started — required for Definition of Done)*

**Goal:** Implement the two authoring tool stubs that keep the Party AI and Weather matrix rows at ⬜.

| Item | Status |
|------|--------|
| `tools/creation_engine.py` — `bake_navmesh(obj_path, out_path)`: parse `.obj` geometry, mark walkable cells, write binary `cooked/ai/<id>.navmesh` | ⬜ |
| `tools/creation_engine.py` — `bake_tod(json_path, out_path)`: read `environment/tod.json` colour curves, sample at 256 time steps, write `cooked/environment/tod.lut` (256 × RGBA8) | ⬜ |
| Pytest for each baker: bake sample input → assert output exists + expected size | ⬜ |

---

## Milestone 22 — Cut-Scene Baker + Cinematic Editor Panel ⬜ *(not started — required for Definition of Done)*

**Goal:** Add the authoring tool and editor panel for the Cinematics subsystem.

| Item | Status |
|------|--------|
| `shared/schemas/cinematic.schema.json` — JSON Schema for `scenes/*.cinematic.json` source format (shots array, keyframes, audio events) | ⬜ |
| `tools/creation_engine.py` — `bake_cinematic(json_path, out_path)`: validate JSON, serialise shot list to binary `cooked/scenes/<id>.cinematic` | ⬜ |
| Cinematic editor panel (`editor/src/panels/CinematicEditorPanel.hpp/.cpp`) — timeline strip, shot add/remove/reorder, keyframe property editor, preview playback in editor viewport | ⬜ |

---

> **Note:** Vulkan catch-up is tracked as **Milestone 14** (see the M14 section above).
> There is no separate M23; all Vulkan deferred items are consolidated under M14.

---

## Ongoing Work (every milestone)

- Add `// TEACHING NOTE` comments to every new subsystem
- Update `docs/ARCHITECTURE.md` when new systems are added
- Keep `samples/vertical_slice_project/` buildable and runnable
- Write tests: C++ test targets + Python pytest
- Keep all CI green: Architecture Lint + Linux Build + Windows Build + Contract Tests + Asset Validation

---

## Milestone Progress Summary

> **Last verified: 2026-04-22** — deep reconciliation pass.

| Milestone | Name | Status |
|-----------|------|--------|
| M0 | Vulkan sandbox | ✅ Complete |
| M1 | Colored triangle (Vulkan SPIR-V) | ✅ Complete |
| M1.5 | D3D11 baseline renderer | ✅ Complete |
| M2 | AssetDB + Cooker | ✅ Complete |
| M3 | Hello Texture + Audio | ✅ Complete |
| M4 | Animation runtime (CPU + GPU skinning) | ✅ Complete |
| M5 | Physics (Jolt) | ✅ Complete |
| M6 | Editor shell (ImGui) | ✅ Complete |
| M7 | World streaming | ✅ Complete |
| M8 | Gameplay integration (all M8.1–M8.9) | ✅ Complete |
| M9 | PBR rendering (Cook-Torrance BRDF) | ✅ Complete |
| M10 | Dynamic sky + weather VFX | ✅ Complete |
| M11 | Vehicle physics | ✅ Complete (Post-M10) |
| M12 | Behaviour tree AI + formation + NavMesh | ✅ Complete (Post-M10) |
| M13 | Cinematics | 🔨 Runtime + tests ✅; baker tool + editor panel ⬜ |
| M14 | Vulkan catch-up | ⬜ Deferred |
| M15 | PAK packager + SDF font renderer | ✅ Complete (Post-M10) |
| M16 | D3D11 depth buffer + IBL | ⬜ Not started (required for DoD) |
| M17 | D3D11 shadow maps + bloom | ⬜ Not started (required for DoD) |
| M18 | X3DAudio 3D positional audio | ⬜ Not started (required for DoD) |
| M19 | Action combat completion (combo FSM + config + tests) | ⬜ Not started (required for DoD) |
| M20 | Quest / dialogue tools + tests | ⬜ Not started (required for DoD) |
| M21 | Nav-mesh baker + ToD LUT baker | ⬜ Not started (required for DoD) |
| M22 | Cut-scene baker + cinematic editor panel | ⬜ Not started (required for DoD) |
| M14 | Vulkan catch-up (full D3D11 parity) | ⬜ Deferred (see M14 section above; no separate M23) |

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
