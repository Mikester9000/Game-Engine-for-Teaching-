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
| `src/engine/platform/win32/Win32Window.hpp/.cpp` | Win32 HWND, message pump, QPC timer |
| `src/engine/rendering/vulkan/VulkanRenderer.hpp/.cpp` | Instance, device, swapchain, render pass, clear-colour loop |
| `src/sandbox/main.cpp` | Entry point for `engine_sandbox` executable |
| `CMakeLists.txt` | `ENGINE_ENABLE_VULKAN` option; `engine_sandbox` target |

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

**Done means:** CI `build-windows.yml` passes; `engine_sandbox.exe --headless` exits 0.

---

## Milestone M1 — Triangle

**Status:** ⬜ Not started

### Goals
- Introduce the graphics pipeline: vertex buffer, index buffer, vertex shader,
  fragment shader, pipeline state object.
- Every shader is a separate `.vert` / `.frag` file (not an inline string).
- TEACHING NOTE added to every new Vulkan concept.

### Deliverables
| File | Description |
|---|---|
| `src/engine/rendering/vulkan/vulkan_pipeline.hpp/.cpp` | Pipeline state, shader module loading |
| `src/engine/rendering/vulkan/vulkan_buffer.hpp/.cpp` | Vertex/index buffer, staging upload |
| `src/engine/rendering/vulkan/vulkan_mesh.hpp/.cpp` | Mesh: vertex + index buffer pair |
| `shaders/triangle.vert` | GLSL vertex shader |
| `shaders/triangle.frag` | GLSL fragment shader |

### Acceptance tests

```bat
:: Build
cmake --build . --config Debug --target engine_sandbox

:: Run — must render a white triangle and exit cleanly on ESC
.\Debug\engine_sandbox.exe

:: Headless — validate pipeline creation does not crash
.\Debug\engine_sandbox.exe --headless --scene triangle
:: Expected: [PASS] Pipeline created. Mesh uploaded. Draw recorded.
```

**Done means:** CI green; triangle visible at runtime; headless validation exits 0.

---

## Milestone M2 — AssetDB + Cooker

**Status:** ⬜ Not started

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

**Status:** ⬜ Not started

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

**Status:** ⬜ Not started

### Goals
- Skeleton, clip evaluation, simple blend tree.
- GPU skinning via joint matrix UBO uploaded to Vulkan.
- `tools/anim_authoring/animation_engine/` (`animation_engine.io.Exporter`) cooks a test clip to `.animc`.

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

**Status:** ⬜ Not started

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

**Status:** ⬜ Not started

### Goals
- A minimal `editor.exe` that opens a window with a scene hierarchy panel,
  an inspector panel, and an asset browser.
- Load / save a scene file (JSON).
- ImGui used for all panels.

### Deliverables
| File | Description |
|---|---|
| `src/editor/editor_main.cpp` | `editor.exe` entry point |
| `src/editor/editor_app.hpp/.cpp` | Editor application class; owns window + ImGui context |
| `src/editor/panels/scene_hierarchy.hpp/.cpp` | ECS entity tree view |
| `src/editor/panels/inspector.hpp/.cpp` | Component property editor |
| `src/editor/panels/asset_browser.hpp/.cpp` | AssetDB browser, drag-drop |
| `src/engine/scene/scene_serialiser.hpp/.cpp` | JSON save/load of ECS snapshot |
| `CMakeLists.txt` | `editor` target; ImGui dependency |

### Acceptance tests

```bat
:: Headless editor startup
.\Debug\editor.exe --headless
:: Expected: [PASS] Editor initialised. Scene hierarchy empty. Inspector ready.

:: Scene round-trip
.\Debug\editor.exe --headless --create-scene tests\scenes\basic.scene.json
.\Debug\editor.exe --headless --load-scene tests\scenes\basic.scene.json --validate
:: Expected: [PASS] Entity count matches. Component data identical.
```

**Done means:** CI green; editor opens and shows a scene; headless scene round-trip passes.

---

## Milestone M7 — World Streaming

**Status:** ⬜ Not started

### Goals
- Zone tiles stream in and out based on camera proximity without a loading screen.
- Async loader runs on a worker thread; main thread never blocks on IO.
- Existing `Zone.hpp/.cpp` integrated under the streaming manager.

