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

**Status:** ⬜

**Purpose:** Load and unload sections of a large continuous world without a
loading screen.  FF15's Duscae region is ~4 km² of seamless terrain; the
engine achieves this by streaming "cells" in/out based on camera proximity.

| Field | Detail |
|---|---|
| **Runtime component(s)** | `src/engine/world/world_streaming.hpp/.cpp` — cell manager, async loader, eviction policy |
| | `src/engine/world/world_partition.hpp/.cpp` — spatial grid, visibility culling |
| | `src/game/world/Zone.hpp/.cpp` — already exists; integrate streaming on top |
| **Tool component(s)** | `tools/creation_engine.py` (level baker, streaming cell packager) |
| **Data formats** | Source: `.level.json`; Cooked: `cooked/levels/<id>.level` (tile data + spawn list, binary) |
| **Acceptance tests** | `engine_sandbox.exe --headless --validate-project samples/ --check streaming` exits 0 |
| | Load 4 adjacent cells; assert no entity is missing or duplicated |
| | Unload a cell; assert all its entities are destroyed |

---

## 2. Party AI

**Status:** ✅ (FSM + A\*) · ⬜ (behaviour tree, formation)

**Purpose:** Three AI-controlled companions move with the player, engage
enemies autonomously, use abilities, and maintain a plausible formation.
FF15's Gladiolus/Ignis/Prompto are always present and fight alongside Noctis.

| Field | Detail |
|---|---|
| **Runtime component(s)** | `src/game/systems/AISystem.hpp/.cpp` — FSM + A\* (exists) |
| | `src/engine/ai/behaviour_tree.hpp/.cpp` — composite, sequence, selector, leaf nodes |
| | `src/engine/ai/formation_system.hpp/.cpp` — slot assignment, follow target |
| | `src/engine/ai/nav_mesh_query.hpp/.cpp` — nav-mesh runtime query interface |
| **Tool component(s)** | `tools/creation_engine.py` — nav-mesh baker (bake `.obj` → cooked nav-mesh) |
| **Data formats** | Source: `party_ai.json` (behaviour tree definition); Cooked: `cooked/ai/<id>.navmesh` |
| **Acceptance tests** | Headless sim 300 frames; assert all 3 party members reach target waypoint |
| | Party disperses when combat starts; re-forms when combat ends |

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

**Status:** ⬜

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

**Status:** ✅ (state enum) · ⬜ (physics, road query, camera)

**Purpose:** The Regalia car traverses roads at speed.  Requires vehicle
physics (force/torque on 4 wheels), road/terrain queries, and a chase camera.

| Field | Detail |
|---|---|
| **Runtime component(s)** | `src/game/systems/vehicle_system.hpp/.cpp` — vehicle state, input, physics apply |
| | `src/engine/physics/vehicle_physics.hpp/.cpp` — wheel raycasts, suspension, torque |
| | `src/engine/rendering/chase_camera.hpp/.cpp` — spring-arm camera |
| **Tool component(s)** | Road spline data: `tools/creation_engine.py` road baker |
| **Data formats** | Source: `vehicles/<id>.vehicle.json`; Cooked: `cooked/vehicles/<id>.vehicle` |
| **Acceptance tests** | Apply throttle for 5 s; assert velocity > 0, position changed along road axis |
| | Brake from speed; assert velocity returns to 0 within expected distance |

---

## 7. Weather & Time-of-Day

**Status:** ✅ (WeatherSystem, day/night cycle) · ⬜ (sky renderer, curve editor)

**Purpose:** The world transitions from dawn to dusk, with dynamic fog, rain,
and clear weather affecting enemy spawns and visual ambiance.

| Field | Detail |
|---|---|
| **Runtime component(s)** | `src/game/systems/WeatherSystem.hpp/.cpp` — time cycle, weather FSM, EventBus publish (exists) |
| | `src/engine/rendering/sky_renderer.hpp/.cpp` — procedural sky, sun/moon, atmospheric scatter |
| | `src/engine/rendering/weather_fx.hpp/.cpp` — rain particles, fog density |
| **Tool component(s)** | `tools/creation_engine.py` — time-of-day curve baker (JSON curves → cooked LUT) |
| **Data formats** | Source: `environment/tod.json` (curves); Cooked: `cooked/environment/tod.lut` |
| **Acceptance tests** | Advance time 24 h headlessly; assert all 4 weather states visited at least once |
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
| **Tool component(s)** | `tools/audio_authoring/audio_engine/` — Python audio synthesis + WAV cooking (see `tools/audio_authoring/`) |
| | Cooker step: normalise WAV, build XMA2 for shipping |
| **Data formats** | Source: `audio/*.wav` + `audio-manifest.json`; Cooked: `cooked/audio/<id>.wav` |
| **Acceptance tests** | `tools/audio_authoring/` tests pass (18 tests); pipeline generates all game assets |
| | XAudio2 backend initialises headlessly (no sound device required in CI) |
| | Play event triggers source voice; assert voice state == active for N ms |

---

## 9. Animation Pipeline

