# Implementation Plan — Build, Dependency & Polish

> **Source assessment:** `docs/ASSESSMENT_2026-04-23_BUILD_POLISH.md`
>
> **Goal:** Bring the engine to a polished, demo-ready / early-beta state by addressing
> the infrastructure, tooling, documentation, and quality gaps identified in the
> 2026-04-23 build-polish assessment.
>
> **Ordering principle:** Items that unblock other work or fix the widest class of
> CI fragility come first.  Each item is scoped to be implementable as a single PR.
>
> **Companion docs:** `docs/ROADMAP.md` (feature milestones), `docs/FF15_REQUIREMENTS_BLUEPRINT.md`
> (subsystem completion matrix), `docs/ASSESSMENT_2026-04-23.md` (content/cook gaps).

---

## Phase 1 — Build Reproducibility (unblocks everything else)

### BP-1: Pin vcpkg baseline in `vcpkg.json`

**Problem:** `vcpkg.json` has no `"builtin-baseline"` field.  Package versions vary
across developer machines and CI runner image updates.  The editor CI job works around
this by pinning the vcpkg *repository tag* (`2024.12.16`) — but the main engine job
uses the system `C:\vcpkg` with no version lock.

**Implementation:**
1. Find the commit SHA of the vcpkg `2024.12.16` tag:
   ```
   git ls-remote https://github.com/microsoft/vcpkg refs/tags/2024.12.16
   ```
2. Add to `vcpkg.json`:
   ```jsonc
   {
     "builtin-baseline": "<sha-from-step-1>",
     ...existing fields...
   }
   ```
3. Verify: run `cmake --preset windows-debug-engine-only` locally with the system vcpkg
   toolchain.  CMake should resolve packages to the same versions as CI.
4. Update `build-windows.yml` physics and save-test jobs to pass
   `-DVCPKG_MANIFEST_INSTALL=OFF` only when intentionally skipping manifest mode.

**Acceptance:** `cmake --preset windows-debug-editor -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/...`
resolves the same package versions on a fresh machine and on CI with no extra flags.

**Files:** `vcpkg.json`, `.github/workflows/build-windows.yml` (comments)

---

### BP-2: Add `scripts/bootstrap.ps1` (one-command developer setup)

**Problem:** There is no single script a new contributor can run to get from a fresh
clone to a working build.

**Implementation (`scripts/bootstrap.ps1`):**
```powershell
<#
.SYNOPSIS  One-command developer bootstrap for the Game Engine for Teaching.
.DESCRIPTION
    Requires: Visual Studio 2022 (Desktop C++ workload), Python 3.9+.
    Automates: vcpkg, Python deps, CMake configure+build, asset cook, smoke test.
#>

param(
    [string]$VcpkgRoot = "C:\vcpkg",
    [string]$Preset    = "windows-ninja-debug-engine-only"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# 1. Bootstrap vcpkg if not present
if (-not (Test-Path "$VcpkgRoot\vcpkg.exe")) {
    Write-Host "[bootstrap] Cloning vcpkg to $VcpkgRoot..."
    git clone https://github.com/microsoft/vcpkg.git $VcpkgRoot --depth 1
    & "$VcpkgRoot\bootstrap-vcpkg.bat" -disableMetrics
}

# 2. Python dependencies
Write-Host "[bootstrap] Installing Python dependencies..."
python -m pip install -r requirements-dev.txt

# 3. CMake configure + build (engine-only — no vcpkg deps needed for D3D11 baseline)
Write-Host "[bootstrap] Configuring CMake ($Preset)..."
cmake --preset $Preset

Write-Host "[bootstrap] Building engine_sandbox, cook, pak..."
cmake --build --preset $Preset --target engine_sandbox cook pak

# 4. Cook vertical slice assets
Write-Host "[bootstrap] Cooking vertical slice assets..."
python samples\vertical_slice_project\cook_assets.py

# 5. Smoke test
$Exe = "build\$Preset\engine_sandbox.exe"
Write-Host "[bootstrap] Running headless smoke test..."
& $Exe --headless
& $Exe --headless --scene authored_content -DENGINE_PROJECT_ROOT=samples\vertical_slice_project

Write-Host "[bootstrap] Done.  Run: $Exe --headless --scene dynamic_sky"
```

