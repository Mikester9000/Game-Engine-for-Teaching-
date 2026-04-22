# FF15 Requirements Blueprint

> **Purpose:** A structured checklist of every subsystem and tool required to
> build a *Final Fantasy XV*-class game.  For each entry, the table lists its
> purpose, the runtime component(s), the offline/tool component(s), the data
> formats it reads/writes, and the acceptance tests that define "done".
>
> Cross-reference with `docs/PROJECT_MILESTONES.md` for the order in which
> these subsystems are built, and `docs/COPILOT_CONTINUATION.md` for coding
> standards and CI rules.

---

## Project Completion Definition

The project is considered **finished** when:

- Every subsystem in the **Subsystem Completion Matrix** (bottom of this file)
  shows ✅ in all three columns (Runtime, Tool, Tests).
- Each subsystem meets the **FF15-comparable quality bar**:
  - **Visuals** — PBR (IBL + shadows + bloom + tonemap), GPU-skinned characters,
    dynamic sky with time-of-day and weather.
  - **Physics** — Jolt-based rigid bodies, character capsule controller (step-up,
    slopes), vehicle wheel-ray physics, physics hit volumes for combat.
  - **Sound** — XAudio2 positional 3D audio, layered music (battle/explore/idle),
    event-driven SFX triggers.
  - **Gameplay** — Real-time action combat (warp-strike, link-strike, ATB),
    open-world streaming, party AI with behaviour tree + formation, quest +
    dialogue, save/load with 15 slots and auto-save.
  - **Tools** — Full cook pipeline (texture, mesh, audio, animation), Dear ImGui
    editor with Play-in-Engine, Python audio + animation authoring tools.
  - **Teaching** — Every non-trivial pattern has a `// TEACHING NOTE` block and
    is demonstrated in `samples/vertical_slice_project/`.
- A student can study this codebase *alone* to understand every technology
  category used in modern AAA game development.

> Quality note: "teaching quality" means each subsystem is a *real*
> implementation of the concept (real PBR, real physics, real positional audio)
> — not a placeholder or toy version.  Stubs do not satisfy the completion bar.

---

## How to read this document

Each subsystem section follows this structure:

| Field | Meaning |
|---|---|
| **Purpose** | Why this system exists; what FF15 feature it enables |
| **Runtime component(s)** | C++ files that run inside the game executable |
| **Tool component(s)** | Offline CLI tools, editors, or pipeline scripts |
| **Data formats** | Files the system reads at runtime and/or produces offline |
| **Acceptance tests** | Exact headless commands or CI checks that must pass |

Status legend: ✅ exists · 🔨 in progress · ⬜ not yet started

---

## 1. Open-World Streaming

**Status:** ✅ Complete (M7 + M8.7)

**Purpose:** Load and unload sections of a large continuous world without a
loading screen.  FF15's Duscae region is ~4 km² of seamless terrain; the
engine achieves this by streaming "cells" in/out based on camera proximity.

| Field | Detail |
|---|---|
| **Runtime component(s)** | `src/engine/world/world_streaming.hpp/.cpp` — `WorldStreamingManager`; proximity load/evict; frame-budget cap ✅ |
| | `src/engine/world/world_partition.hpp/.cpp` — spatial grid; `CellCoord`, `CellIdFromCoord` ✅ |
| | `src/engine/world/async_loader.hpp/.cpp` — worker-thread job queue; `CancelJob` cancellation token ✅ |
| | `src/game/world/GameStreamingManager.hpp/.cpp` — Zone lifecycle wiring; `AnimatorComponent` on streamed entities ✅ |
| **Tool component(s)** | `cook.exe` — copies `.cell.json` → `.level` cooked file; `AssetRegistry.json` updated ✅ |
| **Data formats** | Source: `.cell.json` (zone name, spawns, NPC IDs); Cooked: `Cooked/Levels/<name>.level` ✅ |
| **Acceptance tests** | `streaming_load`: 9 cells load at radius-1; no duplicates ✅ |
| | `streaming_evict`: evict on camera move; mid-load cancellation ✅ |
| | `streaming_async`: 25 cells, cap=4/frame, all load ≤120 frames ✅ |
| | `m8_streaming`: load `cell_0_0.level` from AssetDB via real AssetLoader ✅ |

