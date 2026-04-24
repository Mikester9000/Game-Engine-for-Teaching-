# Issue #83 v2 — Demo_Game Vertical Slice Plan (Living Milestones Doc)
Last updated: 2026-04-24

This document replaces ad-hoc "what's left?" checks with a single source of truth.
Each milestone below is intended to be completed in **one pull request**.

## How to use this doc with Copilot
When starting a new Copilot session, paste:
1) The milestone ID (e.g. **M-DG-S2**)
2) The "Copilot prompt" block from that milestone
3) Any constraints you care about (time limit, minimal diffs, etc.)

**Rule:** 1 PR = 1 milestone. Do not mix milestones.

---

## Current status (repo reality snapshot)
### Demo_Game scaffolding
✅ M-DG-BASE: Demo_Game executable + OpenWorld FSM + CI integration (PR #84)  
✅ M-DG-UI: Boot menu + F1 station overlay + station JSON loading (PR #85)  
✅ M-DG-TEACH: Interact-only teaching tour + lesson panels (PR #87)  

### Engine foundations used by Issue #83 v2
✅ EngineConfig (`src/engine/core/engine_config.*`)  
✅ SaveSystem (`src/engine/save/*`) + headless save_test CI (M26 / PRs #74/#75)

---

# Milestones (PR-sized)

## M-DG-P1 — Engine-wide performance preset system (core toggles)
**Goal:** Add an engine-wide preset system (feature toggles, not resolution-driven).
**Must remain engine-wide:** affects `engine_sandbox` and `demo_game`.

### Scope
- Add a single entry point: `ApplyPerformancePreset(preset, overrides)`
- First-pass toggles:
  - Shadows: OFF / ON (or LOW/HIGH)
  - Bloom/PostFX: OFF / ON
  - VSync/Frame cap: OFF / 60 / 30 (implementation-dependent)

### Non-goals
- No major renderer refactors
- No new external deps
- Do not implement resolution scaling in this milestone

### Files likely involved
- `src/engine/core/engine_config.hpp/.cpp` (extend config model)
- Renderer feature flags (where shadows/bloom/vsync are configured)

### Acceptance criteria
- Preset can be switched and visibly changes output in `engine_sandbox`
- `demo_game` builds and runs unchanged
- Headless modes do not crash (safe no-ops ok)

### Copilot prompt
> Implement M-DG-P1 from docs/ISSUE_83_V2_PLAN.md.
> Add engine-wide performance presets (core toggles only: shadows, bloom, vsync/frame cap).
> Persist via EngineConfig JSON when ENGINE_ENABLE_JSON is enabled; safe defaults otherwise.
> Keep PR scope minimal and avoid renderer refactors. Add a small headless sanity check if feasible.

---

## M-DG-P2 — Add IBL quality toggle to presets
**Goal:** Make IBL a preset-controlled feature without breaking readability.

### Scope
- IBL OFF / LOW / HIGH (or OFF/ON depending on existing implementation)
- If IBL is OFF, ensure a fallback ambient term so materials don't go black.

### Acceptance criteria
- Visible difference in `engine_sandbox`
- Low preset still produces readable materials (FF7/FF8 clarity floor)

### Copilot prompt
> Implement M-DG-P2 from docs/ISSUE_83_V2_PLAN.md.
> Add IBL quality toggles integrated into the existing preset system.
> Ensure IBL-off path remains readable (no black materials).

---

## M-DG-P3 — Texture filtering / anisotropic tiers via presets
**Goal:** Control sampler quality via presets.

### Scope
- Aniso tiers (1/2/4/8/16) or nearest equivalent
- Clamp to supported hardware/feature level

### Acceptance criteria
- Visual change is observable on ground/walls (especially at oblique angles)
- No crashes; safe clamping

### Copilot prompt
> Implement M-DG-P3 from docs/ISSUE_83_V2_PLAN.md.
> Add texture filtering/aniso tiers controlled by performance presets and persisted in EngineConfig.

---

## M-DG-P4 — Optional resolution scale (defer if risky)
**Goal:** Add resolution scaling only if already architecturally safe.

### Scope
- Implement only if renderer already supports internal render target scaling cleanly.
- If not safe: document why and add a TODO plan, then close milestone as deferred.

### Acceptance criteria
- If implemented: scale applies without breaking HUD/UI
- If deferred: doc note added with minimal recommended architecture

### Copilot prompt
> Implement M-DG-P4 from docs/ISSUE_83_V2_PLAN.md.
> Attempt resolution scale only if low-risk. Otherwise document deferral with clear technical reasons and next steps.

---

## M-DG-S1 — Demo_Game settings menu uses engine-wide presets
**Goal:** Settings menu is not stubbed; users can pick preset and volume.

### Scope
- Boot menu Settings: choose preset (Low/Med/High/Classic) + master volume
- Persist to EngineConfig when JSON enabled

### Acceptance criteria
- Persists across restart
- JSON-off build doesn't crash; shows message and uses defaults

### Copilot prompt
> Implement M-DG-S1 from docs/ISSUE_83_V2_PLAN.md.
> Replace stubbed Demo_Game Settings with a working menu that selects performance preset and master volume.
> Persist via EngineConfig (JSON-gated) and keep UI simple and stable.

---

## M-DG-S2 — Demo_Game save/load plumbing (no UI yet)
**Goal:** Wire Demo_Game state into SaveSystem with headless determinism.

### Scope (minimum)
- Save/load: player position (or station/biome + coords) and quest/activity progress
- Add a deterministic headless test for Demo_Game save/load roundtrip (or integrate into existing headless demo tests)

### Acceptance criteria
- Save → mutate → load restores expected state
- JSON-off: safe skip or clear message, no crash

### Copilot prompt
> Implement M-DG-S2 from docs/ISSUE_83_V2_PLAN.md.
> Integrate engine SaveSystem with Demo_Game for minimal state roundtrip and add a deterministic headless test.
> Keep scope tight; do not refactor core engine save format.

---

## M-DG-S3 — Boot menu Continue loads autosave/slot 0
**Goal:** Continue is functional.

### Scope
- Continue loads autosave if present, else slot 0 if present, else "no save"
- No new UI framework; keep consistent with existing boot menu overlay

### Acceptance criteria
- Relaunch and Continue resumes correctly

### Copilot prompt
> Implement M-DG-S3 from docs/ISSUE_83_V2_PLAN.md.
> Make Demo_Game Continue work by loading autosave then slot 0 fallback, with clear user feedback.

---

## M-DG-S4 — Pause menu Save/Load UI
**Goal:** Save/load is discoverable in play.

### Scope
- Pause overlay: Save/Load/Delete for a small slot set
- Show slot metadata if available

### Acceptance criteria
- Works without crashes and respects missing slots

### Copilot prompt
> Implement M-DG-S4 from docs/ISSUE_83_V2_PLAN.md.
> Add in-game pause menu save/load UI using SaveSystem slots and metadata.

---

## Visual floor (FF7/FF8 minimum)
**Definition:** Even with expensive features off, the game should not look broken:
- textures are present and readable
- lighting is stable (no full-black materials)
- no "debug grey" look in primary demo areas

Milestones M-DG-P2 and M-DG-VF1 are the main enforcement points.

## M-DG-VF1 — Visual/readability guardrails on Low preset
**Goal:** Ensure Low preset doesn't destroy readability.

### Scope
- Add fallback ambient term or clamps where needed
- Improve defaults that cause black/overbright

### Acceptance criteria
- Low preset yields readable output in key demo areas

### Copilot prompt
> Implement M-DG-VF1 from docs/ISSUE_83_V2_PLAN.md.
> Add guardrails so Low preset remains readable (FF7/FF8 clarity floor), without adding heavy new effects.

---

## Changelog
- 2026-04-24: Initial Issue #83 v2 plan.

---

Notes:
- Keep this document updated as milestones are completed.
- Prefer adding references to PR numbers in the "Current status" section.
