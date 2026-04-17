# Copilot Continuation Plan

> **Purpose:** This document is the single source of truth that allows Copilot
> (or any contributor) to continue this project with **minimal human interaction**.
> Every decision, constraint, roadmap entry, and acceptance test is recorded
> here.  Before opening a PR, re-read this file and verify that your changes
> satisfy the relevant milestone criteria.

---

## 1. Project Mission

**Mission:** Teach the *exact* process and function of every subsystem, tool,
and pipeline stage required to build a modern AAA game from scratch.

*Final Fantasy XV* is used as the **reference game** throughout this project.
Every feature category below maps to something FF15 ships.  The goal is not to
clone FF15 content; it is to implement the *same categories of technology* so a
student can understand how every piece of a modern engine works.

**Non-goals:**
- Shipping a content-complete game (teaching > content).
- Micro-optimising runtime performance before the architecture is teachable.
- Adding engine features that have no corresponding lesson or acceptance test.

---

## 2. FF15 Feature Categories

The following categories define the complete scope of what this engine must be
able to demonstrate.  Each maps to one or more milestone deliverables in
`docs/PROJECT_MILESTONES.md` and one or more subsystem entries in
`docs/FF15_REQUIREMENTS_BLUEPRINT.md`.

| # | Category | Key runtime systems | Key tool/pipeline systems |
|---|---|---|---|
| 1 | **Open-world streaming** | Zone manager, world partition, async loader | Level baker, streaming cell packager |
| 2 | **Party AI** | FSM + behaviour tree, formation system, pathfinding (A\*) | AI designer tool (data-driven), nav-mesh baker |
| 3 | **Action combat** | ATB/real-time hybrid, hit detection (physics queries), warp-strike, link strikes | Combat tuning tool, damage formula editor |
| 4 | **Quests & objectives** | Quest system, dialogue system, objective tracker | Quest editor, dialogue tree editor |
| 5 | **Cinematics** | Camera rig, cinematic sequencer, blend tree integration | Cut-scene editor, facial anim importer |
| 6 | **Vehicles** | Vehicle physics, road/terrain queries, camera chase | Vehicle data editor |
| 7 | **Weather & time-of-day** | Day/night cycle, weather state machine, sky renderer | Time-of-day curve editor |
| 8 | **Audio pipeline** | Positional audio, music layer system, event bus integration | Audio bank builder, XAudio2 backend |
| 9 | **Animation pipeline** | Skeletal animation, blend trees, IK, root motion, GPU skinning | glTF importer, animation clip cooker |
| 10 | **Physics** | Rigid body, character controller, raycasts, hit volumes | Collision mesh baker, Jolt/PhysX integration |
| 11 | **UI** | HUD, menus, minimap, dialogue box, notification system | UI layout tool, font atlas baker |
| 12 | **Save system** | Serialise ECS world state, versioned save files, auto-save | Save schema validator |
| 13 | **Build / release pipeline** | Asset cooker (`cook.exe`), PAK packager, headless validation | CI gates, golden-file contract tests |

---

## 3. Coding Standards

### 3.1 TEACHING NOTE comments

Every non-obvious decision, every pattern, and every trade-off **must** be
documented with a `// TEACHING NOTE —` comment block directly above the code it
describes.  Follow the established style seen throughout the codebase:

```cpp
// ============================================================================
// TEACHING NOTE — <Short title that explains the concept>
// ============================================================================
//
// Explain WHY this design was chosen, what alternative was rejected and why,
// and what a student should learn from reading this code.
//
// Reference the real-world equivalent (e.g. "This mirrors how FF15's Luminous
// engine handles ...").
```

### 3.2 File-per-feature

Each feature or subsystem lives in **its own translation unit** (`.hpp` + `.cpp`
pair).  Filename == feature name, always lowercase with underscores:

```
src/game/systems/water_physics.cpp      water_physics.hpp
src/game/systems/weather_system.cpp     weather_system.hpp
src/game/systems/quest_01.cpp           quest_01.hpp
src/game/world/world_streaming.cpp      world_streaming.hpp
src/engine/rendering/vulkan/render_graph.cpp
src/engine/audio/xaudio2_backend.cpp
src/engine/animation/blend_tree.cpp
src/engine/physics/character_controller.cpp
```

**Never** put two unrelated systems in the same file.  If a file grows beyond
~500 lines, split it.

### 3.3 Module boundaries

| Layer | May depend on | Must NOT depend on |
|---|---|---|
| `engine/core/` | nothing | everything else |
| `engine/ecs/` | `core/` | `game/`, tools |
| `engine/rendering/` | `core/`, `ecs/` | `game/` |
| `engine/audio/` | `core/`, `ecs/` | `game/`, rendering |
| `engine/animation/` | `core/`, `ecs/` | `game/` |
| `engine/physics/` | `core/`, `ecs/` | `game/` |
| `game/systems/` | `engine/` | tools, editor |
| `game/world/` | `engine/`, `game/systems/` | tools, editor |
| `tools/` | `engine/core/` | `game/` |