---

## 2. Party AI

**Status:** ✅ Complete (FSM + A\* + Behaviour Tree + Formation + NavMesh — Post-M10)

**Purpose:** Three AI-controlled companions move with the player, engage
enemies autonomously, use abilities, and maintain a plausible formation.
FF15's Gladiolus/Ignis/Prompto are always present and fight alongside Noctis.

| Field | Detail |
|---|---|
| **Runtime component(s)** | `src/game/systems/AISystem.hpp/.cpp` — FSM + A\* ✅ |
| | `src/engine/ai/behaviour_tree.hpp/.cpp` — `BtTree`/`BtSequence`/`BtSelector`/`BtCondition`/`BtAction`/`BtBlackboard` ✅ |
| | `src/engine/ai/formation_system.hpp/.cpp` — LINE/V_SHAPE/CIRCLE slot offsets, greedy assignment ✅ |
| | `src/engine/ai/nav_mesh.hpp/.cpp` — grid `NavMesh`, `BakeFromGrid`, A\* `FindPath` (4-dir + diagonal, obstacle routing) ✅ |
| **Tool component(s)** | `tools/creation_engine.py` — nav-mesh baker (bake `.obj` → cooked nav-mesh) (stub) |
| **Data formats** | Source: `party_ai.json` (behaviour tree definition); Cooked: `cooked/ai/<id>.navmesh` |
| **Acceptance tests** | `--headless --scene bt_test`: 4 tests — BT sequence/selector, blackboard, formation offsets, nav-mesh A\* ✅ |

---

## 3. Action Combat

**Status:** ✅ (ATB, warp-strike, link strikes) · ⬜ (hit-detection via physics, real-time input)

**Purpose:** Real-time + ATB hybrid combat.  Noctis warps, strikes, and links
with party members.  Hit detection uses physics ray/shape casts.

| Field | Detail |
|---|---|
| **Runtime component(s)** | `src/game/systems/CombatSystem.hpp/.cpp` — ATB, damage, warp, links (exists) |
| | `src/engine/physics/hit_volume.hpp/.cpp` — attack volume, hurt volume, shape queries |
| | `src/game/systems/combo_system.hpp/.cpp` — input-driven combo state machine |
| **Tool component(s)** | Combat data lives in `src/game/GameData.hpp` (Flyweight); no separate tool yet |
| **Data formats** | `combat_config.json` — damage formulae, ATB speed; loaded at startup |
| **Acceptance tests** | Headless: 1 000 damage roll samples; assert min/max within expected range |
| | Warp strike triggers; entity position updated; MP cost deducted |
| | Link strike fires when companion ATB reaches threshold |

---

## 4. Quests & Objectives

**Status:** ✅ (QuestSystem) · ⬜ (dialogue system, quest editor)

**Purpose:** Track story and side-quest progress.  FF15 has hundreds of hunts,
main quests, and sub-quests, each with objectives, prerequisites, and rewards.

| Field | Detail |
|---|---|
| **Runtime component(s)** | `src/game/systems/QuestSystem.hpp/.cpp` — objective tracking, rewards (exists) |
| | `src/game/systems/dialogue_system.hpp/.cpp` — scripted NPC conversations |
| **Tool component(s)** | `tools/creation_engine.py` — quest data baker (JSON → cooked); quest editor (future) |
| **Data formats** | Source: `quests/*.quest.json`; Cooked: `cooked/quests/<id>.quest` |
| **Acceptance tests** | Start quest → complete objective → assert reward applied and quest marked done |
| | Prerequisite quest not done → assert dependent quest does not start |
| | Dialogue tree reaches terminal node; assert NPC state flag set correctly |

---

## 5. Cinematics

**Status:** ✅ Complete (Post-M10)

