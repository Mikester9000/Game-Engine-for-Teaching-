# ROADMAP — Game Engine for Teaching (FFXV-Style Action RPG)

> **North Star:** Build a complete, teachable Final Fantasy XV-inspired open-world
> action RPG toolchain — with every subsystem documented for learning.
> "Prefer simple-but-real over fancy-but-incomplete."

---

## Milestone 1 — Monorepo Foundation ✅ *(current)*

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

---

## Milestone 2 — Import & Cook Pipeline *(next)*

**Goal:** A complete round-trip: import source asset → cook → engine loads it.

| Item | Priority |
|------|----------|
| Texture importer: PNG/JPG → compressed `.tex` cooked format | HIGH |
| Audio cook: WAV → OGG with loop-point metadata → `.bank` | HIGH |
| Animation cook: Python anim_authoring → `.skelc` + `.animc` | HIGH |
| Asset registry update step (run after cook, updates `AssetRegistry.json`) | HIGH |
| Mesh importer: OBJ/GLTF → engine mesh format | MEDIUM |
| Material importer: JSON material def → cooked material | MEDIUM |
| Engine texture/mesh loader from `Cooked/` directory | MEDIUM |
| Editor import settings UI (mip count, sRGB/linear, compression) | LOW |

---

## Milestone 3 — Editor → Engine "Play In Engine" *(near-term)*

**Goal:** Save a scene in the editor and immediately run it in the engine.

| Item | Priority |
|------|----------|
| Scene save/load (JSON → ECS entities) wired to engine Zone | HIGH |
| "Play In Engine" button in editor — launches engine with current scene | HIGH |
| Hot-reload: engine watches scene file, reloads on save | MEDIUM |
| Basic entity inspector in editor (name, transform, component properties) | MEDIUM |
| Mesh placement in scene editor (pick from content browser, drag to canvas) | MEDIUM |
| Undo/redo stack in scene editor | LOW |

---

## Milestone 4 — World Systems *(medium-term)*

**Goal:** A living open world with streaming, navigation, and combat.

| Item | Priority |
|------|----------|
| World streaming: chunked zone loading/unloading by player position | HIGH |
| PBR renderer: IBL + directional shadow + basic post-FX (tonemap + bloom) | HIGH |
| Navigation mesh generation + runtime pathfinding (A*) | HIGH |
| Character controller: capsule physics, slopes, jump | HIGH |
| Camera system: third-person follow with collision avoidance | HIGH |
| Combat rework: warp-strike, link-strike, combos (FFXV style) | MEDIUM |
| Behavior tree AI: boss patterns using BT instead of FSM | MEDIUM |
| Cloth / particle system (basic) | LOW |

---

## Milestone 5 — Vertical Slice *(long-term)*

**Goal:** One playable chunk of an FFXV-style world to prove the whole pipeline.

| Item | Priority |
|------|----------|
| Town chunk: 1 explorable town (shops, NPC quests, inn) | HIGH |
| Dungeon chunk: 1 dungeon with combat, traps, and a boss | HIGH |
| Overworld: drive between town and dungeon (vehicle system stub) | MEDIUM |
| Cinematic system: simple cutscene player (entity sequences) | MEDIUM |
| Save/load system: deterministic serialization | HIGH |
| Full asset cook pipeline for the vertical slice | HIGH |
| Performance budget: 60 fps in Debug, 120+ fps in Release | MEDIUM |

---

## Ongoing Work (every milestone)

- Add `// TEACHING NOTE` comments to every new subsystem
- Update `docs/ARCHITECTURE.md` when new systems are added
- Keep `samples/vertical_slice_project/` buildable and runnable
- Write tests: C++ test targets + Python pytest
- Keep build green: `cmake --preset windows-debug` must succeed on a clean clone

---

## Decisions Log

| Decision | Rationale |
|----------|-----------|
| Windows-first | Vulkan + Visual Studio is the primary teaching target |
| Qt 6 for editor | Mature, cross-platform, rich tooling APIs |
| Python for authoring tools | Fast iteration, rich ecosystem (NumPy, SciPy) |
| JSON for all shared formats | Human-readable, no extra library on Windows |
| Shared schemas in `shared/` | Single source of truth for all data formats |
| Keep `src/` in place | Avoid breaking existing Linux terminal build |
