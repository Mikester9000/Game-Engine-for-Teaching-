# Project Milestones

> **Purpose:** Each milestone is a PR-sized slice of work that always ends in
> an **executable or headless validation pass**.  "Done" means CI is green and
> the headless acceptance command exits 0.  Never start the next milestone
> until the current one's CI is green.
>
> Cross-reference `docs/COPILOT_CONTINUATION.md` for coding standards and PR
> rules, and `docs/FF15_REQUIREMENTS_BLUEPRINT.md` for the full subsystem
> checklist.

---

## Milestone M0 — Vulkan Sandbox

**Status:** ✅ Complete

### Goals
- Establish a Windows-first build target separate from the Linux terminal game.
- Prove the Vulkan bootstrap works on a developer machine and in CI.

### Deliverables
| File | Description |
|---|---|
| `src/engine/platform/win32/Win32Window.hpp/.cpp` | Win32 HWND, message pump, QPC timer; `headless` param hides the window for CI |
| `src/engine/rendering/vulkan/VulkanRenderer.hpp/.cpp` | Instance, device, swapchain, render pass, clear-colour loop |
| `src/sandbox/main.cpp` | Entry point with `--headless` / `--scene` arg parsing |
| `CMakeLists.txt` | `ENGINE_ENABLE_VULKAN` option; `engine_sandbox` target |
| `.github/workflows/build-linux.yml` | CI: builds terminal game, runs Python tool tests |

### Acceptance tests

```bat
:: Build
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Debug
cmake --build . --config Debug --target engine_sandbox

:: Headless — window must not be required for CI
.\Debug\engine_sandbox.exe --headless
:: Expected output: [PASS] Vulkan device initialised. Swapchain created. Headless mode: skipping present loop.
```

**Done means:** CI `build-linux.yml` passes; `engine_sandbox.exe --headless` exits 0.

---

## Milestone M1 — Triangle

**Status:** ✅ Complete

### Goals
- Introduce the graphics pipeline: vertex buffer, index buffer, vertex shader,
  fragment shader, pipeline state object.
- Every shader is a separate `.vert` / `.frag` file (not an inline string).
- TEACHING NOTE added to every new Vulkan concept.

### Deliverables
| File | Description |
|---|---|
| `src/engine/rendering/vulkan/vulkan_pipeline.hpp/.cpp` | Pipeline state, SPIR-V shader loading, dynamic viewport/scissor |
| `src/engine/rendering/vulkan/vulkan_buffer.hpp/.cpp` | Host-visible GPU buffer; memory type selection |
| `src/engine/rendering/vulkan/vulkan_mesh.hpp/.cpp` | Vertex struct, binding/attribute descriptions, draw call |
| `shaders/triangle.vert` | GLSL vertex shader (position + colour input) |
| `shaders/triangle.frag` | GLSL fragment shader (interpolated colour output) |
| `CMakeLists.txt` | glslc shader compilation; `.spv` output copied next to exe |

### Acceptance tests

```bat
:: Build (Vulkan SDK + glslc required)
cmake --build . --config Debug --target engine_sandbox

:: Run — renders a red/green/blue triangle on an animated background; ESC to exit
.\Debug\engine_sandbox.exe

:: Headless — validate pipeline creation does not crash
.\Debug\engine_sandbox.exe --headless --scene triangle
:: Expected: [PASS] Pipeline created. Mesh uploaded. Draw recorded.
```

**Done means:** triangle visible at runtime; headless validation exits 0.

---

## Milestone M2 — AssetDB + Cooker

**Status:** ✅ Complete

### Goals
- Introduce the asset pipeline.  Source manifests → cooker → cooked assets →
  AssetDB loaded by the runtime.
- Headless validation mode: load project, print registry, exit 0.
- Contract tests pass (golden files for each cooked type).

