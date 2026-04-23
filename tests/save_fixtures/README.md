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
3. A manual diff is the verification step.  Run the following before
   merging any fixture change:

   ```bash
   # Extract the JSON payload from the inline C++ raw string literal,
   # then diff against the golden file.  A zero-exit (no output) = in sync.
   python3 - << 'EOF' | diff - tests/save_fixtures/v0_9_0_minimal.json
   import re
   src = open('src/sandbox/main.cpp').read()
   m = re.search(r'R"JSON\((.*?)\)JSON"', src, re.DOTALL)
   if m:
       print(m.group(1), end='')
   EOF
   ```

   Note: the plain `grep -A N 'kFixtureJSON'` approach does NOT work because
   it includes the surrounding C++ syntax (`static const char ...` and
   `R"JSON(` / `)JSON";`), making `diff` always report differences.

## Adding a new fixture

1. Create `vX_Y_Z_<description>.json` in this directory.
2. Add a corresponding entry to the table above.
3. If a new acceptance sub-test uses this fixture, reference it in the
   `TEACHING NOTE` block of the relevant test in `src/sandbox/main.cpp`.
