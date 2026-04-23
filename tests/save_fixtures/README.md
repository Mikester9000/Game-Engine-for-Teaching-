# Save-System Test Fixtures

This directory holds golden-file fixtures for the `save_test` headless
acceptance scene (`--headless --scene save_test`, M26).

## Files

| File | Version | Purpose |
|------|---------|---------|
| `v0_9_0_minimal.json` | 0.9.0 | Migration test fixture — minimal entity with Health+Name; loaded by Test 2 (migration) to assert the forward-migration path or graceful failure. |

## How fixtures are used

The `save_test` scene in `src/sandbox/main.cpp` (Test 2) writes this exact
payload inline via a raw C++ string literal (`kFixtureJSON`) using
`std::ofstream`.  The file here is the **canonical golden copy** — both the
inline string and this file must stay in sync.

If a future migration step changes the expected output from loading a
`"0.9.0"` save, update:
1. The `kFixtureJSON` constant in the `save_test` block (Test 2) in
   `src/sandbox/main.cpp`.
2. This golden file.

## Adding a new fixture

1. Create `vX_Y_Z_<description>.json` in this directory.
2. Add a corresponding entry to the table above.
3. If a new acceptance sub-test uses this fixture, reference it in the
   `TEACHING NOTE` block of the relevant test in `src/sandbox/main.cpp`.