**Status:** ⬜

**Purpose:** Characters need skeletal animation: idle, walk, run, attack,
react-to-hit, death.  FF15's Noctis has hundreds of clips blended in real time.

| Field | Detail |
|---|---|
| **Runtime component(s)** | `src/engine/animation/skeleton.hpp/.cpp` — joint hierarchy, bind pose |
| | `src/engine/animation/anim_clip.hpp/.cpp` — sampled or hermite keyframes |
| | `src/engine/animation/blend_tree.hpp/.cpp` — 1D/2D blend spaces, transitions |
| | `src/engine/animation/ik_solver.hpp/.cpp` — two-bone IK (foot planting) |
| | `src/engine/animation/gpu_skinning.hpp/.cpp` — upload joint matrices to Vulkan |
| **Tool component(s)** | `tools/anim_authoring/animation_engine/` — Python skeletal animation authoring + `.animc` cooking (see `tools/anim_authoring/`) |
| **Data formats** | Source: `animations/*.gltf`; Cooked: `cooked/animations/<id>.anim` (binary, schema in docs) |
| **Acceptance tests** | Cook `test_idle.gltf`; load `.anim`; evaluate frame 0 and frame N; assert joint transforms match golden file |
| | Blend two clips at weight 0.5; assert output is lerp of the two |

---

## 10. Physics

**Status:** ⬜

**Purpose:** Collision, gravity, character controller, and combat hit queries.
FF15's open world needs accurate terrain collision for characters and vehicles.

| Field | Detail |
|---|---|
| **Runtime component(s)** | `src/engine/physics/physics_world.hpp/.cpp` — Jolt `PhysicsSystem` wrapper |
| | `src/engine/physics/character_controller.hpp/.cpp` — capsule, step-up, slope |
| | `src/engine/physics/rigid_body.hpp/.cpp` — ECS component + Jolt body linkage |
| | `src/engine/physics/raycast.hpp/.cpp` — single/batch ray, shape cast |
| **Tool component(s)** | `tools/creation_engine.py` — collision mesh baker (`.obj` → cooked convex/mesh shapes) |
| **Data formats** | Source: `physics/<id>.phys.json`; Cooked: `cooked/physics/<id>.phys` |
| **Acceptance tests** | Drop sphere from height; assert it contacts floor within expected time (gravity = 9.8 m/s²) |
| | Character steps over a 0.25 m ledge; assert position updated correctly |
| | Raycast from above terrain; assert hit distance matches terrain height |

---

## 11. UI

**Status:** ✅ (ncurses terminal UI, EventBus notifications) · ⬜ (Vulkan HUD, menu stack)

**Purpose:** HUD (HP/MP/ATB bars), menus (inventory, equipment, map), quest
log, dialogue box, and shop.  FF15 uses a clean minimal HUD that scales to 4K.

| Field | Detail |
|---|---|
| **Runtime component(s)** | `src/engine/ui/ui_system.hpp/.cpp` — render-backend-agnostic widget system |
| | `src/engine/ui/hud.hpp/.cpp` — HP/MP/ATB bars, mini-map overlay |
| | `src/engine/ui/menu_stack.hpp/.cpp` — push/pop screen navigation |
| | `src/engine/ui/font_renderer.hpp/.cpp` — font atlas, signed distance field text |
| **Tool component(s)** | `tools/creation_engine.py` — font atlas baker, UI layout export |
| **Data formats** | Source: `ui/*.ui.json`; Cooked: `cooked/ui/<id>.ui`; Font: `cooked/fonts/<id>.font` |
| **Acceptance tests** | Push 3 screens; pop 2; assert top of stack matches expected screen |
| | Render HUD headlessly; assert no GPU validation errors (Vulkan validation layers) |

---

## 12. Save System

**Status:** ⬜

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

**Status:** ✅ (validate-assets CI) · ⬜ (cook.exe, PAK packager, full headless validation)

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

| # | Subsystem | Runtime | Tool | Tests |
|---|---|---|---|---|
| 1 | Open-world streaming | ⬜ | 🔨 | ⬜ |
| 2 | Party AI | 🔨 | ⬜ | ⬜ |
| 3 | Action combat | ✅ | ⬜ | ⬜ |
| 4 | Quests & objectives | ✅ | ⬜ | ⬜ |
| 5 | Cinematics | ⬜ | ⬜ | ⬜ |
| 6 | Vehicles | 🔨 | ⬜ | ⬜ |
| 7 | Weather & time-of-day | ✅ | ⬜ | ⬜ |
| 8 | Audio pipeline | 🔨 | ✅ | ⬜ |
| 9 | Animation pipeline | ⬜ | ⬜ | ⬜ |
| 10 | Physics | ⬜ | ⬜ | ⬜ |
| 11 | UI | 🔨 | ⬜ | ⬜ |
| 12 | Save system | ⬜ | ⬜ | ⬜ |
| 13 | Build / release pipeline | 🔨 | ✅ | ⬜ |

✅ exists · 🔨 in progress · ⬜ not yet started