### Deliverables
| File | Description |
|---|---|
| `src/tools/cook/cook_main.cpp` | `cook.exe` — reads source manifests, writes `cooked/` + `assetdb.json` |
| `src/engine/assets/asset_db.hpp/.cpp` | Runtime registry: ID → cooked path + metadata; load/unload API |
| `src/engine/assets/asset_loader.hpp/.cpp` | Async loader interface (synchronous stub first) |
| `tests/golden/` | Golden cooked files for contract tests |
| `.github/workflows/contract-tests.yml` | CI job: cook → diff golden files |
| `samples/ff15_slice/project.json` | Minimal sample project pointing at example manifests |

### Acceptance tests

```bat
:: Cook sample project
.\Debug\cook.exe --project samples\ff15_slice\

:: Validate output
python tools\validate-assets.py cooked\assetdb.json --verbose
:: Expected: All N assets valid.

:: Headless project load
.\Debug\engine_sandbox.exe --headless --validate-project cooked\
:: Expected: [PASS] AssetDB loaded. N assets registered.

:: Contract tests
ctest --test-dir build -L contract
:: Expected: all pass
```

**Done means:** CI green; `ctest -L contract` all pass; headless project load exits 0.

---

## Milestone M3 — Hello Texture + Hello Audio

**Status:** ✅ Complete

> **What's done:** D3D11 DDS texture loader (`d3d11_texture.hpp/.cpp`), D3D11 textured quad shaders
> (`textured_quad.vs.hlsl` / `textured_quad.ps.hlsl`), D3D11 `LoadScene("textured_quad")` in
> `D3D11Renderer`, XAudio2 backend (`xaudio2_backend.hpp/.cpp`), ECS AudioSystem with music FSM
> (`audio_system.hpp/.cpp`), `AudioSourceComponent` added to ECS, `directxtex` + `imgui` added
> to `vcpkg.json`, CI headless `--scene textured_quad` validation.
>
> **Deferred (Vulkan path):** `vulkan_texture.hpp/.cpp`, `vulkan_descriptor.hpp/.cpp`,
> `shaders/textured_quad.vert/.frag` — implement when Vulkan work resumes (Post-M8).

### Goals
- Extend the cooker with texture and audio support.
- Runtime renders a textured quad using a BC7-compressed DDS loaded from AssetDB.
- XAudio2 backend initialises; a cooked WAV clip plays on keypress.

### Deliverables
| File | Description |
|---|---|
| `src/engine/rendering/vulkan/vulkan_texture.hpp/.cpp` | DDS load, Vulkan image + sampler |
| `src/engine/rendering/vulkan/vulkan_descriptor.hpp/.cpp` | Descriptor set layout, pool, update |
| `src/engine/audio/xaudio2_backend.hpp/.cpp` | Device init, master voice, source voice pool |
| `src/engine/audio/audio_system.hpp/.cpp` | Event-driven play/stop; music layer FSM |
| `shaders/textured_quad.vert/.frag` | UV-mapped quad shaders |
| Cooker update | Texture cooking: PNG → BC7 DDS; Audio cooking: WAV normalisation |

### Acceptance tests

```bat
:: Headless texture load
.\Debug\engine_sandbox.exe --headless --scene textured_quad
:: Expected: [PASS] Texture loaded (512x512, BC7). Descriptor bound. Draw recorded.

:: Headless audio init
.\Debug\engine_sandbox.exe --headless --test audio
:: Expected: [PASS] XAudio2 initialised. Clip 'battle-01-bgm' loaded. Voice created.

:: Creation Engine emits manifest; cooker consumes it
python tools\creation_engine.py emit --manifest /tmp/test.json
.\Debug\cook.exe --manifest /tmp/test.json --out /tmp/cooked/
python tools\validate-assets.py /tmp/cooked/assetdb.json
```

**Done means:** CI green; textured quad renders at runtime; audio plays on keypress; headless tests exit 0.

---

## Milestone M4 — Animation Runtime

**Status:** ✅ Complete