**Acceptance:** On a fresh Windows machine with VS 2022 + Python 3.9,
`.\scripts\bootstrap.ps1` exits 0 and produces a working `engine_sandbox.exe`.

**Files:** `scripts/bootstrap.ps1` (new), `README.md` (add "Quick Start" section)

---

### BP-3: Offline HLSL shader validation via FXC at build time

**Problem:** HLSL shaders are compiled at runtime via `D3DCompileFromFile`.
A shader syntax error is not caught until `--scene <name>` is first called, not
during `cmake --build`.

**Implementation:**
1. In `CMakeLists.txt`, add a `find_program(FXC_EXECUTABLE fxc ...)` call inside the
   `if(ENGINE_ENABLE_D3D11)` block, searching `$(WindowsSdkDir)bin/*/x64/`.
2. If FXC is found, add `add_custom_command(OUTPUT <name>.cso COMMAND fxc ...)` for
   every HLSL shader in `HLSL_SHADERS`.
3. Create an `engine_shaders_d3d11` custom target depending on all `.cso` outputs.
4. Add `add_dependencies(engine_sandbox engine_shaders_d3d11)`.
5. The `.cso` outputs are validation artifacts only (runtime still uses
   `D3DCompileFromFile`) — or optionally change `D3D11Renderer::LoadScene` to load
   the pre-compiled `.cso` when present and fall back to source compilation.
6. Add FXC invocation to CI: `cmake --build --preset windows-ninja-debug-engine-only
   --target engine_shaders_d3d11` before the headless tests.

**Acceptance:** Introducing a deliberate syntax error in any `.hlsl` file causes
`cmake --build` to fail with a clear FXC error message.

**Files:** `CMakeLists.txt`, `.github/workflows/build-windows.yml`

---

### BP-4: Add Release build job to CI

**Problem:** No `windows-release-*` preset runs in CI.  Optimisation-exposed
undefined behaviour and stripped asserts would go undetected.

**Implementation:**
1. Add a `build-windows-release` job in `build-windows.yml` using the existing
   `windows-ninja-release-engine-only` preset (already defined in `CMakePresets.json:74`
   if present, else add it).
2. Build only `engine_sandbox` and `cook`.
3. Run the full headless scene matrix (same steps as the debug job).
4. Set `continue-on-error: false`.

**Acceptance:** CI has a green Release build job on every PR.

**Files:** `CMakePresets.json` (add release preset if missing), `.github/workflows/build-windows.yml`

---

## Phase 2 — Asset Pipeline (texture quality for beta)

### AP-1: BC7 texture compression via `texconv.exe`

**Problem:** `cook_assets.py` produces uncompressed RGBA8 DDS (4 bytes/pixel).
GPU-native BC7 block compression is 0.5–1 bytes/pixel — 4–8× less VRAM.
`directxtex` is already declared in `vcpkg.json` and ships `texconv.exe`.

**Implementation:**
1. In `cook_assets.py`, add a helper `_find_texconv()` that searches:
   - vcpkg install tree (`$VCPKG_ROOT/installed/x64-windows/tools/directxtex/texconv.exe`)
   - Common local paths
   - Falls back to `None` → use existing `_png_to_dds_rgba8()` Pillow path
2. When `texconv.exe` is found, replace the `_png_to_dds_rgba8()` call with:
   ```python
   subprocess.run([texconv, "-f", "BC7_UNORM", "-y", "-o", cooked_dir, src_png], check=True)
   ```
