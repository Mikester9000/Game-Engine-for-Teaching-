# Save-System Test Fixtures

This directory holds golden-file fixtures for the `save_test` headless
acceptance scene (`--headless --scene save_test`, M26).

## Files

| File | Version | Purpose |
|------|---------|---------|
| `v0_9_0_minimal.json` | 0.9.0 | Migration test fixture — minimal entity with Health+Name; loaded by Test 2 (migration) to assert the forward-migration path or graceful failure. |

## How fixtures are used

The `save_test` scene in `src/sandbox/main.cpp` (Test 2) writes an inline
copy of the `v0_9_0_minimal.json` payload via a raw C++ string literal
(`kFixtureJSON`) using `std::ofstream`.  The file here is the **canonical
golden reference** — both the inline string and this file must stay in sync.

### Keeping them in sync (manual process)

There is currently no automated check between the inline string and this
golden file; sync is enforced by convention:

1. If you update the `v0_9_0_minimal.json` fixture, also update the
   `kFixtureJSON` raw string literal inside the `save_test` block
   (Test 2) in `src/sandbox/main.cpp`.
2. If you update the `kFixtureJSON` inline string, also update this file.
3. A quick `diff` between the golden file and the inline string is the
   manual verification step.  Run it before merging any fixture change:

   ```bash
   # Compare inline constant (extracted from main.cpp) against the golden file.
   grep -A 20 'kFixtureJSON\[\]' src/sandbox/main.cpp | diff - tests/save_fixtures/v0_9_0_minimal.json
   ```

   A zero-exit diff confirms they are identical.

## Adding a new fixture

1. Create `vX_Y_Z_<description>.json` in this directory.
2. Add a corresponding entry to the table above.
3. If a new acceptance sub-test uses this fixture, reference it in the
   `TEACHING NOTE` block of the relevant test in `src/sandbox/main.cpp`.
