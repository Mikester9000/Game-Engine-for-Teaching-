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
- Shipping a *content*-complete game (hours of story/world content is not the goal).
- Micro-optimising runtime performance before the architecture is teachable.
- Adding engine features that have no corresponding lesson or acceptance test.

**What "non-content-complete" does NOT mean:**
Each technology category (visuals, physics, sound, gameplay) must be implemented
at the same *class* of solution FF15 uses — real PBR shading, real rigid-body
physics, real positional audio, real action combat — not a stub or toy version.
A teaching engine that uses placeholder geometry and fake physics teaches nothing.

---

### 1.1 Definition of Done — Project Completion Criteria

The project is considered **complete** when all of the following are true:

1. **All 13 subsystems** in `docs/FF15_REQUIREMENTS_BLUEPRINT.md` show ✅ in
   the Runtime, Tool, and Tests columns of the Completion Matrix.

2. **FF15-comparable quality bar is met** across every domain:

   | Domain | Minimum "complete" quality |
   |--------|---------------------------|
   | **Visuals** | PBR rendering (IBL + directional shadows + bloom + tonemapping); GPU-skinned skeletal meshes; dynamic sky (procedural time-of-day + weather FX); Vulkan ≥ 1.3 on Windows |
   | **Physics** | Rigid-body simulation (Jolt Physics); character capsule with step-up / slopes; vehicle wheel-ray physics; physics-based hit volumes for combat |
   | **Sound** | XAudio2 backend; positional 3D audio with distance attenuation; layered music system (battle / exploration / idle blend); event-driven SFX triggers |
   | **Gameplay** | Real-time action combat (warp-strike, link-strike, combo chains, ATB); open-world zone streaming without loading screens; party AI (behaviour tree + formation); quest + dialogue system; save/load (15 slots + auto-save at camp) |
   | **Tools** | Asset cooker (`cook.exe`); texture / mesh / audio / animation import pipeline; Qt 6 scene editor with Play-in-Engine; Python authoring tools for audio and animation |
   | **Teaching** | Every non-trivial pattern has a `// TEACHING NOTE` block; docs in `docs/`; `samples/vertical_slice_project/` demonstrates each subsystem end-to-end |

3. **A student can fully teach themselves** modern game engine development by
   reading only this repository's source code and running its samples.  If any
   subsystem requires consulting external documentation to understand *why* a
   design decision was made, it is not done.

4. **All CI gates are green** — headless validation passes for every subsystem,
   golden-file contract tests pass, and `samples/vertical_slice_project/` cooks
   and runs without errors.

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

> **Current position:** M0 ✅ and M1 ✅ are complete.  **M2 is the active milestone.**

| Milestone | Name | Status | "Done" means |
|---|---|---|---|
| M0 | **Vulkan sandbox** | ✅ Complete | `engine_sandbox.exe` opens a window; Vulkan clears screen; `build-linux.yml` CI passes |
| M1 | **Triangle** | ✅ Complete | `VulkanPipeline` + `VulkanMesh`; `shaders/triangle.vert/.frag` compiled to SPIR-V; coloured triangle draws; `--headless --scene triangle` exits 0 |
| M2 | **AssetDB + Cooker** | ⬜ Next | `src/tools/cook/cook_main.cpp` (`cook.exe`); `src/engine/assets/asset_db.hpp/.cpp`; `engine_sandbox --headless --validate-project` exits 0; `contract-tests.yml` CI; `build-windows.yml` CI |
| M3 | **Hello Texture + Audio** | ⬜ | Vulkan texture (DDS/BC7); descriptor sets; `src/engine/audio/xaudio2_backend.hpp/.cpp`; textured quad renders; cooked WAV plays |
| M4 | **Animation runtime** | ⬜ | `src/engine/animation/skeleton.hpp/.cpp` + `anim_clip` + `blend_tree` + `gpu_skinning`; animated character on screen |
| M5 | **Physics integration** | ⬜ | Jolt Physics via vcpkg; `src/engine/physics/`; character capsule falls; raycast returns hit; headless physics tests pass |
| M6 | **Editor improvements** | ⬜ | Entity inspector panel; scene ECS serialisation; Play-in-Engine button |
| M7 | **World streaming** | ⬜ | `src/engine/world/world_streaming.hpp/.cpp`; async loader; headless streaming tests pass |
| M8 | **Gameplay integration** | ⬜ | All gameplay systems (combat, AI, quests, camp, weather) wired into Vulkan runtime; Lua hooks fire; 300-frame headless sim passes |

