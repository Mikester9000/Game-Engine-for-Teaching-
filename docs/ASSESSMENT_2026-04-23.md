# Monorepo State Assessment — 2026-04-23

> **Supersedes:** `docs/ASSESSMENT_2026-04-22.md` (see that file for the baseline snapshot).
> This document captures a follow-up assessment after PRs #76 (M25) and #77 (M24)
> were merged on 2026-04-23, and serves as the authoritative reference for the
> **next set of Copilot sessions**.

---

## Executive Summary

Since the 2026-04-22 assessment, three milestones that were listed as "not started"
have progressed significantly:

| Milestone | 2026-04-22 Status | 2026-04-23 Status |
|-----------|-------------------|-------------------|
| M24 — Vertical Slice Content Population | ⬜ Not started | 🔨 Assets populated; cook run not committed |
| M25 — Terrain / World Geometry Path | ⬜ Not started | ✅ Complete (PR #76) |
| M26 — Save-System CI Hardening | ⬜ Not started | ✅ Complete (all CI wired) |

The largest remaining gap is now **cook pipeline completeness** — `Cooked/` is
still an empty directory (only `.gitkeep`) even though `Content/` is populated with
40 textures, 32 audio clips, and 19 animation files (PR #77).  A student cloning
the repo today cannot demonstrate the full author → cook → load path without
running `cook_assets.py` manually.

The **Vulkan catch-up (M14) remains deferred** per the active D3D11-first policy.

---

## Subsystem State (2026-04-23)

### ✅ Fully complete

| Subsystem | Evidence |
|-----------|----------|
| D3D11 Renderer (PBR, IBL, shadows, bloom, sky, skinning, terrain, SDF) | All scene CI scenes pass on WARP |
| World Streaming | `streaming_load`, `streaming_evict`, `streaming_async`, `m8_streaming` CI |
| Animation | Skeleton + clip + blend + IK + GPU skinning; 11 Python tests |
| Audio (XAudio2 + X3DAudio) | `audio_3d_test` CI; 32 Python tests |
| Physics (Jolt) | `physics_test`, `vehicle_test`, `terrain_test` CI; `BakeTerrainCollider` ✅ |
| AI (FSM + BT + Formation + NavMesh) | `bt_test` CI (6 sub-tests) |
| Cinematics | `cinematic_test` CI; `bake-cinematic` tool; `CinematicEditorPanel` |
| Combat | `combat_test` CI; `ComboSystem` FSM + config |
| Quests & Dialogue | `quest_test` + `dialogue_test` CI; `quest_baker` tool (15 pytest) |
| Save System + CI | `save_test` CI (engine-only: skip; save-test job: full 3-subtest suite) |
| Terrain rendering + collision | `terrain_test` CI (1: GPU init, 2: heightmap, 3: physics drop) |
| Editor (Dear ImGui) | `build-windows-editor` CI; Play-in-Engine, asset drag-drop |
| Cook pipeline | `cook.exe`, `pak.exe`, contract CI (13 pytest), golden-file diffs |
| M23 Authored Ingestion | `pbr_ibl` scene loads authored mesh/material; fallback SRVs on missing textures |
| M24 Content (assets) | 40 textures, 32 WAVs, 19 anim files present in `Content/` (PR #77) |

### 🔨 In progress / partially complete

| Subsystem | Gap | Priority |
|-----------|-----|----------|
| M24 — Cook verification | `Cooked/` is empty; `cook_assets.py` exits but output not committed | **High** |
| M24 — Texture format | `cook_assets.py` copies PNGs as `.tex` (not DDS/RGBA8); D3D11 loader expects DDS magic or RGBA8 raw | **High** |
| Authored content CI scene | No dedicated `--scene authored_content` hard-failure test that proves authored textures load (not fallback) | **Medium** |

### ⬜ Not yet started (Tool-column gaps)

| Subsystem | Missing Tool | Blueprint Section |
|-----------|--------------|-------------------|
| Physics (§10) | Collision mesh baker — `.obj` → cooked convex/mesh shape binary | §10 Tool ⬜ |
| UI (§11) | Font atlas baker — `.ttf` → cooked `.font` (SDF atlas + glyph metrics) | §11 Tool ⬜ |
| Vehicles (§6) | Road spline baker — `road.json` → cooked binary spline for `VehicleSystem` | §6 Tool ⬜ |
| Cinematics (§5) | Timed audio event CI: `--scene cinematic_test` sub-test for ±1-frame audio timing | §5 Acceptance ⬜ |
| Audio (§8) | WAV normalise + XMA2 cook step (shipping quality; not required for teaching bar) | §8 Tool ⬜ (low priority) |

### ⬜ Deferred

| Subsystem | Reason |
|-----------|--------|
| Vulkan catch-up (M14) | D3D11-first policy — resume only after all D3D11 early-beta goals are green |

---

## Documentation Corrections Applied (2026-04-23)

The following docs were updated to match actual code state (these were stale after
PRs #76 and #77 merged):

| File | Stale claim | Corrected to |
|------|-------------|--------------|
| `docs/PROJECT_MILESTONES.md` §M25 | "⬜ Not started" | ✅ Complete |
| `docs/PROJECT_MILESTONES.md` §M26 | "⬜ Not started" | ✅ Complete |
| `docs/PROJECT_MILESTONES.md` §M24 | "⬜ Not started" | 🔨 Assets populated; cook pending |
| `docs/PROJECT_MILESTONES.md` Future Milestones table | M24/M25/M26 all ⬜ | Updated individually |
| `docs/PROJECT_MILESTONES.md` Progress Summary | M24/M25/M26 all ⬜ | Updated individually |
| `docs/FF15_REQUIREMENTS_BLUEPRINT.md` Completion Matrix | No Terrain row | Terrain row added (#14) |
| `docs/FF15_REQUIREMENTS_BLUEPRINT.md` D3D11 Early-Beta Gaps table | M24/M25/M26 all ⬜ | Updated individually |
| `docs/ROADMAP.md` Remaining Work table | M24/M25/M26 all pending | Updated individually |

---

## Prioritized Plan for Next Copilot Sessions

### Priority 1 — Complete M24 cook verification *(1–3 hours)*

**Why first:** The acceptance criterion for M24 requires `cook_assets.py` to exit 0
**and** `Cooked/` to be populated.  Currently `Cooked/` contains only `.gitkeep`.
Until the cook runs successfully and its output is committed (or CI runs it), M24
cannot be marked ✅.

**Work items:**
1. Run `python samples/vertical_slice_project/cook_assets.py` locally and fix any errors.
2. Verify `Cooked/Textures/`, `Cooked/Audio/`, `Cooked/Anim/`, `Cooked/Materials/`,
   `Cooked/Terrain/` are all populated.
3. Run `engine_sandbox.exe --headless --validate-project samples/vertical_slice_project/` and confirm it exits 0.
4. Add a `cook_assets.py` run step to `contract-tests.yml` or `build-windows.yml` so CI
   always produces a cooked output and validates it.

**Key file:** `samples/vertical_slice_project/cook_assets.py` (the `cook_textures`,
`cook_audio`, `cook_animations` stubs at lines 211–538).

**Acceptance:** `cook_assets.py` exits 0; `Cooked/` is non-empty; `--validate-project` exits 0.

---

### Priority 2 — Fix texture cook format (PNG → loadable format) *(4–6 hours)*

**Why:** `cook_assets.py` copies PNGs to `Cooked/Textures/*.tex` with no format
conversion.  `D3D11Renderer::TryLoadAuthoredMesh` and the authored texture slots
(`d3d11_texture.cpp`) expect DDS magic bytes or a raw RGBA8 header.  Without this
fix, authored textures silently fall back to the 1×1 white SRV in every pbr_ibl run,
meaning the authored content is never actually exercised.

**Recommended approach (Option B — minimal, no Windows SDK dependency):**
- Use Python `Pillow` (or the existing `tools/audio_authoring` PNG decode pattern)
  to read each PNG and write a minimal raw header + RGBA8 pixel data as the `.tex`
  output.
- Update `d3d11_texture.cpp` to recognise the `.tex` raw format (4-byte magic
  `TEX0`, width/height, RGBA8 data).
- Add a cook step test: cook one PNG → `.tex`; confirm `d3d11_texture` loads it.

**Alternative approach (Option C — real DDS/BC cook, higher effort):**
- Use `directxtex` (already in `vcpkg.json`) in `cook.exe` to transcode PNG → BC1/BC3
  DDS for character albedo, terrain albedo, and prop textures.
- Write cooked files as `*.dds` (DDS magic already supported by `d3d11_texture.cpp`).

**Key files:**
- `samples/vertical_slice_project/cook_assets.py` — `cook_textures()` stub, line 211
- `src/engine/rendering/d3d11/d3d11_texture.hpp/.cpp` — DDS/BC parser
- `src/engine/rendering/d3d11/D3D11Renderer.cpp:2495` — `TryLoadAuthoredMesh` DDS-first path

**Acceptance:** After cook, `pbr_ibl` scene loads `hero_albedo.tex` (not the 1×1 fallback);
confirm via a log line or a new sub-test in `pbr_ibl`.

---

### Priority 3 — Collision mesh baker tool *(4–6 hours)*

**Why:** Physics §10 in the Completion Matrix has Tool column ⬜ — the only subsystem
gap that blocks the Physics row from reaching ✅/✅/✅.  Students who want to add a
building or prop to the world have no tool to author its collision shape.

**Work items:**
1. Add `bake-collision` subcommand to `tools/creation_engine.py`:
   - Input: an OBJ file (`Content/Physics/<id>.phys.obj`).
   - Output: a cooked binary (`Cooked/Physics/<id>.phys`) containing vertex/index data
     for a convex-hull shape or a triangle-mesh shape.
   - Simple binary format: magic `PHY1` + vertex count + index count + float[] + uint32[].
2. Add sample `Content/Physics/arena_plane.phys.obj` (already exists as `arena_plane.obj`
   in `Content/Meshes/`).
3. Add pytest coverage (follow `bake_navmesh`/`bake_terrain` pattern in
   `tools/tests/test_creation_engine_bakers.py`).
4. Register `arena_plane` in `AssetRegistry.json` as `type: "collision_mesh"`.
5. Wire `BakeTerrainCollider`-style loading into `PhysicsWorld` for convex props.

**Key files:**
- `tools/creation_engine.py` — add `bake_collision()` + `bake-collision` CLI entry
- `tools/tests/test_creation_engine_bakers.py` — add pytest
- `src/engine/physics/physics_world.hpp/.cpp` — optionally add `CreateConvexMeshFromCooked()`

**Acceptance:** `bake-collision arena_plane.phys.obj arena_plane.phys` exits 0 and
produces a binary file; pytest passes; tool column for Physics becomes ✅.

---

### Priority 4 — Font atlas baker tool *(4–6 hours)*

**Why:** UI §11 in the Completion Matrix has Tool column ⬜.  The `FontRenderer`
generates its SDF atlas at runtime (startup cost, no offline iteration).  A production
font pipeline bakes the atlas offline so artists can tune glyph sets and sizes.

**Work items:**
1. Add `bake-font` subcommand to `tools/creation_engine.py`:
   - Input: a `.ttf` or `.otf` path + optional glyph range.
   - Output: a cooked `.font` binary (magic `FNT1` + atlas width/height +
     glyph count + per-glyph UV/metric table + R8 SDF pixel data).
   - Can re-use the same SDF generation algorithm already in `FontRenderer::Init()`
     (8×8 bitmap → SDF), ported to Python.
2. Add sample `Content/UI/Fonts/ffxv_ui.ttf` placeholder (or use a free font).
3. Add pytest coverage.
4. Update `FontRenderer` to support loading from a cooked `.font` file as an
   alternative to the runtime-generated atlas path.

**Key files:**
- `tools/creation_engine.py` — add `bake_font()` + `bake-font` CLI entry
- `tools/tests/test_creation_engine_bakers.py` — add pytest
- `src/engine/ui/font_renderer.hpp/.cpp` — add `LoadCooked()` method

**Acceptance:** `bake-font ffxv_ui.ttf ffxv_ui.font` exits 0; pytest passes;
`FontRenderer::LoadCooked()` loads the file and renders text correctly.

---

### Priority 5 — Road spline baker tool *(3–5 hours)*

**Why:** Vehicles §6 in the Completion Matrix has Tool column ⬜.  The `VehicleSystem`
exists but has no authored road data to follow; the vehicle moves in free space.
This is the last ⬜ Tool entry for any non-deferred subsystem.

**Work items:**
1. Add `bake-road` subcommand to `tools/creation_engine.py`:
   - Input: a `road.json` with an ordered list of 3D waypoints.
   - Output: a cooked `.road` binary (magic `RD01` + waypoint count + float3[] positions).
2. Add sample `Content/Roads/regalia_route.road.json` to the vertical slice project.
3. Add pytest coverage.
4. Optionally: read the cooked road in `VehicleSystem::Update` and apply the
   nearest-segment heading to the vehicle's steering — this would complete the
   "Regalia follows road" acceptance criterion.

**Key files:**
- `tools/creation_engine.py` — add `bake_road()` + `bake-road` CLI entry
- `tools/tests/test_creation_engine_bakers.py` — add pytest
- `src/engine/vehicle/vehicle_system.hpp/.cpp` — optional road-following integration

**Acceptance:** `bake-road regalia_route.road.json regalia_route.road` exits 0;
pytest passes; (optional) `vehicle_test` extended to assert vehicle follows heading.

---

### Priority 6 — Cinematic timed audio event CI test *(2–3 hours)*

**Why:** §5 Cinematics acceptance tests list "Timed audio event fires within ±1 frame
of declared time ⬜" — the only listed CI requirement not yet covered.  The
`CinematicSequencer` has callbacks for shot changes and completion but no audio event
integration.

**Work items:**
1. Add `AudioEvent` (timestamp + clip ID) to `CinematicSequencer` timeline:
   - `AddAudioEvent(float t, const std::string& clipID)`
   - `Tick(dt)` checks audio events and calls `AudioSystem::Play` at the right frame.
2. Add a 4th sub-test to `cinematic_test` scene:
   - Declare an audio event at t=0.05 s; tick past it; assert the event fired.
3. Update CI comment in `build-windows.yml` to document the new test.

**Key files:**
- `src/engine/cinematics/cinematic_sequencer.hpp/.cpp`
- `src/sandbox/main.cpp` — `--scene cinematic_test` block
- `.github/workflows/build-windows.yml` — cinematic_test CI comment

**Acceptance:** `--headless --scene cinematic_test` passes 4 sub-tests (up from 3);
audio event fires within ±1 frame of declaration.

---

### Priority 7 — Authored content end-to-end CI scene *(3–5 hours)*

**Why:** M23 is marked ✅ but there is no dedicated hard-failure CI test that proves
authored textures actually load (not the 1×1 fallback SRV).  The `pbr_ibl` tests
only confirm the scene renders without crashing.

**Work items:**
1. Add `--scene authored_content` to `main.cpp`:
   - Set `ENGINE_PROJECT_ROOT` to `samples/vertical_slice_project/`.
   - Call `LoadScene("pbr_ibl", shaderDir)`.
   - Assert that at least one authored texture was bound (check via a new
     `D3D11Renderer::GetAuthoredTextureCount()` accessor or a log message).
   - Fail with `[FAIL]` if all texture slots are fallback SRVs.
2. Add step to `build-windows.yml` after the cook step so the test always runs
   against freshly cooked assets.
3. This test is a hard gate: if the cook format is wrong (Priority 2), this scene
   fails — which is exactly the right signal.

**Key files:**
- `src/sandbox/main.cpp` — add `authored_content` scene
- `src/engine/rendering/d3d11/D3D11Renderer.hpp/.cpp` — optional accessor
- `.github/workflows/build-windows.yml` — add step

**Acceptance:** `--headless --scene authored_content` passes and prints at least one
`[OK] Authored texture bound: <name>` line (not all fallbacks).

---

### Priority 8 — Vulkan catch-up (M14) *(DEFERRED — long term)*

Resume only when all D3D11 early-beta slice goals above are green.

**Scope when resumed:**
- `vulkan_texture.hpp/.cpp` — DDS/BC7 to `VkImage`
- `vulkan_descriptor.hpp/.cpp` — descriptor pool + set
- `shaders/textured_quad.vert/.frag` — GLSL textured quad
- Vulkan PBR pipeline (GGX + IBL)
- Vulkan GPU skinning shaders
- Vulkan dynamic sky + weather FX pipeline
- Vulkan HUD (imgui Vulkan binding)
- `build-windows-vulkan` CI job

---

## Session Hygiene Checklist

Run at the **start and end of every Copilot session**:

```bash
# 1. Regenerate curriculum index (catches any new TEACHING NOTE blocks)
python scripts/extract_teaching_notes.py --repo-root .

# 2. Commit if changed (architecture-lint CI will fail if this is stale)
git diff --exit-code docs/CURRICULUM_INDEX.md || \
    git add docs/CURRICULUM_INDEX.md && git commit -m "docs: regenerate CURRICULUM_INDEX.md"

# 3. (Local only) Run Python tests to verify no regressions
pip install -r requirements-dev.txt
python3 -m pytest -q

# 4. (Local only) Build sanity check
cmake --build build -j$(nproc)
```

Update the following status tables when a milestone changes:
- `docs/PROJECT_MILESTONES.md` — `§MXX Status:` header + Future Milestones table + Milestone Progress Summary
- `docs/FF15_REQUIREMENTS_BLUEPRINT.md` — Subsystem Completion Matrix + D3D11 Early-Beta Gaps table
- `docs/ROADMAP.md` — Remaining Work table
- `.github/copilot-instructions.md` — "Current Development Status" table + "Next Milestone" priority table

---

## Evidence Anchors (2026-04-23)

| Claim | Evidence |
|-------|----------|
| M25 ✅ | `src/engine/rendering/d3d11/terrain_renderer.hpp/.cpp` (548 lines); `src/engine/physics/terrain_collision.hpp/.cpp`; `shaders/terrain.vs/ps.hlsl`; `--scene terrain_test` in `main.cpp:4141`; `build-windows.yml` steps at lines 629–631, 782–784 |
| M26 ✅ | `--scene save_test` in `main.cpp:3647` (3 sub-tests: slot_roundtrip, migration, autosave); `build-windows-save-test` job in `build-windows.yml:1039`; `tests/save_fixtures/` (migration fixtures) |
| M24 🔨 | 40 PNGs in `Content/Textures/`; 32 WAVs in `Content/Audio/`; 19 animation JSONs in `Content/Animations/`; `Cooked/` contains only `.gitkeep` |
| Cook format gap | `cook_assets.py:235` comment `# STUB: just copy; real cook would compress` |
| Physics Tool ⬜ | `tools/creation_engine.py` has no `bake-collision` command |
| UI Tool ⬜ | `tools/creation_engine.py` has no `bake-font` command |
| Vehicles Tool ⬜ | `tools/creation_engine.py` has no `bake-road` command |
| Cinematic audio ⬜ | `docs/FF15_REQUIREMENTS_BLUEPRINT.md:176` "Timed audio event fires within ±1 frame ⬜" |
| Authored content CI gap | No `--scene authored_content` hard-failure test in `main.cpp` or `build-windows.yml` |