**Purpose:** In-engine cut-scenes with camera choreography, character
animation, and timed audio.  FF15's opening and Chapter 14 scenes are
in-engine, not pre-rendered video.

| Field | Detail |
|---|---|
| **Runtime component(s)** | `src/engine/cinematics/cinematic_sequencer.hpp/.cpp` — timeline, track, key-frame player |
| | `src/engine/cinematics/camera_rig.hpp/.cpp` — animated camera path, focal blend |
| **Tool component(s)** | `tools/creation_engine.py` — cut-scene baker (timeline JSON → cooked) |
| **Data formats** | Source: `scenes/*.cinematic.json`; Cooked: `cooked/scenes/<id>.cinematic` |
| **Acceptance tests** | Play cinematic headlessly; assert camera reaches final transform within ±0.01 |
| | Timed audio event fires within ±1 frame of declared time |

---

## 6. Vehicles

**Status:** ✅ Complete (Post-M10)

**Purpose:** The Regalia car traverses roads at speed.  Requires vehicle
physics (force/torque on 4 wheels), road/terrain queries, and a chase camera.

| Field | Detail |
|---|---|
| **Runtime component(s)** | `src/engine/vehicle/vehicle_system.hpp/.cpp` — `VehicleSystem` (wheel-ray suspension, spring-damper, Ackermann steer, fuel drain) ✅ |
| | `VehicleComponent` with `WheelState`×4 (spring compression, damping, contact) ✅ |
| | Vehicle chase camera in `src/engine/rendering/camera_system.hpp/.cpp` ✅ |
| **Tool component(s)** | Road spline data: `tools/creation_engine.py` road baker (stub) |
| **Data formats** | Source: `vehicles/<id>.vehicle.json`; Cooked: `cooked/vehicles/<id>.vehicle` |
| **Acceptance tests** | `--headless --scene vehicle_test`: throttle 5 s → velocity > 0; position advances ✅ |

---

## 7. Weather & Time-of-Day

**Status:** ✅ (WeatherSystem, day/night cycle, sky renderer, WeatherFx, D3D11 procedural sky — M10)

**Purpose:** The world transitions from dawn to dusk, with dynamic fog, rain,
and clear weather affecting enemy spawns and visual ambiance.

| Field | Detail |
|---|---|
| **Runtime component(s)** | `src/game/systems/WeatherSystem.hpp/.cpp` — time cycle, weather FSM, EventBus publish (✅) |
| | `src/engine/rendering/sky_renderer.hpp/.cpp` — procedural sky: time-of-day, sun direction, zenith/horizon colour phases, sunset/sunrise tint, cloud cover darkening (✅ M10) |
| | `src/engine/rendering/weather_fx.hpp/.cpp` — fog density, rain intensity, cloud cover smooth lerp (✅ M10) |
| | `src/engine/rendering/d3d11/D3D11Renderer.cpp::LoadSkyScene` — sky.vs.hlsl (SV_VertexID full-screen triangle), sky.ps.hlsl (gradient + sun disc + fog), sky CB (✅ M10) |
| **Tool component(s)** | `tools/creation_engine.py` — time-of-day curve baker (JSON curves → cooked LUT) |
| **Data formats** | Source: `environment/tod.json` (curves); Cooked: `cooked/environment/tod.lut` |
| **Acceptance tests** | `--headless --scene dynamic_sky` — Test 1: GPU pipeline (RecordHeadlessFrame); Test 2: sun above horizon at noon, below at midnight; Test 3: fog density differs ≥0.1 between Clear and Storm ✅ M10 |
| | EventBus receives `WeatherChanged` events at each transition |

---

## 8. Audio Pipeline

**Status:** ✅ (AudioEngine tool, manifest) · ⬜ (XAudio2 runtime, positional audio)

**Purpose:** Background music layers, combat sound effects, ambient audio, and
positional 3D audio.  FF15 uses a layered music system where battle, exploration,
and idle tracks blend based on game state.

