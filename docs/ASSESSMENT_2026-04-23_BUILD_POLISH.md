# Build, Dependency & Polish Assessment — 2026-04-23

> **Scope:** A deep review of the Windows D3D11 build system, dependency management,
> shader toolchain, asset pipeline, bootstrap story, and documentation gaps.
> This assessment was produced on 2026-04-23 and builds on
> `docs/ASSESSMENT_2026-04-23.md` (content/cook gaps) by focusing specifically on
> **infrastructure and polish** rather than game-feature milestones.
>
> **Implementation plan:** `docs/PLAN_BUILD_POLISH.md`

---

## 1. Build & Dependency Setup

### 1a. CMake Layout, Presets, Generators & VS Integration

**Generator strategy:** All presets use **Ninja Multi-Config** (`CMakePresets.json:13`)
as the default. VS 2026 Insiders (`v180`) presets exist but are excluded from CI
(CI only has v143 / VS2022 toolset on `windows-latest`). The Ninja path is CI-proven.

**Visual Studio IDE usage:** There is no `CMakeSettings.json` or `.sln` in the root.
A developer opening the folder in VS 2022 gets auto-detected Ninja presets via VS's
built-in CMake support (VS ≥ 16.6). Switching between `engine-only` and `physics`
presets requires a re-configure; this is not documented anywhere for users.

**Preset proliferation (12 configure + 14 build):** The preset matrix covers every
combination of `{Debug,Release} × {engine-only,editor,physics,vulkan,save}`.
This is comprehensive but fragile at two points:

- The imgui feature name (`docking-experimental` in `vcpkg.json:12` vs `docking`
  in older vcpkg registries) caused a real CI breakage that is now worked around by
  pinning the editor job's vcpkg to tag `2024.12.16` (`build-windows.yml:869`).
- The physics CI job installs Jolt from `$env:TEMP` (classic-mode vcpkg) to avoid
  manifest-mode conflicts with imgui (`build-windows.yml:761–764`). This is fragile
  if the runner image's system `C:\vcpkg` moves.

**Gap:** No `windows-release-*` preset runs in CI. A Release regression (undefined
behaviour exposed by optimisations, stripped asserts) would go undetected.

---

### 1b. Third-Party Dependency Management

| Dependency | How managed | Version lock | Gap |
|---|---|---|---|
| Lua 5.5 | Vendored source `Lua/lua-5.5.0/src/` | Locked (source in-tree) | None — ideal |
| nlohmann/json | vcpkg manifest `vcpkg.json:8` | No baseline | Version drift risk |
| directxtex | vcpkg manifest `vcpkg.json:10` | No baseline | Not actually used by D3D11 path; reserved for Vulkan |
| imgui | vcpkg manifest `vcpkg.json:11–14` | Pinned only in editor CI job | Main job unprotected |
| JoltPhysics | vcpkg manifest `vcpkg.json:14` | No baseline | Physics CI uses classic-mode workaround |
| Vulkan SDK | External install + `VULKAN_SDK` env | Runtime pin via CI action version | Only validated in optional job |
| ncurses | System `apt-get` | No version | Linux only; low risk |
| Python deps | `requirements-dev.txt` (5 packages) | Pinned for pytest/jsonschema; `>=` for numpy/scipy/Pillow | CI installs per-job, not cached |

**Root cause of fragility:** `vcpkg.json` has **no `"builtin-baseline"`** field.
Without a baseline, package versions are whatever the system vcpkg resolves at
configure time — different on every developer machine and CI runner image update.

```jsonc
// MISSING from vcpkg.json — add this:
{
  "builtin-baseline": "<commit-sha-matching-2024.12.16-tag>"
}
```

---

### 1c. HLSL Shader Compilation — Toolchain & Source

**D3D11 HLSL (20 shaders):** Compiled **at runtime** via `D3DCompileFromFile`
(the FXC-compatible API in `d3dcompiler.dll`). No offline compile step exists.
HLSL `.hlsl` source files are copied to the build output directory by a `POST_BUILD`
CMake command (`CMakeLists.txt:1086–1093`) and compiled when `LoadScene()` is
first called at runtime.

**Consequences of runtime-only compilation:**
- Shader syntax errors are not caught at `cmake --build` time — only when the
  engine first runs `LoadScene()`.
- The `d3dcompiler_47.dll` version is whatever ships with the developer's Windows SDK.
  All shaders target **Shader Model 4.0** (`vs_4_0` / `ps_4_0`) which is correct for
  D3D11 but limits future SM 6.x features (ray tracing, mesh shaders).
- `fxc.exe` (the offline FXC compiler) ships in every Windows SDK at
  `$(WindowsSdkDir)bin\$(WindowsSDKVersion)\x64\fxc.exe` and could be used as a
  `add_custom_command` step to precompile `.hlsl` → `.cso` at build time.

**DXC / SM 6.x:** Not used anywhere. Appropriate for the current D3D11 / SM 4.0
target. Will be required if the engine ever adds DirectX Raytracing or Mesh Shaders.

