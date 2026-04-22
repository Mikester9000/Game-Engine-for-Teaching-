# Monorepo State Assessment — 2026-04-22

This document captures the deep repository assessment requested on 2026-04-22 and is the reference snapshot used to reconcile planning and status docs.

## Executive Summary

- D3D11 is the active and feature-rich renderer path; Vulkan parity remains intentionally deferred.
- Milestones M16–M22 are implemented and CI-covered on the D3D11 path.
- The largest remaining gap to an early-beta FFXV-style slice is **runtime ingestion of authored content** (mesh/material loading + populated sample assets), not core rendering algorithms.
- The sample project has strong schema/config coverage but sparse real content in `Content/Textures`, `Content/Audio`, and `Content/Animations`.

## Confirmed Completed Areas

- Rendering: D3D11 depth buffer, IBL (`pbr_ibl`), directional shadow maps (`shadow_test`), bloom (`bloom_test`), dynamic sky (`dynamic_sky`), GPU skinning (`skinned_mesh`), SDF text (`font_test`).
- Gameplay/tooling milestones: combo/combat (`combat_test`), quest/dialogue tooling + tests (`quest_test`, `dialogue_test`), cinematic baker + editor panel (M22), navmesh/tod baker stubs (M21), X3DAudio positional audio (`audio_3d_test`).
- CI: Windows headless scene matrix, Linux python tool tests, contract tests, architecture lint.

## Critical Remaining Gaps (Prioritized)

1. **M23 (new): Authored content ingestion on D3D11**
   - Runtime mesh/material loading from cooked assets into draw path.
   - Bind authored texture sets (albedo/normal/metallic-roughness/AO) where available.

2. **M24 (new): Vertical slice content population**
   - Populate texture/audio/animation sample content in `samples/vertical_slice_project/Content/`.
   - Validate full author→cook→load path with non-placeholder assets.

3. **M25 (new): Terrain/world geometry path**
   - Add terrain/world geometry rendering + collision support for streamed cells.

4. **M26 (new): Save-system CI hardening**
   - Add dedicated `--scene save_test` acceptance coverage for round-trip, migration, and auto-save.

5. **M14: Vulkan catch-up (still deferred)**
   - Resume only when D3D11 early-beta slice goals above are met.

## Evidence Anchors (representative paths)

- Renderer/runtime: `src/engine/rendering/d3d11/D3D11Renderer.cpp`, `src/sandbox/main.cpp`
- Tooling: `tools/creation_engine.py`, `tools/quest_baker/`, `editor/src/panels/CinematicEditorPanel.cpp`
- Sample project: `samples/vertical_slice_project/Content/`, `samples/vertical_slice_project/AssetRegistry.json`
- CI: `.github/workflows/build-windows.yml`, `.github/workflows/build-linux.yml`, `.github/workflows/contract-tests.yml`

## Usage

When status conflicts are found across docs, reconcile them against this assessment and the current source/CI truth, then update:

- `.github/copilot-instructions.md`
- `docs/COPILOT_CONTINUATION.md`
- `docs/ROADMAP.md`
- `docs/PROJECT_MILESTONES.md`
- `docs/FF15_REQUIREMENTS_BLUEPRINT.md`

