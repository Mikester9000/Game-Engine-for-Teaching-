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

**Status:** 🔨 (ATB, warp-strike, link-strikes, elemental damage, status effects in terminal game and D3D11 GameRuntime) · ⬜ (physics hit-detection wired to combat, real-time combo system, combat_config.json loader, headless acceptance tests)

**Purpose:** Real-time + ATB hybrid combat.  Noctis warps, strikes, and links
with party members.  Hit detection uses physics ray/shape casts.

> **Verification note (2026-04-22):** `src/game/systems/CombatSystem.hpp/.cpp` is fully
> implemented (ATB, damage formula, warp-strike, link-strikes, elemental multipliers, status
> effects, loot, M8.1 wired into D3D11 GameRuntime).  `src/engine/physics/hit_volume.hpp/.cpp`
> exists as infrastructure.  **`src/game/systems/combo_system.hpp/.cpp` does NOT exist** — the
> input-driven combo state machine is not yet implemented.  No externalized `combat_config.json`
> exists; all combat data is hard-coded in `GameData.hpp`.  Headless acceptance tests for
> combat correctness are ⬜.

| Field | Detail |
|---|---|
| **Runtime component(s)** | `src/game/systems/CombatSystem.hpp/.cpp` — ATB, damage, warp-strike, link-strikes, elemental damage, status effects, loot ✅ |
| | `src/engine/physics/hit_volume.hpp/.cpp` — attack volume, hurt volume, shape queries ✅ (infrastructure); not yet wired to combo input ⬜ |
| | `src/game/systems/combo_system.hpp/.cpp` — input-driven combo state machine ⬜ **NOT YET CREATED** |
| **Tool component(s)** | Combat data in `src/game/GameData.hpp` (Flyweight; hard-coded) — no separate authoring tool ⬜ |
| **Data formats** | `combat_config.json` — damage formulae, ATB speed, per-weapon combos; loaded at startup ⬜ not yet implemented |
| **Acceptance tests** | `--headless --scene combat_test`: 1 000 damage roll samples; assert min/max within expected range ⬜ |
| | Warp strike triggers; entity position updated; correct tile offset ⬜ |
| | Link strike fires when companion ATB reaches threshold ⬜ |

---

## 4. Quests & Objectives

**Status:** ✅ (QuestSystem ✅, DialogueSystem ✅ M8.6 wired into D3D11 GameRuntime) · ⬜ (quest/dialogue authoring tool, headless acceptance tests)

**Purpose:** Track story and side-quest progress.  FF15 has hundreds of hunts,
main quests, and sub-quests, each with objectives, prerequisites, and rewards.

> **Verification note (2026-04-22):** `src/game/systems/QuestSystem.hpp/.cpp` is fully
> implemented.  `src/game/systems/dialogue_system.hpp/.cpp` was created in M8.6 and is wired
> into the D3D11 GameRuntime and the sample NPC cell.  Both systems are present in the runtime.
> The quest/dialogue **authoring tool** (baker producing cooked quest data) is ⬜ not yet
> created.  Dedicated headless acceptance tests (`--scene quest_test`) are ⬜.

| Field | Detail |
|---|---|
| **Runtime component(s)** | `src/game/systems/QuestSystem.hpp/.cpp` — objective tracking, rewards, prerequisites ✅ |
| | `src/game/systems/dialogue_system.hpp/.cpp` — scripted NPC conversations, dialogue trees ✅ (M8.6) |
| **Tool component(s)** | `tools/creation_engine.py` — quest data baker (JSON → cooked) ⬜ not yet implemented |
| | Quest/dialogue editor panel in `editor/` ⬜ not yet implemented |
| **Data formats** | Source: `quests/*.quest.json`; Cooked: `cooked/quests/<id>.quest` ⬜ schema not finalised |
| **Acceptance tests** | `--headless --scene quest_test`: start quest → complete objective → assert reward applied ⬜ |
| | Prerequisite quest not done → assert dependent quest does not start ⬜ |
| | Dialogue tree reaches terminal node; assert NPC state flag set correctly ⬜ |