| Field | Detail |
|---|---|
| **Runtime component(s)** | `src/engine/audio/xaudio2_backend.hpp/.cpp` — device init, source voice pool |
| | `src/engine/audio/audio_system.hpp/.cpp` — event-driven playback, music layers, 3D emitters |
| **Tool component(s)** | `tools/audio_engine.py` — register clips, emit/consume manifest (exists) |
| | Cooker step: normalise WAV, build XMA2 for shipping |
| **Data formats** | Source: `audio/*.wav` + `audio-manifest.json`; Cooked: `cooked/audio/<id>.wav` |
| **Acceptance tests** | `audio_engine.py consume --manifest ... --list` exits 0 and prints all clips |
| | XAudio2 backend initialises headlessly (no sound device required in CI) |
| | Play event triggers source voice; assert voice state == active for N ms |

---

## 9. Animation Pipeline

**Status:** ✅ Complete (M4 + M4b)

**Purpose:** Characters need skeletal animation: idle, walk, run, attack,
react-to-hit, death.  FF15's Noctis has hundreds of clips blended in real time.

| Field | Detail |
|---|---|
| **Runtime component(s)** | `src/engine/animation/skeleton.hpp/.cpp` — joint hierarchy, bind pose ✅ |
| | `src/engine/animation/anim_clip.hpp/.cpp` — sampled keyframe channels, lerp/slerp evaluation ✅ |
| | `src/engine/animation/blend_tree.hpp/.cpp` — clip nodes + linear blend ✅ |
| | `src/engine/animation/ik_solver.hpp/.cpp` — Two-Bone analytical IK + FABRIK iterative N-joint IK ✅ |
| | `src/engine/animation/gpu_skinning.hpp/.cpp` — `GpuSkinningBuffer` (64 × Mat4 DYNAMIC CB); D3D11 path ✅ |
| | `src/engine/animation/animation_system.hpp/.cpp` — ECS update; advance time, evaluate, update AnimatorComponent ✅ |
| | `shaders/skinned_mesh.vs.hlsl` + `skinned_mesh.ps.hlsl` — D3D11 SM 4.0 skinned mesh shaders ✅ |
| **Tool component(s)** | `tools/anim_authoring/animation_engine` — glTF → cooked `.anim`; 11 pytest tests ✅ |
| **Data formats** | Source: `animations/*.gltf`; Cooked: `Cooked/Anim/*.skelc`, `*.animc` |
| **Acceptance tests** | `--headless --scene skinned_mesh` — D3D11 WARP GPU skinning pipeline ✅ |
| | Python anim_authoring: 11 tests pass ✅ |
| | Vulkan GPU skinning (`shaders/skinned_mesh.vert/.frag` + UBO) — **DEFERRED** |

---

## 10. Physics

**Status:** ✅ Complete (M5)

**Purpose:** Collision, gravity, character controller, and combat hit queries.
FF15's open world needs accurate terrain collision for characters and vehicles.

| Field | Detail |
|---|---|
| **Runtime component(s)** | `src/engine/physics/physics_world.hpp/.cpp` — Jolt `PhysicsSystem` pImpl wrapper ✅ |
| | `src/engine/physics/character_controller.hpp/.cpp` — `JPH::CharacterVirtual`; gravity, step-up, slope-slide, jump ✅ |
| | `src/engine/physics/rigid_body.hpp/.cpp` — `RigidBodyCreator` + ECS `RigidBodyComponent` (component 22) ✅ |
| | `src/engine/physics/raycast.hpp/.cpp` — `CastRay`, `CastRayDown`, `CastSphere`; `ShapeCastHit` ✅ |
| | `src/engine/physics/hit_volume.hpp/.cpp` — `HitVolumeManager`; AABB Attack/Hurt volumes ✅ |
| | `ColliderComponent` (component 23) — `shapeType` (Box/Sphere/Capsule), `halfExtents`, `radius`, `isTrigger` ✅ |
| **Tool component(s)** | `tools/creation_engine.py` — collision mesh baker (`.obj` → cooked convex/mesh shapes) — 🔨 stub |
| **Data formats** | Source: `physics/<id>.phys.json`; Cooked: `cooked/physics/<id>.phys` |
| **Acceptance tests** | `--headless --scene physics_test` — Test 1: `drop_sphere` (gravity=9.8 m/s²); Test 2: `step_ledge` (0.25 m step-up); Test 3: `raycast` ✅ |
| | CI: `build-windows-physics` job (classic-mode vcpkg Jolt, `--scene physics_test`) ✅ |