> **What's done:** C++ skeleton runtime, anim clip evaluation, blend tree, animation system ECS
> update (`AnimatorComponent` in ECS), Two-Bone + FABRIK IK solver, D3D11 GPU skinning constant
> buffer upload, `skinned_mesh.vs.hlsl` + `skinned_mesh.ps.hlsl`, D3D11Renderer `skinned_mesh`
> scene, CI headless `--scene skinned_mesh` validation.
>
> **Deferred (Vulkan path):** `shaders/skinned_mesh.vert/.frag` + Vulkan UBO — implement when
> Vulkan work resumes (Post-M8).

### Goals
- Skeleton, clip evaluation, simple blend tree.
- GPU skinning via joint matrix UBO uploaded to Vulkan.
- `tools/anim_authoring/animation_engine` (`tools/anim_authoring`) cooks a test glTF clip to `.anim`.

### Deliverables
| File | Description |
|---|---|
| `src/engine/animation/skeleton.hpp/.cpp` | Joint hierarchy, bind pose, joint-to-world transform |
| `src/engine/animation/anim_clip.hpp/.cpp` | Sampled keyframe evaluation; hermite interpolation |
| `src/engine/animation/blend_tree.hpp/.cpp` | 1D blend space; two-clip lerp node |
| `src/engine/animation/gpu_skinning.hpp/.cpp` | Upload joint matrices; skinning UBO |
| `src/engine/animation/animation_system.hpp/.cpp` | ECS component update; advance time, evaluate, upload |
| `shaders/skinned_mesh.vert/.frag` | Skinned vertex shader (8 weights max) |
| Cooker update | `.gltf` → `.anim` (binary, defined schema) |
| `tests/golden/test_idle.anim` | Golden file for contract test |

### Acceptance tests

```bat
:: Cook test animation
.\Debug\cook.exe --asset tests\sources\test_idle.gltf --out tests\cooked\

:: Contract test (diff golden)
ctest --test-dir build -L contract -R anim
:: Expected: PASS

:: Headless animation eval
.\Debug\engine_sandbox.exe --headless --scene skinned_mesh --frames 60
:: Expected: [PASS] Frame 0 joint[0] = (expected). Frame 60 joint[0] = (expected). No GPU validation errors.

:: Blend test
.\Debug\engine_sandbox.exe --headless --test blend_tree
:: Expected: [PASS] Blend weight 0.5 produces lerp of input clips.
```

**Done means:** CI green; animated character visible at runtime; golden-file contract test passes.

---

## Milestone M5 — Physics Integration

**Status:** ✅ Complete

> **What's done:** Jolt Physics via vcpkg (`joltphysics`), `PhysicsWorld` wrapper (pImpl façade),
> `CharacterController` (`JPH::CharacterVirtual`, gravity/step-up/slope/jump), `Raycast` +
> `CastRayDown` + `CastSphere` helpers, `HitVolumeManager` (AABB Attack/Hurt volumes),
> `RigidBodyCreator` + `RigidBodyComponent` (component 22) + `ColliderComponent` (component 23)
> in ECS.hpp, `physics_test` headless acceptance scene (3 tests), CI `build-windows-physics` job.

### Goals
- Jolt Physics integrated as a CMake dependency (via vcpkg or submodule).
- Character capsule falls, collides with a floor, steps over small obstacles.
- Raycasts used by combat system for hit detection.

### Deliverables
| File | Description |
|---|---|
| `src/engine/physics/physics_world.hpp/.cpp` | Jolt `PhysicsSystem` wrapper; fixed step update |
| `src/engine/physics/rigid_body.hpp/.cpp` | ECS component bridging ECS transform ↔ Jolt body |
| `src/engine/physics/character_controller.hpp/.cpp` | Capsule, step-up, slope limit, ground query |
| `src/engine/physics/raycast.hpp/.cpp` | Single ray, batch ray, shape cast |
| `src/engine/physics/hit_volume.hpp/.cpp` | Trigger volumes for combat attack/hurt zones |
| `CMakeLists.txt` | Jolt dependency; `ENGINE_ENABLE_PHYSICS` option |

### Acceptance tests