---

## 5. Cinematics

**Status:** ✅ Runtime complete (Post-M10) · ⬜ cut-scene baker tool · ⬜ editor panel

**Purpose:** In-engine cut-scenes with camera choreography, character
animation, and timed audio.  FF15's opening and Chapter 14 scenes are
in-engine, not pre-rendered video.

> **Verification note (2026-04-22):** `src/engine/cinematics/cinematic_sequencer.hpp/.cpp` and
> `src/engine/cinematics/camera_rig.hpp/.cpp` are fully implemented (Post-M10). The
> `--scene cinematic_test` headless CI runs 3 tests (camera position accuracy, shot advance,
> callback firing).  **`CameraComponent.cinematicOverride`** (in `ECS.hpp`) lets the sequencer
> write eye/look-at into the ECS camera for use by `CameraSystem`. The cut-scene baker tool
> and editor panel are both ⬜ not yet implemented.

| Field | Detail |
|---|---|
| **Runtime component(s)** | `src/engine/cinematics/cinematic_sequencer.hpp/.cpp` — timeline, shot/cut, callbacks, `ApplyToCamera()` ECS write ✅ (Post-M10) |
| | `src/engine/cinematics/camera_rig.hpp/.cpp` — keyframed camera path, binary-search Lerp, focal blend ✅ (Post-M10) |
| | `ECS.hpp` — `CameraComponent.cinematicOverride` flag + `cinematicEyePos` / `cinematicLookAt` fields ✅ |
| **Tool component(s)** | `tools/creation_engine.py` — cut-scene baker (timeline JSON → cooked `.cinematic`) ⬜ not yet implemented |
| | Cut-scene editor panel in `editor/` ⬜ not yet implemented |
| **Data formats** | Source: `scenes/*.cinematic.json`; Cooked: `cooked/scenes/<id>.cinematic` ⬜ source schema `shared/schemas/cinematic.schema.json` not yet created; baker ⬜ not yet implemented (M22) |
| **Acceptance tests** | `--headless --scene cinematic_test`: 3 tests (camera position, shot advance, callback) ✅ (Post-M10) |
| | Timed audio event fires within ±1 frame of declared time ⬜ |

---

## 6. Vehicles

**Status:** ✅ Runtime + Tests complete (Post-M10) · ⬜ road spline baker tool

**Purpose:** The Regalia car traverses roads at speed.  Requires vehicle
physics (force/torque on 4 wheels), road/terrain queries, and a chase camera.

> **Verification note (2026-04-22):** `src/engine/vehicle/vehicle_system.hpp/.cpp` fully
> implemented (Post-M10). `VehicleComponent` with `WheelState`×4 (spring compression,
> damping, contact) in `ECS.hpp`. Vehicle chase camera in `camera_system.hpp/.cpp`.
> `--scene vehicle_test` headless CI: throttle 5 s → velocity > 0, position advances ✅.
> Road spline baker tool is ⬜ stub.

| Field | Detail |
|---|---|
| **Runtime component(s)** | `src/engine/vehicle/vehicle_system.hpp/.cpp` — `VehicleSystem` (wheel-ray suspension, spring-damper, Ackermann steer, fuel drain) ✅ |
| | `VehicleComponent` with `WheelState`×4 (spring compression, damping, contact) ✅ |
| | Vehicle chase camera in `src/engine/rendering/camera_system.hpp/.cpp` ✅ |
| **Tool component(s)** | Road spline data: `tools/creation_engine.py` road baker (bake spline JSON → cooked road) ⬜ stub |
| **Data formats** | Source: `vehicles/<id>.vehicle.json`; Cooked: `cooked/vehicles/<id>.vehicle` |
| **Acceptance tests** | `--headless --scene vehicle_test`: throttle 5 s → velocity > 0; position advances ✅ |

