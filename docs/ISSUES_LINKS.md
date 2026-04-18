# Issue Cross-Links and Checklists

<!--
TEACHING NOTE — Why this file exists
--------------------------------------
When issue modification is not available in an automated agent session, we
document the planned cross-links and checklists here so a maintainer can
copy them into the GitHub issues manually.  This file is the canonical
reference for the three tracking issues created in April 2026.
-->

This file tracks the cross-references and actionable checklists for the three
canonical tracking issues for this project. Update the GitHub issues to include
the checklists below, or use this file as the live reference.

---

## Issue #19 — Audit: Current State vs FFXV-Class Teaching Engine Goals

**URL:** https://github.com/Mikester9000/Game-Engine-for-Teaching-/issues/19

**Purpose:** Tracks the gap analysis between the current repo state and the
four stated project goals (tools suite, FFXV-class runtime, teaching comments,
lesson-plan structure).

**Cross-links:**
- Depends on nothing (this is the foundation audit).
- Feeds into #20 (Milestone Plan) and #21 (Refactor).

### Checklist for Issue #19

```markdown
## Audit Checklist

### 1 · Tools Suite
- [ ] Asset cooker (`cook.exe`) compiles and produces `assetdb.json`
- [ ] `cook.exe` integrated into CI (`build-windows.yml`)
- [ ] Python audio authoring tool end-to-end tested (32 tests green)
- [ ] Python animation authoring tool end-to-end tested (11 tests green)
- [ ] Qt 6 editor builds and opens a sample scene
- [ ] `--validate-project` flag in `engine_sandbox` exits 0

### 2 · FFXV-Class Runtime
- [ ] Vulkan PBR pipeline (IBL + directional light) implemented
- [ ] GPU-skinned skeletal mesh renders on screen
- [ ] Jolt Physics: character capsule + raycasts working
- [ ] XAudio2 backend: positional 3D audio plays in engine
- [ ] Open-world zone streaming: no hitch during zone transition
- [ ] Dynamic sky (procedural time-of-day) renders

### 3 · Teaching Comments
- [ ] CI gate (`teaching-standards.yml`) is green on `main`
- [ ] `--all` audit of `src/` shows zero NOTE violations
- [ ] `--all` audit of `tools/` shows zero NOTE violations
- [ ] Every new subsystem PR includes TEACHING NOTE blocks

### 4 · Lesson-Plan Structure
- [ ] `docs/` has a per-subsystem lesson doc for each completed system
- [ ] `samples/vertical_slice_project/` demonstrates every subsystem
- [ ] README curriculum path section is up to date
- [ ] Contract tests (`.github/workflows/contract-tests.yml`) are green

### Dependencies
- Feeds → #20 (Milestone Plan)
- Feeds → #21 (Refactor)
```

---

## Issue #20 — Milestone Plan: Copilot-First FFXV Teaching Engine

**URL:** https://github.com/Mikester9000/Game-Engine-for-Teaching-/issues/20

**Purpose:** Tracks the ordered execution plan for delivering all engine
milestones, optimised for solo Copilot authoring.

**Cross-links:**
- **Depends on** #19 (Audit) for gap analysis and prioritisation.
- **Informs** #21 (Refactor) — architecture improvements unblock milestones.

### Checklist for Issue #20