**Post-M8 work (in order):**
1. `src/engine/ui/` — Vulkan HUD + menu stack + font renderer
2. `src/engine/save/` — ECS snapshot save/load (15 slots + auto-save)
3. Full PBR pipeline — IBL, directional shadow map, bloom, tonemapping
4. Dynamic sky + weather VFX — `sky_renderer.hpp/.cpp`, `weather_fx.hpp/.cpp`
5. `src/game/systems/dialogue_system.hpp/.cpp` — dialogue tree + NPC conversations
6. `src/engine/ai/behaviour_tree.hpp/.cpp` — behaviour tree to replace/augment FSM
7. `src/engine/ai/formation_system.hpp/.cpp` — party formation
8. Vehicle physics — `src/game/systems/vehicle_system.hpp/.cpp` + `src/engine/physics/vehicle_physics.hpp/.cpp`
9. `src/engine/cinematics/` — cinematic sequencer + camera rig
10. `src/tools/pak/pak_main.cpp` — PAK packager for release

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

| Workflow | Trigger | What it does | Status |
|---|---|---|---|
| `build-linux.yml` | push / PR | cmake + make; builds terminal `game`; runs 32+11 Python pytest | ✅ exists |
| `validate-assets.yml` | push / PR touching `assets/` | runs `tools/validate-assets.py` | ✅ exists |
| `build-windows.yml` | push / PR | cmake VS2022; builds `engine_sandbox` + `cook`; headless validate | ⬜ **create for M2** |
| `contract-tests.yml` | push / PR | cook golden assets; diff against stored golden files in `tests/golden/` | ⬜ **create for M2** |

**CI must be green before any PR is merged.**

#### `build-windows.yml` template (create for M2)

```yaml
name: Build Windows (engine_sandbox)
on: [push, pull_request]
jobs:
  build:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - name: Setup Vulkan SDK
        uses: humbletim/setup-vulkan-sdk@v1.2.0
        with:
          vulkan-query-version: 1.3.250.0
          vulkan-components: Vulkan-Headers, Vulkan-Loader
          vulkan-use-cache: true
      - name: Configure CMake
        run: cmake --preset windows-debug
      - name: Build engine_sandbox and cook
        run: cmake --build --preset windows-debug --target engine_sandbox cook
      - name: Headless validate engine
        run: .\build\windows-debug\Debug\engine_sandbox.exe --headless
      - name: Cook sample project (M2+)
        run: .\build\windows-debug\Debug\cook.exe --project samples/vertical_slice_project/
```

#### `contract-tests.yml` template (create for M2)

```yaml
name: Contract Tests (golden-file)
on: [push, pull_request]
jobs:
  contract:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build cook.exe
        run: |
          cmake --preset windows-debug
          cmake --build --preset windows-debug --target cook
      - name: Cook sample project
        run: .\build\windows-debug\Debug\cook.exe --project samples/vertical_slice_project/
      - name: Diff against golden assetdb
        run: |
          python -c "
          import json, sys
          actual   = json.load(open('samples/vertical_slice_project/Cooked/assetdb.json'))
          expected = json.load(open('tests/golden/assetdb_expected.json'))
          if actual != expected:
              print('[FAIL] assetdb.json does not match golden file'); sys.exit(1)
          print('[PASS] assetdb.json matches golden file')
          "
```

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
| `ENGINE_ENABLE_PHYSICS` | `OFF` | `OFF` | Build Jolt Physics integration (M5) |

### 6.2 vcpkg dependency manifest

When the first third-party C++ dependency is added (M2 = `nlohmann-json`), create
`vcpkg.json` in the repository root.  **Do not add dependencies to CMakeLists.txt
without also adding them here.**

```json
{
  "name": "educational-game-engine",
  "version-string": "1.0.0",
  "dependencies": [
    "nlohmann-json"
  ]
}
```

Per-milestone dependencies to add as work progresses:

| Milestone | vcpkg package | Used by |
|-----------|-------------|---------|
| M2 | `nlohmann-json` | `cook.exe` — parse/write JSON manifests |
| M3 | `directxtex` | Vulkan texture — DDS/BC7 compress/decompress |
| M4 | `tinygltf` | Animation — load glTF skeleton + clips |
| M5 | `joltphysics` | Physics — Jolt `PhysicsSystem` wrapper |