---

## 7. Weather & Time-of-Day

**Status:** ✅ (WeatherSystem, day/night cycle, sky renderer, WeatherFx, D3D11 procedural sky — M10) · ⬜ time-of-day curve baker tool (tod.lut)

> **Verification note (2026-04-22):** Runtime and tests are fully implemented (M10 ✅).
> `SkyRenderer`, `WeatherFx`, `shaders/sky.vs.hlsl`, `shaders/sky.ps.hlsl` all exist.
> The `--headless --scene dynamic_sky` CI passes 3 tests (GPU pipeline, sun elevation, fog delta).
> **The `tod.lut` curve baker** (baking a `tod.json` curve file into a precomputed
> colour-gradient LUT) is **⬜ not yet implemented** — the sky uses a real-time procedural
> formula instead of a baked LUT. This is acceptable for teaching but is needed for the
> "Tool" column to reach ✅ in the Subsystem Completion Matrix.

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

**Status:** ✅ (Python AudioEngine tool + 32 tests ✅; XAudio2 backend + AudioSystem + music FSM + AudioSourceComponent ✅ M3; X3DAudio 3D positional audio ✅ M18)

**Purpose:** Background music layers, combat sound effects, ambient audio, and
positional 3D audio.  FF15 uses a layered music system where battle, exploration,
and idle tracks blend based on game state.

> **M18 complete (2026-04-22):** `XAudio2Backend::Init` now calls `X3DAudioInitialize`
> after creating the mastering voice (channel mask from `GetChannelMask`).
> `SetListenerPosition` / `Compute3DVolume` / `Apply3DAttenuation` implement distance
> rolloff using X3DAudio DSP (with a linear math fallback on headless CI).
> `AudioSystem::Update` calls `Apply3DAttenuation` each frame for `is3D=true` sources
> using their `TransformComponent::position`.  CI gate: `--headless --scene audio_3d_test`
> (3 tests: init, at-listener ≥ 0.95, at-maxDist ≤ 0.05).

| Field | Detail |
|---|---|
| **Runtime component(s)** | `src/engine/audio/xaudio2_backend.hpp/.cpp` — device init, X3DAudio init, source voice pool (16 slots), WAV parser, SetListenerPosition, Compute3DVolume, Apply3DAttenuation ✅ |
| | `src/engine/audio/audio_system.hpp/.cpp` — event-driven playback, music FSM crossfade, per-frame 3D attenuation, SetListenerPosition API ✅ |
| | X3DAudio 3D positional audio (distance rolloff) — ✅ M18 complete |
| **Tool component(s)** | `tools/audio_engine.py` — register clips, emit/consume manifest ✅ |
| | `tools/audio_authoring/audio_engine/` — Python synthesiser, DSP, OGG/WAV export ✅ (32 tests) |
| | Cooker step: normalise WAV, build XMA2 for shipping ⬜ |
| **Data formats** | Source: `audio/*.wav` + `audio-manifest.json`; Cooked: `cooked/audio/<id>.wav` |
| **Acceptance tests** | `audio_engine.py consume --manifest ... --list` exits 0 and prints all clips ✅ |
| | Python audio_authoring: 32 tests pass ✅ |
| | `--headless --scene audio_3d_test` (init + at-listener ≥ 0.95 + at-maxDist ≤ 0.05) ✅ M18 |

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

**Status:** ✅ Runtime + Tests complete (M5) · ⬜ collision mesh baker tool not started

**Purpose:** Collision, gravity, character controller, and combat hit queries.
FF15's open world needs accurate terrain collision for characters and vehicles.

> **Verification note (2026-04-22):** All runtime components fully implemented (M5 ✅).
> `tools/creation_engine.py` currently does not provide a collision mesh baker command
> or `.obj`-to-cooked collision baking stub — the file only contains manifest
> register/emit/consume/list commands.  The Physics row in the Completion Matrix
> is therefore updated accordingly from ✅/✅/✅ → ✅/⬜/✅.

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