**Vulkan SPIR-V:** Compiled offline via `glslc` from `$VULKAN_SDK/Bin`
(`CMakeLists.txt:935–986`). Only `triangle.vert/.frag` exist. The path is correct;
no gaps for the current (deferred) Vulkan scope.

---

### 1d. Asset Pipeline & PNG → DDS

**Current state:**
- `cook_assets.py::_png_to_dds_rgba8()` (lines 251–374) converts PNG → **uncompressed
  DDS RGBA8** using Pillow. The DDS magic bytes and `DDS_HEADER` (124 bytes) are
  written correctly and validated by `d3d11_texture.cpp`.
- Without Pillow, the script falls back to copying the raw PNG — which silently fails
  at load time (no DDS magic). The CI installs Pillow before running `cook_assets.py`
  (`build-windows.yml:668`).
- `Cooked/` in the repo contains only `.gitkeep`. The CI cooks assets on every run
  and validates DDS magic via `--scene authored_content`.
- `cook.exe` (C++, `src/tools/cook/cook_main.cpp`) handles `AssetRegistry.json` and
  non-texture assets but has no texture-conversion capability.

**Format gaps for beta quality:**

| Gap | Current | Beta recommendation |
|---|---|---|
| Block compression | RGBA8 uncompressed | BC7 via `texconv.exe` (DirectXTex) |
| Mip-map generation | `dwMipMapCount=1` always | Texconv generates full mip chains automatically |
| Normal map swizzling | Not handled | BC5 (RG) for normal maps |
| Offline validation | DDS magic check via `--scene authored_content` | Extend to verify resolution, format, mip count |

**Recommended PNG → DDS pipeline for beta:**
Call Microsoft's `texconv.exe` (ships with DirectXTex, already in `vcpkg.json:10`)
from within `cook_assets.py`. This removes the Pillow dependency for texture cooking
and produces GPU-native BC7-compressed textures with full mip chains:

```python
# cook_assets.py — replace _png_to_dds_rgba8() call with:
subprocess.run([
    str(texconv_exe),
    "-f", "BC7_UNORM",   # BC7 block-compressed, gamma-correct
    "-y",                 # overwrite output
    "-o", str(cooked_dir),
    str(src_png)
], check=True)
```

For CI WARP headless (CPU-only, no BC7 hardware decoder needed):
D3D11 WARP fully supports BC7 decompression in software; no special handling needed.

---

## 2. "Install Everything at Once" Goal

### 2a. What Exists Today

**No bootstrap script exists.** There is no `bootstrap.ps1`, `setup.bat`, or CMake
`setup` target. A developer cloning the repo must perform all of these steps manually
(none of which are described together in a single place):

1. Install Visual Studio 2022 ("Desktop development with C++" workload).
2. Optionally install vcpkg and set `VCPKG_ROOT`.
3. Run `cmake --preset windows-debug-engine-only`.
4. Install Python dependencies (`pip install -r requirements-dev.txt`).
5. Run `cook_assets.py` before authored-texture testing.

The engine-only D3D11 path (step 3) actually works without vcpkg, because all
D3D11 dependencies are either Windows-SDK-bundled or vendored Lua. This is the
key insight that makes a bootstrap script practical.

### 2b. Automation Boundary

**MUST be pre-installed (cannot be automated without admin/download):**
- Visual Studio 2022 Community with "Desktop development with C++" workload
  (provides MSVC, Windows SDK ≥ 10.0.19041, Ninja).
- Python 3.9+ (`python.org`).

**CAN be automated in a `scripts/bootstrap.ps1`:**
- vcpkg bootstrap (if not already at `C:\vcpkg`).
- Python dependency install (`pip install -r requirements-dev.txt`).
- CMake configure + build for the engine-only preset.
- `cook_assets.py` execution.
- Smoke-test headless run.

**CMake-native automation (add a `setup` custom target):**
```cmake
add_custom_target(setup
    COMMAND python -m pip install -r ${CMAKE_SOURCE_DIR}/requirements-dev.txt
    COMMAND python ${CMAKE_SOURCE_DIR}/samples/vertical_slice_project/cook_assets.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Installing Python deps and cooking vertical slice assets"
)
```

**Vcpkg manifest-mode fix:** Adding `"builtin-baseline"` to `vcpkg.json` would
allow `cmake --preset windows-debug -DCMAKE_TOOLCHAIN_FILE=...` to work
identically on all machines via a single manifest-mode install, eliminating the
per-job classic-mode workarounds.

---

## 3. Documentation Gaps

### 3a. Existing Docs Inventory