Cross-layer coupling that violates this table is a **build error**, enforced via
CMake target dependencies.

### 3.4 Naming conventions

| Construct | Convention | Example |
|---|---|---|
| Types / classes | `PascalCase` | `BlendTree`, `QuestSystem` |
| Functions / methods | `PascalCase` | `Update()`, `StartCombat()` |
| Member variables | `m_camelCase` | `m_currentWeather` |
| Local variables | `camelCase` | `deltaTime`, `entityId` |
| Constants / enums | `UPPER_SNAKE` | `MAX_PARTY_SIZE`, `TileType::WATER` |
| Files | `lower_snake_case` | `weather_system.cpp` |
| CMake targets | `lower_snake_case` | `engine_sandbox`, `game` |

### 3.5 No large monolithic files

The current `ECS.hpp` (~2 000 lines) is a deliberate teaching exception because
the ECS is studied as a whole.  All **new** code must follow file-per-feature.
Do not add new systems to existing files to avoid creating a new translation
unit.

---

## 4. Strict Milestone Roadmap

Every milestone produces an **executable or headless-validation pass** before
the next milestone starts.  This is non-negotiable.  See
`docs/PROJECT_MILESTONES.md` for the detailed definition of each milestone.

| Milestone | Name | "Done" means |
|---|---|---|
| M0 | **Vulkan sandbox** | `engine_sandbox.exe` opens a window; Vulkan clears screen; CI builds and passes |
| M1 | **Triangle** | Vertex buffer, pipeline, shaders; `engine_sandbox` renders a white triangle |
| M2 | **AssetDB + Cooker** | `cook.exe --project sample/` runs; `assetdb.json` produced; headless load validates |
| M3 | **Hello Texture + Audio** | Textured quad renders; cooked audio clip plays via XAudio2 |
| M4 | **Animation runtime** | Cooked glTF skeleton animates on screen; blend tree evaluates |
| M5 | **Physics integration** | Jolt physics; character capsule falls; raycast returns hit |
| M6 | **Editor shell** | `editor.exe` opens; scene hierarchy + inspector; save/load scene |
| M7 | **World streaming** | Zone tiles stream in/out; async loader tested headlessly |
| M8 | **Gameplay integration** | All existing gameplay systems (combat, quests, AI, camp) wired into Vulkan runtime |

---

## 5. Automation-First Approach

### 5.1 Headless validation mode

Every executable **must** support a `--headless` flag that:
- Skips window creation and rendering.
- Runs all startup, load, and simulation logic.
- Prints `[PASS]` or `[FAIL] <reason>` and exits with code 0 or 1.

```bat
:: Example — validate the project loads cleanly without a GPU
engine_sandbox.exe --headless --validate-project samples/ff15_slice/

:: Example — simulate 60 frames deterministically (physics, AI, events)
engine_sandbox.exe --headless --run-sim 60 --project samples/ff15_slice/
```

### 5.2 Contract tests (golden files)

Each cooked asset type has a **golden file** in `tests/golden/`.  The contract
test:
1. Runs the cooker on a known source asset.
2. Compares the output byte-for-byte (or schema-validates) against the golden
   file.
3. Fails CI if they differ.

This ensures tools and runtime stay in sync without manual testing.

```bash
# Run all contract tests
ctest --test-dir build -L contract
```

### 5.3 CI gates (`.github/workflows/`)

| Workflow | Trigger | What it does |
|---|---|---|
| `build-linux.yml` | push / PR | cmake + make; runs `ctest` (terminal game) |
| `build-windows.yml` | push / PR | cmake VS2022; builds `engine_sandbox`; headless validate |
| `validate-assets.yml` | push / PR touching `assets/` | runs `tools/validate-assets.py` |
| `contract-tests.yml` | push / PR | cook golden assets; diff against stored golden files |

**CI must be green before any PR is merged.**

### 5.4 Headless validation commands (current)

```bash
# Linux — build and test the terminal game
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
ctest --output-on-failure

# Windows — build and headless-validate the Vulkan sandbox
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Debug --target engine_sandbox
.\Debug\engine_sandbox.exe --headless

# Validate all asset manifests
python3 tools/validate-assets.py assets/examples/ --verbose
```

---

## 6. Windows / Vulkan Build Assumptions

These are **locked** decisions.  Do not ask about them again; do not change them
without updating this section.