3. Update the `should_recook()` `require_dds=True` logic to also detect stale RGBA8
   files when BC7 is now available.
4. Update `d3d11_texture.cpp` to confirm BC7 (`DXGI_FORMAT_BC7_UNORM`) is handled
   by the DX10-extended header path (already supported at line ~200).
5. Update CI: install `directxtex` via vcpkg before the `cook_assets.py` step, or
   use the `texconv` binary from a published GitHub release.

**Acceptance:** Cooked `.tex` files for PNG sources are BC7 block-compressed when
`texconv.exe` is available; RGBA8 fallback still works when it is not.

**Files:** `samples/vertical_slice_project/cook_assets.py`, `.github/workflows/build-windows.yml`

---

### AP-2: Mip-map generation

**Problem:** `dwMipMapCount=1` always in the cooked DDS.  Without mips, distant
terrain and textures alias and GPU bandwidth is wasted on full-resolution samples.

**Implementation:**
1. When using `texconv.exe` (AP-1), omit the `-m 1` flag — texconv generates a full
   mip chain by default.
2. Update `d3d11_texture.cpp` to read `dwMipMapCount` correctly: the current code
   sets `mipCount=1` unless `DDSD_MIPMAPCOUNT` is set in `dwFlags`.  With texconv
   output, `DDSD_MIPMAPCOUNT` will be set and `dwMipMapCount` will reflect the full
   chain — confirm the existing logic handles this path (lines 314–343).
3. Update `D3D11_TEXTURE2D_DESC.MipLevels` from `1` to the actual mip count returned
   by the loader.

**Acceptance:** A 512×512 texture cooked via texconv produces a DDS with 10 mip
levels; `d3d11_texture.cpp` creates a `D3D11_TEXTURE2D_DESC` with `MipLevels=10`.

**Files:** `samples/vertical_slice_project/cook_assets.py`, `src/engine/rendering/d3d11/d3d11_texture.cpp`

---

### AP-3: CMake `install()` target for distributable packaging

**Problem:** No documented procedure exists for building a distributable release
(what goes in the package, how to configure release vs debug).

**Implementation (`CMakeLists.txt`):**
```cmake
# TEACHING NOTE — CMake install() target
# Running `cmake --install build/windows-release-engine-only --prefix dist/`
# produces a self-contained directory ready for redistribution.
install(TARGETS engine_sandbox cook pak
    RUNTIME DESTINATION bin
)
install(DIRECTORY shaders/ DESTINATION bin/shaders)
install(DIRECTORY samples/vertical_slice_project/Cooked/
    DESTINATION bin/Cooked
)
install(FILES samples/vertical_slice_project/AssetRegistry.json
    DESTINATION bin
)
# Lua DLL (if bundled)
if(EXISTS "${CMAKE_SOURCE_DIR}/Lua/lua55.dll")
    install(FILES Lua/lua55.dll DESTINATION bin)
endif()
```

**Acceptance:** `cmake --install build/windows-ninja-release-engine-only --prefix dist/`
produces a `dist/bin/engine_sandbox.exe` that runs standalone without the build tree.

**Files:** `CMakeLists.txt`, `docs/ASSET_PIPELINE.md` (packaging section)

---

### AP-4: Git SHA version stamping

**Problem:** No `--version` flag; git SHA not embedded in binary.  Post-crash
reproduction requires guessing which build a `.dmp` came from.

**Implementation:**
1. In `CMakeLists.txt`, capture the git SHA at configure time:
   ```cmake
   find_package(Git QUIET)
   if(Git_FOUND)
       execute_process(
           COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
           OUTPUT_VARIABLE ENGINE_GIT_SHA
           OUTPUT_STRIP_TRAILING_WHITESPACE
       )
   else()
       set(ENGINE_GIT_SHA "unknown")
   endif()
   target_compile_definitions(engine_sandbox PRIVATE
       ENGINE_VERSION_SHA="${ENGINE_GIT_SHA}"
   )
   ```