```bat
:: Headless physics
.\Debug\engine_sandbox.exe --headless --test physics_drop
:: Expected: [PASS] Sphere dropped 10 m. Contact at t=1.43 s (±0.05). gravity=9.8 m/s².

.\Debug\engine_sandbox.exe --headless --test character_step
:: Expected: [PASS] Character stepped over 0.25 m ledge. Position advanced.

.\Debug\engine_sandbox.exe --headless --test raycast
:: Expected: [PASS] Ray from (0,10,0) dir (0,-1,0) hit floor at y=0.00.
```

**Done means:** CI green (Jolt builds from source via vcpkg); all three headless physics tests pass.

---

## Milestone M6 — Editor Shell

**Status:** ✅ Complete

### Goals
- A `creation-suite-editor.exe` with a scene hierarchy panel, inspector panel,
  content browser, scene canvas, Play-in-Engine button, and headless CLI mode.
- Scene save/load as JSON (extends `shared/schemas/scene.schema.json`).
- Dear ImGui used for all panels (DockSpace layout).

### Deliverables
| File | Description |
|---|---|
| `editor/src/main.cpp` | Win32 + D3D11 + ImGui entry point; headless mode (`--headless`, `--create-scene`, `--load-scene --validate`) |
| `editor/src/EditorApp.hpp/.cpp` | Editor app: DockSpace, menu bar, Play-in-Engine, Open/Save scene dialogs |
| `editor/src/ContentBrowserPanel.hpp/.cpp` | Content/ file tree via std::filesystem; **ImGui drag-drop source** for CONTENT_ASSET payloads |
| `editor/src/SceneEditorPanel.hpp/.cpp` | 2D entity canvas with JSON save/load; shared-state accessors; **ImGui drag-drop target** (creates entity at drop position) |
| `editor/src/panels/SceneHierarchyPanel.hpp/.cpp` | Entity list: add/delete/rename/duplicate + context menu |
| `editor/src/panels/InspectorPanel.hpp/.cpp` | Table-driven component property editor (DragFloat/DragInt/Checkbox/InputText); Add Component popup |
| `src/engine/scene/scene_serialiser.hpp/.cpp` | `SceneSerialiser::SaveScene/LoadScene/CountEntities` — JSON ↔ ECS World round-trip |
| `editor/CMakeLists.txt` | Updated: adds panel sources + `src/panels/` include path |
| `CMakeLists.txt` | `find_package(nlohmann_json CONFIG QUIET)` + `scene_serialiser.cpp` + `ENGINE_ENABLE_JSON` define |

### Acceptance tests

```bat
:: Headless editor startup (validates panels initialise without D3D11)
.\Debug\creation-suite-editor.exe --headless
:: Expected: [PASS] Editor initialised. Scene hierarchy empty. Inspector ready.

:: Scene round-trip (create → save → load → validate)
.\Debug\creation-suite-editor.exe --headless --create-scene tests\scenes\basic.scene.json
.\Debug\creation-suite-editor.exe --headless --load-scene tests\scenes\basic.scene.json --validate
:: Expected: [PASS] Scene loaded: 1 entity in 'tests\scenes\basic.scene.json'

:: Play-in-Engine (manual — requires engine_sandbox.exe in the same directory)
:: Build > Play in Engine (F5) in the editor GUI
:: Expected: engine_sandbox.exe launches with the preview scene
```

**Done means:** Headless editor scene round-trip exits 0; inspector edits components; hierarchy shows entities with add/rename/delete; Play-in-Engine launches engine_sandbox.

---

## Milestone M7 — World Streaming

**Status:** ✅ Complete

### Goals
- Zone tiles stream in and out based on camera proximity without a loading screen.
- Async loader runs on a worker thread; main thread never blocks on IO.
- Existing `Zone.hpp/.cpp` integrated under the streaming manager.