---

## 11. UI

**Status:** 🔨 (ncurses terminal UI ✅, D3D11 ImGui HUD ✅ M8.5, MenuStack ✅ Post-M10) · ⬜ (font renderer)

**Purpose:** HUD (HP/MP/ATB bars), menus (inventory, equipment, map), quest
log, dialogue box, and shop.  FF15 uses a clean minimal HUD that scales to 4K.

| Field | Detail |
|---|---|
| **Runtime component(s)** | `src/engine/ui/hud.hpp/.cpp` — HP/MP/ATB bars, mini-map overlay ✅ (M8.5) |
| | `src/engine/ui/menu_stack.hpp/.cpp` — push/pop screen navigation (`MenuStack`, `MenuScreen` enum, 9 screen types) ✅ (Post-M10) |
| | `src/engine/ui/font_renderer.hpp/.cpp` — SDF font atlas, D3D11 R8_UNORM text rendering ✅ (Post-M10) |
| **Tool component(s)** | `tools/creation_engine.py` — font atlas baker, UI layout export |
| **Data formats** | Source: `ui/*.ui.json`; Cooked: `cooked/ui/<id>.ui`; Font: `cooked/fonts/<id>.font` |
| **Acceptance tests** | `--headless --scene menu_stack_test`: 6 tests — push/pop/size, restore/floor, PopToBase, Contains, callback, dup-push guard ✅ |
| | `--headless --scene font_test`: 3 tests — Init (SDF atlas+D3D11 resources), RenderText (no crash), Shutdown (COM release) ✅ |
| | Render HUD headlessly; assert no GPU validation errors (Vulkan validation layers) — deferred |

---

## 12. Save System

**Status:** ✅ Complete (M8.8)

**Purpose:** Serialise the entire ECS world state (player, party, quests,
inventory, zone) to disk and restore it.  FF15 supports 15 save slots plus
auto-save at camp.

| Field | Detail |
|---|---|
| **Runtime component(s)** | `src/engine/save/save_system.hpp/.cpp` — serialise ECS snapshot to JSON/binary |
| | `src/engine/save/save_schema.hpp` — versioned save format with migration support |
| **Tool component(s)** | `tools/validate-assets.py` extended with `--save` mode; save schema validator |
| **Data formats** | `saves/slot_<N>.sav.json` (human-readable debug) or `.sav` (binary, release) |
| **Acceptance tests** | Create world state; save; load; assert component data bit-identical |
| | Corrupt a field; load; assert migration or graceful error, not crash |
| | Auto-save triggers after `CampSystem::Rest`; assert file updated |

---

## 13. Build / Release Pipeline

**Status:** ✅ Complete (validate-assets CI ✅, cook.exe ✅, contract CI ✅, headless validation ✅, pak.exe ✅)

**Purpose:** A one-command build that produces a shippable directory: engine +
tools + cooked assets + sample project.  FF15 ships a 100 GB PAK set; ours is a
teaching slice but must follow the same pipeline shape.

| Field | Detail |
|---|---|
| **Runtime component(s)** | `src/tools/cook/cook_main.cpp` — standalone cooker executable (`cook.exe`) |
| | `src/tools/pak/pak_main.cpp` — PAK packager (directory → `.pak` archive) |
| **Tool component(s)** | `tools/validate-assets.py` — schema validator (exists) |
| | `tools/creation_engine.py` — asset bake (exists) |
| | `tools/audio_engine.py` — audio bank builder (exists) |
| | `.github/workflows/contract-tests.yml` — golden-file CI gate |
| **Data formats** | Input: all source manifests; Output: `dist/<target>/` with cooked assets + exe |
| **Acceptance tests** | `cook.exe --project samples/ff15_slice/` exits 0; `assetdb.json` exists and validates |
| | `ctest -L contract` all pass (golden-file diffs clean) |
| | `engine_sandbox.exe --headless --validate-project dist/ff15_slice/` exits 0 |

