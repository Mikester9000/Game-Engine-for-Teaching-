# tests/save_fixtures/

This directory holds **older-version save file fixtures** used by the M26
save-system CI hardening tests.

Fixtures serve two roles:

1. **Python pytest** (`test_save_fixtures.py`) — pure-Python structural
   validation that runs on Linux CI without a C++ build.
2. **C++ headless acceptance scene** (`--headless --scene save_test`) — the
   `save_test` scene in `src/sandbox/main.cpp` loads or embeds these fixtures
   to verify that `SaveSystem::Load()` handles version mismatches gracefully.

---

## Files

| File | Version | Used by | Purpose |
|------|---------|---------|---------|
| `v0_9_0_player.save.json` | 0.9.0 | Python pytest | Full player state (Health, Transform, Quest, Currency); validates migration contract and field structure. |
| `v0_9_0_minimal.json` | 0.9.0 | C++ save_test (Test 2) | Minimal entity with Health+Name; canonical golden reference for the inline `kFixtureJSON` raw-string literal in `main.cpp`. |

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

## Fixture: `v0_9_0_minimal.json`

**Represents:** the minimum valid save payload that `SaveSystem::Load()`
accepts from a 0.9.0 file — one entity with `Health` + `Name` components.

### Keeping C++ inline string in sync

The `save_test` scene (Test 2) embeds this fixture as a C++ raw string
literal (`kFixtureJSON`) via `std::ofstream`.  This file is the **canonical
golden reference** — both must stay in sync manually.

If you update the JSON here, also update `kFixtureJSON` in
`src/sandbox/main.cpp`, and vice versa.  Verify with:

```bash
python3 - << 'EOF' | diff - tests/save_fixtures/v0_9_0_minimal.json
import re
src = open('src/sandbox/main.cpp').read()
m = re.search(r'R"JSON\((.*?)\)JSON"', src, re.DOTALL)
if m:
    print(m.group(1), end='')
EOF
```

A zero-exit (no output) means they are in sync.

---

## Adding new fixtures

1. Name the file `v<MAJOR>_<MINOR>_<PATCH>_<description>.save.json`.
2. Set `"version"` to the **old** version string the fixture represents.
3. Add a row to the Files table above explaining what changed.
4. Add a test case to `test_save_fixtures.py` for the new fixture.
5. If the fixture is also used by a C++ acceptance sub-test, reference it in
   the `TEACHING NOTE` block of the relevant test in `src/sandbox/main.cpp`.