**Status:** ✅ Runtime + Tests complete (Post-M10): D3D11 ImGui HUD ✅ M8.5, MenuStack ✅ Post-M10, FontRenderer ✅ Post-M10 · ⬜ UI authoring tool (font atlas baker, UI layout export)

**Purpose:** HUD (HP/MP/ATB bars), menus (inventory, equipment, map), quest
log, dialogue box, and shop.  FF15 uses a clean minimal HUD that scales to 4K.

> **Verification note (2026-04-22):** The Section 11 "Status" header previously listed
> FontRenderer as ⬜; this was stale.  `src/engine/ui/font_renderer.hpp/.cpp` IS
> implemented (SDF atlas, R8_UNORM D3D11 texture, `shaders/sdf_text.vs.hlsl` +
> `sdf_text.ps.hlsl`), and `--scene font_test` passes 3 CI tests.
> The UI **authoring tool** (font atlas baker producing a cooked `.font` file, and
> a UI layout export from JSON to a cooked `.ui` format) is ⬜ not yet implemented.

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

> **Last verified: 2026-04-22 — deep reconciliation pass after Post-M10 completions.**
> Each cell reflects the actual implementation state verified against source files.
> The project is complete when every cell shows ✅.

> **D3D11 Visuals Quality Bar note:** The "Project Completion Definition" requires
> PBR with IBL + directional shadows + bloom.  As of 2026-04-22 the D3D11 renderer has
> Cook-Torrance BRDF ✅ but **IBL ⬜, directional shadow maps ⬜, and bloom
> post-processing ⬜ are NOT yet implemented**.  These are required for the quality bar
> (see §Visuals in the completion criteria above) and are tracked as Milestones M16–M18
> in `docs/ROADMAP.md`.  The D3D11 depth buffer is also **⬜ not yet created**
> (confirmed by `DrawFrame` comment: "no depth buffer yet") — this is a prerequisite for
> shadow maps.

| # | Subsystem | Runtime | Tool | Tests | Notes |
|---|---|---|---|---|---|
| 1 | Open-world streaming | ✅ | ✅ | ✅ | `WorldStreamingManager` + `GameStreamingManager` + 4 streaming CI scenes; M8.7 wires into D3D11 GameRuntime |
| 2 | Party AI | ✅ | ⬜ | ✅ | FSM + A* + BT (`BtTree`/`BtSequence`/`BtSelector`) + FormationSystem (LINE/V_SHAPE/CIRCLE) + NavMesh A* (Post-M10); nav-mesh baker tool ⬜ |
| 3 | Action combat | 🔨 | ⬜ | ⬜ | ATB/warp-strike/link-strikes in D3D11 GameRuntime (M8.1) ✅; `combo_system.hpp/.cpp` ⬜ NOT CREATED; `combat_config.json` ⬜; headless tests ⬜ |
| 4 | Quests & objectives | ✅ | ⬜ | ⬜ | QuestSystem ✅ + DialogueSystem ✅ in D3D11 GameRuntime (M8.6); quest/dialogue baker tool ⬜; headless acceptance tests ⬜ |
| 5 | Cinematics | ✅ | ⬜ | ✅ | `CameraRig` + `CinematicSequencer` + `cinematic_test` CI ✅ (Post-M10); cut-scene baker tool ⬜; editor panel ⬜ |
| 6 | Vehicles | ✅ | ⬜ | ✅ | `VehicleSystem` + `WheelState`×4 + vehicle chase camera + `vehicle_test` CI ✅ (Post-M10); road spline baker tool ⬜ |
| 7 | Weather & time-of-day | ✅ | ⬜ | ✅ | SkyRenderer + WeatherFx + D3D11 sky pipeline + `dynamic_sky` CI ✅ (M10); tod.lut curve baker ⬜ |
| 8 | Audio pipeline | ✅ | ✅ | ✅ | Python tool + 32 tests ✅; XAudio2 backend + AudioSystem + music FSM ✅ (M3); X3DAudio 3D positional audio ✅ (M18 — distance rolloff, listener position, `audio_3d_test` CI) |
| 9 | Animation pipeline | ✅ | ✅ | ✅ | Python tool + 11 tests ✅; C++ skeleton/clip/blend/IK/GPU skinning (M4/M4b) ✅ |
| 10 | Physics | ✅ | ⬜ | ✅ | Jolt `PhysicsWorld`, `CharacterController`, `Raycast`, `HitVolumeManager`, `RigidBodyComponent`+`ColliderComponent` ✅; collision mesh baker ⬜ not started |
| 11 | UI | ✅ | ⬜ | ✅ | D3D11 ImGui HUD ✅ (M8.5); `MenuStack` + 6-test CI ✅ (Post-M10); `FontRenderer` SDF text + `font_test` CI ✅ (Post-M10); font atlas baker tool ⬜ |
| 12 | Save system | ✅ | ✅ | ✅ | `SaveSystem`: 15 slots + auto-save + `"version"` migration field; JSON ECS snapshot ✅ (M8.8) |
| 13 | Build / release pipeline | ✅ | ✅ | ✅ | Python tools + `cook.exe` + contract CI + `validate-project` + `m8_streaming` CI ✅; `pak.exe` PAK1 packager ✅ (Post-M10) |

