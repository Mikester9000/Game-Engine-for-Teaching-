# Contributing to Game Engine for Teaching

Thank you for your interest in contributing!  This project is an educational
FFXV-style action RPG engine designed so every line of code can be read, studied,
and extended.  Contributions that improve clarity, correctness, or coverage are
especially welcome.

---

## Prerequisites

| Tool | Version | Notes |
|------|---------|-------|
| Visual Studio 2022 | 17.x | Desktop C++ workload required |
| Python | 3.9+ | For authoring tools and tests |
| Windows SDK | 19041+ | Ships with VS 2022; needed for D3D11 + XAudio2 |
| Git | Any | Must be on PATH |
| vcpkg | Any | Bootstrap script installs it automatically |

---

## Quick Build

The fastest path from a fresh clone to a running engine:

```powershell
# From the repo root in a PowerShell prompt:
.\scripts\bootstrap.ps1
```

This script will:
1. Clone and bootstrap vcpkg (if not already at `C:\vcpkg`)
2. Install Python dependencies from `requirements-dev.txt`
3. Configure CMake with the `windows-ninja-debug-engine-only` preset
4. Build `engine_sandbox.exe`, `cook.exe`, and `pak.exe`
5. Cook the vertical slice sample assets
6. Run a headless smoke test

See `scripts/bootstrap.ps1` for full details and parameters.

---

## Manual Build

```bat
:: Configure (no editor, no Vulkan SDK needed)
cmake --preset windows-ninja-debug-engine-only ^
      -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake

:: Build all targets
cmake --build --preset windows-ninja-debug-engine-only

:: Cook assets
cd samples\vertical_slice_project
python cook_assets.py
cd ..\..

:: Headless smoke test
.\build\windows-ninja-debug-engine-only\engine_sandbox.exe --headless
```

---

## Running Tests

### Python tool tests (fastest feedback)

```bash
# From the repo root:
python -m pytest tools/tests/ tests/ -q
```

### C++ headless acceptance tests

Each scene is a self-contained acceptance test.  Run individually:

```bat
.\build\windows-ninja-debug-engine-only\engine_sandbox.exe --headless --scene <name>
```

| Scene | Tests |
|-------|-------|
| `dynamic_sky` | Sky renderer time-of-day + weather |
| `pbr_mesh` | PBR Cook-Torrance BRDF sphere |
| `pbr_ibl` | IBL (BRDF LUT + irradiance + prefiltered) |
| `shadow_test` | Directional shadow map + PCF |
| `bloom_test` | Bright-pass + Gaussian blur + composite |
| `physics_test` | Jolt rigid body + character + raycast |
| `combat_test` | Combo FSM + damage formula |
| `quest_test` | Quest accept / objective / prereq / fail |
| `save_test` | Save slot round-trip + migration |
| `terrain_test` | Terrain renderer + collision |
| `authored_content` | DDS texture cook verification |

### All CI scenes (batch)

```bat
for %%s in (dynamic_sky pbr_mesh pbr_ibl shadow_test bloom_test physics_test combat_test quest_test save_test terrain_test authored_content) do (
    echo Running %%s...
    .\build\windows-ninja-debug-engine-only\engine_sandbox.exe --headless --scene %%s || exit /b 1
)
```

---

## Coding Conventions

This project uses a specific style to keep the code teachable.  Please follow these rules:

### C++

- **Standard:** C++17.  Use `if constexpr`, structured bindings, `std::optional`.
- **Naming:** `PascalCase` for types, `camelCase` for locals, `UPPER_SNAKE` for macros, `snake_case` for filenames.
- **Comments:** Every non-obvious decision **must** have a `// TEACHING NOTE —` comment explaining *why*, not just *what*.
- **Headers:** Use `#pragma once`.  Implementation stays in `.cpp` unless template/inline.
- **RAII:** Prefer RAII for resources.  No raw `new`/`delete` in new code.
- **Windows:** All new C++ must compile clean with MSVC `/W4`.

For the full conventions, see `docs/COPILOT_CONTINUATION.md`.

### Python

- Version: Python 3.9+.
- Style: PEP 8.  Type hints on all public APIs.
- Docstrings: NumPy-style on all public classes and functions.
- Tests: `pytest` for all new functionality.

### CMake

- Minimum version: 3.16.
- Use `target_*` commands (never global `include_directories`).
- Every new CMake block needs a `# TEACHING NOTE` comment.

---

## PR Checklist

Before opening a pull request, confirm:

- [ ] CI is green on your branch (`build-windows` + `build-linux` jobs pass)
- [ ] New C++ code has `// TEACHING NOTE —` comments on non-obvious decisions
- [ ] New headless scenes are listed in the usage comment at the top of `src/sandbox/main.cpp`
- [ ] New Python functionality has `pytest` coverage
- [ ] New shared data formats have a JSON Schema under `shared/schemas/`
- [ ] `docs/COPILOT_CONTINUATION.md` status table is updated if a milestone changed

---

## Project Structure

```
Game-Engine-for-Teaching-/
├── src/engine/     # Platform-independent engine kernel
│   ├── core/       # Logger, EventBus, EngineConfig
│   ├── rendering/  # D3D11 renderer (+ optional Vulkan)
│   ├── animation/  # Skeleton, anim clips, IK, GPU skinning
│   ├── physics/    # Jolt Physics integration
│   ├── audio/      # XAudio2 + X3DAudio
│   └── ...
├── src/game/       # FFXV-style gameplay systems
├── src/sandbox/    # Windows D3D11 test harness (main.cpp)
├── src/tools/      # cook.exe, pak.exe
├── editor/         # Dear ImGui editor (Creation Suite)
├── tools/          # Python authoring tools
├── shaders/        # HLSL shaders (SM 4.0, D3D11)
├── shared/schemas/ # JSON Schemas for all data formats
├── samples/        # Vertical slice sample project
├── scripts/        # bootstrap.ps1, extract_teaching_notes.py
└── docs/           # Architecture, roadmap, pipeline docs
```

---

## Getting Help

- Read `docs/GETTING_STARTED.md` for a 5-minute walkthrough.
- Browse `docs/ASSESSMENT_2026-04-23.md` for the current gap analysis.
- Open a GitHub issue with the `question` label for design questions.