### Deliverables
| File | Description |
|---|---|
| `src/engine/world/world_streaming.hpp/.cpp` | `WorldStreamingManager`: proximity check, async load/evict, frame-budget cap, ImGui debug overlay |
| `src/engine/world/world_partition.hpp/.cpp` | `WorldPartition`: spatial grid; `CellCoord`, `CellIdFromCoord`, `GetCellsNearPosition` |
| `src/engine/world/async_loader.hpp/.cpp` | `AsyncLoader`: single worker thread + job queue; `CancelJob` cancellation token |
| `src/engine/world/cell_data.hpp` | `CellData` + `SpawnEntry` POD structs |
| `src/game/world/GameStreamingManager.hpp/.cpp` | Game-layer subclass: Zone lifecycle wired into streaming hooks |
| `samples/vertical_slice_project/Content/Levels/cell_0_0.cell.json` | First cooked streaming cell; `cook_assets.py` gains `cook_levels()` |

### Acceptance tests

```bat
engine_sandbox.exe --headless --scene streaming_load   :: 9 cells load at radius-1; no duplicates
engine_sandbox.exe --headless --scene streaming_evict  :: evict on camera move; mid-load cancellation
engine_sandbox.exe --headless --scene streaming_async  :: 25 cells, cap=4/frame, all load ≤120 frames
```

**Done means:** All three streaming scenes exit 0 in CI; frame-budget cap confirmed; cancellation tokens work.

---

## Milestone M8 — Gameplay Integration

**Status:** ✅ Complete

### Goals
- All gameplay systems (Combat, AI, Quest, Weather, etc.) wired into the **D3D11** `engine_sandbox` runtime.
- Zone streaming from AssetRegistry → cooked cells → live ECS entities (M8.7).
- Production save system: 15 slots + auto-save + migration (M8.8).
- D3D11 ImGui HUD overlay (M8.5).

### Sub-milestones
| Sub | Deliverable | Status |
|-----|-------------|--------|
| M8.1 | `GameRuntime`: D3D11 main-loop driver; ticks all ECS systems | ✅ |
| M8.2 | `InputMapper`: Win32 keyboard → ECS component state | ✅ |
| M8.3 | `CameraSystem`: third-person follow camera + mouse orbit | ✅ |
| M8.4 | (Part of M4b) AISystem ported to 3D world positions | ✅ |
| M8.5 | D3D11 ImGui HUD: HP/MP bars, ATB gauge, equipped spell | ✅ |
| M8.6 | `DialogueSystem` + QuestSystem → HUD notifications + NPC sample | ✅ |
| M8.7 | `GameStreamingManager` → D3D11 runtime; `AnimatorComponent` on streamed entities; 3 new authored cells | ✅ |
| M8.8 | `SaveSystem`: 15 slots + auto-save; JSON ECS serialisation + `"version"` migration | ✅ |
| M8.9 | `--scene m8_gameplay` headless acceptance test (CI) | ✅ |

### Acceptance tests

```bat
:: M8.9 — gameplay systems headless (run BEFORE cook.exe)
engine_sandbox.exe --headless --scene m8_gameplay
:: [PASS] m8_gameplay: all 3 acceptance tests passed.

:: M8.7 — zone streaming from disk (run AFTER cook.exe)
cook.exe --project samples\vertical_slice_project --verbose
engine_sandbox.exe --headless --scene m8_streaming
:: [PASS] m8_streaming: ≥1 cell(s) loaded from disk via AssetLoader.
```

**Done means:** `m8_gameplay` and `m8_streaming` both exit 0 in CI; all M8 sub-milestones complete.

---

## Milestone M9 — PBR Rendering (Cook-Torrance BRDF)

**Goal:** Implement a physically-based rendering pipeline for D3D11 featuring the
full Cook-Torrance BRDF (metallic-roughness workflow), demonstrating the core
mathematics of modern AAA rendering.

**Deliverables:**
- `shaders/pbr_mesh.vs.hlsl` — SM 4.0 vertex shader: model→world→clip transforms,
  inverse-transpose normal transform (for non-uniform scale correctness).
- `shaders/pbr_mesh.ps.hlsl` — SM 4.0 pixel shader: GGX NDF, Smith geometry,
  Schlick Fresnel, energy-conserving kD/kS split, Lambertian diffuse,
  Reinhard tone mapping, gamma correction.  Fully annotated with TEACHING NOTEs
  explaining each BRDF term.