| Decision | Value | Rationale |
|---|---|---|
| OS target | Windows (primary), Linux (terminal game) | Vulkan + XAudio2 require Windows; Linux keeps teaching baseline alive |
| IDE / generator | Visual Studio 2022 (`Visual Studio 17 2022`) | Most approachable debugger for Windows C++ students |
| Graphics API | Vulkan ≥ 1.3 | Modern, explicit, cross-vendor; matches AAA engine trajectory |
| Audio API | XAudio2 (Windows) | Ships with Windows SDK; zero extra install for students |
| Physics library | Jolt Physics | MIT-licensed, modern C++17, excellent teaching clarity |
| 3D source format | glTF 2.0 | Open standard; supported by Blender and every major DCC |
| Dependency manager | vcpkg (added when first third-party lib is needed) | Reproducible on any Windows machine |
| Scripting | Lua 5.4 | Already integrated; used by terminal game; keep for gameplay scripting |
| CMake minimum | 3.16 | Sufficient for `target_*` modern CMake; pre-installed on CI runners |

### 6.1 CMake options

| Option | Default (Windows) | Default (Linux) | Description |
|---|---|---|---|
| `ENGINE_ENABLE_VULKAN` | `ON` | `OFF` | Build `engine_sandbox` Vulkan target |
| `ENGINE_ENABLE_TERMINAL` | `OFF` | `ON` | Build `game` ncurses terminal target |

---

## 7. Toolchain Integration Points

The three tool repositories (`Creation-Engine`, `Audio-Engine`,
`Animation-Engine`) communicate with the runtime engine via the **shared asset
manifest contract**.  The schema lives in
`assets/schema/asset-manifest.schema.json`.

### 7.1 Manifest schema contract

```jsonc
// assets/schema/asset-manifest.schema.json  (JSON Schema draft-07)
// Fields that ALL tools must emit:
{
  "manifestVersion": "1.0.0",   // SemVer — bump MINOR when fields added
  "assets": [
    {
      "id":           "<stable-guid-or-slug>",  // never changes after creation
      "version":      "1.0.0",                  // SemVer of this asset
      "type":         "audio|texture|model|animation|tilemap|script|material",
      "source":       "relative/path/to/source.ext",
      "hash":         "<sha256-of-source>",     // detects accidental overwrite
      "dependencies": ["<id-of-other-asset>"],  // cooker resolves order
      "tags":         ["combat", "boss"],        // pipeline filtering
      // per-type extension block:
      "audio":     { ... },   // if type == "audio"
      "texture":   { ... },   // if type == "texture"
      "model":     { ... },   // if type == "model"
      "animation": { ... }    // if type == "animation"
    }
  ]
}
```

### 7.2 Cooking outputs contract

The cooker (`cook.exe`) reads source manifests and writes:

```
cooked/
├── assetdb.json          # flat registry: id → cooked path + metadata
├── textures/
│   └── <id>.dds          # BC7-compressed DDS
├── audio/
│   └── <id>.wav          # normalized PCM WAV (or XMA2 for shipping)
├── animations/
│   └── <id>.anim         # proprietary binary clip (schema in docs)
└── levels/
    └── <id>.level        # cooked zone: tile data + entity spawn list
```

### 7.3 Per-tool integration checklist

- [ ] **Creation-Engine:** emits `texture`, `model`, `material`, `tilemap`
      manifest entries → cooker produces `.dds` + `.level`
- [ ] **Audio-Engine:** emits `audio` manifest entries → cooker normalises +
      produces `.wav`; XAudio2 backend loads `.wav` at runtime
- [ ] **Animation-Engine:** emits `animation` manifest entries (source: glTF)
      → cooker produces `.anim`; animation runtime loads `.anim`

### 7.4 GUID / asset-ID rules (locked)

- IDs are **lowercase kebab-case slugs**: `noctis-idle-anim`, `battle-01-bgm`.
- Once an ID is committed, it **never changes** (break is a build error).
- Duplicate IDs across manifests are a **validator error** (CI fails).

---

## 8. PR Instructions for Copilot

When creating a PR for this project, always include:

### 8.1 PR description template

```
## Milestone
<M0 / M1 / M2 … from the roadmap table in COPILOT_CONTINUATION.md>

## What changed
- <file-per-feature list of new/modified files>

## How to build
<exact commands, copy-paste ready>

## How to validate (headless)
<exact commands that produce [PASS] output>

## Acceptance tests that now pass
- [ ] <test 1>
- [ ] <test 2>

## Teaching notes added
<list of TEACHING NOTE comment blocks added / updated>
```

### 8.2 PR size rules

- One PR = one milestone slice (see `docs/PROJECT_MILESTONES.md`).
- Maximum ~500 lines of new C++ per PR (split into smaller PRs otherwise).
- Every PR must leave the build green (CI passes).
- Every PR must update this file if any locked decision changes.

### 8.3 Commit message format

```
<type>(<scope>): <short description>

Types: feat | fix | docs | test | refactor | ci | build
Scope: engine | game | tools | docs | ci
Example: feat(engine): add Vulkan swapchain resize handling
```