---

## Subsystem Completion Matrix

> **Last updated: 2026-04-22 — after Post-M10 (SDF Font Renderer + pak.exe) completion.**
> Update each cell as subsystem code is committed and tests pass.
> The project is complete when every cell shows ✅.

| # | Subsystem | Runtime | Tool | Tests | Notes |
|---|---|---|---|---|---|
| 1 | Open-world streaming | ✅ | ✅ | ✅ | `WorldStreamingManager` + `GameStreamingManager` + 4 streaming CI scenes; M8.7 wires into D3D11 GameRuntime |
| 2 | Party AI | ✅ | ⬜ | ✅ | FSM + A* + BT (`BtTree`/`BtSequence`/`BtSelector`) + FormationSystem (LINE/V_SHAPE/CIRCLE) + NavMesh A* (Post-M10); nav-mesh baker tool ⬜ |
| 3 | Action combat | 🔨 | ⬜ | ⬜ | ATB/warp-strike in D3D11 GameRuntime (M8.1); physics hit-detection ⬜ |
| 4 | Quests & objectives | 🔨 | ⬜ | ⬜ | QuestSystem + DialogueSystem in D3D11 GameRuntime (M8.6); dialogue tree editor ⬜ |
| 5 | Cinematics | ✅ | ⬜ | ✅ | `CameraRig` (keyframe Lerp, binary-search bracket) + `CinematicSequencer` (shot/cut, callbacks, ECS write) + `cinematic_test` CI (Post-M10) ✅; cut-scene editor panel ⬜ |
| 6 | Vehicles | ✅ | ⬜ | ✅ | `VehicleSystem` (wheel-ray suspension, spring-damper, Ackermann steer, fuel) + `WheelState`×4 + vehicle chase camera + `vehicle_test` CI (Post-M10) ✅; road spline baker tool ⬜ |
| 7 | Weather & time-of-day | ✅ | 🔨 | ✅ | SkyRenderer + WeatherFx + D3D11 sky pipeline + `dynamic_sky` headless CI (M10) ✅; tod.lut curve editor ⬜ |
| 8 | Audio pipeline | ✅ | ✅ | ✅ | Python tool + 32 tests ✅; XAudio2 backend + AudioSystem + AudioSourceComponent ✅ (M3) |
| 9 | Animation pipeline | ✅ | ✅ | ✅ | Python tool + 11 tests ✅; C++ skeleton/clip/blend/IK/GPU skinning (M4/M4b) ✅ |
| 10 | Physics | ✅ | ✅ | ✅ | Jolt `PhysicsWorld`, `CharacterController`, `Raycast`, `HitVolumeManager`, `RigidBodyComponent`+`ColliderComponent`; 3 headless CI tests (M5) |
| 11 | UI | ✅ | ⬜ | ✅ | D3D11 ImGui HUD (HP/MP bars, ATB) ✅ (M8.5); `MenuStack` push/pop navigation + 6-test CI ✅ (Post-M10); `FontRenderer` SDF text + `--scene font_test` CI ✅ (Post-M10) |
| 12 | Save system | ✅ | ✅ | ✅ | `SaveSystem`: 15 slots + auto-save + `"version"` migration field; JSON ECS snapshot ✅ (M8.8) |
| 13 | Build / release pipeline | ✅ | ✅ | ✅ | Python tools + `cook.exe` + contract CI + `validate-project` + `m8_streaming` CI ✅; `pak.exe` PAK1 packager ✅ (Post-M10) |

✅ complete · 🔨 in progress / partial · ⬜ not yet started
