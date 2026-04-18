# tests/golden/ — Golden-File Reference Outputs

## TEACHING NOTE — What are Golden Files?

A **golden file** (also called a *reference file* or *snapshot*) is the
expected output of a deterministic process.  The test workflow is:

1. Run the process (e.g., `cook.exe --project samples/vertical_slice_project`).
2. Compare its actual output against the golden file stored in this directory.
3. If they differ, the test fails and CI blocks the PR.

Golden-file tests are used extensively in AAA game studios for:
- Asset pipeline validation (cook output format)
- Shader compilation output verification
- Save-file schema regression tests
- Localisation string database diffs

## Files in This Directory

| File | Produced by | What it tests |
|------|-------------|---------------|
| `assetdb_expected.json` | `cook.exe` | M2 asset cooker output format |

## Updating Golden Files

When the cook output format changes intentionally (e.g., a new field is
added to assetdb.json), update the golden file to match the new output:

```bash
# Re-cook the vertical slice project
./build/cook --project samples/vertical_slice_project

# Copy the new output to the golden file
cp samples/vertical_slice_project/Cooked/assetdb.json tests/golden/assetdb_expected.json

# Commit both the updated source and the updated golden file
git add tests/golden/assetdb_expected.json
git commit -m "update golden file: assetdb format adds version field"
```

## Contract Tests Workflow

The `.github/workflows/contract-tests.yml` CI workflow:

1. Builds `cook` on Ubuntu (Linux cross-platform validation).
2. Runs `cook --project samples/vertical_slice_project`.
3. Compares `Cooked/assetdb.json` against `tests/golden/assetdb_expected.json`.
4. Fails the CI step if any difference is detected.

This ensures the cooker output format remains stable across all PRs.