2. In `src/sandbox/main.cpp`, handle `--version` arg:
   ```cpp
   if (argc >= 2 && std::string(argv[1]) == "--version") {
       std::cout << "engine_sandbox " ENGINE_VERSION_SHA "\n";
       return 0;
   }
   ```
3. Also print the SHA in the first `LOG_INFO` line of `main()`.

**Acceptance:** `engine_sandbox.exe --version` prints a short git SHA.

**Files:** `CMakeLists.txt`, `src/sandbox/main.cpp`

---

## Phase 3 — Logging, Crash Reporting & Config

### LC-1: File-based logging

**Problem:** `Logger` writes only to `stdout`/`stderr`.  Crash logs vanish when the
console window closes.  The `samples/vertical_slice_project/Saved/` directory exists
but is unused by the engine.

**Implementation (`src/engine/core/Logger.cpp`):**
1. In `Logger::Init()`, open a log file at `{saveDir}/Logs/engine_YYYYMMDD_HHMMSS.log`
   (where `saveDir` is `samples/vertical_slice_project/Saved/` by default, overridable
   by the env var `ENGINE_SAVE_DIR`).
2. All `LOG_INFO/WARN/ERROR/DEBUG` calls append to the file in addition to stdout.
3. `Logger::Shutdown()` flushes and closes the file.
4. Add `std::source_location` (C++20) or `__FILE__`/`__LINE__` macro variants for
   `LOG_ERROR_LOC` to include file + line in error entries.

**Acceptance:** After a headless run, a timestamped log file exists under `Saved/Logs/`.

**Files:** `src/engine/core/Logger.hpp`, `src/engine/core/Logger.cpp`

---

### LC-2: Minidump writer (`SetUnhandledExceptionFilter`)

**Problem:** Crashes produce no artefact.  Post-crash reproduction is impossible
without a repro case.

**Implementation (`src/sandbox/main.cpp`):**
```cpp
#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* ep) {
    HANDLE f = CreateFileW(L"Saved/Crashes/crash.dmp",
        GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei{};
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
            f, MiniDumpNormal, &mei, nullptr, nullptr);
        CloseHandle(f);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

// In main(), before anything else:
SetUnhandledExceptionFilter(CrashHandler);
#endif
```

**Acceptance:** A deliberate null-dereference in a test build produces `Saved/Crashes/crash.dmp`.

**Files:** `src/sandbox/main.cpp`

---

### LC-3: Engine configuration JSON

**Problem:** Resolution, keybindings, and audio volume are hardcoded.  Beta users
cannot change settings without recompiling.

**Implementation:**
1. Add `src/engine/core/engine_config.hpp/.cpp` — reads/writes `engine_config.json`
   from the working directory using `nlohmann/json` (already available when
   `ENGINE_ENABLE_JSON` is defined).
2. Schema: `{ "resolution": { "width": 1280, "height": 720 }, "audio": { "masterVolume": 1.0 }, "keys": { "attack": "Z", "dodge": "X" } }`
3. `main.cpp` loads config before creating the window; `Win32Window` uses the resolved
   resolution.
4. `InputMapper` loads key bindings from the config struct.
5. Ship a default `engine_config.json` alongside the executable (CMake `install()` step).

**Acceptance:** Changing `"width": 1920, "height": 1080` in `engine_config.json`
makes the window open at 1080p without recompiling.

**Files:** `src/engine/core/engine_config.hpp/.cpp` (new), `src/sandbox/main.cpp`,
`src/game/systems/input_mapper.cpp`, `CMakeLists.txt`

---

## Phase 4 — Performance & Profiling

### PF-1: GPU timing queries

**Problem:** No GPU frame-time data exists.  Performance regressions in rendering
passes are invisible.