Integrate with CMake by adding to `CMakePresets.json` `cacheVariables`:
```json
"CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
```

---

## 7. Toolchain Integration Points

The three authoring tools (`tools/audio_authoring` (Audio Engine),
`tools/anim_authoring` (Animation Engine), and the Qt editor `editor/`)
communicate with the runtime engine via the **shared asset
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
- [ ] **`tools/audio_authoring` (Audio Engine):** emits `audio` manifest entries → cooker normalises +
      produces `.wav`; XAudio2 backend loads `.wav` at runtime
- [ ] **`tools/anim_authoring` (Animation Engine):** emits `animation` manifest entries (source: glTF)
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

---

## 9. CI Gates — Teaching Standards Enforcement

> **Added in April 2026 (issues #19 / #21).**
> All CI gates below run automatically on every push and pull request that
> touches source code.  A PR cannot be merged if any gate is red.

### 9.1 Overview of CI workflows

| Workflow file | What it checks | Trigger |
|---|---|---|
| `build-linux.yml` | Terminal game compiles; Python tool tests pass | `src/**`, `tools/**` push/PR |
| `validate-assets.yml` | Asset manifests pass JSON Schema validation | `assets/**` push/PR |
| `teaching-standards.yml` | File size ≤ 500 lines; TEACHING NOTE present | `src/**`, `tools/**`, `editor/**`, `shared/**` push/PR |

### 9.2 Teaching-standards gate (new)

The `teaching-standards.yml` workflow runs `tools/ci/check_teaching_standards.py`
against every file added or modified in a PR.  It enforces two rules:

#### Rule 1 — File size limit (500 lines)

Every new or changed `.cpp`, `.hpp`, `.h`, `.py`, `.cmake`, `.yml`, or `.yaml`
file must stay under **500 lines**.

*Rationale:* A Copilot session that reads one focused 200-line file produces
much higher-quality output than one that has to ingest a 2 000-line monolith.
Small files are also easier for students to read and understand.

#### Rule 2 — TEACHING NOTE requirement

Every new or changed C++ / Python file with **≥ 30 lines** must contain at
least one `// TEACHING NOTE` (C++) or `# TEACHING NOTE` (Python) block.

*Rationale:* Teaching notes capture the *why* behind design decisions at the
exact point where a student reads the code.  A note in a separate doc is easy
to miss; a note next to the code is impossible to miss.

### 9.3 How to add a file-size exception

If a file intentionally exceeds 500 lines (e.g. a new system that is taught
as a single cohesive unit), add it to `ALLOWLIST_SIZE` in
`tools/ci/check_teaching_standards.py` with a short comment explaining why
the exception is justified.  Include the allowlist change in the same PR as
the large file.

```python
# In tools/ci/check_teaching_standards.py — ALLOWLIST_SIZE set:
ALLOWLIST_SIZE: frozenset = frozenset({
    ...
    # MySystem.hpp teaches the full XYZ pattern as one cohesive unit.
    "src/engine/mysystem/MySystem.hpp",
})
```

### 9.4 How to run the checks locally

```bash
# Check files changed vs. main branch:
python3 tools/ci/check_teaching_standards.py --git-diff origin/main

# Check specific files you are about to commit:
python3 tools/ci/check_teaching_standards.py src/engine/audio/xaudio2_backend.cpp

# Full audit of every tracked file (useful for periodic housekeeping):
python3 tools/ci/check_teaching_standards.py --all
```

### 9.5 Issue tracking

The three canonical tracking issues for the teaching-standards work are:

| Issue | Title | Role |
|---|---|---|
| [#19](https://github.com/Mikester9000/Game-Engine-for-Teaching-/issues/19) | Audit: Current State vs FFXV-Class Teaching Engine Goals | Foundation audit — gap analysis |
| [#20](https://github.com/Mikester9000/Game-Engine-for-Teaching-/issues/20) | Milestone Plan: Copilot-First FFXV Teaching Engine | Ordered execution plan |
| [#21](https://github.com/Mikester9000/Game-Engine-for-Teaching-/issues/21) | Refactor: Copilot-Optimized Architecture for Teaching Engine | Architecture improvements |

See `docs/ISSUES_LINKS.md` for full cross-link details and actionable checklists.