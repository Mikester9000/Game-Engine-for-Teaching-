# tests/save_fixtures/

This directory holds **older-version save file fixtures** used by the M26
save-system CI hardening tests (`--headless --scene save_test`).

The C++ acceptance tests (in `src/sandbox/main.cpp`, the `save_test` scene
block) copy one of these files into a temporary directory and call
`SaveSystem::Load()` to verify that the migration path handles the version
mismatch gracefully — no crash, a `LOG_WARN` migration message, and the
correct player data restored.

---

## Fixture: `v0_9_0_player.save.json`

**Represents:** a save file produced by game engine version **0.9.0** (before
the 1.0.0 schema stabilisation).

### What changed between 0.9.0 → 1.0.0

| Field / Component | v0.9.0 | v1.0.0 |
|---|---|---|
| Root `version` | `"0.9.0"` | `"1.0.0"` |
| Root `playerName` | **absent** | Present (quick-load UI) |
| Root `playerLevel` | **absent** | Present (quick-load UI) |
| `Transform` rotation (`rx/ry/rz`) | **absent** | Present (defaults `0` on load) |
| `Transform` scale (`sx/sy/sz`) | **absent** | Present (defaults `1` on load) |

### Migration behaviour

`SaveSystem::Load()` detects `"version": "0.9.0"` ≠ `kSaveFormatVersion`
(`"1.0.0"`) and logs:

```
[WARN] SaveSystem::Load: version mismatch (file=0.9.0 current=1.0.0)
       — attempting forward migration.
```

The load then proceeds: `nlohmann::json::value()` with default arguments
fills in every field that is absent from the older payload, so the loaded
`TransformComponent` has `rotation = {0,0,0}` and `scale = {1,1,1}` exactly
as if the player had never rotated or resized their character.

### Fixture content summary

| Field | Value |
|---|---|
| Player name | `"Noctis"` |
| HP / maxHP | 340 / 500 |
| MP / maxMP | 90 / 150 |
| Level | 7 |
| Position | (128, 0, 256) |
| Gil | 38 000 |
| Quest 1001 | Complete (objective 0, progress 1/1) |
| Quest 1002 | In-progress (objective 0, progress 2/3) |

---

## Adding new fixtures

1. Name the file `v<MAJOR>_<MINOR>_<PATCH>_<description>.save.json`.
2. Set `"version"` to the **old** version string the fixture represents.
3. Add a row to the table above explaining what changed.
4. Add a test case to `test_save_fixtures.py` for the new fixture.
5. Reference the fixture in the `save_test` block inside
   `src/sandbox/main.cpp` under the `migration` subtest comment.