**Implementation (`src/engine/rendering/d3d11/D3D11Renderer.cpp`):**
1. Add a `GpuTimer` helper class wrapping `D3D11_QUERY_TIMESTAMP_DISJOINT` +
   two `D3D11_QUERY_TIMESTAMP` queries (begin/end frame).
2. Call `GpuTimer::Begin()` at the start of `DrawFrame()` and `GpuTimer::End()`
   at the end.
3. On the *next* frame, `GpuTimer::Resolve()` reads the disjoint + timestamp results
   (must be one frame delayed) and stores `m_lastGpuFrameTimeMs`.
4. Expose via `D3D11Renderer::GetLastGpuFrameTimeMs()`.
5. Print in the `--headless --scene perf_test` output (new scene: render 100 frames,
   assert avg GPU time < 100 ms on WARP — generous but catches complete hangs).

**Acceptance:** `--headless --scene perf_test` exits 0 and prints per-frame GPU time.

**Files:** `src/engine/rendering/d3d11/D3D11Renderer.hpp/.cpp`, `src/sandbox/main.cpp`

---

### PF-2: PIX/RenderDoc event markers

**Problem:** No `PIXBeginEvent`/`PIXEndEvent` markers at render-pass boundaries.
PIX and RenderDoc captures have no labelled sections.

**Implementation:**
1. Add a `scoped_gpu_event.hpp` header (wraps `PIXBeginEvent`/`PIXEndEvent` when
   `ENGINE_ENABLE_PIX` is defined, otherwise no-ops).
2. Add `SCOPED_GPU_EVENT(ctx, "Shadow pass")` at the top of each render pass in
   `D3D11Renderer::DrawFrame()`.
3. Gate `PIXBeginEvent` behind `ENGINE_ENABLE_PIX` (requires the WinPixEventRuntime
   NuGet package — optional, not added to vcpkg.json for now).
4. Document RenderDoc frame capture setup in `docs/SHADER_PIPELINE.md`.

**Acceptance:** Attaching RenderDoc to `engine_sandbox.exe` shows labelled passes
("Shadow pass", "Lit pass", "Bloom bright-pass", etc.) in the event list.

**Files:** `src/engine/rendering/d3d11/scoped_gpu_event.hpp` (new),
`src/engine/rendering/d3d11/D3D11Renderer.cpp`, `docs/SHADER_PIPELINE.md`

---

## Phase 5 — CI Completeness

### CI-1: Add Windows `pytest` step to `build-windows.yml`

**Problem:** Python tool tests (`tools/tests/`) only run in the Linux `build-linux.yml`
job.  A Windows-only Python regression (e.g. path separator bug in `cook_assets.py`
on Windows) is undetected.

**Implementation (add to `build-windows.yml` after the cook step):**
```yaml
- name: Run Python tool tests on Windows
  run: python -m pytest tools/tests/ tests/ -q --tb=short
  shell: cmd
```

**Also add:** `pip install -r requirements-dev.txt` in the same job before this step.

**Acceptance:** `build-windows.yml` includes a `pytest` step that runs on every PR.

**Files:** `.github/workflows/build-windows.yml`

---

### CI-2: Confirm authored texture actually bound (not 1×1 fallback)

**Problem:** The `--scene authored_content` CI gate validates DDS magic bytes but
does not confirm that `D3D11Renderer` successfully bound an authored texture to a
shader slot (vs. silently using the 1×1 white fallback SRV).

**Implementation:**
1. Extend the `authored_content` scene in `src/sandbox/main.cpp` to:
   - Call `D3D11Renderer::LoadScene("pbr_ibl")` with `ENGINE_PROJECT_ROOT` set.
   - Check that at least one of the four authored texture slots (albedo/normal/MR/AO)
     was populated by a non-fallback SRV.
   - Report `[PASS] authored texture bound` vs `[FAIL] only fallback SRVs bound`.
2. Add a `GetAuthoredTextureBindCount()` method to `D3D11Renderer` that returns
   how many authored (non-fallback) SRVs were bound in the last `LoadScene()`.

