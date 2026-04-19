<!-- AUTO-GENERATED — do not edit by hand.
     Regenerate with:  python scripts/extract_teaching_notes.py
-->

# Curriculum Index

This index is **automatically generated** from every `TEACHING NOTE` block in the repository source code.  Each entry links back to the exact line where the lesson was written.

**Total lessons:** 1068 across 40 subsystems.

---

## Table of Contents

- [CMakeLists.txt](#cmakelists.txt) (47 lessons)
- [ci/workflows](#ciworkflows) (34 lessons)
- [editor/CMakeLists.txt](#editorcmakelists.txt) (5 lessons)
- [editor/src](#editorsrc) (45 lessons)
- [engine/animation](#engineanimation) (85 lessons)
- [engine/assets](#engineassets) (27 lessons)
- [engine/audio](#engineaudio) (32 lessons)
- [engine/core](#enginecore) (50 lessons)
- [engine/ecs](#engineecs) (35 lessons)
- [engine/input](#engineinput) (19 lessons)
- [engine/math](#enginemath) (17 lessons)
- [engine/physics](#enginephysics) (54 lessons)
- [engine/platform](#engineplatform) (28 lessons)
- [engine/rendering](#enginerendering) (215 lessons)
- [engine/scripting](#enginescripting) (29 lessons)
- [game/Game.cpp](#gamegame.cpp) (6 lessons)
- [game/Game.hpp](#gamegame.hpp) (1 lesson)
- [game/GameData.hpp](#gamegamedata.hpp) (26 lessons)
- [game/systems](#gamesystems) (85 lessons)
- [game/world](#gameworld) (70 lessons)
- [samples/vertical_slice_project](#samplesvertical_slice_project) (11 lessons)
- [sandbox/main.cpp](#sandboxmain.cpp) (24 lessons)
- [sandbox/test_world.cpp](#sandboxtest_world.cpp) (4 lessons)
- [sandbox/test_world.hpp](#sandboxtest_world.hpp) (1 lesson)
- [scripts/check_architecture.py](#scriptscheck_architecture.py) (8 lessons)
- [scripts/enemies.lua](#scriptsenemies.lua) (1 lesson)
- [scripts/extract_teaching_notes.py](#scriptsextract_teaching_notes.py) (2 lessons)
- [scripts/main.lua](#scriptsmain.lua) (2 lessons)
- [scripts/quests.lua](#scriptsquests.lua) (1 lesson)
- [shared/runtime](#sharedruntime) (12 lessons)
- [src/main.cpp](#srcmain.cpp) (1 lesson)
- [tests/cook](#testscook) (5 lessons)
- [tools/anim_authoring](#toolsanim_authoring) (20 lessons)
- [tools/audio_authoring](#toolsaudio_authoring) (28 lessons)
- [tools/audio_engine.py](#toolsaudio_engine.py) (6 lessons)
- [tools/audit_teaching_notes.py](#toolsaudit_teaching_notes.py) (10 lessons)
- [tools/cook](#toolscook) (12 lessons)
- [tools/creation_engine.py](#toolscreation_engine.py) (5 lessons)
- [tools/tests](#toolstests) (3 lessons)
- [tools/validate-assets.py](#toolsvalidate-assets.py) (2 lessons)

---

## CMakeLists.txt

### Superbuild / Monorepo Pattern

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L5) (line 5)

──────────────────────────────────────────────
A "superbuild" root CMakeLists.txt orchestrates multiple sub-projects in a
single CMake invocation. This lets you build the engine, editor, and any
C++ tools all at once, yet keep each subproject's build logic isolated in
its own subdirectory.

Monorepo layout:
  src/      — C++ engine & game source code (existing, always built)
  editor/   — Qt 6 Widgets editor (optional; requires Qt installed)
  shared/   — Shared headers and JSON schemas used by all projects

### CMake Build System Design

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L17) (line 17)

─────────────────────────────────────────
CMake is the de-facto standard build-system generator for C++ projects.
It does NOT compile code directly — it generates:
  • Makefiles    (Linux / macOS)
  • Xcode projects (macOS)
  • Visual Studio solutions (Windows)  ← our primary target
…from this platform-neutral description.

Key CMake concepts used here:
  target_sources      — list of .cpp files to compile.
  target_include_dirs — where the compiler looks for #include files.
  target_link_libs    — external libraries to link against.
  target_compile_opts — compiler warning/optimisation flags.

### Out-of-Source Builds (Windows recommended)

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L32) (line 32)

──────────────────────────────────────────────────────────
Use CMake Presets (CMakePresets.json) for the recommended workflow:

  cmake --preset windows-debug
  cmake --build --preset windows-debug

Or the manual workflow:
  mkdir build && cd build
  cmake .. -G "Visual Studio 17 2022" -A x64
  cmake --build . --config Debug --target engine_sandbox

### Optional Subprojects

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L51) (line 51)

The editor uses Dear ImGui (MIT, zero install) rendered via D3D11.
It requires vcpkg to install the imgui[docking,dx11-binding,win32-binding]
package (listed in vcpkg.json).  The engine builds and runs without it.
Set BUILD_EDITOR=ON to include the editor:

  cmake --preset windows-debug -DBUILD_EDITOR=ON

option(BUILD_EDITOR "Build the Dear ImGui Creation Suite editor" OFF)

### C++17 is required for:

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L64) (line 64)

• if constexpr             (used in LuaEngine::SetGlobal<T>)
  • std::optional            (future use)
  • Structured bindings      (auto [k, v] = ...)
  • Fold expressions         (World::View<Components...>)
set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

### CMake Options

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L95) (line 95)

option() declares a boolean flag the user can set on the cmake command line:
  cmake .. -DENGINE_ENABLE_TERMINAL=OFF
The second argument is a help string shown by cmake-gui and ccmake.
The third argument is the default value.

We gate the terminal (ncurses) renderer to non-Windows platforms because:
  1. ncurses is a Linux/macOS library; there is no first-class port on MSVC.
  2. On Windows the engine uses the Vulkan renderer instead.
---------------------------------------------------------------------------

### ENGINE_ENABLE_TERMINAL

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L106) (line 106)

Defaults to ON everywhere EXCEPT Windows (where ncurses is unavailable).
if(WIN32)
option(ENGINE_ENABLE_TERMINAL "Build the ncurses terminal renderer target" OFF)
else()
option(ENGINE_ENABLE_TERMINAL "Build the ncurses terminal renderer target" ON)
endif()

### ENGINE_ENABLE_D3D11 (default ON on Windows)

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L115) (line 115)

---------------------------------------------------------------------------
Direct3D 11 is the *default* Windows renderer.  It ships with Windows 7+
as part of the OS — no SDK download or separate install required.
It supports hardware as old as a GeForce GT 610 (Feature Level 11_0) via the
hardware driver, and any machine/CI runner via D3D11 WARP (software).

This is the recommended baseline for development and CI because:
  • cmake --preset windows-debug-engine-only works WITHOUT a Vulkan SDK.
  • engine_sandbox.exe --headless exits 0 on GitHub-hosted runners (WARP).
  • GT610-era hardware can run the engine at full feature level.
---------------------------------------------------------------------------
if(WIN32)
option(ENGINE_ENABLE_D3D11 "Build the D3D11 renderer backend (GT610-compatible)" ON)
else()
option(ENGINE_ENABLE_D3D11 "Build the D3D11 renderer backend (GT610-compatible)" OFF)
endif()

### ENGINE_ENABLE_VULKAN (optional, high-end path)

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L134) (line 134)

---------------------------------------------------------------------------
Vulkan is the *high-end / modern* renderer backend.  It is built when ON
but is NOT the default runtime renderer — users must pass --renderer vulkan
explicitly to use it.

IMPORTANT: Vulkan SDK is OPTIONAL.  If ENGINE_ENABLE_VULKAN=ON but the
Vulkan SDK is not installed, CMake will log a warning and automatically
disable Vulkan rather than failing the entire configure.  This ensures
that the D3D11 default path always builds even without a Vulkan SDK.

To build with Vulkan:
  1. Install the Vulkan SDK from https://vulkan.lunarg.com/
  2. cmake --preset windows-debug -DENGINE_ENABLE_VULKAN=ON
---------------------------------------------------------------------------
if(WIN32)
option(ENGINE_ENABLE_VULKAN "Build the Vulkan renderer backend (high-end optional)" ON)
else()
option(ENGINE_ENABLE_VULKAN "Build the Vulkan renderer backend (high-end optional)" OFF)
endif()

### ENGINE_ENABLE_PHYSICS (M5 Jolt Physics)

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L156) (line 156)

---------------------------------------------------------------------------
Jolt Physics (MIT licence, by Jorrit Rouwe) is a high-performance physics
library used in Horizon: Forbidden West.  It is installed via vcpkg:

  vcpkg install joltphysics

When ENGINE_ENABLE_PHYSICS=ON and JoltPhysics is found (via vcpkg toolchain
or a manual installation), the physics subsystem is compiled into
engine_sandbox and the ENGINE_ENABLE_PHYSICS preprocessor macro is defined.

If JoltPhysics is NOT found, the option silently falls back to OFF so the
D3D11 baseline build always succeeds — even without Jolt installed.
This mirrors the Vulkan optional pattern used above.

Acceptance test: build with -DENGINE_ENABLE_PHYSICS=ON then run:
  engine_sandbox.exe --headless --scene physics_test
Expected output: "[PASS] physics_test: all 3 acceptance tests passed."
---------------------------------------------------------------------------
option(ENGINE_ENABLE_PHYSICS "Build Jolt Physics integration (M5)" ON)

### find_package(Jolt CONFIG)

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L178) (line 178)

The vcpkg port for joltphysics (v5+) provides a CMake config that
registers the imported target Jolt::Jolt.  The CMake package name is
"Jolt" (not "JoltPhysics") — confirmed by the vcpkg install message:
  "find_package(Jolt CONFIG REQUIRED)"
QUIET suppresses the "not found" message when Jolt is not installed
(we handle it ourselves below).
find_package(Jolt CONFIG QUIET)
if(Jolt_FOUND)
message(STATUS "Jolt (JoltPhysics) found: physics subsystem ENABLED.")
else()
message(STATUS "Jolt (JoltPhysics) NOT found — ENGINE_ENABLE_PHYSICS will be OFF. "
"Install via: vcpkg install joltphysics")
set(ENGINE_ENABLE_PHYSICS OFF CACHE BOOL "" FORCE)
endif()
endif()

### find_package(Curses) sets:

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L200) (line 200)

CURSES_LIBRARIES    — ncurses link flags
  CURSES_INCLUDE_DIRS — header directory (usually /usr/include)

We guard this with ENGINE_ENABLE_TERMINAL so that a Windows CMake configure
does NOT fail on a missing ncurses package.
if(ENGINE_ENABLE_TERMINAL)
find_package(Curses REQUIRED)
message(STATUS "ncurses found: ${CURSES_LIBRARIES}")
endif()

### Build Lua from source (preferred)

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L212) (line 212)

──────────────────────────────────────────────────
Lua is a small, self-contained library (~30 .c files).  Building it from the
bundled source in  Lua/lua-5.5.0/src/  has several advantages over linking
against a pre-built system library:

  • Zero external dependencies — works on any platform, in any CI, with any
    compiler.  No "sudo apt install liblua5.4-dev" step required.
  • Consistent version — everyone builds against Lua 5.5.0 regardless of what
    is installed on their machine.
  • Transparent — students can read the Lua source alongside the engine source.

Detection order:
  1. Bundled source in Lua/lua-5.5.0/src/   ← preferred (always present)
  2. System Lua 5.5 via pkg-config           ← fallback for distros
  3. System Lua 5.4 via pkg-config           ← further fallback
  4. Scripting disabled (warning only)       ← never a hard error
---------------------------------------------------------------------------

### Building a static library from C source in CMake

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L239) (line 239)

-----------------------------------------------------------------------
add_library(target STATIC files…) compiles the listed .c files and
archives them into a single .a / .lib file.  Any target that links
against lua55_static will automatically get the include directories via
the PUBLIC target_include_directories() call below — no manual
include_directories() needed at the call-site.
-----------------------------------------------------------------------

### Platform compile flags for Lua

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L265) (line 265)

On POSIX (Linux/macOS) Lua needs _GNU_SOURCE for POSIX math functions and
popen().  On Windows MSVC we disable several noisy warnings that come from
Lua's intentional C idioms (e.g. condition is always true for integer type).
if(MSVC)
target_compile_options(lua55_static PRIVATE
/wd4244   # conversion from 'double' to 'float'
/wd4267   # size_t → int narrowing
/wd4310   # cast truncates constant value
/wd4324   # structure padding
/wd4702   # unreachable code (Lua uses deliberate unreachable paths)
)
else()
target_compile_definitions(lua55_static PRIVATE _GNU_SOURCE)
target_compile_options(lua55_static PRIVATE
-w   # suppress all warnings in third-party Lua source
)
endif()

### find_package(Vulkan QUIET) — soft failure

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L321) (line 321)

We use QUIET (no error message on missing SDK) and check Vulkan_FOUND
manually.  If the SDK is absent we disable Vulkan and log a warning so
the configure still succeeds and D3D11 can build.

This is the idiomatic "optional dependency" pattern in CMake:
  find_package(X QUIET)
  if(NOT X_FOUND)
      message(WARNING "X not found; feature Y disabled.")
      set(ENGINE_ENABLE_X OFF CACHE BOOL "" FORCE)
  endif()

The Vulkan SDK sets the VULKAN_SDK environment variable.  CMake's built-in
FindVulkan module uses that variable to locate vulkan-1.lib and vulkan/vulkan.h.
if(ENGINE_ENABLE_VULKAN)
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
message(STATUS "Vulkan SDK found: ${Vulkan_LIBRARIES}")
else()
message(WARNING
"[ENGINE] Vulkan SDK not found — ENGINE_ENABLE_VULKAN disabled.\n"
"  Install from https://vulkan.lunarg.com/ to enable the Vulkan backend.\n"
"  The D3D11 backend (ENGINE_ENABLE_D3D11) will still be built."
)
set(ENGINE_ENABLE_VULKAN OFF CACHE BOOL "" FORCE)
endif()
endif()

### Conditional Target Creation

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L363) (line 363)

add_executable() is only called when ENGINE_ENABLE_TERMINAL is ON.
On Windows this block is entirely skipped so MSVC never tries to compile
the ncurses-dependent Renderer.cpp and InputSystem.cpp.
---------------------------------------------------------------------------
if(ENGINE_ENABLE_TERMINAL)

### ENGINE_ENABLE_LUA compile definition

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L408) (line 408)

The terminal game links against Lua 5.5 (built from bundled source or
found as a system package).  ENGINE_ENABLE_LUA activates the Lua
scripting hooks in CombatSystem.cpp and CampSystem.cpp.  When Lua is not
found (unlikely given the bundled source) the hooks compile out cleanly.
if(LUA_LIBRARIES)
target_compile_definitions(game PRIVATE ENGINE_ENABLE_LUA)
endif()

### engine_sandbox Rendering Strategy

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L430) (line 430)

─────────────────────────────────────────────────
engine_sandbox supports two rendering backends selectable at runtime:

  D3D11  (default) — Direct3D 11; GT610-compatible; ships with Windows.
                     Uses WARP software rasteriser for headless CI runs.
                     Built when ENGINE_ENABLE_D3D11=ON (default on Windows).

  Vulkan (optional) — Vulkan 1.0+; modern / high-end.
                      Requires a Vulkan ICD at runtime.
                      Built when ENGINE_ENABLE_VULKAN=ON AND SDK is present.

The D3D11 path is the primary target because:
  • It builds without any external SDK (d3d11.lib ships with the Windows SDK).
  • It runs on CI runners via D3D11 WARP (no GPU required).
  • It supports GT610-era hardware (D3D_FEATURE_LEVEL_10_0 minimum).

Build commands (D3D11, no Vulkan SDK needed):
  cmake --preset windows-debug-engine-only
  cmake --build --preset windows-debug-engine-only --target engine_sandbox
---------------------------------------------------------------------------

### Animation Runtime Sources (M4)

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L468) (line 468)

-----------------------------------------------------------------------
The animation runtime is renderer-agnostic: it runs on both D3D11 and
Vulkan paths.  We add it to SANDBOX_SOURCES unconditionally (still
inside the WIN32 guard since the sandbox is Windows-only).
-----------------------------------------------------------------------
list(APPEND SANDBOX_SOURCES
src/engine/animation/skeleton.cpp
src/engine/animation/anim_clip.cpp
src/engine/animation/blend_tree.cpp
src/engine/animation/animation_system.cpp

### M4b: IK solver (renderer-agnostic, pure C++17).

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L479) (line 479)

Two-Bone analytical IK and FABRIK iterative N-joint IK.
Lives in animation/ alongside the other CPU-side animation systems.
src/engine/animation/ik_solver.cpp
)

### Conditional Source Files

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L487) (line 487)

We add D3D11Renderer.cpp only when the D3D11 feature is enabled.
This keeps the source list explicit and makes it easy to see which
files belong to which backend.
-----------------------------------------------------------------------
if(ENGINE_ENABLE_D3D11)
list(APPEND SANDBOX_SOURCES
src/engine/rendering/d3d11/D3D11Renderer.cpp

### M3: D3D11 texture loader (DDS/BC7 → ID3D11SRV).

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L495) (line 495)

d3d11_texture.cpp is a self-contained DDS parser that uses only
the Windows SDK headers already required by D3D11Renderer.
src/engine/rendering/d3d11/d3d11_texture.cpp

### M4b: GpuSkinningBuffer — D3D11 DYNAMIC constant

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L499) (line 499)

buffer that uploads 64 joint matrices (4096 bytes) to the VS
every frame.  Uses Map/WRITE_DISCARD for zero-stall streaming.
src/engine/animation/gpu_skinning.cpp
)
endif()

### XAudio2 is Windows-only

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L521) (line 521)

xaudio2.h and xaudio2.lib ship with every Windows SDK installation
(alongside d3d11.h / d3d11.lib).  No separate download is needed.
The sources are added to engine_sandbox (not the Linux terminal game).
-----------------------------------------------------------------------
list(APPEND SANDBOX_SOURCES
src/engine/audio/xaudio2_backend.cpp
src/engine/audio/audio_system.cpp
)

### Optional Physics Subsystem

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L534) (line 534)

-----------------------------------------------------------------------

### Physics Source Inclusion Strategy

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L536) (line 536)

-----------------------------------------------------------------------
physics_world.cpp, rigid_body.cpp, character_controller.cpp, and
raycast.cpp all depend on Jolt Physics types and the PhysicsWorld
implementation.  They are ONLY compiled when ENGINE_ENABLE_PHYSICS=ON
and JoltPhysics was found via vcpkg.

hit_volume.cpp is an exception: it contains only pure C++17 AABB math
with no Jolt dependency.  It is compiled in ALL builds so that the
HitVolumeManager (used by the combat system) is always available.

Why NOT include rigid_body.cpp without Jolt?
rigid_body.cpp calls PhysicsWorld::CreateBox/CreateSphere/etc.  Those
methods are defined in physics_world.cpp — which is only compiled when
Jolt is present.  Including rigid_body.cpp without physics_world.cpp
would produce unresolved external linker errors (LNK2019 on MSVC).
-----------------------------------------------------------------------
if(ENGINE_ENABLE_PHYSICS AND Jolt_FOUND)
list(APPEND SANDBOX_SOURCES
src/engine/physics/physics_world.cpp
src/engine/physics/rigid_body.cpp
src/engine/physics/character_controller.cpp
src/engine/physics/raycast.cpp
)
endif()
hit_volume.cpp has no Jolt dependency — always include it.
list(APPEND SANDBOX_SOURCES
src/engine/physics/hit_volume.cpp
)

### Conditional Scripting in engine_sandbox

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L569) (line 569)

LuaEngine.cpp is added to engine_sandbox ONLY when LUA_BUNDLED=ON
(i.e. headers in Lua/include/ AND import lib in Lua/lib/ are found).
Without bundled Lua the scripting system compiles out via ENGINE_ENABLE_LUA
guards.  No build error occurs — scripting is simply unavailable.
-----------------------------------------------------------------------
if(LUA_BUNDLED)
list(APPEND SANDBOX_SOURCES src/engine/scripting/LuaEngine.cpp)
endif()

### Cross-Platform Game Systems

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L582) (line 582)

The gameplay systems (CombatSystem, AISystem, WeatherSystem, etc.) are
pure C++17 with no platform or ncurses dependencies.  They compile on
both Linux (terminal game) and Windows (engine_sandbox) alike.
Including them here lets engine_sandbox run a full simulation and visually
verify all systems are operational via the --scene testworld flag.
-----------------------------------------------------------------------
list(APPEND SANDBOX_SOURCES
src/sandbox/test_world.cpp
src/game/systems/CombatSystem.cpp
src/game/systems/AISystem.cpp
src/game/systems/WeatherSystem.cpp
src/game/systems/QuestSystem.cpp
src/game/systems/InventorySystem.cpp
src/game/systems/ShopSystem.cpp
src/game/systems/CampSystem.cpp
src/game/world/TileMap.cpp
src/game/world/WorldMap.cpp
src/game/world/Zone.cpp
)

### d3d11.lib and dxgi.lib

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L622) (line 622)

These libraries ship with the Windows SDK (included in every Visual
Studio installation).  They do NOT require a separate Vulkan-style SDK
download.  Any Windows developer machine already has them.
-----------------------------------------------------------------------
if(ENGINE_ENABLE_D3D11)
target_link_libraries(engine_sandbox PRIVATE d3d11.lib dxgi.lib d3dcompiler.lib)
endif()

### xaudio2.lib and ole32.lib

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L631) (line 631)

xaudio2.lib ships with the Windows SDK (alongside d3d11.lib).
ole32.lib provides CoInitializeEx / CoUninitialize for the COM runtime
required by XAudio2.  Both are always present on any MSVC installation.
target_link_libraries(engine_sandbox PRIVATE xaudio2.lib ole32.lib)

### Jolt::Jolt

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L641) (line 641)

The vcpkg joltphysics package (v5+) provides an imported CMake target
"Jolt::Jolt" that carries all include directories and link libraries
automatically.  Linking with target_link_libraries() is all that is
needed — no manual include_directories() or add_library() calls required.
if(ENGINE_ENABLE_PHYSICS AND Jolt_FOUND)
target_link_libraries(engine_sandbox PRIVATE Jolt::Jolt)
endif()

### Compile-Time Feature Flags

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L653) (line 653)

ENGINE_ENABLE_D3D11 and ENGINE_ENABLE_VULKAN are passed as preprocessor
macros so the RendererFactory.hpp can conditionally include the right
backend headers and the IRenderer implementations can guard
platform-specific code.

### UNICODE and _UNICODE

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L659) (line 659)

Win32Window.cpp uses std::wstring / const wchar_t* for the window title.
Without these macros MSVC maps CreateWindowEx → CreateWindowExA (narrow),
causing C2440/C2664 errors.
-----------------------------------------------------------------------

### Incremental compile definitions

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L664) (line 664)

We start with definitions that are always required (Win32 header trimming
+ Unicode), then conditionally append backend feature flags.
ENGINE_ENABLE_D3D11 / ENGINE_ENABLE_VULKAN flow into RendererFactory.hpp
so the factory can gate which concrete renderer header(s) it includes.
-----------------------------------------------------------------------
set(SANDBOX_DEFS WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)

### ENGINE_ENABLE_PHYSICS compile definition

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L680) (line 680)

Defined when Jolt Physics is available.  Physics .cpp files use
#ifdef ENGINE_ENABLE_PHYSICS to gate Jolt headers/code.
main.cpp uses it to gate the --scene physics_test acceptance tests.
if(ENGINE_ENABLE_PHYSICS AND Jolt_FOUND)
list(APPEND SANDBOX_DEFS ENGINE_ENABLE_PHYSICS)
endif()

### Lua in engine_sandbox

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L693) (line 693)

──────────────────────────────────────
When LUA_BUNDLED=ON (Lua/lua-5.5.0/src/ exists), the lua55_static CMake
target is already built from source.  We:
  1. Link engine_sandbox against lua55_static (static lib — no DLL needed).
  2. Include directories are inherited via target_link_libraries PUBLIC.
  3. Define ENGINE_ENABLE_LUA to activate all scripting hooks.
  4. Copy lua55.dll alongside the exe (optional — only needed if users
     also run standalone .lua scripts via lua55.exe; the embedded engine
     uses the static lib).
-----------------------------------------------------------------------
if(LUA_BUNDLED)
LUA_LIBRARIES is the CMake target lua55_static; include dirs are
inherited via the PUBLIC target_include_directories on that target.
target_link_libraries(engine_sandbox PRIVATE ${LUA_LIBRARIES})
target_compile_definitions(engine_sandbox PRIVATE ENGINE_ENABLE_LUA)
Copy lua55.dll alongside the exe for convenience (running scripts).
set(LUA55_DLL "${CMAKE_SOURCE_DIR}/Lua/lua55.dll")
if(EXISTS "${LUA55_DLL}")
add_custom_command(TARGET engine_sandbox POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_if_different
"${LUA55_DLL}"
"$<TARGET_FILE_DIR:engine_sandbox>/lua55.dll"
COMMENT "Copying lua55.dll alongside engine_sandbox.exe"
)
endif()
message(STATUS "engine_sandbox: Lua 5.5 scripting ENABLED (static, from source)")
else()
message(STATUS "engine_sandbox: Lua scripting DISABLED (no Lua/include/ or Lua/lib/)")
endif()

### SUBSYSTEM:CONSOLE

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L725) (line 725)

-----------------------------------------------------------------------
By default MSVC creates a GUI app (WinMain entry, no console).
We request CONSOLE so std::cout / std::cerr output is visible in
a terminal — essential for a teaching project and CI validation.
-----------------------------------------------------------------------
if(MSVC)
set_target_properties(engine_sandbox PROPERTIES
LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)
endif()

### Shader Compilation with glslc

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L740) (line 740)

GLSL shaders cannot be loaded directly by Vulkan — they must be compiled
to SPIR-V first.  glslc ships with the Vulkan SDK.

D3D11 uses HLSL compiled to CSO files (or runtime compilation).  HLSL
shader compilation via FXC/DXC will be added in M3 D3D11 textures.
-----------------------------------------------------------------------
if(ENGINE_ENABLE_VULKAN AND Vulkan_FOUND)
find_program(GLSLC_EXECUTABLE glslc
HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
DOC   "glslc GLSL-to-SPIR-V compiler from the Vulkan SDK")

### $<TARGET_FILE_DIR:engine_sandbox>

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L782) (line 782)

This generator expression expands to the directory containing
the built executable (e.g. build/Debug/ on MSVC).
add_custom_command(TARGET engine_sandbox POST_BUILD
COMMAND ${CMAKE_COMMAND} -E make_directory
"$<TARGET_FILE_DIR:engine_sandbox>/shaders"
COMMAND ${CMAKE_COMMAND} -E copy_if_different
${SPV_OUTPUTS}
"$<TARGET_FILE_DIR:engine_sandbox>/shaders/"
COMMENT "Copying compiled shaders to output directory"
)
else()
message(WARNING
"glslc not found — Vulkan SPIR-V shaders will NOT be compiled.\n"
"  Install the Vulkan SDK from https://vulkan.lunarg.com/ "
"and ensure %VULKAN_SDK%/Bin is on your PATH."
)
endif() # GLSLC_EXECUTABLE
endif() # ENGINE_ENABLE_VULKAN

### D3D11 HLSL Shaders: Copy to output directory

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L803) (line 803)

-----------------------------------------------------------------------
D3D11Renderer compiles HLSL shaders at runtime using D3DCompileFromFile
(see D3D11Renderer.cpp :: LoadScene).  The .hlsl source files must
therefore be present next to the executable at runtime.

We copy them with a POST_BUILD command so that both Debug and Release
configurations automatically get up-to-date shader files whenever the
engine is built.

The shaders/ sub-directory mirrors the path returned by GetShaderDir()
in src/sandbox/main.cpp.
-----------------------------------------------------------------------
if(ENGINE_ENABLE_D3D11)
set(HLSL_SHADERS
"${CMAKE_SOURCE_DIR}/shaders/textured_quad.vs.hlsl"
"${CMAKE_SOURCE_DIR}/shaders/textured_quad.ps.hlsl"

### M4b: GPU skinning HLSL shaders.

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L820) (line 820)

skinned_mesh.vs.hlsl implements linear blend skinning (LBS):
  worldPos = Σ weight[i] * (g_joints[boneIndex[i]] * bindPos)
skinned_mesh.ps.hlsl applies Lambertian lighting + color gradient.
"${CMAKE_SOURCE_DIR}/shaders/skinned_mesh.vs.hlsl"
"${CMAKE_SOURCE_DIR}/shaders/skinned_mesh.ps.hlsl"
)

### Standalone Tool Target

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L843) (line 843)

─────────────────────────────────────────────────────────────────────────────
The cook tool is a platform-independent C++ executable that:
  1. Reads AssetRegistry.json from the given project directory.
  2. Copies/converts source assets → Cooked/ directory.
  3. Writes Cooked/assetdb.json for the C++ runtime (AssetDB).

It is independent of the Vulkan renderer so it builds on all platforms
(Windows, Linux, macOS).

Usage after build:
  ./cook --project samples/vertical_slice_project
  .\Debug\cook.exe --project samples\vertical_slice_project
---------------------------------------------------------------------------
add_executable(cook
src/tools/cook/cook_main.cpp
src/engine/core/Logger.cpp
)

### target_include_directories (PRIVATE)

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L862) (line 862)

Only this target needs to see src/ for #include "engine/core/Logger.hpp".
We use PRIVATE so the include path does not leak to anything that links
against cook (there is nothing, but it is good practice).
target_include_directories(cook PRIVATE
src/
)

### MSVC /SUBSYSTEM:CONSOLE

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L870) (line 870)

Same reasoning as engine_sandbox: we want stdout/stderr visible in a
terminal window on Windows.
if(MSVC)
set_target_properties(cook PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
endif()

### add_subdirectory()

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L900) (line 900)

add_subdirectory(dir) tells CMake to also process dir/CMakeLists.txt.
Each subdirectory is a self-contained "project" with its own targets and
build rules, but they can share variables set by the parent (e.g., the
C++ standard already set above).

### Dear ImGui Editor Subproject

**Source:** [`CMakeLists.txt`](CMakeLists.txt#L907) (line 907)

The editor is a Dear ImGui (MIT) application that provides:
  * Content browser  -- file tree via std::filesystem
  * Scene editor     -- 2D entity canvas with JSON save/load (nlohmann-json)
  * Menu bar         -- open project, save scene, cook assets
Requires vcpkg with imgui[docking,dx11-binding,win32-binding] installed.
No Qt installation needed.
message(STATUS "BUILD_EDITOR=ON: including editor/ subproject (Dear ImGui)")
add_subdirectory(editor)
else()
message(STATUS "BUILD_EDITOR=OFF: skipping editor/ (pass -DBUILD_EDITOR=ON to include)")
endif()

---

## ci/workflows

### Why a Dedicated Architecture Lint Workflow?

**Source:** [`.github/workflows/architecture-lint.yml`](.github/workflows/architecture-lint.yml#L5) (line 5)

-----------------------------------------------------------
Large codebases rot slowly.  Each individual commit that sneaks a
cross-layer dependency or creates a 700-line source file seems harmless
in isolation, but after dozens of such commits the codebase becomes hard
to navigate and hard to teach from.  A dedicated CI gate that rejects
these individual commits is the cheapest way to keep the codebase healthy.

This workflow runs two scripts on every push and pull-request:

  1. scripts/check_architecture.py
     • Warns on C++ files exceeding 500 lines (teaching exceptions listed).
     • Errors on forbidden cross-layer #include directives.
     • Warns on C++ source files that contain no TEACHING NOTE block.

  2. scripts/extract_teaching_notes.py
     • Regenerates docs/CURRICULUM_INDEX.md from all TEACHING NOTE blocks.
     • Fails if the committed index is out of date (on pull requests).

Both scripts are pure Python with no third-party dependencies, so the
job runs quickly without any pip install step.

============================================================================

### GitHub Actions Triggers (paths filter)

**Source:** [`.github/workflows/architecture-lint.yml`](.github/workflows/architecture-lint.yml#L28) (line 28)

-------------------------------------------------------
The ``paths`` filter means this workflow only runs when files that could
affect the architecture checks actually change.  A documentation-only PR
that touches only ``docs/*.md`` will generally NOT trigger this workflow,
saving CI minutes and reducing noise.

Exception: ``docs/CURRICULUM_INDEX.md`` IS included in the filter because
it is auto-generated by scripts/extract_teaching_notes.py and the CI gate
verifies that the committed index matches what the script would regenerate.
A PR that modifies only CURRICULUM_INDEX.md (e.g., a manual edit) will
therefore trigger this workflow so that stale-index violations are caught.
============================================================================

### Principle of Least Privilege

**Source:** [`.github/workflows/architecture-lint.yml`](.github/workflows/architecture-lint.yml#L66) (line 66)

Grant only the permissions this workflow actually needs.
contents: read  — needed to check out the repository.
No write permissions are needed.
permissions:
contents: read

### actions/checkout

**Source:** [`.github/workflows/architecture-lint.yml`](.github/workflows/architecture-lint.yml#L83) (line 83)

Clones the repository so subsequent steps can read the source files.
-----------------------------------------------------------------------
- name: Checkout repository
uses: actions/checkout@v4

### Python Setup

**Source:** [`.github/workflows/architecture-lint.yml`](.github/workflows/architecture-lint.yml#L90) (line 90)

Both scripts require Python 3.9+ and use only the standard library.
We pin to Python 3.11 in CI for reproducibility — the same version
used by all other workflows in this repository.  No third-party
packages are needed, so no pip install step is required.
-----------------------------------------------------------------------
- name: Set up Python 3.11
uses: actions/setup-python@v5
with:
python-version: '3.11'

### Stale-Index Detection Pattern

**Source:** [`.github/workflows/architecture-lint.yml`](.github/workflows/architecture-lint.yml#L136) (line 136)

-----------------------------------------------
We regenerate the file, then run ``git diff --exit-code`` which exits
with code 1 if the file changed.  This pattern is common in CI:
generate → compare → fail if different.  It ensures that contributors
who add TEACHING NOTE blocks also update the index before merging.
-----------------------------------------------------------------------
- name: Regenerate curriculum index
run: python scripts/extract_teaching_notes.py --repo-root .

### CI Workflow Purpose

**Source:** [`.github/workflows/build-linux.yml`](.github/workflows/build-linux.yml#L4) (line 4)

============================================================================
This workflow validates the Linux terminal game target on every push and
pull request.  It ensures:

  1. The CMake configure step succeeds.
  2. The C++ code compiles without errors.
  3. The Python authoring tool test suites (audio + animation) pass.

The Vulkan engine_sandbox target is Windows-only (VkSurfaceKHR requires a
Win32 HWND).  Its build is validated in the build-windows workflow.

============================================================================

### GitHub Actions Triggers

**Source:** [`.github/workflows/build-linux.yml`](.github/workflows/build-linux.yml#L17) (line 17)

============================================================================
push / pull_request with paths filters mean this workflow only runs when
relevant files change.  CI time is precious — don't run the full build for
a docs-only change.
============================================================================

### Least-Privilege Permissions

**Source:** [`.github/workflows/build-linux.yml`](.github/workflows/build-linux.yml#L49) (line 49)

GitHub Actions tokens default to write access.  Explicitly restricting
to read-only (contents: read) follows the principle of least privilege —
a compromised action cannot push or modify the repository.
permissions:
contents: read

### actions/checkout

**Source:** [`.github/workflows/build-linux.yml`](.github/workflows/build-linux.yml#L58) (line 58)

Clones the repository into the runner's workspace so subsequent steps
can access the source files.  fetch-depth: 0 gives the full git history
(needed if any step references git tags or the commit count).
-----------------------------------------------------------------------
- name: Checkout repository
uses: actions/checkout@v4

### Installing Dependencies

**Source:** [`.github/workflows/build-linux.yml`](.github/workflows/build-linux.yml#L67) (line 67)

The terminal game requires:
  cmake    — build system generator
  build-essential — GCC + Make
  libncurses5-dev — ncurses terminal renderer
  liblua5.4-dev   — Lua 5.4 scripting engine
-----------------------------------------------------------------------
- name: Install build dependencies
run: |
sudo apt-get update -qq
sudo apt-get install -y --no-install-recommends \
cmake \
build-essential \
libncurses5-dev \
libncursesw5-dev \
liblua5.4-dev

### Parallel Build (-j)

**Source:** [`.github/workflows/build-linux.yml`](.github/workflows/build-linux.yml#L97) (line 97)

$(nproc) returns the number of logical CPU cores on the runner.
Passing -j$(nproc) to make enables parallel compilation, which is
significantly faster than single-threaded compilation on multi-core CI
runners.
-----------------------------------------------------------------------
- name: Build
run: cmake --build build --config Debug -- -j$(nproc)

### cook is not Vulkan-dependent

**Source:** [`.github/workflows/build-linux.yml`](.github/workflows/build-linux.yml#L115) (line 115)

The cook tool uses only C++17 standard library (std::filesystem,
std::fstream, etc.) and our Logger.  It builds on Linux as well as
Windows.  We verify it here to catch compile errors early.
-----------------------------------------------------------------------
- name: Verify cook binary exists
run: test -f build/cook

### Python Setup

**Source:** [`.github/workflows/build-linux.yml`](.github/workflows/build-linux.yml#L137) (line 137)

We pin to Python 3.11 for reproducibility.  The tools require 3.9+.
-----------------------------------------------------------------------
- name: Set up Python 3.11
uses: actions/setup-python@v5
with:
python-version: '3.11'

### Windows CI Workflow Purpose

**Source:** [`.github/workflows/build-windows.yml`](.github/workflows/build-windows.yml#L4) (line 4)

============================================================================
This workflow validates the Windows engine_sandbox (D3D11 default) and the
cross-platform cook.exe tool on every push and pull request.

What it validates:
  1. CMake configure succeeds WITHOUT a Vulkan SDK (D3D11 path).
  2. engine_sandbox.exe compiles without errors (MSVC /W4).
  3. cook.exe compiles without errors.
  4. engine_sandbox.exe --headless exits 0 (D3D11 WARP, no GPU needed).
  5. cook.exe successfully cooks the vertical slice sample project.
  6. engine_sandbox.exe --validate-project loads the cooked AssetDB.

============================================================================

### D3D11 vs Vulkan in CI

**Source:** [`.github/workflows/build-windows.yml`](.github/workflows/build-windows.yml#L18) (line 18)

============================================================================
The primary build job uses D3D11 (default backend) because:
  • d3d11.lib ships with the Windows SDK — no external download needed.
  • D3D11 WARP (software rasteriser) is bundled with Windows, so
    engine_sandbox.exe --headless works on any Windows runner regardless
    of whether a physical GPU or Vulkan driver is present.
  • GT610-era hardware (target baseline) supports D3D11 Feature Level 11_0.

A separate optional job (build-windows-vulkan) validates the Vulkan backend
when a Vulkan SDK is available.  It is allowed to fail initially to avoid
blocking D3D11 CI on Vulkan SDK availability.
============================================================================

### Least-Privilege Permissions

**Source:** [`.github/workflows/build-windows.yml`](.github/workflows/build-windows.yml#L59) (line 59)

GitHub Actions tokens default to write access on the repository.
Restricting to contents: read follows the principle of least privilege:
a compromised action cannot push malicious commits to the repository.
-------------------------------------------------------------------------
permissions:
contents: read

### Ninja on Windows does not automatically pick MSVC.

**Source:** [`.github/workflows/build-windows.yml`](.github/workflows/build-windows.yml#L77) (line 77)

Without vcvars, CMake may detect a GNU toolchain (e.g. MinGW), which
fails to link Windows SDK libraries such as xaudio2.lib.
This step initialises cl.exe/link.exe + Windows SDK paths explicitly.
- name: Setup MSVC developer command prompt
uses: ilammy/msvc-dev-cmd@v1
with:
arch: x64

### CMake Presets in CI

**Source:** [`.github/workflows/build-windows.yml`](.github/workflows/build-windows.yml#L89) (line 89)

cmake --preset windows-debug-engine-only uses CMakePresets.json:
  • Generator: Visual Studio 17 2022 (MSVC x64)
  • ENGINE_ENABLE_D3D11=ON   (default, GT610-compatible)
  • ENGINE_ENABLE_VULKAN=OFF (Vulkan SDK not installed on runner)
  • BUILD_EDITOR=OFF         (Qt not installed on CI runner)

No Vulkan SDK step is needed: d3d11.lib ships with the Windows SDK
that is part of every Visual Studio installation on the runner.
-----------------------------------------------------------------------
- name: Configure CMake (D3D11 engine-only, portable Ninja preset)
run: cmake --preset windows-ninja-debug-engine-only

### D3D11 WARP in Headless CI

**Source:** [`.github/workflows/build-windows.yml`](.github/workflows/build-windows.yml#L125) (line 125)

D3D11 WARP is Microsoft's CPU software rasteriser bundled with every
Windows installation.  When --headless is passed, D3D11Renderer uses
D3D_DRIVER_TYPE_WARP (no GPU or GPU driver needed) and skips swap chain
creation.  This makes the headless check reliable on any Windows runner.
Expected output: "[PASS] D3D11 device initialised..." followed by exit 0.
-----------------------------------------------------------------------
- name: Run headless validation (M0 — D3D11 WARP)
run: .\build\windows-ninja-debug-engine-only\engine_sandbox.exe --headless
shell: cmd

### Skinned mesh headless validation.

**Source:** [`.github/workflows/build-windows.yml`](.github/workflows/build-windows.yml#L146) (line 146)

Loads the "skinned_mesh" scene which compiles skinned_mesh.vs.hlsl and
skinned_mesh.ps.hlsl, creates the GpuSkinningBuffer constant buffer,
uploads 2 joint matrices, and calls DrawIndexed(24).
All of this executes on the WARP software rasteriser — no GPU needed.
Expected output: "[PASS] skinned_mesh scene pipeline OK (WARP headless)."
-----------------------------------------------------------------------
- name: Run headless validation (M4b — skinned_mesh scene)
run: .\build\windows-ninja-debug-engine-only\engine_sandbox.exe --headless --scene skinned_mesh
shell: cmd

### M5 Physics CI Job

**Source:** [`.github/workflows/build-windows.yml`](.github/workflows/build-windows.yml#L174) (line 174)

============================================================================
This job validates the Jolt Physics integration (M5).  It:
  1. Installs joltphysics via the system vcpkg at C:\vcpkg (classic mode).
  2. Configures engine_sandbox with the windows-ninja-debug-physics preset
     (ENGINE_ENABLE_PHYSICS=ON, D3D11 only, no Vulkan SDK needed).
  3. Builds engine_sandbox.
  4. Runs the M5 headless physics acceptance test:
        engine_sandbox.exe --headless --scene physics_test
     Three CPU-only tests are run:
       Test 1 — Drop sphere: gravity simulation accuracy.
       Test 2 — Character step-up: CharacterController ledge traverse.
       Test 3 — Raycast: terrain query distance accuracy.

continue-on-error: false — Physics acceptance tests are a hard M5 CI gate.
The vcpkg install uses classic mode (see below) to avoid manifest-mode
failures on other dependencies such as imgui[docking].
============================================================================
build-windows-physics:
name: Build Windows + Jolt Physics (M5)
runs-on: windows-latest
continue-on-error: false  # TEACHING NOTE — hard M5 CI gate

### Classic-mode vcpkg install (physics job only)

**Source:** [`.github/workflows/build-windows.yml`](.github/workflows/build-windows.yml#L213) (line 213)

-----------------------------------------------------------------------
The project's vcpkg.json lists ALL engine dependencies, including
imgui[docking,dx11-binding,win32-binding] for the editor.  Running
`vcpkg install` from the workspace in "manifest mode" would try to
install every entry — and a specific imgui snapshot on the CI runner
may not carry the docking feature, causing the install to fail even
though this build doesn't need imgui at all.

Fix: run the install from a neutral temp directory (no vcpkg.json)
so vcpkg falls back to "classic mode", where individual packages are
accepted.  We use the system-wide vcpkg at C:\vcpkg (always present
on windows-latest runners).  Packages land in C:\vcpkg\installed\ and
are found by CMake via the C:\vcpkg toolchain file below.

Cache key: package name + OS (joltphysics version rarely changes).
-----------------------------------------------------------------------
- name: Cache vcpkg packages
uses: actions/cache@v4
with:
path: C:\vcpkg\installed
key: vcpkg-joltphysics-${{ runner.os }}-x64

### VCPKG_MANIFEST_INSTALL=OFF

**Source:** [`.github/workflows/build-windows.yml`](.github/workflows/build-windows.yml#L247) (line 247)

The vcpkg CMake toolchain detects vcpkg.json in the project root and
would automatically re-run `vcpkg install` in manifest mode during
cmake configure.  This fails because imgui@1.92.7 on the CI runner
does not carry the "docking" feature required by vcpkg.json, even
though this build doesn't use imgui at all.

Setting VCPKG_MANIFEST_INSTALL=OFF tells the toolchain to skip the
auto-install.  CMake then uses the packages already installed in the
previous classic-mode step (C:\vcpkg\installed\x64-windows\).
-----------------------------------------------------------------------
- name: Configure CMake (D3D11 + Jolt Physics)
run: >
cmake --preset windows-ninja-debug-physics
-DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
-DVCPKG_MANIFEST_INSTALL=OFF
shell: pwsh

### Physics is CPU-only

**Source:** [`.github/workflows/build-windows.yml`](.github/workflows/build-windows.yml#L271) (line 271)

Unlike M3 (textured quad) and M4b (GPU skinning), the physics_test
scene does not touch the D3D11 renderer at all.  It initialises
Jolt Physics, runs three simulations, and exits.  This means it
works on any runner regardless of GPU driver availability.
-----------------------------------------------------------------------
- name: Run physics acceptance tests (M5)
run: .\build\windows-ninja-debug-physics\engine_sandbox.exe --headless --scene physics_test
shell: cmd

### Optional Vulkan CI Job

**Source:** [`.github/workflows/build-windows.yml`](.github/workflows/build-windows.yml#L284) (line 284)

This job validates the Vulkan backend when a Vulkan SDK is available.
It is separated from the primary job so:
  1. Vulkan SDK availability does not block the D3D11 main CI path.
  2. Future hardware runners (self-hosted GPU) can run the full Vulkan test.

continue-on-error: true — Vulkan job failures do not fail the overall CI.
As the project matures (self-hosted GPU runner added), this can be changed
to continue-on-error: false to make Vulkan a hard requirement.
--------------------------------------------------------------------------
build-windows-vulkan:
name: Build Windows Vulkan (optional, MSVC x64)
runs-on: windows-latest
continue-on-error: true

### Keep toolchain consistent with primary Windows job.

**Source:** [`.github/workflows/build-windows.yml`](.github/workflows/build-windows.yml#L309) (line 309)

The Vulkan job also compiles Audio/XAudio2 code paths, so using MSVC
avoids GNU-style -lxaudio2 lookup failures on windows-latest runners.
- name: Setup MSVC developer command prompt
uses: ilammy/msvc-dev-cmd@v1
with:
arch: x64

### Why cache the Vulkan SDK?

**Source:** [`.github/workflows/build-windows.yml`](.github/workflows/build-windows.yml#L320) (line 320)

The Vulkan SDK is ~500 MB.  Without caching, every CI run would
re-download it.  vulkan-use-cache: true stores the download in
GitHub's action cache, keyed by the version string.
-----------------------------------------------------------------------
- name: Install Vulkan SDK
uses: humbletim/setup-vulkan-sdk@v1.2.1
with:
vulkan-query-version: 1.3.250.0
vulkan-components: Vulkan-Headers, Vulkan-Loader
vulkan-use-cache: true

### Vulkan Headless Limitation

**Source:** [`.github/workflows/build-windows.yml`](.github/workflows/build-windows.yml#L342) (line 342)

GitHub-hosted runners install the Vulkan loader but NOT a software ICD
(SwiftShader/lavapipe for Windows).  Running --renderer vulkan --headless
may fail with VK_ERROR_INCOMPATIBLE_DRIVER on runners without a real GPU.
This is expected and why this job has continue-on-error: true.
On a self-hosted GPU runner this step will succeed.
-----------------------------------------------------------------------
- name: Run Vulkan headless validation (optional)
run: .\build\windows-ninja-debug-vulkan\engine_sandbox.exe --renderer vulkan --headless
shell: cmd
continue-on-error: true

### Contract Tests in CI

**Source:** [`.github/workflows/contract-tests.yml`](.github/workflows/contract-tests.yml#L5) (line 5)

======================================
A "contract test" verifies the OUTPUT of a pipeline against an agreed
specification (the "contract").  Here we:

  1. Install the Python authoring tools (audio_authoring + anim_authoring).
  2. Run cook_assets.py on the vertical slice sample project.
  3. Run pytest against tests/cook/ — which validates:
       - The cook exits with code 0.
       - AssetRegistry.json is written and schema-valid.
       - Every cooked path in the registry exists on disk.
       - The mandatory MainTown scene is present.
  4. Run tools/validate-assets.py to lint the resulting registry.

This workflow runs on every push/PR that touches the cook pipeline or
the sample project, keeping the M2 deliverable green at all times.
============================================================================

### Principle of Least Privilege

**Source:** [`.github/workflows/contract-tests.yml`](.github/workflows/contract-tests.yml#L43) (line 43)

This workflow only reads repository content; no write access needed.
permissions:
contents: read

### Python Setup

**Source:** [`.github/workflows/contract-tests.yml`](.github/workflows/contract-tests.yml#L61) (line 61)

We pin to Python 3.11 for reproducibility.  The tools require 3.9+.
-----------------------------------------------------------------------
- name: Set up Python 3.11
uses: actions/setup-python@v5
with:
python-version: '3.11'

### Continuous Integration for Assets

**Source:** [`.github/workflows/validate-assets.yml`](.github/workflows/validate-assets.yml#L5) (line 5)

---------------------------------------------------
In AAA studios the asset pipeline is a first-class citizen of CI.  Every
texture, audio clip, or tilemap that lands in the repository must pass
validation before it can be imported by the engine.  This workflow is the
lightweight educational equivalent of that gate.

Trigger: any push or pull-request that touches
  • assets/**          (manifest files or schema)
  • tools/validate-assets.py  (the validator itself)

On success: job passes, PR can be merged.
On failure: job fails with per-asset error messages so the author knows
            exactly which field is broken and why.
============================================================================

### Principle of Least Privilege

**Source:** [`.github/workflows/validate-assets.yml`](.github/workflows/validate-assets.yml#L37) (line 37)

Grant only the minimum permissions required.  This workflow only reads
repository contents, so no other GITHUB_TOKEN scopes are needed.
permissions:
contents: read

---

## editor/CMakeLists.txt

### Dear ImGui vs Qt Widgets

**Source:** [`editor/CMakeLists.txt`](editor/CMakeLists.txt#L5) (line 5)

------------------------------------------
Qt Widgets uses a RETAINED-MODE UI model:
  * Widget objects (QMainWindow, QTreeView ...) persist between frames.
  * They are updated via method calls (setText, setModel ...).
  * Qt requires a multi-hundred-MB installation and LGPL/commercial licence.

Dear ImGui uses an IMMEDIATE-MODE UI model:
  * Every frame you call ImGui:: functions that BOTH define the UI AND
    query input in one step.  There are no persistent widget objects.
  * ImGui::Button("Save") returns true the frame it was clicked.
  * Licence: MIT -- zero cost, no LGPL viral clauses, no install required.
  * Industry usage: Unreal Engine tools, many AAA engine editors,
    Valve/Epic internal tools.

This migration means:
  Before: find_package(Qt6 ...) -- requires Qt 6 installed + CMAKE_PREFIX_PATH
  After:  find_package(imgui ...) -- provided by vcpkg, zero extra install

### vcpkg Feature Flags

**Source:** [`editor/CMakeLists.txt`](editor/CMakeLists.txt#L24) (line 24)

--------------------------------------
vcpkg supports "features" -- optional compile-time variants of a package.
For imgui we enable three features in vcpkg.json:
  docking        -- enables the dockable-window API (ImGui::DockSpace etc.)
  dx11-binding   -- adds imgui_impl_dx11.h/.cpp for D3D11 rendering
  win32-binding  -- adds imgui_impl_win32.h/.cpp for Win32 window input
vcpkg packages these as additional headers/sources exposed via the same
imgui::imgui imported target.

### find_package with vcpkg-installed libraries

**Source:** [`editor/CMakeLists.txt`](editor/CMakeLists.txt#L47) (line 47)

When CMake is configured with the vcpkg toolchain file
(-DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake),
find_package() automatically finds packages installed by vcpkg.
Each package exposes CMake "imported targets" that carry include paths
and linker flags automatically.
  imgui::imgui               -- Dear ImGui core + D3D11 + Win32 backends
  nlohmann_json::nlohmann_json -- header-only JSON library (already in vcpkg.json)
find_package(imgui         CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

### Linking D3D11 + DXGI

**Source:** [`editor/CMakeLists.txt`](editor/CMakeLists.txt#L92) (line 92)

d3d11.lib  -- Direct3D 11 device, context, swapchain.  Ships with the
              Windows SDK; no extra SDK install beyond Visual Studio.
dxgi.lib   -- DirectX Graphics Infrastructure: swap chain, adapters.
Both are OS components (no DLL to ship alongside the editor).
ImGui's D3D11 backend calls into these via device/context pointers
we create in main.cpp.
target_link_libraries(creation-suite-editor PRIVATE
imgui::imgui
nlohmann_json::nlohmann_json
d3d11
dxgi
)

### UNICODE, WIN32_LEAN_AND_MEAN, NOMINMAX

**Source:** [`editor/CMakeLists.txt`](editor/CMakeLists.txt#L110) (line 110)

UNICODE/_UNICODE  : Win32 API calls default to wide-char (wchar_t) variants.
WIN32_LEAN_AND_MEAN : excludes rarely-used headers from <windows.h>,
  dramatically reducing compile time.
NOMINMAX : prevents windows.h from defining min/max macros that clash
  with std::min / std::max.
target_compile_definitions(creation-suite-editor PRIVATE
UNICODE
_UNICODE
WIN32_LEAN_AND_MEAN
NOMINMAX
)
endif()

---

## editor/src

### ImGui::Begin with a persistent panel name

**Source:** [`editor/src/ContentBrowserPanel.cpp`](editor/src/ContentBrowserPanel.cpp#L49) (line 49)

ImGui identifies windows by their string label.  Using the same label
every frame re-opens the same window rather than creating a new one.
ImGuiWindowFlags_NoCollapse prevents the user from collapsing the panel
(which would hide the file tree entirely with no way to open it again
 without the ini file).
ImGui::Begin("Content Browser", nullptr, ImGuiWindowFlags_NoCollapse);

### TextDisabled

**Source:** [`editor/src/ContentBrowserPanel.cpp`](editor/src/ContentBrowserPanel.cpp#L59) (line 59)

ImGui::TextDisabled renders text in the theme's disabled colour
(grey in the dark theme).  Good for placeholder / hint text.
ImGui::TextDisabled("No project open.");
ImGui::TextDisabled("Use File > Open Project to select a folder.");
ImGui::End();
return;
}

### Recursive ImGui tree with std::filesystem

**Source:** [`editor/src/ContentBrowserPanel.cpp`](editor/src/ContentBrowserPanel.cpp#L92) (line 92)

RenderDirectory() walks one level of a directory.  For each sub-directory
it calls itself recursively (creates a nested TreeNode).
For each file it creates a leaf node with ImGuiTreeNodeFlags_Leaf.

std::filesystem::directory_iterator gives one entry per file/dir.
We sort entries so directories come before files and both sets are
sorted alphabetically -- matching the behaviour of Qt's QFileSystemModel
with sorting enabled.
void ContentBrowserPanel::RenderDirectory(const fs::path& dir, bool forceExpand)
{
std::error_code ec;

### Leaf nodes and selection highlight

**Source:** [`editor/src/ContentBrowserPanel.cpp`](editor/src/ContentBrowserPanel.cpp#L145) (line 145)

ImGuiTreeNodeFlags_Leaf creates a node with no expand arrow.
ImGuiTreeNodeFlags_SpanAvailWidth makes the clickable area fill the
full panel width (not just the text width) -- better UX.
When the user double-clicks, we call the registered callback.
ImGuiTreeNodeFlags leafFlags =
ImGuiTreeNodeFlags_Leaf       |
ImGuiTreeNodeFlags_NoTreePushOnOpen |
ImGuiTreeNodeFlags_SpanAvailWidth;

### std::filesystem vs QFileSystemModel

**Source:** [`editor/src/ContentBrowserPanel.hpp`](editor/src/ContentBrowserPanel.hpp#L6) (line 6)

=============================================================================
The Qt version used QFileSystemModel paired with a QTreeView widget.
QFileSystemModel:
  - Monitors the directory asynchronously (background thread).
  - Emits dataChanged/rowsInserted signals when the file system changes.
  - Qt owns the model; the view subscribes to it.

Without Qt we use C++17 std::filesystem:
  - std::filesystem::directory_iterator walks directory entries.
  - We build our own tree every frame (or on demand) and render it with
    ImGui::TreeNode / ImGui::TreePop.

Trade-off: QFileSystemModel provides live background updates; our approach
refreshes manually (press R or call Refresh()).  For a teaching editor this
is simpler and has no hidden threads.

=============================================================================

### ImGui Tree Nodes

**Source:** [`editor/src/ContentBrowserPanel.hpp`](editor/src/ContentBrowserPanel.hpp#L24) (line 24)

=============================================================================
ImGui has no "tree widget" -- it builds trees from individual calls:

  if (ImGui::TreeNode("Folder"))        // draws an expandable node
  {
      ImGui::TreeNode("child file");    // leaf inside the folder
      ImGui::TreePop();                 // MUST be called if TreeNode() returned true
  }

TreeNodeEx() extends TreeNode() with ImGuiTreeNodeFlags for leaf styling,
default-open, selected highlight, and more.

=============================================================================

### DockSpaceOverViewport

**Source:** [`editor/src/EditorApp.cpp`](editor/src/EditorApp.cpp#L35) (line 35)

ImGui::DockSpaceOverViewport() creates a DockSpace that fills the entire
main viewport.  All other ImGui windows can be docked into this space.

ImGuiDockNodeFlags_PassthruCentralNode lets the game/editor background
(the D3D11 clear colour) show through the undocked central area.
Without this flag the dockspace paints an opaque background over the
central region even when no window is docked there.
ImGui::DockSpaceOverViewport(
ImGui::GetMainViewport(),
ImGuiDockNodeFlags_PassthruCentralNode
);

### ImGui Modal Popups

**Source:** [`editor/src/EditorApp.cpp`](editor/src/EditorApp.cpp#L59) (line 59)

OpenPopup() marks a popup as "open"; BeginPopupModal() renders it.
The popup blocks interaction with windows behind it (modal behaviour).
EndPopup() must always be called if BeginPopupModal returned true.
if (ImGui::BeginPopupModal("About##popup", &m_showAbout,
ImGuiWindowFlags_AlwaysAutoResize))
{
ImGui::TextUnformatted("Creation Suite Editor  v1.0");
ImGui::Separator();
ImGui::TextUnformatted("Part of the Game Engine for Teaching monorepo.");
ImGui::TextUnformatted("Dear ImGui (MIT) for UI.");
ImGui::TextUnformatted("D3D11 (Windows SDK) for rendering.");
ImGui::TextUnformatted("nlohmann-json (MIT) for scene files.");
ImGui::Spacing();
if (ImGui::Button("Close", ImVec2(120, 0)))
{
m_showAbout = false;
ImGui::CloseCurrentPopup();
}
ImGui::EndPopup();
}

### ImGui Menu Bar

**Source:** [`editor/src/EditorApp.cpp`](editor/src/EditorApp.cpp#L98) (line 98)

ImGui::BeginMainMenuBar() / EndMainMenuBar() create a menu bar anchored to
the top of the main viewport (not inside any ImGui window).
ImGui::BeginMenu("Label") opens a submenu; it returns true only while the
menu is open, so the code inside the if() block runs only that frame.
ImGui::MenuItem("Label", "Shortcut", &checked) handles checkboxes too.
void EditorApp::RenderMenuBar()
{
if (!ImGui::BeginMainMenuBar())
return;

### Posting WM_QUIT from within ImGui

**Source:** [`editor/src/EditorApp.cpp`](editor/src/EditorApp.cpp#L164) (line 164)

PostQuitMessage(0) posts a WM_QUIT to the Win32 message queue.
The main loop detects this and sets done = true, triggering cleanup.
PostQuitMessage(0);
}

### ShellExecuteW to run the cook script

**Source:** [`editor/src/EditorApp.cpp`](editor/src/EditorApp.cpp#L185) (line 185)

ShellExecuteW launches an external process using the Windows
shell.  "open" + a .py file invokes the system Python interpreter.
For production you would use CreateProcessW to capture stdout/stderr.
std::wstring wScript(m_projectPath.begin(), m_projectPath.end());
wScript += L"\\cook_assets.py";
ShellExecuteW(nullptr, L"open", L"python",
(L"\"" + wScript + L"\"").c_str(),
nullptr, SW_SHOWNORMAL);
m_statusMessage = "Cook started...";
m_statusTimer   = 4.f;
}
}
ImGui::EndMenu();
}

### Simulating a Status Bar with ImGui

**Source:** [`editor/src/EditorApp.cpp`](editor/src/EditorApp.cpp#L216) (line 216)

ImGui has no built-in "status bar" widget.  We simulate one by creating
a small window pinned to the bottom of the viewport with no decorations.
ImGuiWindowFlags_NoDecoration removes title bar, scrollbar, resize grip.
ImGuiWindowFlags_NoMove prevents the user from dragging it away.
void EditorApp::RenderStatusBar()
{
const ImGuiViewport* viewport = ImGui::GetMainViewport();
constexpr float barHeight     = 22.f;

### IFileOpenDialog (modern Windows folder picker)

**Source:** [`editor/src/EditorApp.cpp`](editor/src/EditorApp.cpp#L258) (line 258)

The classic SHBrowseForFolderW dialog is old (Windows 3.1 era).
The modern alternative is IFileOpenDialog with FOS_PICKFOLDERS set --
it uses the Vista-style folder picker with favourites, breadcrumb nav, etc.
This is the same approach used in modern Win32 applications.
std::string EditorApp::PickFolder(const char* /*title*/)
{
std::string result;
IFileOpenDialog* pFolderDialog = nullptr;

### GetSaveFileName (classic Win32 save dialog)

**Source:** [`editor/src/EditorApp.cpp`](editor/src/EditorApp.cpp#L307) (line 307)

GetSaveFileNameW shows the standard "Save As" dialog. The filter string
uses pairs of "Description\0*.ext\0" terminated by an extra \0.
lpstrDefExt specifies the extension appended if the user omits it.
std::string EditorApp::PickSaveFile(const char* /*title*/, const char* filter,
const char* defaultExt)
{
Convert narrow filter to wide
Count filter length including all embedded NULs
size_t filterLen = 0;
const char* p    = filter;
while (*p || *(p + 1)) { ++filterLen; ++p; }
filterLen += 2;  // trailing double-NUL

### ImGui DockSpace Architecture

**Source:** [`editor/src/EditorApp.hpp`](editor/src/EditorApp.hpp#L6) (line 6)

=============================================================================
Dear ImGui's docking system lets you create a layout of dockable panels,
exactly like Unreal Editor, Unity Editor, or any professional tool.

The central concept is a DockSpace:
  ImGuiID dockId = ImGui::DockSpaceOverViewport();

A DockSpace is a region of the window into which ImGui "windows" can be
docked (pinned) or float freely.  The first time the editor runs, panels
start floating.  The user drags them to dock positions; ImGui saves the
layout to creation-suite-editor.ini automatically.

Compare to the Qt retained-mode approach where dock widgets were explicit
QDockWidget objects added to a QMainWindow via addDockWidget().
With ImGui there is no "dock widget" object -- any ImGui::Begin("Name")
window can be docked just by dragging it.

=============================================================================

### Application State in Immediate-Mode UI

**Source:** [`editor/src/EditorApp.hpp`](editor/src/EditorApp.hpp#L25) (line 25)

=============================================================================
In Qt, application state (current project path, status message ...) was
stored as member variables of QMainWindow and updated via signals/slots.

In ImGui the same state lives as member variables of EditorApp.
There are no signals or slots -- each frame EditorApp::Render() reads the
state and decides what to draw.  When the user clicks a button, the state
is updated immediately (same frame, same call stack).

This makes control flow much easier to follow for students:
  if (ImGui::MenuItem("Open Project..."))
  {
      // open file dialog -- happens RIGHT HERE, no callback needed
      ...
      m_projectPath = result;
  }

=============================================================================

### Native file dialogs without Qt

**Source:** [`editor/src/EditorApp.hpp`](editor/src/EditorApp.hpp#L81) (line 81)

Qt provides QFileDialog::getExistingDirectory() etc.
Without Qt we use the Win32 SHBrowseForFolderW API (folder picker) or
IFileOpenDialog COM interface (modern Windows file dialog).
These helpers wrap those calls and return UTF-8 std::string results.
std::string PickFolder(const char* title);
std::string PickSaveFile(const char* title, const char* filter,
const char* defaultExt);

### What this file teaches

**Source:** [`editor/src/SceneEditorPanel.cpp`](editor/src/SceneEditorPanel.cpp#L6) (line 6)

=============================================================================
 1. ImGui child windows + DrawList for custom 2D rendering
 2. Immediate-mode mouse picking (click → entity selection)
 3. Immediate-mode popup input (replacing QInputDialog)
 4. JSON scene I/O with nlohmann-json (replacing Qt's QJsonDocument)
 5. Guid.hpp for UUID generation (replacing QUuid)

=============================================================================

### ImGui window with no close button

**Source:** [`editor/src/SceneEditorPanel.cpp`](editor/src/SceneEditorPanel.cpp#L40) (line 40)

Passing nullptr as p_open means there is no close (X) button.
ImGuiWindowFlags_NoCollapse keeps the panel always expanded.
ImGui::Begin("Scene Editor", nullptr, ImGuiWindowFlags_NoCollapse);

### ImGui::Columns (simple 2-column layout)

**Source:** [`editor/src/SceneEditorPanel.cpp`](editor/src/SceneEditorPanel.cpp#L57) (line 57)

Columns() divides the current window into N columns separated by a
draggable divider.  Column 0 gets the canvas; column 1 the entity list.
ImGui::Columns(2, "scene_columns", true);
ImGui::SetColumnWidth(0, ImGui::GetContentRegionAvail().x * 0.75f);

### ImGui::BeginChild for a scrollable/bordered sub-region

**Source:** [`editor/src/SceneEditorPanel.cpp`](editor/src/SceneEditorPanel.cpp#L82) (line 82)

BeginChild("id", size, border, flags) creates a clipped sub-region.
  size = (0,0) means "fill remaining content area".
  ImGuiChildFlags_Border draws a 1-pixel border around the canvas.
Everything drawn inside is clipped to this region.
ImVec2 canvasSize = ImGui::GetContentRegionAvail();
canvasSize.y -= 4;  // small margin at the bottom

### ImDrawList

**Source:** [`editor/src/SceneEditorPanel.cpp`](editor/src/SceneEditorPanel.cpp#L98) (line 98)

ImGui::GetWindowDrawList() returns the draw list of the current window.
Commands added to a draw list are rendered in order (painter's algorithm).
All coordinates are in *screen* space (pixels from top-left of the OS window),
so we offset by 'origin' to convert from canvas-local to screen coordinates.
ImDrawList* dl = ImGui::GetWindowDrawList();

### IsWindowHovered + GetMousePos

**Source:** [`editor/src/SceneEditorPanel.cpp`](editor/src/SceneEditorPanel.cpp#L162) (line 162)

IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) returns
true when the canvas child window is hovered, even if another widget is
active (e.g., a drag is in progress).
GetMousePos() returns screen-space coordinates; subtract origin to get
canvas-local coordinates.
const bool hovered = ImGui::IsWindowHovered();
const ImVec2 mousePos = ImGui::GetMousePos();
const float  mx = mousePos.x - origin.x;
const float  my = mousePos.y - origin.y;

### IsKeyPressed vs IsKeyDown

**Source:** [`editor/src/SceneEditorPanel.cpp`](editor/src/SceneEditorPanel.cpp#L201) (line 201)

IsKeyPressed() returns true ONCE on the frame the key goes down.
IsKeyDown()    returns true every frame while the key is held.
For a destructive action like Delete we want IsKeyPressed().
if (hovered && ImGui::IsKeyPressed(ImGuiKey_Delete) && m_selectedIdx >= 0)
{
m_entities.erase(m_entities.begin() + m_selectedIdx);
m_selectedIdx = -1;
}

### Selectable

**Source:** [`editor/src/SceneEditorPanel.cpp`](editor/src/SceneEditorPanel.cpp#L230) (line 230)

ImGui::Selectable renders a clickable row that highlights on hover
and can show as "selected" when the second argument is true.
It returns true the frame it is clicked.
if (ImGui::Selectable(ent.name.c_str(), selected))
m_selectedIdx = i;

### BeginPopupModal

**Source:** [`editor/src/SceneEditorPanel.cpp`](editor/src/SceneEditorPanel.cpp#L260) (line 260)

BeginPopupModal renders a centered modal dialog.
ImGui::InputText writes into m_nameBuffer (a C-style char array).
ImGuiInputTextFlags_EnterReturnsTrue makes Enter commit the input.
bool popupOpen = true;
if (ImGui::BeginPopupModal("New Entity##popup", &popupOpen,
ImGuiWindowFlags_AlwaysAutoResize))
{
ImGui::TextUnformatted("Entity name:");
ImGui::SetNextItemWidth(240.f);

### Guid::New() for UUID generation

**Source:** [`editor/src/SceneEditorPanel.cpp`](editor/src/SceneEditorPanel.cpp#L287) (line 287)

Guid::New() from shared/runtime/Guid.hpp generates an RFC 4122
v4 UUID -- the same pattern used by the cook pipeline and asset
registry.  This replaces QUuid::createUuid() from the Qt version.
ent.id   = Guid::New().ToString();
ent.name = name;
ent.x    = m_pendingClickX;
ent.y    = m_pendingClickY;
m_entities.push_back(ent);
m_selectedIdx = static_cast<int>(m_entities.size()) - 1;
}
ImGui::CloseCurrentPopup();
}

### nlohmann-json for scene save

**Source:** [`editor/src/SceneEditorPanel.cpp`](editor/src/SceneEditorPanel.cpp#L313) (line 313)

nlohmann/json.hpp provides a single-header JSON library (MIT licence).
json j = { {"key", value}, ... } builds a JSON object with initialiser lists.
j.dump(4) returns a pretty-printed string with 4-space indentation.
This replaces Qt's QJsonObject / QJsonDocument API.
bool SceneEditorPanel::SaveScene(const std::string& filePath) const
{
Build ISO-8601 timestamp (replaces QDateTime::currentDateTimeUtc())
auto now = std::chrono::system_clock::now();
std::time_t t = std::chrono::system_clock::to_time_t(now);
std::ostringstream tsStream;
tsStream << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");

### nlohmann-json for scene load

**Source:** [`editor/src/SceneEditorPanel.cpp`](editor/src/SceneEditorPanel.cpp#L360) (line 360)

json::parse(stream) reads from any std::istream.
j.value("key", default) safely reads a key with a fallback if missing.
j.contains("key") tests whether a key exists before accessing it.
This replaces QJsonDocument::fromJson() + QJsonObject::value().
bool SceneEditorPanel::LoadScene(const std::string& filePath)
{
std::ifstream ifs(filePath);
if (!ifs) return false;

### ImGui Custom Drawing with DrawList

**Source:** [`editor/src/SceneEditorPanel.hpp`](editor/src/SceneEditorPanel.hpp#L6) (line 6)

=============================================================================
The Qt version painted the scene by overriding QWidget::paintEvent() and
using QPainter:
  painter.drawRect(...)
  painter.drawText(...)

In Dear ImGui there is no paintEvent -- instead, every panel gets access to
an ImDrawList (a list of draw commands) via:
  ImDrawList* drawList = ImGui::GetWindowDrawList();

You call AddRect(), AddLine(), AddText() etc. on the draw list directly.
Draw commands are batched and executed by the D3D11 backend at Render() time.

The canvas is created with ImGui::BeginChild() -- a scrollable sub-region
within a parent ImGui window.  This is the standard pattern for any
custom-rendered viewport (scene editor, animation timeline, node graph ...).

=============================================================================

### ImGui Popup for Text Input

**Source:** [`editor/src/SceneEditorPanel.hpp`](editor/src/SceneEditorPanel.hpp#L25) (line 25)

=============================================================================
The Qt version used QInputDialog::getText() which blocks the event loop
until the user clicks OK or Cancel.

ImGui is single-threaded and non-blocking.  To get text input:
  1. Set a flag when input is needed (m_pendingEntityName = true).
  2. On the NEXT frame, call ImGui::OpenPopup() + ImGui::BeginPopup().
  3. Inside the popup, call ImGui::InputText() and wait for the user.
  4. On confirmation, use the entered text and close the popup.

This is the immediate-mode equivalent of a blocking dialog.

=============================================================================

### JSON Scene Format

**Source:** [`editor/src/SceneEditorPanel.hpp`](editor/src/SceneEditorPanel.hpp#L39) (line 39)

=============================================================================
The scene is saved as a JSON file following shared/schemas/scene.schema.json.
We use nlohmann-json (already in vcpkg.json) instead of Qt's QJsonDocument.
The format is identical -- the engine reads the same .scene.json files
regardless of whether they were saved by the Qt or the ImGui editor.

=============================================================================

### Plain data struct (no Qt types)

**Source:** [`editor/src/SceneEditorPanel.hpp`](editor/src/SceneEditorPanel.hpp#L57) (line 57)

The Qt version used QString for id and name.  Now we use std::string --
C++ standard library types require no external framework.

### Immediate-Mode GUI (ImGui) vs Retained-Mode GUI

**Source:** [`editor/src/main.cpp`](editor/src/main.cpp#L6) (line 6)

=============================================================================
RETAINED-MODE (Qt, WinForms, GTK ...):
  - Widget objects persist between frames.
  - You PUSH data into them (setText, setModel ...) and register callbacks.
  - The framework decides when to redraw.
  - Typically requires a large external framework installation.

IMMEDIATE-MODE (Dear ImGui):
  - No persistent widget objects -- you RE-DECLARE the entire UI every frame.
  - ImGui::Button("Save") returns true the frame it was clicked.
    That single call handles layout, rendering, AND input query.
  - The same model is used in Unreal Engine's Slate debug overlays,
    Unity's IMGUI system, Valve's in-engine tools, and many AAA studios.
  - Licence: MIT -- zero cost, no LGPL, no install required.

=============================================================================

### Win32 + D3D11 Editor Bootstrap

**Source:** [`editor/src/main.cpp`](editor/src/main.cpp#L23) (line 23)

=============================================================================
The editor is a standalone Win32 + D3D11 executable -- the same low-level
APIs used by the engine itself.  Steps:

  1. RegisterClassExW + CreateWindowExW  -> HWND
  2. D3D11CreateDeviceAndSwapChain       -> ID3D11Device + IDXGISwapChain
  3. ImGui_ImplWin32_Init + ImGui_ImplDX11_Init
  4. Main message/render loop (see below)
  5. Reverse-order cleanup

D3D11 is chosen because it ships with Windows and requires no extra SDK
install (same reason D3D11 is the engine's default renderer).

=============================================================================

### Module-level (global) objects for D3D11

**Source:** [`editor/src/main.cpp`](editor/src/main.cpp#L55) (line 55)

We store D3D11 objects at module scope so both the WndProc and the main
loop can access them without threading overhead.  In a larger engine these
would live inside a Renderer class; for a single-file bootstrap, module
scope is clear and self-documenting.
static ID3D11Device*            g_pd3dDevice         = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext   = nullptr;
static IDXGISwapChain*          g_pSwapChain          = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

### Window Procedure (WndProc)

**Source:** [`editor/src/main.cpp`](editor/src/main.cpp#L80) (line 80)

Every Win32 window requires a "window procedure" callback that processes
messages sent by the OS (WM_SIZE, WM_DESTROY, mouse events, key events ...).
Messages are dispatched by DispatchMessageW in the main loop.

The ImGui Win32 backend provides ImGui_ImplWin32_WndProcHandler -- we call
it first so ImGui can intercept mouse/keyboard input before we handle it.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
return true;  // ImGui consumed the message

### Swap-chain resize on WM_SIZE

**Source:** [`editor/src/main.cpp`](editor/src/main.cpp#L95) (line 95)

When the window is resized the swap chain buffers become stale.
We must release the old render target view, resize the swap chain,
then re-create the render target view with the new dimensions.
if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
{
CleanupRenderTarget();
g_pSwapChain->ResizeBuffers(
0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
CreateRenderTarget();
}
return 0;

### WNDCLASSEXW

**Source:** [`editor/src/main.cpp`](editor/src/main.cpp#L131) (line 131)

WNDCLASSEXW describes the properties of a window class (shared template
from which individual windows are created).
  hCursor  -- the mouse cursor shown over this window.
  hbrBackground -- background brush; NULL_BRUSH = we clear manually (D3D11).
  lpfnWndProc -- pointer to our WndProc message handler.
WNDCLASSEXW wc = {};
wc.cbSize        = sizeof(WNDCLASSEXW);
wc.style         = CS_CLASSDC;
wc.lpfnWndProc   = WndProc;
wc.hInstance     = hInstance;
wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
wc.hbrBackground = nullptr;  // D3D11 clears the background each frame
wc.lpszClassName = L"CreationSuiteEditor";
RegisterClassExW(&wc);

### ImGui Context

**Source:** [`editor/src/main.cpp`](editor/src/main.cpp#L169) (line 169)

IMGUI_CHECKVERSION() verifies the ImGui headers and library are the same
version (catches mismatched ABI at runtime, not just compile time).
CreateContext() allocates the global ImGuiContext -- ONE per application.
IO flags: NavEnableKeyboard lets keyboard navigate menus/widgets.
ConfigFlags |= ImGuiConfigFlags_DockingEnable enables the docking API.
IMGUI_CHECKVERSION();
ImGui::CreateContext();
ImGuiIO& io = ImGui::GetIO();
io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;  // enables dockable panels
Persist the UI layout (dock positions, window sizes) between sessions.
ImGui writes an .ini file next to the executable:
io.IniFilename = "creation-suite-editor.ini";

### ImGui Backends

**Source:** [`editor/src/main.cpp`](editor/src/main.cpp#L186) (line 186)

ImGui itself is platform-agnostic -- it only produces draw calls.
"Backends" translate those draw calls to a specific platform/renderer:
  imgui_impl_win32 -- translates Win32 messages to ImGui input events.
  imgui_impl_dx11  -- renders ImGui draw data via D3D11 draw calls.
Init order: Win32 backend first (it sets up fonts), then D3D11.
ImGui_ImplWin32_Init(hwnd);
ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

### The Game/Editor Loop

**Source:** [`editor/src/main.cpp`](editor/src/main.cpp#L199) (line 199)

An editor loop mirrors a game loop:
  1. Poll OS messages   -- keyboard, mouse, resize, close.
  2. Begin frame        -- tell ImGui a new frame starts.
  3. Build UI           -- call ImGui:: functions to define all panels.
  4. Render             -- ImGui compiles draw lists; D3D11 executes them.
  5. Present            -- swap chain shows the rendered frame.

This is the same structure as engine_sandbox's main loop --
editors and games share the same fundamental architecture.
bool done = false;
while (!done)
{
-- Poll messages ---------------------------------------------------
MSG msg = {};
while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE))
{
TranslateMessage(&msg);
DispatchMessageW(&msg);
if (msg.message == WM_QUIT)
done = true;
}
if (done) break;

### DXGI Present

**Source:** [`editor/src/main.cpp`](editor/src/main.cpp#L243) (line 243)

Present(1, 0) = vsync ON (swap every 1 monitor refresh).
Present(0, 0) = vsync OFF (swap immediately, may tear).
For an editor vsync ON is correct: no need to run faster than 60fps.
g_pSwapChain->Present(1, 0);
}

### CreateDeviceAndSwapChain

**Source:** [`editor/src/main.cpp`](editor/src/main.cpp#L264) (line 264)

D3D11CreateDeviceAndSwapChain is a single-call way to:
  a) Create an ID3D11Device (the GPU abstraction -- create resources).
  b) Create an ID3D11DeviceContext (issue draw/compute commands).
  c) Create an IDXGISwapChain (manage front/back buffers).
DXGI_SWAP_EFFECT_DISCARD is the classic single-buffered swap mode.
For the editor we use D3D_FEATURE_LEVEL_10_0 minimum (same as engine).
static bool CreateDeviceD3D(HWND hWnd)
{
DXGI_SWAP_CHAIN_DESC sd = {};
sd.BufferCount                        = 2;
sd.BufferDesc.Width                   = 0;   // 0 = match window width
sd.BufferDesc.Height                  = 0;   // 0 = match window height
sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
sd.BufferDesc.RefreshRate.Numerator   = 60;
sd.BufferDesc.RefreshRate.Denominator = 1;
sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
sd.OutputWindow                       = hWnd;
sd.SampleDesc.Count                   = 1;   // no MSAA for the editor UI
sd.SampleDesc.Quality                 = 0;
sd.Windowed                           = TRUE;
sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

### WARP software fallback

**Source:** [`editor/src/main.cpp`](editor/src/main.cpp#L309) (line 309)

If hardware creation fails (e.g. in CI or on a machine without D3D11 GPU),
fall back to D3D_DRIVER_TYPE_WARP -- Microsoft's software rasteriser.
WARP is slow but correct -- perfect for automated testing.
if (FAILED(res))
{
res = D3D11CreateDeviceAndSwapChain(
nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
featureLevelArray, static_cast<UINT>(std::size(featureLevelArray)),
D3D11_SDK_VERSION, &sd, &g_pSwapChain,
&g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext
);
if (FAILED(res)) return false;
}

---

## engine/animation

### Handling Edge Cases

**Source:** [`src/engine/animation/anim_clip.cpp`](src/engine/animation/anim_clip.cpp#L44) (line 44)

-----------------------------------------------------------------------
No keyframes → return the identity/zero keyframe at time t.
t <= first → clamp to first keyframe.
t >= last  → clamp to last  keyframe.
Otherwise  → binary search for the surrounding pair, then interpolate.
-----------------------------------------------------------------------
if (m_keyframes.empty())
{
Keyframe kf;
kf.time = t;
return kf;
}

### Lerp vs Slerp

**Source:** [`src/engine/animation/anim_clip.cpp`](src/engine/animation/anim_clip.cpp#L76) (line 76)

Translation and scale are interpolated with lerp (straight-line blend).
Rotation uses slerp (arc on the 4-D unit quaternion sphere) to ensure
smooth, constant-speed rotation without the "spinning the long way"
artefact that naive lerp can produce.
-----------------------------------------------------------------------
Keyframe result;
result.time        = t;
result.translation = math::Vec3::Lerp(kfA.translation, kfB.translation, alpha);
result.rotation    = math::Quat::Slerp(kfA.rotation,    kfB.rotation,    alpha);
result.scale       = math::Vec3::Lerp(kfA.scale,       kfB.scale,       alpha);
return result;
}

### Wrapping Loopable Clips

**Source:** [`src/engine/animation/anim_clip.cpp`](src/engine/animation/anim_clip.cpp#L105) (line 105)

-----------------------------------------------------------------------
For loopable clips we wrap the playback time so it stays within
[0, duration].  fmod() gives the remainder, e.g.:
  fmod(2.1, 2.0) = 0.1  → clip restarts after it reaches the end.

For non-loopable clips we clamp t to [0, duration].
-----------------------------------------------------------------------
float t = time;
if (m_loopable && m_durationSeconds > math::kEps)
{
t = std::fmod(t, m_durationSeconds);
if (t < 0.0f) t += m_durationSeconds;
}
else
{
Clamp to [0, duration].
t = std::max(0.0f, std::min(t, m_durationSeconds));
}

### Per-Channel Evaluation

**Source:** [`src/engine/animation/anim_clip.cpp`](src/engine/animation/anim_clip.cpp#L129) (line 129)

-----------------------------------------------------------------------
For each channel in this clip, evaluate the keyframes at time t and
write the resulting TRS matrix into outLocalTransforms at the joint index.
-----------------------------------------------------------------------
for (const auto& channel : m_channels)
{
int boneIdx = channel.BoneIndex();
if (boneIdx < 0 || boneIdx >= static_cast<int>(outLocalTransforms.size()))
continue;

### TRS matrix order:

**Source:** [`src/engine/animation/anim_clip.cpp`](src/engine/animation/anim_clip.cpp#L143) (line 143)

Row-major D3D11: Scale · Rotation · Translation
outLocalTransforms[static_cast<size_t>(boneIdx)] =
math::Mat4::TRS(kf.translation, kf.rotation, kf.scale);
}
}

### What is an Animation Clip?

**Source:** [`src/engine/animation/anim_clip.hpp`](src/engine/animation/anim_clip.hpp#L6) (line 6)

============================================================================
An animation clip stores one continuous motion (e.g. "walk", "attack",
"idle").  It is divided into *channels*, one per joint that moves during
the clip.  Each channel holds a list of *keyframes* — sampled points in
time where the joint's TRS transform is recorded.

At runtime, the AnimationSystem advances a *playback time* t (in seconds)
and asks each channel to *evaluate* — find the two surrounding keyframes
and interpolate between them:

  translation: lerp  (straight-line; accurate for position/scale)
  rotation:    slerp (arc; accurate for orientation, no gimbal lock)
  scale:       lerp

This file matches the shared schema:  shared/schemas/anim_clip.schema.json
Reference Python model:              tools/anim_authoring/animation_engine/model/__init__.py

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17

### Sparse Channels

**Source:** [`src/engine/animation/anim_clip.hpp`](src/engine/animation/anim_clip.hpp#L66) (line 66)

Not every joint moves in every animation clip.  We only store channels for
joints that actually have keyframes in a given clip.  Joints not present in
the channel list keep their bind-pose transform.

### Binary Search + Lerp/Slerp

**Source:** [`src/engine/animation/anim_clip.hpp`](src/engine/animation/anim_clip.hpp#L93) (line 93)

We binary-search for the two surrounding keyframes in O(log N) time.
For most clips N ≤ 30 keyframes per channel, so even a linear scan
would be fast enough, but binary search scales to dense motion-capture
data (hundreds of keyframes per channel) without modification.

Clamping: t < first keyframe → return first keyframe.
          t > last  keyframe → return last  keyframe.
(Looping is handled at the AnimClip level by wrapping t before calling.)

### Clip Evaluation into Local Transforms

**Source:** [`src/engine/animation/anim_clip.hpp`](src/engine/animation/anim_clip.hpp#L123) (line 123)

AnimClip::Evaluate(time, skeleton, outLocalTransforms) fills one Mat4 per
joint in the skeleton.  Joints that have no channel in this clip are left
as their bind-pose local matrix — so clips can be "sparse" (only animate
the joints they care about) and stacked with other clips in a blend tree.

### How Sparse Channels Work

**Source:** [`src/engine/animation/anim_clip.hpp`](src/engine/animation/anim_clip.hpp#L154) (line 154)

For each joint in the skeleton, we look up whether this clip has a
channel for that joint.  If yes, we evaluate it and build a TRS Mat4.
If no, we copy the bind-pose matrix.  This "fill from bind pose"
strategy lets you author a clip that only moves the legs, for example,
and blend it with a clip that only moves the arms.

### ECS Component Iteration via World::View

**Source:** [`src/engine/animation/animation_system.cpp`](src/engine/animation/animation_system.cpp#L71) (line 71)

-----------------------------------------------------------------------
World::View<T>(callback) iterates all living entities that have
component T and invokes the callback with (entityID, component&).

This is the canonical ECS iteration pattern used by CombatSystem,
AISystem, and all other systems in this codebase.
-----------------------------------------------------------------------
world.View<AnimatorComponent>([&](EntityID /*id*/, AnimatorComponent& animator)
{
UpdateAnimator(animator, deltaTime);
});
}

### Advancing Playback Time

**Source:** [`src/engine/animation/animation_system.cpp`](src/engine/animation/animation_system.cpp#L92) (line 92)

-----------------------------------------------------------------------
We advance currentTime by deltaTime * playbackSpeed.
playbackSpeed of 1.0 = real-time; 2.0 = double speed; 0.5 = half speed.
-----------------------------------------------------------------------
animator.currentTime += deltaTime * animator.playbackSpeed;

### World and Skin Matrices

**Source:** [`src/engine/animation/animation_system.cpp`](src/engine/animation/animation_system.cpp#L149) (line 149)

-----------------------------------------------------------------------
Two more passes over the joint array:
  1. ComputeWorldTransforms: multiply local × parent (forward pass).
  2. ComputeSkinMatrices:    multiply world × invBind (per-joint).

The resulting skin matrices are stored in animator.jointMatrices.
A future milestone (M4b) will upload these to a D3D11 constant buffer
for the skinned mesh vertex shader.
-----------------------------------------------------------------------
std::vector<math::Mat4> worldTransforms;
skeleton->ComputeWorldTransforms(localTransforms, worldTransforms);

### How AnimationSystem Fits in the ECS

**Source:** [`src/engine/animation/animation_system.hpp`](src/engine/animation/animation_system.hpp#L6) (line 6)

============================================================================
AnimationSystem is an ECS *system*: it iterates over every entity that has
an AnimatorComponent and advances its animation state.

Per frame the system does:
  1. Advance currentTime by (deltaTime * playbackSpeed).
  2. Evaluate the blend tree (or direct clip) at the new currentTime.
     This fills an array of local-space joint matrices.
  3. Ask the skeleton to compute world-space joint matrices.
  4. Compute skin matrices (world × invBind) for GPU skinning.
  5. Store the skin matrices in AnimatorComponent::jointMatrices.
     The render system reads these to upload to a D3D11 constant buffer.

### Separation of Animation and Rendering

**Source:** [`src/engine/animation/animation_system.hpp`](src/engine/animation/animation_system.hpp#L20) (line 20)

AnimationSystem only writes to CPU-side arrays.  It does NOT upload
anything to the GPU.  The D3D11 skinning render pass (M4b) will read
AnimatorComponent::jointMatrices and upload them to a constant buffer.
This separation keeps the animation system platform-independent (it runs
identically on Windows and Linux).

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17

### Asset Registries Inside Systems

**Source:** [`src/engine/animation/animation_system.hpp`](src/engine/animation/animation_system.hpp#L58) (line 58)

The system owns maps from ID strings → Skeleton / AnimClip / BlendTree.
This matches the pattern in all other game systems (InventorySystem,
QuestSystem, etc.) where the system owns the "database" of definitions.

At M7 (world streaming) these maps will migrate to the AssetDB.  For M4
they live here to keep things simple.

### std::move

**Source:** [`src/engine/animation/animation_system.hpp`](src/engine/animation/animation_system.hpp#L85) (line 85)

We take the skeleton by value and move it into the map.  This avoids a
deep copy of the joint array.  After the call, the caller's skeleton
object is in a valid-but-unspecified state (effectively empty).

### ECS Update Pattern

**Source:** [`src/engine/animation/animation_system.hpp`](src/engine/animation/animation_system.hpp#L115) (line 115)

We call World::GetComponents<AnimatorComponent>() to obtain every
entity with an AnimatorComponent, then update each one.  This matches
the pattern used by CombatSystem, AISystem, etc.

### Extracting Translation from a Row-Major Mat4

**Source:** [`src/engine/animation/blend_tree.cpp`](src/engine/animation/blend_tree.cpp#L27) (line 27)

---------------------------------------------------------------------------
In row-major order the translation is stored in row 3 (m[3][0..2]).
(In column-major OpenGL convention it would be in column 3 instead.)
---------------------------------------------------------------------------
math::Vec3 ExtractTranslation(const math::Mat4& m)
{
return { m.m[3][0], m.m[3][1], m.m[3][2] };
}

### Extracting Scale from a Row-Major Mat4

**Source:** [`src/engine/animation/blend_tree.cpp`](src/engine/animation/blend_tree.cpp#L38) (line 38)

---------------------------------------------------------------------------
The scale components are the lengths of each row (column in column-major).
Row 0 = X axis, row 1 = Y axis, row 2 = Z axis.
---------------------------------------------------------------------------
math::Vec3 ExtractScale(const math::Mat4& m)
{
auto len = [](float a, float b, float c) { return std::sqrt(a*a + b*b + c*c); };
return {
len(m.m[0][0], m.m[0][1], m.m[0][2]),
len(m.m[1][0], m.m[1][1], m.m[1][2]),
len(m.m[2][0], m.m[2][1], m.m[2][2])
};
}

### Extracting Rotation Quaternion from a Row-Major Mat4

**Source:** [`src/engine/animation/blend_tree.cpp`](src/engine/animation/blend_tree.cpp#L54) (line 54)

---------------------------------------------------------------------------
We divide out the scale to get the pure rotation matrix, then convert the
3×3 rotation matrix to a quaternion using Shepperd's method.

Shepperd's method selects the formula that gives the best numerical
precision based on the largest of the four candidate values
(w², x², y², z²), avoiding near-zero denominators.
---------------------------------------------------------------------------
math::Quat ExtractRotation(const math::Mat4& mat, const math::Vec3& scale)
{
Guard against zero scale.
float sx = (scale.x > math::kEps) ? scale.x : 1.0f;
float sy = (scale.y > math::kEps) ? scale.y : 1.0f;
float sz = (scale.z > math::kEps) ? scale.z : 1.0f;

### Per-Joint TRS Blend

**Source:** [`src/engine/animation/blend_tree.cpp`](src/engine/animation/blend_tree.cpp#L167) (line 167)

-----------------------------------------------------------------------
We cannot directly lerp matrices — that would produce shear / non-
uniform scale artifacts.  Instead we:
  1. Extract TRS from each matrix (decompose).
  2. Lerp translation and scale, slerp rotation.
  3. Rebuild the TRS matrix (recompose).

This is the standard "decompose-blend-recompose" pattern used in
every production animation engine.
-----------------------------------------------------------------------
for (int i = 0; i < n; ++i)
{
const math::Mat4& mA = matA[static_cast<size_t>(i)];
const math::Mat4& mB = matB[static_cast<size_t>(i)];

### What is a Blend Tree?

**Source:** [`src/engine/animation/blend_tree.hpp`](src/engine/animation/blend_tree.hpp#L6) (line 6)

============================================================================
A blend tree is a graph of *nodes* that produce arrays of local joint
transforms.  Leaf nodes sample individual animation clips.  Interior nodes
blend the outputs of their children with weights.

Example:

        LinearBlend (weight=0.3)
       /                      \
  ClipNode("idle")        ClipNode("walk")

If weight = 0.3:
  output[i] = 0.7 * idle[i] + 0.3 * walk[i]  (for every joint i)

In a production engine the blend tree would include 1D/2D parametric blends,
additive layers, IK override nodes, and state-machine-driven transitions.
Here we implement the two essential building blocks: ClipNode and LinearBlend.

### Design Pattern (Composite)

**Source:** [`src/engine/animation/blend_tree.hpp`](src/engine/animation/blend_tree.hpp#L25) (line 25)

The blend tree uses the *Composite* design pattern: BlendNode is an
abstract base, ClipNode and LinearBlendNode are concrete leaf/branch types.
AnimationSystem only sees the BlendNode interface, so adding new node types
(e.g. AdditiveNode, MaskNode) requires no changes to AnimationSystem.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17

### Pure Virtual Interface

**Source:** [`src/engine/animation/blend_tree.hpp`](src/engine/animation/blend_tree.hpp#L60) (line 60)

Declaring Evaluate() as pure virtual forces every subclass to provide an
implementation.  The caller (AnimationSystem) never needs to know which
concrete type it is holding.

### Leaf Nodes Hold Pointers, Not Copies

**Source:** [`src/engine/animation/blend_tree.hpp`](src/engine/animation/blend_tree.hpp#L90) (line 90)

ClipNode holds a raw pointer (non-owning) to an AnimClip.  Ownership of
AnimClip objects stays in the asset manager / AnimationSystem.  This
avoids expensive copies and keeps the blend tree lightweight.

### Per-Joint Matrix Blending

**Source:** [`src/engine/animation/blend_tree.hpp`](src/engine/animation/blend_tree.hpp#L122) (line 122)

We cannot directly lerp 4×4 matrices — they would not remain valid TRS
matrices after blending (their rows would no longer be orthonormal).

Correct approach: decompose each joint's matrix back to TRS, blend the
components individually (lerp translation and scale, slerp rotation), then
rebuild the matrix.

This decompose-blend-recompose approach is what every AAA engine uses for
additive blending.  It is slightly more expensive than matrix lerp but
produces geometrically correct results.

weight = 0.0 → 100% childA
weight = 1.0 → 100% childB
weight = 0.5 → 50/50 blend

### Ownership Model

**Source:** [`src/engine/animation/blend_tree.hpp`](src/engine/animation/blend_tree.hpp#L170) (line 170)

BlendTree owns all its nodes via unique_ptr.  The caller builds the tree
using CreateNode<T>(...), which returns a raw pointer for wiring, while
BlendTree retains ownership.  The tree is destroyed when BlendTree goes
out of scope.

### D3D11 Dynamic Constant Buffer Update Pattern

**Source:** [`src/engine/animation/gpu_skinning.cpp`](src/engine/animation/gpu_skinning.cpp#L6) (line 6)

============================================================================
Every frame the CPU has fresh joint matrices from the AnimationSystem.
We need to get those 4096 bytes into VRAM so the vertex shader can read them.

D3D11 offers several update strategies:

  1. UpdateSubresource()  — copies from CPU to a DEFAULT-usage resource.
                            Causes a pipeline stall if the GPU is still
                            reading the resource.  Slower for per-frame data.

  2. Map(WRITE_DISCARD)   — maps a DYNAMIC resource for CPU writes.
                            The driver "discards" the old data (returns a new
                            memory region) so no GPU stall occurs.
                            Fastest for small, frequently-updated buffers.

  3. Map(WRITE_NO_OVERWRITE) — reuse the same physical memory, but CPU
                               promises not to overwrite data still in flight.
                               Useful for ring-buffer streaming.

We use strategy 2 (WRITE_DISCARD) for simplicity and correctness.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Platform: Windows (D3D11 only)

### Constant Buffer Size Rules

**Source:** [`src/engine/animation/gpu_skinning.cpp`](src/engine/animation/gpu_skinning.cpp#L62) (line 62)

-----------------------------------------------------------------------
D3D11 requires the ByteWidth of a constant buffer to be a non-zero
multiple of 16 bytes.  sizeof(math::Mat4) = 64 bytes (16 floats × 4
bytes), so maxJoints × 64 is always a multiple of 64 which is also a
multiple of 16.
-----------------------------------------------------------------------
const UINT byteWidth = static_cast<UINT>(maxJoints) *
static_cast<UINT>(sizeof(math::Mat4));

### USAGE_DYNAMIC marks this buffer as CPU-writable.

**Source:** [`src/engine/animation/gpu_skinning.cpp`](src/engine/animation/gpu_skinning.cpp#L74) (line 74)

D3D11 places it in write-combined memory accessible by both CPU and GPU.
desc.Usage              = D3D11_USAGE_DYNAMIC;

### BIND_CONSTANT_BUFFER makes it visible in the shader

**Source:** [`src/engine/animation/gpu_skinning.cpp`](src/engine/animation/gpu_skinning.cpp#L77) (line 77)

as a cbuffer.  Only one bind flag is allowed for DYNAMIC resources.
desc.BindFlags          = D3D11_BIND_CONSTANT_BUFFER;

### CPU_ACCESS_WRITE enables Map() with WRITE_DISCARD.

**Source:** [`src/engine/animation/gpu_skinning.cpp`](src/engine/animation/gpu_skinning.cpp#L80) (line 80)

desc.CPUAccessFlags     = D3D11_CPU_ACCESS_WRITE;
desc.MiscFlags          = 0;
desc.StructureByteStride = 0;

### nullptr pInitialData for DYNAMIC resources

**Source:** [`src/engine/animation/gpu_skinning.cpp`](src/engine/animation/gpu_skinning.cpp#L86) (line 86)

-----------------------------------------------------------------------
We do not supply initial data because the CPU will write the joint
matrices via Upload() every frame before the first draw.  Passing
nullptr here is allowed for DYNAMIC usage; the initial contents of the
buffer are undefined but will be fully overwritten before first use.
-----------------------------------------------------------------------
const HRESULT hr = device->CreateBuffer(&desc, nullptr, &m_cbuffer);
if (FAILED(hr))
{
std::cerr << "[GpuSkinningBuffer] CreateBuffer failed. HRESULT=0x"
<< std::hex << static_cast<unsigned long>(hr) << std::dec << "\n";
m_cbuffer = nullptr;
return false;
}

### Map / WRITE_DISCARD / Unmap

**Source:** [`src/engine/animation/gpu_skinning.cpp`](src/engine/animation/gpu_skinning.cpp#L116) (line 116)

-----------------------------------------------------------------------
Map() returns a pointer to GPU-accessible memory.

  D3D11_MAP_WRITE_DISCARD:
    The driver allocates a fresh memory region.  The previous contents
    are "discarded" (the GPU may still be reading the old region, but
    that's fine because we got a NEW region).  This avoids stalls.

  The mapped resource's pData pointer is only valid between Map and Unmap.
  Never cache or use pData outside this window.

  Unmap() commits the write to the driver and invalidates pData.
-----------------------------------------------------------------------
D3D11_MAPPED_SUBRESOURCE mapped = {};
const HRESULT hr = context->Map(m_cbuffer,
0,                        // subresource 0
D3D11_MAP_WRITE_DISCARD,  // discard old data
0,                        // no flags
&mapped);
if (FAILED(hr))
{
std::cerr << "[GpuSkinningBuffer] Map failed. HRESULT=0x"
<< std::hex << static_cast<unsigned long>(hr) << std::dec << "\n";
return;
}

### Fill unused slots with identity matrices.

**Source:** [`src/engine/animation/gpu_skinning.cpp`](src/engine/animation/gpu_skinning.cpp#L147) (line 147)

If count < maxJoints, any joints beyond count get identity so vertices
that reference those joint indices are not deformed unexpectedly.
if (toWrite < m_maxJoints)
{
Build an identity Mat4 and repeat it for remaining slots.
const math::Mat4 identity = math::Mat4::Identity();
auto* dst = static_cast<math::Mat4*>(mapped.pData);
for (int i = toWrite; i < m_maxJoints; ++i)
dst[i] = identity;
}

### VSSetConstantBuffers

**Source:** [`src/engine/animation/gpu_skinning.cpp`](src/engine/animation/gpu_skinning.cpp#L168) (line 168)

-----------------------------------------------------------------------
This binds m_cbuffer to VS constant-buffer slot `slot` (b<slot> in HLSL).
The vertex shader can then access all 64 joint matrices by indexing into
the cbuffer array.

Note: we only set the VS stage here.  If the same joint data were needed
in the geometry or pixel shader, we would also call
GSSetConstantBuffers / PSSetConstantBuffers with the same buffer.
-----------------------------------------------------------------------
context->VSSetConstantBuffers(slot, 1, &m_cbuffer);
}

### What is GPU Skinning?

**Source:** [`src/engine/animation/gpu_skinning.hpp`](src/engine/animation/gpu_skinning.hpp#L6) (line 6)

============================================================================
In CPU skinning the joint-matrix multiplications are done on the CPU and the
already-transformed vertices are sent to the GPU each frame.  That approach
wastes bus bandwidth and CPU cycles.

In GPU skinning (shader skinning):
  1. All joint matrices are uploaded once per frame to a constant buffer.
  2. Each vertex stores bone indices + bone weights (4 influences).
  3. The vertex shader blends the vertex position using those weights:

       worldPos = sum_i( boneWeight[i] * (jointMatrix[boneIndex[i]] * bindPos) )

  4. The vertex buffer itself NEVER changes — it always contains the static
     bind-pose mesh.

This is the standard approach used by every modern AAA game engine because:
  • Only 4096 bytes (64 × 64-byte Mat4) travel the bus per character per
    frame instead of MB of vertex data.
  • The GPU is already designed to execute many identical ALU operations in
    parallel — blending positions with 4 matrices per vertex is trivial.
  • The vertex buffer can be shared between multiple instances (same mesh,
    different skeletons, different animations).

============================================================================

### D3D11 Constant Buffer Layout

**Source:** [`src/engine/animation/gpu_skinning.hpp`](src/engine/animation/gpu_skinning.hpp#L31) (line 31)

============================================================================
A D3D11 constant buffer is a block of bytes accessible to any shader stage.
Layout rules (D3D11 default packing, not cbuffer row_major):
  • Each float4x4 (Mat4) is 64 bytes (4 rows × 4 floats × 4 bytes).
  • 64 matrices × 64 bytes = 4096 bytes (4 KiB) per character.
  • The buffer must be a multiple of 16 bytes — 4096 satisfies this.

The buffer is D3D11_USAGE_DYNAMIC because the CPU writes new joint matrices
every frame.  D3D11_MAP_WRITE_DISCARD tells the driver to return a new
region of memory rather than stalling until the GPU finishes reading the
old data (pipeline hazard avoidance).

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Platform: Windows (D3D11 only)

### D3D11 headers require WIN32_LEAN_AND_MEAN + NOMINMAX guards.

**Source:** [`src/engine/animation/gpu_skinning.hpp`](src/engine/animation/gpu_skinning.hpp#L57) (line 57)

These are defined as compile-time macros by the CMakeLists.txt SANDBOX_DEFS
list, so they are already active when this header is compiled as part of
engine_sandbox.  The #ifndef guards prevent double-definition warnings if
the macros are already set elsewhere.
ifndef WIN32_LEAN_AND_MEAN
 define WIN32_LEAN_AND_MEAN
endif
ifndef NOMINMAX
 define NOMINMAX
endif
include <d3d11.h>

### Why 64?

**Source:** [`src/engine/animation/gpu_skinning.hpp`](src/engine/animation/gpu_skinning.hpp#L78) (line 78)

64 joints covers most humanoid characters (typically 50–60 joints).
64 × sizeof(Mat4) = 64 × 64 = 4096 bytes — fits nicely in one D3D11
constant buffer slot and is well within the D3D11 constant-buffer limit of
65536 bytes (4096 floats = 4096 × 4 bytes = 16 KiB for a cbuffer).

For very complex characters (clothes, face bones) you might raise this to
128 or 256, but that uses more vertex-shader constant registers on older
hardware (GT610 / FL10_0 class).
---------------------------------------------------------------------------
inline constexpr int kMaxGpuJoints = 64;

### D3D11_USAGE_DYNAMIC + CPU_ACCESS_WRITE

**Source:** [`src/engine/animation/gpu_skinning.hpp`](src/engine/animation/gpu_skinning.hpp#L129) (line 129)

These two flags together tell D3D11 that:
  • The GPU only reads the buffer (no GPU writes).
  • The CPU will write to it every frame via Map/Unmap.
The driver can place it in a write-combined memory region (WC) which
is optimised for CPU→GPU streaming with minimal CPU-side caching.

D3D11_MAP_WRITE_DISCARD (used in Upload) tells the driver to give us a
fresh memory region, avoiding any GPU stall waiting for the previous
frame's draw to finish reading the old data.

### Map / Unmap Pattern

**Source:** [`src/engine/animation/gpu_skinning.hpp`](src/engine/animation/gpu_skinning.hpp#L149) (line 149)

Map returns a pointer to GPU-visible memory (or a staging area).
We memcpy the matrices, then Unmap signals to D3D11 that we are done.
After Unmap the CPU pointer is invalid.

Matrices beyond `count` are set to identity so the shader never reads
uninitialised data even if a vertex references a joint index ≥ count.

### VSSetConstantBuffers

**Source:** [`src/engine/animation/gpu_skinning.hpp`](src/engine/animation/gpu_skinning.hpp#L168) (line 168)

A vertex shader can access up to 14 constant buffer slots (D3D11
minimum, more in practice).  We bind the joint matrices to b0 so the
HLSL shader reads them as:

  cbuffer JointCB : register(b0) { float4x4 g_joints[64]; };

If you have a per-object CB (world matrix, etc.) use b1 for that.

### Always unbind constant buffers after drawing.

**Source:** [`src/engine/animation/gpu_skinning.hpp`](src/engine/animation/gpu_skinning.hpp#L182) (line 182)

Leaving a resource bound and then trying to write to it later (e.g. via
Map) can trigger D3D11 debug-layer warnings.

### Two-Bone IK: Step-by-Step

**Source:** [`src/engine/animation/ik_solver.cpp`](src/engine/animation/ik_solver.cpp#L11) (line 11)

============================================================================

Imagine a two-bone arm:  root (shoulder) → mid (elbow) → end (wrist).

             pole
              ↑
        mid●  │
         / \  │
        /   \ │
 root ●       ● end (target)

Given: root, mid, end positions, target position, pole-vector hint.
Find:  new mid position such that end snaps to target.

Step 1 — Measure bone lengths.
  a = |mid  – root|  (upper arm)
  b = |end  – mid |  (forearm)

Step 2 — Clamp target distance.
  d = clamp(|target – root|, 0.001, a + b – ε)
  (If target is beyond full reach, the arm stretches as far as it can.)

Step 3 — Law of Cosines: angle at root.
  cos(α) = (a² + d² – b²) / (2 a d)
  α      = acos(clamp(…, –1, 1))

Step 4 — Construct the bent-elbow direction.
  dir     = normalize(target – root)   (points from root to target)
  perpDir = component of poleDir perpendicular to dir
  (This is what makes the elbow bend in the desired direction.)

Step 5 — New mid position.
  midPos_new = root + a * (cos(α) * dir + sin(α) * perpDir)

============================================================================

### FABRIK: Step-by-Step

**Source:** [`src/engine/animation/ik_solver.cpp`](src/engine/animation/ik_solver.cpp#L47) (line 47)

============================================================================

Chain: j[0] (root) --- j[1] --- j[2] --- j[3] (end effector)
Bone lengths: L[0] = |j[1]-j[0]|, L[1] = |j[2]-j[1]|, L[2] = |j[3]-j[2]|

Per-iteration:

  Forward pass (end to root):
    j[3] = target
    for i = 2 downto 0:
      direction = normalize(j[i] – j[i+1])
      j[i]      = j[i+1] + direction * L[i]

  Backward pass (root to end):
    j[0] = originalRoot
    for i = 1 to N-1:
      direction = normalize(j[i] – j[i-1])
      j[i]      = j[i-1] + direction * L[i-1]

  Stop when |j[N-1] – target| < tolerance.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17

### Use float for bone lengths.

**Source:** [`src/engine/animation/ik_solver.cpp`](src/engine/animation/ik_solver.cpp#L105) (line 105)

These distances are in world units and remain constant throughout the
solve (bones are inextensible rigid segments).
-----------------------------------------------------------------------
const float a = (midPos - rootPos).Length();   // upper bone
const float b = (endPos - midPos).Length();    // lower bone

### Clamping target distance.

**Source:** [`src/engine/animation/ik_solver.cpp`](src/engine/animation/ik_solver.cpp#L119) (line 119)

If the target is too close (d < |a-b|) or too far (d > a+b), the chain
cannot form a valid triangle.  We clamp d so:
  • If the target is farther than (a+b), stretch the chain in a line.
  • If the target is closer than |a-b|, the chain wraps back (rare).
-----------------------------------------------------------------------
const float dRaw = (target - rootPos).Length();
const float dMin = std::fabsf(a - b) + kEps;
const float dMax = a + b - kEps;
const float d    = std::max(dMin, std::min(dMax, dRaw));

### Clamping the cosine before acos().

**Source:** [`src/engine/animation/ik_solver.cpp`](src/engine/animation/ik_solver.cpp#L133) (line 133)

Floating-point arithmetic can produce values slightly outside [-1, 1]
due to rounding.  acos() is undefined outside that range.  Clamping
prevents NaN at the cost of a few ULPs of error, which is undetectable
in animation.
-----------------------------------------------------------------------
const float cosAlpha = std::max(-1.0f, std::min(1.0f,
(a*a + d*d - b*b) / (2.0f * a * d)));
const float alpha    = std::acos(cosAlpha);   // angle at root

### Pole Vector Decomposition

**Source:** [`src/engine/animation/ik_solver.cpp`](src/engine/animation/ik_solver.cpp#L146) (line 146)

The "pole vector" hints which side the elbow bends toward.  We cannot
use it directly because it may not be perpendicular to rootToTarget.
We must project out the component along rootToTarget to get the true
perpendicular direction.

  perpDir = normalize( poleDir – (poleDir · rootToTarget) * rootToTarget )

This is the standard Gram-Schmidt orthogonalization for two vectors.
If poleDir is parallel to rootToTarget (degenerate), we fall back to
an arbitrary perpendicular using the cross product with (0,1,0).
-----------------------------------------------------------------------
const Vec3 rootToTarget = (target - rootPos).Normalized();

### Rotating the root→target axis by alpha.

**Source:** [`src/engine/animation/ik_solver.cpp`](src/engine/animation/ik_solver.cpp#L194) (line 194)

The new elbow is at distance a from root, in the plane spanned by
rootToTarget and perpDir, at angle alpha from rootToTarget.

  outMidPos = root + a * (cos(alpha) * rootToTarget + sin(alpha) * perpDir)

cos(alpha) is the component along the target direction.
sin(alpha) is the component along the pole direction (the "bend").
-----------------------------------------------------------------------
outMidPos = rootPos
+ rootToTarget * (a * std::cos(alpha))
+ perpDir       * (a * std::sin(alpha));

### Shortest-Arc Rotation

**Source:** [`src/engine/animation/ik_solver.cpp`](src/engine/animation/ik_solver.cpp#L222) (line 222)

-----------------------------------------------------------------------
We want the quaternion q such that q rotates srcDir to dstDir along the
shortest arc on the unit sphere.

Algorithm:
  1. Dot product → cosine of angle between the vectors.
  2. Cross product → rotation axis (perpendicular to the plane).
  3. If dot ≈ 1, vectors are nearly identical → return identity.
  4. If dot ≈ -1, vectors are antiparallel → 180° around arbitrary axis.

Reference: "Real-Time Rendering" (Möller et al.), §4.3.
-----------------------------------------------------------------------
const Vec3 sn = srcDir.Normalized();
const Vec3 dn = dstDir.Normalized();

### Computing Bone Lengths on Add

**Source:** [`src/engine/animation/ik_solver.cpp`](src/engine/animation/ik_solver.cpp#L273) (line 273)

-----------------------------------------------------------------------
When the second joint (and each subsequent joint) is added, we compute
the bone length as the distance to the previous joint.  This ensures
the chain is "at rest" (bone lengths are the bind-pose lengths) at
construction time.

The bone at index i connects joint i to joint i+1.
m_boneLengths has one fewer entry than m_joints.
-----------------------------------------------------------------------
if (!m_joints.empty())
{
float len = (worldPos - m_joints.back()).Length();
m_boneLengths.push_back(len);
}
m_joints.push_back(worldPos);
}

### Re-using the solver for a different pose each frame.

**Source:** [`src/engine/animation/ik_solver.cpp`](src/engine/animation/ik_solver.cpp#L300) (line 300)

-----------------------------------------------------------------------
Callers typically reset the joint positions from the current animated
skeleton before solving, so FABRIK operates from the current frame's
pose rather than from a fixed bind pose.  This prevents the IK from
"snapping" visually and gives smoother results.
-----------------------------------------------------------------------
assert(positions.size() == m_joints.size());
m_joints = positions;
}

### If the target is farther than the full chain length,

**Source:** [`src/engine/animation/ik_solver.cpp`](src/engine/animation/ik_solver.cpp#L323) (line 323)

the best we can do is point the chain straight toward the target.
We still run the backward pass which will do exactly this.
-----------------------------------------------------------------------
const Vec3  rootPos     = m_joints[0];   // Remember root for backward pass.
const float totalLen    = TotalLength();
const float targetDist  = (target - rootPos).Length();

### Each iteration does two passes:

**Source:** [`src/engine/animation/ik_solver.cpp`](src/engine/animation/ik_solver.cpp#L334) (line 334)

Forward:  snap end to target, propagate constraints backward.
  Backward: restore root, propagate constraints forward.
On each step we snap a joint toward the previous one while maintaining
bone length.  This is the key insight of FABRIK: it only uses direction
vectors and scalar distances — no matrices, no inverse Jacobians.
-----------------------------------------------------------------------
bool converged = false;

### Check the END effector, not every joint.

**Source:** [`src/engine/animation/ik_solver.cpp`](src/engine/animation/ik_solver.cpp#L374) (line 374)

We only care whether the last joint reached the target.
Individual intermediate joints don't need a tolerance check.
----------------------------------------------------------------
const float endError = (m_joints[static_cast<size_t>(N - 1)] - target).Length();
if (endError < m_tolerance || targetDist > totalLen)
{
converged = true;
break;
}
}

### What is Inverse Kinematics?

**Source:** [`src/engine/animation/ik_solver.hpp`](src/engine/animation/ik_solver.hpp#L6) (line 6)

============================================================================
Forward Kinematics (FK): given joint rotations, compute end-effector position.
Inverse Kinematics (IK): given a desired end-effector position, compute the
joint rotations needed to reach it.

IK is used in FFXV-style games for:
  • Foot placement — keep feet planted on uneven terrain.
  • Hand IK — grip weapons, grab ledges, interact with the world.
  • Look-at — head/eye tracking toward a point of interest.
  • Aim IK — blend a character's spine toward an aim target.

============================================================================

### Two Approaches

**Source:** [`src/engine/animation/ik_solver.hpp`](src/engine/animation/ik_solver.hpp#L19) (line 19)

============================================================================

1. TwoBoneIK — ANALYTICAL (exact, O(1), fixed 2-bone chain)
   Uses the Law of Cosines to compute the exact elbow/knee angle for a
   root→mid→tip chain.  Best for arms (shoulder→elbow→wrist) and legs
   (hip→knee→ankle) because those are always exactly two bones.
   A "pole vector" hint selects which side the middle joint bends toward.

2. FABRIKSolver — ITERATIVE (approximate, fast convergence, N bones)
   Forward And Backward Reaching Inverse Kinematics.  Works on chains of
   any length.  Each iteration does two linear passes (forward and backward)
   along the chain.  Typically converges in 1–5 iterations.
   Best for tails, spines, tentacles, and other multi-joint chains.

============================================================================

### Output Format

**Source:** [`src/engine/animation/ik_solver.hpp`](src/engine/animation/ik_solver.hpp#L35) (line 35)

============================================================================
Both solvers output world-space positions of the solved joints.
The AnimationSystem can use these positions to compute the rotation
quaternion for each joint (rotate the current bone direction toward
the new bone direction using Quat::FromAxisAngle + cross product).

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Platform: All (no platform-specific code)

### When to Use TwoBoneIK

**Source:** [`src/engine/animation/ik_solver.hpp`](src/engine/animation/ik_solver.hpp#L69) (line 69)

Use this for any chain that is EXACTLY two bones long:
  • Arms:  shoulder → elbow → wrist
  • Legs:  hip      → knee  → ankle
  • Claw:  knuckle  → finger_mid → finger_tip

The analytical approach is O(1) and gives the mathematically exact answer
every frame without iteration.

### Law of Cosines

**Source:** [`src/engine/animation/ik_solver.hpp`](src/engine/animation/ik_solver.hpp#L78) (line 78)

Given a triangle with sides a, b, c and angles α, β, γ opposite to them:
  c² = a² + b² – 2ab·cos(γ)
Rearranged to find an angle:
  cos(γ) = (a² + b² – c²) / (2ab)

We use this to find the required angle at the root joint (α) and at the
mid joint (β) given the desired target distance.

### Law of Cosines Derivation

**Source:** [`src/engine/animation/ik_solver.hpp`](src/engine/animation/ik_solver.hpp#L111) (line 111)

-----------------------------------------------------------------------
Let:
  a = |midPos  – rootPos|   (upper bone length)
  b = |endPos  – midPos |   (lower bone length)
  d = clamped(|target – rootPos|, 0.001, a+b)  (target distance)

Angle at root (alpha):
  cos(alpha) = (a² + d² – b²) / (2·a·d)
  alpha      = acos(clamp(…, -1, 1))

Angle at mid (beta):
  cos(beta)  = (a² + b² – d²) / (2·a·b)

We then rotate the root→target axis by alpha using the pole vector as
the rotation axis to find the new mid joint position.
-----------------------------------------------------------------------

### Shortest-Arc Rotation

**Source:** [`src/engine/animation/ik_solver.hpp`](src/engine/animation/ik_solver.hpp#L145) (line 145)

Given two unit vectors, the rotation between them lies in the plane
they span.  The axis is their cross product (perpendicular to both),
and the angle is acos(dot(src, dst)).

Edge cases handled:
  • src ≈ dst: return identity (no rotation needed).
  • src ≈ -dst: return 180° around a stable perpendicular axis.

### FABRIK Algorithm (Aristidou & Lasenby, 2011)

**Source:** [`src/engine/animation/ik_solver.hpp`](src/engine/animation/ik_solver.hpp#L166) (line 166)

"Forward And Backward Reaching Inverse Kinematics"

FABRIK represents each joint as a world-space position.  It converges by
alternating two linear passes:

  Forward pass (end → root):
    Place the end effector at the target.
    Propagate constraints backward along the chain.

  Backward pass (root → end):
    Restore the root to its original position.
    Propagate constraints forward along the chain.

### Why FABRIK is fast

**Source:** [`src/engine/animation/ik_solver.hpp`](src/engine/animation/ik_solver.hpp#L180) (line 180)

Each forward and backward pass is O(N) linear work (just vector arithmetic).
Unlike Jacobian-based methods (which require matrix inversion), FABRIK
converges in 1–5 iterations for most game scenarios.  No trigonometry is
needed; only distance normalization.

### Bone Lengths

**Source:** [`src/engine/animation/ik_solver.hpp`](src/engine/animation/ik_solver.hpp#L186) (line 186)

Bone lengths are computed automatically from the initial joint positions
passed to AddJoint().  They are fixed (inextensible) throughout solving.

### Convergence Criterion

**Source:** [`src/engine/animation/ik_solver.hpp`](src/engine/animation/ik_solver.hpp#L190) (line 190)

Solving stops when |joints[N] – target| < tolerance, or after maxIterations
regardless of error.  For foot planting at 60 fps, 4 iterations is typical.

### Call order matters.

**Source:** [`src/engine/animation/ik_solver.hpp`](src/engine/animation/ik_solver.hpp#L217) (line 217)

Joints must be added root-first (joint 0 = root, joint N = end effector).
The bone length between joint i and joint i+1 is computed automatically.

### Frame Usage Pattern

**Source:** [`src/engine/animation/ik_solver.hpp`](src/engine/animation/ik_solver.hpp#L249) (line 249)

-----------------------------------------------------------------------
Typical per-frame usage:

  // 1. Reset joints to current animated world positions.
  solver.SetJoints(animatedWorldPositions);

  // 2. Solve toward the foot IK target on the terrain.
  bool reached = solver.Solve(footTargetOnTerrain);

  // 3. Derive joint rotations from the solved positions.
  for (int i = 0; i < solver.JointCount()-1; ++i)
  {
      Vec3 newDir = (solver.GetJoint(i+1) - solver.GetJoint(i)).Normalized();
      // … compute Quat to rotate bone[i] toward newDir …
  }
-----------------------------------------------------------------------

### FABRIK state

**Source:** [`src/engine/animation/ik_solver.hpp`](src/engine/animation/ik_solver.hpp#L293) (line 293)

m_joints:      current world positions (modified each Solve() call)
m_bindJoints:  original bind-pose positions (to reset the chain)
m_boneLengths: fixed bone lengths (computed once in AddJoint)
-----------------------------------------------------------------------
std::vector<math::Vec3> m_joints;        ///< Working joint positions
std::vector<float>      m_boneLengths;   ///< bone[i] = |joint[i+1] - joint[i]|

### Mat4 Inversion (Fast 4×4 Rigid-Body Inverse)

**Source:** [`src/engine/animation/skeleton.cpp`](src/engine/animation/skeleton.cpp#L6) (line 6)

============================================================================
A general 4×4 matrix inversion is expensive.  However, TRS (translation +
rotation + scale) matrices have special structure that allows a cheap
"pseudo-inverse":

For a pure rotation matrix R (orthogonal): R^{-1} = R^T (transpose).
For a TRS matrix M = T · R · S:
  M^{-1} = S^{-1} · R^T · T^{-1}

We compute this analytically here to avoid a full Gaussian elimination.
(If the scale is non-uniform the formula is slightly more complex but still
 much cheaper than a generic inversion.)

Reference: Lengyel, "Mathematics for 3D Game Programming", §4.3.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17

### Fast TRS Matrix Inversion

**Source:** [`src/engine/animation/skeleton.cpp`](src/engine/animation/skeleton.cpp#L45) (line 45)

---------------------------------------------------------------------------
For a TRS matrix built from a unit-quaternion rotation and uniform/non-uniform
scale, the inverse can be computed without a general LU-decompose.

Method:
  1. Extract the scale from the column lengths.
  2. Divide each rotation column by its scale to get the pure rotation matrix.
  3. Transpose the rotation block (R^{-1} = R^T for orthogonal matrices).
  4. Compute the inverse translation from the transposed rotation and
     original translation.

If the matrix has zero scale on any axis (degenerate) we fall back to the
identity to avoid a divide-by-zero.
---------------------------------------------------------------------------
math::Mat4 InvertTRS(const math::Mat4& mat)
{
Extract scale from column lengths.
auto len = [&](int col) {
float x = mat.m[0][col], y = mat.m[1][col], z = mat.m[2][col];
return std::sqrt(x*x + y*y + z*z);
};

### Topological invariant

**Source:** [`src/engine/animation/skeleton.cpp`](src/engine/animation/skeleton.cpp#L110) (line 110)

We require that the parent already exists when a child is added.
This means joints are added in breadth-first or depth-first order from
the root — guaranteeing that m_joints is already in topological sort
order when Build() runs.
Joint j;
j.index            = static_cast<int>(m_joints.size());
j.name             = name;
j.parentIndex      = parentIndex;
j.bindTranslation  = translation;
j.bindRotation     = rotation.Normalized();
j.bindScale        = scale;

### Building Bind-Pose World Matrices

**Source:** [`src/engine/animation/skeleton.cpp`](src/engine/animation/skeleton.cpp#L130) (line 130)

-----------------------------------------------------------------------
We compute the bind-pose world-space matrix for each joint in a single
forward pass, then invert it to get invBindMatrix.

local[i] = TRS(bindTranslation, bindRotation, bindScale) for joint i.
world[i] = local[i] * world[parent[i]]   (parent == Identity for root)

The bind-pose local matrices are the bind-pose TRS matrices.
-----------------------------------------------------------------------

### Forward Pass for World Transforms

**Source:** [`src/engine/animation/skeleton.cpp`](src/engine/animation/skeleton.cpp#L172) (line 172)

-----------------------------------------------------------------------
Precondition: localTransforms[i] is the local-space matrix for joint i
(already incorporating the animated TRS, not the bind-pose TRS).

We walk in index order (topologically sorted), so parent is always
computed before child.
-----------------------------------------------------------------------
size_t n = m_joints.size();
worldTransforms.resize(n, math::Mat4::Identity());

### Skin Matrix Computation

**Source:** [`src/engine/animation/skeleton.cpp`](src/engine/animation/skeleton.cpp#L200) (line 200)

-----------------------------------------------------------------------
In row-major (D3D11) convention, matrix multiplication `A * B` applies
A first (transforms points out of A's space) then B (into B's space).

So:  invBind[i] * world[i]
  = (move vertex from bind-pose model space → joint-i local space)
    then (move from joint-i local space → current world space)

Vertex shader reads this as:
  vertexWorld = Σ weight[i] * (skinMatrix[i] * vertexBindPose)
-----------------------------------------------------------------------
size_t n = m_joints.size();
skinMatrices.resize(n, math::Mat4::Identity());

### What is a Skeleton?

**Source:** [`src/engine/animation/skeleton.hpp`](src/engine/animation/skeleton.hpp#L6) (line 6)

============================================================================
A skeleton is a hierarchy of *joints* (also called bones).  Each joint has:
  1. A parent (except the root joint).
  2. A *bind-pose* transform: the joint's rest position relative to its parent.
  3. A *local* transform: the animated pose relative to its parent.
  4. A *world* transform: the final position / orientation in world space,
     computed by walking up the hierarchy:
       world[i] = local[i] × world[parent[i]]

Skinned meshes use the world transform of each joint to deform the mesh:
  vertex_world = Σ (weight[i] × (worldTransform[i] × bindPoseInverse[i]) × vertex_local)

The term "bind pose inverse" (also called the "inverse bind matrix") cancels
out the bind pose so that when the skeleton is in the rest position the mesh
vertices do not move.

This file matches the shared schema:  shared/schemas/skeleton.schema.json
Reference Python model:              tools/anim_authoring/animation_engine/model/__init__.py

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17

### Local vs World Space

**Source:** [`src/engine/animation/skeleton.hpp`](src/engine/animation/skeleton.hpp#L59) (line 59)

Joints are authored (and stored) in *local space* — relative to their parent.
At runtime we compute *world-space* transforms by multiplying child × parent
matrices up the hierarchy (see Skeleton::ComputeWorldTransforms).

bindPose*        — rest-pose local transform (from the authoring tool).
bindPoseInverse  — the inverse of the bind-pose world transform, pre-computed
                   once and used every frame to "un-bind" before applying the
                   current animated pose.  This is the "inverse bind matrix"
                   used in skinning.

### Inverse Bind Matrix

**Source:** [`src/engine/animation/skeleton.hpp`](src/engine/animation/skeleton.hpp#L86) (line 86)

skinVertex = Σ weight[i] * (jointWorldMatrix[i] * invBindMatrix[i]) * localVertex
The invBindMatrix[i] transforms a vertex from "bind-pose model space"
into "joint local space".  Multiplying by jointWorldMatrix[i] then
brings it into "current world space".
math::Mat4  invBindMatrix     = math::Mat4::Identity();
};

### Topology Invariant

**Source:** [`src/engine/animation/skeleton.hpp`](src/engine/animation/skeleton.hpp#L102) (line 102)

Joints are stored in a flat vector sorted such that a parent's index is
always less than its child's index.  This means we can compute world
transforms in a single forward pass (no recursion needed):

  for i in 0..N:
    worldTransform[i] = localTransform[i] * worldTransform[parent[i]]

This "topological sort" of the joint hierarchy is a common optimisation in
production skeletons.

### Why Pre-compute the Inverse?

**Source:** [`src/engine/animation/skeleton.hpp`](src/engine/animation/skeleton.hpp#L143) (line 143)

The inverse bind matrix only changes if the skeleton's rest pose
changes (i.e. if you re-export from the DCC tool).  At runtime it is a
constant that can be baked into the skeleton asset, avoiding a matrix
inversion per joint per frame.

### Forward Pass

**Source:** [`src/engine/animation/skeleton.hpp`](src/engine/animation/skeleton.hpp#L161) (line 161)

We walk the joints in index order (which is guaranteed topological).
For the root (parentIndex == -1) the world transform equals the local
transform.  For every other joint:
  world[i] = local[i] * world[parent[i]]

Multiplying local × parent (not parent × local) is correct in D3D11
row-major convention where the child transform is applied FIRST (it is
in local space) and the parent is applied AFTER (it brings local into
world space).

### Skin Matrix

**Source:** [`src/engine/animation/skeleton.hpp`](src/engine/animation/skeleton.hpp#L180) (line 180)

The skin matrix combines two operations:
  invBindMatrix[i]  — moves the vertex from bind-pose model space into
                      joint-i's local space.
  worldTransform[i] — moves from joint-i's local space into current
                      world space.

Together they implement:
  vertexWorld = Σ weight[i] * skinMatrix[i] * vertexBindPose

---

## engine/assets

### Why parse JSON in a separate .cpp file?

**Source:** [`src/engine/assets/asset_db.cpp`](src/engine/assets/asset_db.cpp#L6) (line 6)

============================================================================
The JSON parsing implementation (nlohmann/json or manual parsing) is an
implementation detail.  By keeping it in the .cpp file we:

 1. Avoid exposing the JSON library headers to every translation unit that
    #includes asset_db.hpp — this dramatically speeds up incremental builds.
 2. Allow swapping the JSON library without changing the public API.
 3. Follow the "Pimpl-light" principle: callers see only what they need.

For M2 we use a minimal hand-written JSON parser that handles the specific
assetdb.json format rather than adding a third-party dependency.  This
teaches the student exactly what JSON parsing involves.  In M3+ we will
replace this with nlohmann/json (added via vcpkg) for full robustness.

============================================================================

### assetdb.json format (produced by cook.exe)

**Source:** [`src/engine/assets/asset_db.cpp`](src/engine/assets/asset_db.cpp#L22) (line 22)

============================================================================
{
  "version": "1.0.0",
  "assets": [
    { "id": "<uuid>", "cookedPath": "<relative/path/to/cooked/file>" },
    ...
  ]
}

We only need "id" and "cookedPath" from each entry.
============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17

### Minimal JSON Parser Helpers

**Source:** [`src/engine/assets/asset_db.cpp`](src/engine/assets/asset_db.cpp#L50) (line 50)

============================================================================
Rather than pulling in a full JSON library for this first iteration, we use
a simple line-by-line extraction approach that works correctly for the flat
assetdb.json format produced by cook_main.cpp.

The algorithm:
  1. Read the whole file into a string.
  2. Find each JSON object that starts with { and ends with }.
  3. Within each object, extract "id" and "cookedPath" values.

This is intentionally simple and educational.  A production engine uses a
proper JSON library (nlohmann/json, rapidjson, etc.).
============================================================================
namespace
{

### std::string::find + substr pattern

**Source:** [`src/engine/assets/asset_db.cpp`](src/engine/assets/asset_db.cpp#L73) (line 73)

This is a common "manual parsing" technique: find the key, skip past the
opening quote, find the closing quote, extract the substring.  It is
fragile (won't handle escaped quotes) but sufficient for machine-generated
JSON with predictable formatting.
---------------------------------------------------------------------------
static std::string extract_string_value(const std::string& object,
const std::string& key)
{
const std::string search = "\"" + key + "\"";
std::size_t pos = object.find(search);
if (pos == std::string::npos) return "";

### std::ifstream for file reading

**Source:** [`src/engine/assets/asset_db.cpp`](src/engine/assets/asset_db.cpp#L114) (line 114)

std::ifstream opens a file for reading.  We check is_open() after
construction because the constructor does not throw on failure by
default (you can enable exceptions via f.exceptions() but that pattern
is less common in game code which prefers explicit error checks).
std::ifstream f(assetDbPath);
if (!f.is_open())
{
LOG_ERROR("AssetDB::Load — cannot open file: " << assetDbPath);
return false;
}

### Deriving the project root from the assetdb.json path

**Source:** [`src/engine/assets/asset_db.cpp`](src/engine/assets/asset_db.cpp#L127) (line 127)

-----------------------------------------------------------------------
assetdb.json lives at <projectRoot>/Cooked/assetdb.json.
std::filesystem::absolute() converts a relative path to absolute using
the current working directory at Load() time — which is reliable because
Load() is called once at startup before any directory changes.

parent_path() twice gives us:
  assetDbPath           → .../samples/project/Cooked/assetdb.json
  .parent_path()        → .../samples/project/Cooked
  .parent_path()        → .../samples/project          ← projectRoot

Storing the absolute project root means GetCookedPath() can always
produce a correct absolute path regardless of where the caller runs.
-----------------------------------------------------------------------
namespace fs = std::filesystem;
m_baseDir = fs::absolute(fs::path(assetDbPath))
.parent_path()  // .../Cooked
.parent_path()  // .../projectRoot
.string();

### std::istreambuf_iterator trick

**Source:** [`src/engine/assets/asset_db.cpp`](src/engine/assets/asset_db.cpp#L149) (line 149)

Constructing a std::string from two istreambuf_iterators reads ALL
characters in the stream in a single step — no loop required.
This is the idiomatic "slurp file" pattern in C++.
std::string contents((std::istreambuf_iterator<char>(f)),
std::istreambuf_iterator<char>());

### Simple object extraction

**Source:** [`src/engine/assets/asset_db.cpp`](src/engine/assets/asset_db.cpp#L160) (line 160)

The assetdb.json format produced by cook_main is:
  { "assets": [ { "id": "...", "cookedPath": "..." }, ... ] }
We scan for JSON object boundaries to extract each asset entry.
-----------------------------------------------------------------------
std::size_t braceDepth = 0;
std::size_t objectStart = std::string::npos;
bool inString = false;

### Absolute path construction

**Source:** [`src/engine/assets/asset_db.cpp`](src/engine/assets/asset_db.cpp#L241) (line 241)

The stored value is a project-root-relative path (e.g. "Cooked/...").
We join it with m_baseDir (the absolute project root captured during
Load()) so the caller receives an absolute path that works regardless
of the current working directory.
namespace fs = std::filesystem;
return (fs::path(m_baseDir) / it->second).string();
}

### What is an Asset Database?

**Source:** [`src/engine/assets/asset_db.hpp`](src/engine/assets/asset_db.hpp#L6) (line 6)

============================================================================
In a game engine, "assets" are data files: textures, audio clips, meshes,
animation clips, scenes, etc.  An asset database (AssetDB) is a lookup
table that answers the question:

  "Given an asset GUID, where is its cooked (runtime-ready) file?"

Why GUIDs instead of file paths?
  • Paths change when a file is moved or renamed; GUIDs never change.
  • Two files can have the same name in different directories; GUIDs are
    globally unique, so there is never ambiguity.
  • The engine always refers to assets by GUID in scene files, scripts, and
    code, so renaming a file on disk does not break anything.

How does it work at runtime?
  1. At engine startup (or project load), call AssetDB::Load() with the
     path to the cooked assetdb.json file.
  2. AssetDB parses the JSON and builds an in-memory hash map:
       GUID (string) → cooked file path (string)
  3. Any system that needs a file calls AssetDB::GetCookedPath(guid).

============================================================================

### assetdb.json vs AssetRegistry.json

**Source:** [`src/engine/assets/asset_db.hpp`](src/engine/assets/asset_db.hpp#L29) (line 29)

============================================================================
AssetRegistry.json  — source-of-truth registry maintained by the editors
                        and cook scripts.  Contains source paths, hashes,
                        and metadata.  Read by the cooker.

assetdb.json        — lightweight runtime index produced by the cooker.
                        Contains only what the engine needs at load time:
                        { id → cooked_path }.  Read by AssetDB::Load().

Keeping these separate means the runtime never has to parse large metadata
blobs that are only relevant to the editor pipeline.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17

### Forward-declaring vs full include

**Source:** [`src/engine/assets/asset_db.hpp`](src/engine/assets/asset_db.hpp#L57) (line 57)

============================================================================
This header includes only the standard library headers needed for the
public interface: std::string, std::unordered_map, std::vector.
We deliberately avoid including heavy headers (json, filesystem) here —
those are implementation details in asset_db.cpp.
Keeping headers lightweight makes compilation faster and reduces coupling.
============================================================================

### Rule of Zero

**Source:** [`src/engine/assets/asset_db.hpp`](src/engine/assets/asset_db.hpp#L101) (line 101)

Because this class owns only an std::unordered_map (which manages its
own memory), the compiler-generated copy/move/destructor are all correct
by default.  We only need to delete copy operations to express intent.
AssetDB(const AssetDB&)            = delete;
AssetDB& operator=(const AssetDB&) = delete;

### Why parse at load time?

**Source:** [`src/engine/assets/asset_db.hpp`](src/engine/assets/asset_db.hpp#L119) (line 119)

Parsing is a one-time O(N) cost at startup.  After that every lookup
is O(1) because std::unordered_map uses a hash table internally.
This pattern — "pay the cost once, amortise over many lookups" — is
fundamental to game engine asset management.

### Base directory derivation

**Source:** [`src/engine/assets/asset_db.hpp`](src/engine/assets/asset_db.hpp#L125) (line 125)

assetdb.json lives at <projectRoot>/Cooked/assetdb.json.
The cooker writes cookedPath values relative to the project root
(e.g. "Cooked/Maps/Town.json").  During Load() we derive the project
root as the grandparent of assetDbPath and store it as m_baseDir.
GetCookedPath() then joins m_baseDir with the stored relative path,
producing an absolute path that resolves correctly regardless of the
caller's working directory.

@param assetDbPath  Absolute or relative path to assetdb.json.
@return true on success; false if the file cannot be opened or parsed.

### Absolute path returned

**Source:** [`src/engine/assets/asset_db.hpp`](src/engine/assets/asset_db.hpp#L159) (line 159)

The path returned is always absolute (or at minimum resolved from the
project root stored during Load()).  This means callers do not need to
know the current working directory — the path works from any location.

Callers should always call Has() first.  If the id is not found, this
function returns an empty string and logs an error.

@param id  UUID v4 string.
@return Absolute cooked file path, or empty string if not found.

### Enabling iteration without exposing internals

**Source:** [`src/engine/assets/asset_db.hpp`](src/engine/assets/asset_db.hpp#L175) (line 175)

Rather than exposing the raw iterator pair of the internal hash map,
we return a snapshot vector.  This keeps the API stable if the internal
data structure changes (e.g. from unordered_map to a sorted map).
The cost is a one-time O(N) copy — acceptable for the rare cases where
the full list is needed (validation, debugging).

@return Vector of all asset GUIDs in insertion-independent order.

### std::unordered_map

**Source:** [`src/engine/assets/asset_db.hpp`](src/engine/assets/asset_db.hpp#L194) (line 194)

-----------------------------------------------------------------------
std::unordered_map<Key, Value> is a hash table.  It provides:
  • O(1) average insert, lookup, erase.
  • O(N) worst-case (all keys hash to the same bucket — very rare).

We use std::string keys (GUIDs) and std::string values (paths).
The std::hash<std::string> specialisation handles hashing automatically.
-----------------------------------------------------------------------
std::unordered_map<std::string, std::string> m_idToPath;

### Base directory for path resolution

**Source:** [`src/engine/assets/asset_db.hpp`](src/engine/assets/asset_db.hpp#L206) (line 206)

-----------------------------------------------------------------------
assetdb.json lives at <projectRoot>/Cooked/assetdb.json.
cookedPath values stored in the JSON are relative to projectRoot.
m_baseDir is set to the absolute projectRoot during Load() so that
GetCookedPath() can return absolute paths regardless of the caller's
working directory.
-----------------------------------------------------------------------
std::string m_baseDir;
};

### Binary File I/O in C++

**Source:** [`src/engine/assets/asset_loader.cpp`](src/engine/assets/asset_loader.cpp#L6) (line 6)

============================================================================
Opening a file with std::ios::binary is essential for non-text assets
(textures, audio, compiled shaders).  Without it, on Windows, the runtime
library translates "\r\n" line endings to "\n", corrupting binary data.

The pattern used here:

  std::ifstream f(path, std::ios::binary | std::ios::ate);
  // std::ios::ate (At The End) opens the file with the read position
  // at the end.  f.tellg() then returns the file size directly.

  auto size = static_cast<std::streamsize>(f.tellg());
  f.seekg(0);    // Rewind to beginning.

  std::vector<uint8_t> buf(size);
  f.read(reinterpret_cast<char*>(buf.data()), size);
  // std::ifstream::read takes char*, but our buffer is uint8_t*.
  // reinterpret_cast is safe here: char and uint8_t are both 1-byte types.

Why pre-size the vector?
Resizing first allocates the exact amount needed in one allocation.
Using push_back/insert would trigger multiple re-allocations as the vector
grows — wasteful for known-size reads.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17

### Precondition assertion style

**Source:** [`src/engine/assets/asset_loader.cpp`](src/engine/assets/asset_loader.cpp#L52) (line 52)

In development builds we want to catch programming errors immediately.
Using a LOG_ERROR + early return is gentler than a hard assert but still
makes the problem visible in the log.  A shipping engine might use
[[likely]]/[[unlikely]] hints or assert() with a descriptive message.
if (!m_db)
{
LOG_ERROR("AssetLoader constructed with null AssetDB pointer.");
}
}

### tellg() after std::ios::ate

**Source:** [`src/engine/assets/asset_loader.cpp`](src/engine/assets/asset_loader.cpp#L94) (line 94)

When a file is opened with std::ios::ate, the initial read position
is at the end of the file.  tellg() (tell get-position) therefore
returns the file size in bytes.
const auto fileSize = f.tellg();
if (fileSize <= 0)
{
LOG_WARN("AssetLoader::LoadRawByPath — file is empty: " << path);
return {};
}

### Synchronous vs Asynchronous Loading

**Source:** [`src/engine/assets/asset_loader.hpp`](src/engine/assets/asset_loader.hpp#L6) (line 6)

============================================================================
This M2 implementation is deliberately *synchronous*:
  • Caller asks for an asset.
  • Loader reads the file immediately (blocks the caller).
  • Caller receives the bytes.

Why synchronous for now?
  Synchronous loading is simple, easy to understand, and correct.  It lets
  us validate the full pipeline (cook → store → load → use) before adding
  the complexity of threading.

The async loader (M7) will use a worker thread + lock-free queue:
  • Caller posts a load request (non-blocking).
  • Worker thread loads the file in the background.
  • Caller polls for completion each frame.
  • On completion, the caller receives the bytes via a callback or future.

This staged approach is how real game engines evolve: correct first,
then optimise.

============================================================================

### Why return std::vector<uint8_t>?

**Source:** [`src/engine/assets/asset_loader.hpp`](src/engine/assets/asset_loader.hpp#L28) (line 28)

============================================================================
Returning raw bytes (uint8_t = unsigned byte, range 0–255) is the most
general format — every file can be represented as bytes.  Higher-level
loaders (TextureLoader, AudioLoader, etc.) call LoadRaw and then parse
the bytes into typed structures.

Alternatives:
  • std::string  — works but semantically wrong (suggests text data).
  • std::span    — zero-copy view, but ownership is unclear.
  • std::vector<uint8_t>  — owning container; clear semantics; correct.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17

### Dependency Injection

**Source:** [`src/engine/assets/asset_loader.hpp`](src/engine/assets/asset_loader.hpp#L96) (line 96)

We take a raw pointer to AssetDB rather than owning it.  This is
"dependency injection" — the caller provides the dependency, giving
them full control over its lifetime.  The loader is a *consumer*, not
an *owner*, of the database.

Raw pointers are fine here because:
 1. Ownership is clear (caller owns the DB).
 2. The DB is not dynamically allocated on behalf of the loader.
 3. Using std::unique_ptr/shared_ptr here would impose unnecessary
    ownership semantics that do not reflect the actual relationship.

@param db  Pointer to a loaded AssetDB.  Must not be null; must outlive
           this AssetLoader.

### std::vector as a raw-bytes buffer

**Source:** [`src/engine/assets/asset_loader.hpp`](src/engine/assets/asset_loader.hpp#L130) (line 130)

Reading a file into a std::vector<uint8_t> is the idiomatic C++17
pattern for binary file I/O:

  std::ifstream f(path, std::ios::binary | std::ios::ate);
  auto size = f.tellg();           // file size from end position
  f.seekg(0);
  std::vector<uint8_t> buf(size);
  f.read(reinterpret_cast<char*>(buf.data()), size);

std::ios::ate opens the file positioned at the end, so tellg() returns
the file size directly.  seekg(0) then rewinds to the beginning.

@param id  Asset GUID.
@return Cooked file bytes, or empty vector if the asset is not found or
        the file cannot be opened.

### Why offer both overloads?

**Source:** [`src/engine/assets/asset_loader.hpp`](src/engine/assets/asset_loader.hpp#L152) (line 152)

LoadRaw(id) is the preferred production path — always reference assets
by GUID so that renames do not break the engine.
LoadRawByPath(path) is a developer convenience for tests and tools
that want to load a specific file without registering it.

@param path  Absolute or relative path to the cooked file.
@return File bytes, or empty vector on failure.

---

## engine/audio

### ECS Audio Loop Design

**Source:** [`src/engine/audio/audio_system.cpp`](src/engine/audio/audio_system.cpp#L6) (line 6)

============================================================================
Each frame, AudioSystem does two passes:

  Pass 1 — Entity SFX:
    Iterate every AudioSourceComponent.  If isPlaying == true and
    voiceIndex == -1 (not yet submitted), call backend.Play() and store
    the returned slot index.  If isPlaying == false and voiceIndex != -1,
    call backend.Stop() and clear the slot.

  Pass 2 — Music FSM:
    If a crossfade is active, ramp the outgoing voice's volume down and
    the incoming voice's volume up linearly.  When the fade completes,
    stop the old voice and clear the fade flag.

============================================================================

### Why Separate Music from SFX?

**Source:** [`src/engine/audio/audio_system.cpp`](src/engine/audio/audio_system.cpp#L22) (line 22)

============================================================================
Music stems loop and are long-lived; SFX are triggered once per event
(footstep, sword swing, ability activation) and are typically short.
Treating them differently lets us:
  • Crossfade only the music channel.
  • Apply 3D spatialisation only to SFX voices.
  • Keep music volume and SFX volume controllable independently.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Target: Windows (MSVC)

### Music Track Registry

**Source:** [`src/engine/audio/audio_system.cpp`](src/engine/audio/audio_system.cpp#L70) (line 70)

-----------------------------------------------------------------------
We store one MusicTrack per MusicState in a fixed-size array indexed
by the enum value.  This gives O(1) lookup with no heap allocation.
The enum values start at 0 (NONE) and go to 4 (MENU) — 5 total.
-----------------------------------------------------------------------
const auto idx = static_cast<size_t>(track.state);
if (idx >= m_tracks.size())
{
LOG_WARN("AudioSystem::RegisterMusicTrack — invalid state index " << idx);
return;
}
m_tracks[idx]    = track;
m_tracksSet[idx] = true;
LOG_INFO("AudioSystem: registered music track state="
<< static_cast<int>(track.state)
<< " clip=" << track.clipID);
}

### Music State Transition

**Source:** [`src/engine/audio/audio_system.cpp`](src/engine/audio/audio_system.cpp#L96) (line 96)

-----------------------------------------------------------------------
If the new state is the same as the current one, do nothing.
Otherwise:
  1. Save the current slot as the "old" (fading out) slot.
  2. Start the new stem on a fresh voice slot.
  3. Set the new stem's volume to 0 (it will fade in).
  4. Kick off the crossfade timer.

XAUDIO2 SetVolume is linear gain (not dB).  We ramp from 0 → target
and target → 0 linearly over CROSSFADE_SECONDS.  A perceptually nicer
curve would be equal-power (√(t)), but linear is easier to teach.
-----------------------------------------------------------------------

### ECS View over AudioSourceComponent

**Source:** [`src/engine/audio/audio_system.cpp`](src/engine/audio/audio_system.cpp#L183) (line 183)

──────────────────────────────────────────────────
World::View iterates only the entities that have AudioSourceComponent.
Entities without it are skipped at zero cost.
-----------------------------------------------------------------------
world.View<AudioSourceComponent>(
[this](EntityID /*id*/, AudioSourceComponent& src)
{
if (src.isPlaying && src.voiceIndex == -1)
{
Start playback — obtain a voice slot.
src.voiceIndex = m_backend.Play(
src.clipID,
src.volume * m_sfxVolume,
src.isLooping
);
}
else if (!src.isPlaying && src.voiceIndex >= 0)
{
Stop playback.
m_backend.Stop(src.voiceIndex);
src.voiceIndex = -1;
}
else if (src.voiceIndex >= 0 && !m_backend.IsPlaying(src.voiceIndex))
{
Voice finished naturally (non-looping clip completed).
src.voiceIndex = -1;
src.isPlaying  = false;
}
}
);

### Linear Volume Crossfade

**Source:** [`src/engine/audio/audio_system.cpp`](src/engine/audio/audio_system.cpp#L228) (line 228)

-----------------------------------------------------------------------
t ∈ [0, 1] where 0 = crossfade begins, 1 = crossfade complete.

  New stem volume = t * targetVolume   (fades IN from silence)
  Old stem volume = (1-t) * oldVolume  (fades OUT to silence)

We call XAudio2Backend::SetSlotVolume() each frame to update the
IXAudio2SourceVoice volume directly on both stems.

At t=1 the old voice is stopped and the flag is cleared.
-----------------------------------------------------------------------

### Layered Music FSM

**Source:** [`src/engine/audio/audio_system.hpp`](src/engine/audio/audio_system.hpp#L6) (line 6)

============================================================================
FF15 uses a layered music system.  The field music is one stem; entering
combat crossfades to the battle stem; winning crossfades to a victory sting
and back to the field stem.  We model this with a simple FSM:

  EXPLORATION ──(enemy spotted)──► BATTLE
  BATTLE      ──(all enemies dead)──► VICTORY
  VICTORY     ──(sting ends)──► EXPLORATION
  ANY         ──(menu opened)──► MENU
  MENU        ──(menu closed)──► previous state

Each state has one music clip GUID.  On transition we:
  1. Fade out the old voice over CROSSFADE_SECONDS.
  2. Fade in the new voice.

============================================================================

### AudioSystem as ECS System

**Source:** [`src/engine/audio/audio_system.hpp`](src/engine/audio/audio_system.hpp#L23) (line 23)

============================================================================
AudioSystem is NOT a standard SystemBase (which updates all matching
entities) — it has two responsibilities:

  a) ECS layer: iterate AudioSourceComponent-bearing entities and
     start/stop source voices in XAudio2Backend.

  b) Music layer: manage the music FSM independently of entities.

We inherit from SystemBase for consistency with the rest of the ECS but
also expose SetMusicState() for direct game-loop use.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Target: Windows (MSVC)

### Music State Machine

**Source:** [`src/engine/audio/audio_system.hpp`](src/engine/audio/audio_system.hpp#L64) (line 64)

──────────────────────────────────────
The music state drives which audio stem is playing.  Transitions trigger
a crossfade rather than an abrupt cut to maintain musical continuity.

NONE means no music is configured for the current zone (silence).

### Dual Responsibility (by design)

**Source:** [`src/engine/audio/audio_system.hpp`](src/engine/audio/audio_system.hpp#L101) (line 101)

──────────────────────────────────────────────────
Real game engines often split entity SFX and music management into separate
systems.  Here we combine them to keep the audio subsystem contained in
two files.  When the engine grows, refactor into:
  - SFXSystem    — entity-level sounds only
  - MusicSystem  — music FSM only

### Why pass World explicitly?

**Source:** [`src/engine/audio/audio_system.hpp`](src/engine/audio/audio_system.hpp#L124) (line 124)

─────────────────────────────────────────────
SystemBase::Update(float dt) does not give us the World pointer.
We override the extended version AudioSystem::Update(World&, float) and
leave the base version as a no-op to satisfy the interface.

### XAudio2 Initialisation Sequence

**Source:** [`src/engine/audio/xaudio2_backend.cpp`](src/engine/audio/xaudio2_backend.cpp#L6) (line 6)

============================================================================
Creating a working XAudio2 device takes three steps:

  1. CoInitializeEx()      — initialise the Windows COM runtime.
  2. XAudio2Create()       — create the XAudio2 engine object.
  3. CreateMasteringVoice() — create the final mix stage that outputs
                              to the default audio device.

After that, source voices are created per sound (or pooled for reuse).
Source voices receive PCM data via XAUDIO2_BUFFER structs and are submitted
with SubmitSourceBuffer() + Start().

============================================================================

### WAV File Format (RIFF/WAVE)

**Source:** [`src/engine/audio/xaudio2_backend.cpp`](src/engine/audio/xaudio2_backend.cpp#L20) (line 20)

============================================================================
A .wav file is a RIFF (Resource Interchange File Format) container:

  Offset  Size  Content
  ------  ----  -------
       0     4  'RIFF'
       4     4  Total file size - 8 (uint32 LE)
       8     4  'WAVE'
      12     4  Chunk ID ('fmt ' or 'data' or others)
      16     4  Chunk size (uint32 LE)
      20+      Chunk data

The 'fmt ' chunk contains a WAVEFORMATEX struct:
  - wFormatTag     (1 = PCM, 3 = float, 0xFFFE = extensible)
  - nChannels      (1 = mono, 2 = stereo)
  - nSamplesPerSec (e.g. 44100, 48000)
  - nAvgBytesPerSec
  - nBlockAlign
  - wBitsPerSample (8, 16, 24, 32)

The 'data' chunk contains the raw interleaved PCM samples.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Target: Windows (MSVC)

### pragma comment(lib, ...) for xaudio2

**Source:** [`src/engine/audio/xaudio2_backend.cpp`](src/engine/audio/xaudio2_backend.cpp#L58) (line 58)

---------------------------------------------------------------------------
xaudio2.lib ships with the Windows SDK alongside d3d11.lib and dxgi.lib.
No separate SDK download is required.  We also link ole32.lib for
CoInitializeEx which is required before XAudio2Create.
---------------------------------------------------------------------------
pragma comment(lib, "xaudio2.lib")
pragma comment(lib, "ole32.lib")

### COM Initialisation

**Source:** [`src/engine/audio/xaudio2_backend.cpp`](src/engine/audio/xaudio2_backend.cpp#L92) (line 92)

-----------------------------------------------------------------------
XAudio2 is a COM object.  Before calling any COM API we must initialise
the COM runtime for this thread.  COINIT_MULTITHREADED allows COM objects
to be used safely across threads — important if you later add an audio
streaming thread.

CoInitializeEx returns S_FALSE if COM is already initialised (not an
error), and RPC_E_CHANGED_MODE if init was called with a conflicting
threading model.  We ignore S_FALSE and treat other failures as fatal.
-----------------------------------------------------------------------
HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
if (FAILED(hr) && hr != S_FALSE)
{
LOG_ERROR("XAudio2Backend::Init — CoInitializeEx failed. HRESULT=0x"
<< std::hex << static_cast<unsigned long>(hr) << std::dec);
return false;
}

### XAudio2Create

**Source:** [`src/engine/audio/xaudio2_backend.cpp`](src/engine/audio/xaudio2_backend.cpp#L112) (line 112)

-----------------------------------------------------------------------
XAudio2Create creates the central audio engine object.  Parameters:
  Flags       = 0          (reserved, must be 0)
  Processor   = XAUDIO2_DEFAULT_PROCESSOR  (let XAudio2 pick a thread)

The engine object (IXAudio2) manages the audio processing graph and
owns all voices.
-----------------------------------------------------------------------
hr = XAudio2Create(&m_xaudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
if (FAILED(hr))
{
LOG_ERROR("XAudio2Backend::Init — XAudio2Create failed. HRESULT=0x"
<< std::hex << static_cast<unsigned long>(hr) << std::dec);
return false;
}

### Mastering Voice

**Source:** [`src/engine/audio/xaudio2_backend.cpp`](src/engine/audio/xaudio2_backend.cpp#L130) (line 130)

-----------------------------------------------------------------------
The mastering voice is the final stage in the audio graph:
  Source Voices → [Submix Voices →] Mastering Voice → OS Audio

Parameters:
  InputChannels    = XAUDIO2_DEFAULT_CHANNELS  (match output device)
  InputSampleRate  = XAUDIO2_DEFAULT_SAMPLERATE (match output device)
  Flags            = 0
  DeviceId         = nullptr (default audio device)
  EffectChain      = nullptr (no DSP effects on the master bus)

Only one mastering voice can be active at a time.
-----------------------------------------------------------------------
hr = m_xaudio2->CreateMasteringVoice(
&m_masterVoice,
XAUDIO2_DEFAULT_CHANNELS,
XAUDIO2_DEFAULT_SAMPLERATE,
0,
nullptr,
nullptr
);
if (FAILED(hr))
{
LOG_ERROR("XAudio2Backend::Init — CreateMasteringVoice failed. HRESULT=0x"
<< std::hex << static_cast<unsigned long>(hr) << std::dec);
m_xaudio2->Release();
m_xaudio2 = nullptr;
return false;
}

### XAudio2 Teardown Order

**Source:** [`src/engine/audio/xaudio2_backend.cpp`](src/engine/audio/xaudio2_backend.cpp#L177) (line 177)

-----------------------------------------------------------------------
Shutdown order matters:
  1. Stop and destroy all source voices.
  2. Destroy the mastering voice.
  3. Release the IXAudio2 engine object.
  4. CoUninitialize — balance the CoInitializeEx call.

Destroying the engine (step 3) while voices are running would leave
dangling IXAudio2SourceVoice pointers — always stop voices first.
-----------------------------------------------------------------------

### Resolving Asset IDs to File Paths

**Source:** [`src/engine/audio/xaudio2_backend.cpp`](src/engine/audio/xaudio2_backend.cpp#L239) (line 239)

-----------------------------------------------------------------------
The AssetDB maps GUID strings (like "3a7f-...") to absolute paths of
cooked .wav files on disk.  We load the raw bytes with AssetLoader,
then parse the RIFF/WAVE header ourselves.

For a production engine you would cache decoded WavData objects so
the same clip can be played concurrently without re-reading the file.
-----------------------------------------------------------------------
engine::assets::AssetLoader loader(m_assetDB);
const std::vector<uint8_t> bytes = loader.LoadRaw(clipID);
if (bytes.empty())
{
LOG_ERROR("XAudio2Backend::Play — failed to load clip: " << clipID);
return -1;
}

### XAUDIO2_BUFFER and PCM Lifetime

**Source:** [`src/engine/audio/xaudio2_backend.cpp`](src/engine/audio/xaudio2_backend.cpp#L276) (line 276)

-----------------------------------------------------------------------
The XAUDIO2_BUFFER struct describes one submitted audio buffer.

  Flags         — XAUDIO2_END_OF_STREAM marks the last buffer in a
                  sequence.  Without it, the voice stalls when the
                  buffer runs out (waiting for more data).

  AudioBytes    — total byte count of the PCM data.
  pAudioData    — pointer to the raw PCM bytes.

  LoopCount     — XAUDIO2_LOOP_INFINITE for infinite loop; 0 = no loop.
  LoopBegin/End — loop region within the buffer (0 = full buffer).

CRITICAL: XAudio2 does NOT copy the buffer data.  pAudioData must
remain valid for the entire duration of playback.  We move the PCM
bytes into s.pcmCache (SourceVoiceSlot member), which lives as long
as the slot is in use.  Stop() clears pcmCache after flushing.
-----------------------------------------------------------------------

### Stopping a Source Voice

**Source:** [`src/engine/audio/xaudio2_backend.cpp`](src/engine/audio/xaudio2_backend.cpp#L342) (line 342)

-----------------------------------------------------------------------
IXAudio2SourceVoice::Stop() pauses the voice but does not reset it.
FlushSourceBuffers() discards all queued data.
Together they bring the voice back to a clean, re-usable state.

After flushing we also clear pcmCache — the PCM bytes are no longer
referenced by any buffer, so releasing the memory is safe here.
-----------------------------------------------------------------------
s.voice->Stop();
s.voice->FlushSourceBuffers();
s.inUse  = false;
s.clipID.clear();
s.pcmCache.clear();  // safe to free now that XAudio2 has no reference
}

### Per-Voice Volume for Crossfading

**Source:** [`src/engine/audio/xaudio2_backend.cpp`](src/engine/audio/xaudio2_backend.cpp#L389) (line 389)

-----------------------------------------------------------------------
IXAudio2SourceVoice::SetVolume() applies a scalar gain to the voice's
output.  0.0 = silent, 1.0 = unity gain.

AudioSystem calls this every frame during a crossfade to linearly ramp
the incoming stem from 0 → target and the outgoing stem from target → 0.
The function is cheap: it queues a volume-change operation on the audio
processing thread with no synchronisation overhead.
-----------------------------------------------------------------------
if (slotIndex < 0 || slotIndex >= static_cast<int>(XAUDIO2_VOICE_POOL_SIZE))
return;
auto& s = m_pool[slotIndex];
if (s.inUse && s.voice)
s.voice->SetVolume(volume);
}

### Parsing RIFF/WAVE

**Source:** [`src/engine/audio/xaudio2_backend.cpp`](src/engine/audio/xaudio2_backend.cpp#L428) (line 428)

-----------------------------------------------------------------------
We walk the chunk tree manually using a byte offset.  Each chunk has:

  uint32_t  id   (4 ASCII chars, e.g. 'fmt ', 'data')
  uint32_t  size (byte count of chunk data; does NOT include id/size)
  uint8_t   data[size]

The RIFF root chunk also has a 4-byte form type ("WAVE").

All integer fields in RIFF are little-endian.
-----------------------------------------------------------------------

### WAVEFORMATEX layout

**Source:** [`src/engine/audio/xaudio2_backend.cpp`](src/engine/audio/xaudio2_backend.cpp#L475) (line 475)

---------------------------------------------------------------
Minimum size is 16 bytes (WAVEFORMATEX without cbSize).
We copy exactly sizeof(WAVEFORMATEX) bytes but never more than
the chunk provides, filling the rest with zeros.
---------------------------------------------------------------
if (chunkSize < 16)
return wav;

### Source Voice Format

**Source:** [`src/engine/audio/xaudio2_backend.cpp`](src/engine/audio/xaudio2_backend.cpp#L535) (line 535)

-----------------------------------------------------------------------
A source voice is created for a specific WAVEFORMATEX.  If the existing
voice was created for a different format (e.g. mono 22 kHz vs stereo
44 kHz), we must destroy and recreate it.

For performance, compare the formats first — recreating voices is a
kernel-level operation and should be minimised.
-----------------------------------------------------------------------
auto& s = m_pool[slotIndex];

### CreateSourceVoice

**Source:** [`src/engine/audio/xaudio2_backend.cpp`](src/engine/audio/xaudio2_backend.cpp#L571) (line 571)

-----------------------------------------------------------------------
Parameters:
  ppSourceVoice  — output pointer.
  pSourceFormat  — pointer to WAVEFORMATEX (or WAVEFORMATEXTENSIBLE).
  Flags          — XAUDIO2_VOICE_NOPITCH disables pitch shifting for
                   a minor performance gain when pitch isn't needed.
  MaxFrequencyRatio — 1.0 = no pitch shift; 2.0 = up to +1 octave.
  pCallback      — optional IXAudio2VoiceCallback for buffer events.
-----------------------------------------------------------------------
const HRESULT hr = m_xaudio2->CreateSourceVoice(
&s.voice,
&fmt,
0,       // No flags (allow pitch shift for music tempo control)
2.0f,    // Allow up to 2× frequency ratio (one octave up)
nullptr  // No callback for now
);

### Why XAudio2?

**Source:** [`src/engine/audio/xaudio2_backend.hpp`](src/engine/audio/xaudio2_backend.hpp#L6) (line 6)

============================================================================
XAudio2 is Microsoft's low-level audio API, included in every Windows
installation since Windows 8 (and available as a redistributable for Win 7).
It is the audio backbone of every modern Microsoft title including:
  - Final Fantasy XV (SQUARE ENIX used WASAPI / XAudio2 on PC)
  - Xbox first-party games
  - Most Unity / Unreal games on Windows

XAudio2 sits just above the hardware:

  App → XAudio2 Source Voices → Submix Voices → Mastering Voice → Speakers

Advantages over legacy DirectSound / FMOD (for a teaching engine):
  • Ships with the Windows SDK — zero extra dependency.
  • Runs on GT610-era hardware (XAudio2 is CPU-only; GPU not involved).
  • Supports 3D positional audio via X3DAudio.
  • Minimal overhead: source voices submit raw PCM buffers directly.

============================================================================

### Source Voice Pool

**Source:** [`src/engine/audio/xaudio2_backend.hpp`](src/engine/audio/xaudio2_backend.hpp#L26) (line 26)

============================================================================
Creating and destroying source voices is expensive (kernel transition).
Real engines pre-allocate a fixed pool of voices at init time and re-use
them.  Each slot tracks which clipID is playing so we can stop by ID.

Pool size of 16 handles:
  • Up to 4 party-member SFX channels
  • Up to 6 enemy SFX channels
  • 2 ambient layers (wind, environment)
  • 2 music crossfade slots
  • 2 UI feedback slots

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Target: Windows (MSVC) — XAudio2 is Windows-only.
Requires: xaudio2.lib (Windows SDK — always present)

### RIFF/WAVE Format

**Source:** [`src/engine/audio/xaudio2_backend.hpp`](src/engine/audio/xaudio2_backend.hpp#L86) (line 86)

────────────────────────────────
A .wav file is a RIFF container with a "WAVE" form type.  The two
mandatory chunks are:

  "fmt " — WAVEFORMATEX: sample rate, channels, bit depth, etc.
  "data" — Raw PCM samples.

XAudio2 source voices are created with the fmt header and fed the data
chunk as an XAUDIO2_BUFFER.

### PCM Buffer Lifetime

**Source:** [`src/engine/audio/xaudio2_backend.hpp`](src/engine/audio/xaudio2_backend.hpp#L111) (line 111)

──────────────────────────────────────
XAudio2 source voices operate asynchronously on an audio thread.  When
you call SubmitSourceBuffer, XAudio2 stores a raw pointer (pAudioData)
and continues reading from it on the audio thread until the buffer
finishes.  The calling code MUST keep the PCM bytes alive for at least
as long as the voice is playing.

We solve this by storing the decoded PCM data directly in the slot.
When Play() allocates a slot, it moves the parsed WavData::pcm vector
here.  When Stop() frees the slot, the vector is cleared.

### Backend vs System

**Source:** [`src/engine/audio/xaudio2_backend.hpp`](src/engine/audio/xaudio2_backend.hpp#L142) (line 142)

──────────────────────────────────
The *backend* owns the low-level XAudio2 objects and knows nothing about
the ECS or game state.  It exposes simple Play/Stop primitives.

The *AudioSystem* (audio_system.hpp) is the ECS-aware layer that reads
AudioSourceComponent data and forwards commands to this backend.

This separation means we could swap XAudio2 for FMOD/SDL_mixer without
changing any gameplay code — only the backend changes.

### Voice Reuse

**Source:** [`src/engine/audio/xaudio2_backend.hpp`](src/engine/audio/xaudio2_backend.hpp#L262) (line 262)

─────────────────────────────
A source voice is format-bound at creation time.  When a new clip has
a different format (e.g. different sample rate) we must destroy and
recreate the voice.  For clips with matching formats we simply reuse
the existing voice, which is cheaper.

---

## engine/core

### Why a Publish-Subscribe (Observer) Pattern?

**Source:** [`src/engine/core/EventBus.hpp`](src/engine/core/EventBus.hpp#L6) (line 6)

----------------------------------------------------------------------------
In a complex game, many systems need to react to things that other systems
do.  Naïve approach: every system holds a pointer to every other system
it might need to notify.  Problems:

  • Tight coupling: changing the CombatSystem requires updating every
    other system that has a pointer to it.
  • Hard to add new systems: to add an AchievementSystem you'd need to
    modify CombatSystem, QuestSystem, etc. to know about it.
  • Circular dependencies: SystemA → SystemB → SystemA can cause compile
    errors and undefined behaviour.

The Observer pattern (also called publish-subscribe or event bus) fixes
this by introducing a MEDIATOR:

  Publisher → EventBus → [Subscriber1, Subscriber2, Subscriber3 …]

The publisher knows NOTHING about subscribers.  New subscribers can be
added without touching the publisher.  Systems are fully decoupled.

Real-world analogies:
  • A newspaper: the publisher prints once; any number of subscribers read.
  • A radio station: one broadcast, many receivers.
  • JavaScript DOM events: element.addEventListener('click', handler).

----------------------------------------------------------------------------

### Template Design

**Source:** [`src/engine/core/EventBus.hpp`](src/engine/core/EventBus.hpp#L33) (line 33)

----------------------------------------------------------------------------
We use a template `EventBus<EventType>` so a single implementation works
for ANY event struct.  There is one bus instance per event type:

  EventBus<CombatEvent>  — all combat notifications
  EventBus<QuestEvent>   — all quest notifications
  EventBus<UIEvent>      — all UI notifications

This approach provides compile-time type safety: you cannot accidentally
subscribe a QuestEvent handler to the CombatEvent bus.

----------------------------------------------------------------------------

### std::function and Lambda Closures

**Source:** [`src/engine/core/EventBus.hpp`](src/engine/core/EventBus.hpp#L46) (line 46)

----------------------------------------------------------------------------
std::function<void(const T&)> is a type-erased wrapper around anything
that can be called with a `const T&` argument: a free function, a member
function wrapped in std::bind, or a *lambda*.

A lambda is an anonymous function defined inline:
  auto handler = [](const CombatEvent& e) { std::cout << e.damage; };

The `[&]` or `[=]` capture clause lets the lambda capture local variables
from the enclosing scope by reference or by value.

IMPORTANT: If a lambda captures `this` (a raw pointer), the subscriber
MUST unsubscribe before the owning object is destroyed — otherwise the
EventBus holds a dangling pointer (use-after-free bug).  The
SubscriptionToken / Unsubscribe() mechanism exists precisely for this.

@author  Educational Game Engine Project
@version 1.0
@date    2024

### Event Struct Design

**Source:** [`src/engine/core/EventBus.hpp`](src/engine/core/EventBus.hpp#L94) (line 94)

─────────────────────────────────────
Each event struct is a *value type* (copyable struct, no virtual methods)
carrying all the data a subscriber might need.  This lets subscribers
react without storing pointers back to the source (which might become
invalid).

Keep event structs small and cheap to copy.  If you need to pass large
data, store it elsewhere and pass only an ID or a shared_ptr.

### Template Singleton

**Source:** [`src/engine/core/EventBus.hpp`](src/engine/core/EventBus.hpp#L237) (line 237)

────────────────────────────────────
EventBus<CombatEvent> and EventBus<QuestEvent> are completely independent
classes generated by the compiler from the same template.  Each has its
own Instance(), its own subscriber list, its own mutex.  There is no
interference between buses for different event types.

This technique is called "static polymorphism" or "template-based type
dispatch" — we get per-type behaviour without virtual functions.

Thread Safety:
  Subscribe, Unsubscribe, and Publish are all guarded by m_mutex.
  A subscriber added from thread A is visible to Publish calls from
  thread B after the Subscribe call returns.

  IMPORTANT: Publish holds the mutex while iterating subscribers.
  If a subscriber calls Subscribe or Publish on the SAME bus inside
  its callback, that will DEADLOCK on a non-recursive mutex.
  Solution: use std::recursive_mutex, or (better) buffer events and
  dispatch them outside the lock — the "deferred dispatch" pattern.
  For this educational engine we use a simple mutex and document the
  re-entrancy restriction.

### std::function overhead

**Source:** [`src/engine/core/EventBus.hpp`](src/engine/core/EventBus.hpp#L274) (line 274)

─────────────────────────────────────────
std::function has a small overhead compared to a raw function pointer:
it heap-allocates the closure if the captured state is large ("small
buffer optimisation" avoids this for small closures on most STL
implementations).  For event systems the overhead is negligible.

### Token-based Unsubscription

**Source:** [`src/engine/core/EventBus.hpp`](src/engine/core/EventBus.hpp#L312) (line 312)

─────────────────────────────────────────────
We return a SubscriptionToken (a unique integer ID) rather than storing
a pointer to the callback.  The token lets the subscriber later call
Unsubscribe(token) to remove itself — this is cleaner than comparing
std::function objects (which are not equality-comparable).

The token is generated by atomically incrementing a global counter.
std::atomic<uint64_t> operations are thread-safe without a mutex.

@param callback  A callable to invoke on each published event.
@return          A token you must store if you want to unsubscribe.

Usage:
@code
  auto token = EventBus<CombatEvent>::Instance().Subscribe(
      [this](const CombatEvent& e) { OnCombatEvent(e); });
  // Later, when 'this' is destroyed:
  EventBus<CombatEvent>::Instance().Unsubscribe(token);
@endcode

### When to Unsubscribe

**Source:** [`src/engine/core/EventBus.hpp`](src/engine/core/EventBus.hpp#L351) (line 351)

──────────────────────────────────────
Failure to unsubscribe before the subscriber is destroyed leads to
undefined behaviour: the EventBus will call a function that no longer
exists, or access memory belonging to a destroyed object.

Best practice: in the destructor of any class that subscribes,
unsubscribe all tokens.  Alternatively, use a "scoped subscription"
RAII wrapper (see ScopedSubscription below).

@param token  The token returned by Subscribe().
@return true if the token was found and removed; false if not found.

### Pass by const reference

**Source:** [`src/engine/core/EventBus.hpp`](src/engine/core/EventBus.hpp#L381) (line 381)

──────────────────────────────────────────
We pass the event as `const EventType&` to avoid copying it.
Each subscriber receives the same single event object.  Subscribers
MUST NOT modify the event; if a subscriber needs to mutate state,
it should copy the event fields it needs.

### Snapshot copy

**Source:** [`src/engine/core/EventBus.hpp`](src/engine/core/EventBus.hpp#L388) (line 388)

───────────────────────────────
We copy the subscriber map into a local vector *before* calling
callbacks.  This prevents an issue where a callback calls Unsubscribe()
(modifying m_subscribers while we are iterating it), which would be
undefined behaviour.

Note the mutex is released before callbacks are invoked!  This allows
callbacks to call Subscribe/Unsubscribe on OTHER buses.  If a callback
needs to modify THIS bus, use a deferred dispatch pattern.

@param event  The event to broadcast to all subscribers.

### std::atomic

**Source:** [`src/engine/core/EventBus.hpp`](src/engine/core/EventBus.hpp#L451) (line 451)

─────────────────────────────
`static` here means this counter is shared across ALL EventBus
instantiations (all event types share the same token namespace).
This guarantees that even if the same integer token were returned by
two different buses, they won't collide, because the counter is global.

std::atomic ensures increment-and-read is indivisible (atomic) across
threads — no race condition, no mutex needed for this counter.

### RAII for Resource Management

**Source:** [`src/engine/core/EventBus.hpp`](src/engine/core/EventBus.hpp#L473) (line 473)

───────────────────────────────────────────────
RAII (Resource Acquisition Is Initialisation) is one of C++'s most
powerful idioms.  The idea: tie the lifetime of a resource (here, a
subscription) to the lifetime of an object on the stack (here, a
ScopedSubscription).

When the ScopedSubscription is destroyed (stack unwinds, owner destroyed),
the destructor automatically calls Unsubscribe().  You can NEVER forget to
unsubscribe — the language enforces it.

This is the same principle behind:
  std::unique_ptr  — automatically frees memory
  std::lock_guard  — automatically unlocks mutex
  std::ifstream    — automatically closes file

@tparam EventType  The event type whose bus this subscription is for.

Usage:
@code
  class CombatHUD {
      ScopedSubscription<CombatEvent> m_combatSub;
  public:
      CombatHUD() {
          m_combatSub = ScopedSubscription<CombatEvent>(
              [this](const CombatEvent& e) { UpdateHUD(e); });
      }
      // Destructor auto-unsubscribes — no manual cleanup needed.
  };
@endcode

### Shorthand vs Method Call

**Source:** [`src/engine/core/EventBus.hpp`](src/engine/core/EventBus.hpp#L574) (line 574)

──────────────────────────────────────────
These free functions are thin wrappers around EventBus<T>::Instance().Publish().
They reduce boilerplate at call sites:

  Publish(e);              // short
  EventBus<CombatEvent>::Instance().Publish(e);  // verbose

Both compile to identical machine code — the call is inlined away.

### Separation of Declaration and Definition

**Source:** [`src/engine/core/Logger.cpp`](src/engine/core/Logger.cpp#L5) (line 5)

──────────────────────────────────────────────────────────
In C++ a *declaration* (in the .hpp) tells the compiler the *shape* of a
class: what members and methods exist.  A *definition* (in the .cpp)
provides the *body* of each method.

Why split?
 • Faster compilation: other .cpp files include Logger.hpp and see only
   declarations.  They do NOT re-compile the function bodies every time.
 • Encapsulation: users of Logger.hpp see the public interface; the
   private implementation details are hidden in Logger.cpp.
 • Link-time coherence: the linker combines all compiled .cpp files into
   the final executable.  Each function body appears in exactly ONE .cpp.

@author  Educational Game Engine Project
@version 1.0
@date    2024

### Anonymous Namespaces

**Source:** [`src/engine/core/Logger.cpp`](src/engine/core/Logger.cpp#L34) (line 34)

───────────────────────────────────────
Wrapping internal-only helpers in `namespace { … }` gives them *internal
linkage*: the linker treats them as if they were declared `static`, meaning
they are invisible to other translation units.  This prevents accidental
name clashes when two .cpp files each define a local helper with the same
name.
---------------------------------------------------------------------------
namespace {

### Constructor Initialiser Lists

**Source:** [`src/engine/core/Logger.cpp`](src/engine/core/Logger.cpp#L77) (line 77)

───────────────────────────────────────────────
Member variables should be initialised in the *member initialiser list*
(the `: m_minLevel(…), m_console(…)` part) rather than assigned inside
the constructor body.  The initialiser list:

 1. Constructs each member directly — no "default construct then assign".
 2. Is the ONLY way to initialise const members and members without
    default constructors.
 3. Runs before the constructor body, so the body can safely use members.

### RAII (Resource Acquisition Is Initialisation)

**Source:** [`src/engine/core/Logger.cpp`](src/engine/core/Logger.cpp#L104) (line 104)

───────────────────────────────────────────────────────────────
The destructor is the natural place to release resources (close files,
free memory, unlock mutexes).  By putting cleanup in the destructor we
guarantee it runs even if the program exits via an exception, because
C++ calls destructors for all objects with static storage duration at
programme exit.  This is the cornerstone of the RAII idiom.

### String Streams (std::ostringstream)

**Source:** [`src/engine/core/Logger.cpp`](src/engine/core/Logger.cpp#L189) (line 189)

──────────────────────────────────────────────────────
std::ostringstream is an in-memory string buffer that supports the same
<< insertion operators as std::cout.  We build the complete log line in
memory before writing it, which minimises the time the mutex is held.

Log line format:
  [2024-03-15 14:35:07.123] [INFO    ] [Logger.cpp:42] Your message here
   ^^^^^^^^^^^^^^^^^^^^^^    ^^^^^^^^^   ^^^^^^^^^^^^^
   timestamp                 level       source location

### mutable mutex

**Source:** [`src/engine/core/Logger.cpp`](src/engine/core/Logger.cpp#L281) (line 281)

──────────────────────────────
The mutex is declared `mutable` in the header so it can be locked even
from `const` member functions like this one.  Without `mutable`, the
compiler would reject `lock(m_mutex)` inside a const method.
std::lock_guard<std::mutex> lock(m_mutex);
return m_minLevel;
}

### Two-level time:

**Source:** [`src/engine/core/Logger.cpp`](src/engine/core/Logger.cpp#L317) (line 317)

std::chrono::system_clock  → wall-clock time with sub-second precision
  std::time_t / std::tm      → calendar representation (for formatting)
-----------------------------------------------------------------------

### switch vs array lookup:

**Source:** [`src/engine/core/Logger.cpp`](src/engine/core/Logger.cpp#L363) (line 363)

An array lookup is O(1) and branch-free; a switch compiles to a
  similar jump table.  Both are fine here.
static const char* names[] = {
"DEBUG",    // 0
"INFO",     // 1
"WARNING",  // 2
"ERROR",    // 3
"CRITICAL"  // 4
};
auto idx = static_cast<uint8_t>(level);
if (idx < 5) return names[idx];
return "UNKNOWN";
}

### What is a Logger?

**Source:** [`src/engine/core/Logger.hpp`](src/engine/core/Logger.hpp#L6) (line 6)

----------------------------------------------------------------------------
A logger is a diagnostic tool that records messages about what the program
is doing at runtime.  Good logging is invaluable for:

 • Debugging: "Why did the enemy not spawn?" → check the log.
 • Performance profiling: log frame times, asset loads.
 • Crash reports: the last few log lines before a crash tell you where
   to look.
 • Education: students can add log calls to understand execution flow.

This logger writes to BOTH the console (for immediate feedback during
development) and a file (for post-mortem analysis after crashes).

----------------------------------------------------------------------------

### The Singleton Pattern

**Source:** [`src/engine/core/Logger.hpp`](src/engine/core/Logger.hpp#L21) (line 21)

----------------------------------------------------------------------------
A Singleton ensures a class has EXACTLY ONE instance, accessible globally.

Implementation:
  1. Make constructors private/deleted so callers cannot create instances.
  2. Provide a static `Instance()` method that creates the one instance
     the first time it is called, then returns it every time after.

Trade-offs:
  Pro : Easy global access without passing objects around everywhere.
  Con : Hard to unit-test (global state), potential initialisation order
        issues across translation units.

The Meyers Singleton (static local variable inside Instance()) is the
preferred C++11 way because:
  • Construction is thread-safe (C++11 guarantees it).
  • Lifetime extends to end-of-program (static storage duration).

----------------------------------------------------------------------------

### Thread Safety

**Source:** [`src/engine/core/Logger.hpp`](src/engine/core/Logger.hpp#L41) (line 41)

----------------------------------------------------------------------------
Games often run subsystems on multiple threads (rendering, audio, physics).
If two threads write to the same file or console simultaneously, output
interleaves and becomes garbled, OR (worse) causes undefined behaviour.

Solution: use a std::mutex.  Only one thread can hold a mutex lock at a
time.  Any thread that tries to lock a held mutex waits (blocks) until the
lock is released.  This serialises access to shared resources.

  std::lock_guard<std::mutex> lock(m_mutex);
  // Only ONE thread runs here at a time.

std::lock_guard is a RAII wrapper: it locks on construction, unlocks on
destruction (when it goes out of scope).  Even if an exception is thrown,
the mutex is correctly unlocked.  This is the safest pattern to use.

----------------------------------------------------------------------------

### Macros

**Source:** [`src/engine/core/Logger.hpp`](src/engine/core/Logger.hpp#L59) (line 59)

----------------------------------------------------------------------------
The LOG_DEBUG / LOG_INFO / LOG_WARN / LOG_ERROR / LOG_CRITICAL macros at
the bottom of this file:

 1. Prepend file name and line number automatically via __FILE__ / __LINE__.
 2. Allow calls like:  LOG_INFO("Player spawned at " << x << ", " << y);
    using C++ stream syntax, which composes strings without explicit
    std::to_string calls.
 3. Can be compiled away in release builds (define NDEBUG to strip
    LOG_DEBUG calls completely, saving performance).

@author  Educational Game Engine Project
@version 1.0
@date    2024

### Log Level Filtering

**Source:** [`src/engine/core/Logger.hpp`](src/engine/core/Logger.hpp#L100) (line 100)

──────────────────────────────────────
Setting a minimum level lets you control verbosity at runtime:

  • In development set level to DEBUG to see everything.
  • In release set level to WARNING to only see important events.
  • After a bug report set level to INFO to understand the user's session.

Any message with a level BELOW the minimum is silently discarded.

### Why LERR and not ERROR or ERR?

**Source:** [`src/engine/core/Logger.hpp`](src/engine/core/Logger.hpp#L110) (line 110)

─────────────────────────────────────────────────
Two different system headers define conflicting macros:

 1. Windows SDK (<wingdi.h>, included via <windows.h> / <d3d11.h> / <xaudio2.h>):
      #define ERROR  0
    Using ERROR as an enum member would produce the illegal expression
    `0 = 3` — MSVC error C2143.

 2. ncurses (<curses.h>, included by the Linux terminal renderer):
      #define ERR  (-1)
    Using ERR as an enum member would produce `(-1) = 3` — GCC error.

The idiomatic fix is to choose a name neither header defines.  LERR (short
for "Logger Error") is safe on both platforms.  Callers continue to use
the LOG_ERROR() convenience macro — the internal name is invisible at
call-sites.

### Meyers Singleton

**Source:** [`src/engine/core/Logger.hpp`](src/engine/core/Logger.hpp#L168) (line 168)

───────────────────────────────────
The `static` keyword on a local variable gives it *static storage
duration*: it is constructed the first time this function is called,
lives until the program ends, and construction is thread-safe in C++11.

This avoids the classic double-checked locking anti-pattern and is
simpler and safer.

### std::chrono vs std::time

**Source:** [`src/engine/core/Logger.hpp`](src/engine/core/Logger.hpp#L277) (line 277)

──────────────────────────────────────────
std::chrono (C++11) provides high-resolution clocks.
std::time / std::localtime gives calendar time (year/month/day/hour…).
We use both: chrono for millisecond precision, ctime for human-readable
calendar strings.

@return Formatted string like "2024-03-15 14:35:07.123".

### ANSI Escape Codes

**Source:** [`src/engine/core/Logger.hpp`](src/engine/core/Logger.hpp#L298) (line 298)

────────────────────────────────────
Most Unix/Linux terminals support ANSI escape sequences embedded in
strings to set text colour.  Format: "\033[<code>m"

  "\033[0m"  — reset to default.
  "\033[32m" — set foreground green.
  "\033[31m" — set foreground red.

Windows cmd.exe traditionally does NOT support ANSI codes, but Windows
Terminal and VS Code's terminal do.  We guard console colour behind
a runtime flag.

### Why Use Macros Here?

**Source:** [`src/engine/core/Logger.hpp`](src/engine/core/Logger.hpp#L333) (line 333)

──────────────────────────────────────
Normally macros are discouraged in modern C++ because they ignore scoping
rules and can cause subtle bugs.  Logging macros are one of the few
legitimate remaining uses because:

 1. __FILE__ and __LINE__ are predefined preprocessor macros that expand
    to the current source file name and line number at the point of the
    macro call — impossible to replicate with a regular function call
    (unless using std::source_location in C++20, which not all compilers
    support yet).

 2. The `do { … } while(false)` idiom wraps multiple statements in a single
    statement, making the macro safe inside if-else without braces:

      if (x) LOG_INFO("x"); else LOG_INFO("no x");  // works correctly

    Without the do-while a multi-line macro would break.

 3. The std::ostringstream stream lets callers use << to compose messages
    without building temporary std::string objects manually:

      LOG_INFO("Player at (" << x << ", " << y << ")");

NDEBUG stripping:
  Wrapping LOG_DEBUG in #ifndef NDEBUG causes the compiler to generate
  ZERO instructions for debug logs in release builds — no performance cost.

### What is this file?

**Source:** [`src/engine/core/Types.hpp`](src/engine/core/Types.hpp#L6) (line 6)

----------------------------------------------------------------------------
In any non-trivial C++ project it pays to have ONE authoritative place that
defines all "vocabulary types": small structs, enums, and typedefs that
are shared across many translation units.  Putting them here means:

 1. Every subsystem includes ONE header instead of defining its own
    conflicting int2 / Point / vec3 types.
 2. Changing a fundamental type (e.g. EntityID from uint32_t to uint64_t)
    only requires editing this one file.
 3. Doxygen / documentation tools can produce a single "data dictionary"
    for students reading the engine.

This design follows the "Single Source of Truth" principle.
----------------------------------------------------------------------------

@author  Educational Game Engine Project
@version 1.0
@date    2024

C++ Standard: C++17

Usage example:
@code
  #include "engine/core/Types.hpp"

  Vec3 position{1.0f, 0.0f, 5.0f};
  position += Vec3{0.0f, 0.0f, 1.0f};  // move forward

  EntityID hero = 42;
  GameState state = GameState::EXPLORING;
@endcode

### Why typedef / using?

**Source:** [`src/engine/core/Types.hpp`](src/engine/core/Types.hpp#L71) (line 71)

─────────────────────────────────────
Using aliases (C++11 `using` syntax) improves readability and provides a
single place to swap the underlying type.  For example, if we later need
64-bit entity IDs for a networked game, we change ONE line here.

`using X = Y;`  is the modern C++11 equivalent of  `typedef Y X;`.
Prefer `using` because it works with templates whereas `typedef` does not.

### Sentinel Values

**Source:** [`src/engine/core/Types.hpp`](src/engine/core/Types.hpp#L119) (line 119)

────────────────────────────────
A sentinel value is a special value that signals an "invalid" or "absent"
state without using a pointer or std::optional.  Here we use the maximum
representable value as the null entity so valid IDs start at 0.

This is the same trick used by many high-performance ECS frameworks
(e.g. EnTT uses entt::null which is ~0u).

### Structs vs Classes

**Source:** [`src/engine/core/Types.hpp`](src/engine/core/Types.hpp#L154) (line 154)

────────────────────────────────────
In C++ a `struct` is identical to a `class` except members are public by
default.  For small "plain-old-data" (POD) types like vectors, structs
are conventional because all members are naturally public.

We define arithmetic operators inline so code like
  Vec2 velocity = direction * speed;
reads naturally, just like mathematical notation.

### Operator Overloading

**Source:** [`src/engine/core/Types.hpp`](src/engine/core/Types.hpp#L164) (line 164)

──────────────────────────────────────
C++ lets you define what `+`, `-`, `*`, `/`, `==`, etc. mean for your own
types.  Rules to follow:
  • Prefer member `operator+=` / `-=` and implement `+` in terms of `+=`
    (this avoids code duplication).
  • Return by value (not reference) from binary operators like `+`.
  • `const` member functions do NOT modify the object — mark them `const`.

### Why squared length?

**Source:** [`src/engine/core/Types.hpp`](src/engine/core/Types.hpp#L236) (line 236)

──────────────────────────────────────
Computing std::sqrt is more expensive than multiplication.  When you
only need to COMPARE magnitudes (e.g. "is the enemy within range?"),
compare the *squared* lengths instead and avoid the sqrt entirely.

  if (delta.LengthSquared() < range * range) { … }

### Normalisation

**Source:** [`src/engine/core/Types.hpp`](src/engine/core/Types.hpp#L252) (line 252)

───────────────────────────────
A normalised (unit) vector encodes only *direction*, not magnitude.
It is the fundamental building block of direction-based calculations:
movement, aiming, surface normals, etc.

Guard against dividing by zero when the vector is already the zero
vector — this returns {0,0} rather than NaN.

### Dot Product

**Source:** [`src/engine/core/Types.hpp`](src/engine/core/Types.hpp#L270) (line 270)

────────────────────────────
dot(A, B) = |A| * |B| * cos(θ)
Where θ is the angle between A and B.

 • dot == 1  → vectors point the same direction (unit vectors).
 • dot == 0  → vectors are perpendicular.
 • dot == -1 → vectors point opposite directions.

Extremely useful for: facing checks, projecting one vector onto another,
and lighting calculations.

### Linear Interpolation (lerp)

**Source:** [`src/engine/core/Types.hpp`](src/engine/core/Types.hpp#L296) (line 296)

──────────────────────────────────────────────
lerp(A, B, t) = A + (B - A) * t

  t = 0.0 → returns A
  t = 0.5 → returns the midpoint
  t = 1.0 → returns B

Used everywhere in games: smooth camera movement, animations, colour
transitions, and AI pathfinding.

@param target  End vector.
@param t       Interpolation factor in [0, 1].

### 3D vs 2D

**Source:** [`src/engine/core/Types.hpp`](src/engine/core/Types.hpp#L334) (line 334)

─────────────────────────
The engine uses Vec3 for world-space positions even when the gameplay is
largely 2D, because the z-axis controls draw order (depth sorting) and
height for jumping/climbing.  This is the same approach taken by many
"2.5D" games including older Final Fantasy titles.

The operator and helper structure mirrors Vec2 — students should recognise
the pattern and understand it generalises to any dimension.

### Cross Product

**Source:** [`src/engine/core/Types.hpp`](src/engine/core/Types.hpp#L385) (line 385)

───────────────────────────────
cross(A, B) produces a vector perpendicular to both A and B.
Its magnitude equals |A|*|B|*sin(θ).

Uses:
 • Computing surface normals (lighting, collision).
 • Determining whether a point is to the left or right of a line.
 • Torque and angular physics.

The cross product is NOT commutative: cross(A,B) = -cross(B,A).

### Colour Representation

**Source:** [`src/engine/core/Types.hpp`](src/engine/core/Types.hpp#L433) (line 433)

────────────────────────────────────────
Many rendering APIs (SDL2, OpenGL, Vulkan) accept colours as four unsigned
bytes in the range [0, 255].  Storing each channel as a uint8_t uses
exactly one byte per channel (4 bytes total) — compact and cache-friendly.

If you need high-dynamic-range (HDR) rendering, use float channels [0,1].

### Scoped Enums (enum class)

**Source:** [`src/engine/core/Types.hpp`](src/engine/core/Types.hpp#L501) (line 501)

---------------------------------------------------------------------------
C++11 introduced `enum class` (also called "scoped enum").
Prefer it over plain `enum` for two reasons:

 1. No implicit integer conversion:  NORTH == 0 is a compile error with
    `enum class Direction`, preventing subtle bugs where you accidentally
    compare an EntityID to a Direction value.

 2. No name pollution:  with a plain `enum`, NORTH, SOUTH etc. are injected
    directly into the enclosing namespace.  With `enum class Direction`,
    you must write Direction::NORTH, which avoids name clashes.
---------------------------------------------------------------------------

### State Machine Basics

**Source:** [`src/engine/core/Types.hpp`](src/engine/core/Types.hpp#L533) (line 533)

──────────────────────────────────────
Almost every game is implemented as a *state machine*.  At each moment the
game is in exactly one state; transitions happen on input or events.

For example:
  EXPLORING → player starts combat → COMBAT
  COMBAT    → all enemies dead     → EXPLORING
  EXPLORING → player opens menu    → INVENTORY

This enum labels each state.  A GameStateManager class (not in this file)
would hold the active state and handle transitions.

### Elemental Systems

**Source:** [`src/engine/core/Types.hpp`](src/engine/core/Types.hpp#L598) (line 598)

───────────────────────────────────
Classic JRPGs use elemental typing to add strategic depth: you learn
enemy weaknesses and choose spells/equipment accordingly.  This engine
follows the FF tradition with three elemental types plus a "no element"
option for neutral/physical damage.

The CombatSystem calculates a damage multiplier based on the attacker's
element versus the defender's resistances (stored in StatsComponent).

### Bitmasks

**Source:** [`src/engine/core/Types.hpp`](src/engine/core/Types.hpp#L622) (line 622)

──────────────────────────
A bitmask stores multiple boolean flags packed into a single integer.
Each flag occupies one bit.  Example:

  POISON  = 0b00000001 = 1
  SLEEP   = 0b00000010 = 2
  Both    = 0b00000011 = 3

Check if POISON is active:  (flags & POISON) != 0
Remove SLEEP:                flags &= ~SLEEP

The enum values below are powers of two so they can be OR'd together.

### Damage Formulas

**Source:** [`src/engine/core/Types.hpp`](src/engine/core/Types.hpp#L693) (line 693)

─────────────────────────────────
Separating damage into types enables fine-grained tuning:

  PHYSICAL   → reduced by Defence stat (armour, shields).
  MAGICAL    → reduced by Spirit / Magic Defence stat (elemental wards).
  TRUE_DAMAGE → bypasses ALL defences; used for fall damage, story kills,
                and some boss mechanics.  Use sparingly — players dislike
                unavoidable damage they cannot mitigate.

### AABB Intersection

**Source:** [`src/engine/core/Types.hpp`](src/engine/core/Types.hpp#L742) (line 742)

────────────────────────────────────
Two rectangles A and B are NOT overlapping if one is entirely to the
left, right, above, or below the other.  Negate that condition to get
the overlap test.  This is the classic "separating axis" check for
axis-aligned boxes.

### Magic Numbers

**Source:** [`src/engine/core/Types.hpp`](src/engine/core/Types.hpp#L870) (line 870)

───────────────────────────────
Avoid "magic numbers" scattered through the codebase (e.g. writing 64.0f
in 20 different files for tile size).  Centralise them here.  If you need
to change the tile size, change ONE constant and recompile.

---

## engine/ecs

### What Is an Entity Component System?

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L6) (line 6)

============================================================================

The ECS architecture is the dominant paradigm in modern game engines
(Unity's DOTS, Unreal's Mass, EnTT, Flecs, etc.).  Understanding it is
essential for any game developer.

─── The Problem with Traditional OOP ──────────────────────────────────────

In a classical object-oriented game design you might write:

  class Enemy : public Character { … };
  class FlyingEnemy : public Enemy { … };
  class FlyingBossEnemy : public FlyingEnemy { … };

The inheritance hierarchy grows complex very quickly.  Adding a new
behaviour (e.g. "can set on fire") requires inserting it at exactly the
right level — or copy-pasting it across unrelated branches.  This is
sometimes called the "diamond problem" or "God class" anti-pattern.

─── ECS: Composition over Inheritance ────────────────────────────────────

ECS replaces deep inheritance with three concepts:

 ENTITY     — A unique ID (just an integer).  Has NO data or behaviour.
              Think of it as a row key in a database.

 COMPONENT  — A plain data struct attached to an entity.
              Examples: HealthComponent, TransformComponent, RenderComponent.
              Components have NO behaviour (no methods beyond helpers).

 SYSTEM     — A function or class that iterates entities which have a
              specific set of components and updates them.
              Examples: MovementSystem, CombatSystem, RenderSystem.

An "Enemy" is just an entity with components:
  TransformComponent + HealthComponent + AIComponent + RenderComponent

A "FlyingEnemy" is the same, plus MovementComponent with airborne=true.
A "FlyingBoss" additionally has a CombatComponent with boss abilities.

No inheritance required!  Composition gives unlimited flexibility.

─── Why ECS is Cache-Friendly ─────────────────────────────────────────────

Modern CPUs are much faster than RAM.  When iterating 10,000 enemies,
accessing components stored in a compact contiguous array (dense storage)
causes far fewer cache misses than following pointer chains to separate
heap-allocated objects.  ECS exploits this for significant performance gains.

─── This Implementation ──────────────────────────────────────────────────

 ComponentPool<T>  — Dense contiguous array storing one component type T.
 EntityManager     — Creates/destroys entities; manages free ID list.
 SystemBase        — Abstract base class for all systems.
 World             — Owns EntityManager + all ComponentPools + all Systems.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024

### Static Type IDs (Compile-time vs Runtime)

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L103) (line 103)

──────────────────────────────────────────────────────────
We need a way to map "HealthComponent" → 0, "TransformComponent" → 1, etc.
so we can use the ID as an index into arrays.

Approach: a static counter inside a templated function.  Each time
GetID<T>() is instantiated for a new type T, it captures the next
counter value.  Because `static` local variables are initialised only once,
the same type always gets the same ID within a single program execution.

IMPORTANT: IDs are assigned in the order components are first registered
(first call to GetID<T>() for each type).  They are NOT stable across
runs unless registration order is deterministic (which it is if you call
World::RegisterComponent<T>() in a fixed order at startup).

### Type Erasure

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L157) (line 157)

──────────────────────────────
The World class needs to store pools for ALL component types in a single
container.  Since each pool has a different element type
(ComponentPool<HealthComponent>, ComponentPool<TransformComponent> …),
they cannot share a single std::vector without type erasure.

Solution: all pools inherit from this abstract base class.
The World holds `std::unique_ptr<IComponentPool>`.
When it needs to call a type-specific method, it casts to
`ComponentPool<T>*` using static_cast (safe because we track which
pool index corresponds to which type).

### Dense vs Sparse Storage

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L197) (line 197)

─────────────────────────────────────────
A *sparse* approach: std::unordered_map<EntityID, T>
  Pro:  Simple.
  Con:  Random memory layout — cache-unfriendly for iteration.
        O(1) average access, but with significant constant factor.

A *dense* approach (this implementation):
  - Keep components in a contiguous std::vector<T> (dense array).
  - Keep a sparse array: entityID → index into the dense array.
  - Keep a reverse map: dense index → entityID (for iterating).

This is the "sparse-set" structure used by EnTT and similar libraries.

Characteristics:
  • O(1) add / remove / get (with constant that involves one array lookup).
  • Iteration walks a tight cache line — CPU loves it.
  • Memory overhead: one extra sparse array and one reverse-map array.

─── Visual Example ─────────────────────────────────────────────────────────

 Entity IDs in the world: 0, 1, 2, 3, 4 ...
 Suppose entities 1, 3, 4 have a HealthComponent.

 m_sparse (indexed by EntityID):
   [_, 0, _, 1, 2, _, ...]    (_ = INVALID, not present)

 m_dense (contiguous component array):
   [HP{entity=1, hp=100}, HP{entity=3, hp=50}, HP{entity=4, hp=200}]

 m_entityDense (dense index → EntityID, for reverse lookup):
   [1, 3, 4]

 Iterating ALL health components: just walk m_dense — perfect cache locality.
 Finding entity 3's health: m_sparse[3] = 1 → m_dense[1].
 Removing entity 3's health: swap-and-pop in m_dense, update m_sparse.

### Perfect Forwarding with T&&

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L257) (line 257)

──────────────────────────────────────────────
The parameter `T component` accepts both lvalues and rvalues.
`std::move(component)` transfers ownership into m_dense for move-only
types, or just copies for trivially-copyable types.  Using a value
parameter here is idiomatic for "sink" functions.

### Swap-and-Pop

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L291) (line 291)

──────────────────────────────
Erasing from the middle of a std::vector shifts all subsequent
elements and invalidates indices — O(n) and expensive.

Swap-and-pop is a classic O(1) trick:
  1. Swap the element to remove with the last element.
  2. Pop the last element (O(1)).
  3. Update the sparse index of the element that was swapped in.

This keeps the dense array compact and fast to iterate, at the cost
of changing the order of elements (order is NOT preserved).

### assert vs exception

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L347) (line 347)

──────────────────────────────────────
`assert()` is a debug-only check (disabled when NDEBUG is defined).
Use assert for "this should never happen if code is correct" violations.
Use exceptions for "this might happen at runtime due to external input."
Forgetting to add a component before getting it is a programmer error,
so assert is appropriate here.

### std::optional

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L369) (line 369)

───────────────────────────────
std::optional<T&> cannot be used (references cannot be optional in the
standard).  We use std::optional<T*> as a nullable pointer.  The caller
must check `if (result)` before dereferencing.

This is safer than returning nullptr because `if (optPtr)` is explicit,
while a raw pointer can be silently used without a null check.

### Range-based for with raw pointers

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L404) (line 404)

───────────────────────────────────────────────────
begin() / end() returning pointers allows range-based for:

  for (auto& hp : hpPool) { hp.current -= poison; }

This is valid because std::vector guarantees contiguous storage and
data() returns a pointer to the first element.

### Trade-off: this uses MAX_ENTITIES * sizeof(size_t)

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L447) (line 447)

bytes even if only a few entities have this component.  For 64k
entities and 8-byte size_t that is 512 KB per component type.
Real ECS engines use more memory-efficient structures for sparse
components, but this is clear and educational.

### Entity Lifecycle

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L467) (line 467)

─────────────────────────────────
Entities are just IDs.  The EntityManager is responsible for:
  1. Handing out fresh IDs when entities are created.
  2. Recycling IDs when entities are destroyed so the ID space doesn't
     exhaust.  (With 65536 slots, recycling is critical for long sessions.)
  3. Tracking which IDs are currently alive (the "living set").
  4. Tracking the component presence mask for each entity.

─── Component Signature / Bitmask ──────────────────────────────────────────
Each entity carries a std::bitset<MAX_COMPONENTS> called its "signature".
Bit i is set if the entity currently has the component with ComponentID i.

Systems use a signature-based check to quickly determine whether an entity
"belongs" to their update loop:

  systemSignature & entitySignature == systemSignature

This is an O(MAX_COMPONENTS/64) bitwise AND — extremely fast.

### Free List

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L513) (line 513)

──────────────────────────
A "free list" (here, a std::queue) holds IDs that are available for
reuse.  When an entity is created we pop from the front; when one is
destroyed we push to the back.  This is O(1) and avoids fragmentation.

### Systems in ECS

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L626) (line 626)

────────────────────────────────
A System encapsulates behaviour that operates on a specific combination
of components.  Key design rules:

 1. Systems do NOT store data — components store data.
 2. Each system declares a "required signature": the set of components
    an entity MUST have for the system to process it.
 3. Each system maintains a cached set of matching entities, updated
    whenever components are added or removed.

Example — MovementSystem:
  Required: TransformComponent + MovementComponent.
  Update(): for each matching entity, apply velocity to position.

Example — CombatSystem:
  Required: TransformComponent + CombatComponent + HealthComponent.
  Update(): process attack inputs, apply damage to nearby enemies.

By caching the entity set, systems avoid re-scanning all entities every
frame.  The World updates the cache when components change.

### Why std::unordered_set?

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L708) (line 708)

──────────────────────────────────────────
We need fast insert, erase, and membership check (O(1) average for all
three).  std::unordered_set provides this via hash-based lookup.

In a production engine you might use a sorted std::vector and binary
search, or a dedicated bitset of alive entities, for better cache
behaviour during the actual Update() iteration.  For readability,
unordered_set is clearest.

### Component Design Guidelines

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L731) (line 731)

─────────────────────────────────────────────
 1. Components are PLAIN DATA — prefer simple structs, no virtual methods.
 2. Keep components small and focused.  A "GodComponent" holding 50 fields
    defeats the purpose of ECS.
 3. Choose field types carefully:
    - Use float for continuous values (position, velocity, time).
    - Use int32_t / uint32_t for discrete values (HP, currency, count).
    - Use bool for toggles (isGrounded, isVisible).
    - Use enum class for state machines (face direction, AI state).
 4. Initialise all fields to safe defaults in the struct definition.

### Right-hand coordinate system:

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L755) (line 755)

+X = right, +Y = up, +Z = toward viewer (out of screen in top-down).

### Naming Conventions

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L795) (line 795)

We use short names (`hp`, `mp`) instead of `currentHP`/`currentMP` so
game-system code stays readable:
  hc.hp -= damage;   ← concise and clear
  hc.currentHP -= damage;  ← verbose for a very common operation
int32_t hp          = 100;  ///< Current hit points.
int32_t maxHp       = 100;  ///< Maximum hit points.
int32_t mp          = 50;   ///< Current magic points.
int32_t maxMp       = 50;   ///< Maximum magic points.
bool    isDowned    = false; ///< True when HP == 0 but can still be revived.
bool    isDead      = false; ///< True when revive window expired.
float   downedTimer = 0.0f; ///< Seconds remaining before downed → dead.
float   regenRate   = 0.0f; ///< Passive HP regen per second (out-of-combat).
float   mpRegenRate = 2.0f; ///< Passive MP regen per second.

### Simple string `name` is used throughout game systems.

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L881) (line 881)

The `internalID` is a separate, stable machine-readable identifier for
save files and scripting, so that renaming the display name in the game
doesn't break existing save data.
std::string name;        ///< Shown in the HUD and menus (e.g. "Noctis").
std::string internalID;  ///< Scripting / save system identifier (e.g. "noctis_lvl1").
std::string title;       ///< Optional title shown in dialogue (e.g. "Crown Prince").
};

### Dual-Mode Rendering

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L913) (line 913)

The engine supports both a sprite-based renderer and an ncurses terminal
renderer.  The fields below are used by the terminal renderer only.
A 'symbol' is the ASCII character drawn at the entity's tile position,
and 'colorPair' is an ncurses color-pair index (1–8 typically).
char    symbol    = '@';  ///< ASCII character for terminal rendering.
int     colorPair = 1;    ///< ncurses color-pair index for terminal rendering.

### Item IDs vs Item Objects

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L998) (line 998)

──────────────────────────────────────────
Storing item *IDs* (integers) rather than full item objects keeps
InventoryComponent compact and decoupled.  A separate ItemDatabase
maps IDs to full item data (name, description, stats, sprite).
This is the "flyweight" design pattern.

### Separation of Currency and Inventory

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L1016) (line 1016)

──────────────────────────────────────────────────────
Currency (Gil) is intentionally stored in CurrencyComponent, NOT here.

Why keep them separate?
  1. Single Source of Truth — having Gil in two components created a
     desync bug: buying an item would deduct from CurrencyComponent but
     InventoryComponent::gil could drift to a different value.
  2. Conceptual clarity — "what items do I carry?" is a different question
     from "how much money do I have?".  Separate components make the
     distinction explicit and teachable.
  3. Not all entities carry items AND currency.  A chest entity might have
     an InventoryComponent (loot) but no CurrencyComponent.

Authoritative Gil balance: world.GetComponent<CurrencyComponent>(entity).gil

### Finite State Machine (FSM) in AI

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L1121) (line 1121)

──────────────────────────────────────────────────
Even complex enemy AI is typically implemented as a Finite State Machine:
a set of states and transitions between them.

Enemy states:
  IDLE    → PATROL  (after standing idle for N seconds)
  PATROL  → ALERT   (player detected within sightRange)
  ALERT   → CHASE   (player confirmed enemy)
  CHASE   → ATTACK  (player within attackRange)
  ATTACK  → CHASE   (player out of attackRange)
  CHASE   → PATROL  (player escaped or combat ended)
  ANY     → FLEE    (HP < 20%)

### Level-up Design

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L1340) (line 1340)

────────────────────────────────
In FF15, characters level up only when resting at camp.  XP is accumulated
during combat and exploration, then "banked" to actual levels at camp.
We model this with pendingXP (accumulated) vs currentXP (cashed in).

### Data-Driven Audio via ECS

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L1505) (line 1505)

============================================================================
Instead of hard-coding sound triggers in gameplay code, we attach an
AudioSourceComponent to any entity that should make noise.  The
AudioSystem (src/engine/audio/audio_system.hpp) iterates all entities that
have this component every frame and forwards play/stop requests to the
XAudio2Backend.

Fields mirror the design from docs/FF15_REQUIREMENTS_BLUEPRINT.md §8:

  clipID       — GUID of the cooked .wav asset in the AssetDB.
  is3D         — Enable position-based volume attenuation.
                 2D (false) = same volume everywhere (UI SFX, music).
                 3D (true)  = attenuated by distance to listener.
  volume       — Master volume scalar [0.0 = silent, 1.0 = full].
  isLooping    — When true the source voice loops until stopped.
  isPlaying    — Set true to start playback; AudioSystem clears it when done.
  maxDistance  — World-units at which 3D audio reaches zero volume.

─── Usage Example ──────────────────────────────────────────────────────────

  // Attach footstep sound to the player entity.
  auto& audio  = world.AddComponent<AudioSourceComponent>(player);
  audio.clipID = "a3f2-footstep-stone";   // AssetDB GUID
  audio.is3D   = false;
  audio.volume = 0.8f;
  audio.isPlaying = true;

============================================================================

### Animator Component vs Animation System

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L1591) (line 1591)

============================================================================
The AnimatorComponent is a *data bag*: it holds references to assets
(skeletonID, currentClipID, blendTreeID) and the current runtime state
(currentTime, playbackSpeed, jointMatrices).

The AnimationSystem is the *logic*: every frame it iterates entities that
have this component, evaluates the blend tree (or direct clip), and writes
the resulting skin matrices into jointMatrices.

This separation of data and logic is the central principle of ECS design.
It enables:
  • Multiple systems reading the same component (e.g. AnimationSystem
    writes matrices; RenderSystem reads them to upload to the GPU).
  • Easy serialisation (data components trivially map to JSON).
  • Cache-friendly iteration (all AnimatorComponent data in one block).

─── Usage Example ──────────────────────────────────────────────────────────

  auto& anim           = world.AddComponent<AnimatorComponent>(noctis);
  anim.skeletonID      = "skel_noctis";   // RegisterSkeleton id
  anim.currentClipID   = "clip_idle";     // RegisterClip id
  anim.playbackSpeed   = 1.0f;
  anim.isPlaying       = true;

  // Each frame, AnimationSystem writes jointMatrices[0..jointCount-1].
  // The D3D11 skinning pass uploads these to a constant buffer.

============================================================================

### ECS + Physics Linkage

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L1671) (line 1671)

============================================================================
This component links an ECS entity to a Jolt Physics body.  The field
`bodyID` stores the opaque uint32_t returned by PhysicsWorld::Create*().

Design principle: ECS components store DATA; physics systems contain LOGIC.

The PhysicsSystem (not yet created, M5 physics wiring) will each frame:
  1. Iterate all entities with RigidBodyComponent.
  2. If bodyID == kInvalidBodyID, call PhysicsWorld::Create*() to register
     the body for the first time, writing the returned ID back here.
  3. Call PhysicsWorld::Step(dt) to advance the simulation.
  4. After the step, copy GetPosition(bodyID) back into TransformComponent.

Note: `bodyID` == 0xFFFFFFFF (kInvalidBodyID) means "not yet created".
      You can also use this to disable physics for an entity temporarily.

─── Static vs Dynamic ──────────────────────────────────────────────────────
  isStatic = true   — immovable body (floors, walls). Zero solver cost.
  isStatic = false  — dynamic body (debris, enemies). Costs solver work.

─── useGravity ─────────────────────────────────────────────────────────────
  true  — body accelerates downward under gravity (normal behaviour).
  false — kinematic: moves only when explicitly set; useful for platforms,
           projectiles with custom trajectory, or flying enemies.

============================================================================

### Collider Component

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L1720) (line 1720)

============================================================================
ColliderComponent stores the *shape* of the physics body:
  • What type of shape (box, sphere, capsule)?
  • What are its dimensions?
  • Is it a trigger (overlap-only, no collision response)?

It is paired with RigidBodyComponent: RigidBody provides mass/layer
properties; Collider provides shape geometry.  Separating them allows
entities that are physical but have no collision (e.g. a kinematic camera
target) or entities that are purely a trigger zone (no rigid body needed).

─── ShapeType ──────────────────────────────────────────────────────────────
  Box     — axis-aligned box; best for crates, terrain patches, pillars.
  Sphere  — perfect sphere; best for projectiles, orbs, rolling objects.
  Capsule — cylinder + rounded ends; best for characters, bipeds, NPCs.
  Mesh    — arbitrary concave mesh (static only); best for terrain.

─── isTrigger ──────────────────────────────────────────────────────────────
  false — generates contact events + collision response (normal behaviour).
  true  — generates overlap events only; no collision response.
          Use for: pickup zones, area triggers, damage fields.

============================================================================

### Facade Pattern

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L1778) (line 1778)

────────────────────────────────
The World class is a *facade*: it provides a simple unified API over the
more complex subsystems (EntityManager, individual ComponentPools, Systems).
Users of the ECS call World methods and never interact with the internals
directly.

Ownership model:
  World owns everything.
  EntityManager lives as a direct member.
  ComponentPools live as unique_ptr<IComponentPool> in m_pools[].
  Systems live as unique_ptr<SystemBase> in m_systems[].

Lifetime: one World per "level" / "scene".  Destroying the World destroys
all entities, components, and systems cleanly.

### Variadic Templates

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L1992) (line 1992)

────────────────────────────────────
`template<typename... Components>` accepts any number of type arguments.
We use a fold expression `(sig.set(ComponentTypeRegistry::GetID<C>()) , ...)`
to set one bit per component type — expanded at compile time.

Fold expressions were introduced in C++17:
  (expr , ...) applies the operator , (comma/sequencing) to each
  expansion of the pack.

### Update Order

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L2039) (line 2039)

──────────────────────────────
Systems are updated in the order they were registered.  Order matters:
  • InputSystem  → read player input first.
  • MovementSystem → apply input to transform.
  • CombatSystem → process attacks.
  • AISystem → enemy decisions.
  • PhysicsSystem → resolve collisions.
  • RenderSystem → draw results last.

### View Pattern / Query

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L2066) (line 2066)

──────────────────────────────────────
A "view" is an on-demand filter over living entities.  It avoids
maintaining a cached set (unlike System).  Good for rare or
one-off queries.

Example usage:
@code
  world.View<TransformComponent, HealthComponent>(
      [](EntityID id, TransformComponent& t, HealthComponent& h) {
          if (h.isDowned) t.velocity = Vec3::Zero();
      });
@endcode

### `if constexpr` and Fold Expressions

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L2080) (line 2080)

──────────────────────────────────────────────────────
The implementation uses parameter pack expansion to call HasComponent<C>
for each type C in Components.  The `&&` fold collapses all checks into
a single boolean.

### Factory Methods

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L2133) (line 2133)

─────────────────────────────────
Rather than calling AddComponent 10 times at every call site, a factory
method bundles common configurations.  The caller can still customise
individual components afterwards.

  EntityID noctis = world.CreateCharacter("Noctis", {0,0,0});
  world.GetComponent<StatsComponent>(noctis).strength = 50;

### static_cast vs dynamic_cast

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L2217) (line 2217)

─────────────────────────────────────────────
dynamic_cast performs a runtime type check (RTTI) and returns nullptr
if the types don't match.  It is safe but has a small runtime cost.

static_cast performs NO runtime check — it trusts the programmer that
the types are correct.  We use it here because we KNOW the pool at
index GetID<T>() was created by make_unique<ComponentPool<T>>() in
RegisterComponent<T>() — the invariant is maintained by design.

Using static_cast in carefully controlled internal code like this is
fine; using it on user-supplied pointers would be dangerous.

### Why a free function?

**Source:** [`src/engine/ecs/ECS.hpp`](src/engine/ecs/ECS.hpp#L2294) (line 2294)

───────────────────────────────────────
Putting registration in a free function keeps the World constructor clean
and makes it easy to add new components without modifying World itself.
This follows the Open-Closed Principle: open for extension, closed for
modification.

@param world  The World to register components with.

---

## engine/input

### Why Separate .hpp and .cpp for a Singleton?

**Source:** [`src/engine/input/InputSystem.cpp`](src/engine/input/InputSystem.cpp#L6) (line 6)

============================================================================

Even for a singleton, separating declaration (.hpp) from implementation
(.cpp) is important:

 • The .hpp contains only what CALLERS need to know (the interface).
 • This .cpp contains the actual ncurses calls — an implementation detail.
 • Changing how we poll input (e.g., switching from ncurses to SDL_Event)
   only requires rewriting this .cpp, not touching any caller code.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17

### Thread-Safe Static Local (C++11 §6.7/4)

**Source:** [`src/engine/input/InputSystem.cpp`](src/engine/input/InputSystem.cpp#L37) (line 37)

─────────────────────────────────────────────────────────
The static keyword on a local variable means it is constructed ONCE
the first time execution reaches this line, and destroyed at program
exit.  The C++11 standard explicitly requires that compilers protect
this construction from data races (using an internal mutex/flag).

This is known as "Meyers' Singleton" after Scott Meyers who popularised
it in "Effective C++".
static InputSystem instance;
return instance;
}

### These calls configure the terminal's input behaviour:

**Source:** [`src/engine/input/InputSystem.cpp`](src/engine/input/InputSystem.cpp#L67) (line 67)

keypad(stdscr, TRUE)
    Enables translation of multi-byte escape sequences (arrow keys,
    function keys) into single ncurses KEY_* integer constants.
    Without this, pressing ↑ would give three characters: ESC, '[', 'A'.

  timeout(ms)
    Sets how long getch() waits for input before returning ERR.
    This is essential for a non-blocking game loop.
keypad(stdscr, TRUE);
timeout(m_timeoutMs);

### getch() in a Game Loop

**Source:** [`src/engine/input/InputSystem.cpp`](src/engine/input/InputSystem.cpp#L103) (line 103)

────────────────────────────────────────
getch() reads one keypress from the terminal.

Behaviour (with timeout(50) set):
  • If a key is pressed:    returns the key code immediately.
  • If no key within 50ms:  returns ERR (-1).

Special key codes (arrows, F-keys) are integers > 255.
Regular printable ASCII is in the range 32–126.
Control characters: Enter=10, Escape=27, Tab=9, Backspace=263/127.

We store the result in m_currentKey for the entire frame so all
game systems see the same input state (see InputSystem.hpp teaching note).
-----------------------------------------------------------------------
m_currentKey = getch();

### Readable Key Names for Debugging

**Source:** [`src/engine/input/InputSystem.cpp`](src/engine/input/InputSystem.cpp#L152) (line 152)

──────────────────────────────────────────────────
Printing raw integer key codes during debugging is tedious.
This function maps the most common codes to human-readable strings.
It is not performance-critical, so a series of if/else is fine.

ncurses provides keyname(key) but it returns NULL for unknown keys
and its output format varies by platform.  We roll our own.
-----------------------------------------------------------------------
if (key == ERR)           return "NONE";
if (key == KEY_UP)        return "KEY_UP";
if (key == KEY_DOWN)      return "KEY_DOWN";
if (key == KEY_LEFT)      return "KEY_LEFT";
if (key == KEY_RIGHT)     return "KEY_RIGHT";
if (key == KEY_ENTER)     return "KEY_ENTER";
if (key == KEY_BACKSPACE) return "KEY_BACKSPACE";
if (key == '\n')          return "Enter";
if (key == 27)            return "Escape";
if (key == '\t')          return "Tab";
if (key == ' ')           return "Space";

### switch over enum class

**Source:** [`src/engine/input/InputSystem.cpp`](src/engine/input/InputSystem.cpp#L187) (line 187)

─────────────────────────────────────────
Because InputAction is a scoped enum (enum class), we must qualify
each case label: InputAction::MOVE_UP, not just MOVE_UP.
The compiler warns if any enumerator is missing from the switch
(when compiled with -Wswitch), which is a useful correctness check.
switch (action)
{
case InputAction::NONE:           return "None";
case InputAction::MOVE_UP:        return "Move Up";
case InputAction::MOVE_DOWN:      return "Move Down";
case InputAction::MOVE_LEFT:      return "Move Left";
case InputAction::MOVE_RIGHT:     return "Move Right";
case InputAction::CONFIRM:        return "Confirm";
case InputAction::CANCEL:         return "Cancel";
case InputAction::OPEN_MENU:      return "Open Menu";
case InputAction::OPEN_INVENTORY: return "Open Inventory";
case InputAction::OPEN_CAMP:      return "Open Camp";
case InputAction::OPEN_MAP:       return "Open Map";
case InputAction::ATTACK:         return "Attack";
case InputAction::DODGE:          return "Dodge";
case InputAction::CAST_MAGIC:     return "Cast Magic";
case InputAction::SUMMON:         return "Summon";
case InputAction::INTERACT:       return "Interact";
case InputAction::SKILL_1:        return "Skill 1";
case InputAction::SKILL_2:        return "Skill 2";
case InputAction::SKILL_3:        return "Skill 3";
case InputAction::SKILL_4:        return "Skill 4";
case InputAction::COUNT:          return "COUNT";
No default — compiler warns if we miss a new action.
}
return "Unknown";
}

### std::unordered_map::operator[]

**Source:** [`src/engine/input/InputSystem.cpp`](src/engine/input/InputSystem.cpp#L228) (line 228)

─────────────────────────────────────────────────
m_bindings[rawKey] = action  inserts OR overwrites the entry for rawKey.
If rawKey doesn't exist in the map, a new entry is created.
If it does exist, the old action is replaced (supports rebinding).
m_bindings[rawKey] = action;
}

### Hash Map Lookup

**Source:** [`src/engine/input/InputSystem.cpp`](src/engine/input/InputSystem.cpp#L267) (line 267)

─────────────────────────────────
std::unordered_map::find() returns an iterator.
  end() == not found.
  otherwise, it->second is the mapped value (the InputAction).

This is O(1) average case — constant time regardless of how many
bindings are registered.  Perfect for a per-frame operation.
-----------------------------------------------------------------------
const auto it = m_bindings.find(m_currentKey);
if (it != m_bindings.end())
{
m_currentAction = it->second;
}
}

### Default Bindings Table

**Source:** [`src/engine/input/InputSystem.cpp`](src/engine/input/InputSystem.cpp#L286) (line 286)

────────────────────────────────────────
We register multiple physical keys for the same logical action (e.g.,
both KEY_UP and 'w' map to MOVE_UP).  This gives players flexibility
without any extra code in game systems — they just check the action.

The pattern mirrors Unity's "Input.GetAxis" with named axes:
  Horizontal: ← → A D
  Vertical:   ↑ ↓ W S
-----------------------------------------------------------------------

### Input Handling in Games

**Source:** [`src/engine/input/InputSystem.hpp`](src/engine/input/InputSystem.hpp#L6) (line 6)

============================================================================

Input handling is deceptively complex in games.  The naive approach — polling
the keyboard inside gameplay code — quickly becomes unmaintainable:

  // BAD — input scattered everywhere, hard to remap
  if (getch() == 'w') player.MoveUp();
  if (getch() == 'a') player.MoveLeft();
  …

A dedicated InputSystem solves this with TWO layers of abstraction:

  Layer 1 — RAW INPUT: physical key codes from the OS / ncurses.
    getch() → integer key code (KEY_UP=259, 'w'=119, ESC=27, etc.)

  Layer 2 — ACTIONS: game-semantic events, independent of key bindings.
    InputAction::MOVE_UP, InputAction::ATTACK, InputAction::OPEN_MENU

The InputSystem maps Layer 1 → Layer 2.  Game systems only query Layer 2
(actions).  When the player remaps keys, only the mapping table changes,
not the game code.  This is the "Input Remapping" or "Input Abstraction"
pattern used by Unity (Input.GetAxis), Unreal (Action Mappings), etc.

============================================================================

### Polling vs Event-Driven Input

**Source:** [`src/engine/input/InputSystem.hpp`](src/engine/input/InputSystem.hpp#L31) (line 31)

============================================================================

This engine uses POLLING: every game-loop iteration we call PollInput()
which asks ncurses "was a key pressed this frame?"

An alternative is EVENT-DRIVEN input: the OS pushes key events to a queue
which game code processes asynchronously.  SDL and Win32 use this model.

POLLING advantages: simple, deterministic, easy to debug.
EVENT-DRIVEN advantages: no missed keystrokes between frames, natural for
                         complex key-combo detection (hold, repeat, release).

For an educational engine with a 20fps game loop, polling is perfect.

============================================================================

### Non-Blocking Input in Game Loops

**Source:** [`src/engine/input/InputSystem.hpp`](src/engine/input/InputSystem.hpp#L47) (line 47)

============================================================================

The game loop runs continuously, updating game state and re-drawing every
frame regardless of whether the player pressed a key.  If getch() BLOCKED
(waited forever for a keypress) the game would freeze between keystrokes.

Solution: configure ncurses with `timeout(ms)` so getch() returns ERR after
at most `ms` milliseconds if no key was pressed.  The InputSystem sets a
50ms timeout → 20 polls/second.  ERR maps to InputAction::NONE.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17

### Why include ncurses.h in the header?

**Source:** [`src/engine/input/InputSystem.hpp`](src/engine/input/InputSystem.hpp#L78) (line 78)

──────────────────────────────────────────────────────
The GameKeys namespace below directly uses ncurses KEY_UP, KEY_DOWN, etc.
These are macros defined in ncurses.h.  We must include it in the header
so that any file including InputSystem.hpp sees the same macro definitions.

In a production engine you would isolate ncurses behind a platform layer
so that InputSystem.hpp could be included by non-ncurses code (e.g., unit
tests on Windows).  For this teaching engine we keep it simple.
include <ncurses.h>

### Why a Namespace Instead of an Enum?

**Source:** [`src/engine/input/InputSystem.hpp`](src/engine/input/InputSystem.hpp#L103) (line 103)

──────────────────────────────────────────────────────
We use a namespace of `constexpr int` constants rather than an enum because
ncurses KEY_* values are plain ints (some > 255, well outside char range).
An enum class would require explicit casting everywhere; a namespace gives
clean, scoped names without the ceremony.

These raw codes are the "physical" layer — they represent actual keys.
Game code should prefer the `InputAction` enum (the "logical" layer) and
call InputSystem::GetAction() to check the current action.

Key code reference:
  Arrow keys      : KEY_UP(259), KEY_DOWN(258), KEY_LEFT(260), KEY_RIGHT(261)
  ASCII printable : 'a'-'z' = 97-122, '0'-'9' = 48-57
  Enter           : '\n' (10) or KEY_ENTER (ncurses)
  Escape          : 27 (decimal)
  Tab             : '\t' (9)

### The Action Abstraction

**Source:** [`src/engine/input/InputSystem.hpp`](src/engine/input/InputSystem.hpp#L214) (line 214)

────────────────────────────────────────
Game systems check for ACTIONS, not raw keys.  For example:

  // System code — portable and remappable:
  if (input.GetAction() == InputAction::MOVE_UP) { … }

  // NOT this — hard-coded and fragile:
  if (rawKey == KEY_UP || rawKey == 'w') { … }

This enum uses `uint8_t` as the underlying type because we never need more
than 256 actions, and small underlying types pack nicely in arrays/bitsets.

### The Singleton Pattern

**Source:** [`src/engine/input/InputSystem.hpp`](src/engine/input/InputSystem.hpp#L270) (line 270)

───────────────────────────────────────
A Singleton ensures there is exactly ONE instance of a class, accessible
globally.  It is appropriate for systems that manage a single, global
resource — here the terminal keyboard.

Implementation via a static local variable (Meyers' Singleton) is the
preferred modern C++ approach because:
 • The instance is created on first use (lazy initialisation).
 • C++11 guarantees static local construction is thread-safe.
 • No need for a heap allocation or explicit deletion.

CRITICISM: Singletons make unit testing harder (global state is hard to
mock).  For a teaching engine the trade-off is acceptable.  In a production
engine consider dependency injection (pass an IInputProvider interface).

Usage:
@code
  InputSystem& input = InputSystem::Get();
  input.Init();

  // In the game loop:
  input.PollInput();
  if (input.GetAction() == InputAction::MOVE_UP) { … }
@endcode

### Meyers' Singleton

**Source:** [`src/engine/input/InputSystem.hpp`](src/engine/input/InputSystem.hpp#L306) (line 306)

──────────────────────────────────
The `static InputSystem instance;` inside the function body is a
"magic static" — guaranteed by the C++11 standard to be:
  1. Constructed the first time Get() is called.
  2. Destroyed in reverse construction order at program exit.
  3. Thread-safe during construction (std::call_once semantics).

### Per-Frame Input Snapshot

**Source:** [`src/engine/input/InputSystem.hpp`](src/engine/input/InputSystem.hpp#L354) (line 354)

──────────────────────────────────────────
By reading input once per frame and caching the result, we ensure all
game systems that run during a frame see THE SAME input state.
This prevents subtle bugs where System A processes "key pressed"
and System B processes "key not pressed" in the same frame because
getch() was called at different times.

After PollInput(), the raw key and the resolved InputAction are
available until the next call to PollInput().

@return The raw ncurses key code, or GameKeys::NONE if no key pressed.

### Rebinding at Runtime

**Source:** [`src/engine/input/InputSystem.hpp`](src/engine/input/InputSystem.hpp#L437) (line 437)

──────────────────────────────────────
The binding table is a simple hash map: raw key → action.
Calling Bind(KEY_UP, InputAction::MOVE_UP) and later
        Bind('w',    InputAction::MOVE_UP)
supports both arrow-key and WASD navigation simultaneously.

To load custom bindings from a config file, read each line and call
Bind() with the parsed values.

@param rawKey  ncurses key code or ASCII value.
@param action  The InputAction to trigger when rawKey is pressed.

---

## engine/math

### Why a Custom Math Library?

**Source:** [`src/engine/math/math_types.hpp`](src/engine/math/math_types.hpp#L6) (line 6)

============================================================================
A full engine would link against a battle-tested library like DirectXMath,
GLM, or Eigen.  For teaching purposes we implement just enough math to run
the animation system, keeping each type short and fully annotated.

Key types:
  Vec3   — 3-component float vector (position, scale, translation).
  Quat   — Unit quaternion (rotation).
  Mat4   — Row-major 4×4 float matrix (transform; compatible with D3D11).

### Row-Major vs Column-Major

**Source:** [`src/engine/math/math_types.hpp`](src/engine/math/math_types.hpp#L17) (line 17)

D3D11 HLSL uses row-major matrices by default: a vertex position is a
row-vector and is multiplied as  pos * matrix  (row × matrix).
OpenGL and Vulkan GLSL use column-major: matrix × column-vector.
This file stores Mat4 in row-major order to match D3D11 convention.
When uploading to a D3D11 constant buffer, NO transpose is needed.
(If you later target Vulkan, you will need to transpose the matrices
 or use the GLSL `layout(row_major)` qualifier.)

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Platform: Windows / Linux (no platform-specific intrinsics)

### Separate from DirectX XMFLOAT3

**Source:** [`src/engine/math/math_types.hpp`](src/engine/math/math_types.hpp#L59) (line 59)

DirectXMath (xmfloat3.h) provides SIMD-accelerated variants of this type.
We use a plain struct here so the animation code is readable without
knowing the DirectXMath API.  Performance-critical paths (e.g. 5000-bone
crowds) would switch to XMVECTOR operations.

### Dot product and cross product

**Source:** [`src/engine/math/math_types.hpp`](src/engine/math/math_types.hpp#L87) (line 87)

Dot product: measures how "aligned" two vectors are.
  dot(a, b) = |a||b|cos θ
  dot > 0: angle < 90°; = 0: perpendicular; < 0: angle > 90°

Cross product: produces a vector perpendicular to both inputs.
  Useful for computing normals and right-hand-rule rotations.
-----------------------------------------------------------------------
float Dot(const Vec3& o)   const { return x*o.x + y*o.y + z*o.z; }

### Lerp is correct for translation and scale.  For

**Source:** [`src/engine/math/math_types.hpp`](src/engine/math/math_types.hpp#L114) (line 114)

rotation use Slerp (on Quat) because Lerp on rotation axis-angles
produces non-constant-speed rotation.
-----------------------------------------------------------------------
static Vec3 Lerp(const Vec3& a, const Vec3& b, float t)
{
return a + (b - a) * t;
}

### Why Quaternions Instead of Euler Angles?

**Source:** [`src/engine/math/math_types.hpp`](src/engine/math/math_types.hpp#L138) (line 138)

Euler angles (pitch / yaw / roll) suffer from *gimbal lock*: when two
rotation axes align, one degree of freedom is lost and rotations become
non-intuitive.

Quaternions represent rotations as q = (x, y, z, w) on the 4-D unit
hypersphere (|q| = 1).  They:
  1. Have no gimbal lock.
  2. Interpolate smoothly via SLERP.
  3. Compose efficiently with multiplication (no trigonometry at runtime).

Convention: q = (xi, yj, zk, w)  where w is the scalar part.
Identity (no rotation): q = (0, 0, 0, 1).

### Axis-Angle to Quaternion

**Source:** [`src/engine/math/math_types.hpp`](src/engine/math/math_types.hpp#L173) (line 173)

q = (sin(θ/2)·axis, cos(θ/2))
This is the most intuitive way to construct a rotation quaternion.

### Order matters: (a * b) first applies a, then b.

**Source:** [`src/engine/math/math_types.hpp`](src/engine/math/math_types.hpp#L203) (line 203)

This is the opposite of matrix post-multiplication in some conventions.
-----------------------------------------------------------------------
Quat operator*(const Quat& o) const
{
return {
w*o.x + x*o.w + y*o.z - z*o.y,
w*o.y - x*o.z + y*o.w + z*o.x,
w*o.z + x*o.y - y*o.x + z*o.w,
w*o.w - x*o.x - y*o.y - z*o.z
};
}

### The sandwich product is the standard way to apply a

**Source:** [`src/engine/math/math_types.hpp`](src/engine/math/math_types.hpp#L220) (line 220)

quaternion rotation to a vector.  The result is equivalent to building
a rotation matrix from the quaternion and multiplying — but faster.

### SLERP vs LERP for Rotations

**Source:** [`src/engine/math/math_types.hpp`](src/engine/math/math_types.hpp#L241) (line 241)

LERP (linear interpolation) on quaternions gives correct results only
near t=0 and t=1.  For t in the middle the interpolated rotation can
move at a non-constant angular speed ("ease in / ease out").
SLERP (spherical linear interpolation) always produces constant-speed,
shortest-arc rotation.  It is the standard for animation blending.

When dot(q1, q2) < 0 we negate q2 to ensure the shortest arc (the
quaternion q and -q represent the same rotation but opposite paths on
the 4-D sphere).

### Row-Major and D3D11

**Source:** [`src/engine/math/math_types.hpp`](src/engine/math/math_types.hpp#L294) (line 294)

D3D11 HLSL matrices are row-major by default.  A transform applied to a
row-vector looks like:

  float4 pos = mul(float4(v, 1), worldMatrix);

where worldMatrix's rows are the world-space axes of the object.
This matches how we store Mat4 here: m[row][col].

### Homogeneous Coordinates

**Source:** [`src/engine/math/math_types.hpp`](src/engine/math/math_types.hpp#L303) (line 303)

The 4×4 matrix adds a 4th component (w) to every vector, enabling
translation to be expressed as a linear operation.  Without the w
component, translation would require an addition separate from the
matrix multiply.

A position vector has w=1 (so translation applies).
A direction vector has w=0 (translation does NOT apply).

### Translation in a 4×4 matrix lives in the last column

**Source:** [`src/engine/math/math_types.hpp`](src/engine/math/math_types.hpp#L332) (line 332)

(column-major) or last row (row-major).  In row-major (D3D11):
  1 0 0 0
  0 1 0 0
  0 0 1 0
  tx ty tz 1    ← translation goes in the 4th ROW, columns 0-2

### Quaternion to Rotation Matrix

**Source:** [`src/engine/math/math_types.hpp`](src/engine/math/math_types.hpp#L367) (line 367)

The 3×3 rotation block of the 4×4 matrix can be computed from the
quaternion components without any trigonometric functions:

  R = | 1-2(y²+z²)   2(xy-wz)    2(xz+wy) |
      | 2(xy+wz)    1-2(x²+z²)   2(yz-wx) |
      | 2(xz-wy)    2(yz+wx)    1-2(x²+y²)|

Because quaternions encode the rotation directly, this avoids the
gimbal-lock problem that Euler-angle rotation matrices can suffer from.

### TRS Matrix Order

**Source:** [`src/engine/math/math_types.hpp`](src/engine/math/math_types.hpp#L402) (line 402)

In D3D11 row-major convention (row vectors) the transform order is:
  final = Scale · Rotation · Translation
which means: first scale, then rotate, then translate.
(In column-major / OpenGL convention the order is reversed.)

### Matrix multiply: (A * B)[i][j] = Σ_k A[i][k] * B[k][j]

**Source:** [`src/engine/math/math_types.hpp`](src/engine/math/math_types.hpp#L415) (line 415)

-----------------------------------------------------------------------
Mat4 operator*(const Mat4& o) const
{
Mat4 res;
for (int row = 0; row < 4; ++row)
for (int col = 0; col < 4; ++col)
{
float sum = 0.0f;
for (int k = 0; k < 4; ++k)
sum += m[row][k] * o.m[k][col];
res.m[row][col] = sum;
}
return res;
}

### D3D11 expects a contiguous array of 16 floats in

**Source:** [`src/engine/math/math_types.hpp`](src/engine/math/math_types.hpp#L433) (line 433)

row-major order, which is exactly what std::array<std::array<float,4>,4>
provides (both arrays are contiguous).
-----------------------------------------------------------------------
const float* Data() const { return &m[0][0]; }
};

---

## engine/physics

### CharacterVirtual Deep Dive

**Source:** [`src/engine/physics/character_controller.cpp`](src/engine/physics/character_controller.cpp#L6) (line 6)

============================================================================

JPH::CharacterVirtual solves character movement with a "predict, move,
resolve" loop:

  1. Predict contacts: shape-cast the capsule in the desired move direction
     looking ahead by mPredictiveContactDistance (default 0.1 m).

  2. Resolve penetration: if the capsule is already overlapping geometry,
     push it out along the contact normal at mPenetrationRecoverySpeed
     (default 1.0 m/s).

  3. Step up: attempt to step over ledges up to mMaxStepHeight (we set
     this to 0.35 m = typical stair riser height in FFXV).

  4. Move: apply the resolved velocity to the capsule position.

  5. Classify ground: query the surface directly below the capsule to
     determine OnGround, OnSteepGround, or InAir state.

The result is stable, jitter-free movement that handles typical game
geometry without manual tuning of per-contact friction coefficients.

─── Coordinate convention ──────────────────────────────────────────────
This engine uses Y-up.  Gravity is -Y.  Forward is +Z (like D3D/HLSL).
Jolt uses the same Y-up convention by default.

─── Layer used for the character ───────────────────────────────────────
The character body uses kLayerMoving so it collides with both static
geometry (floors, walls) and other moving bodies (dynamic crates).

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Platform: All

### CapsuleShape for the character

**Source:** [`src/engine/physics/character_controller.cpp`](src/engine/physics/character_controller.cpp#L98) (line 98)

-----------------------------------------------------------------------
We create a CapsuleShape and wrap it in a RotatedTranslatedShape to
offset the capsule origin to the character's feet rather than its centre.

Jolt's capsule is centred at the origin.  By translating it up by
(halfHeight + radius), the capsule's bottom hemisphere sits at Y=0,
which corresponds to the character's foot position.  This makes
GetPosition() return the foot position — convenient for ground
detection and surface snapping.
-----------------------------------------------------------------------
JPH::CapsuleShapeSettings capsuleSettings(halfHeight, radius);
auto capsuleResult = capsuleSettings.Create();
if (capsuleResult.HasError())
{
LOG_ERROR("[CharacterController] CapsuleShape failed: "
<< capsuleResult.GetError());
return false;
}

### CharacterVirtualSettings

**Source:** [`src/engine/physics/character_controller.cpp`](src/engine/physics/character_controller.cpp#L134) (line 134)

-----------------------------------------------------------------------
Key tuning parameters:
  mMaxSlopeAngle             — 50° gives natural FF15 feel (walk most slopes).
  mMaxStrength               — Push force against dynamic bodies (500 N).
  mMass                      — Used for reaction forces only (not gravity).
  mPenetrationRecoverySpeed  — How quickly overlap is resolved (0.5 m/s).
  mPredictiveContactDistance — Probe ahead 0.1 m for smooth pre-contact.
  mMaxNumHits                — Max contacts per step (256 is generous).
-----------------------------------------------------------------------
JPH::CharacterVirtualSettings settings;
settings.mShape                   = rtResult.Get();
settings.mMaxSlopeAngle           = JPH::DegreesToRadians(50.0f);
settings.mMaxStrength             = 500.0f;
settings.mMass                    = 80.0f;        // ~adult human mass
settings.mPenetrationRecoverySpeed= 0.5f;
settings.mPredictiveContactDistance= 0.1f;
settings.mMaxNumHits              = 256u;
settings.mUp                      = JPH::Vec3::sAxisY();

### CharacterVirtual cleanup

**Source:** [`src/engine/physics/character_controller.cpp`](src/engine/physics/character_controller.cpp#L174) (line 174)

CharacterVirtual is reference-counted via JPH::Ref<>.
Resetting the Ref releases the reference; Jolt deletes the object
when the count reaches zero.  No explicit "remove from world" call
is needed because CharacterVirtual is not a true rigid body.
m_impl->character = nullptr;
m_initialised     = false;
}

### Applying Gravity

**Source:** [`src/engine/physics/character_controller.cpp`](src/engine/physics/character_controller.cpp#L192) (line 192)

-----------------------------------------------------------------------
CharacterVirtual does NOT automatically apply gravity — that is the
game code's responsibility.  We accumulate a vertical velocity
component and add it to the horizontal input each frame.

When grounded we reset the vertical velocity to a small negative value
(−0.1 m/s) to keep the character "pressed" against the ground, which
helps the ground detection stay reliable on gentle slopes.
-----------------------------------------------------------------------
const bool grounded = IsGrounded();

### Jump Impulse

**Source:** [`src/engine/physics/character_controller.cpp`](src/engine/physics/character_controller.cpp#L211) (line 211)

We set the vertical velocity directly to kJumpImpulse.
This is an instantaneous impulse (v = sqrt(2gh) for max height h).
At kJumpImpulse = 5.0 m/s and g = 9.81 m/s², peak height ≈ 1.27 m.
m_velocity.y = kJumpImpulse;
}
}
else
{
Airborne: integrate gravity.
m_velocity.y -= kGravity * dt;
}

### CharacterVirtual::Update() parameters

**Source:** [`src/engine/physics/character_controller.cpp`](src/engine/physics/character_controller.cpp#L232) (line 232)

-----------------------------------------------------------------------
deltaTime    — time step
gravity      — world-space gravity vector (overrides built-in gravity)
settings     — per-update overrides (nullptr = use creation settings)
inCenterOfMassTransform — used for broad-phase; pass identity
broadPhaseLayerFilter   — which broadphase layers to test
objectLayerFilter       — which object layers to test
bodyFilter              — per-body filter (nullptr = collide with all)
shapeFilter             — per-shape filter (nullptr = all shapes)
allocator               — temp allocator from the PhysicsSystem

We pass nullptr for the optional filters — the character collides with
all static and moving bodies.
-----------------------------------------------------------------------
auto& impl = *world.GetImpl();

### Pass-through Collision Filters

**Source:** [`src/engine/physics/character_controller.cpp`](src/engine/physics/character_controller.cpp#L249) (line 249)

We want the character to collide with all static and dynamic bodies.
Providing simple "allow all" implementations of the BroadPhaseLayerFilter
and ObjectLayerFilter abstract base classes achieves this cleanly and
works across all Jolt Physics versions without requiring helper utilities.
struct AllBroadPhase final : public JPH::BroadPhaseLayerFilter {
bool ShouldCollide(JPH::BroadPhaseLayer) const override { return true; }
};
struct AllObjectLayer final : public JPH::ObjectLayerFilter {
bool ShouldCollide(JPH::ObjectLayer) const override { return true; }
};

### GroundState

**Source:** [`src/engine/physics/character_controller.cpp`](src/engine/physics/character_controller.cpp#L298) (line 298)

EOnGround       → standing on a walkable surface
EOnSteepGround  → touching a surface too steep to walk (slope > maxSlopeAngle)
ENotSupported   → no contact (CharacterVirtualSettings::mSupportingVolume)
EInAir          → no ground contact at all
return m_impl->character->GetGroundState()
== JPH::CharacterVirtual::EGroundState::OnGround;
}

### What is a Character Controller?

**Source:** [`src/engine/physics/character_controller.hpp`](src/engine/physics/character_controller.hpp#L6) (line 6)

============================================================================
In games like FFXV, the player character (Noctis) needs special collision
behaviour that a plain rigid body cannot provide cleanly:

  • Gravity   — character falls under gravity when airborne.
  • Step-up   — smoothly climbs over small ledges (0.25 m) without
                bouncing or getting stuck.
  • Slopes    — slides along sloped surfaces up to a max angle; stops
                on steep slopes (e.g. vertical walls).
  • No spin   — remains upright; never tumbles like a dynamic body.
  • Jump      — player-initiated vertical impulse.
  • Grounded  — accurate IsGrounded() query (for jump gating, footstep
                sounds, landing animations).

A plain rigid body would spin when bumped, bounce off walls unpredictably,
and require complex constraint fixes to stay upright.

─── Jolt's CharacterVirtual ─────────────────────────────────────────────
Jolt Physics provides JPH::CharacterVirtual, a "virtual" (non-simulated)
character that has no mass in the solver but uses shape-cast queries to
detect and respond to obstacles each Update() call.

  vs. JPH::Character (rigid body with constraints):
    • CharacterVirtual: cheaper, more predictable, easier to tune.
    • Character (rigid body): reacts to explosions/physics, but wobbles.
  FFXV-style games almost always use the virtual type for the player.

─── CharacterVirtualSettings key fields ─────────────────────────────────
  shape             — CapsuleShape (halfHeight, radius).
  maxSlopeAngle     — Walk angle limit (default 50°); steeper = slide.
  maxStrength       — Force the character can exert on dynamic bodies.
  mass              — Used for push-back on dynamic bodies.
  predictiveContactDistance — How far ahead to probe for contacts.
  penetrationRecoverySpeed  — How fast to resolve overlap (0 = instant).

─── pImpl Pattern ──────────────────────────────────────────────────────
CharacterController uses a pImpl pointer (Impl forward-declared here) so
Jolt headers are included only in character_controller.cpp, keeping
compile times short for all other engine files.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Platform: All (Jolt is cross-platform)

### CharacterController Lifecycle

**Source:** [`src/engine/physics/character_controller.hpp`](src/engine/physics/character_controller.hpp#L80) (line 80)

============================================================================

1. Call Init() once per entity.  Creates the JPH::CharacterVirtual and
   positions it at startPos.

2. Each frame:
   a. Compute a desired velocity from player input (WASD + sprint/jump).
   b. Call Update(world, dt, desiredVelocity, wantJump).
      Internally this calls CharacterVirtual::Update() which:
        i.  Applies gravity to the vertical velocity component.
        ii. Shape-casts the capsule in the movement direction.
        iii.Steps over ledges up to mMaxStepHeight (default 0.4 m).
        iv. Slides along walls and slopes.
        v.  Resolves penetration with mPenetrationRecoverySpeed.
   c. Read GetPosition() and write it into TransformComponent for rendering.
   d. Use IsGrounded() to gate jump triggers and landing animations.

3. Call Shutdown() when the entity is destroyed (removes the body).

─── Units ──────────────────────────────────────────────────────────────
Jolt uses SI units (metres, seconds, kg).  FFXV's open world is in metres.
Typical parameters:
  halfHeight  = 0.85 m   (1.7 m total character height)
  radius      = 0.3  m   (0.6 m diameter capsule)
  jumpImpulse = 5.0  m/s  (gives ~1.3 m peak jump height)

============================================================================

### IsGrounded states

**Source:** [`src/engine/physics/character_controller.hpp`](src/engine/physics/character_controller.hpp#L187) (line 187)

Jolt's CharacterVirtual reports one of three states:
  OnGround       — standing on a surface ≤ maxSlopeAngle.
  OnSteepGround  — standing on a slope > maxSlopeAngle (slides down).
  InAir          — no contact below.
We return true only for OnGround (suitable for jump gating).

### Why AABB and Not Physics Bodies?

**Source:** [`src/engine/physics/hit_volume.cpp`](src/engine/physics/hit_volume.cpp#L5) (line 5)

Using Jolt rigid bodies for hit/hurt volumes would work but introduces
overhead: body creation, broadphase insertion, contact callbacks.
For teaching, AABB overlap is easier to understand and sufficient
for a melee combat demo.

In a shipping game you might use:
  • Jolt CollideShape() with BoxShape — accurate OBB for rotated weapons.
  • Spatial hash grid — O(1) average lookup for many simultaneous volumes.
  • Physics layers ATTACK / HURT — let Jolt's broadphase do the filtering.

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Platform: All

### erase-remove idiom

**Source:** [`src/engine/physics/hit_volume.cpp`](src/engine/physics/hit_volume.cpp#L51) (line 51)

std::remove_if moves matching elements to the end of the vector and
returns an iterator to the first "removed" element.  erase() then
removes them.  This is the idiomatic C++ way to remove from a vector.
m_volumes.erase(
std::remove_if(m_volumes.begin(), m_volumes.end(),
[id](const HitVolume& v){ return v.id == id; }),
m_volumes.end()
);
}

### AABB Overlap Test

**Source:** [`src/engine/physics/hit_volume.cpp`](src/engine/physics/hit_volume.cpp#L115) (line 115)

Two AABBs overlap on all three axes simultaneously.
For each axis, the overlap condition is:
  |centreA - centreB| <= halfA + halfB
If this fails for ANY axis, the boxes are separated (no overlap).
const float dx = std::fabs(a.centre.x - b.centre.x);
const float dy = std::fabs(a.centre.y - b.centre.y);
const float dz = std::fabs(a.centre.z - b.centre.z);

### Physics-Based Hit Detection

**Source:** [`src/engine/physics/hit_volume.hpp`](src/engine/physics/hit_volume.hpp#L6) (line 6)

============================================================================
In FFXV, when Noctis swings his sword, the game doesn't trace a ray from
the blade tip — it creates a "hit volume" (also called an "attack box" or
"hitbox") that follows the sword through space.  Any "hurt volume" (the
defender's body region that can receive damage) that overlaps the attack
box is considered hit.

This two-volume approach has several advantages over raycasting:

  • Wide weapons — a broadsword is wide; a ray from the tip misses targets
    to the sides.  An attack box captures the full blade width.

  • Simultaneous hits — one swing can hit multiple enemies at once without
    needing multiple ray tests.

  • Multi-hit blocking — a shield can block only if its hurt volume overlaps
    the attacker's hit volume.

─── Types of volumes ───────────────────────────────────────────────────────

  HitVolumeType::Attack — created by the attacker (sword, fist, AoE spell).
                          When active, any overlapping Hurt volume is hit.

  HitVolumeType::Hurt   — permanent bounding region of a damageable body.
                          Multiple hurt zones per character (head, torso,
                          legs) enable location-based damage.

─── Implementation Note ───────────────────────────────────────────────────
We implement this with simple axis-aligned box (AABB) overlap tests in
C++ without creating Jolt rigid bodies.  For a shipping game you would use
Jolt's NarrowPhaseQuery::CollideShape() with BoxShape for accurate oriented
box collisions.  AABB is shown here because it is easy to understand and
sufficient for teaching.

─── Usage Example ─────────────────────────────────────────────────────────
  // Register permanent hurt volumes for each enemy.
  HitVolumeID hurtID = hvMgr.Register(HitVolumeType::Hurt, enemyEntity,
                                       {0.4f, 0.9f, 0.4f});

  // On each melee frame, activate the attack volume for the player's sword.
  HitVolumeID atkID  = hvMgr.Register(HitVolumeType::Attack, playerEntity,
                                       {0.1f, 0.5f, 0.8f});   // sword shape
  hvMgr.SetActive(atkID, true);

  // Each frame, update the world positions from entity transforms.
  hvMgr.Update(playerEntity, playerTransform.position + swordOffset);

  // Query overlaps.
  std::vector<EntityID> hitEntities;
  hvMgr.QueryOverlaps(atkID, hitEntities);

  // When swing finishes, remove the attack volume.
  hvMgr.Unregister(atkID);

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Platform: All (no platform-specific code — pure AABB math)

### Manager Ownership Model

**Source:** [`src/engine/physics/hit_volume.hpp`](src/engine/physics/hit_volume.hpp#L128) (line 128)

============================================================================
HitVolumeManager owns all registered volumes.  Callers receive a
HitVolumeID handle and use it to control the volume:
  Register()     — create a volume, get its ID.
  Unregister()   — destroy the volume by ID.
  SetActive()    — toggle whether the volume participates in queries.
  Update()       — move the volume to match the entity's transform.
  QueryOverlaps()— find all entities whose Hurt volumes overlap an Attack.

The manager is intentionally simple (linear array of volumes with linear
overlap scan).  For a large open-world scene with hundreds of simultaneous
combat volumes you would use a spatial acceleration structure (BVH or grid).
============================================================================

### Activation toggle

**Source:** [`src/engine/physics/hit_volume.hpp`](src/engine/physics/hit_volume.hpp#L173) (line 173)

Attack volumes should only be active during the relevant frames of the
attack animation (e.g. frames 8–18 of a 30-frame swing).  Deactivating
them for the rest of the swing avoids false hits on the recovery phase.

@param id      Volume ID.
@param active  True = participates in queries; false = ignored.

### AABB overlap test

**Source:** [`src/engine/physics/hit_volume.hpp`](src/engine/physics/hit_volume.hpp#L196) (line 196)

Two AABBs [cA ± hA] and [cB ± hB] overlap when:
  |cA.x - cB.x| ≤ hA.x + hB.x   (for all three axes)
This is O(N²) over active volumes — fast enough for a small combat scene.

@param attackID   ID of an Attack-type volume.
@param outEntities Receives the EntityID of each entity whose Hurt volume
                   overlaps the attack volume.  May contain duplicates if
                   an entity has multiple hurt volumes.

### Internal Header Pattern

**Source:** [`src/engine/physics/physics_impl.hpp`](src/engine/physics/physics_impl.hpp#L6) (line 6)

============================================================================
This file is an *internal* header: it is included ONLY by physics/ .cpp
files, never by external engine code or game systems.

Why have an internal header at all?
------------------------------------
The PhysicsWorldImpl struct and the Jolt layer-filter classes must be
visible to *multiple* physics .cpp files:
  • physics_world.cpp   — defines PhysicsWorldImpl and the filter classes.
  • character_controller.cpp — accesses PhysicsWorldImpl members to call
    JPH::DefaultBroadPhaseLayerFilter and PhysicsSystem.
  • raycast.cpp         — accesses PhysicsWorldImpl::physicsSystem.

Placing these definitions in a .cpp file would make them invisible to other
translation units.  Placing them in the public physics_world.hpp would
expose Jolt headers to ALL engine code — slow compile times.

The internal-header pattern solves both problems:
  • Jolt headers are isolated to #include "physics_impl.hpp" in the .cpp.
  • All physics .cpp files share the same struct layout.
  • External code only sees the public physics_world.hpp (no Jolt types).

Convention: internal headers live alongside the implementation .cpp files
and use the suffix _impl.hpp.  Do NOT include them from .hpp public headers.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Platform: All (included only from Jolt-enabled builds)

### Jolt Physics Initialisation Order

**Source:** [`src/engine/physics/physics_world.cpp`](src/engine/physics/physics_world.cpp#L6) (line 6)

============================================================================
Jolt Physics requires a specific initialisation sequence that may look
verbose at first but each step has a purpose:

1. JPH::RegisterDefaultAllocator()
   Registers Jolt's default malloc/free-based memory allocator.
   You can replace this with a custom allocator (arena, pool) for
   improved performance in a shipping title.

2. JPH::Factory::sInstance = new JPH::Factory()
   Creates the global type factory used to look up RTTI for shapes,
   bodies, and constraints.  Required before any Jolt object is created.

3. JPH::RegisterTypes()
   Registers all built-in Jolt types (BoxShape, SphereShape, etc.)
   with the factory.  Must come after step 2.

4. TempAllocatorImpl
   A fixed-size scratch allocator Jolt uses for per-frame temporary work
   (contact manifolds, constraint solver working sets).  10 MB is
   sufficient for most games.

5. JobSystemThreadPool
   Jolt's multi-threaded job system.  Uses std::thread internally.
   We allow it to use all available hardware threads for maximum throughput.
   On a single-core CI machine it falls back gracefully to 1 thread.

6. BroadPhaseLayerInterface + Object layer filters
   These small classes tell Jolt which object layers exist and which
   pairs should be tested for collision.  Our two-layer setup
   (NON_MOVING and MOVING) is the standard minimal configuration.
   They are defined in physics_impl.hpp so character_controller.cpp
   can also access them.

7. PhysicsSystem::Init(...)
   Allocates the internal body array (up to maxBodies), body pair cache,
   contact cache, and constraint solver buffers.

─── Memory Budget ──────────────────────────────────────────────────────────
The numbers below are tuned for a small demo scene:
  maxBodies            = 65 536   (plenty for an open-world zone)
  numBodyMutexes       = 0        (0 = auto-choose)
  maxBodyPairs         = 65 536
  maxContactConstraints= 10 240
  tempAllocatorSize    = 10 MB
Scale these up for a shipping title with thousands of rigid bodies.

─── Reference ──────────────────────────────────────────────────────────────
Jolt Physics GitHub: https://github.com/jrouwe/JoltPhysics
Jolt "Hello World" sample: JoltPhysics/Samples/HelloWorld/HelloWorld.cpp
============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Platform: All (Jolt is cross-platform)

### Internal header

**Source:** [`src/engine/physics/physics_world.cpp`](src/engine/physics/physics_world.cpp#L69) (line 69)

---------------------------------------------------------------------------
physics_impl.hpp provides the PhysicsWorldImpl struct definition, the
Jolt layer-filter classes, and all Jolt core headers.  It is only
included by physics/ .cpp files (never by external code).
---------------------------------------------------------------------------
include "engine/physics/physics_impl.hpp"

### Jolt Trace / Assert callbacks

**Source:** [`src/engine/physics/physics_world.cpp`](src/engine/physics/physics_world.cpp#L101) (line 101)

Jolt calls JPH_Trace (a global function pointer) for internal log messages.
In debug builds Jolt also calls JPH_AssertFailed on assertion failures.
We route them through our Logger so all output goes to one place.
static void JoltTraceImpl(const char* fmt, ...)
{
char buf[512];
va_list args;
va_start(args, fmt);
std::vsnprintf(buf, sizeof(buf), fmt, args);
va_end(args);
LOG_DEBUG("[Jolt] " << buf);
}

### Step 1: Register the default allocator

**Source:** [`src/engine/physics/physics_world.cpp`](src/engine/physics/physics_world.cpp#L149) (line 149)

-----------------------------------------------------------------------
Jolt's RegisterDefaultAllocator() sets up the global allocator function
pointers (Allocate, Free, AlignedAllocate, AlignedFree) to the standard
malloc/free equivalents.  In a shipping engine you would plug in a
custom pool allocator here for better cache performance.
-----------------------------------------------------------------------
JPH::RegisterDefaultAllocator();

### Step 2: Trace / assert callbacks

**Source:** [`src/engine/physics/physics_world.cpp`](src/engine/physics/physics_world.cpp#L159) (line 159)

-----------------------------------------------------------------------
JPH::Trace = JoltTraceImpl;
JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = JoltAssertFailed;)

### Step 3: Create the type factory

**Source:** [`src/engine/physics/physics_world.cpp`](src/engine/physics/physics_world.cpp#L165) (line 165)

-----------------------------------------------------------------------
if (!JPH::Factory::sInstance)
JPH::Factory::sInstance = new JPH::Factory();

### Step 4: Register all built-in Jolt types

**Source:** [`src/engine/physics/physics_world.cpp`](src/engine/physics/physics_world.cpp#L171) (line 171)

-----------------------------------------------------------------------
JPH::RegisterTypes();

### Step 5: Temp allocator (10 MB scratch pool)

**Source:** [`src/engine/physics/physics_world.cpp`](src/engine/physics/physics_world.cpp#L176) (line 176)

-----------------------------------------------------------------------
constexpr size_t kTempAllocBytes = 10u * 1024u * 1024u;
m_impl->tempAllocator =
std::make_unique<JPH::TempAllocatorImpl>(kTempAllocBytes);

### Step 6: Job system

**Source:** [`src/engine/physics/physics_world.cpp`](src/engine/physics/physics_world.cpp#L183) (line 183)

-----------------------------------------------------------------------
Use hardware_concurrency()-1 threads; clamp to at least 1.
const int numThreads =
std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1);

### Step 7: PhysicsSystem

**Source:** [`src/engine/physics/physics_world.cpp`](src/engine/physics/physics_world.cpp#L196) (line 196)

-----------------------------------------------------------------------
constexpr JPH::uint kMaxBodies             = 65536u;
constexpr JPH::uint kNumBodyMutexes        = 0u;
constexpr JPH::uint kMaxBodyPairs          = 65536u;
constexpr JPH::uint kMaxContactConstraints = 10240u;

### Physics Step and Sub-Steps

**Source:** [`src/engine/physics/physics_world.cpp`](src/engine/physics/physics_world.cpp#L228) (line 228)

A single physics step with large dt can cause "tunnelling".
Using substeps=1 at 60 FPS (dt=0.0167 s) is stable for typical objects.
m_impl->physicsSystem->Update(
dt,
substeps,
m_impl->tempAllocator.get(),
m_impl->jobSystem.get()
);
}

### Shutdown Order

**Source:** [`src/engine/physics/physics_world.cpp`](src/engine/physics/physics_world.cpp#L246) (line 246)

Remove all bodies before destroying the PhysicsSystem.
auto& bi = m_impl->BodyInterface();
JPH::BodyIDVector allBodies;
m_impl->physicsSystem->GetBodies(allBodies);
for (const JPH::BodyID& id : allBodies)
{
bi.RemoveBody(id);
bi.DestroyBody(id);
}

### Body Creation Pattern

**Source:** [`src/engine/physics/physics_world.cpp`](src/engine/physics/physics_world.cpp#L275) (line 275)

Every Jolt body is created in three steps:
  1. Create a Shape (BoxShape, SphereShape, …).
  2. Build BodyCreationSettings — position, rotation, layer, motion type.
  3. Call BodyInterface::CreateAndAddBody().

### Explicit Mass Override

**Source:** [`src/engine/physics/physics_world.cpp`](src/engine/physics/physics_world.cpp#L303) (line 303)

By default Jolt derives mass from the shape's density (uniform 1000 kg/m³).
To match the caller-supplied mass we set CalculateInertia so Jolt still
computes a physically correct inertia tensor for this shape, but uses our
mass value instead of a density estimate.  Static bodies ignore mass.
if (!isStatic && mass > 0.0f)
{
settings.mOverrideMassProperties =
JPH::EOverrideMassProperties::CalculateInertia;
settings.mMassPropertiesOverride.mMass = mass;
}

### Jolt RayCast

**Source:** [`src/engine/physics/physics_world.cpp`](src/engine/physics/physics_world.cpp#L457) (line 457)

JPH::RRayCast direction vector length = the maximum ray distance.
const JPH::RVec3 joltOrigin(origin.x, origin.y, origin.z);
const JPH::Vec3  joltDir(direction.x * maxDist,
direction.y * maxDist,
direction.z * maxDist);

### What is a Physics Engine?

**Source:** [`src/engine/physics/physics_world.hpp`](src/engine/physics/physics_world.hpp#L6) (line 6)

============================================================================
A physics engine simulates the behaviour of physical objects in a virtual
world.  For a game like Final Fantasy XV, physics handles:

  • Gravity          — characters and objects fall to the ground.
  • Collision        — characters don't walk through walls or floors.
  • Raycasts         — "line of sight" queries; ground detection.
  • Rigid bodies     — dynamic objects (crates, boulders) react to forces.
  • Character capsule — the player moves along slopes and over ledges.
  • Hit volumes      — attack shapes overlap hurt shapes for combat.

─── Why Jolt Physics? ──────────────────────────────────────────────────────
Jolt Physics (MIT licence, by Jorrit Rouwe) is a modern deterministic
multi-threaded physics library used in Horizon Forbidden West.  It is:

  • Cross-platform: Windows, Linux, macOS, consoles.
  • Cache-friendly: uses a SoA (Structure of Arrays) internal layout.
  • Deterministic: fixed-precision simulation for replay / network sync.
  • Well-documented: the source ships with extensive comments.

─── Architecture: Wrapper Pattern ─────────────────────────────────────────
PhysicsWorld wraps the raw Jolt API behind a simple engine-friendly
interface.  Engine code (ECS systems, gameplay) uses PhysicsWorld methods
(uint32_t body IDs, math::Vec3 positions) without seeing Jolt types.
Only the physics .cpp files include <Jolt/…> headers.

This is the *Facade* design pattern — one of the most useful tools for
managing third-party library coupling.

─── Body ID Strategy ───────────────────────────────────────────────────────
Jolt uses JPH::BodyID, which is a uint32_t holding an index+sequence.
We expose that uint32_t directly in the public API so callers never
need to include Jolt headers.  0xFFFFFFFF is the "invalid" sentinel.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Platform: All (Jolt is cross-platform)

### pImpl (Pointer to IMPLementation) Pattern

**Source:** [`src/engine/physics/physics_world.hpp`](src/engine/physics/physics_world.hpp#L60) (line 60)

By forward-declaring PhysicsWorldImpl and storing it as a unique_ptr<Impl>,
callers who #include this header never see the Jolt types.  This means:
  1. Compile times are shorter — Jolt headers are only parsed in .cpp files.
  2. The public interface is stable — changing Jolt internals doesn't force
     a recompile of all consumers of this header.
  3. The ABI is stable — adding fields to Impl doesn't change sizeof(World).
---------------------------------------------------------------------------
namespace engine::physics::detail { struct PhysicsWorldImpl; }

### Raycast Anatomy

**Source:** [`src/engine/physics/physics_world.hpp`](src/engine/physics/physics_world.hpp#L88) (line 88)

A raycast fires a mathematical ray (origin + direction × length) through the
physics world and returns the *first* solid surface it touches.  The result
contains:
  • distance   — how far along the ray the hit occurred (0 = at origin).
  • position   — world-space contact point.
  • normal     — outward surface normal at the contact point.
  • bodyID     — which body was hit (uint32_t = Jolt BodyID opaque value).

Typical uses:
  • Ground detection for character controller.
  • Projectile hit detection.
  • Line-of-sight checks for AI.

### Physics World Lifecycle

**Source:** [`src/engine/physics/physics_world.hpp`](src/engine/physics/physics_world.hpp#L120) (line 120)

============================================================================

1. Call Init() once at scene startup.
   This allocates the TempAllocator, JobSystem, and PhysicsSystem.
   Without Init() all other methods are undefined behaviour.

2. Each game frame, call Step(dt).
   dt is the elapsed seconds since the last frame.
   Jolt advances the simulation by exactly that time, stepping up to
   kMaxSubSteps sub-steps internally for stability.

3. After Step(), read body positions via GetPosition() and copy them
   into your ECS TransformComponent to update the visible scene.

4. Call Shutdown() when the scene is destroyed.
   This removes all bodies and frees the Jolt objects.

─── Object Layers ──────────────────────────────────────────────────────────
Jolt uses "object layers" to control which objects collide with which.
We define two layers:

  kLayerNonMoving (0) — static geometry: floors, walls, terrain.
  kLayerMoving    (1) — dynamic objects: characters, projectiles, crates.

Moving objects collide with everything.
Non-moving objects only collide with moving objects (not each other).
This is the standard two-layer setup described in the Jolt samples.

─── Thread Safety ──────────────────────────────────────────────────────────
PhysicsWorld is NOT thread-safe.  Create/destroy bodies and call Step()
from the same thread (typically the main game thread).  Jolt's *internal*
update is multi-threaded via its JobSystem; external calls are single-thread.

============================================================================

### Explicit Sub-Steps

**Source:** [`src/engine/physics/physics_world.hpp`](src/engine/physics/physics_world.hpp#L210) (line 210)

This wrapper exposes sub-stepping as an explicit caller-controlled
parameter.  The default is a single simulation step per frame
(`substeps = 1`).  Callers that need extra stability for a larger `dt`
can request more sub-steps explicitly (e.g. `substeps = 2` when
`dt > 1/30 s`).  The sub-step count is passed directly to
`PhysicsSystem::Update()` without further clamping.

@param dt        Elapsed seconds (typically 1/60 for a 60 FPS game).
@param substeps  Number of simulation sub-steps to execute. Default: 1.

### Box Shape

**Source:** [`src/engine/physics/physics_world.hpp`](src/engine/physics/physics_world.hpp#L235) (line 235)

halfExtents is the half-size along each axis, so a 1×1×1 cube has
halfExtents = (0.5, 0.5, 0.5).  This is the convention used by most
physics engines (Jolt, Bullet, PhysX).

@param pos          World-space centre position.
@param halfExtents  Half-size on each axis (metres).
@param mass         Mass in kilograms.  Ignored when isStatic = true.
@param isStatic     True = immovable geometry; false = dynamic body.
@return Opaque body ID (uint32_t) or kInvalidBodyID on failure.

### Capsule Shape for Characters

**Source:** [`src/engine/physics/physics_world.hpp`](src/engine/physics/physics_world.hpp#L264) (line 264)

A capsule is a cylinder with hemispherical caps.  It is the standard
shape for character controllers because:
  • No sharp edges → slides smoothly off walls and slopes.
  • The rounded bottom → naturally steps up small ledges.
  • Stable standing orientation → doesn't tip over.

halfHeight is the half-length of the *cylindrical* part only.
The total height = 2 * (halfHeight + radius).

@param pos         World-space centre (bottom of the capsule).
@param halfHeight  Half-height of the cylindrical section (metres).
@param radius      Capsule radius (metres).
@param mass        Mass (kg).
@param isStatic    True = static.
@return Opaque body ID or kInvalidBodyID.

### Why Expose Internals?

**Source:** [`src/engine/physics/physics_world.hpp`](src/engine/physics/physics_world.hpp#L332) (line 332)

CharacterController and HitVolumeManager need access to Jolt's
BodyInterface and NarrowPhaseQuery.  Rather than duplicating every
needed method on PhysicsWorld, we expose the Impl pointer to classes
that live in the same physics/ module.  External code (ECS systems,
gameplay) must NOT call this method.

### Thin Wrappers

**Source:** [`src/engine/physics/raycast.cpp`](src/engine/physics/raycast.cpp#L5) (line 5)

CastRay and CastSphere are thin wrappers over PhysicsWorld methods and
Jolt's NarrowPhaseQuery API.  Keeping them separate from physics_world.cpp
demonstrates the Single Responsibility Principle: physics_world.cpp manages
body lifecycle; raycast.cpp handles query logic.

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Platform: All

### Sphere shape cast via Jolt

**Source:** [`src/engine/physics/raycast.cpp`](src/engine/physics/raycast.cpp#L67) (line 67)

We create a temporary SphereShape and use the NarrowPhaseQuery's
CastShape() to sweep it along the ray.

ShapeCastSettings controls:
  mBackFaceModeTriangles  — whether back faces count as hits (we disable).
  mUseShrunkenShapeAndConvexRadius — use exact sphere surface (on by default).
JPH::SphereShape sphereShape(radius);
sphereShape.SetEmbedded();  // prevent auto-deletion

### Raycasting in Games

**Source:** [`src/engine/physics/raycast.hpp`](src/engine/physics/raycast.hpp#L6) (line 6)

============================================================================
A raycast is a fundamental building block in game physics.  It answers the
question "what is the first solid object in direction D from point O?"

Common uses in an FFXV-style engine:

  Ground detection    — Character controller fires a ray downward to find
                        the exact ground height for foot IK (planting feet
                        on uneven terrain).

  Line of sight       — AI fires a ray between enemy and player to check
                        whether any obstacle blocks the view.

  Shooting / targeting— A bullet fires a ray from the gun muzzle in the
                        aim direction.  The first hit is the target.

  Hit detection       — Melee attack areas can be queries as box shape casts.

  Camera collision    — The camera fires a ray from the player to the ideal
                        camera position; if it hits geometry the camera pulls
                        in to avoid clipping through walls.

─── RayCast vs ShapeCast ───────────────────────────────────────────────
  RayCast   — infinitely thin ray (zero radius).  Fastest query.
  ShapeCast — sweeps a shape (sphere, box, capsule) along a ray.
              Useful when you want a "thick" test (e.g. "is there room for
              the character capsule to walk here?").

This file provides both.
============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Platform: All (no platform-specific code)

### Fraction vs Distance

**Source:** [`src/engine/physics/raycast.hpp`](src/engine/physics/raycast.hpp#L64) (line 64)

Jolt returns a fraction in [0, 1] where 1 = full sweep length.
We convert to a world-space distance (metres) for convenience:
  distance = fraction × sweepLength

### Normalised Direction

**Source:** [`src/engine/physics/raycast.hpp`](src/engine/physics/raycast.hpp#L85) (line 85)

The direction vector MUST be normalised (length == 1).  If you have a
direction from A to B, normalise it first:
  Vec3 dir = B - A;  float len = dir.Length();  dir = dir / len;
Then pass len as maxDist.

@param world      The physics world to query.
@param origin     Ray start (world space).
@param direction  Normalised direction vector (must have length ~= 1).
@param maxDist    Maximum ray distance (metres).
@param outHit     Populated when hit == true.
@return True if a hit was found.

### Ground Detection Pattern

**Source:** [`src/engine/physics/raycast.hpp`](src/engine/physics/raycast.hpp#L107) (line 107)

This helper encapsulates the most common raycast use-case in action games:
finding the ground height directly below a point.  It fires a ray in the
-Y direction and returns the hit distance.

Use cases:
  • Foot IK: fire from each ankle, plant the foot at hit.position.
  • Camera: determine if camera has ground underneath.
  • NavMesh probe: snap a pathfinding node to the terrain surface.

@param world    The physics world.
@param origin   Point to probe from (world space).
@param maxDist  How far down to search (metres).
@param outHit   Populated when hit == true.
@return True if ground was found within maxDist.

### Sphere Sweep for Thick Queries

**Source:** [`src/engine/physics/raycast.hpp`](src/engine/physics/raycast.hpp#L135) (line 135)

A sphere sweep is equivalent to a "fat" ray — it tests whether a sphere of
the given radius can travel from origin in direction without hitting anything.
Use this when you need a "is there room for object of radius R" test.

Example: Camera spring-arm
  Sweep a sphere of radius 0.3 m from the player's shoulder to the desired
  camera position.  If it hits, the camera position is the hit point.

@param world      The physics world.
@param origin     Sweep start (sphere centre).
@param direction  Normalised sweep direction.
@param radius     Sphere radius (metres).
@param maxDist    Sweep length (metres).
@param outHit     Populated when hit == true.
@return True if the sphere hit something.

### Thin Wrapper

**Source:** [`src/engine/physics/rigid_body.cpp`](src/engine/physics/rigid_body.cpp#L5) (line 5)

RigidBodyCreator delegates directly to PhysicsWorld's factory methods.
Its value is the Descriptor struct — a single point that aggregates all
parameters, making call sites readable:

  RigidBodyCreator::Descriptor d;
  d.shapeType  = RigidBodyCreator::ShapeType::Box;
  d.halfExtents = { 0.5f, 0.5f, 0.5f };
  d.position    = { 0.0f, 5.0f, 0.0f };
  d.mass        = 2.0f;
  uint32_t id = RigidBodyCreator::Create(world, d);

Compare this to a raw PhysicsWorld call which requires choosing between
CreateBox / CreateSphere / CreateCapsule at the call site.  The Descriptor
approach is especially useful when the shape type comes from a data file.

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Platform: All

### Rigid Bodies in Game Physics

**Source:** [`src/engine/physics/rigid_body.hpp`](src/engine/physics/rigid_body.hpp#L6) (line 6)

============================================================================
A rigid body is an idealised object that:
  • Has a fixed shape (no deformation).
  • Has mass, moment of inertia, and a centre of mass.
  • Responds to forces, torques, and collisions according to Newton's laws.

In FFXV, rigid bodies are used for:
  • Debris and destructible props (crates, barrels, rocks).
  • Enemy ragdolls after death.
  • Physics-based puzzle objects.
  • Projectiles (grenades, spells with AoE hits).

─── Static vs Dynamic ──────────────────────────────────────────────────────
  Static  — infinite mass; never moves under physics.
            Examples: floors, walls, terrain meshes.
            Cost: zero solver work per frame (broadphase only).

  Dynamic — finite mass; accelerated by gravity and collisions.
            Examples: debris, physics-driven enemies, rolling boulders.
            Cost: one integration step + constraint solve per body per frame.

─── ECS Integration ────────────────────────────────────────────────────────
The RigidBodyCreator bridges the ECS and the physics simulation:
  1. An entity gets a RigidBodyComponent + ColliderComponent.
  2. A physics setup pass reads those components and calls
     RigidBodyCreator::CreateFromComponents(), which creates the Jolt body
     and writes the resulting bodyID back into RigidBodyComponent::bodyID.
  3. Each frame, the physics system calls Step(), then reads positions
     via PhysicsWorld::GetPosition(bodyID) and writes them back into
     TransformComponent so the renderer sees updated positions.

─── Sync Direction ─────────────────────────────────────────────────────────
  Physics → ECS:   After Step(), copy positions/rotations to TransformComponent.
  ECS → Physics:   When gameplay teleports an entity (e.g. warp-strike),
                   call PhysicsWorld::SetPosition(bodyID, newPos).

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Platform: All (no platform-specific code)

### Single Responsibility

**Source:** [`src/engine/physics/rigid_body.hpp`](src/engine/physics/rigid_body.hpp#L70) (line 70)

RigidBodyCreator is a stateless utility class (all methods are static).
It has one responsibility: translating shape type + parameters + position
into a Jolt body ID.  It does NOT own the PhysicsWorld or any body lifetime.

Keeping factory logic separate from PhysicsWorld keeps each class small
and focused — easier to test, easier to read.

### Choosing a Collision Shape

**Source:** [`src/engine/physics/rigid_body.hpp`](src/engine/physics/rigid_body.hpp#L89) (line 89)

Box      — fastest; best for crates, pillars, terrain patches.
  Sphere   — spheres roll smoothly; best for cannonballs, orbs.
  Capsule  — smooth sliding; best for characters, NPCs, props.

In a shipping game you would also support:
  ConvexHull   — arbitrary convex mesh (imported from .obj/.fbx).
  MeshShape    — concave mesh for complex terrain (static only).
  HeightField  — optimised terrain shape (height map grid).

---

## engine/platform

### Implementation File Organisation

**Source:** [`src/engine/platform/win32/Win32Window.cpp`](src/engine/platform/win32/Win32Window.cpp#L6) (line 6)

============================================================================
The .cpp file contains only the implementation.  The .hpp file (which every
user of this class includes) never includes windows.h directly — that
detail is encapsulated here.  Users of Win32Window see only the clean
public interface and never have to worry about name pollution from the
Windows headers.

@author  Educational Game Engine Project
@version 1.0
@date    2024

### Destructor = Guaranteed Cleanup

**Source:** [`src/engine/platform/win32/Win32Window.cpp`](src/engine/platform/win32/Win32Window.cpp#L36) (line 36)

Calling Shutdown() here means the window is always destroyed, even if
the caller forgets to call Shutdown() explicitly.
Shutdown();
}

### GetModuleHandle(nullptr)

**Source:** [`src/engine/platform/win32/Win32Window.cpp`](src/engine/platform/win32/Win32Window.cpp#L49) (line 49)

Every Win32 executable is a "module".  GetModuleHandle(nullptr) returns
the HINSTANCE (module handle) of the currently running .exe.  We need
it to register and create window classes.
-----------------------------------------------------------------------
m_hinstance = GetModuleHandle(nullptr);
m_width  = width;
m_height = height;

### WNDCLASSEX

**Source:** [`src/engine/platform/win32/Win32Window.cpp`](src/engine/platform/win32/Win32Window.cpp#L61) (line 61)

Before creating any window, Windows requires us to "register" a window
class — a template that describes the window's icon, cursor, background
colour, and most importantly the WndProc callback.
Multiple windows can share the same class; we only register ours once.
-----------------------------------------------------------------------
WNDCLASSEX wc   = {};
wc.cbSize       = sizeof(WNDCLASSEX);
wc.style        = CS_HREDRAW | CS_VREDRAW;  // Redraw on horizontal/vertical resize
wc.lpfnWndProc  = Win32Window::WndProc;     // Our event callback
wc.hInstance    = m_hinstance;
wc.hCursor      = LoadCursor(nullptr, IDC_ARROW);   // Standard arrow cursor
wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
wc.lpszClassName = kWindowClassName;

### AdjustWindowRect

**Source:** [`src/engine/platform/win32/Win32Window.cpp`](src/engine/platform/win32/Win32Window.cpp#L85) (line 85)

WS_OVERLAPPEDWINDOW gives a standard title bar + min/max/close buttons.
The title bar and borders consume pixels.  AdjustWindowRect inflates
the requested rect so the *client* area (usable drawing area) ends up
exactly width × height pixels.
-----------------------------------------------------------------------
RECT wr = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

### CreateWindowEx

**Source:** [`src/engine/platform/win32/Win32Window.cpp`](src/engine/platform/win32/Win32Window.cpp#L97) (line 97)

dwExStyle  : extended style flags (0 = defaults)
lpClassName: the class we registered above
lpWindowName: title bar text
dwStyle    : WS_OVERLAPPEDWINDOW = standard resizable window
x, y       : CW_USEDEFAULT lets Windows pick a sensible screen position
nWidth/nHeight: inflated rect dimensions from AdjustWindowRect
hWndParent : nullptr = top-level window (no parent)
hMenu      : nullptr = no menu bar
hInstance  : our module handle
lpParam    : extra data pointer — we pass 'this' so WndProc can recover it
-----------------------------------------------------------------------
m_hwnd = CreateWindowEx(
0,
kWindowClassName,
title.c_str(),
WS_OVERLAPPEDWINDOW,
CW_USEDEFAULT, CW_USEDEFAULT,
wr.right - wr.left,
wr.bottom - wr.top,
nullptr,
nullptr,
m_hinstance,
this    // <-- passed as lpCreateParams; retrieved in WM_NCCREATE
);

### Headless Window

**Source:** [`src/engine/platform/win32/Win32Window.cpp`](src/engine/platform/win32/Win32Window.cpp#L133) (line 133)

In headless mode the Win32 window still exists and has a valid HWND —
this is necessary for Vulkan surface creation (VkSurfaceKHR is tied to
a real HWND on Win32).  We simply never call ShowWindow(SW_SHOW), so
the window is never painted to the screen.  This lets CI machines
validate Vulkan initialisation without requiring a display.
-----------------------------------------------------------------------
if (!headless)
{
ShowWindow(m_hwnd, SW_SHOW);
UpdateWindow(m_hwnd);
}

### QueryPerformanceFrequency

**Source:** [`src/engine/platform/win32/Win32Window.cpp`](src/engine/platform/win32/Win32Window.cpp#L149) (line 149)

This returns the number of QPC ticks per second.  It is constant for
the lifetime of the process, so we query it once here.
-----------------------------------------------------------------------
QueryPerformanceFrequency(&m_frequency);
QueryPerformanceCounter(&m_lastCounter);  // Capture the starting time

### DestroyWindow vs PostQuitMessage

**Source:** [`src/engine/platform/win32/Win32Window.cpp`](src/engine/platform/win32/Win32Window.cpp#L172) (line 172)

DestroyWindow sends WM_DESTROY to the window, then removes it from the
screen and frees its resources.  Our WndProc handles WM_DESTROY by
calling PostQuitMessage which enqueues WM_QUIT.  That WM_QUIT is what
sets m_running = false in the message pump loop.
if (m_hwnd)
{
DestroyWindow(m_hwnd);
m_hwnd = nullptr;
}

### Computing Delta Time

**Source:** [`src/engine/platform/win32/Win32Window.cpp`](src/engine/platform/win32/Win32Window.cpp#L198) (line 198)

-----------------------------------------------------------------------
Before pumping messages (which might take non-trivial time), we capture
the current QPC value.  The gap between this and m_lastCounter is the
real wall-clock time of the previous frame.

We clamp delta time to 0.25 s to prevent the "spiral of death": if the
application is paused by the debugger or a long GC, a single enormous
dt could cause the physics simulation to explode.
-----------------------------------------------------------------------
LARGE_INTEGER now;
QueryPerformanceCounter(&now);

### PeekMessage vs GetMessage

**Source:** [`src/engine/platform/win32/Win32Window.cpp`](src/engine/platform/win32/Win32Window.cpp#L218) (line 218)

-----------------------------------------------------------------------
GetMessage BLOCKS until a message arrives — useless for a game loop.
PeekMessage returns immediately (PM_REMOVE removes it from the queue).
We drain every pending message in a tight loop so the OS never thinks
the app is unresponsive.
-----------------------------------------------------------------------
MSG msg = {};
while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
{
if (msg.message == WM_QUIT)
{
m_running = false;
}
TranslateMessage converts key-down/key-up pairs into WM_CHAR for
text input.  DispatchMessage routes the message to WndProc.
TranslateMessage(&msg);
DispatchMessage(&msg);
}
}

### Why Static?

**Source:** [`src/engine/platform/win32/Win32Window.cpp`](src/engine/platform/win32/Win32Window.cpp#L242) (line 242)

Windows stores a plain C function pointer in the WNDCLASSEX struct.  C++
member functions have an implicit `this` parameter, so they are not
compatible with plain function pointers.  The solution is a static member
function that retrieves the instance pointer we stored via SetWindowLongPtr.
===========================================================================
LRESULT CALLBACK Win32Window::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
-----------------------------------------------------------------------
Retrieve the Win32Window* that was stored during WM_NCCREATE.
-----------------------------------------------------------------------
Win32Window* self = reinterpret_cast<Win32Window*>(
GetWindowLongPtr(hWnd, GWLP_USERDATA)
);

### WM_NCCREATE

**Source:** [`src/engine/platform/win32/Win32Window.cpp`](src/engine/platform/win32/Win32Window.cpp#L260) (line 260)

-----------------------------------------------------------------------
This is the very first message Windows sends to a new window, *before*
WM_CREATE.  We use it to stash our `this` pointer in the window's user
data slot so every subsequent message can find it via GetWindowLongPtr.
-----------------------------------------------------------------------
case WM_NCCREATE:
{
lParam points to a CREATESTRUCT whose lpCreateParams is our `this`.
auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
SetWindowLongPtr(hWnd, GWLP_USERDATA,
reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
return DefWindowProc(hWnd, msg, wParam, lParam);
}

### Swapchain Recreation

**Source:** [`src/engine/platform/win32/Win32Window.cpp`](src/engine/platform/win32/Win32Window.cpp#L278) (line 278)

Vulkan swapchains are tied to a specific width and height.  When the
window is resized, the swapchain must be destroyed and rebuilt to match
the new dimensions.  We set a flag here; the renderer checks it each
frame and rebuilds when needed.
-----------------------------------------------------------------------
case WM_SIZE:
if (self)
{
self->m_width   = LOWORD(lParam);
self->m_height  = HIWORD(lParam);
self->m_resized = true;
}
return 0;

### PostQuitMessage

**Source:** [`src/engine/platform/win32/Win32Window.cpp`](src/engine/platform/win32/Win32Window.cpp#L303) (line 303)

Calling PostQuitMessage(0) places a WM_QUIT message in the queue.
PollEvents() will see that WM_QUIT and set m_running = false, which
causes the main game loop to exit cleanly.
-----------------------------------------------------------------------
case WM_DESTROY:
PostQuitMessage(0);
return 0;

### Virtual-Key Codes

**Source:** [`src/engine/platform/win32/Win32Window.cpp`](src/engine/platform/win32/Win32Window.cpp#L315) (line 315)

wParam holds a Virtual-Key (VK) code, e.g. VK_ESCAPE.  We handle
Escape here to give the user a keyboard-only exit path.
-----------------------------------------------------------------------
case WM_KEYDOWN:
if (wParam == VK_ESCAPE)
{
PostQuitMessage(0);
}
return 0;

### Platform Abstraction

**Source:** [`src/engine/platform/win32/Win32Window.hpp`](src/engine/platform/win32/Win32Window.hpp#L6) (line 6)

============================================================================
A "platform layer" is the bridge between the operating system and the rest
of the engine.  It is intentionally the ONLY place that includes Windows-
specific headers (windows.h, etc.).  Every other engine subsystem stays
OS-agnostic by depending on this layer through a clean interface.

On Windows the OS API is called "Win32" (even on 64-bit machines).
The key Win32 concepts we use here are:

  HWND   — a "handle to a window" (opaque pointer owned by Windows).
  HINSTANCE — a handle to the current process/module.
  WNDCLASSEX — a struct that describes what kind of window we want.
  WndProc — a callback function Windows calls for every window event.

============================================================================

### High-Resolution Timer

**Source:** [`src/engine/platform/win32/Win32Window.hpp`](src/engine/platform/win32/Win32Window.hpp#L22) (line 22)

============================================================================
Windows provides QueryPerformanceCounter (QPC) which counts at a very
high frequency (typically tens of millions of ticks per second).  Together
with QueryPerformanceFrequency (QPF) this gives sub-microsecond timing that
is essential for a smooth fixed-timestep game loop.

  elapsed_seconds = (counter_end - counter_start) / frequency

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Target OS: Windows (Win32 API)

### WIN32_LEAN_AND_MEAN

**Source:** [`src/engine/platform/win32/Win32Window.hpp`](src/engine/platform/win32/Win32Window.hpp#L43) (line 43)

---------------------------------------------------------------------------
Including <windows.h> pulls in a *massive* amount of Win32 API declarations.
Defining WIN32_LEAN_AND_MEAN before the include trims out rarely-used
subsystems (COM, OLE, Cryptography, etc.) and dramatically speeds up
compilation.  Always define it in engine code.
---------------------------------------------------------------------------
ifndef WIN32_LEAN_AND_MEAN
define WIN32_LEAN_AND_MEAN
endif
ifndef NOMINMAX

### NOMINMAX

**Source:** [`src/engine/platform/win32/Win32Window.hpp`](src/engine/platform/win32/Win32Window.hpp#L54) (line 54)

Windows.h defines macros called `min` and `max` that clash with
std::min / std::max.  Defining NOMINMAX before the include disables
those macros.
define NOMINMAX
endif
include <windows.h>

### class Win32Window

**Source:** [`src/engine/platform/win32/Win32Window.hpp`](src/engine/platform/win32/Win32Window.hpp#L70) (line 70)

===========================================================================
This class encapsulates everything the engine needs from the operating
system:
  1. A visible window (HWND) the user can see and close.
  2. A message pump — Windows sends keyboard, mouse, resize, close events
     to a registered callback (WndProc).  We must pump this queue every
     frame or Windows will think the application has frozen and show the
     "not responding" overlay.
  3. A delta-time source so the game loop knows how long each frame took.

Lifetime:
  Win32Window w;
  if (!w.Init("My Window", 1280, 720)) { /* error */ }
  while (w.IsRunning()) {
      w.PollEvents();
      double dt = w.GetDeltaTime();
      // ... render frame ...
  }
  w.Shutdown();   // optional — destructor calls it automatically
===========================================================================
class Win32Window
{
public:
-----------------------------------------------------------------------
Construction / Destruction
-----------------------------------------------------------------------

### Two-Phase Initialisation

**Source:** [`src/engine/platform/win32/Win32Window.hpp`](src/engine/platform/win32/Win32Window.hpp#L101) (line 101)

Call Init() explicitly after construction.  This allows the object to
be stored as a member variable before we are ready to create the window.

### RAII

**Source:** [`src/engine/platform/win32/Win32Window.hpp`](src/engine/platform/win32/Win32Window.hpp#L110) (line 110)

The destructor guarantees the window is cleaned up even if the caller
forgets to call Shutdown().  This prevents "zombie" windows from
lingering after a crash or early return.

### Client Area vs Window Area

**Source:** [`src/engine/platform/win32/Win32Window.hpp`](src/engine/platform/win32/Win32Window.hpp#L139) (line 139)

A Win32 window has a title bar and borders whose size depends on the
Windows theme.  The "client area" is the drawable region *inside* those
decorations.  We use AdjustWindowRect to inflate our desired client size
so that the final window delivers exactly the pixels we asked for.

### Message Pump

**Source:** [`src/engine/platform/win32/Win32Window.hpp`](src/engine/platform/win32/Win32Window.hpp#L162) (line 162)

Windows is event-driven: it posts events (WM_KEYDOWN, WM_CLOSE, …) into
a per-thread queue.  PeekMessage / TranslateMessage / DispatchMessage
drains that queue and routes each event to our WndProc callback.

We use PeekMessage (non-blocking) rather than GetMessage (blocking) so
that the game loop keeps running at full speed even with no user input.

Must be called once per frame before drawing.

### Delta Time

**Source:** [`src/engine/platform/win32/Win32Window.hpp`](src/engine/platform/win32/Win32Window.hpp#L177) (line 177)

"Delta time" (dt) is the duration of the previous frame in seconds.
It is used in the game loop to advance simulations independently of
frame rate: position += velocity * dt.

@return Delta time in seconds (clamped to a maximum of 0.25 s to
        prevent "spiral of death" when the app is paused/debugged).

### WndProc

**Source:** [`src/engine/platform/win32/Win32Window.hpp`](src/engine/platform/win32/Win32Window.hpp#L235) (line 235)

Windows calls this function for every event directed at our window.
It must be static (or a free function) because Windows stores a plain
C function pointer, not a C++ member function pointer.  We recover the
this pointer via GetWindowLongPtr(GWLP_USERDATA) which we set in Init().

@param hWnd   The window that received the message.
@param msg    The message identifier (WM_CLOSE, WM_SIZE, WM_KEYDOWN, …).
@param wParam First parameter — meaning depends on the message.
@param lParam Second parameter — meaning depends on the message.

### High-Resolution Timer State

**Source:** [`src/engine/platform/win32/Win32Window.hpp`](src/engine/platform/win32/Win32Window.hpp#L263) (line 263)

-----------------------------------------------------------------------
QueryPerformanceCounter gives us a 64-bit counter value.
QueryPerformanceFrequency tells us how many counts equal one second.
We record m_lastCounter at the *end* of PollEvents so we can compute
the gap at the start of the next PollEvents call.
-----------------------------------------------------------------------
LARGE_INTEGER m_frequency  = {};    ///< QPC ticks per second (constant).
LARGE_INTEGER m_lastCounter = {};   ///< QPC value at previous frame.
double        m_deltaTime   = 0.0;  ///< Seconds elapsed last frame.

---

## engine/rendering

### Renderer Abstraction (Strategy Pattern)

**Source:** [`src/engine/rendering/IRenderer.hpp`](src/engine/rendering/IRenderer.hpp#L6) (line 6)

============================================================================
This interface decouples the sandbox / game loop from the underlying
graphics API.  The concrete implementations are:

  D3D11Renderer  — Direct3D 11 backend; GT610-class hardware baseline.
                   Default on Windows.  Uses WARP software rasteriser in
                   headless / CI mode for zero-driver-requirement validation.

  VulkanRenderer — Vulkan 1.0+ backend; modern / high-end path.
                   Requires a Vulkan ICD on the machine.

The factory in RendererFactory.hpp decides which concrete renderer to
construct based on a runtime flag (--renderer d3d11 | vulkan).

============================================================================

### Why D3D11 as the Baseline?

**Source:** [`src/engine/rendering/IRenderer.hpp`](src/engine/rendering/IRenderer.hpp#L22) (line 22)

============================================================================
D3D11 with Feature Level 10_0 runs on GPUs released as far back as 2006
(GeForce 8 series, Radeon HD 2000).  A GeForce GT 610 — a common 2012 entry-
level card that is still in service — supports D3D11 Feature Level 11_0.
D3D11 ships with every Windows 7+ installation as part of the OS, so there
is zero setup for end users and CI runners alike.

Vulkan is reserved for the high-end / modern path (M3+) and is built when
ENGINE_ENABLE_VULKAN=ON but is not the runtime default.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Target: Windows (MSVC)

### Pure Virtual Interface

**Source:** [`src/engine/rendering/IRenderer.hpp`](src/engine/rendering/IRenderer.hpp#L61) (line 61)

Every method is pure virtual (= 0).  This means IRenderer cannot be
instantiated directly.  Only concrete subclasses that override every method
can be instantiated.  The virtual destructor ensures that deleting a
D3D11Renderer or VulkanRenderer through an IRenderer pointer calls the
right destructor chain.
===========================================================================
class IRenderer
{
public:
virtual ~IRenderer() = default;

### Translation Units

**Source:** [`src/engine/rendering/Renderer.cpp`](src/engine/rendering/Renderer.cpp#L6) (line 6)

============================================================================
In C++ a program is built from "translation units" — roughly one .cpp file
plus all the headers it includes.  By putting implementation in .cpp files:

 1. Compilation is faster: changing Renderer.cpp does not force recompilation
    of every file that includes Renderer.hpp.
 2. The implementation (ncurses calls) is hidden from callers — information
    hiding / encapsulation.
 3. Linking is simpler: the linker stitches object files together once.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17

### Include Order Convention

**Source:** [`src/engine/rendering/Renderer.cpp`](src/engine/rendering/Renderer.cpp#L27) (line 27)

──────────────────────────────────────────
Best practice (Google/LLVM style guides) is to include:
  1. The header for THIS .cpp (ensures it is self-contained)
  2. C system headers (stdio.h, etc.)
  3. C++ standard library headers
  4. Third-party library headers (ncurses, Lua, …)
  5. Project headers

By including Renderer.hpp first, the compiler verifies that Renderer.hpp
can be parsed without relying on any prior includes — making it genuinely
standalone.
---------------------------------------------------------------------------
include "engine/rendering/Renderer.hpp"

### Destructor as Safety Net

**Source:** [`src/engine/rendering/Renderer.cpp`](src/engine/rendering/Renderer.cpp#L70) (line 70)

─────────────────────────────────────────
If the program exits without explicitly calling Shutdown() (e.g., via an
exception), this destructor ensures ncurses releases the terminal.
Without it the user's shell would be left in raw/noecho mode — broken.
if (m_initialised)
{
Shutdown();
}
}

### init_pair(id, foreground, bg)

**Source:** [`src/engine/rendering/Renderer.cpp`](src/engine/rendering/Renderer.cpp#L196) (line 196)

─────────────────────────────────────────────
id         : pair number 1–255 (0 is reserved for terminal default).
foreground : one of COLOR_BLACK/RED/GREEN/YELLOW/BLUE/MAGENTA/CYAN/WHITE
             or a 256-colour index, or -1 for terminal default.
background : same as foreground, or -1 for terminal default.

A_BOLD can be added with attron() to make colours brighter on terminals
that only support 8 colours (each foreground has a "bright" variant).

These pairings deliberately echo JRPG color conventions:
  • Cyan player — classic Noctis/hero colour
  • Red enemies — universal danger colour
  • Yellow NPCs — friendly / attention-grabbing
-----------------------------------------------------------------------

### Member Initialiser List

**Source:** [`src/engine/rendering/Renderer.cpp`](src/engine/rendering/Renderer.cpp#L505) (line 505)

─────────────────────────────────────────
C++ allows you to initialise class members BEFORE the constructor body
runs using the initialiser list (the `: member(value)` syntax).

Prefer this over assignment in the constructor body for two reasons:
  1. const members and reference members (like m_renderer) MUST be
     initialised in the list — they cannot be default-constructed then
     assigned.
  2. For non-trivial types it avoids double-initialisation (default
     construct then assign = two operations; list-init = one).
LOG_INFO("MapRenderer created: viewport " + std::to_string(viewportW)
+ "x" + std::to_string(viewportH)
+ " at screen (" + std::to_string(screenX)
+ "," + std::to_string(screenY) + ")");
}

### Integer Division

**Source:** [`src/engine/rendering/Renderer.cpp`](src/engine/rendering/Renderer.cpp#L529) (line 529)

──────────────────────────────────
m_viewportW / 2 uses integer (truncating) division.  For an odd-width
viewport this shifts the camera slightly left-of-centre — imperceptible
in practice.  Floating-point division + rounding would be more precise
but adds complexity for negligible gain.
m_viewOriginX = centerTileX - (m_viewportW / 2);
m_viewOriginY = centerTileY - (m_viewportH / 2);
}

### Data-Driven Tile Rendering

**Source:** [`src/engine/rendering/Renderer.cpp`](src/engine/rendering/Renderer.cpp#L573) (line 573)

─────────────────────────────────────────────
Each TileType maps to a display character and a colour.
This switch statement is the "render dictionary" — it bridges
the abstract game-data TileType enum with concrete visual choices.

An alternative approach is a lookup table (array of structs indexed by
the enum value).  That would be faster (O(1) vs O(n) for switch on some
compilers) but less readable for students.  Modern compilers compile
switch statements on dense enums as jump tables anyway.
-----------------------------------------------------------------------
switch (tile)
{
FLOOR: a middle dot (·) represents empty walkable space.
Using ACS_BULLET (·) is ideal but we use '.' for max portability.
case TileType::FLOOR:
outChar  = '.';
outColor = CP_FLOOR;
break;

### "Explored but not visible" rendering

**Source:** [`src/engine/rendering/Renderer.cpp`](src/engine/rendering/Renderer.cpp#L703) (line 703)

─────────────────────────────────────────────────────
Tiles the player has visited but cannot currently see are drawn in
CP_FLOOR (dim white) regardless of their actual type.  This creates
the classic "memory map" effect: you know the layout but not the
current state (enemies/chests may have changed while out of view).
-----------------------------------------------------------------------
m_renderer.DrawChar(sx, sy, tileChar, CP_FLOOR);
}
else
{
Completely unexplored — draw solid black (space).
m_renderer.DrawChar(sx, sy, ' ', CP_DEFAULT);
}
}

### Field of View (FOV) overlay

**Source:** [`src/engine/rendering/Renderer.cpp`](src/engine/rendering/Renderer.cpp#L735) (line 735)

──────────────────────────────────────────────
This draws a darkening overlay OUTSIDE the player's vision radius.
Tiles within `radius` world-tile units are left at full brightness;
tiles beyond are overdrawn with a dim '#' to represent darkness.

Algorithm:
  For every screen cell in the viewport:
    1. Convert screen → world coordinates.
    2. Compute Euclidean distance from the viewer.
    3. If distance > radius, draw a dark overlay character.

A production engine would use a proper shadowcasting algorithm (e.g.
"Precise Permissive FOV" or the "recursive shadowcasting" algorithm)
to handle walls blocking sight lines.  This simple circle check is an
intentional simplification to keep the code readable for learners.
-----------------------------------------------------------------------

### Why ncurses for a Game Engine?

**Source:** [`src/engine/rendering/Renderer.hpp`](src/engine/rendering/Renderer.hpp#L6) (line 6)

============================================================================

Most "real" game engines use OpenGL, Vulkan, or DirectX for hardware-
accelerated 2D/3D rendering.  Those APIs require hundreds of lines of setup
before a single pixel appears.  For an educational engine our goal is to
focus on GAME-ENGINE CONCEPTS (ECS, scripting, combat systems, AI) rather
than graphics driver boilerplate.

ncurses (New Curses) solves this beautifully:
 • It draws text characters and coloured blocks to the terminal.
 • Setup is a handful of function calls (no GPU required).
 • Students can run the engine over SSH on any machine.
 • The "rendering pipeline" is simple enough to reason about in a lecture.

The conceptual mapping to a real renderer is:

  ncurses concept            | Real renderer equivalent
  ---------------------------+-----------------------------
  WINDOW* (character buffer) | Back-buffer / framebuffer
  refresh() / wrefresh()     | Present / SwapBuffers
  COLOR_PAIR(n)              | Shader uniform / material
  mvaddch(y,x,c)             | Draw call (one texel)
  clear()                    | Clear colour pass

============================================================================

### ncurses Coordinate System

**Source:** [`src/engine/rendering/Renderer.hpp`](src/engine/rendering/Renderer.hpp#L32) (line 32)

============================================================================

ncurses uses (row, col) = (y, x) ordering, which is the OPPOSITE of the
mathematical (x, y) convention.  This trips up almost every new learner.

Internally ncurses thinks in "row-major" order (like a printed page):
  Row 0 is the TOP of the screen.
  Col 0 is the LEFT  of the screen.
  Increasing row → moving DOWN.
  Increasing col → moving RIGHT.

All public API functions in this engine use the conventional (x, y) / (col,
row) order.  The renderer is responsible for swapping them when calling into
ncurses, keeping the rest of the engine free from this quirk.

============================================================================

### ncurses Color Pairs

**Source:** [`src/engine/rendering/Renderer.hpp`](src/engine/rendering/Renderer.hpp#L49) (line 49)

============================================================================

ncurses represents colour as a PAIR: (foreground, background).
- Up to 256 pairs are available on a 256-colour terminal.
- Pair 0 is the terminal default (usually white on black) and cannot be
  changed.
- You define pairs 1–255 with: init_pair(pair_id, fg_colour, bg_colour);
- You apply a pair with: attron(COLOR_PAIR(pair_id));
- You reset it with:     attroff(COLOR_PAIR(pair_id));

Colours available without 256-colour mode:
  COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_YELLOW,
  COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE

The CP_* constants defined below map game concepts (player, enemy, UI) to
colour-pair IDs that are initialised inside TerminalRenderer::Init().

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17

### Forward-declaring a scoped enum

**Source:** [`src/engine/rendering/Renderer.hpp`](src/engine/rendering/Renderer.hpp#L108) (line 108)

─────────────────────────────────────────────────
C++11 allows forward-declaring an enum class if you also specify its
underlying type.  The declaration must exactly match the definition.
  enum class TileType : uint8_t;   ← forward declaration (no enumerators)
Any file that needs the ACTUAL enumerator names (WALL, FLOOR, etc.) must
still #include "game/GameData.hpp".
enum class TileType : uint8_t;

### Why constexpr instead of #define?

**Source:** [`src/engine/rendering/Renderer.hpp`](src/engine/rendering/Renderer.hpp#L126) (line 126)

──────────────────────────────────────────────────
Old C code used #define for constants:
  #define CP_PLAYER 2
The preprocessor blindly replaces the token before compilation, so the
compiler never sees the name, which makes debugging painful.

Modern C++ uses `constexpr int` (or `constexpr auto`):
  constexpr int CP_PLAYER = 2;
This is type-safe, scoped, and visible to the debugger.

`constexpr` means "evaluate this at compile time."  The value is baked
directly into the binary just like #define, but with full type-checking.

### The Renderer Layer

**Source:** [`src/engine/rendering/Renderer.hpp`](src/engine/rendering/Renderer.hpp#L200) (line 200)

────────────────────────────────────
In a layered engine architecture the renderer sits between the OS/hardware
and higher-level systems (map renderer, UI system, combat display):

  [ Game Logic ]   ← calls →   [ MapRenderer / UIRenderer ]
                                        ↓ calls
                              [ TerminalRenderer ]   ← this class
                                        ↓ calls
                              [ ncurses C library ]
                                        ↓
                              [ Terminal / OS ]

Responsibilities of this class:
 1. ncurses lifecycle (Init / Shutdown).
 2. Primitive drawing operations (characters, strings, boxes, progress bars).
 3. Flushing the back-buffer to the screen (Refresh).
 4. Querying terminal dimensions (GetWidth / GetHeight).

### Why a Class Instead of Global Functions?

**Source:** [`src/engine/rendering/Renderer.hpp`](src/engine/rendering/Renderer.hpp#L219) (line 219)

─────────────────────────────────────────────────────────
Encapsulating ncurses in a class means:
 • All ncurses state lives in one place.
 • We can mock/replace the renderer in tests.
 • The destructor provides a safety net (calling Shutdown if forgotten).
 • It is easy to extend — swap ncurses for SDL_ttf without touching callers.

### Separating Construction from Initialisation

**Source:** [`src/engine/rendering/Renderer.hpp`](src/engine/rendering/Renderer.hpp#L236) (line 236)

────────────────────────────────────────────────────────────
Many engine objects separate the constructor (allocates memory, sets
defaults) from an explicit Init() call (acquires OS resources).
This is the "two-phase initialisation" pattern.

Reason: constructors cannot return error codes.  Init() can (it returns
bool here and logs failures).  This also makes the object safe to store
as a member variable before the terminal is ready.

### RAII (Resource Acquisition Is Initialisation)

**Source:** [`src/engine/rendering/Renderer.hpp`](src/engine/rendering/Renderer.hpp#L251) (line 251)

──────────────────────────────────────────────────────────────
RAII is one of C++'s most important idioms.  Resources (memory, file
handles, OS handles) are acquired in a constructor or Init() and
released in the destructor.  This guarantees cleanup even if an
exception is thrown, eliminating resource leaks.

Here the "resource" is the raw terminal: ncurses takes over the
terminal (disabling echo, hiding the cursor, etc.).  If we crash or
throw without calling endwin(), the terminal is left in a broken state.
The destructor prevents that.

### Back-Buffer / Double Buffering

**Source:** [`src/engine/rendering/Renderer.hpp`](src/engine/rendering/Renderer.hpp#L301) (line 301)

──────────────────────────────────────────────
ncurses maintains an internal "virtual screen" (the back-buffer).
Drawing calls write to this buffer.  The screen is only updated when
Refresh() is called.  This is conceptually identical to double-
buffering in OpenGL (glClear → draw → SwapBuffers).

Without double-buffering you would see partial frames — flickering.
With it, the player only ever sees complete, consistent frames.

clear() marks every cell of the virtual screen as blank.
erase() does the same but does not move the cursor; we use clear().

### The Character Cell Model

**Source:** [`src/engine/rendering/Renderer.hpp`](src/engine/rendering/Renderer.hpp#L319) (line 319)

─────────────────────────────────────────
An ASCII terminal divides the screen into a grid of "cells".
Each cell holds exactly ONE character plus colour attributes.
This maps perfectly to tile-based 2D games:
  cell (x, y) = tile at column x, row y.

@param x         Column (0 = left edge).
@param y         Row    (0 = top  edge).
@param c         ASCII character to draw.
@param colorPair CP_* constant selecting foreground/background colours.

### ACS Box-Drawing Characters

**Source:** [`src/engine/rendering/Renderer.hpp`](src/engine/rendering/Renderer.hpp#L365) (line 365)

────────────────────────────────────────────
ncurses provides the ACS_* macros for portable box-drawing characters
(the ┌ ─ ┐ │ └ ┘ glyphs).  These are rendered as real Unicode box-
drawing characters on modern terminals, but fall back gracefully to
ASCII (+, -, |) on older systems.

ACS_ULCORNER = ┌,  ACS_URCORNER = ┐
ACS_LLCORNER = └,  ACS_LRCORNER = ┘
ACS_HLINE    = ─,  ACS_VLINE    = │

@param x         Left column of the outer border.
@param y         Top  row    of the outer border.
@param w         Total width  including border (≥ 3).
@param h         Total height including border (≥ 3).
@param title     Text shown in the top border bar (centred).
@param colorPair CP_* colour pair for border and title.

### Visual Feedback in Game UI

**Source:** [`src/engine/rendering/Renderer.hpp`](src/engine/rendering/Renderer.hpp#L390) (line 390)

────────────────────────────────────────────
Progress bars communicate numerical quantities intuitively.
A full bar is drawn as a row of filled block characters (█ = ACS_BLOCK
or a '#' fallback).  The unfilled portion uses a lighter character.

Example (50% full, width=10):  [#####     ]

@param x         Left column of the progress bar (does NOT include '[').
@param y         Row of the bar.
@param w         Total width in columns including brackets.
@param current   Current value (clamped to [0, max]).
@param max       Maximum value (must be > 0).
@param colorPair CP_* colour pair for the filled portion.

### The Game Loop and Refresh

**Source:** [`src/engine/rendering/Renderer.hpp`](src/engine/rendering/Renderer.hpp#L416) (line 416)

───────────────────────────────────────────
A typical game loop looks like:
  while (running) {
    ProcessInput();
    Update(deltaTime);
    Clear();         ← erase back-buffer
    Draw();          ← write to back-buffer
    Refresh();       ← blit back-buffer → screen
  }

Only calling Refresh() at the END of all drawing minimises flicker
because the player never sees an intermediate frame.

### Dynamic Terminal Sizes

**Source:** [`src/engine/rendering/Renderer.hpp`](src/engine/rendering/Renderer.hpp#L439) (line 439)

────────────────────────────────────────
Terminals can be resized at runtime (SIGWINCH on Unix).  A robust
renderer polls getmaxx/getmaxy on every frame so the UI adapts.
For this engine we read dimensions fresh each time from ncurses.

@return Number of columns available (usually 80–220 on modern systems).

### Separation of Concerns

**Source:** [`src/engine/rendering/Renderer.hpp`](src/engine/rendering/Renderer.hpp#L503) (line 503)

────────────────────────────────────────
TerminalRenderer knows nothing about GAME concepts — it draws chars.
MapRenderer knows game concepts (tiles, fog-of-war, entities) but delegates
all actual drawing to a TerminalRenderer reference it holds.

This is the "Dependency Injection" pattern:
  MapRenderer depends on TerminalRenderer (an abstraction) rather than
  calling ncurses directly.  If we swap ncurses for SDL we only need to
  provide a new TerminalRenderer implementation, not rewrite MapRenderer.

### The Viewport

**Source:** [`src/engine/rendering/Renderer.hpp`](src/engine/rendering/Renderer.hpp#L514) (line 514)

─────────────────────────────
A game map is often LARGER than the terminal window.  The viewport is a
window that tracks the player and shows only the visible portion of the map.

  Full map (e.g. 200×200 tiles)
  ┌────────────────────────────┐
  │                            │
  │     ┌──────────┐           │
  │     │ VIEWPORT │  camera   │
  │     │  (shown) │ ──────►  │
  │     └──────────┘           │
  │                            │
  └────────────────────────────┘

m_viewOriginX/Y is the top-left tile of what is currently visible.
To convert a world tile (wx, wy) to a screen position:
  screenX = (wx - m_viewOriginX) * m_tileWidth  + m_screenOffsetX
  screenY = (wy - m_viewOriginY) * m_tileHeight + m_screenOffsetY

For ASCII rendering each tile is one character, so m_tileWidth = 1.

### Visibility States

**Source:** [`src/engine/rendering/Renderer.hpp`](src/engine/rendering/Renderer.hpp#L583) (line 583)

───────────────────────────────────
A typical tile can be in one of three states:

  1. VISIBLE   — Inside the player's field of view (FOV).
                 Drawn at full brightness with correct colour.
  2. EXPLORED  — The player visited here before but it is outside FOV.
                 Drawn in dim grey so the map stays legible.
  3. UNEXPLORED — Never seen.
                 Drawn as solid black or completely omitted.

@param worldX   Tile column in world space.
@param worldY   Tile row    in world space.
@param tile     The type of tile to render.
@param visible  True if the tile is currently in the player's FOV.
@param explored True if the player has visited this tile before.

### FOV / Fog of War

**Source:** [`src/engine/rendering/Renderer.hpp`](src/engine/rendering/Renderer.hpp#L620) (line 620)

──────────────────────────────────
Fog of War (FOW) is a classic game mechanic that limits how much of
the map the player can see.  A simple implementation:

  For every cell in the viewport:
    dist = distance from viewer to cell
    if dist ≤ radius → visible (draw bright)
    else             → not visible (draw dim or skip)

More sophisticated engines use ray-casting or the "shadowcasting"
algorithm (Björn Bergström, 2001) for better corner behaviour.
This implementation uses a simple circle check as a teaching example.

@param viewerWorldX  World-space column of the FOV origin (the player).
@param viewerWorldY  World-space row    of the FOV origin.
@param radius        How many tiles away the player can see.

### Factory Pattern for Renderer Selection

**Source:** [`src/engine/rendering/RendererFactory.hpp`](src/engine/rendering/RendererFactory.hpp#L6) (line 6)

============================================================================
The factory pattern decouples object creation from usage.  The caller says
"give me a renderer" and the factory decides which concrete type to
instantiate based on a runtime RendererBackend enum value.

This keeps main.cpp clean — it only knows about IRenderer, not about
D3D11Renderer or VulkanRenderer directly.  Swapping the backend is a
one-line change to the --renderer flag.

============================================================================

### Runtime Backend Selection vs Compile-Time

**Source:** [`src/engine/rendering/RendererFactory.hpp`](src/engine/rendering/RendererFactory.hpp#L17) (line 17)

============================================================================
We could also select the backend at compile time via preprocessor macros,
but runtime selection is more flexible for testing:

  engine_sandbox.exe                  → D3D11 (default)
  engine_sandbox.exe --renderer d3d11 → D3D11 explicit
  engine_sandbox.exe --renderer vulkan → Vulkan (requires ENGINE_ENABLE_VULKAN)

If a backend was not compiled in, the factory logs an error and returns
nullptr so the caller can gracefully exit.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Target: Windows (MSVC)

### RendererBackend Enum

**Source:** [`src/engine/rendering/RendererFactory.hpp`](src/engine/rendering/RendererFactory.hpp#L60) (line 60)

---------------------------------------------------------------------------
A strongly-typed enum class prevents accidental integer-to-enum conversions
and avoids polluting the enclosing namespace with the enumerator names.
---------------------------------------------------------------------------
enum class RendererBackend
{
D3D11,   // Direct3D 11 — GT610-compatible baseline (default on Windows)
Vulkan,  // Vulkan 1.0+ — modern / high-end path
};

### Parsing Command-Line Enum Values

**Source:** [`src/engine/rendering/RendererFactory.hpp`](src/engine/rendering/RendererFactory.hpp#L74) (line 74)

We use a simple string comparison.  The default is D3D11 so any unrecognised
value falls through to D3D11 with a warning.
---------------------------------------------------------------------------
inline RendererBackend ParseRendererBackend(const std::string& s)
{
if (s == "vulkan") return RendererBackend::Vulkan;
if (s == "d3d11")  return RendererBackend::D3D11;

### D3D11 Initialisation Sequence

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L6) (line 6)

============================================================================
D3D11 device creation is much simpler than Vulkan:

  1. Call D3D11CreateDevice (or D3D11CreateDeviceAndSwapChain).
  2. The OS driver selects the GPU automatically.
  3. The swap chain is created via DXGI.
  4. Create a render-target view (RTV) from the back buffer.

Compare to Vulkan where you enumerate physical devices, create logical
devices, pick queue families, create surfaces, and manage semaphores.
D3D11 hides most of that behind the driver — which makes it *easier* to
use but *harder* to reason about performance.  Both styles are worth
understanding.

============================================================================

### WARP Software Renderer

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L22) (line 22)

============================================================================
For headless / CI mode we pass D3D_DRIVER_TYPE_WARP to
D3D11CreateDevice().  WARP (Windows Advanced Rasterization Platform) is a
highly optimised CPU-based Direct3D implementation built into Windows.  It
supports Feature Level 11_0 in software and requires NO GPU driver, making
it perfect for GitHub-hosted CI runners.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Target: Windows (MSVC)

### pragma comment(lib, ...) vs CMake target_link_libraries

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L43) (line 43)

---------------------------------------------------------------------------
On MSVC we can tell the linker which .lib to pull in directly from source
using #pragma comment(lib, ...).  For D3D11 this is convenient because
d3d11.lib, dxgi.lib, and d3dcompiler.lib ship with the Windows SDK (always
present on MSVC) and we don't need a separate find_package() in CMake.

We still list them in target_link_libraries in CMakeLists.txt for clarity
and cross-toolchain compatibility.
---------------------------------------------------------------------------
pragma comment(lib, "d3d11.lib")
pragma comment(lib, "dxgi.lib")
pragma comment(lib, "d3dcompiler.lib")

### Quad Vertex Layout

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L70) (line 70)

Each vertex carries a 2-D NDC position (no Z — the VS sets it to 0) and a
2-D UV coordinate.  D3D11 convention: UV (0,0) = top-left of the texture.
The quad is centred at the origin and spans ±0.5 in NDC, giving a quad
that fills half the screen in each dimension.
pragma pack(push, 1)
struct QuadVertex
{
float x, y;  ///< NDC position ([-1,+1] range; z will be 0, w will be 1)
float u, v;  ///< Texture UV (0,0 = top-left; 1,1 = bottom-right)
};
pragma pack(pop)

### Index Buffer

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L92) (line 92)

Two clockwise triangles (D3D11 default front face) sharing the diagonal edge:
  Triangle 0: top-left (0), top-right (1), bottom-left (2)
  Triangle 1: top-right (1), bottom-right (3), bottom-left (2)
static const uint16_t kQuadIndices[6] = { 0, 1, 2, 1, 3, 2 };

### SkinnedVertex Layout

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L102) (line 102)

The skinned mesh vertex format carries:
  pos       — bind-pose position in NDC space (z=0, w=1)
  normal    — surface normal for lighting
  uv        — texture coordinates (V encodes height for the gradient)
  boneIndex — up to 4 joint indices into the constant buffer array
  boneWeight — corresponding weights (must sum to 1.0 per vertex)

We use uint32_t for bone indices and float for weights to keep the
CPU code readable.  In production you would use uint8_t (indices 0-255)
and UNORM8 weights to halve the vertex size.
pragma pack(push, 1)
struct SkinnedVertex
{
float    x, y, z;            ///< NDC position (z=0 for this flat demo)
float    nx, ny, nz;         ///< Surface normal
float    u, v;               ///< UV (V = height, 0 = bottom, 1 = top)
uint32_t boneIndex[4];       ///< Bone indices (only 2 joints used in demo)
float    boneWeight[4];      ///< Bone weights (sum = 1.0 per vertex)
};
pragma pack(pop)

### Skinned Strip Geometry

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L124) (line 124)

5 rows × 2 vertices = 10 vertices forming a vertical strip in NDC.
  x = ±0.10 NDC (narrow strip for clarity)
  y = -0.8 to +0.8 NDC (tall, covers 80% of screen height)

Skeleton (2 joints):
  Joint 0 (bone 0): root at NDC origin — identity skin matrix — vertices STATIC.
  Joint 1 (bone 1): child — Rotation(Z, θ) skin matrix — vertices ROTATE.

Skinning weights interpolate from 100% bone 0 at the bottom to 100% bone 1
in the top half, with a smooth 2-row blend zone in the middle.  This
demonstrates the blend artefact that linear blend skinning produces at joints.

Normals all point toward the camera (0, 0, -1) for consistent lighting.
The strip lies in the Z=0 plane.
static const SkinnedVertex kSkinnedVerts[10] =
{
Row 0: y = -0.80  — fully weighted to bone 0 (static anchor)
{ -0.10f, -0.80f, 0.0f,   0,0,-1,   0.0f, 0.00f,   {0,0,0,0}, {1.00f,0.00f,0.0f,0.0f} },
{  0.10f, -0.80f, 0.0f,   0,0,-1,   1.0f, 0.00f,   {0,0,0,0}, {1.00f,0.00f,0.0f,0.0f} },

### Index Buffer (4 quads = 8 triangles = 24 indices)

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L162) (line 162)

Vertex layout (strip, left = x=-0.1, right = x=+0.1, y increases upward):
  Row 0 (y=-0.8):  v0 (left)   v1 (right)
  Row 1 (y=-0.4):  v2 (left)   v3 (right)
  Row 2 (y= 0.0):  v4 (left)   v5 (right)
  Row 3 (y=+0.4):  v6 (left)   v7 (right)
  Row 4 (y=+0.8):  v8 (left)   v9 (right)

D3D11 default: front face = CLOCKWISE when viewed from the camera.
The camera is conceptually at z<0, looking toward +z.
Screen-space X is right, Y is up.

For each quad (rows i and i+1), the two triangles are:
  Triangle A: v[2i], v[2i+1], v[2i+2]  →  (left-bottom, right-bottom, left-top)
  Triangle B: v[2i+1], v[2i+3], v[2i+2] →  (right-bottom, right-top, left-top)

Screen-space CW verification for Triangle A (using row 0):
  v0=(-0.1,-0.8), v1=(+0.1,-0.8), v2=(-0.1,-0.4)
  Cross product (v1-v0) × (v2-v0):
    (0.2, 0, 0) × (0, 0.4, 0) = (0, 0, 0.08)  — positive z = CW in screen space ✓
static const uint16_t kSkinnedIndices[24] =
{
0, 1, 2,    1, 3, 2,   // Quad 0: rows 0-1
2, 3, 4,    3, 5, 4,   // Quad 1: rows 1-2
4, 5, 6,    5, 7, 6,   // Quad 2: rows 2-3
6, 7, 8,    7, 9, 8,   // Quad 3: rows 3-4
};

### Driver Type Selection

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L212) (line 212)

-----------------------------------------------------------------------
D3D_DRIVER_TYPE_HARDWARE — uses the physical GPU (fastest).
D3D_DRIVER_TYPE_WARP     — uses the CPU software rasteriser (universal).

In headless mode (CI, validation) we force WARP so the binary runs on
any Windows machine regardless of GPU driver state.  WARP supports
Feature Level 11_0 in software, which is more than enough for CI.
-----------------------------------------------------------------------
const D3D_DRIVER_TYPE driverType =
headless ? D3D_DRIVER_TYPE_WARP
: D3D_DRIVER_TYPE_HARDWARE;

### Feature Levels

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L226) (line 226)

-----------------------------------------------------------------------
We request feature levels in descending order.  D3D11CreateDevice picks
the highest level the hardware (or WARP) supports and stores the result
in m_featureLevel.

  11_0 — GeForce GT 610 (Kepler/Fermi rebrand), Radeon HD 7000 series
  10_1 — GeForce 9 / 200 / 400 series with updated drivers
  10_0 — GeForce 8 series, Radeon HD 2000–3000 (absolute minimum)

Feature Level 10_0 is the project's hard minimum for hardware support.
-----------------------------------------------------------------------
const D3D_FEATURE_LEVEL featureLevels[] = {
D3D_FEATURE_LEVEL_11_0,
D3D_FEATURE_LEVEL_10_1,
D3D_FEATURE_LEVEL_10_0,
};
const UINT numFeatureLevels = static_cast<UINT>(
sizeof(featureLevels) / sizeof(featureLevels[0]));

### Device Creation Flags

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L247) (line 247)

-----------------------------------------------------------------------
D3D11_CREATE_DEVICE_DEBUG enables the D3D11 debug layer (analogous to
Vulkan's validation layer).  We only enable it in Debug builds to avoid
the performance overhead in Release.
-----------------------------------------------------------------------
UINT createDeviceFlags = 0;
if defined(_DEBUG) || defined(DEBUG)
createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
endif

### D3D11CreateDevice vs D3D11CreateDeviceAndSwapChain

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L261) (line 261)

We separate device creation from swap chain creation so that headless
mode can skip the swap chain entirely (no HWND needed).
-----------------------------------------------------------------------
HRESULT hr = D3D11CreateDevice(
nullptr,            // Use default adapter (first enumerated GPU)
driverType,
nullptr,            // Software module — nullptr unless REFERENCE type
createDeviceFlags,
featureLevels,
numFeatureLevels,
D3D11_SDK_VERSION,
&m_device,
&m_featureLevel,
&m_context
);

### Fallback from Debug Layer to No-Debug

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L281) (line 281)

-----------------------------------------------------------------------
On some Windows installations the optional D3D11 debug layer DLL
(D3D11_1SDKLayers.dll) is not installed.  When the debug flag is set
and the DLL is absent, D3D11CreateDevice returns E_FAIL.  We retry
without the debug flag so CI runners still succeed.
-----------------------------------------------------------------------
if (createDeviceFlags & D3D11_CREATE_DEVICE_DEBUG)
{
createDeviceFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
hr = D3D11CreateDevice(
nullptr, driverType, nullptr,
createDeviceFlags,
featureLevels, numFeatureLevels,
D3D11_SDK_VERSION,
&m_device, &m_featureLevel, &m_context
);
}

### DXGI Swap Chain Description

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L346) (line 346)

-----------------------------------------------------------------------
IDXGISwapChain is the bridge between D3D11 and the OS window manager.
It manages a ring of back buffers; we render to one while the other is
being displayed.  Key fields:

  BufferCount       — number of back buffers (2 = double-buffering).
  BufferDesc.Format — pixel format; BGRA_8888 is the most compatible.
  SwapEffect        — DISCARD = fastest, FLIP_SEQUENTIAL = modern.
  Windowed          — TRUE for a windowed swap chain.
-----------------------------------------------------------------------
DXGI_SWAP_CHAIN_DESC scDesc = {};
scDesc.BufferCount                        = 2;
scDesc.BufferDesc.Width                   = width;
scDesc.BufferDesc.Height                  = height;
scDesc.BufferDesc.Format                  = DXGI_FORMAT_B8G8R8A8_UNORM;
scDesc.BufferDesc.RefreshRate.Numerator   = 60;
scDesc.BufferDesc.RefreshRate.Denominator = 1;
scDesc.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
scDesc.OutputWindow                       = hwnd;
scDesc.SampleDesc.Count                   = 1;   // No MSAA for baseline
scDesc.SampleDesc.Quality                 = 0;
scDesc.Windowed                           = TRUE;

### DXGI_SWAP_EFFECT_DISCARD

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L369) (line 369)

The oldest swap effect; supported on all D3D11 hardware.  The contents
of the back buffer are undefined after Present — we always clear so it
doesn't matter.  Modern code would use FLIP_DISCARD on Win10+.
scDesc.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

### Obtaining the IDXGIFactory via the Device's Adapter

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L376) (line 376)

-----------------------------------------------------------------------
We must create the swap chain through the same DXGI factory that owns
the adapter the D3D11 device was created on.  The safest way to get
that factory is to query the device's parent adapter via COM's QueryInterface.
-----------------------------------------------------------------------
IDXGIDevice*  dxgiDevice  = nullptr;
IDXGIAdapter* dxgiAdapter = nullptr;
IDXGIFactory* dxgiFactory = nullptr;

### Render-Target View (RTV)

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L419) (line 419)

A RTV is a "view" that tells D3D11 which texture sub-resource to render
into.  Here we point it at the swap chain's back buffer.
-----------------------------------------------------------------------
ID3D11Texture2D* backBuffer = nullptr;
hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
reinterpret_cast<void**>(&backBuffer));
if (FAILED(hr)) {
std::cerr << "[D3D11Renderer] GetBuffer failed.\n";
return false;
}

### Caching Back-Buffer Dimensions

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L440) (line 440)

-----------------------------------------------------------------------
Store the back-buffer size so DrawFrame can set the viewport and bind
the RTV correctly on every frame.  The viewport must match the
back-buffer size or the rasteriser will clip rendered geometry.
-----------------------------------------------------------------------
m_width  = width;
m_height = height;

### Flush and Flush-to-Idle before release

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L478) (line 478)

Before releasing any D3D11 objects we flush the immediate context so
any in-flight GPU commands are drained.  Without this, destroying
resources the GPU is still referencing can cause device-removed errors.
if (m_context)
m_context->Flush();

### D3D11 Frame Setup: Bind RTV + Viewport

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L503) (line 503)

-----------------------------------------------------------------------
Before issuing any draw or clear commands we must:

  1. OMSetRenderTargets — tell the Output Merger (OM) stage which
     texture(s) to write into.  The second parameter is the depth-
     stencil view (nullptr here because we have no depth buffer yet).

  2. RSSetViewports — tell the Rasteriser (RS) stage the region of the
     render target to use.  TopLeftX/Y = 0 means "use the full texture".
     Without an explicit viewport call, the rasteriser falls back to
     implementation-defined behaviour on some drivers.
-----------------------------------------------------------------------

### D3D11 Clear

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L531) (line 531)

-----------------------------------------------------------------------
ClearRenderTargetView fills the back buffer with a solid colour.
The clear happens *before* draw calls so geometry is composited on top
of the clear colour, not underneath it.
-----------------------------------------------------------------------
const float clearColor[4] = { clearR, clearG, clearB, 1.0f };
m_context->ClearRenderTargetView(m_renderTarget, clearColor);

### Scene Draw Pass (M3+)

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L541) (line 541)

-----------------------------------------------------------------------
If a scene has been loaded via LoadScene(), draw it on top of the clear
colour.  The RTV and viewport are already set from the frame setup above,
so the draw methods can issue draw calls directly.

M3: textured_quad — a UV-mapped full-screen quad.
M4b: skinned_mesh — a GPU-skinned animated strip (2-joint skeleton).
-----------------------------------------------------------------------
if (m_quadScene.loaded && m_currentScene == "textured_quad")
DrawTexturedQuad();

### Advancing the demo animation timer.

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L553) (line 553)

m_sceneTime accumulates real elapsed time (seconds) and is used by
DrawSkinnedMesh() to compute a sinusoidal joint rotation angle.
We advance it unconditionally so LoadScene("skinned_mesh") can start
animating immediately.
m_sceneTime += 1.0f / 60.0f;   // TEACHING NOTE: approx 60fps fixed step

### Present interval

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L564) (line 564)

-----------------------------------------------------------------------
Present(1, 0) — sync to VBlank (v-sync on), 60fps cap on 60Hz monitors.
Present(0, 0) — present as fast as possible (no v-sync).
We use v-sync for the demo to avoid tearing.
-----------------------------------------------------------------------
m_swapChain->Present(1, 0);
}

### Swap Chain Resize Sequence (D3D11)

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L582) (line 582)

1. Release the render-target view (it references the old back buffer).
2. Call IDXGISwapChain::ResizeBuffers — the swap chain resizes in place.
3. Re-acquire the back buffer and create a new RTV.
Missing step 1 causes E_INVALIDARG because the buffer is still bound.

### Off-Screen Validation for Headless CI

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L631) (line 631)

-----------------------------------------------------------------------
In headless mode the swap chain does not exist (no HWND surface).
To validate that the D3D11 device can actually render — not just that
it was created — we:

  1. Create a small off-screen D3D11_TEXTURE2D (64×64, RGBA8).
  2. Create a Render-Target View (RTV) for it.
  3. Clear the RTV to a known colour.
  4. Flush the command queue.
  5. Release the temporary resources.

This round-trip exercises the full D3D11 resource creation + clear
path on the WARP software renderer, confirming the device is functional
even without a physical GPU or display.

A future iteration could read back the pixel data via a staging texture
and assert the exact clear colour to catch subtle driver bugs.
-----------------------------------------------------------------------
if (!m_initialised)
return false;

### COM Reference Counting

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L680) (line 680)

COM objects are reference-counted.  CreateRenderTargetView internally
calls AddRef on the texture, so the texture stays alive even after we
Release() our own handle (offscreenTex).  We release early to keep
resource lifetimes tight and avoid leaks if the next check returns early.
offscreenTex->Release();
if (FAILED(hr))
{
std::cerr << "[D3D11Renderer] RecordHeadlessFrame: CreateRenderTargetView failed.\n";
return false;
}

### Validating the Scene Pipeline in Headless Mode (M3+)

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L697) (line 697)

-----------------------------------------------------------------------
If a scene has been loaded (e.g. "textured_quad"), we bind the offscreen
RTV and run one draw call to confirm the full shader + geometry +
texture pipeline is functional under WARP.
We set a small viewport matching the 64×64 offscreen texture so the
rasteriser clips correctly.
-----------------------------------------------------------------------
if (m_quadScene.loaded && m_currentScene == "textured_quad")
{
D3D11_VIEWPORT vp = {};
vp.Width    = 64.0f;
vp.Height   = 64.0f;
vp.MaxDepth = 1.0f;
m_context->RSSetViewports(1, &vp);

### Headless validation for the GPU skinning scene (M4b).

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L717) (line 717)

We bind the off-screen RTV, set a matching 64×64 viewport, and call
DrawSkinnedMesh() once.  This validates that the skinned mesh pipeline
(VS, PS, input layout, joint constant buffer, geometry buffers)
compiles and executes correctly under WARP without a physical GPU.
if (m_skinnedScene.loaded && m_currentScene == "skinned_mesh")
{
D3D11_VIEWPORT vp = {};
vp.Width    = 64.0f;
vp.Height   = 64.0f;
vp.MaxDepth = 1.0f;
m_context->RSSetViewports(1, &vp);

### C++ requires functions to be declared before use.

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L744) (line 744)

LoadSkinnedMeshScene is a file-scope static helper defined later in this
translation unit.  Rather than move the entire 300-line function above
LoadScene (which would hurt reading order), we use a forward declaration.
static bool LoadSkinnedMeshScene(
ID3D11Device*                    device,
const std::string&               shaderDir,
D3D11Renderer::SkinnedMeshScene& scene);

### Runtime HLSL Compilation with D3DCompileFromFile

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L797) (line 797)

-----------------------------------------------------------------------
D3D11 shaders are written in HLSL and can be compiled either:
  a) At build time with fxc.exe or dxc.exe → .cso (compiled shader object)
  b) At runtime with D3DCompileFromFile() → ID3DBlob of bytecode

We use runtime compilation here for two reasons:
  1. Students can edit the .hlsl files and immediately see the effect
     by restarting the engine — no separate build step.
  2. It avoids adding a fxc.exe pre-build step to CMake which would
     require a special Windows SDK tool search.

In a production engine you would always use pre-compiled .cso files
for shipping because runtime compilation is slower and exposes shader
source to end-users.

D3DCompileFromFile takes a WIDE character path (LPCWSTR) because the
Windows filesystem API uses UTF-16 internally.
-----------------------------------------------------------------------

### Embedded Fallback HLSL

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L823) (line 823)

-----------------------------------------------------------------------
If the .hlsl files are not present on disk (e.g. a minimal CI run that
didn't copy shaders) we compile from inline string literals.  This
guarantees LoadScene always succeeds in any environment.

The fallback shaders are identical to the .hlsl files; keeping them in
sync is the developer's responsibility.  An alternative design would use
a resource file (.rc) to embed the HLSL at link time.
-----------------------------------------------------------------------
static const char* kVsFallback =
"struct VSInput { float2 pos:POSITION; float2 uv:TEXCOORD0; };\n"
"struct PSInput { float4 pos:SV_POSITION; float2 uv:TEXCOORD0; };\n"
"PSInput main(VSInput i) { PSInput o; o.pos=float4(i.pos,0,1); o.uv=i.uv; return o; }\n";

### std::wstring for Win32 wide-char path

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L857) (line 857)

D3DCompileFromFile requires a LPCWSTR (wide string) path.
std::filesystem::path::wstring() gives us that on MSVC.
std::wstring wpath = path.wstring();
hr = D3DCompileFromFile(
wpath.c_str(),
nullptr,          // no macro definitions
nullptr,          // no include handler
entryPoint,
target,
D3DCOMPILE_ENABLE_STRICTNESS,   // catch undeclared variables
0,                              // no effect compile flags
&code,
&errors
);
}

### Creating Shader Objects

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L937) (line 937)

-----------------------------------------------------------------------
D3D11 separates shader compilation (→ bytecode blob) from shader object
creation (→ ID3D11VertexShader / ID3D11PixelShader).  The bytecode is
needed once for object creation + input-layout creation, then can be
discarded.  We Release() the blobs after we are done with them.
-----------------------------------------------------------------------
HRESULT hr = m_device->CreateVertexShader(
vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
nullptr, &m_quadScene.vs);

### Input Layout

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L969) (line 969)

-----------------------------------------------------------------------
The Input Assembler (IA) stage needs to know how the raw bytes in the
vertex buffer map to shader input semantics.  D3D11_INPUT_ELEMENT_DESC
describes each field:

  SemanticName   — matches the HLSL attribute name ("POSITION", "TEXCOORD")
  SemanticIndex  — for arrays (e.g. TEXCOORD0 vs TEXCOORD1)
  Format         — DXGI_FORMAT_R32G32_FLOAT = two 32-bit floats
  InputSlot      — which vertex buffer slot (we only have slot 0)
  AlignedByteOffset — byte offset within the vertex struct
-----------------------------------------------------------------------
D3D11_INPUT_ELEMENT_DESC layoutDesc[] =
{
{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
D3D11_INPUT_PER_VERTEX_DATA, 0 },
{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8,
D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

### Vertex and Index Buffers

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L1005) (line 1005)

-----------------------------------------------------------------------
D3D11_BUFFER_DESC describes the buffer's purpose and access pattern:

  Usage = DEFAULT  — GPU reads and writes; CPU cannot map.
                     Fastest for geometry that never changes.
  BindFlags = VERTEX_BUFFER / INDEX_BUFFER — usage in the IA stage.

D3D11_SUBRESOURCE_DATA carries the initial CPU data that is uploaded
to VRAM when CreateBuffer() is called.  After the call the CPU buffer
is no longer referenced by D3D11.
-----------------------------------------------------------------------
{
D3D11_BUFFER_DESC bd  = {};
bd.ByteWidth          = static_cast<UINT>(sizeof(kQuadVerts));
bd.Usage              = D3D11_USAGE_DEFAULT;
bd.BindFlags          = D3D11_BIND_VERTEX_BUFFER;

### Texture Loading vs Fallback

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L1054) (line 1054)

-----------------------------------------------------------------------
We look for a test DDS texture in the shaderDir's parent (project root)
under "samples/vertical_slice_project/Cooked/textures/".  If not found
we create a 1×1 white RGBA8 texture inline so LoadScene never fails due
to a missing asset.

This pattern — "try to load asset, fall back to procedural placeholder" —
is common in AAA engines.  It lets the pipeline validate even when
content hasn't been cooked yet.
-----------------------------------------------------------------------
fs::path ddsPath = fs::path(shaderDir) / ".." / ".." / ".." / ".."
/ "samples" / "vertical_slice_project"
/ "Cooked" / "textures" / "test_texture.dds";
ddsPath = ddsPath.lexically_normal();

### Procedural 1×1 White Fallback Texture

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L1080) (line 1080)

-----------------------------------------------------------------------
When no DDS file is present we create a 1×1 RGBA8 white texture
directly without going through the DDS loader.  This exercises the
same texture-binding code path so the quad renders in white.
-----------------------------------------------------------------------
std::cout << "[D3D11Renderer] No DDS found; using 1×1 white fallback texture.\n";

### LoadScene_SkinnedMesh (private helper — inlined in LoadScene)

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L1160) (line 1160)

We use a local lambda at file scope to keep the main LoadScene() readable.
All resource creation follows the same pattern as the textured quad:
  compile HLSL → create shaders → create input layout → create buffers.

### Fallback HLSL for the skinned mesh vertex shader.

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L1173) (line 1173)

-----------------------------------------------------------------------
This is a minimal version of skinned_mesh.vs.hlsl that performs linear
blend skinning for up to 4 bone influences.  It matches the full HLSL
file that ships in the shaders/ directory; the inline copy guarantees
LoadScene never fails even if the .hlsl files were not copied to the
output directory (e.g. on a first clean build before POST_BUILD runs).
-----------------------------------------------------------------------
static const char* kSkinnedVsFallback =
"cbuffer JointCB:register(b0){float4x4 g_joints[64];};\n"
"struct VSIn{float3 p:POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;"
"uint4 bi:BLENDINDICES;float4 bw:BLENDWEIGHT;};\n"
"struct PSIn{float4 p:SV_POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;};\n"
"PSIn main(VSIn i){\n"
"  float4 sp=float4(0,0,0,0);float3 sn=float3(0,0,0);\n"
"  [unroll]for(int b=0;b<4;++b){\n"
"    float w=i.bw[b];uint idx=i.bi[b];\n"
"    sp+=w*mul(float4(i.p,1),g_joints[idx]);\n"
"    sn+=w*mul(i.n,(float3x3)g_joints[idx]);}\n"
"  PSIn o;float wi=(abs(sp.w)>0.0001f)?(1.0f/sp.w):1.0f;\n"
"  o.p=float4(sp.xyz*wi,1);o.n=normalize(sn);o.uv=i.uv;return o;}\n";

### SkinnedVertex Input Layout

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L1280) (line 1280)

The D3D11_INPUT_ELEMENT_DESC array must exactly match the SkinnedVertex
struct defined at the top of this file (field order and byte offsets).

BLENDINDICES uses DXGI_FORMAT_R32G32B32A32_UINT  (4 × uint32 = 16 bytes).
BLENDWEIGHT  uses DXGI_FORMAT_R32G32B32A32_FLOAT (4 × float  = 16 bytes).

In production you would use R8G8B8A8_UINT + R8G8B8A8_UNORM (4+4 = 8 bytes
per vertex instead of 32 bytes) to reduce vertex bandwidth.  We use 32-bit
types here for readability.
-----------------------------------------------------------------------
D3D11_INPUT_ELEMENT_DESC layout[] =
{
{ "POSITION",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
{ "NORMAL",        0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
{ "TEXCOORD",      0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
{ "BLENDINDICES",  0, DXGI_FORMAT_R32G32B32A32_UINT,  0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
{ "BLENDWEIGHT",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

### Why cull-none for the skinning demo?

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L1353) (line 1353)

The strip starts facing the camera but rotates 360° as bone 1 oscillates.
With the default back-face culling the strip disappears every 180°.
D3D11_CULL_NONE makes both faces visible — useful for flat 2-sided meshes
like cloth, leaves, and demo strips.  For opaque characters you typically
keep cull-back and ensure mesh normals are consistent.
-----------------------------------------------------------------------
{
D3D11_RASTERIZER_DESC rd = {};
rd.FillMode = D3D11_FILL_SOLID;
rd.CullMode = D3D11_CULL_NONE;
rd.FrontCounterClockwise = FALSE;
rd.DepthClipEnable = TRUE;
hr = device->CreateRasterizerState(&rd, &scene.rastState);
if (FAILED(hr)) {
std::cerr << "[D3D11Renderer] CreateRasterizerState (skinned) failed.\n";
return false;
}
}

### The D3D11 Draw Call Sequence

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L1385) (line 1385)

-----------------------------------------------------------------------
Every draw call in D3D11 requires the full pipeline state to be set:

  IA — Input Assembler: topology, vertex buffer, index buffer, layout.
  VS — Vertex Shader: program + constant buffers.
  PS — Pixel Shader: program + textures + samplers.
  OM — Output Merger: render targets + blend state.

The OM is already configured by the caller (DrawFrame or RecordHeadlessFrame).
We set IA, VS, and PS here for the quad draw call.

### PSSetShaderResources / PSSetSamplers

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L1397) (line 1397)

These calls bind texture resources and sampler states to HLSL registers.
register(t0) in HLSL ↔ slot 0 of PSSetShaderResources.
register(s0) in HLSL ↔ slot 0 of PSSetSamplers.
-----------------------------------------------------------------------

### Constructing the Skin Matrices for the 2-Joint Demo

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L1453) (line 1453)

-----------------------------------------------------------------------
The demo skeleton has two joints:

  Joint 0 (bone 0 — root):
    bind-pose world matrix = Identity.
    invBind[0]             = Identity.
    worldAnim[0]           = Identity (root never moves).
    skinMatrix[0]          = Identity * Identity = Identity.
    → Vertices weighted to bone 0 are NOT deformed (static anchor).

  Joint 1 (bone 1 — child):
    bind-pose world matrix = Identity (joint 1 is co-located at origin).
    invBind[1]             = Identity.
    worldAnim[1]           = Rotation(Z, θ(t)).
    skinMatrix[1]          = Identity * Rotation(Z, θ) = Rotation(Z, θ).
    → Vertices weighted to bone 1 rotate around the world origin.

The result: the bottom half of the strip (bone 0) is fixed; the top
half (bone 1) sweeps an arc; the blend zone smoothly transitions.
-----------------------------------------------------------------------
const float angle = std::sin(m_sceneTime * 1.5f) * (kPi * 0.25f);  // ±45°

### Input Assembler (IA) Stage Setup

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L1488) (line 1488)

-----------------------------------------------------------------------
We set the same four IA parameters as any other draw call:
  IASetInputLayout      — tells D3D11 how to decode vertex bytes.
  IASetPrimitiveTopology — triangles for a solid mesh.
  IASetVertexBuffers    — our SkinnedVertex buffer.
  IASetIndexBuffer      — 16-bit index buffer.
-----------------------------------------------------------------------
m_context->IASetInputLayout(m_skinnedScene.inputLayout);
m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

### DrawIndexed

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L1512) (line 1512)

-----------------------------------------------------------------------
DrawIndexed(indexCount, startIndex, baseVertex):
  • Reads `indexCount` indices starting at `startIndex`.
  • Each index is added to `baseVertex` to get the actual vertex index.
For our strip: 24 indices, 0 start, 0 base offset.
-----------------------------------------------------------------------
m_context->DrawIndexed(static_cast<UINT>(m_skinnedScene.indexCount), 0, 0);

### Release Order (LIFO vs Creation Order)

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L1534) (line 1534)

COM objects must be released in reverse-creation order when one object
holds a reference to another.  For independent scene objects (shaders,
buffers, textures) the order doesn't strictly matter, but releasing in
reverse makes intent clear.

### Release order: state objects first (they don't depend on

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.cpp`](src/engine/rendering/d3d11/D3D11Renderer.cpp#L1564) (line 1564)

shaders), then shaders, then geometry buffers, then the CB.
if (m_skinnedScene.rastState)   { m_skinnedScene.rastState->Release();   m_skinnedScene.rastState   = nullptr; }
if (m_skinnedScene.ps)          { m_skinnedScene.ps->Release();          m_skinnedScene.ps          = nullptr; }
if (m_skinnedScene.vs)          { m_skinnedScene.vs->Release();          m_skinnedScene.vs          = nullptr; }
if (m_skinnedScene.inputLayout) { m_skinnedScene.inputLayout->Release(); m_skinnedScene.inputLayout = nullptr; }
if (m_skinnedScene.indexBuf)    { m_skinnedScene.indexBuf->Release();    m_skinnedScene.indexBuf    = nullptr; }
if (m_skinnedScene.vertexBuf)   { m_skinnedScene.vertexBuf->Release();   m_skinnedScene.vertexBuf   = nullptr; }

### Why Direct3D 11?

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.hpp`](src/engine/rendering/d3d11/D3D11Renderer.hpp#L6) (line 6)

============================================================================
D3D11 is the sweet spot for a "runs everywhere on Windows" renderer:

  • Ships with Windows 7, 8, 10, 11 — zero end-user setup.
  • Supported by every discrete GPU from ~2006 onwards (Feature Level 10_0).
  • A GeForce GT 610 (2012 Kepler-rebrand / Fermi) supports FL 11_0.
  • D3D11 WARP (Windows Advanced Rasterization Platform) is a CPU software
    rasteriser bundled with Windows — it means CI runners and machines
    without a physical GPU can still run the renderer for validation.

Feature Level Baseline:
  We request [11_0, 10_1, 10_0] in order.  The device is created at the
  highest level the hardware supports; at minimum 10_0 is required.

============================================================================

### D3D11 Device Hierarchy

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.hpp`](src/engine/rendering/d3d11/D3D11Renderer.hpp#L22) (line 22)

============================================================================
D3D11 has three core objects:

  ID3D11Device         — the logical GPU.  Used for resource creation
                         (buffers, textures, shaders, states).  Thread-safe
                         when the flag D3D11_CREATE_DEVICE_SINGLETHREADED
                         is NOT set.

  ID3D11DeviceContext  — the immediate drawing context.  Records draw /
                         dispatch commands and executes them.  NOT thread-
                         safe (you need deferred contexts for MT recording).

  IDXGISwapChain       — the flip chain that presents rendered frames to
                         the OS compositor.  Belongs to DXGI (DirectX
                         Graphics Infrastructure), not D3D11 directly.

============================================================================

### WARP (Software Renderer) for Headless CI

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.hpp`](src/engine/rendering/d3d11/D3D11Renderer.hpp#L40) (line 40)

============================================================================
When --headless is passed (or ENGINE_D3D11_FORCE_WARP is defined) we create
the device with D3D_DRIVER_TYPE_WARP instead of D3D_DRIVER_TYPE_HARDWARE.
WARP:
  • Requires no GPU driver.
  • Runs on every GitHub-hosted Windows runner out of the box.
  • Supports FL 11_0 in software.
  • Is slower than hardware but correct for validation.
In headless mode we also skip IDXGISwapChain creation — WARP has no window
to present to, and we only need device-creation validation for CI.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Target: Windows (MSVC)
Requires: d3d11.lib, dxgi.lib, d3dcompiler.lib (Windows SDK — always present)

### D3D11 / DXGI Headers

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.hpp`](src/engine/rendering/d3d11/D3D11Renderer.hpp#L69) (line 69)

---------------------------------------------------------------------------
These headers ship with the Windows SDK — no separate download needed.
d3d11.h     — D3D11 device, context, resource types.
dxgi.h      — DXGI swap chain, adapter, factory types.
d3dcompiler.h — runtime HLSL compilation via D3DCompileFromFile.
---------------------------------------------------------------------------
include <d3d11.h>
include <dxgi.h>
include <d3dcompiler.h>

### Object Lifecycle

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.hpp`](src/engine/rendering/d3d11/D3D11Renderer.hpp#L91) (line 91)

All COM objects (ID3D11Device, etc.) are managed via raw COM pointers.
We call Release() manually in Shutdown() in reverse-creation order.
An alternative is Microsoft::WRL::ComPtr<T> which auto-releases; we use
raw pointers here so the release sequence is explicit and teachable.
===========================================================================
class D3D11Renderer : public IRenderer
{
public:
D3D11Renderer();
~D3D11Renderer() override;

### COM pointer naming convention

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.hpp`](src/engine/rendering/d3d11/D3D11Renderer.hpp#L165) (line 165)

We prefix all COM interface pointers with m_ (member) and use the
interface name as the type hint.  e.g. m_device is an ID3D11Device*.

### Storing Back-Buffer Dimensions

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.hpp`](src/engine/rendering/d3d11/D3D11Renderer.hpp#L177) (line 177)

-----------------------------------------------------------------------
We cache the current back-buffer size so that DrawFrame can set the
viewport correctly on every frame.  Without an explicit viewport the
rasteriser uses a full-surface default on some drivers, but it is
better practice to set it explicitly so the behaviour is predictable
across hardware and WARP.
-----------------------------------------------------------------------
uint32_t                m_width         = 0;
uint32_t                m_height        = 0;

### Public Scene-Resource Structs

**Source:** [`src/engine/rendering/d3d11/D3D11Renderer.hpp`](src/engine/rendering/d3d11/D3D11Renderer.hpp#L193) (line 193)

-----------------------------------------------------------------------
These inner structs are public to allow static helper functions in the
.cpp file to create and populate them without requiring friendship.
They contain only D3D11 COM pointers and are plain aggregates —
making them public does not expose any hidden invariants.

TexturedQuadScene (M3): resources for the UV-mapped quad.
SkinnedMeshScene  (M4b): resources for the GPU-skinned strip.
-----------------------------------------------------------------------

### DDS Parsing without a Third-Party Library

**Source:** [`src/engine/rendering/d3d11/d3d11_texture.cpp`](src/engine/rendering/d3d11/d3d11_texture.cpp#L6) (line 6)

============================================================================
Many engines pull in DirectXTex or DirectXTK for DDS loading.  This file
implements a minimal, self-contained parser that handles the formats
needed by the M3 milestone:

  - RGBA8 uncompressed (for UI textures, debugging).
  - BC1 / DXT1         (for low-quality opaque geometry).
  - BC3 / DXT5         (for textures with alpha channels).
  - BC7                (high-quality; preferred for M3 cooked assets).

Implementing the parser from scratch makes it easy to understand what the
DirectXTex library does internally — and shows that DDS files are not magic,
just a well-specified binary format.

============================================================================

### D3D11 Texture Upload

**Source:** [`src/engine/rendering/d3d11/d3d11_texture.cpp`](src/engine/rendering/d3d11/d3d11_texture.cpp#L22) (line 22)

============================================================================
Creating a GPU texture from CPU data (DDS bytes) in D3D11:

  1. Fill D3D11_TEXTURE2D_DESC with width, height, mip count, DXGI format.
  2. Fill D3D11_SUBRESOURCE_DATA for each mip level:
       pSysMem     = pointer into the DDS data bytes for this mip.
       SysMemPitch = bytes per row (for block-compressed: (w/4)*blockSize).
  3. Call ID3D11Device::CreateTexture2D with desc + subresource array.
  4. Call ID3D11Device::CreateShaderResourceView on the texture.

The GPU copies the data at creation time — we do not need to keep the
CPU-side bytes alive after CreateTexture2D returns.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Target: Windows (MSVC)

### DDS Magic Number

**Source:** [`src/engine/rendering/d3d11/d3d11_texture.cpp`](src/engine/rendering/d3d11/d3d11_texture.cpp#L60) (line 60)

All DDS files begin with the 4-byte magic value 0x20534444 ('DDS ').
This lets us quickly reject non-DDS files before parsing the header.
static constexpr uint32_t DDS_MAGIC = 0x20534444u;  // 'DDS '

### Packed structs

**Source:** [`src/engine/rendering/d3d11/d3d11_texture.cpp`](src/engine/rendering/d3d11/d3d11_texture.cpp#L83) (line 83)

#pragma pack ensures no compiler-inserted padding.  DDS structures must
match the byte layout exactly as stored in the file.

### Row Pitch for Block-Compressed Textures

**Source:** [`src/engine/rendering/d3d11/d3d11_texture.cpp`](src/engine/rendering/d3d11/d3d11_texture.cpp#L139) (line 139)

──────────────────────────────────────────────────────────
Block-compressed formats work on 4×4 texel blocks.  The minimum width is
one block (4 texels).  Row pitch = ceil(width/4) * bytesPerBlock.

  BC1 / DXT1 = 8 bytes per 4×4 block (0.5 bytes/texel on average).
  BC3 / DXT5 = 16 bytes per 4×4 block (1 byte/texel on average).
  BC7        = 16 bytes per 4×4 block (same as BC3 but higher quality).
  RGBA8      = 4 bytes per texel.

### Release Order

**Source:** [`src/engine/rendering/d3d11/d3d11_texture.cpp`](src/engine/rendering/d3d11/d3d11_texture.cpp#L242) (line 242)

The SRV holds an internal reference to the Texture2D.  We release the
SRV first so the Texture2D can be destroyed immediately after.
if (m_srv)     { m_srv->Release();     m_srv     = nullptr; }
if (m_sampler) { m_sampler->Release(); m_sampler = nullptr; }
m_width  = 0;
m_height = 0;
m_format = DXGI_FORMAT_UNKNOWN;
}

### Binary File Reading

**Source:** [`src/engine/rendering/d3d11/d3d11_texture.cpp`](src/engine/rendering/d3d11/d3d11_texture.cpp#L261) (line 261)

-----------------------------------------------------------------------
We open in binary mode (std::ios::binary) to prevent the C++ runtime
from translating CR/LF pairs on Windows, which would corrupt the data.
-----------------------------------------------------------------------
std::ifstream file(filePath, std::ios::binary | std::ios::ate);
if (!file.is_open())
{
LOG_ERROR("D3D11Texture::LoadFromFile — file not found: " << filePath);
return false;
}

### DDS Format Detection

**Source:** [`src/engine/rendering/d3d11/d3d11_texture.cpp`](src/engine/rendering/d3d11/d3d11_texture.cpp#L347) (line 347)

──────────────────────────────────────
Old DDS files (pre-DX10) use FourCC codes in DDS_PIXELFORMAT.
New DDS files (DX10 header) carry a DXGI_FORMAT value directly.
We detect which variant we have by checking for the 'DX10' FourCC.
-----------------------------------------------------------------------

### Mip Map Upload

**Source:** [`src/engine/rendering/d3d11/d3d11_texture.cpp`](src/engine/rendering/d3d11/d3d11_texture.cpp#L398) (line 398)

───────────────────────────────
We pass all mip levels to CreateTexture2D in one call.
Each mip level has its own D3D11_SUBRESOURCE_DATA:
  pSysMem     = pointer to the mip's data in the DDS buffer.
  SysMemPitch = bytes per row for this mip's width.

The subresource index is computed as: arraySlice * mipCount + mipIndex.
For a single 2D texture (arraySlice = 0), index == mipIndex.
-----------------------------------------------------------------------

### SRV for Texture2D with Mips

**Source:** [`src/engine/rendering/d3d11/d3d11_texture.cpp`](src/engine/rendering/d3d11/d3d11_texture.cpp#L461) (line 461)

────────────────────────────────────────────
D3D11_SRV_DIMENSION_TEXTURE2D tells D3D11 this is a 2D texture SRV.
MostDetailedMip = 0  → start from the highest-res mip.
MipLevels = -1       → include all mip levels down to the smallest.
-----------------------------------------------------------------------
D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
srvDesc.Format                    = m_format;
srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
srvDesc.Texture2D.MostDetailedMip = 0;
srvDesc.Texture2D.MipLevels       = static_cast<UINT>(-1); // all mips

### Sampler States

**Source:** [`src/engine/rendering/d3d11/d3d11_texture.cpp`](src/engine/rendering/d3d11/d3d11_texture.cpp#L506) (line 506)

-----------------------------------------------------------------------
A D3D11_SAMPLER_DESC controls how the GPU interpolates texels:

  Filter       — how to blend between texels.
                 D3D11_FILTER_ANISOTROPIC uses the surrounding texels
                 weighted by angle of incidence — produces the sharpest
                 results on surfaces viewed at oblique angles (floors,
                 walls).  Bilinear is cheaper but blurrier at angles.

  AddressU/V/W — what happens beyond [0,1] UV:
                 WRAP  = texture tiles (repeat).
                 CLAMP = edge pixel is stretched.
                 MIRROR = alternating flip.

  MaxAnisotropy — quality level for anisotropic filtering (1–16).
                  D3D_FEATURE_LEVEL_10_0 supports up to 16×.
                  GT610 supports 16× anisotropic; use 4–8 for a
                  performance/quality balance on older hardware.

  MipLODBias   — shifts the selected mip up or down.  0 = automatic.
  MinLOD/MaxLOD — clamps the mip level.  0 to FLT_MAX = all mips.
-----------------------------------------------------------------------
D3D11_SAMPLER_DESC sampDesc = {};
sampDesc.Filter         = D3D11_FILTER_ANISOTROPIC;
sampDesc.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
sampDesc.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
sampDesc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
sampDesc.MaxAnisotropy  = 4;          // 4× anisotropic — good quality on GT610.
sampDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
sampDesc.MinLOD         = 0.0f;
sampDesc.MaxLOD         = D3D11_FLOAT32_MAX;

### Why DDS for Game Textures?

**Source:** [`src/engine/rendering/d3d11/d3d11_texture.hpp`](src/engine/rendering/d3d11/d3d11_texture.hpp#L6) (line 6)

============================================================================
PNG and JPEG store full RGBA8 data.  A 2048×2048 RGBA8 texture uses 16 MB
of GPU VRAM.  DDS (DirectDraw Surface) with block compression:

  BC1 (DXT1)  — 4 bits/pixel (8:1 ratio vs RGBA8).  No alpha.
  BC3 (DXT5)  — 8 bits/pixel (4:1 ratio).  Full alpha channel.
  BC7          — 8 bits/pixel (4:1 ratio).  High quality; replaces BC3.

A 2048×2048 BC7 texture uses only 4 MB instead of 16 MB — critical for
GT610-class GPUs which have 1–2 GB VRAM at most.

BC7 is hardware-decompressed on every D3D11_FEATURE_LEVEL_11_0 GPU
(including the GT610) with zero GPU cost.  The CPU never sees the raw pixels.

============================================================================

### DDS File Format

**Source:** [`src/engine/rendering/d3d11/d3d11_texture.hpp`](src/engine/rendering/d3d11/d3d11_texture.hpp#L22) (line 22)

============================================================================
DDS is a Microsoft container format with this layout:

  Offset  Size   Content
  ------  ----   -------
       0     4   Magic: 0x20534444 ('DDS ')
       4   124   DDS_HEADER struct
     128   var   DDS_HEADER_DXT10 (optional, present when FourCC == 'DX10')
     128+  var   Texture data (mip 0, then mip 1, ...; face 0, face 1, ...)

The DDS_HEADER_DXT10 extension (introduced in Direct3D 10) carries the
DXGI_FORMAT enum, enabling formats like BC7 that didn't exist in earlier
DDS revisions.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Target: Windows (MSVC)
Requires: d3d11.lib, dxgi.lib (Windows SDK — always present)

### Resource View Types

**Source:** [`src/engine/rendering/d3d11/d3d11_texture.hpp`](src/engine/rendering/d3d11/d3d11_texture.hpp#L73) (line 73)

──────────────────────────────────────
D3D11 separates resource *creation* from resource *binding*:

  ID3D11Texture2D          — the raw texture data on the GPU.
  ID3D11ShaderResourceView — a "window" into that data that a shader can
                             read via a `Texture2D` or `sampler2D`.
  ID3D11SamplerState       — filtering and wrap mode settings.

Shaders never take raw Texture2D pointers — they always bind through a
ShaderResourceView.  This indirection allows one texture to be bound as
different "views" (e.g. one mip level at a time).

Usage:
@code
  D3D11Texture tex;
  if (tex.LoadFromFile(device, context, "Cooked/textures/hero.dds")) {
      ID3D11ShaderResourceView* srv = tex.GetSRV();
      ID3D11SamplerState*       smp = tex.GetSampler();
      ctx->PSSetShaderResources(0, 1, &srv);
      ctx->PSSetSamplers(0, 1, &smp);
  }
@endcode

### Reading This File

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L6) (line 6)

============================================================================
Read the private helper functions in the order they are called from Init():

  CreateInstance()       — §1
  SetupDebugMessenger()  — §2
  CreateSurface()        — §3
  PickPhysicalDevice()   — §4
  CreateLogicalDevice()  — §5
  CreateSwapchain()      — §6
  CreateImageViews()     — §7
  CreateRenderPass()     — §8
  CreateFramebuffers()   — §9
  CreateCommandPool()    — §10
  CreateCommandBuffers() — §11
  CreateSyncObjects()    — §12

Then read DrawFrame() to see how a single frame is rendered.

@author  Educational Game Engine Project
@version 1.0
@date    2024

### VK_CHECK

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L65) (line 65)

Vulkan functions return VkResult.  VK_SUCCESS = 0; anything else is a
warning or error.  This macro prints the failing call and returns false
from the enclosing function, which propagates the failure up to Init().
---------------------------------------------------------------------------
define VK_CHECK(call)                                                     \
do {                                                                   \
VkResult _result = (call);                                         \
if (_result != VK_SUCCESS) {                                       \
std::cerr << "[VulkanRenderer] " #call " failed with VkResult=" \
<< _result << " (" << __FILE__ << ":" << __LINE__    \
<< ")\n";                                            \
return false;                                                  \
}                                                                  \
} while (0)

### VkInstance

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L84) (line 84)

The instance is the connection between your application and the Vulkan
runtime library (vulkan-1.dll on Windows).  It holds global state (layers,
extensions) and is the root object from which all other Vulkan objects
are created.

Required instance extensions for a Win32 window:
  VK_KHR_surface           — base surface extension (platform-agnostic)
  VK_KHR_win32_surface     — Win32-specific surface creation
  VK_EXT_debug_utils       — debug callbacks (Debug builds only)
===========================================================================
bool VulkanRenderer::CreateInstance()
{
-----------------------------------------------------------------------

### VkApplicationInfo

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L98) (line 98)

This struct is purely informational — the driver uses it for crash
reports and optimisation hints.  The important field is apiVersion:
request Vulkan 1.2 which is widely supported on modern hardware.
-----------------------------------------------------------------------
VkApplicationInfo appInfo  = {};
appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
appInfo.pApplicationName   = "EngineSandbox";
appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
appInfo.pEngineName        = "Educational Game Engine";
appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
appInfo.apiVersion         = VK_API_VERSION_1_2;

### Validation Layer Check

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L112) (line 112)

Before requesting validation layers, confirm the Vulkan runtime actually
has them installed.  If the Vulkan SDK is not installed, or the SDK
version is too old, the layer list may be empty.

We perform this check BEFORE building the extension list so we know
whether to request VK_EXT_debug_utils (only needed when layers are on).

When layers are unavailable we degrade gracefully: log a warning and
continue without validation.  This lets headless CI runners (which have
the Vulkan loader but not the full SDK) pass the bootstrap test while
developers on a full Vulkan SDK installation still get validation.
-----------------------------------------------------------------------
if (kEnableValidationLayers)
{
uint32_t layerCount = 0;
vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
std::vector<VkLayerProperties> available(layerCount);
vkEnumerateInstanceLayerProperties(&layerCount, available.data());

### Extension Functions

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L190) (line 190)

VK_EXT_debug_utils adds new Vulkan functions that are not in the core API.
You must load them at runtime via vkGetInstanceProcAddr (similar to
GetProcAddress on Win32 or dlsym on Linux).  We load them once here and
use them to create/destroy the debug messenger.
===========================================================================
bool VulkanRenderer::SetupDebugMessenger()
{
if (!m_validationLayersActive)
return true;   // Nothing to do when validation layers are off.

### Message Severity and Type Filters

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L214) (line 214)

We subscribe to ERROR and WARNING messages from all message types
(general Vulkan issues, validation layer checks, and performance hints).
VERBOSE / INFO floods the console so we leave them off by default.
-----------------------------------------------------------------------
VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {};
messengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
messengerInfo.messageSeverity =
VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
messengerInfo.messageType =
VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT     |
VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT  |
VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
messengerInfo.pfnUserCallback = VulkanRenderer::DebugCallback;
messengerInfo.pUserData       = nullptr;

### VkSurfaceKHR

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L245) (line 245)

A surface is the abstract "window connection" that Vulkan uses to present
rendered images.  On Windows we use VK_KHR_win32_surface to attach the
surface directly to our HWND (window handle).
===========================================================================
bool VulkanRenderer::CreateSurface(HINSTANCE hinstance, HWND hwnd)
{
VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
surfaceInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
surfaceInfo.hinstance = hinstance;
surfaceInfo.hwnd      = hwnd;

### Physical Device Selection

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L266) (line 266)

A machine may have multiple GPUs (e.g. integrated + discrete).  We
enumerate all of them and pick the first one that:
  a) Has a graphics queue family.
  b) Has a present queue family for our surface.
  c) Supports the required extensions (e.g. VK_KHR_swapchain).
  d) Has at least one swapchain format and one present mode.
A production engine would score devices (prefer discrete over integrated,
prefer larger VRAM, etc.) but for teaching "first suitable" is fine.
===========================================================================
bool VulkanRenderer::PickPhysicalDevice()
{
uint32_t deviceCount = 0;
vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
if (deviceCount == 0)
{
std::cerr << "[VulkanRenderer] No Vulkan-capable GPU found.\n";
return false;
}

### Logical Device vs Physical Device

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L313) (line 313)

Physical device  = the actual GPU chip.
Logical device   = your application's interface to that GPU.
                   You choose which features and queues to enable.

We create one logical device per physical device.  Most apps only have one.
The logical device also gives us handles to the graphics and present queues.
===========================================================================
bool VulkanRenderer::CreateLogicalDevice()
{
QueueFamilyIndices indices = FindQueueFamilies(m_physicalDevice);

### Unique Queue Families

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L326) (line 326)

If the graphics family and present family are the same (common on
desktop GPUs) we only need one VkDeviceQueueCreateInfo.  Using a set
deduplicates the indices automatically.
-----------------------------------------------------------------------
std::set<uint32_t> uniqueFamilies = {
indices.graphicsFamily,
indices.presentFamily
};

### VkPhysicalDeviceFeatures

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L349) (line 349)

This struct enables optional GPU features (geometry shaders, multi-viewport,
etc.).  We request none for the minimal clear-screen starter.
VkPhysicalDeviceFeatures deviceFeatures = {};

### Swapchain

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L375) (line 375)

The swapchain is a ring of images.  The GPU renders into one image while
the display controller scans out another.  When the rendered image is
ready, "present" swaps it to the front so the user sees the new frame.

Key swapchain parameters we choose:
  format      — pixel layout (e.g. BGRA8_UNORM) and colour space (SRGB).
  presentMode — how to handle frames: FIFO = vsync, Mailbox = triple buffer.
  extent      — resolution of the swapchain images (matches window size).
  imageCount  — how many images in the ring (min+1 to avoid stalling).
===========================================================================
bool VulkanRenderer::CreateSwapchain(uint32_t width, uint32_t height)
{
SwapchainSupportDetails support = QuerySwapchainSupport(m_physicalDevice);

### Image Count

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L395) (line 395)

We request minImageCount + 1 to ensure the CPU can keep encoding while
the GPU is presenting the previous frame (pipelining).  Clamp to
maxImageCount (0 = no limit).
-----------------------------------------------------------------------
uint32_t imageCount = support.capabilities.minImageCount + 1;
if (support.capabilities.maxImageCount > 0 &&
imageCount > support.capabilities.maxImageCount)
{
imageCount = support.capabilities.maxImageCount;
}

### Queue Sharing Mode

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L418) (line 418)

If graphics and present queues are from different families, images
must be shared (VK_SHARING_MODE_CONCURRENT).  If they are the same
family, exclusive is more efficient.
-----------------------------------------------------------------------
QueueFamilyIndices indices = FindQueueFamilies(m_physicalDevice);
uint32_t queueFamilyIndices[] = {
indices.graphicsFamily,
indices.presentFamily
};

### VkImageView

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L467) (line 467)

A VkImage is raw GPU memory.  A VkImageView describes HOW to interpret
that memory — which mip-levels, array layers, and format to use.  You
never bind a VkImage directly to a framebuffer; you always use an image view.
===========================================================================
bool VulkanRenderer::CreateImageViews()
{
m_swapchainImageViews.resize(m_swapchainImages.size());

### Component Swizzle

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L484) (line 484)

We use IDENTITY so R→R, G→G, B→B, A→A.  You could remap channels
here (e.g. monochrome by routing R into all channels).
viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

### Render Pass

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L515) (line 515)

A render pass describes the "shape" of a rendering operation:
  • What attachments are written to (colour, depth, stencil)?
  • How should they be loaded at the start of the pass (CLEAR / LOAD / DONT_CARE)?
  • How should they be stored at the end (STORE / DONT_CARE)?
  • How does the GPU transition image layouts through the pass?

For our clear-screen demo we have ONE colour attachment.
LOAD_OP_CLEAR means "clear the attachment to a solid colour at pass start"
— this is how the background colour is produced.
===========================================================================
bool VulkanRenderer::CreateRenderPass()
{
-----------------------------------------------------------------------

### VkAttachmentDescription

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L529) (line 529)

format       — must match the swapchain image format.
samples      — MSAA sample count; 1 = no anti-aliasing.
loadOp       — CLEAR: GPU zeros the attachment at render pass start.
storeOp      — STORE: GPU writes results to memory (needed for present).
initialLayout — image layout BEFORE the pass begins.
finalLayout   — image layout AFTER the pass.
  PRESENT_SRC_KHR = layout required for vkQueuePresentKHR.
-----------------------------------------------------------------------
VkAttachmentDescription colorAttachment = {};
colorAttachment.format         = m_swapchainFormat;
colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

### Subpass

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L549) (line 549)

A render pass can have multiple "subpasses" (e.g. G-buffer fill, then
lighting), allowing the GPU driver to optimise data flow between them
using "tile memory" on mobile GPUs.  For our demo, one subpass suffices.
-----------------------------------------------------------------------
VkAttachmentReference colorRef = {};
colorRef.attachment = 0;   // Index into the attachment descriptions array
colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

### Subpass Dependency

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L564) (line 564)

Subpass dependencies declare memory and execution ordering between
subpasses (or between the render pass and external commands).

Here we express: "wait until the previous frame's image has been
released from the present engine (COLOR_ATTACHMENT_OUTPUT stage) before
writing colour to the attachment in this frame."
-----------------------------------------------------------------------
VkSubpassDependency dependency = {};
dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;   // "before this render pass"
dependency.dstSubpass    = 0;
dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
dependency.srcAccessMask = 0;
dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

### Framebuffer

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L596) (line 596)

A framebuffer is the collection of image views that the render pass writes
to.  We create one framebuffer per swapchain image view so the GPU knows
WHICH image to write to for each acquired swapchain slot.
===========================================================================
bool VulkanRenderer::CreateFramebuffers()
{
m_framebuffers.resize(m_swapchainImageViews.size());

### Command Pool

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L631) (line 631)

Commands are recorded into VkCommandBuffers, which are allocated from a
VkCommandPool.  The pool manages a chunk of GPU memory for command storage.
Command pools are NOT thread-safe; each thread that records commands needs
its own pool.  For this single-threaded demo, one pool is fine.

VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT allows us to reset and
re-record individual command buffers rather than resetting the entire pool.
===========================================================================
bool VulkanRenderer::CreateCommandPool()
{
QueueFamilyIndices indices = FindQueueFamilies(m_physicalDevice);

### Command Buffers

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L656) (line 656)

We allocate one command buffer per swapchain image.  In DrawFrame we will
pick the command buffer that corresponds to the acquired image index,
reset it, and record a new clear command into it.

VK_COMMAND_BUFFER_LEVEL_PRIMARY means these can be submitted to a queue
directly.  "Secondary" command buffers are called from primaries
(useful for multi-threaded recording, out of scope here).
===========================================================================
bool VulkanRenderer::CreateCommandBuffers()
{
m_commandBuffers.resize(m_swapchainImages.size());

### Synchronisation Overview

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L682) (line 682)

Vulkan gives you explicit, fine-grained synchronisation.  We use:

  VkSemaphore — GPU-side signal.  Used to order GPU operations:
    • m_imageAvailableSemaphores[f]: "the swapchain image is ready to write to"
      (signalled by vkAcquireNextImageKHR, waited on by vkQueueSubmit).
    • m_renderFinishedSemaphores[f]: "the GPU has finished rendering"
      (signalled by vkQueueSubmit, waited on by vkQueuePresentKHR).

  VkFence — CPU-visible gate.  Used to prevent the CPU from submitting
    frame N+MAX while the GPU is still processing it:
    • m_inFlightFences[f]: signalled when the GPU finishes the frame.
      The CPU waits on it at the START of DrawFrame.

### Frames in Flight vs Swapchain Image Index

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L696) (line 696)

m_currentFrame cycles through 0 … kMaxFramesInFlight-1 (the "slot").
imageIndex is the index returned by vkAcquireNextImageKHR (may be
different from m_currentFrame if the swapchain has more images).
m_imagesInFlight[imageIndex] tracks which in-flight fence was LAST used
for that swapchain image, to avoid writing to an image still being presented.
===========================================================================
bool VulkanRenderer::CreateSyncObjects()
{
m_imagesInFlight.assign(m_swapchainImages.size(), VK_NULL_HANDLE);

### VK_FENCE_CREATE_SIGNALED_BIT

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L710) (line 710)

Fences start unsignalled by default.  If we wait on an unsignalled fence
before the first DrawFrame(), we deadlock.  Creating them pre-signalled
means the first vkWaitForFences returns immediately.
VkFenceCreateInfo fenceInfo = {};
fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

### headless parameter

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L738) (line 738)

The Vulkan renderer does not currently support a swap-chain-less headless
mode (that requires VK_KHR_offscreen_surface or a render-to-texture path).
We accept the parameter for interface conformance but ignore it: the full
Vulkan init chain (including swap chain) always runs.  When Vulkan is used
in CI headless mode, the CI job is marked continue-on-error: true because
GitHub-hosted runners may lack a Vulkan ICD.
m_hinstance = hinstance;
m_hwnd      = hwnd;

### Per-Frame Rendering

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L769) (line 769)

Every frame follows the same six steps (see DrawFrame() in VulkanRenderer.hpp).
===========================================================================
void VulkanRenderer::DrawFrame(float clearR, float clearG, float clearB)
{
if (!m_initialised) return;

### vkWaitForFences

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L779) (line 779)

We wait with a 1-second timeout.  In practice the GPU should finish
well within a single display refresh period.
-----------------------------------------------------------------------
vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame],
VK_TRUE, UINT64_MAX);

### vkAcquireNextImageKHR

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L789) (line 789)

This asks the swapchain "which image can I write to next?"
m_imageAvailableSemaphores is signalled when the image is truly ready
(i.e. the display has finished reading the previous frame from it).
The returned imageIndex might differ from m_currentFrame.
-----------------------------------------------------------------------
uint32_t imageIndex = 0;
VkResult acquireResult = vkAcquireNextImageKHR(
m_device, m_swapchain, UINT64_MAX,
m_imageAvailableSemaphores[m_currentFrame],
VK_NULL_HANDLE,
&imageIndex);

### vkQueueSubmit

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L838) (line 838)

We tell the GPU:
  "Wait on imageAvailableSemaphore (stage COLOR_ATTACHMENT_OUTPUT)
   before writing colour pixels, then execute the command buffer,
   then signal renderFinishedSemaphore."
After submission we reset and signal inFlightFence when done so
the next DrawFrame() call for this slot can proceed.
-----------------------------------------------------------------------
VkPipelineStageFlags waitStages[] = {
VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
};

### vkQueuePresentKHR

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L872) (line 872)

Wait for renderFinishedSemaphore (i.e. the GPU finished drawing), then
flip the swapchain image to the screen (or hand it back to the
compositor on platforms like Windows with DWM).
-----------------------------------------------------------------------
VkPresentInfoKHR presentInfo = {};
presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
presentInfo.waitSemaphoreCount = 1;
presentInfo.pWaitSemaphores    = &m_renderFinishedSemaphores[m_currentFrame];
presentInfo.swapchainCount     = 1;
presentInfo.pSwapchains        = &m_swapchain;
presentInfo.pImageIndices      = &imageIndex;

### Extracted Recording

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L900) (line 900)

We moved command buffer recording into its own helper so:
  1. DrawFrame() is shorter and easier to follow.
  2. RecordHeadlessFrame() can validate recording without a full frame.

The function records:
  • vkBeginCommandBuffer
  • vkCmdBeginRenderPass (LOAD_OP_CLEAR provides the animated background)
  • vkCmdSetViewport / vkCmdSetScissor   (dynamic state, set each frame)
  • vkCmdBindPipeline + vkCmdBindVertexBuffers + vkCmdDraw (if scene loaded)
  • vkCmdEndRenderPass
  • vkEndCommandBuffer
===========================================================================
void VulkanRenderer::RecordCommandBuffer(VkCommandBuffer cmdBuf,
VkFramebuffer   framebuffer,
float           clearR,
float           clearG,
float           clearB) const
{
Begin recording.
VkCommandBufferBeginInfo beginInfo = {};
beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
vkBeginCommandBuffer(cmdBuf, &beginInfo);

### VkClearValue

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L925) (line 925)

LOAD_OP_CLEAR fills the attachment with this colour before any draw
commands run.  The animated RGB values produce the rainbow background.
-----------------------------------------------------------------------
VkClearValue clearValue = {};
clearValue.color.float32[0] = clearR;
clearValue.color.float32[1] = clearG;
clearValue.color.float32[2] = clearB;
clearValue.color.float32[3] = 1.0f;  // alpha = fully opaque

### Dynamic Viewport and Scissor

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L951) (line 951)

Because we declared these as dynamic pipeline states, we set them
here in the command buffer instead of baking them into the PSO.
This means window resizes only need to update these commands, not
recreate the whole pipeline.
VkViewport viewport = {};
viewport.x        = 0.0f;
viewport.y        = 0.0f;
viewport.width    = static_cast<float>(m_swapchainExtent.width);
viewport.height   = static_cast<float>(m_swapchainExtent.height);
viewport.minDepth = 0.0f;
viewport.maxDepth = 1.0f;
vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

### Scene Dispatch Pattern

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L982) (line 982)

This thin dispatcher will grow as milestones add more scene types.
Each scene name maps to a private loader that creates the appropriate
pipeline, meshes, and resources for that scene.
===========================================================================
bool VulkanRenderer::LoadScene(const std::string& sceneName,
const std::string& shaderDir)
{
if (sceneName == "triangle")
return LoadTriangleScene(shaderDir);

### NDC Coordinates for a Visible Triangle

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L1018) (line 1018)

The triangle is positioned in Vulkan NDC space:
  top vertex    (0,  -0.5) = centre, near top   → red
  right vertex  (0.5, 0.5) = lower right        → green
  left vertex  (-0.5, 0.5) = lower left         → blue

In Vulkan NDC, y = -1 is the TOP of the screen, y = +1 is the BOTTOM
(opposite of OpenGL).  These coordinates produce a triangle that is
centred and fills roughly 1/4 of the screen.
-----------------------------------------------------------------------
const std::vector<Vertex> vertices = {
{{ 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},  // top   — red
{{ 0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},  // right — green
{{-0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},  // left  — blue
};

### Headless Validation

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L1050) (line 1050)

In CI (no display) we cannot present frames, but we CAN validate that:
  • The pipeline was created successfully.
  • The mesh was uploaded.
  • A command buffer can be recorded with the draw call.
This method records commandBuffer[0] using framebuffer[0] and a fixed
black clear colour, then returns without submitting.
===========================================================================
bool VulkanRenderer::RecordHeadlessFrame()
{
if (m_commandBuffers.empty() || m_framebuffers.empty())
{
std::cerr << "[VulkanRenderer] No command buffers / framebuffers for headless record.\n";
return false;
}

### Swapchain Recreation

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L1077) (line 1077)

On window resize or DPI change, the old swapchain is invalidated.  We must:
  1. Wait for the GPU to finish all in-flight work (vkDeviceWaitIdle).
  2. Destroy swapchain-dependent objects (framebuffers, image views, swapchain).
  3. Recreate them with the new dimensions.
Command buffers and sync objects are NOT swapchain-dependent (they survive).
===========================================================================
void VulkanRenderer::RecreateSwapchain(uint32_t newWidth, uint32_t newHeight)
{
Minimised window — width or height can be 0; skip until restored.
if (newWidth == 0 || newHeight == 0) return;

### vkDeviceWaitIdle

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L1132) (line 1132)

Before destroying ANY Vulkan object, ensure the GPU has finished all
submitted work.  Destroying a semaphore or fence that the GPU is still
waiting on is undefined behaviour.
vkDeviceWaitIdle(m_device);

### Only print ERROR and WARNING (we filtered INFO/VERBOSE

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L1219) (line 1219)

in SetupDebugMessenger, but this is a secondary guard).
if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
{
std::cerr << "[VulkanValidation] " << pCallbackData->pMessage << "\n";
}

### Surface Format

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L1292) (line 1292)

Prefer BGRA8_SRGB with SRGB colour space because:
  • BGRA8 maps directly to the monitor's 8-bit-per-channel BGRA layout.
  • sRGB colour space means the hardware automatically gamma-corrects the
    final image so colours look correct on standard monitors.
===========================================================================
VkSurfaceFormatKHR VulkanRenderer::ChooseSwapSurfaceFormat(
const std::vector<VkSurfaceFormatKHR>& available) const
{
for (const auto& f : available)
{
if (f.format     == VK_FORMAT_B8G8R8A8_SRGB &&
f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
{
return f;
}
}
Fallback: just use whatever the driver prefers.
return available[0];
}

### Present Modes

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L1316) (line 1316)

FIFO           — VSync; images queued; guaranteed to exist on all drivers.
  Mailbox        — Triple buffering; most recently rendered image is always
                   shown; minimal latency without tearing.  Prefer if available.
  Immediate      — No sync; frame is shown immediately; may tear.
===========================================================================
VkPresentModeKHR VulkanRenderer::ChooseSwapPresentMode(
const std::vector<VkPresentModeKHR>& available) const
{
for (const auto& mode : available)
{
if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
return mode;
}
FIFO is always available (Vulkan spec guarantees it).
return VK_PRESENT_MODE_FIFO_KHR;
}

### Swap Extent

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.cpp`](src/engine/rendering/vulkan/VulkanRenderer.cpp#L1337) (line 1337)

On high-DPI (HiDPI / Retina) displays, the framebuffer resolution can
differ from the window's "logical" size.  We use the capabilities' current
extent when available, otherwise clamp our desired size to the min/max range.
===========================================================================
VkExtent2D VulkanRenderer::ChooseSwapExtent(
const VkSurfaceCapabilitiesKHR& capabilities,
uint32_t desiredWidth, uint32_t desiredHeight) const
{
if (capabilities.currentExtent.width != UINT32_MAX)
{
The surface dictates the exact size (most platforms).
return capabilities.currentExtent;
}

### Why Vulkan?

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.hpp`](src/engine/rendering/vulkan/VulkanRenderer.hpp#L6) (line 6)

============================================================================
Vulkan is a modern, explicit GPU API created by the Khronos Group.
Unlike OpenGL (which hides GPU state management behind a driver), Vulkan
makes EVERY detail explicit:

  • You create the instance, device, queues, swapchain manually.
  • You record command buffers and submit them yourself.
  • You manage synchronisation with semaphores and fences.
  • You decide memory layouts, pipeline stages, and image transitions.

This verbosity is a feature for engine programmers: zero surprises, zero
hidden driver work.  It also maps directly to how modern GPUs actually work.

============================================================================

### Vulkan "Bootstrap" Pattern

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.hpp`](src/engine/rendering/vulkan/VulkanRenderer.hpp#L21) (line 21)

============================================================================
Every Vulkan application goes through the same setup sequence:

  1.  Instance          — connects your app to the Vulkan runtime.
  2.  Debug messenger   — routes validation layer messages to our callback.
  3.  Surface           — a renderable window (here: VkSurfaceKHR via Win32).
  4.  Physical device   — pick the GPU you want to render on.
  5.  Logical device    — an interface to the physical device + queue handles.
  6.  Swapchain         — a ring of presentable images tied to the window.
  7.  Image views       — how Vulkan "sees" each swapchain image.
  8.  Render pass       — describes the attachments (color, depth) and how
                          they are loaded/stored each frame.
  9.  Framebuffers      — binds each image view to the render pass.
 10.  Command pool/buffers — recorded GPU work packets.
 11.  Sync primitives   — semaphores + fences for CPU↔GPU coordination.

DrawFrame() ties it all together:
  acquire → record clear → submit → present

============================================================================

### Validation Layers

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.hpp`](src/engine/rendering/vulkan/VulkanRenderer.hpp#L42) (line 42)

============================================================================
In Debug builds we enable VK_LAYER_KHRONOS_validation.  This layer runs
inside the Vulkan driver and checks every API call for misuse — wrong
argument types, missing barriers, use-after-free of handles, etc.
It has a real CPU cost, so it is disabled in Release builds.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Requires: Vulkan SDK (https://vulkan.lunarg.com/)

### Vulkan + Win32 Surface Headers

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.hpp`](src/engine/rendering/vulkan/VulkanRenderer.hpp#L61) (line 61)

---------------------------------------------------------------------------
<vulkan/vulkan.h> — core Vulkan types and functions.
VK_USE_PLATFORM_WIN32_KHR must be defined BEFORE including vulkan.h so that
the Win32-specific declarations (VkWin32SurfaceCreateInfoKHR etc.) are
included.  We define it here; the CMakeLists also passes it as a compile
definition to be safe.
The #ifndef guard avoids MSVC C4005 "macro redefinition" warnings when both
the CMake compile definition AND this header define the same macro.
---------------------------------------------------------------------------
ifndef VK_USE_PLATFORM_WIN32_KHR
define VK_USE_PLATFORM_WIN32_KHR
endif
ifndef WIN32_LEAN_AND_MEAN
define WIN32_LEAN_AND_MEAN
endif
ifndef NOMINMAX
define NOMINMAX
endif
include <windows.h>
include <vulkan/vulkan.h>

### Frames in Flight

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.hpp`](src/engine/rendering/vulkan/VulkanRenderer.hpp#L107) (line 107)

"Frames in flight" means we allow the CPU to record frame N+1 while the
GPU is still rendering frame N.  2 is a common choice: more than 2 adds
latency without much throughput gain.
static constexpr uint32_t kMaxFramesInFlight = 2;

### Object Ownership

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.hpp`](src/engine/rendering/vulkan/VulkanRenderer.hpp#L120) (line 120)

Each VkXxx handle is just an opaque integer (64 bits on 64-bit platforms).
Vulkan NEVER automatically destroys anything — you must call the matching
vkDestroyXxx function.  Missing even one destroy call is a resource leak.
We use explicit Shutdown() rather than destructors here so the order of
destruction is visible and teachable.

### IRenderer interface

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.hpp`](src/engine/rendering/vulkan/VulkanRenderer.hpp#L127) (line 127)

VulkanRenderer implements IRenderer (engine/rendering/IRenderer.hpp) so it
can be selected by RendererFactory at runtime via --renderer vulkan.
The D3D11Renderer implements the same interface as the default backend.
===========================================================================
class VulkanRenderer : public IRenderer
{
public:

### Constructor and Destructor Out-of-Line for Incomplete Types

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.hpp`](src/engine/rendering/vulkan/VulkanRenderer.hpp#L135) (line 135)

Both the constructor AND the destructor must be *defined* in
VulkanRenderer.cpp (not here in the header) because m_pipeline and
m_triangleMesh are std::unique_ptr<T> where T is only *forward-declared*
in this header.

Why does this matter?
  std::unique_ptr<T>::~unique_ptr() calls delete on T, which requires the
  *complete* definition of T to call its destructor.  MSVC enforces this
  strictly: if the compiler encounters a `= default` constructor or
  destructor definition in the header (where T is incomplete), it will
  immediately try to instantiate unique_ptr<T>::~unique_ptr() → error
  C2027 (incomplete type) + C2338 (static_assert: can't delete incomplete
  type).

The fix: declare both here; define both in the .cpp where the full
vulkan_mesh.hpp and vulkan_pipeline.hpp are already included.
VulkanRenderer();
~VulkanRenderer() override;

### Frame Lifecycle

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.hpp`](src/engine/rendering/vulkan/VulkanRenderer.hpp#L187) (line 187)

1. vkWaitForFences        — stall the CPU until the GPU finished
                              the *previous* use of this frame slot.
  2. vkAcquireNextImageKHR  — ask the swapchain for the next image.
  3. vkResetCommandBuffer   — clear the previously recorded commands.
  4. Record: begin render pass (LOAD_OP_CLEAR = GPU clears for us),
             end render pass.
  5. vkQueueSubmit          — send the command buffer to the GPU.
  6. vkQueuePresentKHR      — flip the rendered image to the screen.

@param clearR  Red channel of the clear colour   (0.0 – 1.0).
@param clearG  Green channel of the clear colour.
@param clearB  Blue channel of the clear colour.

### Scene Loading Pattern

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.hpp`](src/engine/rendering/vulkan/VulkanRenderer.hpp#L214) (line 214)

For M1 the "scene" is just a hardcoded triangle.  In later milestones
LoadScene will parse a scene JSON file from the asset DB and spawn
entities, load meshes, and wire up scripts.

### Pre-recording vs per-frame recording

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.hpp`](src/engine/rendering/vulkan/VulkanRenderer.hpp#L284) (line 284)

For a fully static scene (triangle with no animated clear colour) we
could pre-record once.  We keep per-frame recording here for two reasons:
  1. DrawFrame already re-records to animate the clear colour.
  2. Pre-recording with the clear-colour animation would require a
     separate "dynamic clear" mechanism (push constants or a UBO).

### Queue Family Indices

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.hpp`](src/engine/rendering/vulkan/VulkanRenderer.hpp#L298) (line 298)

-----------------------------------------------------------------------
A "queue" in Vulkan is a submission channel to the GPU.  Different queues
can run in parallel.  Queues belong to "families" — a family declares
which operations it supports (graphics, compute, transfer, present).
We need at least one family that supports GRAPHICS and one that supports
PRESENT (they are often the same family on desktop hardware).
-----------------------------------------------------------------------
struct QueueFamilyIndices
{
uint32_t graphicsFamily = UINT32_MAX;  ///< UINT32_MAX = not found.
uint32_t presentFamily  = UINT32_MAX;

### Swapchain Support Details

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.hpp`](src/engine/rendering/vulkan/VulkanRenderer.hpp#L321) (line 321)

-----------------------------------------------------------------------
Before creating a swapchain we must query three things:
  capabilities — min/max image count, extent range, transform flags.
  formats      — pixel format + colour space combos the surface supports.
  presentModes — how images are queued for display (FIFO, Mailbox, etc.).
-----------------------------------------------------------------------
struct SwapchainSupportDetails
{
VkSurfaceCapabilitiesKHR        capabilities = {};
std::vector<VkSurfaceFormatKHR> formats;
std::vector<VkPresentModeKHR>   presentModes;
};

### Runtime Validation Layer Flag

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.hpp`](src/engine/rendering/vulkan/VulkanRenderer.hpp#L394) (line 394)

kEnableValidationLayers is a compile-time constant that enables the
validation path in Debug builds.  m_validationLayersActive is a runtime
override that is set to false when the validation layer is not installed
in the current environment (e.g., a CI runner with Vulkan libraries but
no full SDK).  This lets headless CI tests pass without the SDK while
developers on a full Vulkan SDK installation still get validation.
-----------------------------------------------------------------------
bool        m_validationLayersActive = kEnableValidationLayers;

### std::unique_ptr for Optional Subsystems

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.hpp`](src/engine/rendering/vulkan/VulkanRenderer.hpp#L411) (line 411)

The pipeline and mesh may or may not exist (they are created only when
LoadScene() is called).  Using unique_ptr<T> models "optionally present"
cleanly: nullptr = not loaded, non-null = active.  The destructor auto-
calls Destroy() through the unique_ptr deleter.
std::unique_ptr<VulkanPipeline> m_pipeline;    ///< Graphics PSO (null until LoadScene).
std::unique_ptr<VulkanMesh>     m_triangleMesh; ///< Triangle geometry (null until LoadScene).

### Debug vs Release Validation

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.hpp`](src/engine/rendering/vulkan/VulkanRenderer.hpp#L429) (line 429)

kEnableValidationLayers is a compile-time constant so the compiler can
dead-code-eliminate all validation setup in Release builds.
static constexpr bool kEnableValidationLayers = true;
endif

### Debug Callback Signature

**Source:** [`src/engine/rendering/vulkan/VulkanRenderer.hpp`](src/engine/rendering/vulkan/VulkanRenderer.hpp#L446) (line 446)

The signature must exactly match PFN_vkDebugUtilsMessengerCallbackEXT.
We print ERROR and WARNING messages; verbose INFO messages are ignored
to keep the output readable.

### VkBufferCreateInfo

**Source:** [`src/engine/rendering/vulkan/vulkan_buffer.cpp`](src/engine/rendering/vulkan/vulkan_buffer.cpp#L21) (line 21)

Most Vulkan "CreateInfo" structs follow the same pattern:
  sType    — identifies the struct type for Vulkan's type-safe C API.
  pNext    — extension chain pointer; nullptr unless using extensions.
  flags    — mostly unused for basic buffers.
  size     — byte size of the buffer.
  usage    — bitmask of how the buffer will be used (vertex, index, uniform…).
  sharingMode — EXCLUSIVE means only one queue family accesses it at a time.
===========================================================================
bool VulkanBuffer::Create(VkDevice              device,
VkPhysicalDevice      physDevice,
VkDeviceSize          size,
VkBufferUsageFlags    usage,
VkMemoryPropertyFlags properties)
{
m_device = device;
m_size   = size;

### Buffer vs Memory

**Source:** [`src/engine/rendering/vulkan/vulkan_buffer.cpp`](src/engine/rendering/vulkan/vulkan_buffer.cpp#L42) (line 42)

A VkBuffer is just a descriptor (usage, size, format).  It has NO
memory yet.  Memory is allocated separately and "bound" in step 3.
This separation lets you sub-allocate a large VkDeviceMemory block
across many buffers — important for performance in real engines.
-----------------------------------------------------------------------
VkBufferCreateInfo bufInfo = {};
bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
bufInfo.size        = size;
bufInfo.usage       = usage;
bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

### VkMemoryRequirements

**Source:** [`src/engine/rendering/vulkan/vulkan_buffer.cpp`](src/engine/rendering/vulkan/vulkan_buffer.cpp#L64) (line 64)

After buffer creation, the driver knows:
  size          — actual bytes needed (may be larger than requested due
                  to alignment constraints).
  alignment     — buffer must start at a multiple of this value.
  memoryTypeBits — bitmask of memory types this buffer can be bound to.
-----------------------------------------------------------------------
VkMemoryRequirements memReqs = {};
vkGetBufferMemoryRequirements(m_device, m_buffer, &memReqs);

### vkBindBufferMemory

**Source:** [`src/engine/rendering/vulkan/vulkan_buffer.cpp`](src/engine/rendering/vulkan/vulkan_buffer.cpp#L105) (line 105)

The third parameter is the byte offset into the VkDeviceMemory block.
Using 0 means the buffer starts at the beginning of the allocation.
In a pool allocator, multiple buffers share one big VkDeviceMemory and
each uses a different offset.
-----------------------------------------------------------------------
result = vkBindBufferMemory(m_device, m_buffer, m_memory, 0);
if (result != VK_SUCCESS)
{
std::cerr << "[VulkanBuffer] vkBindBufferMemory failed: " << result << "\n";
vkFreeMemory(m_device, m_memory, nullptr);
vkDestroyBuffer(m_device, m_buffer, nullptr);
m_memory = VK_NULL_HANDLE;
m_buffer = VK_NULL_HANDLE;
return false;
}

### vkMapMemory / vkUnmapMemory

**Source:** [`src/engine/rendering/vulkan/vulkan_buffer.cpp`](src/engine/rendering/vulkan/vulkan_buffer.cpp#L129) (line 129)

For HOST_VISIBLE memory only.
vkMapMemory returns a CPU pointer into the GPU memory.  We memcpy our
vertex data into it, then unmap.  Because the memory is HOST_COHERENT,
no explicit flush is needed — the GPU sees the write immediately.
===========================================================================
void VulkanBuffer::Upload(const void* data, VkDeviceSize size)
{
if (!m_initialised || !data || size == 0) return;

### Destruction Order

**Source:** [`src/engine/rendering/vulkan/vulkan_buffer.cpp`](src/engine/rendering/vulkan/vulkan_buffer.cpp#L148) (line 148)

The buffer must be destroyed BEFORE the memory.  Destroying the memory
while the buffer still references it is undefined behaviour.
===========================================================================
void VulkanBuffer::Destroy()
{
if (!m_initialised) return;
m_initialised = false;

### Memory Type Selection

**Source:** [`src/engine/rendering/vulkan/vulkan_buffer.cpp`](src/engine/rendering/vulkan/vulkan_buffer.cpp#L172) (line 172)

vkGetPhysicalDeviceMemoryProperties returns a table of memory types, each
belonging to a heap (e.g. "256 MB VRAM", "32 GB system RAM").  We iterate
and find the first type whose index bit is set in typeBits AND whose flags
include all of our required property flags.
===========================================================================
uint32_t VulkanBuffer::FindMemoryType(VkPhysicalDevice      physDevice,
uint32_t              typeBits,
VkMemoryPropertyFlags properties)
{
VkPhysicalDeviceMemoryProperties memProps = {};
vkGetPhysicalDeviceMemoryProperties(physDevice, &memProps);

### Why a Separate Buffer Class?

**Source:** [`src/engine/rendering/vulkan/vulkan_buffer.hpp`](src/engine/rendering/vulkan/vulkan_buffer.hpp#L6) (line 6)

============================================================================
Every piece of data the GPU reads (vertex positions, uniform matrices,
index arrays, …) lives in a VkBuffer.  Creating a buffer involves three
steps that must happen together:
  1. Create the VkBuffer descriptor.
  2. Query its memory requirements (size, alignment, type constraints).
  3. Allocate VkDeviceMemory and bind it to the buffer.

Wrapping this in a RAII class (VulkanBuffer) prevents the common bug of
forgetting step 3, or destroying objects in the wrong order.

============================================================================

### M1 Simplification: Host-Visible Memory

**Source:** [`src/engine/rendering/vulkan/vulkan_buffer.hpp`](src/engine/rendering/vulkan/vulkan_buffer.hpp#L19) (line 19)

============================================================================
There are two broad GPU memory categories:

  HOST_VISIBLE — CPU can read/write it directly (via vkMapMemory).
                 The GPU can still read it, but bandwidth is lower.

  DEVICE_LOCAL  — lives entirely on the GPU's fast VRAM.
                 The CPU cannot access it directly.  Data must be
                 transferred through a "staging buffer" (a host-visible
                 temporary buffer) using a GPU copy command.

For M1 (a static triangle with only 60 bytes), HOST_VISIBLE is fine.
M2+ will introduce staging buffers for meshes loaded from disk.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Requires: Vulkan SDK

### No Copying

**Source:** [`src/engine/rendering/vulkan/vulkan_buffer.hpp`](src/engine/rendering/vulkan/vulkan_buffer.hpp#L71) (line 71)

Buffers represent unique GPU resources.  We delete copy semantics so the
compiler catches accidental copies (which would double-free).
Move semantics are not implemented here for teaching simplicity; use
std::unique_ptr<VulkanBuffer> when ownership transfer is needed.
===========================================================================
class VulkanBuffer
{
public:
VulkanBuffer()  = default;
~VulkanBuffer() { Destroy(); }

### Memory Properties

**Source:** [`src/engine/rendering/vulkan/vulkan_buffer.hpp`](src/engine/rendering/vulkan/vulkan_buffer.hpp#L100) (line 100)

The combination HOST_VISIBLE | HOST_COHERENT means:
  HOST_VISIBLE  — the CPU can vkMapMemory and get a writeable pointer.
  HOST_COHERENT — writes are immediately visible to the GPU without an
                  explicit vkFlushMappedMemoryRanges call.

### vkMapMemory

**Source:** [`src/engine/rendering/vulkan/vulkan_buffer.hpp`](src/engine/rendering/vulkan/vulkan_buffer.hpp#L118) (line 118)

vkMapMemory gives us a plain void* to the GPU memory.  We memcpy into
it, then vkUnmapMemory releases the mapping.  On discrete GPUs (VRAM)
this would be too slow for large assets — that is why staging buffers
exist.  For a 60-byte triangle it is perfectly fine.

### Memory Types

**Source:** [`src/engine/rendering/vulkan/vulkan_buffer.hpp`](src/engine/rendering/vulkan/vulkan_buffer.hpp#L157) (line 157)

A physical device exposes several memory "heaps" (e.g. VRAM, system RAM)
and multiple "types" pointing into those heaps.  The requirements
object from vkGetBufferMemoryRequirements gives us a bitmask of which
types are compatible; we also need the type to have our desired
property flags.  This helper finds the first type that satisfies both.

### HOST_VISIBLE Vertex Buffer (M1 simplification)

**Source:** [`src/engine/rendering/vulkan/vulkan_mesh.cpp`](src/engine/rendering/vulkan/vulkan_mesh.cpp#L34) (line 34)

-----------------------------------------------------------------------
We allocate the vertex buffer directly in host-visible memory so the
CPU can write to it without a staging buffer.  This is perfectly fine
for small static meshes.

In M2+ we will introduce:
  1. A staging buffer  — temporary host-visible upload region.
  2. A device-local buffer — fast GPU-only memory.
  3. A vkCmdCopyBuffer  — GPU copy from staging → device-local.
The staging approach is mandatory for large assets (meshes from disk).
-----------------------------------------------------------------------
bool ok = m_vertexBuffer.Create(
device, physDevice, bufferSize,
VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
);

### Binding and Drawing

**Source:** [`src/engine/rendering/vulkan/vulkan_mesh.cpp`](src/engine/rendering/vulkan/vulkan_mesh.cpp#L81) (line 81)

The command buffer must already have a pipeline bound before Draw() is
called (see VulkanPipeline::Bind).  The sequence in a frame is:
  1. vkCmdBeginRenderPass
  2. vkCmdSetViewport / vkCmdSetScissor  (dynamic state)
  3. vkCmdBindPipeline                   (pipeline::Bind)
  4. vkCmdBindVertexBuffers              (here)
  5. vkCmdDraw                           (here)
  6. vkCmdEndRenderPass
===========================================================================
void VulkanMesh::Draw(VkCommandBuffer commandBuffer) const
{
if (!m_initialised) return;

### Binding Slot

**Source:** [`src/engine/rendering/vulkan/vulkan_mesh.cpp`](src/engine/rendering/vulkan/vulkan_mesh.cpp#L98) (line 98)

The "firstBinding" parameter (0) must match the binding in
GetBindingDescription().  The GPU uses this to know which vertex buffer
to read when it fetches attribute data.
-----------------------------------------------------------------------
VkBuffer vertexBuffers[] = { m_vertexBuffer.GetBuffer() };
VkDeviceSize offsets[]   = { 0 };

### Stride

**Source:** [`src/engine/rendering/vulkan/vulkan_mesh.cpp`](src/engine/rendering/vulkan/vulkan_mesh.cpp#L127) (line 127)

sizeof(Vertex) = sizeof(float)*2 + sizeof(float)*3 = 20 bytes.
After reading one vertex, the GPU pointer advances by stride bytes
to find the next vertex in the buffer.
VkVertexInputBindingDescription binding = {};
binding.binding   = 0;                          // slot index
binding.stride    = sizeof(Vertex);             // bytes per vertex
binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // advance per vertex
return binding;
}

### VK_FORMAT_R32G32_SFLOAT

**Source:** [`src/engine/rendering/vulkan/vulkan_mesh.cpp`](src/engine/rendering/vulkan/vulkan_mesh.cpp#L149) (line 149)

Vulkan reuses its texture format enum for vertex attributes.
R32G32_SFLOAT = two 32-bit signed floats = GLSL vec2.
-----------------------------------------------------------------------
attrs[0].binding  = 0;
attrs[0].location = 0;   // matches "layout(location = 0)" in triangle.vert
attrs[0].format   = VK_FORMAT_R32G32_SFLOAT;
attrs[0].offset   = offsetof(Vertex, position);   // 0

### What is a "Mesh" in a GPU Engine?

**Source:** [`src/engine/rendering/vulkan/vulkan_mesh.hpp`](src/engine/rendering/vulkan/vulkan_mesh.hpp#L6) (line 6)

============================================================================
A mesh is the combination of:
  1. Geometry data — vertex positions, normals, UVs, colours, …
  2. GPU buffers   — the VkBuffer objects that hold the data on the GPU.
  3. Draw metadata — vertex count, index count, primitive topology, …

For M1 we keep it minimal: one vertex buffer holding 3 coloured 2D vertices
(no index buffer, no depth, no normals).  This is the smallest mesh that
can demonstrate the full Vulkan graphics pipeline.

============================================================================

### Vertex Layout

**Source:** [`src/engine/rendering/vulkan/vulkan_mesh.hpp`](src/engine/rendering/vulkan/vulkan_mesh.hpp#L18) (line 18)

============================================================================
The C++ Vertex struct must exactly match the GLSL shader inputs:

  Vertex { float position[2]; float color[3]; }  — 5 floats = 20 bytes/vertex

  GLSL:
    layout(location = 0) in vec2 inPosition;   // bytes 0-7
    layout(location = 1) in vec3 inColor;      // bytes 8-19

VkVertexInputBindingDescription describes the stride (20 bytes per vertex).
VkVertexInputAttributeDescription describes each attribute's offset, format,
and binding point.  Mismatching these with the struct layout is a common
Vulkan bug that the validation layers will catch.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Requires: Vulkan SDK

### Plain-Old-Data (POD) Vertex

**Source:** [`src/engine/rendering/vulkan/vulkan_mesh.hpp`](src/engine/rendering/vulkan/vulkan_mesh.hpp#L68) (line 68)

We use a simple C struct (no constructors, no virtual functions) because the
GPU reads raw binary data.  The layout of this struct in memory must match
exactly what the shader expects at each attribute location.

offsetof() gives the byte offset of a field within the struct.  We use it
in GetAttributeDescriptions() to tell Vulkan where each field starts.
===========================================================================
struct Vertex
{
float position[2];  ///< NDC position: x ∈ [-1,+1], y ∈ [-1,+1].
float color[3];     ///< Linear RGB colour: each channel 0.0 – 1.0.
};

### vkCmdBindVertexBuffers

**Source:** [`src/engine/rendering/vulkan/vulkan_mesh.hpp`](src/engine/rendering/vulkan/vulkan_mesh.hpp#L127) (line 127)

This command tells the GPU where to fetch vertex data.  We bind to
"binding 0" (matching the binding in GetBindingDescription).

### vkCmdDraw

**Source:** [`src/engine/rendering/vulkan/vulkan_mesh.hpp`](src/engine/rendering/vulkan/vulkan_mesh.hpp#L131) (line 131)

For a non-indexed draw:
  vertexCount     — total vertices to process.
  instanceCount   — 1 = no instancing.
  firstVertex     — offset into the vertex buffer (0 = start).
  firstInstance   — offset for instance ID (0 = no instancing).

@param commandBuffer  Active recording command buffer.

### Vertex Binding

**Source:** [`src/engine/rendering/vulkan/vulkan_mesh.hpp`](src/engine/rendering/vulkan/vulkan_mesh.hpp#L149) (line 149)

A "binding" is a slot into which a vertex buffer is plugged at
draw time (vkCmdBindVertexBuffers).  We use binding 0.
The stride is sizeof(Vertex) — how many bytes to advance per vertex.
inputRate VERTEX means "advance once per vertex" (as opposed to
INSTANCE which advances once per rendered instance).

### Vertex Attributes

**Source:** [`src/engine/rendering/vulkan/vulkan_mesh.hpp`](src/engine/rendering/vulkan/vulkan_mesh.hpp#L161) (line 161)

Each attribute maps a byte range in the vertex struct to a shader
input location.
  Attribute 0 (location 0): position — VK_FORMAT_R32G32_SFLOAT (vec2).
  Attribute 1 (location 1): color    — VK_FORMAT_R32G32B32_SFLOAT (vec3).

VK_FORMAT_R32G32_SFLOAT means "two 32-bit signed floats" — it
re-uses texture format names because Vulkan has one format enum for
everything (pixels, vertex data, uniform data).

### Reading Binary Files in C++

**Source:** [`src/engine/rendering/vulkan/vulkan_pipeline.cpp`](src/engine/rendering/vulkan/vulkan_pipeline.cpp#L30) (line 30)

std::ios::ate positions the stream at the END so tellg() returns the file
size immediately.  We allocate a vector of that size, seek back to the
beginning, and read the whole file in one call.
===========================================================================
std::vector<char> VulkanPipeline::LoadSpvFile(const std::string& path)
{
std::ifstream file(path, std::ios::ate | std::ios::binary);
if (!file.is_open())
{
std::cerr << "[VulkanPipeline] Cannot open shader: " << path << "\n";
return {};
}

### VkShaderModuleCreateInfo

**Source:** [`src/engine/rendering/vulkan/vulkan_pipeline.cpp`](src/engine/rendering/vulkan/vulkan_pipeline.cpp#L57) (line 57)

pCode is a pointer to uint32_t (4-byte words), but we loaded bytes.
The reinterpret_cast is safe because std::vector guarantees contiguous
storage and SPIR-V modules are always 4-byte aligned.
codeSize is in BYTES (not words) — a common gotcha.
===========================================================================
VkShaderModule VulkanPipeline::CreateShaderModule(VkDevice                 device,
const std::vector<char>& code)
{
if (code.empty()) return VK_NULL_HANDLE;

### Pipeline Creation Order

**Source:** [`src/engine/rendering/vulkan/vulkan_pipeline.cpp`](src/engine/rendering/vulkan/vulkan_pipeline.cpp#L85) (line 85)

Vulkan's graphics pipeline is built from a large struct,
VkGraphicsPipelineCreateInfo, that references many smaller sub-structs
describing every pipeline stage.  We fill them from top to bottom,
following the logical order of the GPU pipeline:

  Vertex input  →  Input assembly  →  Vertex shader
  →  Rasterisation  →  Fragment shader  →  Colour blend  →  Output

The pipeline layout must be created first because the pipeline references it.
The shader modules can be destroyed immediately after the pipeline is created.
===========================================================================
bool VulkanPipeline::Create(VkDevice           device,
VkRenderPass       renderPass,
const std::string& vertSpvPath,
const std::string& fragSpvPath)
{
m_device = device;

### VkPipelineShaderStageCreateInfo

**Source:** [`src/engine/rendering/vulkan/vulkan_pipeline.cpp`](src/engine/rendering/vulkan/vulkan_pipeline.cpp#L127) (line 127)

-----------------------------------------------------------------------
One entry per programmable stage.  Key fields:
  stage  — which stage (VERTEX, FRAGMENT, GEOMETRY, …).
  module — the VkShaderModule containing the SPIR-V.
  pName  — entry-point function name in the GLSL source ("main").
-----------------------------------------------------------------------
VkPipelineShaderStageCreateInfo shaderStages[2] = {};

### Vertex Input State

**Source:** [`src/engine/rendering/vulkan/vulkan_pipeline.cpp`](src/engine/rendering/vulkan/vulkan_pipeline.cpp#L147) (line 147)

-----------------------------------------------------------------------
Describes the memory layout of the vertex buffer.  We get the binding
and attribute descriptions from VulkanMesh which owns the Vertex struct.
This keeps the vertex layout defined in one place (VulkanMesh) and
referenced here without duplication.
-----------------------------------------------------------------------
auto bindingDesc   = VulkanMesh::GetBindingDescription();
auto attributeDesc = VulkanMesh::GetAttributeDescriptions();

### Input Assembly State

**Source:** [`src/engine/rendering/vulkan/vulkan_pipeline.cpp`](src/engine/rendering/vulkan/vulkan_pipeline.cpp#L166) (line 166)

-----------------------------------------------------------------------
Describes how vertex indices form primitives.
TRIANGLE_LIST = every 3 consecutive vertices form one triangle.
primitiveRestartEnable = VK_FALSE (only relevant for indexed strip draws).
-----------------------------------------------------------------------
VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
inputAssembly.primitiveRestartEnable = VK_FALSE;

### Dynamic Viewport and Scissor

**Source:** [`src/engine/rendering/vulkan/vulkan_pipeline.cpp`](src/engine/rendering/vulkan/vulkan_pipeline.cpp#L178) (line 178)

-----------------------------------------------------------------------
By listing VIEWPORT and SCISSOR as dynamic states, we tell Vulkan that
these values will be set by vkCmdSetViewport / vkCmdSetScissor in the
command buffer rather than being baked into the pipeline.

Benefit: when the window is resized, we only need to update the
viewport/scissor commands — we do NOT need to recreate the pipeline.
This is the modern (Vulkan 1.3) recommended approach.
-----------------------------------------------------------------------
VkDynamicState dynamicStates[] = {
VK_DYNAMIC_STATE_VIEWPORT,
VK_DYNAMIC_STATE_SCISSOR
};

### Rasterisation State

**Source:** [`src/engine/rendering/vulkan/vulkan_pipeline.cpp`](src/engine/rendering/vulkan/vulkan_pipeline.cpp#L206) (line 206)

-----------------------------------------------------------------------
polygonMode  FILL  = solid triangles (wireframe = FILL → LINE, requires a GPU feature).
cullMode     NONE  = draw both front and back faces (no backface culling for our demo).
frontFace    CCW   = counter-clockwise winding = front face (standard convention).
depthClamp / depthBias — disabled for this demo.
lineWidth    1.0f  = standard; >1.0 requires the wideLines GPU feature.
-----------------------------------------------------------------------
VkPipelineRasterizationStateCreateInfo rasterizer = {};
rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
rasterizer.depthClampEnable        = VK_FALSE;
rasterizer.rasterizerDiscardEnable = VK_FALSE;
rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
rasterizer.cullMode                = VK_CULL_MODE_NONE;
rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
rasterizer.depthBiasEnable         = VK_FALSE;
rasterizer.lineWidth               = 1.0f;

### Multisampling (MSAA)

**Source:** [`src/engine/rendering/vulkan/vulkan_pipeline.cpp`](src/engine/rendering/vulkan/vulkan_pipeline.cpp#L225) (line 225)

-----------------------------------------------------------------------
rasterizationSamples = 1 bit = no anti-aliasing.  MSAA is introduced
in a later milestone when render quality becomes a concern.
-----------------------------------------------------------------------
VkPipelineMultisampleStateCreateInfo multisampling = {};
multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
multisampling.sampleShadingEnable  = VK_FALSE;

### Colour Blend Attachment

**Source:** [`src/engine/rendering/vulkan/vulkan_pipeline.cpp`](src/engine/rendering/vulkan/vulkan_pipeline.cpp#L236) (line 236)

-----------------------------------------------------------------------
blendEnable = VK_FALSE means no blending: the fragment shader output
completely replaces the existing pixel.  srcColorBlendFactor and
dstColorBlendFactor are ignored when blendEnable is FALSE.

colorWriteMask: which channels to write.  RGBA = write all four.
-----------------------------------------------------------------------
VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
colorBlendAttachment.blendEnable    = VK_FALSE;
colorBlendAttachment.colorWriteMask =
VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

### Pipeline Layout

**Source:** [`src/engine/rendering/vulkan/vulkan_pipeline.cpp`](src/engine/rendering/vulkan/vulkan_pipeline.cpp#L257) (line 257)

-----------------------------------------------------------------------
The layout declares the descriptor sets (textures, UBOs) and push
constants the shaders use.  For a vertex-only-input triangle, no
resources are needed, so we create an empty layout.
-----------------------------------------------------------------------
VkPipelineLayoutCreateInfo layoutInfo = {};
layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
layoutInfo.setLayoutCount         = 0;
layoutInfo.pSetLayouts            = nullptr;
layoutInfo.pushConstantRangeCount = 0;
layoutInfo.pPushConstantRanges    = nullptr;

### basePipelineHandle

**Source:** [`src/engine/rendering/vulkan/vulkan_pipeline.cpp`](src/engine/rendering/vulkan/vulkan_pipeline.cpp#L296) (line 296)

Vulkan allows you to derive a new pipeline from an existing one for
faster creation.  We don't use this feature yet.
pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
pipelineInfo.basePipelineIndex   = -1;

### Pipeline Cache

**Source:** [`src/engine/rendering/vulkan/vulkan_pipeline.cpp`](src/engine/rendering/vulkan/vulkan_pipeline.cpp#L303) (line 303)

-----------------------------------------------------------------------
The second parameter is an optional VkPipelineCache.  A cache stores
the compiled pipeline data so it can be reused across application runs.
Huge win for startup time on real games; we pass VK_NULL_HANDLE for
simplicity (the cache is treated as empty and nothing is saved).
-----------------------------------------------------------------------
VkResult result = vkCreateGraphicsPipelines(
device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline);

### Layout vs Pipeline Destruction Order

**Source:** [`src/engine/rendering/vulkan/vulkan_pipeline.cpp`](src/engine/rendering/vulkan/vulkan_pipeline.cpp#L340) (line 340)

The pipeline MUST be destroyed before the pipeline layout that it was
created with.  The layout itself can be destroyed any time after the
pipeline that references it is destroyed.
===========================================================================
void VulkanPipeline::Destroy()
{
if (!m_initialised) return;
m_initialised = false;

### What is a Graphics Pipeline?

**Source:** [`src/engine/rendering/vulkan/vulkan_pipeline.hpp`](src/engine/rendering/vulkan/vulkan_pipeline.hpp#L6) (line 6)

============================================================================
In Vulkan the "pipeline" is a large baked state object that captures
everything the GPU needs to know to execute a draw call:

  • Shader stages   — which vertex/fragment/… programs to run.
  • Vertex input    — layout of the vertex buffer data.
  • Input assembly  — how vertices form primitives (triangles, lines, …).
  • Viewport/scissor — rendered area (we use dynamic state so it survives
                       window resizes without recreating the pipeline).
  • Rasterisation   — fill vs wireframe, culling, line width, depth bias.
  • Colour blending — how the fragment output is blended with the existing
                      framebuffer pixel.

In OpenGL, state is a global bag of switches changed at any time.  Vulkan's
PSO approach forces you to commit all relevant state upfront, letting the
driver compile one optimal native-ISA shader variant — no runtime surprises.

============================================================================

### Pipeline Layout

**Source:** [`src/engine/rendering/vulkan/vulkan_pipeline.hpp`](src/engine/rendering/vulkan/vulkan_pipeline.hpp#L25) (line 25)

============================================================================
The VkPipelineLayout declares how shader resources (uniform buffers,
textures) are bound — via descriptor set layouts and push constants.

For M1 (a simple coloured triangle) we need NO resources at all: vertex
data comes from the vertex buffer, colour from interpolated attributes.
So our pipeline layout is empty (zero descriptor sets, zero push constants).

============================================================================

### SPIR-V Shader Loading

**Source:** [`src/engine/rendering/vulkan/vulkan_pipeline.hpp`](src/engine/rendering/vulkan/vulkan_pipeline.hpp#L35) (line 35)

============================================================================
Vulkan does not accept GLSL directly.  Shaders must be compiled to SPIR-V:

  glslc triangle.vert -o triangle.vert.spv   (Vulkan SDK tool)

At runtime we load the .spv file as a raw byte array and pass it to
vkCreateShaderModule.  The driver compiles SPIR-V to GPU machine code at
that point (or lazily during pipeline creation on some drivers).

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Requires: Vulkan SDK

### Single Responsibility

**Source:** [`src/engine/rendering/vulkan/vulkan_pipeline.hpp`](src/engine/rendering/vulkan/vulkan_pipeline.hpp#L80) (line 80)

VulkanPipeline only creates and destroys the PSO.  It does NOT own the
render pass (that is VulkanRenderer's responsibility).  This separation
lets us swap pipelines without touching the render pass.
===========================================================================
class VulkanPipeline
{
public:
VulkanPipeline()  = default;
~VulkanPipeline() { Destroy(); }

### Why Pass the Render Pass?

**Source:** [`src/engine/rendering/vulkan/vulkan_pipeline.hpp`](src/engine/rendering/vulkan/vulkan_pipeline.hpp#L107) (line 107)

The pipeline must be compatible with a specific render pass subpass —
the attachment formats and sample counts must match.  This is Vulkan's
way of ensuring the pipeline was compiled for the right framebuffer format.

### VK_PIPELINE_BIND_POINT_GRAPHICS

**Source:** [`src/engine/rendering/vulkan/vulkan_pipeline.hpp`](src/engine/rendering/vulkan/vulkan_pipeline.hpp#L133) (line 133)

Vulkan has separate pipeline bind points for graphics, compute, and
ray-tracing pipelines.  Graphics pipelines are bound to the GRAPHICS
bind point.

@param commandBuffer  Recording command buffer.

### Binary File Read

**Source:** [`src/engine/rendering/vulkan/vulkan_pipeline.hpp`](src/engine/rendering/vulkan/vulkan_pipeline.hpp#L156) (line 156)

SPIR-V is a binary format (not text).  We must open with std::ios::binary
to avoid the C standard library's newline translation on Windows, which
would corrupt the binary data (0x0D 0x0A → 0x0A would mis-align words).

### VkShaderModule Lifetime

**Source:** [`src/engine/rendering/vulkan/vulkan_pipeline.hpp`](src/engine/rendering/vulkan/vulkan_pipeline.hpp#L170) (line 170)

A shader module can be destroyed immediately after pipeline creation —
the driver has copied what it needs.  We destroy both modules at the
end of Create() to avoid holding onto them unnecessarily.

---

## engine/scripting

### Anatomy of a Lua Binding

**Source:** [`src/engine/scripting/LuaEngine.cpp`](src/engine/scripting/LuaEngine.cpp#L6) (line 6)

============================================================================

Each C++ function exposed to Lua follows the same pattern:

  static int binding_function_name(lua_State* L)
  {
      // 1. Validate argument count
      int nargs = lua_gettop(L);  // number of args on the stack

      // 2. Read arguments (stack index 1 = first arg, 2 = second, etc.)
      const char* msg = luaL_checkstring(L, 1);   // error if not string
      int amount      = luaL_checkinteger(L, 2);  // error if not int

      // 3. Do the work
      LOG_INFO(msg);

      // 4. Push return values
      lua_pushinteger(L, 42);

      // 5. Return the number of values pushed
      return 1;   // 1 return value
  }

luaL_check* functions:
  • luaL_checkstring(L, n)  — requires arg n to be a string; errors otherwise
  • luaL_checkinteger(L, n) — requires arg n to be an integer
  • luaL_optnumber(L, n, d) — optional number, default d if missing

============================================================================

### The Lua Stack and Error Handling

**Source:** [`src/engine/scripting/LuaEngine.cpp`](src/engine/scripting/LuaEngine.cpp#L36) (line 36)

============================================================================

The Lua stack state must be "balanced" across function calls.
Rule of thumb:
  • Every lua_push*() increments the stack top.
  • lua_pop(L, n) removes n values.
  • lua_pcall(L, nargs, nret, 0) removes nargs and pushes nret (or 1 error).

After a FAILED lua_pcall:
  Stack = [ error_message_string ]
  You must pop this string after reading it, otherwise the stack grows
  without bound ("stack overflow" crash after many errors).

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17

### Why SetWorld is not a trivial one-liner

**Source:** [`src/engine/scripting/LuaEngine.cpp`](src/engine/scripting/LuaEngine.cpp#L155) (line 155)

─────────────────────────────────────────────────────────
RegisterEngineBindings() stores the World* in the Lua registry under the
key "engine_world".  Every C binding function retrieves it at call time
via GetWorldFromRegistry(), so if the C++ member m_world were updated
without updating the registry the binding would operate on stale memory.

By keeping both in sync here we guarantee that:
  1. C++ code that calls lua.m_world directly sees the new world.
  2. Lua binding functions that call GetWorldFromRegistry() also see it.
  3. Passing nullptr safely clears the binding before a scene is unloaded,
     preventing any script that fires during teardown from touching dead
     memory (binding functions guard: "if (!world) { return 0; }").
void LuaEngine::SetWorld(World* world)
{
m_world = world;

### luaL_loadfile internals

**Source:** [`src/engine/scripting/LuaEngine.cpp`](src/engine/scripting/LuaEngine.cpp#L198) (line 198)

─────────────────────────────────────────
luaL_loadfile(L, filename):
  1. Opens the file.
  2. Lexes and parses the Lua source into bytecode.
  3. Wraps the bytecode in a Lua closure.
  4. Pushes the closure onto the stack.
  5. Returns LUA_OK (0) on success, or an error code.

The file is NOT executed yet — just compiled.  This separation lets
us check for syntax errors before running anything.
-----------------------------------------------------------------------
const int loadResult = luaL_loadfile(m_L, filename.c_str());
if (loadResult != LUA_OK)
{
HandleLuaError("LoadScript('" + filename + "') — compile error");
return false;
}

### Anonymous Namespace for Binding Functions

**Source:** [`src/engine/scripting/LuaEngine.cpp`](src/engine/scripting/LuaEngine.cpp#L528) (line 528)

──────────────────────────────────────────────────────────
We put the binding functions in an anonymous namespace so they have
INTERNAL LINKAGE — they are not visible outside this translation unit.
This prevents name collisions with other modules and is good hygiene.

Alternative: declare them as static functions (also gives internal linkage).
The anonymous namespace is preferred in modern C++ as static has a
different (and confusing) meaning for class members.
──────────────────────────────────────────────────────────────────────────

### Lua Registry

**Source:** [`src/engine/scripting/LuaEngine.cpp`](src/engine/scripting/LuaEngine.cpp#L544) (line 544)

──────────────────────────────
The Lua registry is a special table accessible only from C (not from Lua
scripts).  It is located at pseudo-index LUA_REGISTRYINDEX.

We store the World pointer as a light userdata in the registry so that all
binding functions can find it without needing a global C++ pointer.

Light userdata: a raw void* stored in Lua's value system without GC tracking.
-----------------------------------------------------------------------
static World* GetWorldFromRegistry(lua_State* L)
{
Retrieve the World pointer stored under the key "engine_world".
lua_getfield(L, LUA_REGISTRYINDEX, "engine_world");
World* w = static_cast<World*>(lua_touserdata(L, -1));
lua_pop(L, 1);
return w;   // may be nullptr if SetWorld() was never called
}

### Single Source of Truth for Gil

**Source:** [`src/engine/scripting/LuaEngine.cpp`](src/engine/scripting/LuaEngine.cpp#L651) (line 651)

Gil is stored exclusively in CurrencyComponent, NOT in InventoryComponent.
Always read CurrencyComponent when you need the player's currency balance.
int gold = 0;
world->View<NameComponent, CurrencyComponent>(
[&](EntityID /*eid*/, NameComponent& name, CurrencyComponent& cc)
{
if (name.name == "Noctis" || name.name == "Player")
{
gold = static_cast<int>(cc.gil);
}
});

### Finding the player and modifying their inventory.

**Source:** [`src/engine/scripting/LuaEngine.cpp`](src/engine/scripting/LuaEngine.cpp#L681) (line 681)

GameDatabase::FindItem(name) does a linear search by name.
We look up the ItemData first so we can use its canonical ID.
const ItemData* itemData = nullptr;
for (const auto& it : GameDatabase::GetItems()) {
if (it.name == itemName) { itemData = &it; break; }
}
if (!itemData) {
LOG_WARN("[Lua] game_add_item: unknown item '" + std::string(itemName) + "'");
return 0;
}
const uint32_t itemID = itemData->id;

### Lua Registry Storage

**Source:** [`src/engine/scripting/LuaEngine.cpp`](src/engine/scripting/LuaEngine.cpp#L804) (line 804)

──────────────────────────────────────
lua_pushlightuserdata(L, ptr) pushes a raw pointer as a "light userdata"
value.  It is NOT garbage-collected (the GC doesn't know about it).
It is solely the programmer's responsibility to ensure the pointer
remains valid for as long as the Lua state exists.

Here m_world is owned by the Application which outlives the LuaEngine,
so the pointer remains valid.
-----------------------------------------------------------------------
lua_pushlightuserdata(m_L, static_cast<void*>(m_world));
lua_setfield(m_L, LUA_REGISTRYINDEX, "engine_world");

### Two Names, One Function

**Source:** [`src/engine/scripting/LuaEngine.cpp`](src/engine/scripting/LuaEngine.cpp#L819) (line 819)

─────────────────────────────────────────
Lua scripts in this project consistently use the name "engine_log" for
the logging function.  The C++ binding is registered under BOTH names:
  "engine_log"  — used throughout all .lua scripts (canonical name).
  "game_log"    — kept for backward compatibility and as an alias.
lua_register is a single-instruction macro (pushcfunction + setglobal),
so registering the same C function twice is cheap and harmless.
lua_register(m_L, "engine_log",           binding_game_log);  // canonical
lua_register(m_L, "game_log",             binding_game_log);  // alias
lua_register(m_L, "game_get_player_hp",   binding_game_get_player_hp);
lua_register(m_L, "game_heal_player",     binding_game_heal_player);
lua_register(m_L, "game_get_gold",        binding_game_get_gold);
lua_register(m_L, "game_add_item",        binding_game_add_item);
lua_register(m_L, "game_start_combat",    binding_game_start_combat);
lua_register(m_L, "game_complete_quest",  binding_game_complete_quest);
lua_register(m_L, "game_show_message",    binding_game_show_message);

### Exposing C++ Constants to Lua

**Source:** [`src/engine/scripting/LuaEngine.cpp`](src/engine/scripting/LuaEngine.cpp#L840) (line 840)

──────────────────────────────────────────────
We push each constant as a Lua global.  In Lua, uppercase globals by
convention (matching C++ ALL_CAPS) signal "do not modify these."
-----------------------------------------------------------------------
lua_pushinteger(m_L, MAX_INV_SLOTS);
lua_setglobal(m_L, "MAX_INVENTORY_SLOTS");

### Why Scripting in a Game Engine?

**Source:** [`src/engine/scripting/LuaEngine.hpp`](src/engine/scripting/LuaEngine.hpp#L6) (line 6)

============================================================================

Game logic written directly in C++ has several drawbacks for designers and
educators:
 • Long compile-edit-test cycles (minutes per change).
 • C++ requires knowing memory management, the type system, etc.
 • Designers / writers who are not C++ programmers cannot contribute.

An embedded scripting language solves all three problems:
 • Scripts are loaded and executed at RUNTIME — no recompilation needed.
 • Lua's syntax is simpler than C++ (closer to Python or pseudocode).
 • Non-programmers can write quest scripts, dialogue trees, or balance
   values using Lua without touching the engine code.

The pattern is: C++ provides the "engine" (fast, safe, compiled)
                Lua  provides the "game logic" (flexible, hot-reloadable)

This architecture is used by:
  • World of Warcraft (add-on system)
  • Roblox (entire gameplay)
  • CryEngine, Unreal (scripting subsystems)
  • Factorio, Civilization V/VI, Garry's Mod

============================================================================

### How Lua Embeds into C++

**Source:** [`src/engine/scripting/LuaEngine.hpp`](src/engine/scripting/LuaEngine.hpp#L31) (line 31)

============================================================================

Lua is designed to be embedded.  The C API is based on a STACK:

  C calls   → lua_push*(L, value)   to send data INTO Lua
  C reads   → lua_to*(L, index)     to read  data FROM Lua's stack
  Lua calls → C functions registered with lua_register()

Every Lua-to-C function (lua_CFunction) has this signature:
  static int myFunc(lua_State* L) { ... return nResults; }

The stack index convention:
  Positive indices: 1 = bottom of stack (first pushed)
  Negative indices: -1 = top of stack   (last pushed / most recent)

Example — calling Lua from C++:
@code
  lua_getglobal(L, "on_combat_start");   // push Lua function
  lua_pushinteger(L, enemyID);           // push argument
  lua_pcall(L, 1, 0, 0);                // call with 1 arg, 0 returns
@endcode

Example — C function callable from Lua:
@code
  static int game_log(lua_State* L) {
    const char* msg = lua_tostring(L, 1);  // get first arg
    LOG_INFO(msg);
    return 0;  // no return values pushed
  }
  lua_register(L, "game_log", game_log);
@endcode

============================================================================

### lua_pcall vs lua_call

**Source:** [`src/engine/scripting/LuaEngine.hpp`](src/engine/scripting/LuaEngine.hpp#L65) (line 65)

============================================================================

lua_call(L, nargs, nret)   — calls a Lua function.  If an error occurs,
                             it PROPAGATES the error (like an uncaught throw).
                             This usually terminates the program.

lua_pcall(L, nargs, nret, msgh) — "protected call" analogous to try/catch.
                                   On error it returns a non-zero error code
                                   and pushes the error message onto the stack.
                                   The program continues.

ALWAYS use lua_pcall in game code.  A buggy quest script should never
crash the entire game.  The LuaEngine wraps all calls in lua_pcall and
logs errors via the Logger system.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17

### extern "C" for C Headers

**Source:** [`src/engine/scripting/LuaEngine.hpp`](src/engine/scripting/LuaEngine.hpp#L104) (line 104)

──────────────────────────────────────────
Lua is written in C, not C++.  C and C++ use different "name mangling" for
symbols in object files.  C++ encodes overload information into the symbol
name (e.g., `_ZN3foo3barEi`); C does not (just `bar`).

Without extern "C", the C++ linker would look for C++-mangled symbols that
don't exist in the Lua library, causing linker errors.

`extern "C" { ... }` tells the C++ compiler to use C-style (unmangled)
linkage for everything inside the block.

### Lua include-path cascade

**Source:** [`src/engine/scripting/LuaEngine.hpp`](src/engine/scripting/LuaEngine.hpp#L116) (line 116)

─────────────────────────────────────────
Lua headers reach the compiler via different paths depending on how Lua was
set up.  We check in order of preference:

  1. <lua.h> directly on the include path — used when CMake built Lua from
     the bundled source (Lua/lua-5.5.0/src/) and exposed its directory as
     a PUBLIC include on the lua55_static target.  This works on all
     platforms (Windows MSVC, Linux GCC/Clang, macOS).

  2. <lua5.5/lua.h> — system Lua 5.5 installed in a versioned sub-dir
     (e.g. /usr/include/lua5.5/).

  3. <lua5.4/lua.h> — system Lua 5.4 installed in a versioned sub-dir
     (e.g. /usr/include/lua5.4/ on Debian/Ubuntu via liblua5.4-dev).

The cascade ensures the file compiles on every supported platform without
manual per-developer configuration.
extern "C" {
if __has_include(<lua.h>)
Bundled Lua (any platform) — Lua/lua-5.5.0/src/ on the include path via
the lua55_static CMake target PUBLIC include dirs.
 include <lua.h>
 include <lualib.h>
 include <lauxlib.h>
elif __has_include(<lua5.5/lua.h>)
System Lua 5.5 installed in a versioned subdirectory.
 include <lua5.5/lua.h>
 include <lua5.5/lualib.h>
 include <lua5.5/lauxlib.h>
elif __has_include(<lua5.4/lua.h>)
System Lua 5.4 installed in a versioned subdirectory (Debian/Ubuntu).
 include <lua5.4/lua.h>
 include <lua5.4/lualib.h>
 include <lua5.4/lauxlib.h>
else
 error "Lua headers not found.  Ensure Lua/lua-5.5.0/ is present in the repo."
endif
}

### Lua Tables

**Source:** [`src/engine/scripting/LuaEngine.hpp`](src/engine/scripting/LuaEngine.hpp#L171) (line 171)

────────────────────────────
In Lua, tables are the ONLY data structure.  They function as:
  • Arrays:      t = {10, 20, 30}      t[1] == 10
  • Dictionaries: t = {name="Noctis"}  t["name"] == "Noctis"
  • Objects:      t.attack = 50        (syntactic sugar for t["attack"])
  • Modules:      return { foo=bar }

The LuaTable struct helps C++ code read Lua tables returned from functions
or stored as globals.  It holds the stack index of the table and provides
typed getters.

Usage:
@code
  LuaTable tbl = engine.GetTableGlobal("player_stats");
  int hp    = tbl.GetInt("hp", 100);    // default 100 if key missing
  float spd = tbl.GetFloat("speed", 1.0f);
  std::string name = tbl.GetString("name", "Unknown");
@endcode

### Engine ↔ Script Communication

**Source:** [`src/engine/scripting/LuaEngine.hpp`](src/engine/scripting/LuaEngine.hpp#L242) (line 242)

───────────────────────────────────────────────
There are two directions of communication:

  C++ → Lua: Call a Lua function from C++.
    Use case: trigger an NPC's dialogue callback, fire a quest event.
    API: LuaEngine::CallFunction("on_quest_complete", questId)

  Lua → C++: Call a C++ function from Lua.
    Use case: a Lua script wants to heal the player.
    API: In Lua:   game_heal_player(50)
         In C++:   LuaEngine::RegisterFunction("game_heal_player", &fn)

The LuaEngine::RegisterEngineBindings() method registers all game-relevant
C++ functions so Lua scripts can call them freely.

### lua_State Lifecycle

**Source:** [`src/engine/scripting/LuaEngine.hpp`](src/engine/scripting/LuaEngine.hpp#L258) (line 258)

─────────────────────────────────────
A lua_State (often called "L") represents one Lua interpreter instance.
It owns:
  • The global variable table
  • The call stack
  • The garbage collector
  • Loaded modules

luaL_newstate()  → create a new state (allocates memory)
lua_close(L)     → destroy the state and free all memory

We create ONE state on Init() and share it for all scripts.
Multiple states are possible (e.g., sandboxed scripts), but one is simpler.

### Non-Owning Pointers

**Source:** [`src/engine/scripting/LuaEngine.hpp`](src/engine/scripting/LuaEngine.hpp#L321) (line 321)

─────────────────────────────────────
A raw pointer T* in C++ is non-owning by convention (the pointee is
owned elsewhere and outlives this use).  For ownership semantics use
std::unique_ptr (sole owner) or std::shared_ptr (shared ownership).
Here the World is owned by the Application, which outlives LuaEngine.

### Why we ALSO update the Lua registry here

**Source:** [`src/engine/scripting/LuaEngine.hpp`](src/engine/scripting/LuaEngine.hpp#L328) (line 328)

──────────────────────────────────────────────────────────
RegisterEngineBindings() stores the initial World pointer in the Lua
registry under the key "engine_world" so that every C binding can
retrieve it without a global C++ variable.  If SetWorld() only updated
m_world but NOT the registry, binding functions called after a scene
change would silently operate on the old (possibly destroyed) World —
a classic dangling-pointer bug that is almost impossible to diagnose
at runtime.

The fix: mirror every assignment to m_world with a matching registry
update.  lua_pushlightuserdata / lua_setfield atomically replaces the
stored pointer while the Lua state is still valid.

@param world  Pointer to the new active ECS World (may be nullptr to
              clear the binding before a scene unload).

### luaL_loadfile vs luaL_dofile

**Source:** [`src/engine/scripting/LuaEngine.hpp`](src/engine/scripting/LuaEngine.hpp#L354) (line 354)

──────────────────────────────────────────────
luaL_dofile(L, path)  — loads AND runs the file.  Simple but provides
                         less control over error handling.

luaL_loadfile(L, path) — compiles the file into a Lua "chunk" (a closure)
                          and pushes it onto the stack.  Does NOT run it.

lua_pcall(L, 0, 0, 0) — pops and executes the chunk with error protection.

We use loadfile + pcall so we can:
  1. Get a detailed error message on failure.
  2. Have one centralised error-handling path.

@param filename  Path to the .lua file (relative to working directory).
@return true if the script loaded and ran without errors.

### Overloads vs Templates for Lua calls

**Source:** [`src/engine/scripting/LuaEngine.hpp`](src/engine/scripting/LuaEngine.hpp#L396) (line 396)

──────────────────────────────────────────────────────
We provide typed overloads rather than a single variadic template
because the Lua push API is typed:
  lua_pushinteger(L, val)   for integers
  lua_pushnumber(L, val)    for floats/doubles
  lua_pushstring(L, str)    for strings

A variadic template (C++17 fold expressions) would look like:
  template<typename... Args>
  bool CallFunction(const std::string& name, Args&&... args);
This is more elegant but harder to read for students learning templates.
We provide both (the variadic version is the primary, overloads exist
as convenient wrappers for the common cases).

@param funcName  Lua global function name.
@param arg       The integer argument to pass.
@return true on success.

### lua_CFunction Type

**Source:** [`src/engine/scripting/LuaEngine.hpp`](src/engine/scripting/LuaEngine.hpp#L455) (line 455)

────────────────────────────────────
Every function exposed to Lua must have this exact signature:

  int myFunction(lua_State* L);

It receives arguments from the Lua call via the stack (lua_tostring,
lua_tointeger, etc.) and pushes return values onto the stack.
The return value of the C function is the COUNT of values pushed.

lua_register(L, "name", fn) is a macro for:
  lua_pushcfunction(L, fn);
  lua_setglobal(L, "name");

@param name  Name the function will have in Lua global scope.
@param fn    A C function pointer matching int(lua_State*).

### Template Specialisation

**Source:** [`src/engine/scripting/LuaEngine.hpp`](src/engine/scripting/LuaEngine.hpp#L481) (line 481)

─────────────────────────────────────────
GetGlobal<T> is a function template.  The implementation uses
`if constexpr` (C++17) to select the right lua_to* call at compile time
based on T.  This is a form of COMPILE-TIME DISPATCH.

For example:
  GetGlobal<int>("level")       → calls lua_tointeger()
  GetGlobal<float>("speed")     → calls lua_tonumber()
  GetGlobal<std::string>("name")→ calls lua_tostring()

`if constexpr` is evaluated at compile time; the unused branches are
completely removed from the binary (unlike a runtime `if`).

@tparam T  The C++ type to read.  Supported: int, float, double,
           std::string, bool.
@param name          Name of the Lua global variable.
@param defaultValue  Returned if the variable doesn't exist or is nil.
@return The value as type T, or defaultValue on failure.

### SetGlobal and Type Dispatch

**Source:** [`src/engine/scripting/LuaEngine.hpp`](src/engine/scripting/LuaEngine.hpp#L507) (line 507)

─────────────────────────────────────────────
Like GetGlobal, this template uses if constexpr to select the right
lua_push* variant.  After pushing the value, lua_setglobal(L, name)
pops it and stores it in Lua's global table.

This is useful for:
  • Passing game state to scripts ("level", "difficulty", "player_name")
  • Hot-patching balance values without recompiling
  • Configuring scripts from C++ before running them

@tparam T    The C++ type.  Supported: int, float, double, std::string, bool.
@param name  The Lua global variable name to set.
@param val   The value to assign.

### When to Use the Raw Pointer

**Source:** [`src/engine/scripting/LuaEngine.hpp`](src/engine/scripting/LuaEngine.hpp#L547) (line 547)

─────────────────────────────────────────────
Most code should use the LuaEngine API.  Use the raw state only when:
  • Integrating a third-party Lua binding library (sol2, LuaBridge).
  • Writing complex binding code that goes beyond the LuaEngine API.
  • Debugging stack contents with lua_gettop / lua_typename.

### Templates in Headers

**Source:** [`src/engine/scripting/LuaEngine.hpp`](src/engine/scripting/LuaEngine.hpp#L607) (line 607)

──────────────────────────────────────
Function templates must have their FULL implementation visible at the point
of instantiation (where GetGlobal<int>(...) is written in user code).
This means template implementations typically live in the .hpp file, not
the .cpp file.

Alternatives:
  1. Put the implementation in the .hpp (done here — simplest for learners).
  2. Use explicit instantiations: declare the template in .cpp and add
     `template int GetGlobal<int>(...);` etc.  Faster compile, less flexible.
  3. Use a .inl file included at the bottom of the .hpp for organisation.

### if constexpr (C++17)

**Source:** [`src/engine/scripting/LuaEngine.hpp`](src/engine/scripting/LuaEngine.hpp#L640) (line 640)

──────────────────────────────────────
`if constexpr` evaluates its condition at COMPILE TIME.
Only the branch matching T is compiled; the others are discarded.
This is zero-overhead compared to a runtime `if`.

std::is_same_v<T, int> is true only when T == int exactly.
std::is_integral_v<T> is true for int, long, short, char, bool, etc.

---

## game/Game.cpp

### Dependency Injection via Constructor

**Source:** [`src/game/Game.cpp`](src/game/Game.cpp#L47) (line 47)

Each system receives only the objects it needs, via constructor.
CombatSystem uses EventBus internally via its singleton, so it only
needs the World pointer.  Other systems need a UIEvent bus to show
notifications to the player.
m_combat    = std::make_unique<CombatSystem>(&m_world);
m_inventory = std::make_unique<InventorySystem>(&m_world,
&EventBus<UIEvent>::Instance());
m_quests    = std::make_unique<QuestSystem>(&m_world,
&EventBus<QuestEvent>::Instance(),
&EventBus<UIEvent>::Instance());
m_magic     = std::make_unique<MagicSystem>(&m_world, &LuaEngine::Get());
m_shop      = std::make_unique<ShopSystem>(&m_world,
&EventBus<UIEvent>::Instance());
m_camp      = std::make_unique<CampSystem>(&m_world,
&EventBus<UIEvent>::Instance());
m_weather   = std::make_unique<WeatherSystem>(
&EventBus<WorldEvent>::Instance());

### Component Registration

**Source:** [`src/game/Game.cpp`](src/game/Game.cpp#L91) (line 91)

Before any entity can use a component type, the ECS World must know about
it.  RegisterComponent<T>() allocates the component pool (a packed array)
and assigns T a unique component-type ID.  Calling View<A, B>() later
queries only entities that have BOTH A and B — no overhead for entities
that have neither.  This is the core benefit of an ECS architecture.
m_world.RegisterComponent<TransformComponent>();
m_world.RegisterComponent<HealthComponent>();
m_world.RegisterComponent<StatsComponent>();
m_world.RegisterComponent<NameComponent>();
m_world.RegisterComponent<RenderComponent>();
m_world.RegisterComponent<MovementComponent>();
m_world.RegisterComponent<CombatComponent>();
m_world.RegisterComponent<InventoryComponent>();
m_world.RegisterComponent<QuestComponent>();
m_world.RegisterComponent<DialogueComponent>();
m_world.RegisterComponent<AIComponent>();
m_world.RegisterComponent<PartyComponent>();
m_world.RegisterComponent<MagicComponent>();
m_world.RegisterComponent<EquipmentComponent>();
m_world.RegisterComponent<StatusEffectsComponent>();
m_world.RegisterComponent<LevelComponent>();
m_world.RegisterComponent<CurrencyComponent>();
m_world.RegisterComponent<SkillsComponent>();
m_world.RegisterComponent<CampComponent>();
m_world.RegisterComponent<VehicleComponent>();
LOG_INFO("All 20 component types registered.");
}

### Single Source of Truth

**Source:** [`src/game/Game.cpp`](src/game/Game.cpp#L163) (line 163)

Gil is stored ONLY in CurrencyComponent.  InventoryComponent no longer
carries a gil field (that was removed to eliminate a desync bug where
buying items could deduct from one field but leave the other unchanged).
auto& cc  = m_world.AddComponent<CurrencyComponent>(m_playerID);
cc.gil    = 500;

### Party members are regular entities with all the same

**Source:** [`src/game/Game.cpp`](src/game/Game.cpp#L221) (line 221)

components as the player.  The PartyComponent on the "party entity"
just holds their IDs.

### Fixed-Timestep Game Loop

**Source:** [`src/game/Game.cpp`](src/game/Game.cpp#L320) (line 320)

──────────────────────────────────────────
The game loop runs as fast as possible but caps to 60 FPS by sleeping
any remaining frame time.  `dt` (delta-time) is the real elapsed time
in seconds since the last frame; it's passed to systems like CombatSystem
and AISystem so they tick at the right real-world rate regardless of
frame rate.  The 0.1 s clamp prevents "spiral-of-death" — if the game
hitches for a second (e.g. the debugger pauses), dt would normally be
1.0 s and physics/AI would explode; clamping limits the damage.
using Clock    = std::chrono::steady_clock;
using Duration = std::chrono::duration<float>;

### Simple Text-Based Save System

**Source:** [`src/game/Game.cpp`](src/game/Game.cpp#L908) (line 908)

We write key=value pairs to a plain text file.  This is easy to debug
(you can open the save file in a text editor) and human-readable, but
slow and fragile for large games.  A production engine would use a
binary format or a database (e.g. SQLite) for speed and reliability.
std::ofstream out(filename);
if (!out) { LOG_ERROR("SaveGame: cannot open " + filename); return; }

---

## game/Game.hpp

### The Application Class Pattern

**Source:** [`src/game/Game.hpp`](src/game/Game.hpp#L6) (line 6)

============================================================================

A Game class (sometimes called Application, Engine, or GameManager) is the
top-level object that:

 1. OWNS all subsystem objects (renderer, input, ECS world, systems).
 2. INITIALISES them in the correct dependency order.
 3. RUNS the main loop (a fixed-timestep update + render loop).
 4. SHUTS THEM DOWN in reverse order (RAII / destructor chain).

It is NOT responsible for game-specific logic — that lives in Systems.
The Game class is the "glue" between the platform (OS, hardware) and the
game logic.

─── Singleton vs. Dependency Injection ─────────────────────────────────────

Using a Singleton (Game::Instance()) for the top-level application class is
widely accepted: there is genuinely exactly one game running at a time.
However, all SUBSYSTEMS should be accessed through the Game object (not as
their own singletons when possible) so that unit tests can construct a Game
with mock systems.

─── Game States ─────────────────────────────────────────────────────────────

The Game class contains a STATE MACHINE over GameState enum values.
Each Update() and Render() call dispatches to a state-specific function.
This keeps state transitions explicit and prevents "spaghetti" logic where
Update() checks dozens of boolean flags.

@author  Educational Game Engine Project
@version 1.0
@date    2024

---

## game/GameData.hpp

### Header-Only Libraries & The "Data Layer"

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L6) (line 6)

============================================================================

This file is a **header-only** data definition layer.  Every struct,
enum, and database lookup function lives here with no corresponding .cpp.

WHY HEADER-ONLY?
────────────────
In small-to-medium projects, putting all read-only game content in a single
header is practical:
  • A single `#include "GameData.hpp"` gives any system access to every
    item, spell, enemy, and quest without linking against a separate library.
  • The C++17 `inline` keyword on static member functions ensures that the
    One-Definition Rule (ODR) is satisfied — every translation unit that
    includes this header shares the same function body without duplicate-
    symbol linker errors.
  • Static local variables inside `inline` functions are thread-safe in
    C++11+ (guaranteed by the standard), making our lazy-init database
    safe even in multi-threaded scenarios.

DESIGN PATTERN — Flyweight
──────────────────────────
Rather than embedding full item/enemy structs in every entity, components
store only a lightweight uint32_t ID.  The ID is used as a key into
GameDatabase to retrieve the full data object *on demand*.  This is the
classic "Flyweight" pattern:
  Shared intrinsic state  → GameDatabase (read-only, common to all copies)
  Per-instance state      → Components on each ECS entity (health, position)

DESIGN PATTERN — Repository / Service Locator
──────────────────────────────────────────────
GameDatabase acts as a global read-only repository.  Callers never need to
instantiate it — they call static methods:
  const ItemData* item = GameDatabase::FindItem(23);
This mirrors the "Service Locator" pattern common in game engines.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024

C++ Standard: C++17

### Enumeration Design

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L71) (line 71)

─────────────────────────────────────
Using an enum class (scoped enum) for categories prevents accidental
integer arithmetic on them:
  WEAPON + 1  →  compile error with enum class  (good!)
  WEAPON + 1  →  compiles silently with plain enum (dangerous)

Each ItemType maps to different UI displays, inventory filters,
equip slots, and scripting behaviour.

### Nested Categorisation

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L94) (line 94)

──────────────────────────────────────
A weapon's ItemType is always WEAPON, but the WeaponType refines what
*kind* of weapon it is.  This two-level categorisation is common in RPGs:
  Level 1: ItemType::WEAPON  (is it a weapon at all?)
  Level 2: WeaponType::SWORD (what *kind* of weapon?)

The combat system reads WeaponType to choose attack combos and hit boxes.

### Data-Driven Design

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L122) (line 122)

────────────────────────────────────
Rather than hard-coding item behaviour in C++ classes (e.g.
`class FlametongueItem : public SwordItem`), we describe items as
*data records*.  The game engine reads fields like `element` and `attack`
and applies generic logic.  Adding a new item only requires a new row in
the database — no new C++ code.

This "data-driven" approach is industry standard.  Major studios store item
data in spreadsheets (Excel/Google Sheets) that are exported to JSON or
binary and loaded at runtime.  For this educational engine we inline the
data directly in C++ initialiser lists, which is equivalent and keeps
everything in one place for students.

Lua Callbacks
─────────────
`luaEquipCallback` stores the name of a Lua function that is called when
this item is equipped.  Example:
  "onEquipFlametongue"  →  Lua function that adds fire damage to attacks.
This lets designers write item effects in Lua without recompiling C++.
See the Scripting section of the engine documentation for details.

### Spell as Data vs Spell as Code

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L181) (line 181)

───────────────────────────────────────────────
It is tempting to model each spell as a C++ class:
  class FireSpell : public Spell { void Cast() override { … } }
But this leads to hundreds of small classes and a tangled hierarchy.

A better approach: spells are DATA, and a single `SpellSystem` reads the
data and executes generic behaviour:
  - Deal `baseDamage` multiplied by the caster's magic stat.
  - Apply `element` for weakness calculations.
  - Deduct `mpCost` from the caster's MP.
  - Call `luaCastCallback` for any special behaviour.

This makes spell balancing trivial: designers change numbers in the database
without touching C++.

### Species Template vs Instance

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L225) (line 225)

─────────────────────────────────────────────
EnemyData is a *template* (species definition), not a live entity.
When the Zone spawns a Goblin, it reads EnemyData{id=1} to know the
goblin's stats, then creates an ECS entity and copies those values into
HealthComponent, StatsComponent, etc.

Multiple live Goblins share one EnemyData record — this is the Flyweight
pattern in action.  If a designer buffs the Goblin's HP from 50 to 60,
every future Goblin spawn benefits.

Drop Tables
───────────
`dropItemIDs` is a simple list of item IDs this enemy *can* drop.
The actual drop resolution (random selection, drop rates) is handled by
the CombatSystem using the enemy's `luck` stat as a multiplier.

AI Callback
───────────
`luaAICallback` names a Lua function invoked each AI tick.  This allows
complex bosses (Iron Giant, Adamantoise) to have hand-crafted behaviours
without hard-coding them in C++.

### Quest Systems

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L286) (line 286)

──────────────────────────────
Most RPG quest systems reduce to a small set of objective patterns.
Rather than hard-coding each quest in C++, we define objective *types*
and attach numeric parameters (targetID, requiredCount).  A generic
QuestSystem then updates `currentCount` when relevant events occur:

  KILL_ENEMY:       CombatSystem fires CombatEvent; QuestSystem checks if
                    the dead entity's EnemyData.id matches targetID.
  COLLECT_ITEM:     InventorySystem fires ItemEvent; QuestSystem checks
                    if the item's id matches targetID.
  REACH_LOCATION:   MovementSystem fires WorldEvent when player enters a
                    zone; QuestSystem checks zone.id vs targetID.
  TALK_TO_NPC:      DialogueSystem fires DialogueEvent on conversation end.
  HUNT_TARGET:      Like KILL_ENEMY but linked to a HuntData entry.

### Incremental Progress

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L314) (line 314)

──────────────────────────────────────
`currentCount` and `requiredCount` allow objectives to track partial
progress ("Kill 2 / 3 Goblins").  The QuestSystem increments
`currentCount` on matching events, then sets `isComplete = true` when
currentCount >= requiredCount.

This avoids special-casing each quest: the same increment-and-check logic
handles all numeric objectives generically.

### Main vs Side Quests

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L337) (line 337)

─────────────────────────────────────
`isMainQuest` separates the critical story path from optional content.
The quest log UI uses this flag to show "Main Quest" vs "Side Quest"
headers.  Main quests can also trigger cutscenes and world state changes
that side quests cannot.

Prerequisite Quests
───────────────────
`prereqQuestIDs` implements quest gating: "The Final Trial" only becomes
available after completing "The Crystal's Call" AND "Darkness Rising".
The QuestSystem checks all prerequisite quest IDs are completed before
making a new quest available to the player.

### Composition for Recipes

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L379) (line 379)

──────────────────────────────────────────
A recipe is composed of multiple ingredients.  Storing them as a
`std::vector<RecipeIngredient>` allows variable-length ingredient lists
without a fixed array size.  The CampSystem iterates the list and checks
the player's inventory for each ingredient before allowing cooking.

### Temporary Buffs (Duration-Based Effects)

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L395) (line 395)

──────────────────────────────────────────────────────────
Camp meals grant bonuses that last for `durationTurns` combat turns.
The CampComponent on the player entity tracks the active meal and its
remaining duration.  This is a simple timer-based buff system:

  On rest: copy meal bonuses into CampComponent.
  On each combat turn: decrement durationTurns in CampComponent.
  When durationTurns == 0: remove the bonuses.

This avoids complex "aura" systems while still teaching the concept of
temporary effects with explicit durations.

Inspired by the camp cooking system in Final Fantasy XV where Ignis
prepares meals that provide next-day buffs.

### Price Multipliers

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L440) (line 440)

──────────────────────────────────
Many RPGs use multipliers on a base item price rather than separate
buy/sell prices per item.  This means:
  buyPrice  = item.price * buyMultiplier    (usually 1.0 — full price)
  sellPrice = item.price * sellMultiplier   (usually 0.5 — half price)

Benefits:
  • Rebalancing prices only requires changing one base value per item.
  • Special shops (black market) can have buyMultiplier = 1.5 (mark-up).
  • Player skills can modify sellMultiplier as a reward.

The ShopSystem reads these multipliers to compute final prices during
the buying/selling interaction.

### Hunt / Bounty Systems

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L474) (line 474)

──────────────────────────────────────
Many open-world RPGs (FF14, FF15, Monster Hunter) use hunt/bounty
systems as repeatable side content.  A hunt is a special kill quest
with a unique target and high rewards.

Key design decisions here:
  • Hunts reference EnemyData.id via `targetEnemyID` — they use the
    same enemy types as normal encounters, just with special conditions.
  • `prereqQuestID` gates hunts behind story progress (you can't hunt
    Adamantoise before completing the main storyline).
  • `isCompleted` is a runtime flag — the database record stays
    immutable but the player's save data tracks completion per hunt.
    In a real save system this flag would live in a separate SaveData
    struct, not the master database.

### Zone / Level Architecture

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L513) (line 513)

──────────────────────────────────────────
The world is divided into named zones (similar to "areas" in FF15 or
"scenes" in Unity).  ZoneData is the configuration record read when
a zone is loaded.  The Zone class (Zone.hpp) owns the live runtime state
(tilemap, spawned enemies, NPCs).

`recommendedLevel` is shown to the player as a difficulty hint.  If the
player enters a high-level zone early, they get a warning but are not
blocked — this is the "open-world" design philosophy.

`dangerLevel` (1–5) is used by the AI system to scale patrol density and
by the atmosphere system to adjust ambient audio intensity.

### Static Singleton vs Namespace

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L556) (line 556)

============================================================================

We use a class with only static methods rather than a namespace so that:
  1. We can forward-declare GameDatabase in headers without pulling in the
     full definition.
  2. It is visually clear that all members share the same "owner".
  3. Students can see how a static singleton is structured before learning
     about the more complex classic Singleton pattern.

### Lazy Initialisation (Meyers Singleton)

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L566) (line 566)

─────────────────────────────────────────────────────────
Each Get*() method returns a reference to a `static local` vector.
In C++11+, static local variables are initialised the *first* time the
function is called — this is called "lazy initialisation" or the
"Meyers Singleton" pattern (named after Scott Meyers who popularised it).

The key benefits:
  • Thread-safe: the C++11 standard guarantees that static locals are
    initialised exactly once, even under concurrent access.
  • No global constructor ordering issues: the vector is built only when
    first needed, never before main() starts.
  • Simple: no heap allocation, no manual memory management.

### Linear Search vs Hash Map

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L580) (line 580)

──────────────────────────────────────────
FindItem(), FindSpell() etc. perform linear O(n) searches.  For a game
with ~100 items this is perfectly adequate — the dataset fits in L1 cache
and the call frequency is low (only during equip / battle start).

For larger datasets (1000+ entries), replace with:
  std::unordered_map<uint32_t, const ItemData*>
keyed by ID, giving O(1) average-case lookup.

============================================================================

### Returning Pointers vs References

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L617) (line 617)

──────────────────────────────────────────────────
We return a pointer (not a reference) so callers can test for nullptr:
  if (auto* item = GameDatabase::FindItem(99)) { use(*item); }
Returning a reference would force us to throw or assert on failure,
which is less friendly for "optional" lookups.

### Separating Schema from Data

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L741) (line 741)

─────────────────────────────────────────────
The public Get*() methods (above) define the *interface*.
The private Build*() methods (below) define the *data*.
This separation means a student reading the public API never has to
scroll past hundreds of data rows.  Conversely, a designer editing
item stats only touches the Build*() sections.
─────────────────────────────────────────────────────────────────────

### Designated Initialisers (C++20) vs Field Order (C++17)

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L757) (line 757)

──────────────────────────────────────────────────────────────────────────
In C++20 you could write:
  ItemData{ .id=1, .name="Broadsword", .attack=15, … }
In C++17 we rely on the order of fields in the struct definition,
so the initialiser list must match the struct layout exactly.

For readability we construct each entry field-by-field using assignment,
which is unambiguous regardless of field order.

### Spell Tiers

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L1169) (line 1169)

────────────────────────────
Classic FF magic comes in three tiers: base (Fire), -ra (Fira), -ga (Firaga).
Each tier roughly triples the damage and doubles the MP cost.
This progression gives players meaningful upgrade moments and a reason
to conserve MP: "Should I use Firaga now, or save MP for the boss?"

The "castTime" field creates a risk-reward trade-off: stronger spells
take longer to cast, leaving the caster vulnerable.

### Balancing Enemy Stats

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L1246) (line 1246)

───────────────────────────────────────
Enemy stats follow rough guidelines for an action RPG:
  HP      ≈ level * 30 (normal) to level * 250 (boss)
  Attack  ≈ level * 2.5
  Defence ≈ level * 1.5
  XP      ≈ level * 15 (normal) to level * 100 (boss)
  Gil     ≈ level * 8  (normal) to level * 500 (boss)

These are starting points — playtesters adjust them for game feel.
Keeping the formulas visible in comments helps future maintainers
understand *why* Behemoth has 1500 HP and not 500.

### Quest Narrative Structure

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L1415) (line 1415)

───────────────────────────────────────────
The quests follow a classic three-act structure:
  Act 1 (Q1–Q3):  Introduction; player learns basic mechanics.
  Act 2 (Q4–Q8):  Escalation; harder challenges, story reveals.
  Act 3 (Q9–Q10): Climax; final boss, ultimate reward.

Prerequisites (prereqQuestIDs) enforce narrative order without hard-
coding quest logic in C++.  The QuestSystem simply checks whether all
prerequisite quests are complete before unlocking the next step.

### Camp Meal Design Philosophy (inspired by FF15)

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L1577) (line 1577)

───────────────────────────────────────────────────────────────
Each recipe has a distinct buff theme:
  • Survival Fried Rice:    small all-rounder (good for early game)
  • Grilled Barramundi:     HP/defence focus (tank role)
  • Hunter's Stew:         balanced ATK+DEF (frontliner)
  • Fried Clam Fettuccine: magic focus (mage build)
  • Mother and Child Bowl: party HP + MP (healer-friendly)
  • Sautéed Mushrooms:     magic focus, cheap ingredients
  • Smoked Behemoth:       attack specialist (late-game power)
  • Crownsguard Chowder:   ultimate defence (endgame)

Ingredient items use IDs that correspond to existing item database
entries.  The camp UI checks the player's inventory for each ingredient.

### Shop Progression

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L1662) (line 1662)

──────────────────────────────────
Shops are gated by location and story progress:
  Hammerhead:   starter area; basic weapons, common consumables.
  Lestallum:    mid-game city; wider selection, elemental gear.
  Meldacio:     mid-late game; advanced weapons, accessories.
  Hunter's HQ:  reward shop; rare equipment bought with gil/tokens.

`buyMultiplier=1.0` means items sell at their listed price.
`sellMultiplier=0.5` means players recover half the price — standard RPG.
A high-tier shop might offer `sellMultiplier=0.6` as a small reward.

### Hunt Design

**Source:** [`src/game/GameData.hpp`](src/game/GameData.hpp#L1704) (line 1704)

────────────────────────────
Hunts are optional contracts posted at the Hunter's HQ.  They are
designed around the idea that players voluntarily seek out powerful
enemies for great rewards, rather than stumbling into them.

The `prereqQuestID` field gates high-level hunts behind story progress.
For example, the Iron Giant hunt requires completing "The Road to Dawn"
to ensure the player has at least basic equipment.

---

## game/systems

### A* Algorithm Overview

**Source:** [`src/game/systems/AISystem.cpp`](src/game/systems/AISystem.cpp#L6) (line 6)

============================================================================

A* is the gold standard for grid pathfinding.  It extends Dijkstra's
algorithm with a heuristic function h(n) that estimates the remaining
cost from node n to the goal.

Core formula: f(n) = g(n) + h(n)
  g(n) = exact cost from start to n (accumulated step costs)
  h(n) = estimated cost from n to goal (Manhattan distance)
  f(n) = priority of n in the open set

The algorithm always expands the node with the lowest f(n) first.
If h(n) never overestimates (admissible), A* finds the OPTIMAL path.

@author  Educational Game Engine Project
@version 1.0

### Manhattan distance is admissible for 4-directional grids.

**Source:** [`src/game/systems/AISystem.cpp`](src/game/systems/AISystem.cpp#L378) (line 378)

return static_cast<float>(std::abs(a.tileX - b.tileX) +
std::abs(a.tileY - b.tileY));
}

### We have two enums for the same concept because the

**Source:** [`src/game/systems/AISystem.cpp`](src/game/systems/AISystem.cpp#L449) (line 449)

AISystem header defines its own AIState for readability, while
AIComponent::State is the stored value.  We bridge them here.
switch (newState) {
case AIState::IDLE:      ai.currentState = AIComponent::State::IDLE;   break;
case AIState::WANDERING: ai.currentState = AIComponent::State::PATROL; break;
case AIState::CHASING:   ai.currentState = AIComponent::State::CHASE;  break;
case AIState::ATTACKING: ai.currentState = AIComponent::State::ATTACK; break;
case AIState::FLEEING:   ai.currentState = AIComponent::State::FLEE;   break;
case AIState::DEAD:      ai.currentState = AIComponent::State::DEAD;   break;
}
ai.stateTimer = 0.0f;
}

### Enemy AI Architecture

**Source:** [`src/game/systems/AISystem.hpp`](src/game/systems/AISystem.hpp#L6) (line 6)

============================================================================

Artificial Intelligence in games is NOT about making "smart" opponents.
It is about making opponents that are FUN TO PLAY AGAINST — which often
means making them predictable enough that players can learn their patterns.

─── Finite State Machine (FSM) ─────────────────────────────────────────────

The simplest and most common AI architecture is the Finite State Machine.
An entity is always in exactly ONE state, and TRANSITIONS happen based on
conditions (distance to player, HP percentage, timer expiry).

Our enemy FSM:

  ┌──────┐  always     ┌───────────┐  player in range  ┌─────────┐
  │ IDLE │ ──────────► │ WANDERING │ ────────────────► │ CHASING │
  └──────┘             └───────────┘                   └────┬────┘
      ▲                      ▲                              │
      │  player lost         │  player far away             │ player adjacent
      │                      └──────────────────────────────│──────────────┐
      │                                                      ▼              │
      │                                               ┌──────────┐         │
      │  HP < 25%                                     │ ATTACKING│ ◄───────┘
      │ ◄─────────────────────────────────────────── └──────────┘
      │                                                    │
      ▼                                                    │ HP < 25%
  ┌─────────┐                                             ▼
  │ FLEEING │                                         ┌──────┐
  └─────────┘                                         │ DEAD │
                                                      └──────┘

### Why FSM?

**Source:** [`src/game/systems/AISystem.hpp`](src/game/systems/AISystem.hpp#L38) (line 38)

The FSM pattern is:
  ✓ Easy to understand and debug (always know what state an entity is in).
  ✓ Easy to extend (add a STUNNED state without touching other states).
  ✓ Deterministic (same inputs → same outputs; reproducible bugs).
  ✗ Doesn't scale to complex behaviours (use Behaviour Trees for that).

─── Behaviour Trees (Advanced Concept) ─────────────────────────────────────

Behaviour Trees are a more powerful alternative to FSMs used in production
games (Halo, The Last of Us, Unreal Engine).  They represent behaviour as
a tree of nodes:

  Selector (try children left to right until one succeeds)
    ├── Sequence (is enemy adjacent AND has cooldown expired?)
    │     ├── CheckAdjacent(player)
    │     └── AttackAction(player)
    └── Sequence (can enemy see player?)
          ├── CheckLineOfSight(player)
          └── MoveTowardAction(player)

We don't implement a full BT here to keep the code educational, but
students are encouraged to research it as a next step.

─── A* Pathfinding ──────────────────────────────────────────────────────────

A* (pronounced "A star") is the gold-standard 2D grid pathfinding algorithm.
It finds the SHORTEST path from start to goal, guaranteed.

A* extends Dijkstra's algorithm with a HEURISTIC ESTIMATE of the remaining
distance, making it much faster in practice.

Key concepts:

  g(n) = exact cost to reach node n from start (number of steps so far).
  h(n) = HEURISTIC estimate of distance from n to goal (Manhattan distance).
  f(n) = g(n) + h(n) = total estimated cost through n.

  OPEN SET   = nodes to be evaluated (priority queue ordered by f).
  CLOSED SET = nodes already evaluated.

Algorithm:
  1. Add start to open set with f=h(start).
  2. Pop the node with lowest f from open set.
  3. If it is the goal, reconstruct path and return.
  4. For each passable neighbour:
     - Compute g' = g(current) + stepCost.
     - If g' < g(neighbour) (found a shorter path), update neighbour and
       add it to the open set with new f = g' + h(neighbour).
  5. If open set is empty, no path exists.

MANHATTAN HEURISTIC: h(n) = |n.x - goal.x| + |n.y - goal.y|
Admissible (never overestimates on a 4-directional grid without diagonal).

─── TileCoord ───────────────────────────────────────────────────────────────

Pathfinding works in TILE coordinates (integers), not world coordinates
(floats).  The TileCoord struct from Types.hpp holds (tileX, tileY).
The TransformComponent stores (x, y) in WORLD units; we divide by
TILE_SIZE to convert.

@author  Educational Game Engine Project
@version 1.0
@date    2024

C++ Standard: C++17

### Mirroring Component Enums

**Source:** [`src/game/systems/AISystem.hpp`](src/game/systems/AISystem.hpp#L130) (line 130)

───────────────────────────────────────────
AIComponent (in ECS.hpp) has its own `State` enum for the same values.
We declare a standalone `AIState` enum here for use in AISystem logic
(switch statements, function signatures) to keep the system header
self-contained.  In the Update loop, we sync between AIComponent::State
and AIState via static_cast (they have matching underlying values).

### Priority Queue Ordering

**Source:** [`src/game/systems/AISystem.hpp`](src/game/systems/AISystem.hpp#L156) (line 156)

──────────────────────────────────────────
std::priority_queue is a MAX-heap by default.  For A* we need a MIN-heap
(process lowest f-cost first).  We achieve this by defining operator>
and using `std::greater<PathNode>` as the comparator:

  std::priority_queue<PathNode, std::vector<PathNode>, std::greater<PathNode>>

The operator> compares fCost, making the queue pop the lowest fCost.

### A* Complexity

**Source:** [`src/game/systems/AISystem.hpp`](src/game/systems/AISystem.hpp#L260) (line 260)

─────────────────────────────────
Time:  O(E log V) where V = number of tiles, E = edges (4 neighbours each).
Space: O(V) for the open/closed sets and came-from map.

For a 40×40 tile map (1600 tiles) this is very fast (<0.1ms).
For very large maps, consider:
  - Hierarchical pathfinding (HPA*): divide map into clusters.
  - Pre-computed waypoint graphs for known routes.
  - Steering behaviours (seek, flee) for enemies that don't need
    exact paths.

### Simple vs Complex AI Decision Making

**Source:** [`src/game/systems/AISystem.hpp`](src/game/systems/AISystem.hpp#L322) (line 322)

──────────────────────────────────────────────────────
This is REACTIVE AI: the enemy looks at current state and picks an
action.  More sophisticated AI (used in chess, StarCraft) uses
LOOK-AHEAD: "If I do X, what will the player do, and how should I
respond to THAT?".  Look-ahead requires search trees (minimax, MCTS)
which are far beyond a simple action-RPG's needs.

### Path Reconstruction

**Source:** [`src/game/systems/AISystem.hpp`](src/game/systems/AISystem.hpp#L364) (line 364)

──────────────────────────────────────
During A* we store `cameFrom[n] = parent_of_n` for each node we visit.
When we reach the goal, we walk backwards through cameFrom:
  goal → parent → grandparent → … → start
Then reverse the list to get start → goal order.

### Optional Lua dependency (see CombatSystem.cpp for context)

**Source:** [`src/game/systems/CampSystem.cpp`](src/game/systems/CampSystem.cpp#L11) (line 11)

ifdef ENGINE_ENABLE_LUA
include "../../engine/scripting/LuaEngine.hpp"  // on_camp_rest, on_level_up hooks
endif

### We use const TileMap& to signal read-only access.

**Source:** [`src/game/systems/CampSystem.cpp`](src/game/systems/CampSystem.cpp#L36) (line 36)

const Tile& tile = map.GetTile(x, y);

### Check ALL ingredients before consuming ANY.

**Source:** [`src/game/systems/CampSystem.cpp`](src/game/systems/CampSystem.cpp#L121) (line 121)

if (!HasIngredients(player, *recipe)) {
if (m_uiBus) {
UIEvent ev;
ev.type = UIEvent::Type::SHOW_NOTIFICATION;
ev.text = "Missing ingredients for " + recipe->name + ".";
m_uiBus->Publish(ev);
}
return false;
}

### FF15's pending XP system: combat awards "tentative" XP

**Source:** [`src/game/systems/CampSystem.cpp`](src/game/systems/CampSystem.cpp#L211) (line 211)

which only converts to real levels at camp.  This creates tension.
if (m_world->HasComponent<LevelComponent>(player)) {
auto& lc   = m_world->GetComponent<LevelComponent>(player);
bool levUp = lc.pendingLevelUp;
uint32_t oldLevel = lc.level;
lc.ApplyBankedXP();

### Conditional Hook Firing

**Source:** [`src/game/systems/CampSystem.cpp`](src/game/systems/CampSystem.cpp#L227) (line 227)

We only fire on_level_up when the level actually changed.
Comparing oldLevel to lc.level (after ApplyBankedXP) is the
canonical pattern: "detect the change, then notify observers."

The new level is passed as an integer argument so Lua scripts can
branch on specific thresholds:
  function on_level_up(newLevel)
      if newLevel == 10 then unlock_ultimate_technique() end
  end
if (lc.level > oldLevel) {
ifdef ENGINE_ENABLE_LUA
LuaEngine::Get().CallFunction("on_level_up",
static_cast<int>(lc.level));
endif
}
}

### Ordering Hook Calls

**Source:** [`src/game/systems/CampSystem.cpp`](src/game/systems/CampSystem.cpp#L258) (line 258)

on_camp_rest fires AFTER HP/MP restoration, XP application, and level-ups
are all complete.  This means Lua scripts can safely query the player's
new level, current HP, etc. without worrying about partial state.

Example use in quests.lua:
  function on_camp_rest()
      GameState.day = GameState.day + 1
      engine_log("Day " .. GameState.day .. " begins.")
  end
ifdef ENGINE_ENABLE_LUA
LuaEngine::Get().CallFunction("on_camp_rest");
endif

### Camp System Inspiration (FF15)

**Source:** [`src/game/systems/CampSystem.hpp`](src/game/systems/CampSystem.hpp#L6) (line 6)

============================================================================

Final Fantasy XV's camp system is one of the most beloved features in
the series.  The key design insight: camping is NOT just a "save and
rest" button.  It is a RITUAL that:

 1. Converts PENDING XP → ACTUAL LEVELS (delayed gratification).
 2. Applies MEAL BUFFS crafted by Ignis (strategic food preparation).
 3. ADVANCES TIME (narrative consequence — Ardyn's demons are stronger
    at night, so dawn camping choices are meaningful).
 4. Provides SOCIAL MOMENTS (party banter — not modelled in this engine
    but mentioned for completeness).

Our CampSystem captures points 1-3.

─── Delayed XP Levelling ────────────────────────────────────────────────────

Combat grants XP to LevelComponent::pendingXP.  It is NOT immediately
added to currentXP.  When Rest() is called, ApplyBankedXP() is invoked
on the LevelComponent, converting pending XP into actual level-ups.

Why delay?  It creates a tension: "Do I explore more to bank more XP,
or rest now before I die?".  It also avoids mid-dungeon level-up UI
interrupting tense gameplay.

─── Meal Buffs ──────────────────────────────────────────────────────────────

Each recipe (RecipeData in GameData.hpp) provides temporary stat bonuses
stored in CampComponent.  The CampSystem copies the bonus values from
RecipeData into CampComponent fields after cooking.  The CombatSystem
reads CampComponent to add meal bonuses to damage calculations.

─── ActiveMealBuff ──────────────────────────────────────────────────────────

A single `static ActiveMealBuff currentBuff` tracks the current meal
across the game session.  Making it static (rather than per-entity) is
a simplification for this educational engine — in a multiplayer or
multi-party game, you'd need one buff per character.

─── Safe Camping ────────────────────────────────────────────────────────────

CanCamp() checks:
  1. The tile at (x, y) has TileType::CAMP_SITE or is GRASS/PLAINS
     (not inside a dungeon — TileType::WALL or DUNGEON tiles block camping).
  2. No enemies are in close proximity (checked via AIComponent::state).

@author  Educational Game Engine Project
@version 1.0
@date    2024

C++ Standard: C++17

### Static Data for Game-Global State

**Source:** [`src/game/systems/CampSystem.hpp`](src/game/systems/CampSystem.hpp#L82) (line 82)

──────────────────────────────────────────────────
`CampSystem::currentBuff` is declared `static` because there is only
ONE active meal at a time in the game, regardless of how many CampSystem
instances exist.  Class-level (static) data is initialised ONCE for the
whole program.

Alternatives:
  - Store in CampComponent on the player entity (preferred in production).
  - Store in a global singleton GameState struct.

For this educational engine, the static field keeps the example simple
while still demonstrating the concept of "game-global state".

### Const References for Read-Only Parameters

**Source:** [`src/game/systems/CampSystem.hpp`](src/game/systems/CampSystem.hpp#L168) (line 168)

───────────────────────────────────────────────────────────
We pass `map` as a const reference because we only need to READ tile
data.  Using const& instead of a pointer communicates to the reader
that: (1) the parameter is never null, and (2) the function will not
modify the map.  This is a good C++ practice for "inspect but don't
modify" parameters.

### Ingredient Validation

**Source:** [`src/game/systems/CampSystem.hpp`](src/game/systems/CampSystem.hpp#L220) (line 220)

──────────────────────────────────────
We ALWAYS check ALL ingredients before consuming ANY.  This prevents
the frustrating situation where the game consumes half your items and
then fails because of the last ingredient.  "All-or-nothing" validation
is a core pattern in any transaction-like operation.

### Why Rest is a Rich System

**Source:** [`src/game/systems/CampSystem.hpp`](src/game/systems/CampSystem.hpp#L256) (line 256)

───────────────────────────────────────────
Many students initially implement "rest" as just `hp = maxHp`.
But rich rest mechanics create meaningful decisions:
  - Time advances → some quests/NPCs are time-sensitive.
  - Levelling happens → plan your ability upgrades.
  - Meal buffs expire → choose the right recipe for tomorrow's combat.

### When to Use Static Members

**Source:** [`src/game/systems/CampSystem.hpp`](src/game/systems/CampSystem.hpp#L277) (line 277)

────────────────────────────────────────────
Use static members for data that is genuinely SINGULAR: the current
meal, the game clock, the player's currency.  Avoid statics for data
that could plausibly need multiple instances in a future design.

### Optional Lua scripting dependency

**Source:** [`src/game/systems/CombatSystem.cpp`](src/game/systems/CombatSystem.cpp#L13) (line 13)

LuaEngine.hpp requires the Lua 5.4 headers (lua5.4/lua.h) which are only
available when Lua is installed.  ENGINE_ENABLE_LUA is defined by CMake for
targets that link Lua (the terminal `game` target).  engine_sandbox builds
on Windows without Lua, so the scripting hook is compiled out — combat
still works fully, the Lua callback simply does not fire.
ifdef ENGINE_ENABLE_LUA
include "../../engine/scripting/LuaEngine.hpp"  // on_combat_start hook
endif

### Using the EventBus Singleton

**Source:** [`src/game/systems/CombatSystem.cpp`](src/game/systems/CombatSystem.cpp#L91) (line 91)

EventBus<CombatEvent>::Instance() returns the global bus for CombatEvents.
Any system that subscribed (UI, audio, camera) will receive this event.
CombatEvent ev;
ev.type     = CombatEvent::Type::SKILL_USED; // Closest type for battle start.
ev.sourceID = player;
EventBus<CombatEvent>::Instance().Publish(ev);

### C++ → Lua Hook Pattern

**Source:** [`src/game/systems/CombatSystem.cpp`](src/game/systems/CombatSystem.cpp#L108) (line 108)

─────────────────────────────────────────
A "hook" is a named Lua function that the engine calls at a predefined
moment (battle start, level-up, camp rest, etc.).  Designers and modders
can define these functions in .lua files to customise behaviour without
recompiling the C++ engine.

Convention used in this engine:
  • C++ calls  LuaEngine::Get().CallFunction("on_<event>", ...)
  • Lua defines function on_<event>(...) end   in a .lua script
  • If the function is NOT defined in Lua, CallFunction returns false
    and logs a debug message — it is never a fatal error.

For on_combat_start we pass the name of the first enemy as a string
argument so quest scripts can react ("if enemyName == 'Goblin' then ...").
{
std::string firstEnemyName = "Unknown";
if (!m_aliveEnemies.empty()) {
EntityID firstEnemy = m_aliveEnemies[0];
if (m_world->HasComponent<NameComponent>(firstEnemy)) {
firstEnemyName = m_world->GetComponent<NameComponent>(firstEnemy).name;
}
}
ifdef ENGINE_ENABLE_LUA
LuaEngine::Get().CallFunction("on_combat_start", firstEnemyName);
endif
}
}

### Status Effect Ticking

**Source:** [`src/game/systems/CombatSystem.cpp`](src/game/systems/CombatSystem.cpp#L147) (line 147)

We accumulate time in m_statusTickTimer.  When it crosses the interval,
we process all DoT/buff effects and reset the timer.
This prevents status effects from being processed EVERY frame (wasteful)
while still applying them on a consistent real-time cadence.
m_statusTickTimer -= dt;
if (m_statusTickTimer <= 0.0f) {
m_statusTickTimer = STATUS_TICK_INTERVAL;

### Player vs. Enemy Action Timing

**Source:** [`src/game/systems/CombatSystem.cpp`](src/game/systems/CombatSystem.cpp#L178) (line 178)

m_turn == 0 means it is the player's turn.  Player actions are triggered
by button presses (PlayerAttack, PlayerFlee, etc.), not by the ATB timer.
Enemy turns ARE driven by the ATB timer: when the timer fires for turn>0,
the enemy at index (m_turn-1) acts automatically.
if (m_turn > 0) {
int enemyIdx = m_turn - 1;
if (enemyIdx < static_cast<int>(m_aliveEnemies.size())) {
EnemyTurn(m_aliveEnemies[enemyIdx]);
}
AdvanceTurn();
}
}

### Reading Equipment Stats

**Source:** [`src/game/systems/CombatSystem.cpp`](src/game/systems/CombatSystem.cpp#L209) (line 209)

The weapon's attack bonus is stored as a CACHED value in
EquipmentComponent::bonusStrength to avoid re-scanning the database
on every attack.  RecalculateEquipStats() must be called whenever
equipment changes (handled by InventorySystem).
int weaponBonus = 0;
if (m_world->HasComponent<EquipmentComponent>(m_playerID)) {
auto& eq = m_world->GetComponent<EquipmentComponent>(m_playerID);
weaponBonus = eq.bonusStrength;
}

### Order Matters Here

**Source:** [`src/game/systems/CombatSystem.cpp`](src/game/systems/CombatSystem.cpp#L226) (line 226)

Base player damage is applied first, then link strikes.
ProcessLinkStrikes() applies its own damage to targetHp internally
and fires its own event, so we only subtract base damage here.
targetHp.hp = std::max(0, targetHp.hp - damage);

### Skill Damage Scaling

**Source:** [`src/game/systems/CombatSystem.cpp`](src/game/systems/CombatSystem.cpp#L299) (line 299)

Each skill multiplies base physical attack by a skill-specific factor.
For simplicity, we hardcode 1.5× for all skills here.  In a full
engine, the factor would be stored in a SkillData database entry.
int weaponBonus = 0;
if (m_world->HasComponent<EquipmentComponent>(m_playerID)) {
weaponBonus = m_world->GetComponent<EquipmentComponent>(m_playerID).bonusStrength;
}

### Magic Damage Formula

**Source:** [`src/game/systems/CombatSystem.cpp`](src/game/systems/CombatSystem.cpp#L371) (line 371)

magic_damage = spellData.baseDamage × (caster.magic / 10.0)
The caster's magic stat scales spell power.  Division by 10 normalises
the multiplier so a magic stat of 50 → 5× base, 100 → 10× base.
int magicStat = 10;
if (m_world->HasComponent<StatsComponent>(m_playerID)) {
magicStat = m_world->GetComponent<StatsComponent>(m_playerID).magic;
}

### Warp-Strike Teleportation

**Source:** [`src/game/systems/CombatSystem.cpp`](src/game/systems/CombatSystem.cpp#L477) (line 477)

We copy the target's TransformComponent position to the player,
offset by one tile.  No projectile arc is simulated in the
tile-based engine — the effect is instantaneous from the player's
perspective, matching FF15's near-instant warp animations.
if (m_world->HasComponent<TransformComponent>(target) &&
m_world->HasComponent<TransformComponent>(m_playerID))
{
auto& targetTf = m_world->GetComponent<TransformComponent>(target);
auto& playerTf = m_world->GetComponent<TransformComponent>(m_playerID);
Place player one tile above the target (in tile space).
playerTf.position.x = targetTf.position.x;
playerTf.position.z = targetTf.position.z - TILE_SIZE;
}

### Enemy Ability Selection

**Source:** [`src/game/systems/CombatSystem.cpp`](src/game/systems/CombatSystem.cpp#L597) (line 597)

A real AI would pick from a list of abilities based on HP, MP, and
cooldowns.  For this base implementation, all enemies use a basic
physical attack.  Override this with the SelectAbility() logic from
AISystem for more interesting fights.
int enemyAttack = 10;
if (m_world->HasComponent<StatsComponent>(enemy)) {
enemyAttack = m_world->GetComponent<StatsComponent>(enemy).strength;
}

### Damage Formula Breakdown

**Source:** [`src/game/systems/CombatSystem.cpp`](src/game/systems/CombatSystem.cpp#L669) (line 669)

raw = max(1, (strength × 2 + baseDmg) - defence)

Why multiply strength by 2?  This amplifies the STAT DIFFERENCE between
characters.  A hero with 30 strength vs a goblin with 5 defence deals:
  raw = max(1, 60 + baseDmg - 5) = 55 + baseDmg

If we didn't multiply by 2, higher-level characters would barely outdamage
lower-level ones.  The ×2 creates the visible "power scaling" feel.
int raw = std::max(1, (atkStrength * 2 + baseDmg) - defDefence);

### Damage Variance

**Source:** [`src/game/systems/CombatSystem.cpp`](src/game/systems/CombatSystem.cpp#L694) (line 694)

A small random range (85–115% of base) prevents combat from feeling
mechanical.  The player doesn't know the exact formula, so variance
makes outcomes feel "organic".  Too much variance (50–200%) would
make planning feel pointless; too little (99–101%) makes it
predictable and boring.
std::uniform_real_distribution<float> varianceDist(0.85f, 1.15f);
float variance = varianceDist(m_rng);

### DoT Scaling

**Source:** [`src/game/systems/CombatSystem.cpp`](src/game/systems/CombatSystem.cpp#L723) (line 723)

Poison deals 5% of the target's max HP per tick.
This scales with target's health, making poison relevant at all
levels without needing to rebalance poison damage manually.
if (m_world->HasComponent<HealthComponent>(target)) {
tickDmg = m_world->GetComponent<HealthComponent>(target).maxHp * 0.05f;
}
}

### Looking Up Enemy Data

**Source:** [`src/game/systems/CombatSystem.cpp`](src/game/systems/CombatSystem.cpp#L828) (line 828)

We use NameComponent to find the enemy's data ID.  In a production engine
you'd store the EnemyData::id in a dedicated EnemyTypeComponent.
For simplicity, we use CombatComponent::xpReward as a proxy species ID.
if (!m_world->HasComponent<CombatComponent>(defender)) return 1.0f;

### Party Link Strikes

**Source:** [`src/game/systems/CombatSystem.cpp`](src/game/systems/CombatSystem.cpp#L874) (line 874)

─────────────────────────────────────────────────────────────────────────
In FF15, party members spontaneously assist the player when the player
lands a hit.  Each companion has a 30% chance to "link" in and auto-
attack the same target for 30% of their own physical attack damage.

This mechanic rewards keeping all four party members alive and healthy:
  • More living members  →  more link-strike chances per player attack.
  • Higher-level members →  more bonus damage per link strike.

Implementation note: we accumulate ALL link-strike damage and apply it
to the target's HP in a single step here, then publish one composite
event rather than one event per member.  This keeps the UI readable and
avoids multiple redundant death checks.

### Why apply damage here instead of in PlayerAttack?

**Source:** [`src/game/systems/CombatSystem.cpp`](src/game/systems/CombatSystem.cpp#L926) (line 926)

ProcessLinkStrikes is a helper that owns the full link-strike
resolution: roll chances, compute damage, AND apply it.  The caller
(PlayerAttack) only needs the final integer to display in the UI.
Keeping the side effects here avoids duplicated HP subtraction logic.
targetHp.hp = std::max(0, targetHp.hp - totalLinkDamage);

### Iterating and Removing from Arrays

**Source:** [`src/game/systems/CombatSystem.cpp`](src/game/systems/CombatSystem.cpp#L955) (line 955)

We iterate backward so that erasing an entry doesn't skip the next one.
for (uint32_t i = 0; i < se.count; ) {
auto& entry = se.active[i];
entry.remaining -= dt;

### Level-Difference XP Bonus

**Source:** [`src/game/systems/CombatSystem.cpp`](src/game/systems/CombatSystem.cpp#L1001) (line 1001)

If the enemy is higher level than the player, the player earns a
10% XP bonus per level difference.  This rewards the risk of
fighting tougher enemies and encourages exploration into danger zones.
int playerLevel = 1;
if (m_world->HasComponent<LevelComponent>(m_playerID)) {
playerLevel = m_world->GetComponent<LevelComponent>(m_playerID).level;
}
int enemyLevel = 1;
if (m_world->HasComponent<LevelComponent>(enemy)) {
enemyLevel = m_world->GetComponent<LevelComponent>(enemy).level;
}
int levelDiff = std::max(0, enemyLevel - playerLevel);
float xpMult  = 1.0f + levelDiff * 0.1f;
int xpGained  = static_cast<int>(combat.xpReward * xpMult);

### Random Item Drops

**Source:** [`src/game/systems/CombatSystem.cpp`](src/game/systems/CombatSystem.cpp#L1031) (line 1031)

We look up the enemy's species data to find the drop table.
Each item in dropItemIDs has a 30% chance to drop (simple model).
A production engine would use per-item drop rates and rarity tiers.
const EnemyData* enemyData = GameDatabase::FindEnemy(combat.xpReward);
if (enemyData) {
std::uniform_real_distribution<float> dist(0.0f, 1.0f);
for (uint32_t dropID : enemyData->dropItemIDs) {
if (dist(m_rng) < 0.30f) {
m_result.itemsDropped.push_back(dropID);

### Turn Cycling

**Source:** [`src/game/systems/CombatSystem.cpp`](src/game/systems/CombatSystem.cpp#L1071) (line 1071)

Turn order cycles: 0 (player) → 1 → 2 → ... → aliveEnemies.size() → 0
We skip turns for dead entities automatically.
if (m_aliveEnemies.empty()) {
m_turn = 0;
return;
}

### Combat System Architecture

**Source:** [`src/game/systems/CombatSystem.hpp`](src/game/systems/CombatSystem.hpp#L6) (line 6)

============================================================================

What makes combat systems interesting to study is that they must balance
three competing concerns at once:

 1. GAME DESIGN  — Is fighting fun?  Is it fair?  Is it easy to balance?
 2. PERFORMANCE  — Can we evaluate 6 enemies every frame without lag?
 3. READABILITY  — Can a new programmer understand the code in minutes?

─── Turn-Based vs Real-Time ────────────────────────────────────────────────

Classic JRPGs (Final Fantasy 1-10) use strict turn-based combat: you pick
an action, then the enemy picks an action, then both resolve.  This is
easy to implement and reason about, but can feel slow.

Modern action RPGs (FF15, FF16, Kingdom Hearts) use real-time combat where
button presses trigger immediate attacks.  Under the hood, however, most
still have a discrete "action resolution" step — they just hide it behind
animations that mask the latency.

OUR APPROACH:  We implement TURN-BASED resolution (simple, teachable) while
using a `turnTimer` float to make it FEEL real-time.  When the timer ticks
down, the current actor takes their action and the timer resets.  This is
called an "Active Time Battle" (ATB) system, first introduced in FF4.

─── ECS Integration ────────────────────────────────────────────────────────

The combat system reads from components (StatsComponent, HealthComponent,
etc.) but does NOT store per-entity combat state itself.  Temporary combat
state (status effects, cooldowns) lives in StatusEffectsComponent on the
entity.  The system only orchestrates WHAT happens, not what entities ARE.

─── Damage Formula ─────────────────────────────────────────────────────────

Our formula is inspired by Final Fantasy XV's damage model:

  rawDmg  = (attacker.strength * 2 + weapon.attack) - defender.defence
  damage  = rawDmg * elementMultiplier * critMultiplier * variance

where:
  elementMultiplier  = 1.5  if defender is weak to the element
                     = 0.5  if defender resists the element
                     = 1.0  otherwise
  critMultiplier     = 1.5  on critical hit, 1.0 otherwise
  variance           = random float in [0.85, 1.15] for natural spread

─── Status Effects ─────────────────────────────────────────────────────────

Status effects are stored as a bitmask in StatusEffectsComponent.  On each
`statusTickTimer` interval, the combat system iterates all combatants and
applies periodic damage (POISON, BURNED) or checks for expiry.

─── Warp-Strike ────────────────────────────────────────────────────────────

Warp-Strike is a signature ability from FF15: Noctis throws his weapon to
a distant enemy, then warps to it for a powerful strike.  In our engine it
teleports the player's TransformComponent to the target, then deals
1.5× normal damage.  No projectile physics needed for the tile-map scale.

─── Link Strikes ───────────────────────────────────────────────────────────

When the player lands a hit, each other party member has a 30% chance to
automatically "link" in and add 30% of their own attack as bonus damage.
This models the party cooperation mechanic from FF15 Techniques.

@author  Educational Game Engine Project
@version 1.0
@date    2024

C++ Standard: C++17

### Returning Complex Values

**Source:** [`src/game/systems/CombatSystem.hpp`](src/game/systems/CombatSystem.hpp#L111) (line 111)

─────────────────────────────────────────
Rather than passing multiple out-parameters to GetCombatResult():
  GetCombatResult(bool& won, int& xp, int& gil, vector<uint32_t>& items)
we bundle everything into one struct.  This makes the call site cleaner:
  auto result = combat.GetCombatResult();
  if (result.playerWon) { ... }

In C++17 you can also use structured bindings:
  auto [won, xp, gil, drops] = combat.GetCombatResult();  // if we add ==
But for struct types, named member access is usually clearer.

### Dependency Injection

**Source:** [`src/game/systems/CombatSystem.hpp`](src/game/systems/CombatSystem.hpp#L168) (line 168)

──────────────────────────────────────
We pass the World as a constructor parameter rather than using a global
variable.  This is "dependency injection": the CombatSystem DECLARES its
dependencies clearly, making it easier to:
  • Unit-test (swap in a mock World).
  • Reason about lifetime (the World must outlive the CombatSystem).
  • Avoid hidden coupling through globals.

### Delta Time

**Source:** [`src/game/systems/CombatSystem.hpp`](src/game/systems/CombatSystem.hpp#L204) (line 204)

────────────────────────────
`dt` (delta time) is the elapsed real time since the last Update() call.
Using dt for timers instead of frame counts decouples game logic from
frame rate.  A 60 FPS game and a 30 FPS game both see the same timer
behaviour because:
  timer -= dt;   // at 60 FPS: timer -= 0.0167 per frame
                 // at 30 FPS: timer -= 0.0333 per frame
Both halve the timer at the same wall-clock rate.

### Enemy AI in Combat

**Source:** [`src/game/systems/CombatSystem.hpp`](src/game/systems/CombatSystem.hpp#L297) (line 297)

────────────────────────────────────
We separate STRATEGIC AI (handled by AISystem: patrol, chase, flee)
from TACTICAL AI (handled here: which action to use in battle).
This keeps each class focused on one responsibility (Single Responsibility
Principle from SOLID design principles).

### AddItem: Stacking vs Slot Allocation

**Source:** [`src/game/systems/InventorySystem.cpp`](src/game/systems/InventorySystem.cpp#L14) (line 14)

---------------------------------------------------------------------------
When adding a stackable item we first try to merge it into an existing
slot that holds the same itemID (stacking).  Only if no existing stack
exists do we consume a new empty slot.  This mirrors how FF15 and most
RPGs manage inventory: potions stack to 99, weapons each take a full slot.
The m_uiBus publish notifies the UI layer without any direct dependency
between InventorySystem and the UI rendering code — a clean separation
achieved via the EventBus pattern.
---------------------------------------------------------------------------
bool InventorySystem::AddItem(EntityID entity, uint32_t itemID, uint32_t quantity)
{
if (!m_world->HasComponent<InventoryComponent>(entity)) return false;
auto& inv = m_world->GetComponent<InventoryComponent>(entity);

### UseItem: Remove-Then-Apply Pattern

**Source:** [`src/game/systems/InventorySystem.cpp`](src/game/systems/InventorySystem.cpp#L89) (line 89)

---------------------------------------------------------------------------
We remove the item from inventory BEFORE applying its effect.  This
prevents exploits where an effect callback might add the same item back
and allow infinite use.  It also means a "use" that fails (e.g. HP is
already full) still consumes the item — matching the design of most
action RPGs including FF15.
---------------------------------------------------------------------------
bool InventorySystem::UseItem(EntityID entity, uint32_t itemID)
{
const ItemData* item = GameDatabase::FindItem(itemID);
if (!item) return false;
if (!RemoveItem(entity, itemID, 1)) return false;

### TransferItem: Pre-flight Space Check

**Source:** [`src/game/systems/InventorySystem.cpp`](src/game/systems/InventorySystem.cpp#L235) (line 235)

---------------------------------------------------------------------------
Before removing the item from the source we verify the destination
inventory can accept it.  We check for an existing stack (if stackable)
or a free slot — exactly the same logic AddItem would run.  This avoids
the "item destroyed on transfer to full bag" bug that affects many games.
---------------------------------------------------------------------------
bool InventorySystem::TransferItem(EntityID from, EntityID to,
uint32_t itemID, uint32_t qty)
{
if (GetItemCount(from, itemID) < qty) return false;

### Switch on Enum to Map to Struct Fields

**Source:** [`src/game/systems/InventorySystem.cpp`](src/game/systems/InventorySystem.cpp#L266) (line 266)

Each EquipSlot enum value maps to one field in EquipmentComponent.
We return a reference so callers can both read and write the slot.
switch (slot) {
case EquipSlot::WEAPON:     return equip.weaponID;
case EquipSlot::OFFHAND:    return equip.offhandID;
case EquipSlot::HEAD:       return equip.headID;
case EquipSlot::BODY:       return equip.bodyID;
case EquipSlot::LEGS:       return equip.legsID;
case EquipSlot::ACCESSORY1: return equip.accessory1;
case EquipSlot::ACCESSORY2: return equip.accessory2;
default:                    return equip.weaponID;  // fallback
}
}

### Inventory System Design

**Source:** [`src/game/systems/InventorySystem.hpp`](src/game/systems/InventorySystem.hpp#L6) (line 6)

============================================================================

An inventory system has more depth than it first appears.  Key concerns:

─── Storage Model ───────────────────────────────────────────────────────────

We use a SLOT-BASED inventory: the player has MAX_INV_SLOTS (99) slots,
each holding one item stack (itemID + quantity).  This matches classic
JRPG inventories (FF7-style) and is easy to render in a grid UI.

Alternative: WEIGHT-BASED (Skyrim, Diablo).  Each item has a weight value;
the player has a carry limit.  More realistic, but requires more UI work.

─── Equipment Slots ─────────────────────────────────────────────────────────

Equipment is separate from the general inventory.  When an item is equipped:
  1. It is removed from the inventory slot.
  2. Its item ID is stored in the appropriate EquipmentComponent field.
  3. RecalculateEquipStats() reads all equipped item stat bonuses and writes
     them into EquipmentComponent::bonusXxx fields (cached stats).

On each damage calculation, the CombatSystem reads the CACHED bonuses from
EquipmentComponent — it doesn't re-scan equipment on every hit.  The cache
is marked dirty (EquipmentComponent::statsDirty) when equipment changes.

─── EquipSlot enum ──────────────────────────────────────────────────────────

We define EquipSlot here (not in Types.hpp) because it is an implementation
detail of the inventory/equipment system.  Types.hpp holds vocabulary types
shared across ALL subsystems.  EquipSlot is only needed by InventorySystem,
UI, and serialisation — not by Combat or AI.

@author  Educational Game Engine Project
@version 1.0
@date    2024

C++ Standard: C++17

### Enum as a Switch/Array Index

**Source:** [`src/game/systems/InventorySystem.hpp`](src/game/systems/InventorySystem.hpp#L66) (line 66)

──────────────────────────────────────────────
Using `enum class` here means we can use EquipSlot values as switch-case
labels or as array indices (after a static_cast<int>) without ambiguity.
The `COUNT` sentinel at the end gives the number of valid slots:

  std::array<uint32_t, (int)EquipSlot::COUNT> slotItems;

This is a common C++ pattern to size arrays from enums.

### Stackable vs Non-Stackable Items

**Source:** [`src/game/systems/InventorySystem.hpp`](src/game/systems/InventorySystem.hpp#L142) (line 142)

──────────────────────────────────────────────────
Consumables (potions, ethers) are stackable: 10 potions occupy one slot.
Equipment items are NOT stackable: each sword takes its own slot.
We check ItemData::isStackable from the database to decide.

### Dirty Flags (Lazy Recalculation)

**Source:** [`src/game/systems/InventorySystem.hpp`](src/game/systems/InventorySystem.hpp#L219) (line 219)

──────────────────────────────────────────────────
Recomputing stats on every combat frame would be wasteful — equipment
rarely changes.  We set `statsDirty = true` when equipment changes,
then recalculate only when the stats are actually needed (here, eagerly
on any equip/unequip, but could be deferred to first combat use).
This "dirty flag" pattern is very common in game engines.

### Reference Returns from Helpers

**Source:** [`src/game/systems/InventorySystem.hpp`](src/game/systems/InventorySystem.hpp#L268) (line 268)

─────────────────────────────────────────────────
Returning a reference to a struct member avoids code duplication:
instead of 7 separate switch blocks in EquipItem and UnequipItem,
both delegate to this one helper.  The reference is valid as long as
the EquipmentComponent object lives (which is the World's lifetime).

### We convert the target's world position to tile

**Source:** [`src/game/systems/MagicSystem.cpp`](src/game/systems/MagicSystem.cpp#L111) (line 111)

coordinates before passing to ApplyAoe, which works in tile space.
if (spell->aoeRadius > 0 && m_world->HasComponent<TransformComponent>(target)) {
const auto& origin = m_world->GetComponent<TransformComponent>(target);
TileCoord centre{
static_cast<int>(origin.position.x / TILE_SIZE),
static_cast<int>(origin.position.z / TILE_SIZE)
};
ApplyAoe(caster, centre, spell->aoeRadius, *spell, map);
}

### We pass the caster ID as the first int argument and

**Source:** [`src/game/systems/MagicSystem.cpp`](src/game/systems/MagicSystem.cpp#L124) (line 124)

the target ID as a string (entity ID → string) to use the available
CallFunction overloads.  Lua scripts receive: casterID (int), targetID (string).
m_lua->CallFunction(spell->luaCastCallback,
static_cast<int>(caster),
std::to_string(static_cast<int>(target)));
}

### We do a dry-run check FIRST, then consume.

**Source:** [`src/game/systems/MagicSystem.cpp`](src/game/systems/MagicSystem.cpp#L159) (line 159)

This prevents partial-crafting side effects.
for (int i = 0; i < quantity; ++i) {
for (uint32_t ingredID : recipe->ingredientItemIDs) {
if (!invSys.RemoveItem(entity, ingredID, 1)) {
LOG_WARN("CraftMagic: insufficient ingredient " +
std::to_string(ingredID));
return false;
}
}
}

### Static local: constructed once, returned by reference.

**Source:** [`src/game/systems/MagicSystem.cpp`](src/game/systems/MagicSystem.cpp#L205) (line 205)

static const std::vector<MagicCraftRecipe> recipes = BuildCraftRecipes();
return recipes;
}

### Iterate all entities with TransformComponent and

**Source:** [`src/game/systems/MagicSystem.cpp`](src/game/systems/MagicSystem.cpp#L255) (line 255)

apply damage to those within `radius` tiles of the origin.
We use Chebyshev distance (max of |dx|, |dy|) for a square blast:
  |dx| <= radius AND |dy| <= radius  →  square area of effect.
(Manhattan distance would give a diamond shape instead.)
m_world->View<TransformComponent, HealthComponent>(
[&](EntityID id, TransformComponent& tc, HealthComponent& hc) {
if (id == caster) return;  // don't hit the caster
Convert world position to tile coordinates for distance check.
int entityTileX = static_cast<int>(tc.position.x / TILE_SIZE);
int entityTileZ = static_cast<int>(tc.position.z / TILE_SIZE);
int dx = std::abs(entityTileX - origin.tileX);
int dy = std::abs(entityTileZ - origin.tileY);
if (dx <= radius && dy <= radius) {
int aoe = ComputeSpellDamage(caster, id, spell);
hc.hp -= aoe;
if (hc.hp <= 0) { hc.hp = 0; hc.isDead = true; }
}
});
}

### Magic System Design: FF15 Inspired

**Source:** [`src/game/systems/MagicSystem.hpp`](src/game/systems/MagicSystem.hpp#L6) (line 6)

============================================================================

Final Fantasy XV introduced a unique magic system where spells are NOT
purchased from shops.  Instead, the player:
  1. Gathers elemental energy from sources in the world (Elemancy).
  2. Crafts spells (called "Flasks") by combining elemental energy with items.
  3. Throws the flasks in combat — they can hit allies too, making them
     dangerous to use carelessly.

This creates interesting resource management gameplay.  We simplify slightly:

OUR APPROACH:
  - Raw elemental items (collected in the world) map to ElementType.
  - CraftMagic() consumes ingredients and adds spell uses to MagicComponent.
  - CastSpell() deducts MP (the "flask" charge) and applies spell effects.

─── Spell as Data ──────────────────────────────────────────────────────────

We follow the "data-driven" philosophy: spells are rows in SpellData[]
(in GameData.hpp), not C++ subclasses.  The MagicSystem reads SpellData and
executes GENERIC behaviour:

  1. Deduct mpCost from caster's MP.
  2. Calculate baseDamage × caster.magic × element multiplier.
  3. If aoeRadius > 0, apply to all entities within radius on the TileMap.
  4. If luaCastCallback is set, call the Lua function for special effects.

This makes it trivial to add new spells: just add a row to GameData.
No new C++ code required.

─── Lua Integration ────────────────────────────────────────────────────────

Special spell effects (terrain modification, summons, time stop) are too
complex to encode in a flat data struct.  We delegate them to Lua scripts
via LuaEngine::CallFunction().  Students can modify spell behaviour by
editing Lua files without recompiling C++.

─── AoE and TileMap ────────────────────────────────────────────────────────

Area-of-effect spells pass a TileMap reference to find all entities near
the impact point.  This teaches students about spatial queries on 2D grids.

@author  Educational Game Engine Project
@version 1.0
@date    2024

C++ Standard: C++17

### Recipe as a Value Type

**Source:** [`src/game/systems/MagicSystem.hpp`](src/game/systems/MagicSystem.hpp#L79) (line 79)

─────────────────────────────────────────
A MagicCraftRecipe is a pure data struct (no methods, no pointers).
This makes it trivially copyable and safe to store in a static vector.

The `ingredientItemIDs` vector lists item IDs that are consumed when
crafting.  The resulting `quantity` units are added to the entity's
MagicComponent as charges for spells of the given element.

Example: crafting Fire magic requires itemID=5 (Fire Shard) × 2 and
produces 5 fire casts.

### AoE via TileMap

**Source:** [`src/game/systems/MagicSystem.hpp`](src/game/systems/MagicSystem.hpp#L170) (line 170)

─────────────────────────────────
For AoE spells, we iterate over all entities with TransformComponent
and test whether their tile coordinates fall within `aoeRadius` tiles
of the target's position.  This is a linear scan — O(N) where N is
the number of entities with TransformComponents.  For small maps this
is acceptable.  Spatial hashing or a quadtree would be needed for
thousands of entities.

### Consuming Multiple Resource Types

**Source:** [`src/game/systems/MagicSystem.hpp`](src/game/systems/MagicSystem.hpp#L200) (line 200)

───────────────────────────────────────────────────
This teaches "multi-resource consumption" patterns common in crafting
systems.  We loop `quantity` times and remove each ingredient batch
with RemoveItem.  If any removal fails (not enough items), we stop
early and return false — no partial crafting is performed.

### Static Local Initialisation (C++11)

**Source:** [`src/game/systems/MagicSystem.hpp`](src/game/systems/MagicSystem.hpp#L240) (line 240)

─────────────────────────────────────────────────────
Using `static const vector<…> recipes = BuildRecipes()` inside the
function body means the vector is constructed exactly once, the first
time the function is called, and then reused on all subsequent calls.
This is thread-safe in C++11 (initialisation is guaranteed to happen
exactly once even with concurrent calls).

### Separation of Concerns

**Source:** [`src/game/systems/QuestSystem.cpp`](src/game/systems/QuestSystem.cpp#L5) (line 5)

────────────────────────────────────────
The .cpp file contains ONLY implementation — no new types or macros.
This keeps the public interface (.hpp) clean and easy to read.

@author  Educational Game Engine Project
@version 1.0

### QuestEntry stores RUNTIME progress while QuestData holds

**Source:** [`src/game/systems/QuestSystem.cpp`](src/game/systems/QuestSystem.cpp#L62) (line 62)

the static definition.  We copy only the IDs, not the full definition,
to keep memory usage low.
QuestEntry& entry = qc.quests[qc.activeCount++];
entry.questID    = questID;
entry.objective  = 0;  // first objective
entry.progress   = 0;
entry.required   = static_cast<int32_t>(
data->objectives.empty() ? 1 : data->objectives[0].requiredCount);
entry.isComplete = false;
entry.isFailed   = false;

### Clamp progress so it never exceeds the required amount.

**Source:** [`src/game/systems/QuestSystem.cpp`](src/game/systems/QuestSystem.cpp#L110) (line 110)

This prevents displaying "4/3 goblins killed" in the UI.
if (entry->progress > entry->required) entry->progress = entry->required;

### We match objective type AND the specific enemy type.

**Source:** [`src/game/systems/QuestSystem.cpp`](src/game/systems/QuestSystem.cpp#L271) (line 271)

if (obj.type == QuestObjectiveType::KILL_ENEMY &&
obj.targetID == enemyDataID)
{
UpdateObjective(player, e.questID, e.objective, 1);
}
}
}

### Quest System Architecture

**Source:** [`src/game/systems/QuestSystem.hpp`](src/game/systems/QuestSystem.hpp#L6) (line 6)

============================================================================

Quest systems are fundamentally a STATE MACHINE with DATABASE LOOKUP.

─── Quest State Machine ────────────────────────────────────────────────────

Each quest in the QuestComponent cycles through these states:

  NOT_ACCEPTED → [AcceptQuest] → IN_PROGRESS
  IN_PROGRESS  → [CompleteQuest] → COMPLETED
  IN_PROGRESS  → [FailQuest]     → FAILED

"In progress" itself contains an OBJECTIVE sub-machine:

  objective[0] IN_PROGRESS → objective[0] COMPLETE → objective[1] IN_PROGRESS → …

─── Data vs Logic ──────────────────────────────────────────────────────────

QuestData (in GameData.hpp) is the STATIC definition of a quest:
  - What are the objectives?
  - What are the rewards?
  - What are the prerequisites?

QuestEntry (in ECS.hpp / QuestComponent) is the RUNTIME state:
  - Which objective is the player on?
  - How much progress has been made?
  - Is the quest completed or failed?

The QuestSystem bridges the two: it reads static data to validate actions,
and writes to the runtime QuestComponent to record progress.

─── Event-Driven Objective Tracking ────────────────────────────────────────

Rather than polling ("does the player have 3 goblins killed this frame?"),
other systems call the QuestSystem's callback methods:

  CombatSystem calls OnEnemyKilled() when an enemy dies.
  InventorySystem calls OnItemCollected() when items are picked up.
  Zone calls OnLocationReached() when the player enters a trigger zone.

This is the Observer pattern in reverse: the QuestSystem RECEIVES events
rather than publishing them.  It then publishes QuestEvents itself to
notify the UI.

─── Prerequisite System ────────────────────────────────────────────────────

QuestData::prereqQuestIDs lists quests that must be completed before this
one can be accepted.  CanAcceptQuest() iterates this list and checks the
player's QuestComponent for COMPLETED entries.  This enables branching
narrative: quest chains, side-quests gated by story progress, etc.

@author  Educational Game Engine Project
@version 1.0
@date    2024

C++ Standard: C++17

### Structured Progress Tracking

**Source:** [`src/game/systems/QuestSystem.hpp`](src/game/systems/QuestSystem.hpp#L151) (line 151)

─────────────────────────────────────────────
Rather than storing progress per-objective in the QuestComponent
(which would require a 2D array), we advance a single `objective`
counter in QuestEntry.  When objective == objectives.size(), the
quest is complete.  Individual objective progress is tracked in
QuestEntry::progress against QuestEntry::required.

### CurrencyComponent is the Single Source of Truth for Gil.

**Source:** [`src/game/systems/ShopSystem.cpp`](src/game/systems/ShopSystem.cpp#L102) (line 102)

InventoryComponent no longer holds a gil field.  All Gil checks and
mutations go through CurrencyComponent exclusively.
if (!m_world->HasComponent<CurrencyComponent>(player)) {
LOG_WARN("BuyItem: player has no CurrencyComponent.");
if (m_uiBus) {
UIEvent ev;
ev.type = UIEvent::Type::SHOW_NOTIFICATION;
ev.text = "Not enough Gil!";
m_uiBus->Publish(ev);
}
return false;
}

### Shop System Design

**Source:** [`src/game/systems/ShopSystem.hpp`](src/game/systems/ShopSystem.hpp#L6) (line 6)

============================================================================

A shop system has three main responsibilities:

 1. CATALOGUE  — What items does this vendor sell?  At what price?
 2. TRANSACTION — Validate the player's funds, move the item.
 3. UI FEEDBACK — Tell the player what happened.

─── Price Calculation ───────────────────────────────────────────────────────

Buy price:  ItemData::price × ShopData::buyMultiplier
Sell price: ItemData::price × ShopData::sellMultiplier (always ≤ buy price)

Using multipliers instead of fixed prices means:
  - A "black market" shop has buyMultiplier 2.0 (double price).
  - A "guild friend" vendor has buyMultiplier 0.8 (20% discount).
  - All prices update automatically when you change the base item price.

─── State Machine ───────────────────────────────────────────────────────────

The ShopSystem tracks whether a shop is currently "open" via `m_isOpen`
and `m_activeShopID`.  Only one shop can be open at a time (the player
can only talk to one vendor at once).

  CLOSED → [OpenShop]  → OPEN
  OPEN   → [CloseShop] → CLOSED
  OPEN   → [BuyItem]   → OPEN  (transaction, stays open)
  OPEN   → [SellItem]  → OPEN

─── Key Items ───────────────────────────────────────────────────────────────

Items with ItemData::isKeyItem == true cannot be sold.  Key items are
plot-critical (e.g. "Crystal Key", "Prince's Ring") and should never
leave the player's inventory.  SellItem() checks this flag.

─── Gil Integration ─────────────────────────────────────────────────────────

Gil is stored in two places:
  - InventoryComponent::gil  (quick access, kept in sync)
  - CurrencyComponent::gil   (authoritative, 64-bit to avoid overflow)

Transactions use CurrencyComponent::SpendGil() and EarnGil() to ensure
correct overflow handling.  The InventoryComponent::gil is updated as
a cache for the UI.

@author  Educational Game Engine Project
@version 1.0
@date    2024

C++ Standard: C++17

### Transactional Validation

**Source:** [`src/game/systems/ShopSystem.hpp`](src/game/systems/ShopSystem.hpp#L156) (line 156)

──────────────────────────────────────────
We check ALL preconditions BEFORE making any changes.
This prevents partial state: the player cannot lose Gil and then
fail to receive the item because their inventory is full.
Always validate first, then commit.

### Returning by Value vs Reference

**Source:** [`src/game/systems/ShopSystem.hpp`](src/game/systems/ShopSystem.hpp#L208) (line 208)

──────────────────────────────────────────────────
We return by VALUE here (copying the vector) rather than a const
reference to ShopData's internal vector.  This is safer: the caller
can store the result without worrying about the ShopData being
invalidated.  The copy is small (typically 5–20 item IDs).

### TIME_SCALE converts real seconds to game seconds.

**Source:** [`src/game/systems/WeatherSystem.cpp`](src/game/systems/WeatherSystem.cpp#L36) (line 36)

With TIME_SCALE = 60, 1 real second = 1 game minute.
totalGameSeconds += dt * TIME_SCALE;

### We map 24-hour clock to 7 named periods.

**Source:** [`src/game/systems/WeatherSystem.cpp`](src/game/systems/WeatherSystem.cpp#L63) (line 63)

Thresholds mirror when FF15 characters comment on the time.
if (hour >= 5.0f  && hour < 8.0f)   return TimeOfDay::DAWN;
if (hour >= 8.0f  && hour < 12.0f)  return TimeOfDay::MORNING;
if (hour >= 12.0f && hour < 15.0f)  return TimeOfDay::NOON;
if (hour >= 15.0f && hour < 18.0f)  return TimeOfDay::AFTERNOON;
if (hour >= 18.0f && hour < 22.0f)  return TimeOfDay::EVENING;
if (hour >= 22.0f && hour < 24.0f)  return TimeOfDay::NIGHT;
0:00–5:00
return TimeOfDay::MIDNIGHT;
}

### We convert hours to game-seconds and add directly.

**Source:** [`src/game/systems/WeatherSystem.cpp`](src/game/systems/WeatherSystem.cpp#L120) (line 120)

The next Update() will detect the period change and broadcast the event.
totalGameSeconds += hours * 3600.0f;
LOG_INFO("Time advanced by " + std::to_string(hours) + " hours. " +
"New time: " + GetTimeString());
}

### Probabilistic weather state machine.

**Source:** [`src/game/systems/WeatherSystem.cpp`](src/game/systems/WeatherSystem.cpp#L144) (line 144)

Each weather type has different transition probabilities.
We roll once per WEATHER_CHECK_INTERVAL seconds.
std::uniform_real_distribution<float> dist(0.0f, 1.0f);
float roll = dist(m_rng);

### Time and Weather Systems

**Source:** [`src/game/systems/WeatherSystem.hpp`](src/game/systems/WeatherSystem.hpp#L6) (line 6)

============================================================================

An in-game clock serves multiple gameplay purposes:

 1. ATMOSPHERE  — Lighting changes; dawn is orange, midnight is deep blue.
 2. ENEMY SPAWNS — Nocturnal enemies (daemons in FF15) appear at night.
 3. NPC SCHEDULES — Shops close at night; NPCs go home.
 4. QUEST TRIGGERS — "Meet at the fountain at noon."
 5. WEATHER EFFECTS — Rain reduces fire spell damage; fog reduces visibility.

─── Time Representation ────────────────────────────────────────────────────

We store game time as a single float: `totalGameSeconds`.

  Time 0.0 = 06:00 on Day 1 (dawn).
  A full day = 86,400 GAME seconds.

But 86,400 real seconds is 24 real hours — too slow.  We compress time:
  Real 1 minute = Game 1 hour  (60× compression)
  So a full game day = 24 real minutes.

Implementation:
  totalGameSeconds += dt × TIME_SCALE   (TIME_SCALE = 60.0f)

To find current game hour:
  float secondsToday = fmod(totalGameSeconds, SECONDS_PER_DAY);
  float gameHour     = 6.0f + secondsToday / 3600.0f;  // starts at 6am

─── TimeOfDay Periods ───────────────────────────────────────────────────────

We divide the 24-hour cycle into named periods (from Types.hpp):

  DAWN       06:00 - 07:59
  MORNING    08:00 - 11:59
  NOON       12:00 - 13:59
  AFTERNOON  14:00 - 17:59
  EVENING    18:00 - 19:59
  NIGHT      20:00 - 23:59
  MIDNIGHT   00:00 - 05:59

A WorldEvent::TIME_CHANGED is published when the period transitions.

─── Weather State Machine ───────────────────────────────────────────────────

Weather transitions are probabilistic and time-of-day dependent:

  - CLEAR:  Can transition to CLOUDY (20%), RAIN (5%), FOG (5%) per hour.
  - CLOUDY: Can transition to RAIN (30%), CLEAR (15%), STORM (10%) per hour.
  - RAIN:  Can transition to CLEAR (10%), STORM (20%) per hour.
  - STORM:  Can transition to RAIN (40%) per hour.
  - FOG:  Can transition to CLEAR (25%) per hour.
  - SNOWY:  Appears only in high-altitude zones (set via SetWeather directly).

Weather changes also fire WorldEvent::WEATHER_CHANGED.

─── Effect on Gameplay ──────────────────────────────────────────────────────

The WeatherSystem itself only tracks state.  Other systems RESPOND to the
WorldEvent broadcast:
  - Renderer: adjusts tile colour tinting.
  - CombatSystem: applies weather damage multipliers.
  - AISystem: increases enemy aggro range in fog (limited visibility).
  - SpawnSystem: activates daemon spawns at night.

This demonstrates the power of the event-bus pattern for decoupled reactions
to a single state change.

@author  Educational Game Engine Project
@version 1.0
@date    2024

C++ Standard: C++17

### Floating Point Accumulation

**Source:** [`src/game/systems/WeatherSystem.hpp`](src/game/systems/WeatherSystem.hpp#L167) (line 167)

─────────────────────────────────────────────
Adding `dt` to a float every frame accumulates small rounding errors.
For game clocks this is acceptable — a few milliseconds of drift per
day is invisible to players.  For financial or scientific simulations,
use double or a fixed-point integer representation.

### String Formatting in C++17

**Source:** [`src/game/systems/WeatherSystem.hpp`](src/game/systems/WeatherSystem.hpp#L195) (line 195)

────────────────────────────────────────────
We use std::to_string and manual zero-padding.  In C++20 you can use
std::format("{:02d}:{:02d}", hours, minutes).  For C++17 compatibility
we build the string manually with conditional zero-padding.

### Probabilistic State Machines

**Source:** [`src/game/systems/WeatherSystem.hpp`](src/game/systems/WeatherSystem.hpp#L267) (line 267)

──────────────────────────────────────────────
Instead of hard rules ("rain always follows clouds"), we use
probabilities.  This gives organic-feeling variation:

  roll = uniform_real(0, 1)
  if roll < 0.20: transition to CLOUDY
  elif roll < 0.25: transition to RAIN
  elif roll < 0.30: transition to FOG
  else: stay CLEAR

Adjusting the probabilities is the core of weather "feel" tuning.

---

## game/world

### Separating Interface from Implementation

**Source:** [`src/game/world/TileMap.cpp`](src/game/world/TileMap.cpp#L7) (line 7)

============================================================================

TileMap.hpp declares WHAT the TileMap class can do (its public interface).
TileMap.cpp defines HOW it does it (the implementation).

This separation benefits:
  1. COMPILE TIMES: Files that include TileMap.hpp do not need to recompile
     when only the .cpp changes (as long as the .hpp interface is unchanged).
  2. ENCAPSULATION: Callers cannot see private helpers like ShadowCastOctant
     or GenerateDungeonImpl.
  3. READABILITY: Users read the clean .hpp; implementors dig into the .cpp.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024

### Vector Initialisation

**Source:** [`src/game/world/TileMap.cpp`](src/game/world/TileMap.cpp#L49) (line 49)

──────────────────────────────────────
`std::vector<Tile>(w * h, Tile{})` allocates a contiguous block of
(w * h) default-constructed Tile objects in one allocation.
This is more efficient than push_back()-ing each tile individually:
  • Only one heap allocation.
  • The compiler can vectorise the zero-initialisation.
  • The resulting memory is contiguous — cache-friendly.

### Assertions in Debug vs Release

**Source:** [`src/game/world/TileMap.cpp`](src/game/world/TileMap.cpp#L77) (line 77)

─────────────────────────────────────────────────
`assert(condition)` expands to a runtime check in debug builds and
compiles to NOTHING in release builds (when NDEBUG is defined).
This means:
  • In debug: invalid coordinates trigger a clear, informative crash.
  • In release: no overhead from bounds checking (trust callers).

If you need bounds checking in release too (e.g. for modding support),
use a non-assert check that returns m_outOfBoundsTile.

### Single Point of Update

**Source:** [`src/game/world/TileMap.cpp`](src/game/world/TileMap.cpp#L108) (line 108)

────────────────────────────────────────
Always set tile types through SetTile(), never by writing to tile.type
directly.  This ensures isPassable stays consistent with the type.

If callers wrote:
  GetTile(x,y).type = TileType::FLOOR;  // BAD: isPassable not updated!
then pathfinding would still treat the tile as impassable.

SetTile() is the "single point of update" that keeps everything in sync.

### Batch Reset

**Source:** [`src/game/world/TileMap.cpp`](src/game/world/TileMap.cpp#L153) (line 153)

────────────────────────────
Rather than iterating and setting each flag individually (which is fine),
we show a range-based for loop over the vector.  The compiler will
auto-vectorise this tight loop (set bool to false repeatedly in a
contiguous array) — no manual SIMD needed for educational code.

### Slope Convention

**Source:** [`src/game/world/TileMap.cpp`](src/game/world/TileMap.cpp#L250) (line 250)

──────────────────────────────────
"Slope" here is the tangent of the angle from the viewer to the tile edge.
A slope of 1.0 means 45° (the maximum extent of one octant).
A slope of 0.0 means 0° (the central axis of the octant).

The visible wedge narrows as walls cast shadows.  When startSlope <=
endSlope, the wedge has been completely shadowed and we can return.

The function processes tiles from (row, -row) to (row, 0) in octant-local
coordinates, which corresponds to the full width of that scan row.

### Bidirectional Mapping

**Source:** [`src/game/world/TileMap.cpp`](src/game/world/TileMap.cpp#L357) (line 357)

──────────────────────────────────────
Serialisation requires a bijection: TileType → char (write) and
char → TileType (read).  The two switch statements implement both
directions.  Keeping them adjacent makes it easy to verify they are
consistent (every case in one matches a case in the other).

If a character is unrecognised (e.g. from a newer version of the engine),
we default to FLOOR so the map is still playable.  A production engine
would log a warning and potentially refuse to load.

### std::ostringstream

**Source:** [`src/game/world/TileMap.cpp`](src/game/world/TileMap.cpp#L395) (line 395)

────────────────────────────────────
std::ostringstream accumulates string output like `std::cout` but
writes to an in-memory string buffer.  Calling `.str()` retrieves the
accumulated string.

Alternative: build a `std::string` with `+=` for each character.
`+=` with a single char is O(1) amortised (no reallocation if capacity
allows).  For large maps, pre-reserving (`result.reserve(...)`) is faster
than using ostringstream.

We choose ostringstream here to teach the pattern; performance-critical
code might use the reserve-then-append approach instead.

### std::istringstream for Parsing

**Source:** [`src/game/world/TileMap.cpp`](src/game/world/TileMap.cpp#L434) (line 434)

─────────────────────────────────────────────────
`std::istringstream` wraps a string and presents it as an input stream.
We use:
  ss >> width >> height;    to read the two integers on the header line.
  std::getline(ss, line);   to read each subsequent row character by character.

The `ss.ignore()` call after reading integers skips the newline that
`>>` leaves unread in the stream buffer.

### Error Tolerance

**Source:** [`src/game/world/TileMap.cpp`](src/game/world/TileMap.cpp#L444) (line 444)

─────────────────────────────────
If the string is malformed (e.g. wrong number of rows), we log a warning
and stop rather than crashing.  Partial maps are better than crashes.

### Two-Pass Room Generation

**Source:** [`src/game/world/TileMap.cpp`](src/game/world/TileMap.cpp#L492) (line 492)

──────────────────────────────────────────
We use two passes:
  Pass 1: fill every tile with FLOOR (faster than selective setting).
  Pass 2: overwrite the border tiles with WALL.

Pass 1 is O(w * h); Pass 2 is O(2w + 2h).  Together still O(w * h).
An alternative is a single pass with an `if (border) WALL else FLOOR`
branch, but two passes are clearer and the branch-free approach can
actually be slower due to branch misprediction on small maps.

### std::random_device

**Source:** [`src/game/world/TileMap.cpp`](src/game/world/TileMap.cpp#L536) (line 536)

─────────────────────────────────────
`std::random_device` is a non-deterministic entropy source backed by
the OS (e.g. /dev/urandom on Linux, CryptGenRandom on Windows).
`rd()` returns a single random 32-bit value suitable for seeding a PRNG.

Note: on some platforms (e.g. MinGW on Windows), `std::random_device`
may be deterministic.  Using a time-based seed as a fallback is
common in portable code.

### Overlap Check

**Source:** [`src/game/world/TileMap.cpp`](src/game/world/TileMap.cpp#L577) (line 577)

──────────────────────────────
The overlap test uses the AABB (Axis-Aligned Bounding Box) intersection
test, augmented by a 1-tile padding to prevent rooms touching.  Two
rectangles (r1 and r2) do NOT overlap when:
  r1.right  <= r2.left    OR
  r2.right  <= r1.left    OR
  r1.bottom <= r2.top     OR
  r2.bottom <= r1.top
With padding=1, extend each rectangle by 1 in all directions before
checking.

### L-Shaped Corridors

**Source:** [`src/game/world/TileMap.cpp`](src/game/world/TileMap.cpp#L589) (line 589)

─────────────────────────────────────
An L-corridor is two perpendicular straight corridors meeting at a corner.
For centres (cx1,cy1) and (cx2,cy2):
  Option A (H then V):
    1. Walk from cx1 to cx2 along row cy1 (horizontal leg).
    2. Walk from cy1 to cy2 along column cx2 (vertical leg).
  Option B (V then H):
    1. Walk from cy1 to cy2 along column cx1.
    2. Walk from cx1 to cx2 along row cy2.
The random coin flip between options gives varied junction positions.

────────────────────────────────────────────────────────────────────────────

### Tile Maps in 2D Games

**Source:** [`src/game/world/TileMap.hpp`](src/game/world/TileMap.hpp#L6) (line 6)

============================================================================

A tile map divides the world into a regular grid of equal-sized cells
(tiles).  This is the foundation of countless games from the NES era
(Zelda, Final Fantasy 1–6) through modern roguelikes (Caves of Qud,
Dwarf Fortress).

WHY TILE MAPS?
──────────────
1. Memory efficiency:  A 40×40 map is just 1600 Tile structs instead of
   1600 heap-allocated objects.  A contiguous flat array is cache-friendly.
2. Grid math:  Pathfinding (A*), FOV, and dungeon generation are much
   simpler on a grid than on arbitrary meshes.
3. Indexed access:  Any tile can be reached in O(1) by its (x, y)
   coordinate:  `tile = m_tiles[y * width + x]`.
4. Serialisation:  A tile map serialises trivially — write each tile's
   type as a character, one row per line.

COORDINATE SYSTEM
─────────────────
This engine uses the convention:
  (0, 0)  = top-left tile
  +X      = rightward (East)
  +Y      = downward  (South)

This matches screen-space coordinates and makes row-major array indexing
( index = y * width + x ) natural.

============================================================================

### Field of View (FOV)

**Source:** [`src/game/world/TileMap.hpp`](src/game/world/TileMap.hpp#L36) (line 36)

============================================================================

In dungeon games the player can only see tiles within a certain radius
that are not blocked by walls.  This is called the "Field of View" (FOV)
or "Line of Sight" (LOS) computation.

NAIVE APPROACH — Raycasting
────────────────────────────
Cast a ray from the player to every tile in the radius.  If the ray
doesn't hit a wall, the tile is visible.  Simple, but:
  • O(n²) rays, each of O(radius) steps → O(n² * radius) total.
  • Artifacts: some tiles incorrectly hidden, some incorrectly shown,
    depending on ray angle quantisation.

BETTER APPROACH — Recursive Shadowcasting (Björn Bergström, 2001)
──────────────────────────────────────────────────────────────────
Shadowcasting solves FOV by tracking "shadows" cast by opaque tiles.
Key ideas:
  1. Divide the visible area into 8 octants (45° wedges).
  2. For each octant, scan rows from the origin outward.
  3. Track the start and end "slope" of the currently lit region.
  4. When an opaque (wall) tile is encountered, it casts a shadow —
     everything behind it at steeper slopes is dark.
  5. Recursively cast light for the lit portion to the left of the wall.

Complexity: O(r²) — proportional to the visible area.
Quality:    Symmetric, no artifacts, handles thick walls correctly.

Reference:  http://www.roguebasin.com/index.php/FOV_using_recursive_shadowcasting

============================================================================

### Procedural Dungeon Generation

**Source:** [`src/game/world/TileMap.hpp`](src/game/world/TileMap.hpp#L68) (line 68)

============================================================================

Procedural generation creates levels algorithmically, providing infinite
variety without manual authoring.  Our algorithm:

ROOM-CORRIDOR ALGORITHM
───────────────────────
1. Fill the entire map with WALL tiles.
2. Attempt to place N randomly-sized rectangular rooms:
   a. Pick a random position and size.
   b. Check it doesn't overlap existing rooms (plus a 1-tile padding).
   c. If valid, carve out FLOOR tiles in the room.
3. For each new room after the first, connect it to the previous room
   with an L-shaped corridor:
   a. Randomly decide: horizontal-then-vertical, or vertical-then-horizontal.
   b. Carve FLOOR tiles along both legs of the L.
4. Place STAIRS_UP in the first room, STAIRS_DOWN in the last.

This produces dungeon layouts where all rooms are reachable from any other
room — a key requirement for playability.

Variations to explore:
  • BSP (Binary Space Partitioning) for more evenly distributed rooms.
  • Cellular automata for organic cave systems.
  • Wave Function Collapse for pattern-coherent levels.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024

C++ Standard: C++17

### Enum-Driven Tile Behaviour

**Source:** [`src/game/world/TileMap.hpp`](src/game/world/TileMap.hpp#L129) (line 129)

───────────────────────────────────────────
Rather than storing separate boolean fields for every possible tile
property (isWalkable, isTransparent, isInteractable, …), we use a single
TileType enum.  The engine derives all properties from this one value:

  IsPassable()  →  switch(type): WALL=false, WATER=false, else=true
  IsOpaque()    →  switch(type): WALL=true, else=false
  GetChar()     →  switch(type): FLOOR='.', WALL='#', …

This keeps the Tile struct small and makes adding new tile types easy:
add an enum value and update the switch statements — no structural changes.

### Enum Value Ranges

**Source:** [`src/game/world/TileMap.hpp`](src/game/world/TileMap.hpp#L142) (line 142)

───────────────────────────────────
Keeping enum values compact (no gaps) lets us use them as array indices,
e.g. for a lookup table of colours or characters.  Always start at 0.

### Terminal Rendering (Roguelike Style)

**Source:** [`src/game/world/TileMap.hpp`](src/game/world/TileMap.hpp#L174) (line 174)

──────────────────────────────────────────────────────
Even modern games sometimes use ASCII art for their maps during
development ("ASCII roguelike" style).  Single characters serve as
clear, compact tile representations that work in any terminal.
This function is the bridge between the TileType enum and the character
that the renderer draws on screen.

In a sprite-based renderer you would replace this with a function that
returns a texture atlas UV rectangle.

@param type  The TileType to look up.
@return      The ASCII character used to represent this tile.

### Colour Pairs in Terminal Rendering

**Source:** [`src/game/world/TileMap.hpp`](src/game/world/TileMap.hpp#L216) (line 216)

────────────────────────────────────────────────────
Text-mode UIs (ncurses, ANSI terminals) display each cell with an
independent foreground (text) and background colour.  A "colour pair"
encodes both at once.  The renderer uses `first` to colour the glyph
returned by GetTileChar(), and `second` to fill the cell background.

### Visual Design for Readability

**Source:** [`src/game/world/TileMap.hpp`](src/game/world/TileMap.hpp#L223) (line 223)

───────────────────────────────────────────────
Colour choices should convey meaning at a glance:
  • WALL = grey on dark grey  (opaque, solid)
  • WATER = white on blue     (bright waves on water)
  • GRASS = green on dark green
  • CHEST = yellow on black   (treasure! attention-grabbing)
  • SAVE_POINT = cyan on black (calm, restorative)

This uses the Color struct from Types.hpp.

### Separating Queries from Data

**Source:** [`src/game/world/TileMap.hpp`](src/game/world/TileMap.hpp#L278) (line 278)

──────────────────────────────────────────────
We store this logic as a free function rather than in the Tile struct
so that it is the single authoritative definition of "what blocks movement."
This avoids the tile passability flag getting out of sync with the type
(e.g. someone adds a new type but forgets to update IsPassable).

The Tile struct still caches the `isPassable` boolean for fast lookup
during FOV and pathfinding, but it is *initialised* using this function
inside SetTile().

@param type  TileType to check.
@return true if an entity can walk on this tile by default.

### Passability vs Opacity

**Source:** [`src/game/world/TileMap.hpp`](src/game/world/TileMap.hpp#L304) (line 304)

────────────────────────────────────────
Not all blocking tiles are opaque and vice versa:
  WATER:    impassable (blocks movement) but NOT opaque (you can see across it)
  FOREST:   passable (you can walk in) AND partially opaque (reduces FOV)
  WALL:     impassable AND opaque

For simplicity we treat WALL as the only fully opaque tile.  A more
sophisticated FOV could treat FOREST as partially transparent (reduces
view radius by 2, say) — this is left as an exercise for the student.

@param type  TileType to check.
@return true if this tile blocks line-of-sight.

### Small Structs and Cache Efficiency

**Source:** [`src/game/world/TileMap.hpp`](src/game/world/TileMap.hpp#L331) (line 331)

─────────────────────────────────────────────────────
Each Tile is kept as small as possible.  A 40×40 map has 1600 tiles;
a 200×200 map has 40,000.  Padding matters:

  Current layout (without padding):
    TileType    (uint8_t) = 1 byte
    bool × 3             = 3 bytes
    uint32_t             = 4 bytes
  Total:                   8 bytes → 2 cache lines for 16 tiles (64B each)

The 40,000-tile map fits in 320 KB — well within typical L2 cache (256–1024 KB).
A naive pointer-per-tile design would scatter tiles across the heap, causing
cache misses on every access.

### Explored vs Visible

**Source:** [`src/game/world/TileMap.hpp`](src/game/world/TileMap.hpp#L346) (line 346)

─────────────────────────────────────
Two booleans track different states:
  isVisible  = currently in the player's FOV (bright rendering).
  isExplored = was visible at some point in the past (dim/grey rendering).

This is the classic "fog of war" system:
  isVisible  && isExplored  → draw tile at full brightness
  !isVisible && isExplored  → draw tile dim/grey (remembered but unseen)
  !isVisible && !isExplored → draw as black (completely unknown)

### Tiles and Entities

**Source:** [`src/game/world/TileMap.hpp`](src/game/world/TileMap.hpp#L366) (line 366)

────────────────────────────────────
Storing an EntityID here lets us do O(1) "is this tile occupied?"
checks during pathfinding, collision, and AI.  The alternative —
scanning all entities every time — is O(n).

Kept as uint32_t instead of EntityID to avoid including ECS.hpp
in this header (which would create a circular dependency).  The
sentinel value 0 means "no entity" (entity IDs start from 0 in
our ECS but the tile starts unoccupied, not entity-0).
In practice, entity IDs are only stored here for non-player-
controlled entities (chests, doors, NPCs).

### Class Invariants

**Source:** [`src/game/world/TileMap.hpp`](src/game/world/TileMap.hpp#L391) (line 391)

─────────────────────────────────
A class invariant is a condition that is always true for a valid object.
TileMap's invariant:
  m_tiles.size() == m_width * m_height

Every method either:
  (a) preserves this invariant, or
  (b) deliberately re-establishes it (e.g. Resize()).

The constructor establishes the invariant.  Methods like SetTile() and
GetTile() assert it via bounds checking.

### RAII (Resource Acquisition Is Initialisation)

**Source:** [`src/game/world/TileMap.hpp`](src/game/world/TileMap.hpp#L404) (line 404)

──────────────────────────────────────────────────────────────
`std::vector` manages the tile array's lifetime automatically (RAII).
When TileMap is destroyed, the vector's destructor frees the memory.
No `new` / `delete` needed — and no possibility of leaks.

### Default Arguments

**Source:** [`src/game/world/TileMap.hpp`](src/game/world/TileMap.hpp#L425) (line 425)

────────────────────────────────────
Providing default arguments (=0) lets callers write:
  TileMap map;           // 0×0, resize later
  TileMap map(40, 40);   // 40×40 starting map
Without needing two separate constructor overloads.

### Bounds Checking

**Source:** [`src/game/world/TileMap.hpp`](src/game/world/TileMap.hpp#L457) (line 457)

────────────────────────────────
Accessing `m_tiles[y * m_width + x]` with invalid coordinates is
undefined behaviour (UB) in C++ — it could read garbage memory or
crash.  We assert bounds in debug builds.

In release builds, asserts are typically disabled for performance.
Consider using a safe version that returns a "null tile" sentinel
for release builds if the map is player-modified at runtime.

### Derived State (isPassable)

**Source:** [`src/game/world/TileMap.hpp`](src/game/world/TileMap.hpp#L479) (line 479)

────────────────────────────────────────────
`isPassable` is *derived* from `type`, but we cache it in the tile
to avoid calling TileTypeIsPassable() on every pathfinding step.
SetTile() re-derives and caches it whenever the type changes.
This is the "dirty flag" + "cached computed value" pattern.

### The isExplored Flag

**Source:** [`src/game/world/TileMap.hpp`](src/game/world/TileMap.hpp#L526) (line 526)

─────────────────────────────────────
`isExplored` is never reset to false during normal play — it tracks
the player's total exploration history.  On a new dungeon load it
starts false everywhere.  This implements the classic "fog of war":
  Dark (unexplored) → Dim (explored but not currently visible) → Bright (visible)

### Serialisation Formats

**Source:** [`src/game/world/TileMap.hpp`](src/game/world/TileMap.hpp#L564) (line 564)

───────────────────────────────────────
We use human-readable text so students can inspect and hand-edit maps
in any text editor.  For production games you would use:
  • Binary format (more compact, faster read, not human-readable).
  • JSON/XML (human-readable, self-describing, but verbose).
  • A custom binary format with versioning headers.

The tile type <→ character mapping is defined in GetTileChar() /
CharToTileType(), making the encoding easy to look up.

@return A std::string containing the entire serialised map.

### Simple Room Generation

**Source:** [`src/game/world/TileMap.hpp`](src/game/world/TileMap.hpp#L604) (line 604)

────────────────────────────────────────
Even a "trivial" empty room has a useful border-wall loop pattern:
  for x in [0, w): SetTile(x, 0, WALL);    top row
  for x in [0, w): SetTile(x, h-1, WALL);  bottom row
  for y in [0, h): SetTile(0, y, WALL);     left column
  for y in [0, h): SetTile(w-1, y, WALL);   right column
This teaches: nested loops, boundary conditions, and the distinction
between "whole map" and "border only" iteration.

### Procedural Generation & Randomness

**Source:** [`src/game/world/TileMap.hpp`](src/game/world/TileMap.hpp#L622) (line 622)

─────────────────────────────────────────────────────
We use `std::mt19937` (Mersenne Twister), a high-quality pseudo-random
number generator seeded from `std::random_device`.

  std::mt19937 rng(std::random_device{}());

WHY NOT `std::rand`?
  • std::rand produces poor-quality sequences (short period, biased).
  • std::mt19937 has a period of 2^19937 − 1 — effectively infinite.
  • Seeding with std::random_device provides OS-level entropy.

SEEDED vs UNSEEDED:
  • A fixed seed (e.g. `rng(42)`) produces the same dungeon every run.
    Useful for testing and "seed-sharing" (players sharing dungeon codes).
  • A random seed (std::random_device) produces unique dungeons.
  • Good engines expose a "seed" parameter so both modes are possible.

### Reproducible Generation

**Source:** [`src/game/world/TileMap.hpp`](src/game/world/TileMap.hpp#L647) (line 647)

─────────────────────────────────────────
Reproducible generation is crucial for:
  • Speedrunning (players can practice a specific layout).
  • Multiplayer consistency (all clients use the same seed).
  • Bug reproduction ("the crash always happens on seed 12345").
  • Game design iteration (inspect the same map after changing gen params).

### Row-Major vs Column-Major

**Source:** [`src/game/world/TileMap.hpp`](src/game/world/TileMap.hpp#L685) (line 685)

──────────────────────────────────────────
We store tiles in *row-major* order: row 0 first, then row 1, etc.
index = y * width + x

Column-major would be: index = x * height + y

Row-major is conventional for 2D grids in C++ and matches the way
humans read (left-to-right, top-to-bottom).  It is also optimal
for iterating over a full row in sequence (sequential memory access).

@param x  Column index.
@param y  Row index.
@return   Flat array index.

### Octant Transforms

**Source:** [`src/game/world/TileMap.hpp`](src/game/world/TileMap.hpp#L716) (line 716)

────────────────────────────────────
The 8 octants of the circle are symmetric.  Instead of writing 8
separate functions, we use a 2×2 transformation matrix {xx,xy,yx,yy}
that remaps "octant-local" coordinates to world coordinates.

For octant 0 (east-southeast): xx=1, xy=0, yx=0, yy=1
  world_dx = dx*xx + dy*xy = dx*1 + dy*0 = dx
  world_dy = dx*yx + dy*yy = dx*0 + dy*1 = dy

For octant 4 (west-northwest): xx=-1, xy=0, yx=0, yy=-1
  world_dx = -dx
  world_dy = -dy

The 8 sets of {xx,xy,yx,yy} are hard-coded in a static table inside
ComputeFOV().  Each call to ShadowCastOctant() uses one row.

### We define zones as data, not code. Each zone has an

**Source:** [`src/game/world/WorldMap.cpp`](src/game/world/WorldMap.cpp#L21) (line 21)

ID, a display name, a position on the world-map screen, and a list of
connected zone IDs (adjacency list).

Zone IDs match GameData.hpp ZoneData::id entries.

### World Map as a Graph

**Source:** [`src/game/world/WorldMap.hpp`](src/game/world/WorldMap.hpp#L6) (line 6)

============================================================================

In many RPGs the "world map" is not a free-roam 3D space but a simplified
GRAPH: nodes are named locations (towns, dungeons, outposts) and edges are
travel routes between them.

This design has several advantages:
  1. Clear scope — each Zone loads/unloads independently.
  2. Fast travel — "teleport to node" is trivial.
  3. Quest gating — lock edges until the player completes prerequisites.

Data structure choice: std::unordered_map<uint32_t, WorldNode> gives
O(1) average lookup by zone ID, which is common (every player movement
checks the map).

@author  Educational Game Engine Project
@version 1.0

### Graph Node vs. Zone

**Source:** [`src/game/world/WorldMap.hpp`](src/game/world/WorldMap.hpp#L45) (line 45)

─────────────────────────────────────
A WorldNode is a LIGHTWEIGHT DESCRIPTOR — it holds the zone ID and a
2D position for drawing on the map screen, but NOT the full TileMap.
The TileMap lives inside the Zone object (loaded on demand).

Keeping the descriptor light means ALL nodes can be in memory at once
(for the map screen UI) without loading every TileMap simultaneously.

### Data-Driven World Definition

**Source:** [`src/game/world/WorldMap.hpp`](src/game/world/WorldMap.hpp#L101) (line 101)

──────────────────────────────────────────────
Zone metadata comes from GameDatabase::GetZones() (conceptually — here
we define the graph inline).  In a shipped game this would be loaded
from a data file or database, allowing level designers to add zones
without recompiling C++.

### Adjacency List Representation

**Source:** [`src/game/world/WorldMap.hpp`](src/game/world/WorldMap.hpp#L123) (line 123)

─────────────────────────────────────────────
Each WorldNode stores connectedZoneIDs (an adjacency list).
This is more memory-efficient than an adjacency matrix for sparse
graphs (most zones connect to 2–4 others, not all others).

### Simple Text Serialization

**Source:** [`src/game/world/WorldMap.hpp`](src/game/world/WorldMap.hpp#L165) (line 165)

──────────────────────────────────────────
We use plain text (not binary) for portability and debuggability.
Players and modders can open save files in a text editor.  For large
amounts of data, binary formats (FlatBuffers, protobuf) are faster.

### Implementation File Structure

**Source:** [`src/game/world/Zone.cpp`](src/game/world/Zone.cpp#L6) (line 6)

============================================================================

This file implements the methods declared in Zone.hpp.  The organisation:

  1. Includes
  2. Static member initialisation  (s_emptyString)
  3. Zone::Load()                  (zone setup)
  4. Zone::Unload()                (cleanup)
  5. Zone::Update()                (per-frame logic)
  6. Zone::SpawnEnemies()          (batch spawn)
  7. Zone::SpawnNPCs()             (NPC creation)
  8. Zone::SpawnOneEnemy()         (single-entity creation)
  9. Zone::AddSpawnPoint()
 10. Zone::GetName()
 11. Zone::RegisterDefaultSpawnPoints()
 12. Zone::FindPlayerSpawn()
 13. Zone::PruneDeadEntities()

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024

### static_assert

**Source:** [`src/game/world/Zone.cpp`](src/game/world/Zone.cpp#L55) (line 55)

──────────────────────────────
`static_assert` evaluates a condition at compile time and fails the build
with a friendly message if the condition is false.
Here we verify that uint32_t and EntityID are the same size so storing
EntityIDs in uint32_t vectors is safe.
static_assert(sizeof(uint32_t) == sizeof(EntityID),
"Zone.cpp: uint32_t and EntityID must be the same size for safe casting.");

### Static Member Variables with Non-trivial Types

**Source:** [`src/game/world/Zone.cpp`](src/game/world/Zone.cpp#L69) (line 69)

─────────────────────────────────────────────────────────────────
Static data members of class types (like std::string) must be defined in
exactly ONE translation unit (.cpp file).  Writing `= ""` here provides
both the definition and its initial value.  Without this line, the linker
would report an "undefined reference to Zone::s_emptyString" error.
const std::string Zone::s_emptyString = "";

### Defensive Programming

**Source:** [`src/game/world/Zone.cpp`](src/game/world/Zone.cpp#L85) (line 85)

──────────────────────────────────────
We validate inputs at the start of Load() and return false on any failure.
This is "defensive programming": assume inputs might be invalid and check
them explicitly rather than trusting callers.

Benefits:
  • Clear error messages in the log (LOG_WARN identifies the problem).
  • Zone remains in a valid "unloaded" state on failure.
  • Callers can detect failure via the return value.

### Entity Cleanup Order

**Source:** [`src/game/world/Zone.cpp`](src/game/world/Zone.cpp#L158) (line 158)

──────────────────────────────────────
We destroy entities before clearing the ID lists, so we don't lose track
of which entities need cleanup.  The order:
  1. Destroy all enemy entities.
  2. Destroy all NPC entities.
  3. Destroy all chest entities.
  4. Clear all ID lists.
  5. Clear spawn points.
  6. Reset flags and pointers.

Using separate loops for each category keeps the code readable and
makes it easy to skip cleanup of one category if needed.

### Periodic vs Every-Frame Updates

**Source:** [`src/game/world/Zone.cpp`](src/game/world/Zone.cpp#L226) (line 226)

──────────────────────────────────────────────────
We use `m_updateTimer` to perform heavy operations (like scanning all
enemy entities for death) only every HEAVY_UPDATE_INTERVAL seconds
rather than every frame.

This is a common optimisation pattern:
  Light operations  → every frame   (spawn timer countdown)
  Heavy operations  → every N seconds (entity validity scan)

For an educational engine with small zones, even heavy operations are
fast.  But the pattern is worth demonstrating: at production scale with
100 zones each with 50 enemies, every-frame scans add up.

### Batch Spawn vs On-Demand Spawn

**Source:** [`src/game/world/Zone.cpp`](src/game/world/Zone.cpp#L293) (line 293)

─────────────────────────────────────────────────
SpawnEnemies() spawns ALL enemies at once when the zone loads.
An alternative is "lazy spawn": only spawn enemies the player is near
(within some activation radius).  Lazy spawn reduces ECS entity count
but requires distance checks every frame.

For this educational engine, batch spawn on load is simpler and the
entity count is small.  A production open-world game would use lazy spawn.

### NPC Entity Components

**Source:** [`src/game/world/Zone.cpp`](src/game/world/Zone.cpp#L334) (line 334)

──────────────────────────────────────
NPCs require fewer components than enemies:
  • TransformComponent  — position on the map.
  • NameComponent       — display name for the dialogue system.
  • RenderComponent     — sprite to draw.
  • DialogueComponent   — conversation data (tree ID).

NPCs do NOT have AIComponent (they are not autonomous) or
CombatComponent (they do not fight).  This is ECS composition in action:
NPCs and enemies are both "entities", but they have different
component sets and therefore different behaviours.

### Component Assembly from Data

**Source:** [`src/game/world/Zone.cpp`](src/game/world/Zone.cpp#L438) (line 438)

──────────────────────────────────────────────
This function demonstrates how EnemyData drives component initialisation.
The pattern is:
  1. Look up the data record.
  2. Create a blank ECS entity.
  3. Build each component from the data fields.
  4. Attach the component to the entity.

Every component is initialised to a known state derived from the data.
There are no "magic defaults" hidden deep in component structs — the intent
is explicit here where it can be read and understood.

### ECS World Registration

**Source:** [`src/game/world/Zone.cpp`](src/game/world/Zone.cpp#L451) (line 451)

────────────────────────────────────────
`World::AddComponent<T>()` will assert if T has not been registered via
`World::RegisterComponent<T>()`.  Registration is performed once at
engine startup.  If you encounter a registration assert here, check that
all component types used below are registered in the engine init code.

### Spawn Point Distribution Strategy

**Source:** [`src/game/world/Zone.cpp`](src/game/world/Zone.cpp#L647) (line 647)

──────────────────────────────────────────────────
We use a simple grid-walk approach to place spawn points on passable tiles:
  • Divide the map into a grid of cells (spawnCellW × spawnCellH).
  • Place one spawn per enemy ID, cycling through cells.
  • Within each cell, pick the first passable non-special tile.

This produces a reasonably spread-out distribution without requiring
designer input.  A production game would load spawn positions from a
data file instead.

Special tiles (STAIRS, CHEST, SAVE_POINT) are excluded from spawn
positions to avoid blocking critical interactions.

### Tile Searching Strategy

**Source:** [`src/game/world/Zone.cpp`](src/game/world/Zone.cpp#L754) (line 754)

──────────────────────────────────────────
We search for tile types in priority order:
  Priority 1: STAIRS_UP  (conventional entrance marker in dungeons)
  Priority 2: First passable FLOOR tile in the interior (safe fallback)

The search scans row by row from top-left, which biases the spawn
toward the top-left of the map.  For outdoor zones this is fine;
for dungeons the STAIRS_UP tile is usually near the centre of the
first generated room.

### The Erase-Remove Idiom

**Source:** [`src/game/world/Zone.cpp`](src/game/world/Zone.cpp#L812) (line 812)

────────────────────────────────────────
In C++, removing elements from a std::vector while iterating is tricky.
The standard solution is the "erase-remove idiom":

  v.erase(std::remove_if(v.begin(), v.end(), predicate), v.end());

std::remove_if(begin, end, pred):
  Moves all elements for which pred returns TRUE to the END of the range
  and returns an iterator to the first "removed" element.
  (It does NOT actually remove them from the vector.)

v.erase(first_removed, v.end()):
  Actually erases the "removed" elements.

Combined, these two calls efficiently remove all matching elements in
O(n) time.

WHY NOT ERASE INSIDE THE LOOP?
  Calling v.erase(it) inside a range-for loop invalidates 'it', causing
  undefined behaviour.  The erase-remove idiom avoids this completely.

C++20 provides `std::erase_if(v, pred)` as a cleaner alternative.
We show the C++17 idiom here for compatibility.

### Zone Architecture

**Source:** [`src/game/world/Zone.hpp`](src/game/world/Zone.hpp#L6) (line 6)

============================================================================

A "Zone" (also called "Area", "Level", or "Scene" in different engines)
is the fundamental unit of game world organisation.  Everything the player
can explore at one time lives in one Zone:
  • The tilemap (geometry of walls, floors, doors, etc.).
  • All spawned enemy entities, NPC entities, and interactive objects.
  • Spawn rules defining where and what to spawn.
  • A reference to the static ZoneData configuration (from GameData.hpp).

ZONE LIFECYCLE
──────────────
1. Load(ZoneData&):  Called when the player enters this zone.
   - Builds or loads the TileMap.
   - Registers spawn points for enemies and NPCs.
   - Does NOT spawn entities yet (that happens in SpawnEnemies/SpawnNPCs).

2. SpawnEnemies(World&):  Creates ECS entities for each spawn point.
   - Reads EnemyData from GameDatabase to fill components.
   - Attaches: TransformComponent, HealthComponent, StatsComponent,
     NameComponent, RenderComponent, AIComponent, CombatComponent.

3. SpawnNPCs(World&):  Creates ECS entities for non-hostile NPCs.
   - Attaches: TransformComponent, NameComponent, RenderComponent,
     DialogueComponent.

4. Update(World&, float dt):  Called every game frame while the zone
   is active.
   - Checks if respawn timers have elapsed.
   - Removes EntityIDs of dead/destroyed enemies from the tracking lists.
   - Can trigger ambient events (random encounters, day/night spawns).

5. Unload(World&):  Called when the player leaves this zone.
   - Destroys all enemy and NPC entities in the ECS.
   - Clears spawn point lists.
   - TileMap memory is released when the Zone object goes out of scope.

OWNERSHIP MODEL
───────────────
• Zone OWNS the TileMap (member variable).
• Zone OWNS the lists of EntityIDs it spawned.
• Zone does NOT own the ECS World — it is passed by reference.
• Zone holds a NON-OWNING pointer (const ZoneData*) to the static
  database record in GameDatabase.  The GameDatabase is never destroyed
  during gameplay, so the pointer is always valid.

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024

C++ Standard: C++17

### Data vs Runtime State

**Source:** [`src/game/world/Zone.hpp`](src/game/world/Zone.hpp#L87) (line 87)

──────────────────────────────────────
SpawnPoint is a lightweight *specification*: it says "spawn an enemy of
type enemyDataID at tile (x, y)".  It does NOT hold a live EntityID.

Once SpawnEnemies() runs, each SpawnPoint is used to create an ECS entity.
The created entity's ID is stored separately in m_enemyEntities.

Separating the specification (SpawnPoint) from the live instance (EntityID)
allows respawning: when an enemy dies, we don't need to remember where
it was; the SpawnPoint still knows.

### Single Responsibility

**Source:** [`src/game/world/Zone.hpp`](src/game/world/Zone.hpp#L118) (line 118)

──────────────────────────────────────
Zone has one responsibility: manage the state of a single explorable area.
It does NOT:
  • Handle player input (that is the GameStateManager's job).
  • Render the map (that is the RenderSystem's job).
  • Implement combat logic (that is the CombatSystem's job).

Zone *does*:
  • Own the TileMap geometry.
  • Track which entities it spawned (so it can clean them up on Unload).
  • Drive the spawn/respawn cycle for its enemies and NPCs.

This single-responsibility design makes Zone easy to test in isolation:
you can call Load(), SpawnEnemies(), Unload() in a unit test without
needing a renderer or input system.

### Explicit Unload

**Source:** [`src/game/world/Zone.hpp`](src/game/world/Zone.hpp#L151) (line 151)

────────────────────────────────
We do NOT automatically call Unload(world) in the destructor because
the destructor doesn't have access to the World reference.

Rule of thumb: whenever a class manages external state (here, ECS entities),
provide an explicit cleanup method and call it before the object is destroyed.
Relying on destructors for state cleanup across system boundaries leads
to hard-to-trace bugs.

### Separate Load from Construct

**Source:** [`src/game/world/Zone.hpp`](src/game/world/Zone.hpp#L185) (line 185)

─────────────────────────────────────────────
We do not do loading in the constructor because:
  1. Constructors cannot return error codes.
  2. Loading may fail (corrupt data, missing resources) — a failed
     load leaves the object in a valid "unloaded" state, which is
     better than a half-constructed object.
  3. Zones may be pre-allocated and loaded lazily (just-in-time when
     the player approaches).

### Cleanup Responsibility

**Source:** [`src/game/world/Zone.hpp`](src/game/world/Zone.hpp#L205) (line 205)

────────────────────────────────────────
Zone spawned these entities, so Zone is responsible for destroying them.
This is the "creator owns" principle: whoever allocates/creates a resource
is responsible for freeing/destroying it.

### Frame Update vs Event-Driven

**Source:** [`src/game/world/Zone.hpp`](src/game/world/Zone.hpp#L224) (line 224)

──────────────────────────────────────────────
Respawn timers are updated per-frame (polling), not via events.
For a large number of zones loaded simultaneously, an event-driven
approach (enemy death fires an event, zone subscribes) would be more
efficient.  For a single-zone game like this, polling is simpler.

### Component Initialisation

**Source:** [`src/game/world/Zone.hpp`](src/game/world/Zone.hpp#L245) (line 245)

──────────────────────────────────────────
Components are initialised from EnemyData fields, not from hard-coded
defaults.  Example:
  HealthComponent hp;
  hp.hp    = enemyData->hp;
  hp.maxHp = enemyData->hp;
This ensures the enemy's stats in-game exactly match the database.

### NPC Placement

**Source:** [`src/game/world/Zone.hpp`](src/game/world/Zone.hpp#L264) (line 264)

──────────────────────────────
In this simplified engine, NPC positions are distributed across
passable tiles algorithmically.  A production game would have hand-
authored NPC positions stored in the zone asset file.

### Tombstone / Deferred Removal

**Source:** [`src/game/world/Zone.hpp`](src/game/world/Zone.hpp#L364) (line 364)

───────────────────────────────────────────────
We cannot remove dead entities inside the Update() loop while iterating
over m_enemyEntities (iterator invalidation).  Instead we use the
erase-remove idiom:
  entities.erase(
    std::remove_if(entities.begin(), entities.end(), isDead),
    entities.end()
  );

This is O(n) in the size of the list — acceptable for a small zone.
A production engine might use a "graveyard" list and batch-remove.

### std::vector<uint32_t> vs std::vector<EntityID>

**Source:** [`src/game/world/Zone.hpp`](src/game/world/Zone.hpp#L395) (line 395)

───────────────────────────────────────────────────────────────
We store uint32_t here rather than EntityID to avoid having to
include the full ECS header in Zone.hpp for just one typedef.
Since EntityID is `using EntityID = uint32_t;`, they are identical
at the binary level.  The static_assert in Zone.cpp double-checks this.

### std::list for Spawn Points

**Source:** [`src/game/world/Zone.hpp`](src/game/world/Zone.hpp#L411) (line 411)

────────────────────────────────────────────
We use std::list (doubly-linked list) here because:
  • Spawn points can be added and removed at arbitrary positions
    (e.g. scripted spawns during quests) without invalidating other
    iterators.  std::vector would invalidate iterators on insert/erase.
  • We do not need random access (index-based) to spawn points.
  • The number of spawn points per zone is small (< 50), so cache
    efficiency of std::list vs std::vector is not a concern here.

In a performance-critical system (MMO server with thousands of zones),
a sorted std::vector + tombstone approach would be faster.

---

## samples/vertical_slice_project

### What is "Cooking"?

**Source:** [`samples/vertical_slice_project/cook_assets.py`](samples/vertical_slice_project/cook_assets.py#L6) (line 6)

### Why a single cook script?

**Source:** [`samples/vertical_slice_project/cook_assets.py`](samples/vertical_slice_project/cook_assets.py#L31) (line 31)

Every engine (Unreal, Unity, Godot) has a cook/export step.  Keeping it as
a simple Python script makes it:
• Easy to understand and modify
• Runnable from CI pipelines (GitHub Actions, Jenkins)
• Debuggable with standard Python tools
"""

### Optional Tool Integration

**Source:** [`samples/vertical_slice_project/cook_assets.py`](samples/vertical_slice_project/cook_assets.py#L50) (line 50)

---------------------------------------------------------------------------
We try to import the Python authoring tools that live in tools/.  If they
are installed (e.g. via  pip install -e tools/audio_authoring)  the cook
step will use them for real audio/animation processing.  If they are NOT
installed (e.g. on a machine that only has the raw C++ build tools) we fall
back to simple file-copy stubs so the pipeline still works end-to-end.
---------------------------------------------------------------------------
try:
from animation_engine.integration import AnimAssetPipeline  # type: ignore
_HAS_ANIM_ENGINE = True
except ImportError:
_HAS_ANIM_ENGINE = False

### Hashing

**Source:** [`samples/vertical_slice_project/cook_assets.py`](samples/vertical_slice_project/cook_assets.py#L87) (line 87)

We store a hash of each source asset in the registry.  On the next cook
run we compare the current hash to the stored one.  If they match, the
asset is unchanged and we skip it (incremental rebuild).
"""
h = hashlib.sha256()
with path.open("rb") as f:
for chunk in iter(lambda: f.read(65536), b""):
h.update(chunk)
return h.hexdigest()

### Texture Cooking (Stub)

**Source:** [`samples/vertical_slice_project/cook_assets.py`](samples/vertical_slice_project/cook_assets.py#L116) (line 116)

A real cook step would:
1. Decode the PNG with Pillow or stb_image.
2. Generate mip levels (halve dimensions until 1×1).
3. Compress to BC1/BC3/BC7 using ispc_texcomp or DirectXTex.
4. Write a custom .tex binary header + compressed mip data.
For now we just copy the file to Cooked/ to show the pipeline structure.
"""
texture_src = CONTENT_DIR / "Textures"
texture_dst = COOKED_DIR  / "Textures"
ensure_dir(texture_dst)

### Audio Cooking

**Source:** [`samples/vertical_slice_project/cook_assets.py`](samples/vertical_slice_project/cook_assets.py#L156) (line 156)

When the  tools/audio_authoring  package is installed, this function uses
the  audio_engine.dsp  module to normalise each WAV file (target LUFS,
true-peak ceiling) before writing to Cooked/Audio/.  It also writes a
.bank JSON manifest that the C++ runtime reads at startup.

### Why normalise at cook time?

**Source:** [`samples/vertical_slice_project/cook_assets.py`](samples/vertical_slice_project/cook_assets.py#L165) (line 165)

Normalising audio during the cook step (not at runtime) means:
1. The runtime doesn't waste CPU cycles on DSP during gameplay.
2. All audio has consistent loudness regardless of how the source WAVs
were recorded.
3. The cooked asset is the final, verified form — what ships in the game.
"""
audio_src  = CONTENT_DIR / "Audio"
audio_dst  = COOKED_DIR  / "Audio"
ensure_dir(audio_dst)

### Scene Cooking

**Source:** [`samples/vertical_slice_project/cook_assets.py`](samples/vertical_slice_project/cook_assets.py#L258) (line 258)

For simple JSON scenes, cooking is mostly a copy + validation step.
A real cook might:
1. Validate against scene.schema.json.
2. Resolve asset references (replace names with GUIDs from the registry).
3. Write a compact binary version for faster runtime loading.
"""
maps_src = CONTENT_DIR / "Maps"
maps_dst = COOKED_DIR  / "Maps"
ensure_dir(maps_dst)

### Animation Cooking

**Source:** [`samples/vertical_slice_project/cook_assets.py`](samples/vertical_slice_project/cook_assets.py#L295) (line 295)

When the  tools/anim_authoring  package is installed, this function uses
the  AnimAssetPipeline  class from  animation_engine.integration  to
deserialise skeletons and clips, apply key-frame reduction and then write
the cooked .skelc / .animc files.

### Python 3.9 compatibility

**Source:** [`samples/vertical_slice_project/cook_assets.py`](samples/vertical_slice_project/cook_assets.py#L382) (line 382)

Path.is_relative_to() was added in Python 3.9.  We use a try/except
approach so the code also runs on Python 3.8 (the minimum for some CI
runners).  When the path is not under base we return the full string.
"""
try:
return str(path.relative_to(base))
except ValueError:
return str(path)

### Asset Registry

**Source:** [`samples/vertical_slice_project/cook_assets.py`](samples/vertical_slice_project/cook_assets.py#L396) (line 396)

The registry is the single source of truth for all cooked assets.
It maps stable GUIDs → file paths + hashes.  The engine reads it at
startup to build an in-memory lookup table.
"""
data = {
"$schema":     "../../shared/schemas/asset_registry.schema.json",
"version":     "1.0.0",
"generatedAt": datetime.now(tz=timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
"assets":      registry,
}
REGISTRY_FILE.write_text(json.dumps(data, indent=2), encoding="utf-8")
print(f"\n  Registry written: {REGISTRY_FILE.name}  ({len(registry)} assets)")

---

## sandbox/main.cpp

### What is engine_sandbox?

**Source:** [`src/sandbox/main.cpp`](src/sandbox/main.cpp#L6) (line 6)

============================================================================
engine_sandbox is the *first real rendering milestone* of this educational
engine.  It demonstrates:

  M0 — Window + renderer bootstrap: open a Win32 window, initialise the
       chosen rendering backend, render an animated clear-colour loop.
  M1 — Triangle: vertex shader, fragment shader, PSO, vertex buffer — the
       classic "hello triangle" that proves the graphics pipeline works.

============================================================================

### Renderer Backends

**Source:** [`src/sandbox/main.cpp`](src/sandbox/main.cpp#L17) (line 17)

============================================================================
engine_sandbox supports two rendering backends selectable at runtime:

  --renderer d3d11   (default) — Direct3D 11; works on any GPU from ~2006+
                                 including the GeForce GT 610.  Uses D3D11
                                 WARP (CPU software renderer) in headless/CI
                                 mode so no GPU driver is needed in CI.

  --renderer vulkan  (optional) — Vulkan 1.0+; the modern / high-end path.
                                  Requires a Vulkan ICD on the machine.
                                  Only available when compiled with
                                  ENGINE_ENABLE_VULKAN=ON.

D3D11 is the default because:
  1. It ships with Windows (no install needed).
  2. It runs on older hardware (GT610 class) and CI runners alike.
  3. D3D11 WARP lets --headless succeed on GitHub-hosted Windows runners
     without any GPU driver installed.

============================================================================

### WinMain vs main()

**Source:** [`src/sandbox/main.cpp`](src/sandbox/main.cpp#L38) (line 38)

============================================================================
A standard Windows console app uses main().  A Windows GUI app (no console
window) uses WinMain.  For teaching purposes we use the standard main()
entry point and link with the CONSOLE subsystem so log output is visible
in the terminal.  When you ship a game you would switch to WinMain and
pipe log output to a file instead.

============================================================================

### --headless Mode

**Source:** [`src/sandbox/main.cpp`](src/sandbox/main.cpp#L47) (line 47)

============================================================================
Running with --headless skips the main render loop and exits with code 0
after printing "[PASS]".  This allows CI machines (which have no display
and may have no GPU driver) to validate the bootstrap path.

In D3D11 mode, headless uses WARP — a CPU software rasteriser bundled with
every Windows installation — so the binary works on any Windows runner.

Usage:
  engine_sandbox.exe                              # D3D11 windowed (default)
  engine_sandbox.exe --headless                   # D3D11 WARP headless (CI)
  engine_sandbox.exe --renderer vulkan --headless # Vulkan headless
  engine_sandbox.exe --headless --scene triangle  # M1 validation
  engine_sandbox.exe --scene testworld            # M3 full system demo (windowed)
  engine_sandbox.exe --headless --scene testworld # M3 full system demo (CI)
  engine_sandbox.exe --scene textured_quad        # M3 D3D11 textured quad (windowed)
  engine_sandbox.exe --headless --scene textured_quad  # M3 D3D11 texture CI
  engine_sandbox.exe --scene skinned_mesh         # M4b GPU skinning demo (windowed)
  engine_sandbox.exe --headless --scene skinned_mesh   # M4b GPU skinning CI
  engine_sandbox.exe --headless --scene physics_test   # M5 physics acceptance tests (CI)

============================================================================

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17
Target: Windows (MSVC)

### M5 Physics headless test

**Source:** [`src/sandbox/main.cpp`](src/sandbox/main.cpp#L86) (line 86)

---------------------------------------------------------------------------
The physics_test scene exercises the Jolt Physics integration on the CPU:
  1. Drop sphere — gravity simulation acceptance test.
  2. Character step-up — CharacterController ledge traverse test.
  3. Raycast — terrain query acceptance test.
ENGINE_ENABLE_PHYSICS is defined when CMake finds JoltPhysics via vcpkg.
Without it the physics_test path exits with a helpful message instead of
failing silently.
---------------------------------------------------------------------------
ifdef ENGINE_ENABLE_PHYSICS
 include "engine/physics/physics_world.hpp"
 include "engine/physics/character_controller.hpp"
 include "engine/physics/raycast.hpp"
 include "engine/physics/hit_volume.hpp"
endif

### Shader Directory Resolution

**Source:** [`src/sandbox/main.cpp`](src/sandbox/main.cpp#L112) (line 112)

---------------------------------------------------------------------------
The compiled shader files (.spv for Vulkan, .cso for D3D11) are placed next
to the executable by CMake.  We derive the executable's directory from
argv[0] to construct an absolute path, so the binary works from any CWD.
---------------------------------------------------------------------------
static std::string GetShaderDir(const char* argv0)
{
namespace fs = std::filesystem;
fs::path p(argv0);
fs::path dir = p.has_parent_path() ? p.parent_path() : fs::path(".");
return (dir / "shaders" / "").string();   // trailing separator
}

### Entry Point with argc/argv

**Source:** [`src/sandbox/main.cpp`](src/sandbox/main.cpp#L127) (line 127)

---------------------------------------------------------------------------
We use int main(int argc, char* argv[]) so the executable can receive
command-line flags.  This is standard C++ — on Windows the MSVC linker
routes it to the correct Windows entry point when we use /SUBSYSTEM:CONSOLE.
---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
try
{
-------------------------------------------------------------------
Step 0 — Parse command-line arguments.
-------------------------------------------------------------------

### Command-Line Parsing

**Source:** [`src/sandbox/main.cpp`](src/sandbox/main.cpp#L140) (line 140)

We use a simple linear scan rather than a third-party flag library
to keep the dependency count zero and the code readable.
-------------------------------------------------------------------
bool        headless         = false;
std::string scene;               // empty = no scene; "triangle" = M1
std::string validateProject;     // M2: path to project dir
std::string rendererArg;         // "d3d11" or "vulkan"; empty = default

### --validate-project flag

**Source:** [`src/sandbox/main.cpp`](src/sandbox/main.cpp#L162) (line 162)

-----------------------------------------------------------
This M2 flag validates that the project's cooked asset
database can be loaded and that every registered asset is
accessible.  It runs without opening a renderer window and
exits 0 on success.

Intended CI usage (after cook.exe has run):
  engine_sandbox.exe --validate-project samples/vertical_slice_project
-----------------------------------------------------------
validateProject = argv[++i];
}
else if (std::strcmp(argv[i], "--renderer") == 0 && i + 1 < argc)
{
-----------------------------------------------------------

### --renderer flag

**Source:** [`src/sandbox/main.cpp`](src/sandbox/main.cpp#L177) (line 177)

-----------------------------------------------------------
Selects the graphics backend at runtime.
  --renderer d3d11   → Direct3D 11 (default; GT610 compatible)
  --renderer vulkan  → Vulkan 1.0+ (requires Vulkan ICD)
-----------------------------------------------------------
rendererArg = argv[++i];
}
}

### Validate-Only Mode

**Source:** [`src/sandbox/main.cpp`](src/sandbox/main.cpp#L190) (line 190)

This path runs cook validation without opening any renderer window.
It exercises the AssetDB + AssetLoader pipeline introduced in M2.
-------------------------------------------------------------------
if (!validateProject.empty())
{
namespace fs = std::filesystem;

### Validating every asset in the database

**Source:** [`src/sandbox/main.cpp`](src/sandbox/main.cpp#L219) (line 219)

db.All() returns all GUIDs.  We iterate every GUID and call
loader.LoadRaw(), which opens the cooked file.  An empty return
vector signals a failure — either the cooked file is missing,
corrupted, or the path is wrong.
for (const std::string& guid : db.All())
{
const auto bytes = loader.LoadRaw(guid);
if (bytes.empty())
++loadErrors;
}

### Default Backend: D3D11

**Source:** [`src/sandbox/main.cpp`](src/sandbox/main.cpp#L244) (line 244)

If --renderer is not specified we use D3D11 because it works on all
Windows machines from Win7 (GT610-compatible) and on CI runners
with no GPU driver (via the WARP software renderer).
-------------------------------------------------------------------
const auto backend = engine::rendering::ParseRendererBackend(rendererArg);

### Factory Usage

**Source:** [`src/sandbox/main.cpp`](src/sandbox/main.cpp#L274) (line 274)

CreateRenderer returns a std::unique_ptr<IRenderer> so ownership
is clear: main() owns the renderer, and it is automatically
destroyed when the unique_ptr goes out of scope.
-------------------------------------------------------------------
auto renderer = engine::rendering::CreateRenderer(backend);
if (!renderer)
{
std::cerr << "[engine_sandbox] Failed to create renderer.\n";
window.Shutdown();
return 1;
}

### Headless Exit Protocol

**Source:** [`src/sandbox/main.cpp`](src/sandbox/main.cpp#L323) (line 323)

Acceptance tests expect exactly one "[PASS]" line on stdout
followed by exit code 0.  Any other output (or non-zero exit) = fail.
-------------------------------------------------------------------
if (headless)
{
if (scene == "triangle")
{
Validate: record one frame to confirm the draw call works.
if (!renderer->RecordHeadlessFrame())
{
std::cout << "[FAIL] Headless frame recording failed.\n";
renderer->Shutdown();
window.Shutdown();
return 1;
}
std::cout << "[PASS] Pipeline created. Mesh uploaded. Draw recorded.\n";
}
else if (scene == "textured_quad" || scene == "skinned_mesh")
{
-----------------------------------------------------------

### Headless Scene Validation (M3 / M4b)

**Source:** [`src/sandbox/main.cpp`](src/sandbox/main.cpp#L344) (line 344)

-----------------------------------------------------------
RecordHeadlessFrame() creates a 64×64 off-screen render
target, binds it, draws the scene once, then flushes.
It validates that the full GPU pipeline (VS, PS, buffers,
constant buffers, textures) can be compiled and executed
on the WARP software rasteriser without errors.

"textured_quad": exercises D3D11 texture + HLSL pipeline.
"skinned_mesh":  exercises GPU skinning CB + HLSL skinning VS.
-----------------------------------------------------------
if (!renderer->RecordHeadlessFrame())
{
std::cout << "[FAIL] Headless " << scene << " scene recording failed.\n";
renderer->Shutdown();
window.Shutdown();
return 1;
}
std::cout << "[PASS] " << scene << " scene pipeline OK (WARP headless).\n";
}
else if (scene == "physics_test")
{
-----------------------------------------------------------

### M5 Physics Acceptance Tests

**Source:** [`src/sandbox/main.cpp`](src/sandbox/main.cpp#L367) (line 367)

-----------------------------------------------------------
The physics_test headless path exercises three of the M5
acceptance criteria from FF15_REQUIREMENTS_BLUEPRINT.md §10:

  Test 1 — Drop sphere from height.
    Create a floor (static box) at Y=0 and a sphere at Y=10.
    Step 120 frames at 1/60 s (= 2 simulated seconds).
    A sphere starting at 10 m under 9.81 m/s² gravity takes
    ~1.43 s to hit the floor: Y = 10 - ½×9.81×t² = 0 at t≈1.43.
    After 2 s the sphere must have Y ≤ 0.5 + epsilon.

  Test 2 — Character steps over a 0.25 m ledge.
    Place a 0.25 m-high box on the floor.
    Walk the character toward and over the ledge for 2 s.
    Assert the character's Y position is above the ledge top.

  Test 3 — Raycast from above terrain.
    Fire a downward ray from Y=5 toward the floor at Y=0.
    Assert hit.distance ≈ 5.0 m (within ±0.01 m).
-----------------------------------------------------------

### Generous tolerance for CI

**Source:** [`src/sandbox/main.cpp`](src/sandbox/main.cpp#L488) (line 488)

On WARP (software) and with a 1/60 s step the
character may land slightly above or below the exact
ledge height.  We accept anything >= ledgeTop - 0.1 m.
if (charY < ledgeTop - 0.1f)
{
std::cout << "[FAIL] physics_test/step_ledge: "
<< "charY=" << charY
<< " expected >=" << (ledgeTop - 0.1f) << "\n";
++testsFailed;
}
else
{
std::cout << "[OK] physics_test/step_ledge: "
<< "charY=" << charY
<< " (cleared " << ledgeTop << " m ledge)\n";
}

### Build-time gate

**Source:** [`src/sandbox/main.cpp`](src/sandbox/main.cpp#L572) (line 572)

If joltphysics was not found by CMake, ENGINE_ENABLE_PHYSICS
is not defined and this physics_test scene is not available.
Build with -DENGINE_ENABLE_PHYSICS=ON and install joltphysics
via vcpkg to enable this validation path.
-----------------------------------------------------------
std::cout << "[SKIP] physics_test: ENGINE_ENABLE_PHYSICS not defined "
"(rebuild with joltphysics via vcpkg).\n"
"[PASS] physics_test: skipped (no Jolt Physics in build).\n";
endif
}
else if (scene == "testworld")
{
-----------------------------------------------------------

### Headless TestWorld

**Source:** [`src/sandbox/main.cpp`](src/sandbox/main.cpp#L586) (line 586)

-----------------------------------------------------------
Boots all gameplay systems, runs 600 fixed-dt frames, then
exits 0 if every system reported OK.  Ideal for CI: no GPU
or audio hardware required.
-----------------------------------------------------------
engine::sandbox::TestWorld tw;
if (!tw.Init())
{
std::cout << "[FAIL] TestWorld::Init() returned false.\n";
renderer->Shutdown();
window.Shutdown();
return 1;
}

### Fixed Timestep vs Variable Timestep

**Source:** [`src/sandbox/main.cpp`](src/sandbox/main.cpp#L632) (line 632)

For this minimal demo we use a simple variable-timestep loop:
render as fast as the GPU allows (limited by vsync).
A real game loop uses a fixed timestep for deterministic physics.
-------------------------------------------------------------------
double totalTime = 0.0;

### TestWorld integration in the render loop

**Source:** [`src/sandbox/main.cpp`](src/sandbox/main.cpp#L640) (line 640)

-----------------------------------------------------------------------
When --scene testworld is specified, we create a TestWorld and call
tw.Update(dt) each frame.  The TestWorld returns an RGB clear-colour
(GetClearColour) that reflects the current game state:

  Combat active → red tint       Night → deep navy
  Victory flash → gold pulse     Rain  → grey-blue
  Camping       → warm orange    Day   → sky blue

This gives a visual confirmation that the game systems are running
and changing state.  As more rendering milestones land, replace the
clear-colour with actual geometry + lighting draw calls.
-----------------------------------------------------------------------
std::unique_ptr<engine::sandbox::TestWorld> testWorld;
if (scene == "testworld")
{
testWorld = std::make_unique<engine::sandbox::TestWorld>();
if (!testWorld->Init())
{
std::cerr << "[FAIL] TestWorld::Init() returned false.\n";
renderer->Shutdown();
window.Shutdown();
return 1;
}
}

### std::sin / std::cos for animation

**Source:** [`src/sandbox/main.cpp`](src/sandbox/main.cpp#L694) (line 694)

Each channel has a different phase offset so they don't all
peak at the same moment, producing a smooth rainbow sweep.
const float speed = 0.5f;
const float tF    = static_cast<float>(totalTime);
clearR = (std::sin(tF * speed + 0.0f)   + 1.0f) * 0.5f;
clearG = (std::sin(tF * speed + 2.094f) + 1.0f) * 0.5f;  // 2pi/3
clearB = (std::sin(tF * speed + 4.189f) + 1.0f) * 0.5f;  // 4pi/3
}

### Shutdown Order

**Source:** [`src/sandbox/main.cpp`](src/sandbox/main.cpp#L710) (line 710)

The renderer must be shut down BEFORE the window because the
swap chain / surface references the HWND.  Destroying the window
first would leave the renderer pointing at a destroyed handle.
-------------------------------------------------------------------
renderer->Shutdown();
window.Shutdown();

---

## sandbox/test_world.cpp

### Keep TestWorld as a low-risk integration smoke test.

**Source:** [`src/sandbox/test_world.cpp`](src/sandbox/test_world.cpp#L30) (line 30)

It should depend only on stable APIs so CI can quickly verify that
engine_sandbox starts, updates ECS state, and renders a deterministic frame.
RegisterAllComponents(m_world);

### Placeholder clip GUID

**Source:** [`src/sandbox/test_world.cpp`](src/sandbox/test_world.cpp#L55) (line 55)

The clip ID here is a non-fatal placeholder for sandbox smoke testing.
If the backend cannot resolve it through AssetDB, playback simply no-ops.
auto& audio = m_world.AddComponent<AudioSourceComponent>(m_player);
audio.clipID = "guid-sfx-footstep";
audio.volume = 0.5f;
audio.isPlaying = false;

### Simple deterministic motion is ideal for CI checks.

**Source:** [`src/sandbox/test_world.cpp`](src/sandbox/test_world.cpp#L96) (line 96)

A predictable sine-wave path gives a visible animated signal in windowed
runs while remaining fully deterministic in headless mode.
tf.position.x += dt * kMoveSpeed;
tf.position.y = std::sin(m_time * kSineFrequency) * kSineAmplitude;

### Oscillation formula for deterministic visual feedback

**Source:** [`src/sandbox/test_world.cpp`](src/sandbox/test_world.cpp#L102) (line 102)

Uses base + amplitude * sin(frequency * t) for a smooth periodic pulse.
Choosing a pulse frequency different from movement frequency creates a
subtle beat effect, making state motion easier to observe in test runs.
const float pulse = kPulseBase + kPulseScale * std::sin(m_time * kPulseFrequency);
m_clearR = kRedBase + pulse * kRedVariation;
m_clearG = kGreenBase + pulse * kGreenVariation;
m_clearB = kBlueBase + pulse * kBlueVariation;
}

---

## sandbox/test_world.hpp

### Stable integration surface

**Source:** [`src/sandbox/test_world.hpp`](src/sandbox/test_world.hpp#L12) (line 12)

Keep this class API intentionally small and stable because `main.cpp`
drives TestWorld in both headless CI and interactive sandbox mode.
TestWorld();
~TestWorld() = default;

---

## scripts/check_architecture.py

### Why Architecture Enforcement Matters

**Source:** [`scripts/check_architecture.py`](scripts/check_architecture.py#L6) (line 6)

### Forbidden-Include Table

**Source:** [`scripts/check_architecture.py`](scripts/check_architecture.py#L55) (line 55)

----------------------------------------
Each entry in LAYER_RULES maps a *source layer* (a glob-like path prefix
relative to ``src/``) to the set of *forbidden* path substrings that must
NOT appear in its ``#include`` directives.

The table is taken directly from COPILOT_CONTINUATION.md § 3.3:

  Layer              May depend on       Must NOT depend on
  engine/core/       (nothing else)      everything below
  engine/ecs/        core/               game/, tools
  engine/rendering/  core/, ecs/         game/
  engine/audio/      core/, ecs/         game/, rendering/
  engine/animation/  core/, ecs/         game/
  engine/physics/    core/, ecs/         game/
  game/systems/      engine/             tools/, editor/
  game/world/        engine/, systems/   tools/, editor/
  tools/             engine/core/        game/

Implementation note: we match on the *normalised* include path string
(forward slashes, relative to the repo root ``src/`` directory).

### Teaching Exceptions to the 500-Line Rule

**Source:** [`scripts/check_architecture.py`](scripts/check_architecture.py#L96) (line 96)

----------------------------------------------------------
These files intentionally exceed 500 lines because their size is
*pedagogically justified*: the ECS is studied as a whole, and splitting
it would obscure the relationships between components.  Every exception
must be listed here with a rationale comment.

Files listed here are exempt from the line-count violation check but are
still subject to all other checks (layer boundaries, TEACHING NOTEs).

### Suppressing Known Pre-existing Violations

**Source:** [`scripts/check_architecture.py`](scripts/check_architecture.py#L178) (line 178)

----------------------------------------------------------
A freshly introduced lint rule will almost always find violations in existing
code.  The correct workflow is:

  1. List all *pre-existing* violations here with a clear rationale and a
     TODO comment noting the refactor needed to remove them.
  2. New violations that are NOT in this list immediately fail CI.
  3. Work through the list over time, removing entries as the refactors land.

Key: (repo-relative file path, forbidden include substring)
Value: human-readable rationale for allowing the exception.

### Why 500 Lines?

**Source:** [`scripts/check_architecture.py`](scripts/check_architecture.py#L280) (line 280)

### Include-Based Layer Checking

**Source:** [`scripts/check_architecture.py`](scripts/check_architecture.py#L322) (line 322)

### Documentation as a First-Class Requirement

**Source:** [`scripts/check_architecture.py`](scripts/check_architecture.py#L402) (line 402)

### skip_dirs excludes build artefacts and third-party sources.

**Source:** [`scripts/check_architecture.py`](scripts/check_architecture.py#L439) (line 439)

"Lua" is excluded because Lua/lua-5.5.0/ contains vendored third-party
source that intentionally has no TEACHING NOTE blocks and may be large.
Scanning it would produce false-positive warnings.
skip_dirs = {"build", "build-test", ".git", "__pycache__", "node_modules", "Lua"}
result: list[Path] = []
for path in sorted(repo_root.rglob("*")):
if any(part in skip_dirs for part in path.parts):
continue
if path.suffix in (".cpp", ".hpp", ".h", ".c"):
result.append(path)
return result

---

## scripts/enemies.lua

### Script-Defined Abilities

**Source:** [`scripts/enemies.lua`](scripts/enemies.lua#L6) (line 6)

---

## scripts/extract_teaching_notes.py

### Automated Documentation Extraction

**Source:** [`scripts/extract_teaching_notes.py`](scripts/extract_teaching_notes.py#L6) (line 6)

### Deterministic Line Endings

**Source:** [`scripts/extract_teaching_notes.py`](scripts/extract_teaching_notes.py#L435) (line 435)

-------------------------------------------
Explicitly write LF (\n) line endings regardless of the host platform.
Without this, Python's default text mode on Windows translates every \n
to \r\n (CRLF), causing ``git diff --exit-code`` to detect a spurious
difference whenever the index is regenerated on a Windows CI runner or
developer machine.  We open in binary mode and encode ourselves to
guarantee byte-identical output across Linux, macOS, and Windows.
output_path.write_bytes(markdown.encode("utf-8"))

---

## scripts/main.lua

### Why Lua for Scripting?

**Source:** [`scripts/main.lua`](scripts/main.lua#L6) (line 6)

### Tables as Modules

**Source:** [`scripts/main.lua`](scripts/main.lua#L25) (line 25)

---

## scripts/quests.lua

### Data-Driven Quests with Lua

**Source:** [`scripts/quests.lua`](scripts/quests.lua#L6) (line 6)

---

## shared/runtime

### Why GUIDs?

**Source:** [`shared/runtime/Guid.hpp`](shared/runtime/Guid.hpp#L6) (line 6)

=============================================================================
In a game engine, every asset needs a stable identity that survives:
  • File renames  (the editor changes the path, but the GUID stays the same)
  • Directory moves (same problem)
  • Concurrent edits by multiple developers

A GUID (also called UUID — Universally Unique Identifier) is a 128-bit
number formatted as "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" (36 chars).
RFC 4122 Version 4 GUIDs use random bits, making collisions astronomically
unlikely (~5.3 × 10^36 possible values).

Unreal Engine uses FGuid for the same purpose.  Unity uses string GUIDs
stored in .meta sidecar files.  We follow the same pattern here.

=============================================================================
Usage:
  Guid id  = Guid::New();          // Generate a new random GUID
  Guid id2 = Guid::FromString(s);  // Parse from "xxxxxxxx-xxxx-xxxx-xxxx-xxxx"
  std::string s = id.ToString();   // Convert back to string
  bool valid = id.IsValid();        // True unless default-constructed
  bool eq    = (id == id2);         // Equality comparison

=============================================================================

### UUID v4 Format

**Source:** [`shared/runtime/Guid.hpp`](shared/runtime/Guid.hpp#L72) (line 72)

In a version-4 UUID the layout is:
  xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
  where 4 = version nibble
        y = variant nibble (must be 8, 9, A, or B)
All other bits are random.

### std::mt19937 (Mersenne Twister)

**Source:** [`shared/runtime/Guid.hpp`](shared/runtime/Guid.hpp#L81) (line 81)

mt19937 is the recommended general-purpose pseudo-random engine in C++.
We seed it once with a non-deterministic random_device.
thread_local ensures each thread has its own engine state.
thread_local static std::mt19937 rng{std::random_device{}()};
std::uniform_int_distribution<uint32_t> dist;

### std::ostringstream and hex formatting

**Source:** [`shared/runtime/Guid.hpp`](shared/runtime/Guid.hpp#L158) (line 158)

We use setfill('0') + setw(8) + hex to zero-pad the hex output.
std::ostringstream oss;
oss << std::hex << std::setfill('0');
oss << std::setw(8) << m_data[0] << '-';

### std::hash specialization

**Source:** [`shared/runtime/Guid.hpp`](shared/runtime/Guid.hpp#L190) (line 190)

This lets you use Guid as a key in std::unordered_map or std::unordered_set.
namespace std
{
template<>
struct hash<Guid>
{
size_t operator()(const Guid& g) const noexcept
{
Simple FNV-1a mix of the string representation
size_t h = 14695981039346656037ULL;
const std::string s = g.ToString();
for (char c : s)
{
h ^= static_cast<size_t>(c);
h *= 1099511628211ULL;
}
return h;
}
};
}

### Why a shared Log.hpp?

**Source:** [`shared/runtime/Log.hpp`](shared/runtime/Log.hpp#L6) (line 6)

=============================================================================
Every subsystem (engine, editor, tools) needs to log messages.  Without a
shared logger, each module invents its own — some use printf, some use
std::cout, some add timestamps, some don't.  The result is inconsistent
output that's hard to filter, redirect, or silence in tests.

This header provides five macros:

  LOG_INFO(msg)    – Informational messages (white / default)
  LOG_WARN(msg)    – Warnings that don't prevent execution (yellow)
  LOG_ERROR(msg)   – Errors that indicate a problem (red)
  LOG_DEBUG(msg)   – Verbose debug output (grey, stripped in Release)
  LOG_FATAL(msg)   – Unrecoverable error — logs then std::terminate()

All macros accept a C++ stream expression, so you can write:

  LOG_INFO("Loaded scene: " << sceneName << " in " << ms << "ms");

=============================================================================
Compile-time switches
=============================================================================

  #define LOG_SILENT          – suppress all output (useful in unit tests)
  #define LOG_LEVEL_WARN      – suppress INFO and DEBUG
  #define LOG_LEVEL_ERROR     – suppress INFO, DEBUG, WARN
  #define LOG_NO_COLOUR       – disable ANSI colour codes (for Windows CMD)

=============================================================================
Usage
=============================================================================

  #include "shared/runtime/Log.hpp"

  bool ok = LoadAsset(path);
  if (!ok) {
      LOG_ERROR("Failed to load asset: " << path);
      return false;
  }
  LOG_INFO("Asset loaded: " << path);

=============================================================================

### We write to std::cerr (not std::cout) so that log

**Source:** [`shared/runtime/Log.hpp`](shared/runtime/Log.hpp#L111) (line 111)

output doesn't mix with program output when piping to files.
std::cerr << colour
<< "[" << Timestamp() << "]"
<< "[" << level << "] "
<< message
<< _LOG_COL_GREY
<< "  (" << file << ":" << line << ")"
<< _LOG_COL_RESET
<< "\n";
}
}  // namespace Log

### Macro trick: the do { } while(0) wrapper lets you write

**Source:** [`shared/runtime/Log.hpp`](shared/runtime/Log.hpp#L139) (line 139)

LOG_INFO("x = " << x);
and have it behave as a single statement in all contexts (e.g. after if).

### In Release builds, LOG_DEBUG is a no-op at compile

**Source:** [`shared/runtime/Log.hpp`](shared/runtime/Log.hpp#L165) (line 165)

time so it generates zero code.  Always safe to leave in.
      define LOG_DEBUG(msg) do { } while(0)
  else
      define LOG_DEBUG(msg) \
do { \
std::ostringstream _log_ss; \
_log_ss << msg; \
Log::Write("DEBUG", _LOG_COL_GREY, _log_ss.str(), __FILE__, __LINE__); \
} while(0)
  endif

### Why Version Files?

**Source:** [`shared/runtime/VersionedFile.hpp`](shared/runtime/VersionedFile.hpp#L6) (line 6)

=============================================================================
Every data file shared between authoring tools and the engine MUST carry a
version number. When you change a field name, add/remove a required field,
or change the semantics of a value, you increment the version number.

Without versioning:
  Developer A saves a scene in the editor.
  Developer B pulls the code, engine format changed — their saves break.
  Users install an update — their save files no longer load.

With versioning:
  Reader checks version, applies migration if needed, then processes.

This file provides a minimal wrapper around nlohmann/json (or any JSON
library you choose) that enforces:
  1. Every file has a "version" field.
  2. Reading checks the version and can reject or migrate old data.
  3. Writing always stamps the current version.

=============================================================================
Dependencies:
  This header uses the C++ standard library only (fstream, string, etc.).
  JSON parsing is delegated to whatever library the including project uses.
  To use with Qt: include <QJsonDocument> and adapt VersionedFile::Read().
  To use standalone: include a header-only JSON lib (e.g. nlohmann/json).

=============================================================================

### Semantic Versioning (SemVer)

**Source:** [`shared/runtime/VersionedFile.hpp`](shared/runtime/VersionedFile.hpp#L48) (line 48)

MAJOR.MINOR.PATCH
  • MAJOR: breaking change — old readers can't load this file.
  • MINOR: backwards-compatible addition — old readers ignore new fields.
  • PATCH: bug fix — no format change.

### Minimal Header Pattern

**Source:** [`shared/runtime/VersionedFile.hpp`](shared/runtime/VersionedFile.hpp#L95) (line 95)

Every shared file starts with at least these two fields:
  { "version": "1.0.0", ... }

The reader extracts the version first, then decides how to parse the rest.
This allows format migrations without breaking older projects.

---

## src/main.cpp

### main() Should Be Minimal

**Source:** [`src/main.cpp`](src/main.cpp#L6) (line 6)

============================================================================

A well-designed C++ application's main() function should be very short.
Its job is to:
  1. Construct the top-level application object.
  2. Call Init().
  3. Call Run() (blocks until the user quits).
  4. Call Shutdown().
  5. Return 0 (success) or a non-zero error code.

ALL actual logic lives inside the application class and its subsystems.
This makes the code testable (you can construct `Game` in a test without
running main) and portable (you can replace the entry point for different
platforms — e.g., WinMain on Windows).

─── Return Values ────────────────────────────────────────────────────────────
Conventionally:
  0  = success
  1  = generic failure
 -1  = fatal exception

@author  Educational Game Engine Project
@version 1.0

---

## tests/cook

### Contract Tests

**Source:** [`tests/cook/test_cook_pipeline.py`](tests/cook/test_cook_pipeline.py#L4) (line 4)

### scope="module"

**Source:** [`tests/cook/test_cook_pipeline.py`](tests/cook/test_cook_pipeline.py#L65) (line 65)

Using module-level scope means the cook is only executed once even when
multiple tests in this file each request this fixture.  This speeds up
the test suite considerably.
"""
return run_cook()

### JSON Schema Validation

**Source:** [`tests/cook/test_cook_pipeline.py`](tests/cook/test_cook_pipeline.py#L134) (line 134)

We use the  jsonschema  library to validate the registry programmatically.
This is the same check that  tools/validate-assets.py  performs; having
it here means a failing cook is caught immediately in pytest output.
"""
try:
import jsonschema  # type: ignore
except ImportError:
pytest.skip("jsonschema not installed — skipping schema validation")

### Golden Path Verification

**Source:** [`tests/cook/test_cook_pipeline.py`](tests/cook/test_cook_pipeline.py#L198) (line 198)

This test embodies the main contract: if the cook script says an asset
is at a cooked path, that file MUST exist.  A missing cooked file means
the registry is out of sync with the file system — a fatal engine error.
"""
for asset in registry.get("assets", []):
cooked = asset.get("cooked")
if cooked:
full_path = SAMPLE_DIR / cooked
assert full_path.exists(), (
f"Asset '{asset.get('name', asset['id'])}' cooked path "
f"'{cooked}' does not exist at {full_path}"
)

### Golden File

**Source:** [`tests/cook/test_cook_pipeline.py`](tests/cook/test_cook_pipeline.py#L215) (line 215)

This is the minimal "golden" contract for the vertical slice: at least
one scene (MainTown) must be cooked and registered.  Add more golden
assertions here as the project grows.
"""
scene_assets = [
a for a in registry.get("assets", [])
if a.get("type") == "scene" and "MainTown" in a.get("name", "")
]
assert len(scene_assets) >= 1, (
"Expected at least one 'scene' asset named MainTown in the registry"
)

---

## tools/anim_authoring

### (untitled)

**Source:** [`tools/anim_authoring/animation_engine/__init__.py`](tools/anim_authoring/animation_engine/__init__.py#L5) (line 5)

### Package Layering

**Source:** [`tools/anim_authoring/animation_engine/animation/__init__.py`](tools/anim_authoring/animation_engine/animation/__init__.py#L4) (line 4)

### Tkinter vs. Qt

**Source:** [`tools/anim_authoring/animation_engine/editor/__init__.py`](tools/anim_authoring/animation_engine/editor/__init__.py#L4) (line 4)

### Integration Layer Pattern

**Source:** [`tools/anim_authoring/animation_engine/integration/__init__.py`](tools/anim_authoring/animation_engine/integration/__init__.py#L4) (line 4)

### Cook Pipeline

**Source:** [`tools/anim_authoring/animation_engine/integration/anim_pipeline.py`](tools/anim_authoring/animation_engine/integration/anim_pipeline.py#L4) (line 4)

### Cook Pipeline Design

**Source:** [`tools/anim_authoring/animation_engine/integration/anim_pipeline.py`](tools/anim_authoring/animation_engine/integration/anim_pipeline.py#L86) (line 86)

The pipeline follows a simple "scan → transform → write" pattern:
1. Scan the content directory for JSON source files.
2. For each file, determine its type (skeleton or clip) from the
``$schema`` field or filename convention.
3. Deserialise, re-serialise to the cooked path.
"""

### Serialization

**Source:** [`tools/anim_authoring/animation_engine/io/__init__.py`](tools/anim_authoring/animation_engine/io/__init__.py#L4) (line 4)

### Single Responsibility Principle

**Source:** [`tools/anim_authoring/animation_engine/io/__init__.py`](tools/anim_authoring/animation_engine/io/__init__.py#L30) (line 30)

The Exporter does ONE thing: write data to files.  It doesn't know how
to create or edit the data — that's the caller's job.  This separation
makes the exporter easy to test and easy to swap out.
"""

### Binary Formats

**Source:** [`tools/anim_authoring/animation_engine/io/__init__.py`](tools/anim_authoring/animation_engine/io/__init__.py#L64) (line 64)

JSON is great for authoring (human-readable, diffable) but slow
to parse at runtime.  A binary format is faster to load because:
1. No text parsing needed.
2. Data is already in the correct memory layout.
3. Smaller file size (floats as 4 bytes, not 10-20 chars).

### Round-Trip Testing

**Source:** [`tools/anim_authoring/animation_engine/io/__init__.py`](tools/anim_authoring/animation_engine/io/__init__.py#L81) (line 81)

A good Exporter+Importer pair should satisfy:
import(export(obj)) == obj
This is called a "round-trip" test and is a great first test to write
for any serialization system.
"""

### Why separate math_utils?

**Source:** [`tools/anim_authoring/animation_engine/math_utils/__init__.py`](tools/anim_authoring/animation_engine/math_utils/__init__.py#L4) (line 4)

### Data Models

**Source:** [`tools/anim_authoring/animation_engine/model/__init__.py`](tools/anim_authoring/animation_engine/model/__init__.py#L4) (line 4)

### Why a custom Vec3?

**Source:** [`tools/anim_authoring/animation_engine/model/__init__.py`](tools/anim_authoring/animation_engine/model/__init__.py#L33) (line 33)

We could use NumPy arrays, but for small data-model types a named
dataclass with x/y/z fields is self-documenting and avoids array
shape errors.  Performance-critical bulk operations use NumPy internally.
"""
x: float = 0.0
y: float = 0.0
z: float = 0.0

### Why Quaternions?

**Source:** [`tools/anim_authoring/animation_engine/model/__init__.py`](tools/anim_authoring/animation_engine/model/__init__.py#L75) (line 75)

Euler angles (yaw/pitch/roll) suffer from "gimbal lock" — a singularity
where two axes align and a degree of freedom is lost.
Quaternions are a 4D representation that avoids this problem.
They interpolate smoothly (SLERP), which is critical for animation.
See: https://en.wikipedia.org/wiki/Quaternions_and_spatial_rotation
"""
x: float = 0.0
y: float = 0.0
z: float = 0.0
w: float = 1.0  # Identity quaternion: no rotation

### SLERP

**Source:** [`tools/anim_authoring/animation_engine/model/__init__.py`](tools/anim_authoring/animation_engine/model/__init__.py#L99) (line 99)

SLERP interpolates on the 4D unit sphere, giving constant-speed,
shortest-path rotation interpolation.  This is why animated bones
don't "snap" between keyframes.
"""
Normalise inputs
a = self.normalized()
b = other.normalized()

### Bone Hierarchy

**Source:** [`tools/anim_authoring/animation_engine/model/__init__.py`](tools/anim_authoring/animation_engine/model/__init__.py#L146) (line 146)

A skeleton is a tree of bones.  Each bone has a parent (except the root).
Animations are stored in LOCAL space (relative to parent), but the engine
needs WORLD space (relative to the origin) for skinning.
World transform = Parent.WorldTransform × Local.Transform
"""
index: int              
name:  str              
parent_index: int = -1

### Linear Interpolation vs. SLERP

**Source:** [`tools/anim_authoring/animation_engine/model/__init__.py`](tools/anim_authoring/animation_engine/model/__init__.py#L252) (line 252)

We lerp translations (straight-line blend) but slerp rotations
(arc blend on unit quaternion sphere). Mixing these gives smooth,
natural-looking motion.
"""
if not self.keyframes:
return Keyframe(time)

### Runtime vs. Authoring

**Source:** [`tools/anim_authoring/animation_engine/runtime/__init__.py`](tools/anim_authoring/animation_engine/runtime/__init__.py#L4) (line 4)

### Animator Design

**Source:** [`tools/anim_authoring/animation_engine/runtime/__init__.py`](tools/anim_authoring/animation_engine/runtime/__init__.py#L17) (line 17)

The Animator follows the same pattern as the C++ AnimationSystem:
animator.load(skeleton, clip)
transforms = animator.evaluate(time_seconds)
"""

### Why test round-trips?

**Source:** [`tools/anim_authoring/tests/test_animation_engine.py`](tools/anim_authoring/tests/test_animation_engine.py#L11) (line 11)

A serialization round-trip test (export → import → compare) is one of the
most valuable tests you can write. It exercises both the exporter and
importer in one shot, and catches any field-name mismatches between the two.
"""

---

## tools/audio_authoring

### (untitled)

**Source:** [`tools/audio_authoring/audio_engine/__init__.py`](tools/audio_authoring/audio_engine/__init__.py#L5) (line 5)

### Façade Pattern

**Source:** [`tools/audio_authoring/audio_engine/engine.py`](tools/audio_authoring/audio_engine/engine.py#L6) (line 6)

### Graceful degradation

**Source:** [`tools/audio_authoring/audio_engine/engine.py`](tools/audio_authoring/audio_engine/engine.py#L39) (line 39)

NumPy is required for audio processing. If it's not installed, we provide
a helpful error message with installation instructions.
try:
import numpy as np
_NUMPY_AVAILABLE = True
except ImportError:
_NUMPY_AVAILABLE = False
np = None  # type: ignore[assignment]

### Constructor Dependency Injection

**Source:** [`tools/audio_authoring/audio_engine/engine.py`](tools/audio_authoring/audio_engine/engine.py#L74) (line 74)

All dependencies (generator, exporter) are created here with sensible
defaults. Advanced users can pass different values to customise behaviour.
This is "constructor injection" — a dependency injection pattern.
"""

### Return types

**Source:** [`tools/audio_authoring/audio_engine/engine.py`](tools/audio_authoring/audio_engine/engine.py#L121) (line 121)

We return the NumPy array AND optionally save to file. This lets
callers chain operations (e.g. apply more DSP) without re-reading
the file from disk.
"""
_require_numpy()
STUB: Returns silence until the real MusicGenerator is wired up.
Replace this with: from .ai.generator import MusicGenerator
duration = (bars or 8) * 2.0   # 2 seconds per bar as default
samples  = int(duration * self.sample_rate)
audio    = np.zeros((samples, 2), dtype=np.float32)

### Graceful import

**Source:** [`tools/audio_authoring/audio_engine/engine.py`](tools/audio_authoring/audio_engine/engine.py#L221) (line 221)

We try to import scipy first (most common). If it's not available
we fall back to a minimal WAV writer using Python's built-in wave
module. This keeps the tool usable without heavy dependencies.
"""
_require_numpy()
output = Path(path)
output.parent.mkdir(parents=True, exist_ok=True)

### Audio File Formats

**Source:** [`tools/audio_authoring/audio_engine/export/audio_exporter.py`](tools/audio_authoring/audio_engine/export/audio_exporter.py#L4) (line 4)

### RIFF Chunk Format

**Source:** [`tools/audio_authoring/audio_engine/export/audio_exporter.py`](tools/audio_authoring/audio_engine/export/audio_exporter.py#L99) (line 99)

A WAV file is made of named chunks: ``fmt `` (format), ``data``
(samples), and optionally ``smpl`` (sample metadata including loop
points).  We append the ``smpl`` chunk after the ``data`` chunk.
"""
path = Path(path)
wav_bytes = path.read_bytes()

### Why miniaudio?

**Source:** [`tools/audio_authoring/audio_engine/integration/cpp/AudioSystem.hpp`](tools/audio_authoring/audio_engine/integration/cpp/AudioSystem.hpp#L88) (line 88)

==========================================================================

miniaudio is the most popular single-file audio library for C/C++ games.

  • Public domain (Unlicense) — no attribution required.
  • Zero external dependencies — works on any C11/C++11 compiler.
  • Supports WAV, MP3, FLAC, OGG (with bundled decoders).
  • Cross-platform: Windows (WASAPI), macOS (CoreAudio), Linux (ALSA/Pulse).
  • Used in real-world indie games and teaching frameworks.

Compare this to heavier alternatives:
  SDL_mixer  — easy but pulls in all of SDL2.
  OpenAL     — powerful but complex (source/listener/buffer model).
  FMOD       — professional but commercial license required.
  miniaudio  — sweet spot for a teaching engine: simple + real-world.

==========================================================================

### AudioSystem Design

**Source:** [`tools/audio_authoring/audio_engine/integration/cpp/AudioSystem.hpp`](tools/audio_authoring/audio_engine/integration/cpp/AudioSystem.hpp#L106) (line 106)

==========================================================================

The AudioSystem follows the same pattern as every other system in this
engine:

  1. Owned by the Game class (not a singleton).
  2. Init() / Shutdown() lifecycle, called from Game::Init/Shutdown.
  3. Exposes an Update(float dt) hook for fade-in/fade-out effects.
  4. Lua C bindings as static methods (AudioSystem::Lua_PlaySFX etc.).

─── Asset naming convention ─────────────────────────────────────────────

The audio assets generated by the Python Audio Engine follow a strict
naming convention so the C++ side can locate them without configuration:

  assets/audio/
  ├── music/
  │   ├── music_main_menu.wav      ← GameState::MAIN_MENU
  │   ├── music_exploring.wav      ← GameState::EXPLORING
  │   ├── music_combat.wav         ← GameState::COMBAT
  │   ├── music_boss_combat.wav    ← used by CombatSystem for boss fights
  │   ├── music_dialogue.wav       ← GameState::DIALOGUE
  │   ├── music_vehicle.wav        ← GameState::VEHICLE
  │   ├── music_camping.wav        ← GameState::CAMPING
  │   ├── music_inventory.wav      ← GameState::INVENTORY
  │   ├── music_shopping.wav       ← GameState::SHOPPING
  │   └── music_victory.wav        ← played after GameState::COMBAT clears
  ├── sfx/
  │   ├── sfx_combat_hit.wav
  │   ├── sfx_spell_cast.wav
  │   ├── sfx_level_up.wav
  │   ├── sfx_quest_complete.wav
  │   └── … (see game_state_map.py for the full list)
  └── voice/
      ├── voice_welcome.wav
      ├── voice_level_up.wav
      └── … (see game_state_map.py for the full list)

@author  Audio Engine / Game Engine for Teaching project
@version 1.0

### Graceful degradation:

**Source:** [`tools/audio_authoring/audio_engine/integration/cpp/AudioSystem.hpp`](tools/audio_authoring/audio_engine/integration/cpp/AudioSystem.hpp#L252) (line 252)

Audio initialisation *can* fail (no sound card, CI server …).
  Rather than crashing the game, we set m_initialised = false and
  make every subsequent call a no-op.  The game still runs silently.

### This is a lightweight sanity check, not a full

**Source:** [`tools/audio_authoring/audio_engine/integration/cpp/AudioSystem.hpp`](tools/audio_authoring/audio_engine/integration/cpp/AudioSystem.hpp#L463) (line 463)

checksum validation.  In a production engine you would also verify
file sizes / hashes here.
namespace fs = std::filesystem;

### Using a lookup table instead of a switch/case makes it

**Source:** [`tools/audio_authoring/audio_engine/integration/cpp/AudioSystem.hpp`](tools/audio_authoring/audio_engine/integration/cpp/AudioSystem.hpp#L559) (line 559)

easy to add new states: just add one entry here and generate the asset.

### A full crossfade would:

**Source:** [`tools/audio_authoring/audio_engine/integration/cpp/AudioSystem.hpp`](tools/audio_authoring/audio_engine/integration/cpp/AudioSystem.hpp#L593) (line 593)

1. Keep the old track playing at decreasing volume.
  2. Start the new track at zero volume.
  3. Increase the new track's volume over kCrossfadeDuration seconds.
  4. Stop the old track when its volume reaches 0.
Implementing this is left as an exercise.
}

### Separation of Concerns

**Source:** [`tools/audio_authoring/audio_engine/integration/game_state_map.py`](tools/audio_authoring/audio_engine/integration/game_state_map.py#L4) (line 4)

### Lua ↔ C++ Audio Bridge

**Source:** [`tools/audio_authoring/audio_engine/integration/lua/audio.lua`](tools/audio_authoring/audio_engine/integration/lua/audio.lua#L5) (line 5)

### Module pattern

**Source:** [`tools/audio_authoring/audio_engine/integration/lua/audio.lua`](tools/audio_authoring/audio_engine/integration/lua/audio.lua#L49) (line 49)

We return a table of functions instead of polluting the global namespace.
This is idiomatic Lua (similar to Python's "import module" pattern).
local Audio = {}

### Defensive nil check

**Source:** [`tools/audio_authoring/audio_engine/integration/lua/audio.lua`](tools/audio_authoring/audio_engine/integration/lua/audio.lua#L63) (line 63)

If the C++ side never registered the binding (headless server, stub
build), we silently skip rather than crashing the script.
if audio_play_sfx then
audio_play_sfx(event_key)
end
end

### Always return the module table at the end of the file.

**Source:** [`tools/audio_authoring/audio_engine/integration/lua/audio.lua`](tools/audio_authoring/audio_engine/integration/lua/audio.lua#L176) (line 176)

Lua's require() system caches this table and returns it to every caller,
so all scripts share the same Audio instance.
return Audio

### Audio Effects Chain

**Source:** [`tools/audio_authoring/audio_engine/synthesizer/effects.py`](tools/audio_authoring/audio_engine/synthesizer/effects.py#L4) (line 4)

### Soft vs Hard Clipping

**Source:** [`tools/audio_authoring/audio_engine/synthesizer/effects.py`](tools/audio_authoring/audio_engine/synthesizer/effects.py#L127) (line 127)

Hard clipping (np.clip) creates a harsh, digital distortion.
Soft clipping (tanh) smoothly saturates – similar to tube amplifiers.
This creates a warmer, more musical character.
"""
drive = max(1.0, float(drive))
ceiling = float(np.clip(ceiling, 0.0, 1.0))
out = np.tanh(signal.astype(np.float64) * drive) / np.tanh(drive)
return (out * ceiling).astype(np.float32)

### ADSR Envelope

**Source:** [`tools/audio_authoring/audio_engine/synthesizer/envelope.py`](tools/audio_authoring/audio_engine/synthesizer/envelope.py#L4) (line 4)

### Low-Pass Filters

**Source:** [`tools/audio_authoring/audio_engine/synthesizer/filter.py`](tools/audio_authoring/audio_engine/synthesizer/filter.py#L4) (line 4)

### Instrument Design Pattern

**Source:** [`tools/audio_authoring/audio_engine/synthesizer/instrument.py`](tools/audio_authoring/audio_engine/synthesizer/instrument.py#L4) (line 4)

### Flyweight Pattern

**Source:** [`tools/audio_authoring/audio_engine/synthesizer/instrument.py`](tools/audio_authoring/audio_engine/synthesizer/instrument.py#L119) (line 119)

All presets are built on-demand from lightweight parameter sets.
Each call to :meth:`get` creates a new :class:`Instrument` instance.
This avoids shared mutable state between tracks.
"""

### Additive Synthesis

**Source:** [`tools/audio_authoring/audio_engine/synthesizer/oscillator.py`](tools/audio_authoring/audio_engine/synthesizer/oscillator.py#L4) (line 4)

### Band-limiting

**Source:** [`tools/audio_authoring/audio_engine/synthesizer/oscillator.py`](tools/audio_authoring/audio_engine/synthesizer/oscillator.py#L38) (line 38)

To avoid aliasing (unwanted high-frequency artefacts), a real oscillator
filters out harmonics above the Nyquist frequency (sample_rate / 2).
Here we use a simplified poly-BLEP (polynomial Band-Limited Step) approach:
the square/saw are produced via the sine sum up to Nyquist, which is
accurate enough for game audio.
"""

### What to test in a façade class

**Source:** [`tools/audio_authoring/tests/test_audio_engine.py`](tools/audio_authoring/tests/test_audio_engine.py#L4) (line 4)

---

## tools/audio_engine.py

### Audio Pipeline Integration

**Source:** [`tools/audio_engine.py`](tools/audio_engine.py#L6) (line 6)

### Data Class as Value Object

**Source:** [`tools/audio_engine.py`](tools/audio_engine.py#L109) (line 109)

### Factory Method

**Source:** [`tools/audio_engine.py`](tools/audio_engine.py#L163) (line 163)

### Service Class / Facade

**Source:** [`tools/audio_engine.py`](tools/audio_engine.py#L193) (line 193)

### Manifest Emission

**Source:** [`tools/audio_engine.py`](tools/audio_engine.py#L284) (line 284)

### Manifest Consumption

**Source:** [`tools/audio_engine.py`](tools/audio_engine.py#L323) (line 323)

---

## tools/audit_teaching_notes.py

### Why Automate Code-Quality Audits?

**Source:** [`tools/audit_teaching_notes.py`](tools/audit_teaching_notes.py#L6) (line 6)

### Python dataclasses (PEP 557)

**Source:** [`tools/audit_teaching_notes.py`](tools/audit_teaching_notes.py#L52) (line 52)

===========================================================================
@dataclass auto-generates __init__, __repr__, and __eq__ from annotated
class fields.  This removes boilerplate and makes the code self-documenting:
a reader can see all fields at a glance without searching through __init__.

Comparison to namedtuple:
  dataclass  — mutable, supports default values, inheritable.
  namedtuple — immutable, slightly faster, tuple-compatible.
For result aggregation we prefer dataclass for its mutability.
===========================================================================

### Why count distinct note *blocks*, not note *lines*?

**Source:** [`tools/audit_teaching_notes.py`](tools/audit_teaching_notes.py#L97) (line 97)

A teaching note often spans multiple lines::

### Title

**Source:** [`tools/audit_teaching_notes.py`](tools/audit_teaching_notes.py#L101) (line 101)

============================================================
Explanation line 1
Explanation line 2

### Counting distinct note blocks.

**Source:** [`tools/audit_teaching_notes.py`](tools/audit_teaching_notes.py#L147) (line 147)

We increment the counter when we encounter "TEACHING NOTE" for the
first time after a non-note line.  This means a multi-line block
counts as ONE note, not N lines.
has_note_marker = "TEACHING NOTE" in stripped
if has_note_marker and not prev_had_note:
teaching_note_count += 1
prev_had_note = True
elif not has_note_marker:
prev_had_note = False

### Division-by-zero guard

**Source:** [`tools/audit_teaching_notes.py`](tools/audit_teaching_notes.py#L180) (line 180)

A file could have zero code lines (e.g., a header with only macros or
blank lines).  We treat such files as passing to avoid false alarms.
density = (note_count / code_lines * 100.0) if code_lines > 0 else 100.0

### Generator-based file discovery with Path.rglob()

**Source:** [`tools/audit_teaching_notes.py`](tools/audit_teaching_notes.py#L203) (line 203)

``Path.rglob("*.cpp")`` recursively yields all .cpp files under a
directory.  We filter out excluded paths with a simple substring check.
Using generators (yield-based) keeps memory usage low — no need to
materialise the full list of thousands of files.

### Console output formatting

**Source:** [`tools/audit_teaching_notes.py`](tools/audit_teaching_notes.py#L271) (line 271)

We use fixed-width columns aligned with ljust/rjust to make the output
easy to scan in a terminal.  The same technique is used by tools like
pytest, eslint, and clang-tidy for their human-readable output.
"""
bar = "=" * 72
print(bar)
print("  TEACHING NOTE Coverage Audit")
print(bar)
print(f"  Minimum density  : {min_density:.1f} notes per 100 code lines")
print(f"  Files scanned    : {result.total_files}")
print(f"  Total code lines : {result.total_code_lines}")
print(f"  Total notes      : {result.total_notes}")
print(f"  Overall density  : {result.overall_density:.2f}")
print()

### argparse

**Source:** [`tools/audit_teaching_notes.py`](tools/audit_teaching_notes.py#L359) (line 359)

argparse is Python's standard command-line argument library.  It auto-
generates --help text from the descriptions you provide and handles
type conversion, defaults, and validation.  This is preferable to
manually parsing sys.argv because it is robust and self-documenting.
"""
p = argparse.ArgumentParser(
prog="audit_teaching_notes.py",
description="Audit TEACHING NOTE comment coverage in C++ source files.",
)
p.add_argument(
"--min-density",
type=float,
default=0.5,
metavar="FLOAT",
help="Minimum TEACHING NOTEs per 100 code lines per file (default: 0.5).",
)
p.add_argument(
"--fail-below",
type=float,
default=0.3,
metavar="FLOAT",
help=(
"Exit with code 1 if the overall project density drops below "
"this value (default: 0.3).  Use 0 to never fail."
),
)
p.add_argument(
"--paths",
nargs="+",
default=["src", "editor"],
metavar="PATH",
help="Directories to scan (default: src editor).",
)
p.add_argument(
"--ext",
nargs="+",
default=[".cpp", ".hpp", ".h"],
metavar="EXT",
help="File extensions to include (default: .cpp .hpp .h).",
)
p.add_argument(
"--exclude",
nargs="*",
default=["build/", "build-", ".git/", "third_party/", "vcpkg/"],
metavar="FRAGMENT",
help="Path sub-strings that cause a file to be skipped.",
)
p.add_argument(
"--report",
choices=["text", "json"],
default="text",
help="Output format: 'text' (default) or 'json'.",
)
p.add_argument(
"--verbose",
"-v",
action="store_true",
help="Show per-file results for all files, not just failures.",
)
return p

### Exit code conventions

**Source:** [`tools/audit_teaching_notes.py`](tools/audit_teaching_notes.py#L449) (line 449)

Unix convention: 0 = success, non-zero = failure.
CI tools (GitHub Actions, Jenkins) check the exit code automatically:
a non-zero exit marks the step as failed and stops the pipeline.
if args.fail_below > 0 and result.overall_density < args.fail_below:
if args.report == "text":
print(
f"\n[FAIL] Overall density {result.overall_density:.2f} is below "
f"the required minimum {args.fail_below:.2f}."
)
return 1

---

## tools/cook

### What is cook.exe?

**Source:** [`src/tools/cook/cook_main.cpp`](src/tools/cook/cook_main.cpp#L6) (line 6)

============================================================================
Every commercial game engine has an "asset cooking" step that transforms
raw source assets (PNG, WAV, FBX, JSON) into compact, engine-optimised
binary files.  cook.exe is our standalone command-line cooker.

Cooking serves three purposes:
 1. Format conversion: PNG → BC7 DDS (GPU-compressed texture);
                       WAV → OGG (smaller audio);
                       FBX → .mesh (engine-native binary mesh).
 2. Validation: ensure all referenced assets exist and are well-formed.
 3. Indexing: build assetdb.json (GUID → cooked path map) for the runtime.

For M2 the cook steps are stubs: we copy the source file to Cooked/ with
the correct naming convention and write assetdb.json.  Real conversion will
be added in M3 (textures) and M4 (audio/animation).

============================================================================

### Cook pipeline design

**Source:** [`src/tools/cook/cook_main.cpp`](src/tools/cook/cook_main.cpp#L24) (line 24)

============================================================================

 Input:  AssetRegistry.json   (produced by editor / cook_assets.py)
         Content/             (raw source assets)

 Output: Cooked/<type>/<name>.<ext>   (cooked assets — content unchanged for M2)
         Cooked/assetdb.json          (GUID → cookedPath index for runtime)

The assetdb.json format is deliberately minimal — it contains only what the
engine runtime needs:
 {
   "version": "1.0.0",
   "assets": [
     { "id": "<uuid>", "cookedPath": "Cooked/<type>/<file>" },
     ...
   ]
 }

============================================================================

### Why a standalone exe instead of a library?

**Source:** [`src/tools/cook/cook_main.cpp`](src/tools/cook/cook_main.cpp#L44) (line 44)

============================================================================
The cooker runs at AUTHOR time (on the developer's machine or in CI), not
at RUNTIME (in the shipped game).  Keeping it as a separate executable:
 • Means the cook logic never ships in the game binary (smaller, safer).
 • Can be run from CI without starting the full engine.
 • Is easy to invoke from Python scripts or shell scripts.
 • Maps exactly to how Unreal's UnrealPak and Unity's BuildAssetBundles work.

============================================================================

Usage:
  cook.exe --project <path/to/project>

Example:
  cook.exe --project samples/vertical_slice_project

Exit codes:
  0  — all assets cooked successfully.
  1  — fatal error (missing registry, malformed JSON, I/O failure).

@author  Educational Game Engine Project
@version 1.0
@date    2024
C++ Standard: C++17

### Using std::filesystem (C++17)

**Source:** [`src/tools/cook/cook_main.cpp`](src/tools/cook/cook_main.cpp#L82) (line 82)

============================================================================
std::filesystem provides cross-platform directory and path manipulation.
Key types:
  fs::path          — a file-system path (can be relative or absolute).
  fs::create_directories(p) — create all directories in the path.
  fs::copy_file(src, dst)   — copy a single file.
  fs::exists(p)             — check if a path exists.
This replaces platform-specific mkdir() / CopyFile() calls, making the
code portable across Windows, Linux, and macOS.
============================================================================
namespace fs = std::filesystem;

### Minimal JSON helpers for cook output

**Source:** [`src/tools/cook/cook_main.cpp`](src/tools/cook/cook_main.cpp#L96) (line 96)

============================================================================
For M2 we hand-write the JSON output rather than using a JSON library.
This is intentional: it teaches students exactly what JSON looks like and
how to produce it programmatically.  The format is simple enough that a
robust hand-written serialiser is fine.

In M3 we will add nlohmann/json via vcpkg for both reading and writing.
============================================================================
namespace
{

### JSON string escaping rules:

**Source:** [`src/tools/cook/cook_main.cpp`](src/tools/cook/cook_main.cpp#L112) (line 112)

\  → \\      (backslash)
  "  → \"      (double quote)
  \n → \n      (newline)
A minimal implementation is fine for file paths and GUIDs, which never
contain special characters.
---------------------------------------------------------------------------
static std::string json_escape(const std::string& s)
{
std::string out;
out.reserve(s.size());
for (char c : s)
{
switch (c)
{
case '"':  out += "\\\""; break;
case '\\': out += "\\\\"; break;
case '\n': out += "\\n";  break;
case '\r': out += "\\r";  break;
case '\t': out += "\\t";  break;
default:   out += c;      break;
}
}
return out;
}

### Simple JSON parsing with braces tracking

**Source:** [`src/tools/cook/cook_main.cpp`](src/tools/cook/cook_main.cpp#L180) (line 180)

The registry contains an array of objects.  We extract each object using
brace-depth tracking (same technique used in AssetDB::Load), then pull out
the fields we need.  This is a teaching-grade parser; production code would
use nlohmann/json (M3+).
---------------------------------------------------------------------------
static std::vector<AssetEntry> parse_asset_registry(const fs::path& registryPath)
{
std::ifstream f(registryPath);
if (!f.is_open())
{
std::cerr << "[cook] ERROR: Cannot open AssetRegistry.json: "
<< registryPath << "\n";
return {};
}

### Main function structure for a tool

**Source:** [`src/tools/cook/cook_main.cpp`](src/tools/cook/cook_main.cpp#L245) (line 245)

============================================================================
A well-structured command-line tool main() does:
  1. Parse arguments — determine what to do.
  2. Validate inputs — fail fast with a clear error if something is wrong.
  3. Do work — iterate assets, copy/convert, write output.
  4. Report results — print a summary so CI can understand what happened.
  5. Return 0 on success, non-zero on any error — so CI scripts can check.
============================================================================
int main(int argc, char* argv[])
{
-----------------------------------------------------------------------
Step 1 — Argument parsing.
-----------------------------------------------------------------------
std::string projectDir;
bool        verbose = false;

### Incremental cooking (future enhancement)

**Source:** [`src/tools/cook/cook_main.cpp`](src/tools/cook/cook_main.cpp#L341) (line 341)

A real cooker checks the hash of the source file against the stored
hash in the registry.  If they match, the asset is up-to-date and the
cook step is skipped.  For M2 we always recook (unconditional copy).
Incremental cooking will be introduced when we add the real conversion
steps in M3.
-----------------------------------------------------------------------
int  cooked  = 0;
int  skipped = 0;
int  errors  = 0;

### Source path resolution strategy

**Source:** [`src/tools/cook/cook_main.cpp`](src/tools/cook/cook_main.cpp#L359) (line 359)

AssetRegistry.json stores the source path in two possible conventions:
  1. Relative to the project root (e.g., "Content/Maps/file.json")
  2. Relative to the Content/ directory (e.g., "Maps/file.json")
We try both conventions so cook.exe works with registries generated
by the Python cook_assets.py script (convention 2) and future
editor-generated registries (convention 1).
fs::path srcPath;

### Derived cooked path

**Source:** [`src/tools/cook/cook_main.cpp`](src/tools/cook/cook_main.cpp#L404) (line 404)

When no cooked path is specified, we place the asset under
Cooked/<AssetType>/<filename>.  This mirrors Unreal Engine's
cooked content layout where each asset type has its own folder.
dstPath = cookedDir / entry.type / srcPath.filename();
}

### Why a separate assetdb.json and not just use the registry?

**Source:** [`src/tools/cook/cook_main.cpp`](src/tools/cook/cook_main.cpp#L456) (line 456)

AssetRegistry.json can be very large in a real project (hundreds of
fields per asset).  assetdb.json is a stripped-down runtime index:
only the GUID and cooked path are needed.  This keeps engine startup
fast and reduces memory usage.
-----------------------------------------------------------------------
const fs::path assetDbPath = cookedDir / "assetdb.json";

---

## tools/creation_engine.py

### Creation Pipeline Integration

**Source:** [`tools/creation_engine.py`](tools/creation_engine.py#L6) (line 6)

### Generic Asset Record

**Source:** [`tools/creation_engine.py`](tools/creation_engine.py#L117) (line 117)

### Creation Engine as a Service Facade

**Source:** [`tools/creation_engine.py`](tools/creation_engine.py#L179) (line 179)

### Manifest Emission from the Creation Engine

**Source:** [`tools/creation_engine.py`](tools/creation_engine.py#L260) (line 260)

### Manifest Consumption in the Creation Engine

**Source:** [`tools/creation_engine.py`](tools/creation_engine.py#L296) (line 296)

---

## tools/tests

### Testing the Asset Pipeline Validator

**Source:** [`tools/tests/test_validate_assets.py`](tools/tests/test_validate_assets.py#L6) (line 6)

### importing a module whose filename contains a hyphen

**Source:** [`tools/tests/test_validate_assets.py`](tools/tests/test_validate_assets.py#L46) (line 46)

Python identifiers cannot contain hyphens, so the normal `import` statement
does not work for validate-assets.py.  importlib.util.spec_from_file_location
loads the file directly and registers it under an alias.
_spec = _ilu.spec_from_file_location(
"validate_assets", _TOOLS_DIR / "validate-assets.py"
)
va = _ilu.module_from_spec(_spec)  # type: ignore[arg-type]
_spec.loader.exec_module(va)  # type: ignore[union-attr]

### JSON Schema draft-07 cannot express cross-property

**Source:** [`tools/tests/test_validate_assets.py`](tools/tests/test_validate_assets.py#L193) (line 193)

numeric comparisons (loopEnd > loopStart).  That constraint lives in
the built-in structural checker.  We call it directly here so the test
is deterministic regardless of whether jsonschema is installed.
manifest = _load_json(_FIXTURES_INVALID / "bad-loop-points.json")
schema = _load_schema()
errors = va._validate_manifest_builtin(manifest, schema)
self.assertGreater(
len(errors), 0,
"bad-loop-points manifest should produce built-in validation errors",
)
self.assertTrue(
any("loop" in e.lower() for e in errors),
f"Expected loop-point error from built-in checker, got: {errors}",
)

---

## tools/validate-assets.py

### Asset Pipeline Validation

**Source:** [`tools/validate-assets.py`](tools/validate-assets.py#L6) (line 6)

### type narrowing: if an extension block exists we validate

**Source:** [`tools/validate-assets.py`](tools/validate-assets.py#L209) (line 209)

it; we only *warn* (not error) if the extension is absent, because some
pipeline steps add it lazily.
if asset_type == "audio":
if ext is None:
return [f"{prefix}: missing 'audio' extension block (required for type='audio')"]
for f in ("format", "sampleRate", "channels", "durationSeconds"):
if f not in ext:
errors.append(f"{prefix}.audio: missing required field '{f}'")
if ext.get("format") not in _AUDIO_FORMATS:
errors.append(
f"{prefix}.audio.format: '{ext.get('format')}' not in {sorted(_AUDIO_FORMATS)}"
)
for num_field in ("sampleRate", "channels"):
v = ext.get(num_field)
if v is not None and (not isinstance(v, int) or v < 1):
errors.append(f"{prefix}.audio.{num_field}: must be a positive integer")
v = ext.get("durationSeconds")
if v is not None and (not isinstance(v, (int, float)) or v < 0):
errors.append(f"{prefix}.audio.durationSeconds: must be >= 0")
cat = ext.get("category")
if cat is not None and cat not in _AUDIO_CATEGORIES:
errors.append(
f"{prefix}.audio.category: '{cat}' not in {sorted(_AUDIO_CATEGORIES)}"
)
loop point consistency
loop_start = ext.get("loopStart")
loop_end = ext.get("loopEnd")
if loop_start is not None and loop_end is not None:
if loop_end <= loop_start:
errors.append(
f"{prefix}.audio: loopEnd ({loop_end}) must be > loopStart ({loop_start})"
)

---