```markdown
## Milestone Execution Checklist

### M2 — AssetDB + Cooker
- [ ] `src/tools/cook/cook_main.cpp` parses `AssetRegistry.json`
- [ ] `src/engine/assets/asset_db.hpp/.cpp` — Load / GetCookedPath / Has
- [ ] `src/engine/assets/asset_loader.hpp/.cpp` — synchronous LoadRaw
- [ ] `engine_sandbox --validate-project` exits 0
- [ ] `tests/golden/assetdb_expected.json` golden file added
- [ ] `.github/workflows/contract-tests.yml` CI green

### M3 — Texture + Audio
- [ ] Vulkan texture (DDS/BC7) loads and renders (textured quad visible)
- [ ] `src/engine/audio/xaudio2_backend.hpp/.cpp` implemented
- [ ] Cooked audio plays through XAudio2 in engine_sandbox
- [ ] TEACHING NOTE blocks in all new files

### M4 — Animation Runtime
- [ ] `src/engine/animation/skeleton.hpp/.cpp`
- [ ] `src/engine/animation/anim_clip.hpp/.cpp`
- [ ] `src/engine/animation/blend_tree.hpp/.cpp`
- [ ] GPU skinning UBO uploaded; animated character visible on screen

### M5 — Physics
- [ ] Jolt Physics via vcpkg; character capsule falls + lands
- [ ] Raycasts return correct hit points
- [ ] `PhysicsWorld` wrapper integrated into ECS update loop

### M6 — Editor Inspector + Play-in-Engine
- [ ] Entity inspector panel shows / edits component values
- [ ] Scene hierarchy panel lists entities
- [ ] "Play in Engine" button launches `engine_sandbox.exe` with scene file

### M7 — World Streaming
- [ ] `src/engine/world/world_streaming.hpp/.cpp` — proximity load/evict
- [ ] No frame spike > 2ms during zone transition
- [ ] Async loader uses `std::thread` + lock-free queue

### M8 — Gameplay Integration
- [ ] All terminal gameplay systems (combat, AI, quests, weather) wired into Vulkan runtime
- [ ] Party, inventory, magic, shop, camp all function in Vulkan game

### Ongoing (every milestone)
- [ ] CI teaching-standards gate stays green
- [ ] `samples/vertical_slice_project/` updated with each new subsystem
- [ ] COPILOT_CONTINUATION.md status table updated after each milestone

### Dependencies
- Requires → #19 (Audit) for gap analysis
- Informs  → #21 (Refactor) for architecture improvements
```

---

## Issue #21 — Refactor: Copilot-Optimized Architecture for Teaching Engine

**URL:** https://github.com/Mikester9000/Game-Engine-for-Teaching-/issues/21

**Purpose:** Tracks the architecture improvements needed to keep the codebase
maintainable and Copilot-friendly as it grows toward FFXV-class complexity.

**Cross-links:**
- **Depends on** #19 (Audit) to identify which files need refactoring.
- **Depends on** #20 (Milestone Plan) for prioritisation order.

### Checklist for Issue #21

```markdown
## Refactor Checklist

### CI / Automation
- [x] `tools/ci/check_teaching_standards.py` added — file size + TEACHING NOTE gate
- [x] `.github/workflows/teaching-standards.yml` added — runs on every PR
- [ ] CI gate extended to check forbidden cross-module includes (optional)
- [ ] Curriculum index script added (extracts all TEACHING NOTE blocks to docs)

### File Modularity
- [ ] Audit `src/` for any file > 500 lines (besides allowlisted exceptions)
- [ ] Audit `tools/` for any file > 500 lines (besides allowlisted exceptions)
- [ ] Split any non-allowlisted file that exceeds the limit

### TEACHING NOTE Coverage
- [ ] Run `python3 tools/ci/check_teaching_standards.py --all` and resolve all NOTE violations
- [ ] Ensure every subsystem `.cpp` / `.hpp` pair has at least one TEACHING NOTE

### Module Boundary Enforcement
- [ ] Document which modules may include which headers in `docs/ARCHITECTURE.md`
- [ ] Add CMake interface targets to make forbidden includes a compile error

### Documentation
- [ ] `docs/COPILOT_CONTINUATION.md` §CI Gates section describes new checks
- [x] `docs/ISSUES_LINKS.md` created — cross-links issues #19 / #20 / #21

### Dependencies
- Requires → #19 (Audit) for violation inventory
- Requires → #20 (Milestone Plan) for priority ordering
```

---

## Manual Steps for Maintainer

The following updates should be made directly in GitHub Issues:

1. **Issue #19** — Add the checklist from the "Checklist for Issue #19" section above
   as a new comment or edit the issue body.  Add the labels `audit` and `teaching`.

2. **Issue #20** — Add the checklist from the "Checklist for Issue #20" section above.
   Add the following cross-references to the issue body:
   - "Depends on #19 (Audit) for gap analysis and prioritisation."
   - "Informs #21 (Refactor) — architecture improvements unblock milestones."
   Add the labels `milestone`, `teaching`, and `FFXV`.

3. **Issue #21** — Add the checklist from the "Checklist for Issue #21" section above.
   Add the following cross-references to the issue body:
   - "Depends on #19 (Audit) to identify which files need refactoring."
   - "Depends on #20 (Milestone Plan) for prioritisation order."
   Add the labels `refactor`, `teaching`, and `copilot`.