**Acceptance:** `--scene authored_content` fails if all four texture slots fall back
to the 1×1 white SRV (i.e., the authored texture paths were not resolved).

**Files:** `src/sandbox/main.cpp`, `src/engine/rendering/d3d11/D3D11Renderer.hpp/.cpp`

---

### CI-3: PAK round-trip test (extract then `--validate-project`)

**Problem:** The CI packs a PAK archive but never validates that it can be
extracted and loaded at runtime.

**Implementation (add to `build-windows.yml` after the existing PAK step):**
```yaml
- name: Extract PAK archive and validate
  run: |
    .\build\windows-ninja-debug-engine-only\pak.exe ^
        --extract samples\vertical_slice_project\Cooked\test_output.pak ^
        --dest %TEMP%\pak_extracted
    .\build\windows-ninja-debug-engine-only\engine_sandbox.exe ^
        --validate-project %TEMP%\pak_extracted
  shell: cmd
```

**Acceptance:** The CI job validates that assets packed into the PAK can be
extracted and verified by `engine_sandbox --validate-project`.

**Files:** `.github/workflows/build-windows.yml`

---

## Phase 6 — Documentation

### DOC-1: `CONTRIBUTING.md` (new file at repo root)

**Contents:**
- Prerequisites table (VS 2022, Python 3.9+, Windows SDK)
- Quick build: `.\scripts\bootstrap.ps1` (links to BP-2)
- Manual build: `cmake --preset windows-ninja-debug-engine-only && cmake --build --preset ...`
- Running Python tests: `python -m pytest -q`
- Running C++ headless tests: `engine_sandbox.exe --headless --scene <name>`
- Coding conventions (excerpt/link from `docs/COPILOT_CONTINUATION.md`)
- PR checklist (CI must be green, TEACHING NOTE in new code, tests for new scenes)

**Files:** `CONTRIBUTING.md` (new)

---

### DOC-2: `docs/GETTING_STARTED.md` (new)

**Contents:**
- 5-minute tutorial: clone → bootstrap → run `dynamic_sky` scene interactively
- Diagram: Content/ → cook_assets.py → Cooked/ → engine_sandbox.exe
- What to try next: editing `highland.terrain.json`, adding a new HLSL shader

**Files:** `docs/GETTING_STARTED.md` (new)

---

### DOC-3: `docs/SHADER_PIPELINE.md` (new)

**Contents:**
- How to add a new HLSL shader (step-by-step: author → add to `HLSL_SHADERS` list in
  `CMakeLists.txt` → call `D3DCompileFromFile` in a new `LoadScene()` branch)
- SM 4.0 restrictions and why they are kept (GT610 / WARP CI compatibility)
- Shader debugging with RenderDoc (attach, capture, inspect)
- Shader debugging with PIX
- Future: offline FXC compilation (BP-3), DXC for SM 6.x

**Files:** `docs/SHADER_PIPELINE.md` (new)

---

### DOC-4: `docs/ASSET_PIPELINE.md` (new)

**Contents:**
- End-to-end cook guide: source conventions, running `cook_assets.py`, `cook.exe`
- DDS format rationale (why not PNG at runtime)
- BC7 compression setup (once AP-1 is implemented)
- PAK packaging: `pak.exe --input Cooked/ --output game.pak`
- Validating assets: `--scene authored_content`, `--validate-project`
- Adding a new asset type: schema → registry entry → cook step → runtime loader

**Files:** `docs/ASSET_PIPELINE.md` (new)

---

### DOC-5: Update `README.md`

**Changes:**
- Add "Prerequisites" table: VS 2022, Python 3.9+, Windows SDK 19041+
- Add "Quick Build" section using the CI-proven preset
- Link to `CONTRIBUTING.md` and `docs/GETTING_STARTED.md`
- Add `--version` flag example (once AP-4 is implemented)