- `D3D11Renderer::PBRScene` struct — holds VS, PS, input layout, VB (UV sphere),
  IB, three constant buffers (perFrameCB b0, lightCB b1, materialCB b2),
  and a cull-none rasterizer state.
- `D3D11Renderer::LoadScene("pbr_mesh")` — compiles HLSL at runtime, generates a
  UV sphere (16×16 stacks/slices, 289 verts, 1536 indices), uploads all GPU
  resources, initialises light (warm directional sun) and material (gold-like).
- `D3D11Renderer::DrawPBRMesh()` — updates perFrameCB with a slowly-rotating world
  matrix each frame; builds LookAt + perspective projection matrices inline with
  TEACHING NOTE derivations.
- `--scene pbr_mesh` in `engine_sandbox` — windowed demo (rotating PBR sphere)
  and `--headless --scene pbr_mesh` CI validation.
- CI step in `build-windows.yml` — WARP headless smoke test.

**What M9 does NOT include (deferred to future milestones):**
- Image-Based Lighting (IBL) cubemaps (irradiance + prefiltered specular + BRDF LUT).
- Directional shadow maps.
- Bloom / post-processing.
- PBR textures (albedo map, metallic-roughness map, normal map, AO map).
- Vulkan PBR pipeline.

| Sub-task | Description | Status |
|---|---|---|
| M9.1 | `pbr_mesh.vs.hlsl` — SM 4.0 PBR vertex shader | ✅ |
| M9.2 | `pbr_mesh.ps.hlsl` — full Cook-Torrance BRDF pixel shader | ✅ |
| M9.3 | `PBRScene` struct + D3D11 resource creation in D3D11Renderer | ✅ |
| M9.4 | UV sphere geometry generation (16×16 stacks/slices) | ✅ |
| M9.5 | Per-frame LookAt + perspective projection matrices in DrawPBRMesh | ✅ |
| M9.6 | `--scene pbr_mesh` and `--headless --scene pbr_mesh` in main.cpp | ✅ |
| M9.7 | CI acceptance step: `build-windows.yml` headless WARP run | ✅ |

**CI validation command:**
```
engine_sandbox.exe --headless --scene pbr_mesh
```
Expected output: `[PASS] pbr_mesh scene pipeline OK (WARP headless).`

**Done means:** `--headless --scene pbr_mesh` exits 0 in CI (WARP software rasteriser);
all three constant buffers mapped correctly; sphere geometry drawn without D3D11 errors.

---

## Milestone M10 — Dynamic Sky + Weather VFX

**Goal:** Implement a procedural time-of-day sky renderer with weather effects (fog, rain,
cloud cover) that runs as a standalone D3D11 scene and validates on WARP in CI.

**What M10 adds:**

- `WeatherType` enum (`Clear/Cloudy/Rain/Storm`) + `WeatherFxState` struct in
  `engine/rendering/weather_fx.hpp/.cpp`.
- `WeatherFx` — translates a `WeatherType` into smoothly-lerped fog density,
  rain intensity, and cloud cover values.
- `SkyShaderConstants` (80 bytes, 5 × float4) — maps directly to the HLSL cbuffer.
- `SkyRenderer` in `engine/rendering/sky_renderer.hpp/.cpp` — time-of-day clock
  (60× compression), sun direction formula (`sin/cos` on shifted arc), zenith/horizon
  colour phases (night / day / sunset/sunrise), cloud darkening, fog colour matching.
- `shaders/sky.vs.hlsl` — SV_VertexID full-screen triangle trick (no VB needed).
- `shaders/sky.ps.hlsl` — exponential gradient + sun disc + glow + fog overlay +
  rain darkening + Reinhard tonemap + gamma correction (SM 4.0).
- `D3D11Renderer::SkyScene` struct + `LoadSkyScene()` helper + `DrawSky()` method.
- `D3D11Renderer::DrawFrame()` — calls `m_skyRenderer.Update(dt)` + `DrawSky()` when
  `m_currentScene == "dynamic_sky"`.