| File | Status | Gap |
|---|---|---|
| `README.md` | Exists (20.7 KB) | No "Prerequisites" table; no "Quick Build" section using the CI-proven preset |
| `docs/ARCHITECTURE.md` | ✅ Well-structured | Updated through M25; solid |
| `docs/ROADMAP.md` | ✅ Per-milestone table | Up to date (2026-04-23) |
| `docs/FF15_REQUIREMENTS_BLUEPRINT.md` | ✅ Completion matrix | Open Tool ⬜ rows for physics/UI/vehicles |
| `docs/PROJECT_MILESTONES.md` | ✅ PR-sized slice definitions | Current |
| `docs/ASSESSMENT_2026-04-23.md` | ✅ Content/cook gap priorities | Authoritative for M24–M27 |
| `docs/COPILOT_CONTINUATION.md` | AI-instruction file | Not a user-facing contributor guide |
| `CONTRIBUTING.md` | **MISSING** | No contributor setup guide anywhere |
| `docs/GETTING_STARTED.md` | **MISSING** | No rapid 5-minute tutorial |
| `docs/SHADER_PIPELINE.md` | **MISSING** | No guide on adding/debugging HLSL shaders |
| `docs/ASSET_PIPELINE.md` | **MISSING** | No end-to-end cook guide for contributors |

### 3b. Beta-Quality Readiness Gaps (Categorized)

**Build reproducibility:**
- No vcpkg baseline pinned → version drift across machines.
- No Release build in CI → optimisation regressions undetected.
- No offline HLSL shader validation → shader errors surface only at runtime.

**CI completeness:**
- No Windows `pytest` step in `build-windows.yml` (Python tool tests only run on Linux).
- `authored_content` scene validates DDS magic but does not confirm an authored texture
  was actually bound (vs. silent 1×1 fallback).
- `build-windows-vulkan` job cannot run shaders (no Vulkan ICD on GitHub runners);
  `continue-on-error: true` means Vulkan build failures are invisible.
- No golden-file auto-regeneration workflow.

**Asset pipeline for beta:**
- RGBA8 uncompressed DDS (4 bytes/pixel) vs BC7 (0.5–1 bytes/pixel) — 4–8× VRAM waste.
- No mip-map generation (`dwMipMapCount=1` always).
- PAK round-trip test exists for write but not for extract-then-load.
- Cooked outputs not committed; fresh clone cannot `--validate-project` without running cook.

**Configuration & input:**
- Resolution, keybindings, audio volume are hardcoded — no user config file.
- `InputMapper` uses hardcoded Win32 VK codes; no gamepad or rebinding support.

**Crash reporting & logging:**
- `Logger` writes to `stdout`/`stderr` only — no file output to `Saved/Logs/`.
- No Windows structured exception handler / minidump writer in `main.cpp`.
- No file/line metadata in `LOG_ERROR` calls.

**Performance capture:**
- No GPU timing queries in `D3D11Renderer`.
- No `PIXBeginEvent`/`PIXEndEvent` markers at render-pass boundaries.
- No CPU profiling annotations.

**Packaging:**
- No CMake `install()` target — no documented procedure for building a distributable.
- No version stamping — git SHA not embedded in binary; no `--version` flag.
- PAK extract-then-load round-trip not tested in CI.

---

## 4. Summary — Top 10 Issues for Beta Quality

| Priority | Issue | Files affected |
|---|---|---|
| 1 | Pin vcpkg baseline in `vcpkg.json` | `vcpkg.json` |
| 2 | Add `scripts/bootstrap.ps1` for one-command Windows developer setup | New file |
| 3 | Add offline HLSL compilation (FXC) as CMake build-time step | `CMakeLists.txt` |
| 4 | Replace RGBA8 DDS cook with BC7 via `texconv.exe` | `cook_assets.py` |
| 5 | Add CMake `install()` target for distributable packaging | `CMakeLists.txt` |
| 6 | Add file-based logging + minidump writer | `src/engine/core/Logger.cpp`, `src/sandbox/main.cpp` |
| 7 | Add engine config JSON (resolution, keybindings, audio volume) | New + `src/sandbox/main.cpp` |
| 8 | Write `CONTRIBUTING.md` and `docs/GETTING_STARTED.md` | New files |
| 9 | Add Windows `pytest` step to `build-windows.yml` | `.github/workflows/build-windows.yml` |
| 10 | Embed git SHA as `--version` flag and in window/log output | `CMakeLists.txt`, `src/sandbox/main.cpp` |

---

## Evidence Anchors

| Claim | File / Line |
|---|---|
| No vcpkg baseline | `vcpkg.json` (entire file; no `"builtin-baseline"` key) |
| Classic-mode vcpkg workaround | `.github/workflows/build-windows.yml:761–764` |
| Runtime HLSL compilation | `CMakeLists.txt:1086–1093`; `src/engine/rendering/d3d11/D3D11Renderer.cpp:53–55` |
| RGBA8 uncompressed DDS only | `samples/vertical_slice_project/cook_assets.py:251–374` |
| `mipCount=1` always | `src/engine/rendering/d3d11/d3d11_texture.cpp:314–343` |
| No `CONTRIBUTING.md` | Root directory listing |
| No `bootstrap.ps1` | Root directory listing |
| `Cooked/` is `.gitkeep` | `samples/vertical_slice_project/Cooked/` |
| No Release CI job | `CMakePresets.json:74–84`; `build-windows.yml` (no release job) |
| No GPU timing or PIX markers | `src/engine/rendering/d3d11/D3D11Renderer.cpp` (no `D3D11_QUERY_TIMESTAMP_DISJOINT` or PIX includes) |