✅ complete · 🔨 in progress / partial · ⬜ not yet started

---

## D3D11 Visuals Quality Bar — Remaining Items

The following rendering features are required by the "Project Completion Definition" quality
bar (PBR with IBL + shadows + bloom) and are **not yet implemented** as of 2026-04-22.
All are D3D11-only per the active policy.  Vulkan parity is deferred.

| Feature | Status | Prerequisite | Milestone |
|---------|--------|-------------|-----------|
| D3D11 depth buffer (DSV + `DepthStencilState`) | ✅ | — | M16 |
| PBR texture maps (albedo, metallic-roughness, normal, AO) | ✅ | depth buffer | M16 |
| D3D11 IBL (irradiance cubemap + prefiltered env + BRDF LUT) | ✅ | depth buffer | M16 |
| D3D11 directional shadow map (shadow pass + PCF sampling in `pbr_mesh.ps.hlsl`) | ✅ | depth buffer | M17 |
| D3D11 bloom post-processing (bright-pass → Gaussian blur → composite) | ✅ | depth buffer | M17 |
| X3DAudio positional 3D audio (distance rolloff, emitter/listener panning) | ✅ | — | M18 |
| `combo_system.hpp/.cpp` — real-time input-driven combo state machine | ⬜ | — | M19 |
| `combat_config.json` + loader — externalized combat tuning data | ⬜ | — | M19 |
| Combat headless acceptance tests (`--scene combat_test`) | ⬜ | combo_system | M19 |
| Quest/dialogue baker tool (`creation_engine.py` quest bake) | ⬜ | — | M20 |
| Quest/dialogue headless acceptance tests (`--scene quest_test`) | ⬜ | baker | M20 |
| Nav-mesh baker tool (`creation_engine.py` bake `.obj` → `.navmesh`) | ⬜ | — | M21 |
| Time-of-day LUT baker (`tod.json` curves → `cooked/environment/tod.lut`) | ⬜ | — | M21 |
| Cut-scene baker tool (timeline JSON → cooked `.cinematic`) | ⬜ | — | M22 |
| Cinematic editor panel in Dear ImGui editor | ⬜ | baker | M22 |
| Vulkan catch-up (textures, descriptors, PBR, skinning, sky, HUD) | ⬜ | D3D11 complete | M14 |