- `D3D11Renderer::RecordHeadlessFrame()` — sky scene WARP validation path.
- `D3D11Renderer::UnloadScene()` — releases VS, PS, CB for the sky scene.
- `--scene dynamic_sky` in `main.cpp` — windowed demo + 3-test headless acceptance:
  1. GPU pipeline (RecordHeadlessFrame on WARP)
  2. Time-of-day: sun above horizon at noon, below at midnight
  3. Weather: fog density ≥ 0.1 higher in Storm vs Clear

**What M10 does NOT include (deferred):**
- Volumetric clouds or cloud shadow casters.
- IBL sky cubemap (HDR capture + irradiance convolution).
- Rain particle system (GPU particles).
- Time-of-day curve editor (tod.lut authoring tool).
- Vulkan sky pipeline.

| Sub-task | Description | Status |
|---|---|---|
| M10.1 | `weather_fx.hpp/.cpp` — WeatherType + WeatherFxState + lerp transitions | ✅ |
| M10.2 | `sky_renderer.hpp/.cpp` — time-of-day, sun direction, colour phases, SkyShaderConstants | ✅ |
| M10.3 | `shaders/sky.vs.hlsl` — SV_VertexID full-screen triangle | ✅ |
| M10.4 | `shaders/sky.ps.hlsl` — gradient + sun disc + fog + weather + Reinhard + gamma | ✅ |
| M10.5 | `D3D11Renderer` `SkyScene` struct + `LoadSkyScene()` + `DrawSky()` | ✅ |
| M10.6 | `--scene dynamic_sky` + `--headless --scene dynamic_sky` in main.cpp | ✅ |
| M10.7 | CI acceptance step: `build-windows.yml` headless WARP run | ✅ |

**CI validation command:**
```
engine_sandbox.exe --headless --scene dynamic_sky
```
Expected output:
```
[OK] dynamic_sky Test 1/3: GPU pipeline OK (WARP headless).
[OK] dynamic_sky Test 2/3: time-of-day sun elevation OK ...
[OK] dynamic_sky Test 3/3: weather fog delta OK ...
[PASS] dynamic_sky: all 3 acceptance tests passed.
```

**Done means:** `--headless --scene dynamic_sky` exits 0 in CI; sky CB uploaded correctly;
sun elevation formula validated; weather fog transitions validated.

---

## Future Milestones (Post-M10)

| ID | Name | Key deliverable |
|---|---|---|
| M9 | PBR Rendering | ✅ Cook-Torrance BRDF + metallic-roughness + UV sphere (D3D11) |
| M10 | Dynamic Sky | ✅ Procedural time-of-day + weather VFX (rain, fog) — D3D11 |
| M11 | Vehicle Physics | Wheel-ray suspension + chase camera + `VehicleComponent` |
| M12 | Behaviour Tree AI | Boss patterns; formation system; nav-mesh baker |
| M13 | Cinematics | `CinematicSequencer` + camera rig + cut-scene editor |
| M14 | Vulkan Catch-up | Resume all DEFERRED Vulkan items (textures, descriptors, PBR, skinning) |
| M15 | PAK Packager + Release | `pak.exe`; distribution directory; installer |

---

## Milestone Progress Summary

| Milestone | Name | Status |
|---|---|---|
| M0 | Vulkan sandbox | ✅ Complete |
| M1 | Triangle | ✅ Complete |
| M1.5 | D3D11 baseline renderer | ✅ Complete |
| M2 | AssetDB + Cooker | ✅ Complete |
| M3 | Hello Texture + Audio | ✅ Complete |
| M4 | Animation runtime | ✅ Complete |
| M5 | Physics integration | ✅ Complete |
| M6 | Editor shell | ✅ Complete |
| M7 | World streaming | ✅ Complete |
| M8 | Gameplay integration | ✅ Complete |
| M9 | PBR Rendering (Cook-Torrance BRDF) | ✅ Complete |
| M10 | Dynamic Sky + Weather VFX | ✅ Complete |