### Deliverables
| File | Description |
|---|---|
| `src/engine/world/world_streaming.hpp/.cpp` | Cell manager, proximity check, async load/evict |
| `src/engine/world/world_partition.hpp/.cpp` | Spatial grid, visibility frustum cull |
| `src/engine/world/async_loader.hpp/.cpp` | `std::thread` + lock-free queue; job: load cell → callback |
| `src/game/world/Zone.hpp/.cpp` | Updated to cooperate with streaming manager |
| `tools/creation_engine.py` | Cell packager: zone JSON → `cooked/levels/<id>.level` |

### Acceptance tests

```bat
:: Headless streaming — load 4 adjacent cells
.\Debug\engine_sandbox.exe --headless --test streaming_load --cells 4
:: Expected: [PASS] 4 cells loaded. Entity count = N. No duplicates.

:: Headless streaming — evict 1 cell
.\Debug\engine_sandbox.exe --headless --test streaming_evict
:: Expected: [PASS] Cell evicted. Entity count reduced by expected amount. No dangling refs.

:: Async load does not block main thread
.\Debug\engine_sandbox.exe --headless --test streaming_async --frames 120
:: Expected: [PASS] Main thread frame time never exceeded 2 ms during cell load.
```

**Done means:** CI green; all streaming headless tests pass; no frame spikes > 2 ms during cell load.

---

## Milestone M8 — Gameplay Integration

**Status:** ⬜ Not started

### Goals
- All existing gameplay systems (combat, quests, AI, camp, inventory, magic,
  shop, weather) wired into the Vulkan runtime instead of the ncurses terminal.
- Lua scripting still works via `LuaEngine`.
- Sample FF15 slice project: one zone, one enemy, one quest, one party member.

### Deliverables
| File | Description |
|---|---|
| `src/game/Game.hpp/.cpp` | Updated `Game::Init` and `Game::Run` to use Vulkan + audio + physics |
| `src/game/systems/combat_system.hpp/.cpp` | Renamed from `CombatSystem`; uses physics raycasts |
| `src/game/systems/quest_system.hpp/.cpp` | Renamed; triggers dialogue + camera events |
| `src/game/systems/ai_system.hpp/.cpp` | Renamed; uses behaviour tree + nav-mesh |
| `src/game/systems/weather_system.hpp/.cpp` | Renamed; drives sky renderer |
| `samples/ff15_slice/` | Full sample project: zone, enemy, quest, manifest, scripts |

### Acceptance tests

```bat
:: Headless full simulation — 300 frames
.\Debug\engine_sandbox.exe --headless --run-sim 300 --project samples\ff15_slice\
:: Expected: [PASS] 300 frames. Combat resolved. Quest objective updated. Camp rest triggered. No crashes.

:: Lua hooks fire
.\Debug\engine_sandbox.exe --headless --test lua_hooks
:: Expected: [PASS] on_combat_start fired. on_camp_rest fired. on_level_up fired.

:: Full CI suite
ctest --test-dir build
:: Expected: all tests pass
```

**Done means:** CI green; full headless simulation passes 300 frames; all Lua hooks fire; sample project loads and validates.

---

## Future Milestones (Post-M8)

| ID | Name | Key deliverable |
|---|---|---|
| M9 | Cinematics | `cinematic_sequencer` + cut-scene editor |
| M10 | Vehicle physics | `vehicle_physics` + road baker + chase camera |
| M11 | Advanced AI | Full behaviour tree; nav-mesh baker |
| M12 | UI system | Vulkan HUD; menu stack; font renderer |
| M13 | Save system | ECS snapshot save/load; migration |
| M14 | Behaviour tree AI | Boss patterns; formation AI |
| M15 | PAK packager + release | `pak.exe`; distribution directory; installer |

---

## Milestone Progress Summary

| Milestone | Name | Status |
|---|---|---|
| M0 | Vulkan sandbox | ✅ Complete |
| M1 | Triangle | ⬜ |
| M2 | AssetDB + Cooker | ⬜ |
| M3 | Hello Texture + Audio | ⬜ |
| M4 | Animation runtime | ⬜ |
| M5 | Physics integration | ⬜ |
| M6 | Editor shell | ⬜ |
| M7 | World streaming | ⬜ |
| M8 | Gameplay integration | ⬜ |