**Files:** `README.md`

---

## Implementation Order & Dependencies

```
Phase 1 (BP-1) ──► Phase 2 (AP-1, AP-2) ──► Phase 3 (LC-1, LC-2, LC-3)
     │
     ├──► Phase 5 (CI-1, CI-2, CI-3)
     │
     └──► Phase 6 (DOC-1..5) — can start in parallel with any phase
```

| Item | Depends on | Estimated scope |
|---|---|---|
| BP-1 (vcpkg baseline) | — | Small (1-line vcpkg.json change + CI comment) |
| BP-2 (bootstrap.ps1) | BP-1 | Small (new PS1 file + README update) |
| BP-3 (FXC offline) | — | Medium (CMakeLists.txt + CI step) |
| BP-4 (Release CI) | BP-1 | Small (new CI job + optional preset) |
| AP-1 (BC7 texconv) | BP-1 | Medium (cook_assets.py + CI vcpkg install) |
| AP-2 (mip maps) | AP-1 | Small (d3d11_texture.cpp + texconv flag) |
| AP-3 (CMake install) | — | Small (CMakeLists.txt install() block) |
| AP-4 (git SHA) | — | Small (CMakeLists.txt + main.cpp) |
| LC-1 (file logging) | — | Medium (Logger.cpp + hpp changes) |
| LC-2 (minidump) | — | Small (main.cpp + dbghelp.lib) |
| LC-3 (config JSON) | ENGINE_ENABLE_JSON | Medium (new files + wiring) |
| PF-1 (GPU timing) | — | Medium (D3D11Renderer + new query API) |
| PF-2 (PIX markers) | — | Small (new header + event wrappers) |
| CI-1 (Windows pytest) | BP-2 | Small (build-windows.yml step) |
| CI-2 (authored bind check) | — | Small (main.cpp + renderer method) |
| CI-3 (PAK round-trip) | — | Small (build-windows.yml step) |
| DOC-1 (CONTRIBUTING.md) | BP-2 | Small |
| DOC-2 (GETTING_STARTED.md) | BP-2 | Small |
| DOC-3 (SHADER_PIPELINE.md) | BP-3 | Small |
| DOC-4 (ASSET_PIPELINE.md) | AP-1 | Small |
| DOC-5 (README update) | DOC-1, DOC-2 | Small |

---

## Acceptance Criteria Summary

When all items in this plan are complete:

- [ ] `.\scripts\bootstrap.ps1` exits 0 on a fresh Windows machine with VS 2022 + Python 3.9
- [ ] `cmake --preset windows-debug-editor -DCMAKE_TOOLCHAIN_FILE=...` resolves identical vcpkg packages on any machine
- [ ] A deliberate HLSL syntax error causes `cmake --build` to fail with a clear FXC error
- [ ] CI has a green Release build job on every PR
- [ ] Cooked `.tex` files are BC7 block-compressed (not RGBA8) when `texconv.exe` is available
- [ ] `d3d11_texture.cpp` creates textures with the correct mip count from texconv output
- [ ] `cmake --install` produces a self-contained `dist/bin/` directory
- [ ] `engine_sandbox.exe --version` prints a short git SHA
- [ ] After a headless run, a timestamped log exists in `Saved/Logs/`
- [ ] `Saved/Crashes/crash.dmp` is written on an unhandled exception
- [ ] Editing `engine_config.json` changes the window resolution without recompiling
- [ ] PIX/RenderDoc captures show labelled render passes
- [ ] `build-windows.yml` includes a `pytest` step that runs on every PR
- [ ] `--scene authored_content` fails if no authored texture is successfully bound
- [ ] `pak.exe --extract` + `--validate-project` round-trip passes in CI
- [ ] `CONTRIBUTING.md`, `docs/GETTING_STARTED.md`, `docs/SHADER_PIPELINE.md`, `docs/ASSET_PIPELINE.md` exist and are accurate
