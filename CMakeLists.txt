# ===========================================================================
# CMakeLists.txt — Game Engine for Teaching (Monorepo Superbuild)
# ===========================================================================
#
# TEACHING NOTE — Superbuild / Monorepo Pattern
# ──────────────────────────────────────────────
# A "superbuild" root CMakeLists.txt orchestrates multiple sub-projects in a
# single CMake invocation. This lets you build the engine, editor, and any
# C++ tools all at once, yet keep each subproject's build logic isolated in
# its own subdirectory.
#
# Monorepo layout:
#   src/      — C++ engine & game source code (existing, always built)
#   editor/   — Qt 6 Widgets editor (optional; requires Qt installed)
#   shared/   — Shared headers and JSON schemas used by all projects
#
# TEACHING NOTE — CMake Build System Design
# ─────────────────────────────────────────
# CMake is the de-facto standard build-system generator for C++ projects.
# It does NOT compile code directly — it generates:
#   • Makefiles    (Linux / macOS)
#   • Xcode projects (macOS)
#   • Visual Studio solutions (Windows)  ← our primary target
# …from this platform-neutral description.
#
# Key CMake concepts used here:
#   target_sources      — list of .cpp files to compile.
#   target_include_dirs — where the compiler looks for #include files.
#   target_link_libs    — external libraries to link against.
#   target_compile_opts — compiler warning/optimisation flags.
#
# TEACHING NOTE — Out-of-Source Builds (Windows recommended)
# ──────────────────────────────────────────────────────────
# Use CMake Presets (CMakePresets.json) for the recommended workflow:
#
#   cmake --preset windows-debug
#   cmake --build --preset windows-debug
#
# Or the manual workflow:
#   mkdir build && cd build
#   cmake .. -G "Visual Studio 17 2022" -A x64
#   cmake --build . --config Debug --target engine_sandbox
#

cmake_minimum_required(VERSION 3.16)
project(EducationalGameEngine VERSION 1.0 LANGUAGES CXX C)

# ---------------------------------------------------------------------------
# Git SHA version stamping (AP-4)
# ---------------------------------------------------------------------------
# TEACHING NOTE — Embedding the Build Commit SHA
# ─────────────────────────────────────────────────────────────────────────────
# Post-crash reproduction requires knowing *which* build a dump came from.
# We embed the short git SHA at CMake configure time so it is compiled into
# the binary as a string literal.  The SHA is exposed via:
#
#   engine_sandbox.exe --version   →  "engine_sandbox abc1234"
#   LOG_INFO at startup            →  "[Init] Build: abc1234"
#
# How it works:
#   1. find_package(Git QUIET) locates git on PATH.
#   2. execute_process() runs 'git rev-parse --short HEAD' at configure time.
#   3. The result is stored in ENGINE_GIT_SHA.
#   4. The macro is forwarded to engine_sandbox via target_compile_definitions.
# ─────────────────────────────────────────────────────────────────────────────
find_package(Git QUIET)
if(Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE ENGINE_GIT_SHA
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
if(NOT ENGINE_GIT_SHA)
    set(ENGINE_GIT_SHA "unknown")
endif()
message(STATUS "Engine build SHA : ${ENGINE_GIT_SHA}")

# ---------------------------------------------------------------------------
# Monorepo top-level option: Build the Dear ImGui editor
# ---------------------------------------------------------------------------
# TEACHING NOTE -- Optional Subprojects
# The editor uses Dear ImGui (MIT, zero install) rendered via D3D11.
# It requires vcpkg to install the imgui[docking,dx11-binding,win32-binding]
# package (listed in vcpkg.json).  The engine builds and runs without it.
# Set BUILD_EDITOR=ON to include the editor:
#
#   cmake --preset windows-debug -DBUILD_EDITOR=ON
#
option(BUILD_EDITOR "Build the Dear ImGui Creation Suite editor" OFF)

# ---------------------------------------------------------------------------
# Top-level option: Build the Demo_Game standalone executable
# ---------------------------------------------------------------------------
# TEACHING NOTE — Demo_Game Executable
# ─────────────────────────────────────────────────────────────────────────────
# Demo_Game is the player-facing binary built from src/demo_game/demo_main.cpp.
# It reuses all engine + gameplay sources from engine_sandbox but starts
# directly in the open-world game mode with a boot menu.
#
# Enable it with:
#   cmake --preset windows-ninja-debug-demo
#   cmake --preset windows-debug-engine-only -DBUILD_DEMO_GAME=ON
#
# Build target: demo_game
#   demo_game.exe --headless           # CI validation (no window)
#   demo_game.exe                      # Full windowed open world
# ─────────────────────────────────────────────────────────────────────────────
option(BUILD_DEMO_GAME "Build the Demo_Game standalone open-world executable" OFF)

# ---------------------------------------------------------------------------
# C++ standard
# ---------------------------------------------------------------------------
# TEACHING NOTE — C++17 is required for:
#   • if constexpr             (used in LuaEngine::SetGlobal<T>)
#   • std::optional            (future use)
#   • Structured bindings      (auto [k, v] = ...)
#   • Fold expressions         (World::View<Components...>)
set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

# ---------------------------------------------------------------------------
# Compiler warnings
# ---------------------------------------------------------------------------
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    # Suppress noisy pedantic warnings from Lua C headers.
    add_compile_options(-Wno-deprecated-declarations)
endif()

# ---------------------------------------------------------------------------
# Build type
# ---------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ---------------------------------------------------------------------------
# CMake options — platform feature gates
# ---------------------------------------------------------------------------
# TEACHING NOTE — CMake Options
# option() declares a boolean flag the user can set on the cmake command line:
#   cmake .. -DENGINE_ENABLE_TERMINAL=OFF
# The second argument is a help string shown by cmake-gui and ccmake.
# The third argument is the default value.
#
# We gate the terminal (ncurses) renderer to non-Windows platforms because:
#   1. ncurses is a Linux/macOS library; there is no first-class port on MSVC.
#   2. On Windows the engine uses the Vulkan renderer instead.
# ---------------------------------------------------------------------------

# TEACHING NOTE — ENGINE_ENABLE_TERMINAL
# Defaults to ON everywhere EXCEPT Windows (where ncurses is unavailable).
if(WIN32)
    option(ENGINE_ENABLE_TERMINAL "Build the ncurses terminal renderer target" OFF)
else()
    option(ENGINE_ENABLE_TERMINAL "Build the ncurses terminal renderer target" ON)
endif()

# ---------------------------------------------------------------------------
# TEACHING NOTE — ENGINE_ENABLE_D3D11 (default ON on Windows)
# ---------------------------------------------------------------------------
# Direct3D 11 is the *default* Windows renderer.  It ships with Windows 7+
# as part of the OS — no SDK download or separate install required.
# It supports hardware as old as a GeForce GT 610 (Feature Level 11_0) via the
# hardware driver, and any machine/CI runner via D3D11 WARP (software).
#
# This is the recommended baseline for development and CI because:
#   • cmake --preset windows-debug-engine-only works WITHOUT a Vulkan SDK.
#   • engine_sandbox.exe --headless exits 0 on GitHub-hosted runners (WARP).
#   • GT610-era hardware can run the engine at full feature level.
# ---------------------------------------------------------------------------
if(WIN32)
    option(ENGINE_ENABLE_D3D11 "Build the D3D11 renderer backend (GT610-compatible)" ON)
else()
    option(ENGINE_ENABLE_D3D11 "Build the D3D11 renderer backend (GT610-compatible)" OFF)
endif()

# ---------------------------------------------------------------------------
# TEACHING NOTE — ENGINE_ENABLE_VULKAN (optional, high-end path)
# ---------------------------------------------------------------------------
# Vulkan is the *high-end / modern* renderer backend.  It is built when ON
# but is NOT the default runtime renderer — users must pass --renderer vulkan
# explicitly to use it.
#
# IMPORTANT: Vulkan SDK is OPTIONAL.  If ENGINE_ENABLE_VULKAN=ON but the
# Vulkan SDK is not installed, CMake will log a warning and automatically
# disable Vulkan rather than failing the entire configure.  This ensures
# that the D3D11 default path always builds even without a Vulkan SDK.
#
# To build with Vulkan:
#   1. Install the Vulkan SDK from https://vulkan.lunarg.com/
#   2. cmake --preset windows-debug -DENGINE_ENABLE_VULKAN=ON
# ---------------------------------------------------------------------------
if(WIN32)
    option(ENGINE_ENABLE_VULKAN "Build the Vulkan renderer backend (high-end optional)" ON)
else()
    option(ENGINE_ENABLE_VULKAN "Build the Vulkan renderer backend (high-end optional)" OFF)
endif()

# ---------------------------------------------------------------------------
# TEACHING NOTE — ENGINE_ENABLE_PHYSICS (M5 Jolt Physics)
# ---------------------------------------------------------------------------
# Jolt Physics (MIT licence, by Jorrit Rouwe) is a high-performance physics
# library used in Horizon: Forbidden West.  It is installed via vcpkg:
#
#   vcpkg install joltphysics
#
# When ENGINE_ENABLE_PHYSICS=ON and JoltPhysics is found (via vcpkg toolchain
# or a manual installation), the physics subsystem is compiled into
# engine_sandbox and the ENGINE_ENABLE_PHYSICS preprocessor macro is defined.
#
# If JoltPhysics is NOT found, the option silently falls back to OFF so the
# D3D11 baseline build always succeeds — even without Jolt installed.
# This mirrors the Vulkan optional pattern used above.
#
# Acceptance test: build with -DENGINE_ENABLE_PHYSICS=ON then run:
#   engine_sandbox.exe --headless --scene physics_test
# Expected output: "[PASS] physics_test: all 3 acceptance tests passed."
# ---------------------------------------------------------------------------
option(ENGINE_ENABLE_PHYSICS "Build Jolt Physics integration (M5)" ON)

if(ENGINE_ENABLE_PHYSICS)
    # TEACHING NOTE — find_package(Jolt CONFIG)
    # The vcpkg port for joltphysics (v5+) provides a CMake config that
    # registers the imported target Jolt::Jolt.  The CMake package name is
    # "Jolt" (not "JoltPhysics") — confirmed by the vcpkg install message:
    #   "find_package(Jolt CONFIG REQUIRED)"
    # QUIET suppresses the "not found" message when Jolt is not installed
    # (we handle it ourselves below).
    find_package(Jolt CONFIG QUIET)
    if(Jolt_FOUND)
        message(STATUS "Jolt (JoltPhysics) found: physics subsystem ENABLED.")
    else()
        message(STATUS "Jolt (JoltPhysics) NOT found — ENGINE_ENABLE_PHYSICS will be OFF. "
                       "Install via: vcpkg install joltphysics")
        set(ENGINE_ENABLE_PHYSICS OFF CACHE BOOL "" FORCE)
    endif()
endif()

# ---------------------------------------------------------------------------
# nlohmann/json — optional for engine_sandbox (M6 scene serialiser)
# ---------------------------------------------------------------------------
# TEACHING NOTE — nlohmann/json in the engine
# nlohmann-json is already in vcpkg.json (required by cook.exe and the editor).
# When the vcpkg toolchain is active, find_package(nlohmann_json) will find it
# automatically.  We use QUIET so a missing package is not an error — the
# scene_serialiser will compile out its JSON code via #ifdef ENGINE_ENABLE_JSON.
#
# Why not REQUIRED?  The engine_sandbox must build on any Windows machine, even
# without a vcpkg toolchain configured.  nlohmann/json is a "nice to have" for
# runtime scene loading; the engine works without it.
# ---------------------------------------------------------------------------
find_package(nlohmann_json CONFIG QUIET)
if(nlohmann_json_FOUND)
    message(STATUS "nlohmann/json found: ENGINE_ENABLE_JSON ENABLED (M6 scene serialiser).")
else()
    message(STATUS "nlohmann/json NOT found — scene_serialiser JSON code compiled out. "
                   "Install via vcpkg or pass -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake")
endif()

# ---------------------------------------------------------------------------
# Find required system libraries (platform-gated)
# ---------------------------------------------------------------------------

# ---- ncurses (terminal renderer — non-Windows only) -----------------------
# TEACHING NOTE — find_package(Curses) sets:
#   CURSES_LIBRARIES    — ncurses link flags
#   CURSES_INCLUDE_DIRS — header directory (usually /usr/include)
#
# We guard this with ENGINE_ENABLE_TERMINAL so that a Windows CMake configure
# does NOT fail on a missing ncurses package.
if(ENGINE_ENABLE_TERMINAL)
    find_package(Curses REQUIRED)
    message(STATUS "ncurses found: ${CURSES_LIBRARIES}")
endif()

# ---- Lua 5.5 (scripting) — build from bundled source or fall back to system --
# TEACHING NOTE — Build Lua from source (preferred)
# ──────────────────────────────────────────────────
# Lua is a small, self-contained library (~30 .c files).  Building it from the
# bundled source in  Lua/lua-5.5.0/src/  has several advantages over linking
# against a pre-built system library:
#
#   • Zero external dependencies — works on any platform, in any CI, with any
#     compiler.  No "sudo apt install liblua5.4-dev" step required.
#   • Consistent version — everyone builds against Lua 5.5.0 regardless of what
#     is installed on their machine.
#   • Transparent — students can read the Lua source alongside the engine source.
#
# Detection order:
#   1. Bundled source in Lua/lua-5.5.0/src/   ← preferred (always present)
#   2. System Lua 5.5 via pkg-config           ← fallback for distros
#   3. System Lua 5.4 via pkg-config           ← further fallback
#   4. Scripting disabled (warning only)       ← never a hard error
# ---------------------------------------------------------------------------

set(LUA_BUNDLED    OFF)
set(LUA_LIBRARIES  "")
set(LUA_INCLUDE_DIRS "")

set(_LUA_SRC_DIR "${CMAKE_SOURCE_DIR}/Lua/lua-5.5.0/src")

if(EXISTS "${_LUA_SRC_DIR}/lua.h")
    # -----------------------------------------------------------------------
    # TEACHING NOTE — Building a static library from C source in CMake
    # -----------------------------------------------------------------------
    # add_library(target STATIC files…) compiles the listed .c files and
    # archives them into a single .a / .lib file.  Any target that links
    # against lua55_static will automatically get the include directories via
    # the PUBLIC target_include_directories() call below — no manual
    # include_directories() needed at the call-site.
    # -----------------------------------------------------------------------

    # Collect every .c file in the Lua source tree.
    file(GLOB _LUA_ALL_C "${_LUA_SRC_DIR}/*.c")

    # Remove the two standalone tool entry points — these define their own
    # main() and are NOT part of the embeddable library:
    #   lua.c   — the interactive REPL / script runner
    #   luac.c  — the byte-code compiler
    list(REMOVE_ITEM _LUA_ALL_C
        "${_LUA_SRC_DIR}/lua.c"
        "${_LUA_SRC_DIR}/luac.c"
    )

    add_library(lua55_static STATIC ${_LUA_ALL_C})

    # PUBLIC so any target that links lua55_static inherits the include dir.
    target_include_directories(lua55_static PUBLIC "${_LUA_SRC_DIR}")

    # TEACHING NOTE — Platform compile flags for Lua
    # On POSIX (Linux/macOS) Lua needs _GNU_SOURCE for POSIX math functions and
    # popen().  On Windows MSVC we disable several noisy warnings that come from
    # Lua's intentional C idioms (e.g. condition is always true for integer type).
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

    set(LUA_BUNDLED    ON)
    set(LUA_LIBRARIES  lua55_static)
    set(LUA_INCLUDE_DIRS "${_LUA_SRC_DIR}")
    message(STATUS "Lua 5.5: building from bundled source at ${_LUA_SRC_DIR}")

else()
    # Bundled source not present — try system-wide installations.
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(LUA QUIET lua5.5)
        if(NOT LUA_FOUND)
            pkg_check_modules(LUA QUIET lua5.4)
        endif()
    endif()

    if(NOT LUA_FOUND)
        # Manual fallback search.
        find_path(LUA_INCLUDE_DIRS lua.h
            PATHS /usr/include/lua5.5 /usr/include/lua5.4
                  /usr/local/include/lua5.5 /usr/local/include/lua5.4
                  /usr/include/lua /usr/local/include/lua)
        find_library(LUA_LIBRARIES
            NAMES lua5.5 lua55 lua5.4 lua54 lua
            PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu)
    endif()

    if(LUA_LIBRARIES AND LUA_INCLUDE_DIRS)
        message(STATUS "Lua: system library found: ${LUA_LIBRARIES}")
    else()
        message(STATUS "Lua not found — scripting will be disabled.")
        message(STATUS "  Bundled source missing from Lua/lua-5.5.0/ (expected after git pull)")
        set(LUA_LIBRARIES  "")
        set(LUA_INCLUDE_DIRS "")
    endif()
endif()

# ---- Vulkan SDK (optional high-end renderer) --------------------------------
# TEACHING NOTE — find_package(Vulkan QUIET) — soft failure
# We use QUIET (no error message on missing SDK) and check Vulkan_FOUND
# manually.  If the SDK is absent we disable Vulkan and log a warning so
# the configure still succeeds and D3D11 can build.
#
# This is the idiomatic "optional dependency" pattern in CMake:
#   find_package(X QUIET)
#   if(NOT X_FOUND)
#       message(WARNING "X not found; feature Y disabled.")
#       set(ENGINE_ENABLE_X OFF CACHE BOOL "" FORCE)
#   endif()
#
# The Vulkan SDK sets the VULKAN_SDK environment variable.  CMake's built-in
# FindVulkan module uses that variable to locate vulkan-1.lib and vulkan/vulkan.h.
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

# ---------------------------------------------------------------------------
# Engine sources (shared kernel — platform-independent)
# ---------------------------------------------------------------------------

set(ENGINE_CORE_SOURCES
    src/engine/core/Logger.cpp
    src/engine/scripting/LuaEngine.cpp
    src/engine/assets/asset_db.cpp
    src/engine/assets/asset_loader.cpp
)

# ---------------------------------------------------------------------------
# Terminal game target (Linux / macOS — existing target, unchanged)
# ---------------------------------------------------------------------------
# TEACHING NOTE — Conditional Target Creation
# add_executable() is only called when ENGINE_ENABLE_TERMINAL is ON.
# On Windows this block is entirely skipped so MSVC never tries to compile
# the ncurses-dependent Renderer.cpp and InputSystem.cpp.
# ---------------------------------------------------------------------------
if(ENGINE_ENABLE_TERMINAL)

    set(TERMINAL_ENGINE_SOURCES
        src/engine/rendering/Renderer.cpp
        src/engine/input/InputSystem.cpp
    )

    set(GAME_SOURCES
        src/game/world/TileMap.cpp
        src/game/world/Zone.cpp
        src/game/world/WorldMap.cpp
        src/game/systems/CombatSystem.cpp
        src/game/systems/InventorySystem.cpp
        src/game/systems/QuestSystem.cpp
        src/game/systems/MagicSystem.cpp
        src/game/systems/ShopSystem.cpp
        src/game/systems/CampSystem.cpp
        src/game/systems/WeatherSystem.cpp
        src/game/systems/AISystem.cpp
        # TEACHING NOTE — Post-M10 Behaviour Tree AI (terminal game).
        # The engine/ai/ layer is pure C++17 with no rendering or platform
        # dependencies — it builds on Linux (ncurses game) and Windows alike.
        src/engine/ai/behaviour_tree.cpp
        src/engine/ai/formation_system.cpp
        src/engine/ai/nav_mesh.cpp
        # TEACHING NOTE — M19 Action Combat: ComboSystem FSM (terminal game).
        # The ComboSystem is renderer-agnostic and builds on Linux unchanged.
        src/engine/combat/combo_system.cpp
        src/game/Game.cpp
    )

    add_executable(game
        src/main.cpp
        ${ENGINE_CORE_SOURCES}
        ${TERMINAL_ENGINE_SOURCES}
        ${GAME_SOURCES}
    )

    target_include_directories(game PRIVATE
        src/
        ${CURSES_INCLUDE_DIRS}
        ${LUA_INCLUDE_DIRS}
    )

    target_link_libraries(game PRIVATE
        ${CURSES_LIBRARIES}
        ${LUA_LIBRARIES}
    )

    # TEACHING NOTE — ENGINE_ENABLE_LUA compile definition
    # The terminal game links against Lua 5.5 (built from bundled source or
    # found as a system package).  ENGINE_ENABLE_LUA activates the Lua
    # scripting hooks in CombatSystem.cpp and CampSystem.cpp.  When Lua is not
    # found (unlikely given the bundled source) the hooks compile out cleanly.
    if(LUA_LIBRARIES)
        target_compile_definitions(game PRIVATE ENGINE_ENABLE_LUA)
    endif()

    # Copy Lua scripts to build directory after build.
    add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/scripts
            $<TARGET_FILE_DIR:game>/scripts
        COMMENT "Copying Lua scripts to build directory."
    )

endif() # ENGINE_ENABLE_TERMINAL

# ---------------------------------------------------------------------------
# engine_sandbox target (Windows — D3D11 baseline + optional Vulkan)
# ---------------------------------------------------------------------------
# TEACHING NOTE — engine_sandbox Rendering Strategy
# ─────────────────────────────────────────────────
# engine_sandbox supports two rendering backends selectable at runtime:
#
#   D3D11  (default) — Direct3D 11; GT610-compatible; ships with Windows.
#                      Uses WARP software rasteriser for headless CI runs.
#                      Built when ENGINE_ENABLE_D3D11=ON (default on Windows).
#
#   Vulkan (optional) — Vulkan 1.0+; modern / high-end.
#                       Requires a Vulkan ICD at runtime.
#                       Built when ENGINE_ENABLE_VULKAN=ON AND SDK is present.
#
# The D3D11 path is the primary target because:
#   • It builds without any external SDK (d3d11.lib ships with the Windows SDK).
#   • It runs on CI runners via D3D11 WARP (no GPU required).
#   • It supports GT610-era hardware (D3D_FEATURE_LEVEL_10_0 minimum).
#
# Build commands (D3D11, no Vulkan SDK needed):
#   cmake --preset windows-debug-engine-only
#   cmake --build --preset windows-debug-engine-only --target engine_sandbox
# ---------------------------------------------------------------------------

# Only build engine_sandbox on Windows (it needs Win32 + a graphics API).
if(WIN32 AND (ENGINE_ENABLE_D3D11 OR ENGINE_ENABLE_VULKAN))

    # -----------------------------------------------------------------------
    # Collect sources — always include the platform layer and core.
    # -----------------------------------------------------------------------
    set(SANDBOX_SOURCES
        src/sandbox/main.cpp
        src/engine/platform/win32/Win32Window.cpp
        src/engine/core/Logger.cpp
        src/engine/core/engine_config.cpp
        src/engine/assets/asset_db.cpp
        src/engine/assets/asset_loader.cpp
    )

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    # TEACHING NOTE — Animation Runtime Sources (M4)
    # -----------------------------------------------------------------------
    # The animation runtime is renderer-agnostic: it runs on both D3D11 and
    # Vulkan paths.  We add it to SANDBOX_SOURCES unconditionally (still
    # inside the WIN32 guard since the sandbox is Windows-only).
    # -----------------------------------------------------------------------
    list(APPEND SANDBOX_SOURCES
        src/engine/animation/skeleton.cpp
        src/engine/animation/anim_clip.cpp
        src/engine/animation/blend_tree.cpp
        src/engine/animation/animation_system.cpp
        # TEACHING NOTE — M4b: IK solver (renderer-agnostic, pure C++17).
        # Two-Bone analytical IK and FABRIK iterative N-joint IK.
        # Lives in animation/ alongside the other CPU-side animation systems.
        src/engine/animation/ik_solver.cpp
    )

    # -----------------------------------------------------------------------
    # TEACHING NOTE — Behaviour Tree AI Sources (Post-M10)
    # -----------------------------------------------------------------------
    # The engine/ai/ layer provides THREE complementary AI systems:
    #
    #   behaviour_tree.cpp — BT framework (BtSequence, BtSelector, BtCondition,
    #     BtAction, BtBlackboard, BtTree).  Zero external dependencies.
    #
    #   formation_system.cpp — FormationSystem (LINE / V_SHAPE / CIRCLE slot
    #     layouts).  Depends on ECS (TransformComponent) but NOT on game/.
    #
    #   nav_mesh.cpp — Grid NavMesh (A* on a walkability grid).  Replaces
    #     TileMap dependency for engine-layer pathfinding.
    #
    # All three files are pure C++17 with no platform or rendering deps;
    # they compile on both Windows (SANDBOX) and Linux (game terminal) alike.
    # -----------------------------------------------------------------------
    list(APPEND SANDBOX_SOURCES
        src/engine/ai/behaviour_tree.cpp
        src/engine/ai/formation_system.cpp
        src/engine/ai/nav_mesh.cpp
    )

    # -----------------------------------------------------------------------
    # TEACHING NOTE — Cinematics Sources (Post-M10)
    # -----------------------------------------------------------------------
    # The engine/cinematics/ layer provides TWO subsystems for cut-scene
    # production:
    #
    #   camera_rig.cpp — Keyframed camera path.  Stores an ordered list of
    #     CameraKeyframes (position, look-at, FOV, time) and evaluates the
    #     interpolated camera state at any shot-local time using binary search
    #     + Vec3::Lerp.  Zero platform or rendering dependencies.
    #
    #   cinematic_sequencer.cpp — Shot/cut orchestrator.  Holds an ordered
    #     list of (CameraRig, duration) pairs, advances time each frame via
    #     Tick(dt), fires OnShotChanged / OnComplete callbacks, and pushes the
    #     interpolated camera state into the active ECS CameraComponent via
    #     ApplyToCamera(World&) — setting the new cinematicOverride flag so
    #     CameraSystem uses the cinematic eye position instead of orbit math.
    #
    # Both files are pure C++17 (no D3D11 or Vulkan dependency); they are
    # always included in the sandbox build so --scene cinematic_test is
    # available in every configuration.
    # -----------------------------------------------------------------------
    list(APPEND SANDBOX_SOURCES
        src/engine/cinematics/camera_rig.cpp
        src/engine/cinematics/cinematic_sequencer.cpp
    )


    # D3D11 backend sources (included when ENGINE_ENABLE_D3D11=ON).
    # -----------------------------------------------------------------------
    # TEACHING NOTE — Conditional Source Files
    # We add D3D11Renderer.cpp only when the D3D11 feature is enabled.
    # This keeps the source list explicit and makes it easy to see which
    # files belong to which backend.
    # -----------------------------------------------------------------------
    if(ENGINE_ENABLE_D3D11)
        list(APPEND SANDBOX_SOURCES
            src/engine/rendering/d3d11/D3D11Renderer.cpp
            # TEACHING NOTE — M3: D3D11 texture loader (DDS/BC7 → ID3D11SRV).
            # d3d11_texture.cpp is a self-contained DDS parser that uses only
            # the Windows SDK headers already required by D3D11Renderer.
            src/engine/rendering/d3d11/d3d11_texture.cpp
            # TEACHING NOTE — M4b: GpuSkinningBuffer — D3D11 DYNAMIC constant
            # buffer that uploads 64 joint matrices (4096 bytes) to the VS
            # every frame.  Uses Map/WRITE_DISCARD for zero-stall streaming.
            src/engine/animation/gpu_skinning.cpp
            # TEACHING NOTE — M10: SkyRenderer + WeatherFx are renderer-agnostic
            # CPU objects that compute time-of-day sky colours and weather state.
            # They are compiled alongside D3D11Renderer because D3D11Renderer
            # owns a SkyRenderer member (m_skyRenderer).  A future Vulkan backend
            # would also include these files without modification.
            src/engine/rendering/sky_renderer.cpp
            src/engine/rendering/weather_fx.cpp
            # TEACHING NOTE — Post-M10: SDF FontRenderer (D3D11 text rendering).
            # font_renderer.cpp builds a CPU-side signed distance field atlas from
            # an embedded 8×8 bitmap font, uploads it as an R8_UNORM D3D11 texture,
            # and renders screen-space text quads using sdf_text.vs/ps.hlsl.
            # Gated on ENGINE_ENABLE_D3D11 because it uses D3D11 COM types.
            src/engine/ui/font_renderer.cpp
            # TEACHING NOTE — M25: TerrainRenderer (heightmap-driven grid mesh).
            # terrain_renderer.cpp generates a W×H vertex/index grid from float
            # height samples, compiles terrain.vs/ps.hlsl at runtime via
            # D3DCompileFromFile, and renders the grid each frame.  Gated on
            # ENGINE_ENABLE_D3D11 because it uses D3D11 COM types directly.
            src/engine/rendering/d3d11/terrain_renderer.cpp
        )
    endif()

    # -----------------------------------------------------------------------
    # Vulkan backend sources (included when ENGINE_ENABLE_VULKAN=ON).
    # -----------------------------------------------------------------------
    if(ENGINE_ENABLE_VULKAN)
        list(APPEND SANDBOX_SOURCES
            src/engine/rendering/vulkan/VulkanRenderer.cpp
            src/engine/rendering/vulkan/vulkan_pipeline.cpp
            src/engine/rendering/vulkan/vulkan_buffer.cpp
            src/engine/rendering/vulkan/vulkan_mesh.cpp
        )
    endif()

    # -----------------------------------------------------------------------
    # Audio backend sources (Windows-only, M3 XAudio2 backend).
    # -----------------------------------------------------------------------
    # TEACHING NOTE — XAudio2 is Windows-only
    # xaudio2.h and xaudio2.lib ship with every Windows SDK installation
    # (alongside d3d11.h / d3d11.lib).  No separate download is needed.
    # The sources are added to engine_sandbox (not the Linux terminal game).
    # -----------------------------------------------------------------------
    list(APPEND SANDBOX_SOURCES
        src/engine/audio/xaudio2_backend.cpp
        src/engine/audio/audio_system.cpp
    )

    # -----------------------------------------------------------------------
    # Physics sources (M5 — Jolt Physics, optional).
    # -----------------------------------------------------------------------
    # TEACHING NOTE — Optional Physics Subsystem
    # -----------------------------------------------------------------------
    # TEACHING NOTE — Physics Source Inclusion Strategy
    # -----------------------------------------------------------------------
    # physics_world.cpp, rigid_body.cpp, character_controller.cpp, and
    # raycast.cpp all depend on Jolt Physics types and the PhysicsWorld
    # implementation.  They are ONLY compiled when ENGINE_ENABLE_PHYSICS=ON
    # and JoltPhysics was found via vcpkg.
    #
    # hit_volume.cpp is an exception: it contains only pure C++17 AABB math
    # with no Jolt dependency.  It is compiled in ALL builds so that the
    # HitVolumeManager (used by the combat system) is always available.
    #
    # Why NOT include rigid_body.cpp without Jolt?
    # rigid_body.cpp calls PhysicsWorld::CreateBox/CreateSphere/etc.  Those
    # methods are defined in physics_world.cpp — which is only compiled when
    # Jolt is present.  Including rigid_body.cpp without physics_world.cpp
    # would produce unresolved external linker errors (LNK2019 on MSVC).
    # -----------------------------------------------------------------------
    if(ENGINE_ENABLE_PHYSICS AND Jolt_FOUND)
        list(APPEND SANDBOX_SOURCES
            src/engine/physics/physics_world.cpp
            src/engine/physics/rigid_body.cpp
            src/engine/physics/character_controller.cpp
            src/engine/physics/raycast.cpp
            # TEACHING NOTE — M11 Vehicle Physics (Post-M10)
            # vehicle_system.cpp implements the VehicleSystem: four wheel-ray
            # suspension raycasts, spring-damper force model, Ackermann steering,
            # and fuel drain.  It requires PhysicsWorld for the downward raycasts
            # and chassis box body, so it is compiled only when Jolt is present.
            src/engine/vehicle/vehicle_system.cpp
            # TEACHING NOTE — M25 Terrain Collision
            # terrain_collision.cpp creates a JPH::HeightFieldShape from a
            # float height array and registers it as a static physics body.
            # It depends on PhysicsWorld (physics_world.cpp) and uses the
            # internal physics_impl.hpp to access Jolt's BodyInterface — so it
            # must be compiled alongside the other Jolt-dependent sources.
            src/engine/physics/terrain_collision.cpp
        )
    endif()
    # hit_volume.cpp has no Jolt dependency — always include it.
    list(APPEND SANDBOX_SOURCES
        src/engine/physics/hit_volume.cpp
    )

    # -----------------------------------------------------------------------
    # Scene serialiser (M6) — JSON ↔ ECS World round-trip.
    # -----------------------------------------------------------------------
    # TEACHING NOTE — Conditional JSON sources
    # scene_serialiser.cpp is compiled in ALL Windows sandbox builds.
    # The actual JSON I/O code inside it is guarded by #ifdef ENGINE_ENABLE_JSON.
    # When nlohmann/json is NOT found, the functions log an error and return
    # false — no crash, no undefined symbols.  This way the sandbox always
    # links cleanly regardless of vcpkg availability.
    # -----------------------------------------------------------------------
    list(APPEND SANDBOX_SOURCES
        src/engine/scene/scene_serialiser.cpp
    )

    # -----------------------------------------------------------------------
    # World streaming sources (M7 — async loader + spatial partition).
    # -----------------------------------------------------------------------
    # TEACHING NOTE — M7 World Streaming Source Strategy
    # ─────────────────────────────────────────────────────
    # The three world-streaming modules are pure C++17 with no platform or
    # graphics-API dependency.  They compile on Windows (engine_sandbox) and
    # will later compile on Linux as well when the terminal game is wired up
    # in M8.  They are always included in the sandbox build so the headless
    # streaming acceptance tests (--scene streaming_load / streaming_evict /
    # streaming_async) are always available.
    #
    # async_loader.cpp   — std::thread worker + job queue.
    # world_partition.cpp — spatial grid coordinate conversion + neighbour query.
    # world_streaming.cpp — delta-based load/evict coordinator.
    # -----------------------------------------------------------------------
    list(APPEND SANDBOX_SOURCES
        src/engine/world/async_loader.cpp
        src/engine/world/world_partition.cpp
        src/engine/world/world_streaming.cpp
    )

    # -----------------------------------------------------------------------
    # LuaEngine sources (optional — only when bundled Lua headers are present).
    # -----------------------------------------------------------------------
    # TEACHING NOTE — Conditional Scripting in engine_sandbox
    # LuaEngine.cpp is added to engine_sandbox ONLY when LUA_BUNDLED=ON
    # (i.e. headers in Lua/include/ AND import lib in Lua/lib/ are found).
    # Without bundled Lua the scripting system compiles out via ENGINE_ENABLE_LUA
    # guards.  No build error occurs — scripting is simply unavailable.
    # -----------------------------------------------------------------------
    if(LUA_BUNDLED)
        list(APPEND SANDBOX_SOURCES src/engine/scripting/LuaEngine.cpp)
    endif()

    # -----------------------------------------------------------------------
    # TestWorld — game systems compiled into engine_sandbox (M3+).
    # -----------------------------------------------------------------------
    # TEACHING NOTE — Cross-Platform Game Systems
    # The gameplay systems (CombatSystem, AISystem, WeatherSystem, etc.) are
    # pure C++17 with no platform or ncurses dependencies.  They compile on
    # both Linux (terminal game) and Windows (engine_sandbox) alike.
    # Including them here lets engine_sandbox run a full simulation and visually
    # verify all systems are operational via the --scene testworld flag.
    # -----------------------------------------------------------------------
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
        # TEACHING NOTE — M7.1: GameStreamingManager wires Zone lifecycle into
        # WorldStreamingManager.  Compiled alongside Zone.cpp so that
        # OnCellLoaded / OnEvictCell can call Zone::SpawnEnemies / Zone::Unload.
        src/game/world/GameStreamingManager.cpp
        # TEACHING NOTE — M8 Gameplay Integration: new systems.
        # game_runtime.cpp owns and drives all gameplay systems from engine_sandbox.
        # input_mapper.cpp reads Win32 key state → ECS components.
        # dialogue_system.cpp tracks NPC dialogue interaction state.
        # camera_system.cpp computes view/proj matrices for the D3D11 renderer.
        # hud.cpp extracts HudState snapshots from the ECS World.
        # menu_stack.cpp implements the push/pop screen navigation stack (UI milestone).
        # save_system.cpp serialises/deserialises the ECS World to JSON save files.
        src/sandbox/game_runtime.cpp
        src/game/systems/input_mapper.cpp
        src/game/systems/dialogue_system.cpp
        src/engine/rendering/camera_system.cpp
        src/engine/ui/hud.cpp
        src/engine/ui/menu_stack.cpp
        src/engine/save/save_system.cpp
        # TEACHING NOTE — M19 Action Combat: ComboSystem FSM.
        # combo_system.cpp is pure C++17 with no platform or rendering
        # dependencies.  It compiles on Linux (terminal game) and Windows alike.
        # JSON loading inside it is guarded by ENGINE_ENABLE_JSON.
        src/engine/combat/combo_system.cpp
        # TEACHING NOTE — Demo_Game OpenWorld state machine
        # open_world.cpp is a pure C++17 module with no D3D11 or Win32
        # dependency.  It is compiled into engine_sandbox so the headless
        # --scene demo_world acceptance test is available in every CI build.
        src/demo_game/open_world.cpp
        # TEACHING NOTE — Demo_Game Quest & Activity Manager (M-DG9)
        # demo_quest_manager.cpp is a pure C++17 module (no D3D11/Win32/ECS).
        # It is compiled into engine_sandbox alongside open_world.cpp so the
        # --scene demo_world acceptance test 7 (quest_manager) runs in CI.
        # demo_game inherits it automatically via DEMO_GAME_SOURCES derivation.
        src/demo_game/demo_quest_manager.cpp
    )

    add_executable(engine_sandbox ${SANDBOX_SOURCES})

    # -----------------------------------------------------------------------
    # Include directories.
    # -----------------------------------------------------------------------
    target_include_directories(engine_sandbox PRIVATE
        src/
    )

    # Add Vulkan include path only when Vulkan is available.
    if(ENGINE_ENABLE_VULKAN AND Vulkan_FOUND)
        target_include_directories(engine_sandbox PRIVATE
            ${Vulkan_INCLUDE_DIRS}
        )
    endif()

    # -----------------------------------------------------------------------
    # Link libraries.
    # -----------------------------------------------------------------------
    # TEACHING NOTE — d3d11.lib and dxgi.lib
    # These libraries ship with the Windows SDK (included in every Visual
    # Studio installation).  They do NOT require a separate Vulkan-style SDK
    # download.  Any Windows developer machine already has them.
    # -----------------------------------------------------------------------
    if(ENGINE_ENABLE_D3D11)
        target_link_libraries(engine_sandbox PRIVATE d3d11.lib dxgi.lib d3dcompiler.lib)
    endif()

    # TEACHING NOTE — xaudio2.lib and ole32.lib
    # xaudio2.lib ships with the Windows SDK (alongside d3d11.lib).
    # ole32.lib provides CoInitializeEx / CoUninitialize for the COM runtime
    # required by XAudio2.  Both are always present on any MSVC installation.
    target_link_libraries(engine_sandbox PRIVATE xaudio2.lib ole32.lib)

    if(ENGINE_ENABLE_VULKAN AND Vulkan_FOUND)
        target_link_libraries(engine_sandbox PRIVATE Vulkan::Vulkan)
    endif()

    # TEACHING NOTE — Jolt::Jolt
    # The vcpkg joltphysics package (v5+) provides an imported CMake target
    # "Jolt::Jolt" that carries all include directories and link libraries
    # automatically.  Linking with target_link_libraries() is all that is
    # needed — no manual include_directories() or add_library() calls required.
    if(ENGINE_ENABLE_PHYSICS AND Jolt_FOUND)
        target_link_libraries(engine_sandbox PRIVATE Jolt::Jolt)
    endif()

    # TEACHING NOTE — nlohmann_json::nlohmann_json (M6)
    # Header-only library; linking the CMake imported target adds the vcpkg
    # include path so #include <nlohmann/json.hpp> resolves in scene_serialiser.cpp.
    if(nlohmann_json_FOUND)
        target_link_libraries(engine_sandbox PRIVATE nlohmann_json::nlohmann_json)
    endif()

    # -----------------------------------------------------------------------
    # Compile definitions.
    # -----------------------------------------------------------------------
    # TEACHING NOTE — Compile-Time Feature Flags
    # ENGINE_ENABLE_D3D11 and ENGINE_ENABLE_VULKAN are passed as preprocessor
    # macros so the RendererFactory.hpp can conditionally include the right
    # backend headers and the IRenderer implementations can guard
    # platform-specific code.
    #
    # TEACHING NOTE — UNICODE and _UNICODE
    # Win32Window.cpp uses std::wstring / const wchar_t* for the window title.
    # Without these macros MSVC maps CreateWindowEx → CreateWindowExA (narrow),
    # causing C2440/C2664 errors.
    # -----------------------------------------------------------------------
    # TEACHING NOTE — Incremental compile definitions
    # We start with definitions that are always required (Win32 header trimming
    # + Unicode), then conditionally append backend feature flags.
    # ENGINE_ENABLE_D3D11 / ENGINE_ENABLE_VULKAN flow into RendererFactory.hpp
    # so the factory can gate which concrete renderer header(s) it includes.
    # -----------------------------------------------------------------------
    set(SANDBOX_DEFS WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
        "ENGINE_VERSION_SHA=\"${ENGINE_GIT_SHA}\"")

    if(ENGINE_ENABLE_D3D11)
        list(APPEND SANDBOX_DEFS ENGINE_ENABLE_D3D11)
    endif()

    if(ENGINE_ENABLE_VULKAN AND Vulkan_FOUND)
        list(APPEND SANDBOX_DEFS ENGINE_ENABLE_VULKAN VK_USE_PLATFORM_WIN32_KHR)
    endif()

    # TEACHING NOTE — ENGINE_ENABLE_PHYSICS compile definition
    # Defined when Jolt Physics is available.  Physics .cpp files use
    # #ifdef ENGINE_ENABLE_PHYSICS to gate Jolt headers/code.
    # main.cpp uses it to gate the --scene physics_test acceptance tests.
    if(ENGINE_ENABLE_PHYSICS AND Jolt_FOUND)
        list(APPEND SANDBOX_DEFS ENGINE_ENABLE_PHYSICS)
    endif()

    # TEACHING NOTE — ENGINE_ENABLE_JSON compile definition (M6)
    # Defined when nlohmann/json is available via vcpkg.  The scene_serialiser
    # gates all JSON parsing/writing code behind this macro.
    if(nlohmann_json_FOUND)
        list(APPEND SANDBOX_DEFS ENGINE_ENABLE_JSON)
    endif()

    target_compile_definitions(engine_sandbox PRIVATE ${SANDBOX_DEFS})

    # -----------------------------------------------------------------------
    # Lua 5.5 scripting support for engine_sandbox (Windows).
    # -----------------------------------------------------------------------
    # TEACHING NOTE — Lua in engine_sandbox
    # ──────────────────────────────────────
    # When LUA_BUNDLED=ON (Lua/lua-5.5.0/src/ exists), the lua55_static CMake
    # target is already built from source.  We:
    #   1. Link engine_sandbox against lua55_static (static lib — no DLL needed).
    #   2. Include directories are inherited via target_link_libraries PUBLIC.
    #   3. Define ENGINE_ENABLE_LUA to activate all scripting hooks.
    #   4. Copy lua55.dll alongside the exe (optional — only needed if users
    #      also run standalone .lua scripts via lua55.exe; the embedded engine
    #      uses the static lib).
    # -----------------------------------------------------------------------
    if(LUA_BUNDLED)
        # LUA_LIBRARIES is the CMake target lua55_static; include dirs are
        # inherited via the PUBLIC target_include_directories on that target.
        target_link_libraries(engine_sandbox PRIVATE ${LUA_LIBRARIES})
        target_compile_definitions(engine_sandbox PRIVATE ENGINE_ENABLE_LUA)
        # Copy lua55.dll alongside the exe for convenience (running scripts).
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

    # -----------------------------------------------------------------------
    # TEACHING NOTE — SUBSYSTEM:CONSOLE
    # -----------------------------------------------------------------------
    # By default MSVC creates a GUI app (WinMain entry, no console).
    # We request CONSOLE so std::cout / std::cerr output is visible in
    # a terminal — essential for a teaching project and CI validation.
    # -----------------------------------------------------------------------
    if(MSVC)
        set_target_properties(engine_sandbox PROPERTIES
            LINK_FLAGS "/SUBSYSTEM:CONSOLE"
        )
    endif()

    # -----------------------------------------------------------------------
    # Vulkan shader compilation (glslc) — only when Vulkan is enabled.
    # -----------------------------------------------------------------------
    # TEACHING NOTE — Shader Compilation with glslc
    # GLSL shaders cannot be loaded directly by Vulkan — they must be compiled
    # to SPIR-V first.  glslc ships with the Vulkan SDK.
    #
    # D3D11 uses HLSL compiled to CSO files (or runtime compilation).  HLSL
    # shader compilation via FXC/DXC will be added in M3 D3D11 textures.
    # -----------------------------------------------------------------------
    if(ENGINE_ENABLE_VULKAN AND Vulkan_FOUND)
        find_program(GLSLC_EXECUTABLE glslc
            HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
            DOC   "glslc GLSL-to-SPIR-V compiler from the Vulkan SDK")

        if(GLSLC_EXECUTABLE)
            message(STATUS "glslc found: ${GLSLC_EXECUTABLE}")

            set(SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/shaders")
            set(SHADER_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")

            # List every shader here; add more as milestones progress.
            set(GLSL_SHADERS
                "${SHADER_SOURCE_DIR}/triangle.vert"
                "${SHADER_SOURCE_DIR}/triangle.frag"
            )

            foreach(GLSL_SHADER ${GLSL_SHADERS})
                get_filename_component(SHADER_NAME "${GLSL_SHADER}" NAME)
                set(SPV_OUTPUT "${SHADER_BINARY_DIR}/${SHADER_NAME}.spv")

                add_custom_command(
                    OUTPUT  "${SPV_OUTPUT}"
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_BINARY_DIR}"
                    COMMAND ${GLSLC_EXECUTABLE} "${GLSL_SHADER}" -o "${SPV_OUTPUT}"
                    DEPENDS "${GLSL_SHADER}"
                    COMMENT "Compiling GLSL shader: ${SHADER_NAME} -> ${SHADER_NAME}.spv"
                )
                list(APPEND SPV_OUTPUTS "${SPV_OUTPUT}")
            endforeach()

            add_custom_target(engine_shaders DEPENDS ${SPV_OUTPUTS})
            add_dependencies(engine_sandbox engine_shaders)

            # Copy compiled .spv files next to the executable after each build.
            # TEACHING NOTE — $<TARGET_FILE_DIR:engine_sandbox>
            # This generator expression expands to the directory containing
            # the built executable (e.g. build/Debug/ on MSVC).
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

    # -----------------------------------------------------------------------
    # TEACHING NOTE — D3D11 HLSL Shaders: Copy to output directory
    # -----------------------------------------------------------------------
    # D3D11Renderer compiles HLSL shaders at runtime using D3DCompileFromFile
    # (see D3D11Renderer.cpp :: LoadScene).  The .hlsl source files must
    # therefore be present next to the executable at runtime.
    #
    # We copy them with a POST_BUILD command so that both Debug and Release
    # configurations automatically get up-to-date shader files whenever the
    # engine is built.
    #
    # The shaders/ sub-directory mirrors the path returned by GetShaderDir()
    # in src/sandbox/main.cpp.
    # -----------------------------------------------------------------------
    if(ENGINE_ENABLE_D3D11)
        set(HLSL_SHADERS
            "${CMAKE_SOURCE_DIR}/shaders/textured_quad.vs.hlsl"
            "${CMAKE_SOURCE_DIR}/shaders/textured_quad.ps.hlsl"
            # TEACHING NOTE — M4b: GPU skinning HLSL shaders.
            # skinned_mesh.vs.hlsl implements linear blend skinning (LBS):
            #   worldPos = Σ weight[i] * (g_joints[boneIndex[i]] * bindPos)
            # skinned_mesh.ps.hlsl applies Lambertian lighting + color gradient.
            "${CMAKE_SOURCE_DIR}/shaders/skinned_mesh.vs.hlsl"
            "${CMAKE_SOURCE_DIR}/shaders/skinned_mesh.ps.hlsl"
            # TEACHING NOTE — M9: PBR Cook-Torrance HLSL shaders.
            # pbr_mesh.vs.hlsl transforms vertices to world/clip space and
            #   computes world-space normals via the inverse-transpose matrix.
            # pbr_mesh.ps.hlsl implements the full Cook-Torrance BRDF:
            #   GGX NDF + Smith geometry + Schlick Fresnel + Reinhard tonemap
            #   + gamma correction.  Metallic-roughness material workflow.
            "${CMAKE_SOURCE_DIR}/shaders/pbr_mesh.vs.hlsl"
            "${CMAKE_SOURCE_DIR}/shaders/pbr_mesh.ps.hlsl"
            # TEACHING NOTE — M10: Dynamic sky HLSL shaders.
            # sky.vs.hlsl generates a full-screen triangle using SV_VertexID
            #   (no vertex buffer required; 3 vertices cover the entire viewport).
            # sky.ps.hlsl implements the procedural sky:
            #   zenith-to-horizon gradient + sun disc + atmospheric scatter
            #   approximation + fog overlay + rain darkening + Reinhard tonemap.
            "${CMAKE_SOURCE_DIR}/shaders/sky.vs.hlsl"
            "${CMAKE_SOURCE_DIR}/shaders/sky.ps.hlsl"
            # TEACHING NOTE — Post-M10: SDF text rendering HLSL shaders.
            # sdf_text.vs.hlsl transforms screen-space pixel quads to NDC.
            # sdf_text.ps.hlsl samples the SDF atlas and applies smoothstep
            #   anti-aliasing to reconstruct crisp glyph edges at any scale.
            "${CMAKE_SOURCE_DIR}/shaders/sdf_text.vs.hlsl"
            "${CMAKE_SOURCE_DIR}/shaders/sdf_text.ps.hlsl"
            # TEACHING NOTE — M16: PBR + Image-Based Lighting (IBL) shaders.
            # pbr_ibl.vs.hlsl is structurally identical to pbr_mesh.vs.hlsl —
            #   it outputs worldPos, worldNrm, and UV to the pixel shader.
            # pbr_ibl.ps.hlsl adds the full split-sum IBL ambient:
            #   • BRDF LUT (t0) — precomputed scale+bias for split-sum Fresnel.
            #   • Irradiance cubemap (t1) — diffuse environment light (sampled
            #     with N; convoluted with a cosine-weighted hemisphere kernel).
            #   • Prefiltered env cubemap (t2) — specular reflections at every
            #     roughness level (5 mip levels, sampled via SampleLevel(r*4)).
            #   FresnelSchlickRoughness() is used for the ambient kS term so
            #   very rough metals don't have unrealistically high ambient specular.
            "${CMAKE_SOURCE_DIR}/shaders/pbr_ibl.vs.hlsl"
            "${CMAKE_SOURCE_DIR}/shaders/pbr_ibl.ps.hlsl"

            # TEACHING NOTE — M17: Directional shadow map shaders.
            # shadow.vs.hlsl       — depth-only VS for the shadow pass (one matrix
            #   multiply, no PS output).  Identity world is baked into lightViewProj.
            # shadow_lit.vs.hlsl   — lit-pass VS: outputs worldPos + worldNrm for
            #   light-space reprojection and Lambert shading in the PS.
            # shadow_lit.ps.hlsl   — lit-pass PS: 3×3 PCF shadow lookup via
            #   SamplerComparisonState (D3D11_COMPARISON_LESS_EQUAL) + Lambert diffuse.
            "${CMAKE_SOURCE_DIR}/shaders/shadow.vs.hlsl"
            "${CMAKE_SOURCE_DIR}/shaders/shadow_lit.vs.hlsl"
            "${CMAKE_SOURCE_DIR}/shaders/shadow_lit.ps.hlsl"

            # TEACHING NOTE — M17: HDR bloom post-processing shaders.
            # All three passes reuse sky.vs.hlsl as the full-screen triangle VS
            # (SV_VertexID → full-screen quad trick — no vertex buffer needed).
            # bloom_bright.ps.hlsl — extracts pixels with luminance > threshold.
            #   Reads: t0 = scene RGBA8 RT.  Writes: bright pixels at full lum,
            #   dark pixels as black (0,0,0,1).
            # bloom_blur.ps.hlsl   — separable 5-tap Gaussian blur.
            #   g_direction selects horizontal (1,0) or vertical (0,1) axis.
            #   Weight kernel: [0.227, 0.194, 0.122, 0.054, 0.016].
            # bloom_composite.ps.hlsl — adds bloom × g_bloomStrength to the scene,
            #   then applies per-channel Reinhard tonemap → LDR output.
            "${CMAKE_SOURCE_DIR}/shaders/bloom_bright.ps.hlsl"
            "${CMAKE_SOURCE_DIR}/shaders/bloom_blur.ps.hlsl"
            "${CMAKE_SOURCE_DIR}/shaders/bloom_composite.ps.hlsl"

            # TEACHING NOTE — M25: Terrain heightmap shaders.
            # terrain.vs.hlsl transforms the CPU-generated heightmap grid vertices
            #   (pos, normal, uv) through world→view→clip matrices and passes
            #   world-space pos + normal to the pixel shader.
            # terrain.ps.hlsl applies Lambert diffuse + ambient with height-based
            #   colour blending (grass at low Y, rock at high Y) + Reinhard tonemap.
            # Both use a shared TerrainCB constant buffer bound to b0 in both stages.
            "${CMAKE_SOURCE_DIR}/shaders/terrain.vs.hlsl"
            "${CMAKE_SOURCE_DIR}/shaders/terrain.ps.hlsl"
        )

        add_custom_command(TARGET engine_sandbox POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:engine_sandbox>/shaders"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${HLSL_SHADERS}
                "$<TARGET_FILE_DIR:engine_sandbox>/shaders/"
            COMMENT "Copying D3D11 HLSL shaders to output directory"
        )
    endif() # ENGINE_ENABLE_D3D11

endif() # WIN32 AND (ENGINE_ENABLE_D3D11 OR ENGINE_ENABLE_VULKAN)

# ---------------------------------------------------------------------------
# demo_game target — Demo_Game standalone open-world executable (Epic: Demo_Game)
# ---------------------------------------------------------------------------
# TEACHING NOTE — Demo_Game vs engine_sandbox
# ─────────────────────────────────────────────────────────────────────────────
# engine_sandbox is the internal test harness; demo_game is the player-facing
# binary.  Both share the same engine + gameplay source files; the only
# difference is the entry point:
#
#   engine_sandbox  → src/sandbox/main.cpp  (--scene dispatch, CI tests)
#   demo_game       → src/demo_game/demo_main.cpp (open world, boot menu)
#
# The OpenWorld state machine (open_world.hpp/.cpp) is a NEW pure-C++17
# module compiled into both demo_game (primary) and engine_sandbox (for the
# --scene demo_world CI headless test).  It has no D3D11 or Win32 dependency.
#
# Usage:
#   demo_game.exe                # windowed: boot menu → open world
#   demo_game.exe --headless     # CI: headless demo_world acceptance test
# ─────────────────────────────────────────────────────────────────────────────
if(BUILD_DEMO_GAME AND WIN32 AND ENGINE_ENABLE_D3D11)

    # TEACHING NOTE — Deriving DEMO_GAME_SOURCES from SANDBOX_SOURCES
    # ─────────────────────────────────────────────────────────────────
    # Rather than maintaining a duplicate source list (which is easy to let
    # drift when new engine/game files are added), we derive DEMO_GAME_SOURCES
    # from the fully-populated SANDBOX_SOURCES variable and swap only the
    # executable entry point.  Both targets stay in lockstep automatically.
    #
    # Steps:
    #   1. Copy the entire SANDBOX_SOURCES list.
    #   2. Remove src/sandbox/main.cpp (the test-harness entry point).
    #   3. Append src/demo_game/demo_main.cpp (the player-facing entry point).
    #
    # open_world.cpp is already present in SANDBOX_SOURCES (added for the
    # --scene demo_world acceptance test), so it does not need to be re-added.
    # All conditionally-included sources (Jolt physics, LuaEngine, etc.) are
    # carried over automatically because SANDBOX_SOURCES was already populated
    # by those conditional blocks above.
    set(DEMO_GAME_SOURCES ${SANDBOX_SOURCES})
    list(REMOVE_ITEM DEMO_GAME_SOURCES src/sandbox/main.cpp)
    list(APPEND DEMO_GAME_SOURCES src/demo_game/demo_main.cpp)

    add_executable(demo_game ${DEMO_GAME_SOURCES})

    target_include_directories(demo_game PRIVATE src/)

    target_link_libraries(demo_game PRIVATE
        d3d11.lib dxgi.lib d3dcompiler.lib xaudio2.lib ole32.lib
    )

    set(DEMO_DEFS WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
        ENGINE_ENABLE_D3D11
        "ENGINE_VERSION_SHA=\"${ENGINE_GIT_SHA}\"")

    if(nlohmann_json_FOUND)
        target_link_libraries(demo_game PRIVATE nlohmann_json::nlohmann_json)
        list(APPEND DEMO_DEFS ENGINE_ENABLE_JSON)
    endif()

    if(LUA_BUNDLED)
        target_link_libraries(demo_game PRIVATE ${LUA_LIBRARIES})
        list(APPEND DEMO_DEFS ENGINE_ENABLE_LUA)
    endif()

    target_compile_definitions(demo_game PRIVATE ${DEMO_DEFS})

    if(MSVC)
        set_target_properties(demo_game PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
    endif()

    # Copy HLSL shaders alongside demo_game.exe at build time.
    add_custom_command(TARGET demo_game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:demo_game>/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${HLSL_SHADERS}
            "$<TARGET_FILE_DIR:demo_game>/shaders/"
        COMMENT "Copying D3D11 HLSL shaders to demo_game output directory"
    )

    message(STATUS "  demo_game target : ON (BUILD_DEMO_GAME=ON)")
else()
    # TEACHING NOTE — Status messages should distinguish "not requested"
    # from "requested but unavailable due to unmet platform/backend
    # requirements" so configure-time output tells the user what to fix.
    if(BUILD_DEMO_GAME AND NOT WIN32)
        message(STATUS "  demo_game target : SKIPPED (requires WIN32 platform)")
    elseif(BUILD_DEMO_GAME AND NOT ENGINE_ENABLE_D3D11)
        message(STATUS "  demo_game target : SKIPPED (requires ENGINE_ENABLE_D3D11=ON)")
    else()
        message(STATUS "  demo_game target : OFF (pass -DBUILD_DEMO_GAME=ON to enable)")
    endif()
endif()

# ---------------------------------------------------------------------------
# cook target — standalone C++ asset cooker (Milestone M2)
# ---------------------------------------------------------------------------
# TEACHING NOTE — Standalone Tool Target
# ─────────────────────────────────────────────────────────────────────────────
# The cook tool is a platform-independent C++ executable that:
#   1. Reads AssetRegistry.json from the given project directory.
#   2. Copies/converts source assets → Cooked/ directory.
#   3. Writes Cooked/assetdb.json for the C++ runtime (AssetDB).
#
# It is independent of the Vulkan renderer so it builds on all platforms
# (Windows, Linux, macOS).
#
# Usage after build:
#   ./cook --project samples/vertical_slice_project
#   .\Debug\cook.exe --project samples\vertical_slice_project
# ---------------------------------------------------------------------------
add_executable(cook
    src/tools/cook/cook_main.cpp
    src/engine/core/Logger.cpp
)

# TEACHING NOTE — target_include_directories (PRIVATE)
# Only this target needs to see src/ for #include "engine/core/Logger.hpp".
# We use PRIVATE so the include path does not leak to anything that links
# against cook (there is nothing, but it is good practice).
target_include_directories(cook PRIVATE
    src/
)

# TEACHING NOTE — MSVC /SUBSYSTEM:CONSOLE
# Same reasoning as engine_sandbox: we want stdout/stderr visible in a
# terminal window on Windows.
if(MSVC)
    set_target_properties(cook PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
endif()

message(STATUS "  cook target      : ON (always built)")



# ---------------------------------------------------------------------------
# pak target — standalone PAK1 archive packager (Post-M10)
# ---------------------------------------------------------------------------
# TEACHING NOTE — PAK1 Packager Tool
# ─────────────────────────────────────────────────────────────────────────────
# The pak tool bundles an input directory into a binary .pak archive
# (PAK1 format).  It mirrors the role of CRIWARE CPK / SQEX PAK in a real
# AAA game (e.g. Final Fantasy XV): consolidate thousands of loose asset
# files into a single seekable archive for efficient distribution and streaming.
#
# PAK1 format (little-endian):
#   Header (12 bytes): magic=0x31_4B_41_50 | version=1 | fileCount=N
#   FileTable: N × (pathLen uint16 + path UTF-8 + dataOffset uint64 + dataSize uint64)
#   FileData: N raw blobs concatenated
#
# Usage after build:
#   pak.exe --input  samples\vertical_slice_project\Cooked\ --output game.pak
#   pak.exe --list   game.pak
#   pak.exe --extract game.pak --output out\
# ---------------------------------------------------------------------------
add_executable(pak
    src/tools/pak/pak_main.cpp
    src/engine/core/Logger.cpp
)

target_include_directories(pak PRIVATE
    src/
)

# TEACHING NOTE — MSVC /SUBSYSTEM:CONSOLE
# Same reasoning as cook.exe: stdout/stderr must be visible in a terminal.
if(MSVC)
    set_target_properties(pak PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
endif()

message(STATUS "  pak target       : ON (always built)")



# ---------------------------------------------------------------------------
# cook_samples — convenience target to cook the vertical-slice sample project
# ---------------------------------------------------------------------------
# TEACHING NOTE — Custom build targets for developer ergonomics
# ─────────────────────────────────────────────────────────────────────────────
# New developers cloning the repository need cooked sample data before the
# runtime can stream real level content.  The cook_samples target automates
# this step: it runs cook.exe on the vertical-slice project and generates
#   samples/vertical_slice_project/Cooked/assetdb.json
#   samples/vertical_slice_project/Cooked/Levels/cells_manifest.json
# (plus cooked copies of every asset in AssetRegistry.json).
#
# Usage:
#   cmake --build <preset> --target cook_samples
#
# The target depends on the cook executable so CMake guarantees cook.exe is
# up-to-date before running it.
#
# This target is NOT included in the default ALL build to avoid surprising
# long cook times for headless CI jobs that provide pre-baked Cooked/ data.
# Opt in explicitly with `--target cook_samples` whenever you need fresh data.
# ─────────────────────────────────────────────────────────────────────────────
add_custom_target(cook_samples
    COMMAND $<TARGET_FILE:cook>
            --project "${CMAKE_SOURCE_DIR}/samples/vertical_slice_project"
    DEPENDS cook
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Cooking vertical-slice sample assets (generates Cooked/ + cells_manifest.json)..."
    VERBATIM
)

message(STATUS "  cook_samples     : add --target cook_samples to generate cooked sample data")



message(STATUS "=== Build Configuration ===")
message(STATUS "  CXX Standard     : C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build type       : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Terminal target  : ${ENGINE_ENABLE_TERMINAL}")
message(STATUS "  D3D11 backend    : ${ENGINE_ENABLE_D3D11}  (default runtime)")
message(STATUS "  Vulkan backend   : ${ENGINE_ENABLE_VULKAN} (optional high-end)")
if(ENGINE_ENABLE_TERMINAL)
    message(STATUS "  ncurses          : ${CURSES_LIBRARIES}")
    message(STATUS "  Lua              : ${LUA_LIBRARIES}")
endif()
if(ENGINE_ENABLE_VULKAN AND Vulkan_FOUND)
    message(STATUS "  Vulkan libs      : ${Vulkan_LIBRARIES}")
endif()
message(STATUS "  Physics (Jolt)   : ${ENGINE_ENABLE_PHYSICS}")
message(STATUS "===========================")

# ---------------------------------------------------------------------------
# Optional subprojects
# ---------------------------------------------------------------------------
# TEACHING NOTE — add_subdirectory()
# add_subdirectory(dir) tells CMake to also process dir/CMakeLists.txt.
# Each subdirectory is a self-contained "project" with its own targets and
# build rules, but they can share variables set by the parent (e.g., the
# C++ standard already set above).

if(BUILD_EDITOR)
    # TEACHING NOTE -- Dear ImGui Editor Subproject
    # The editor is a Dear ImGui (MIT) application that provides:
    #   * Content browser  -- file tree via std::filesystem
    #   * Scene editor     -- 2D entity canvas with JSON save/load (nlohmann-json)
    #   * Menu bar         -- open project, save scene, cook assets
    # Requires vcpkg with imgui[docking,dx11-binding,win32-binding] installed.
    # No Qt installation needed.
    message(STATUS "BUILD_EDITOR=ON: including editor/ subproject (Dear ImGui)")
    add_subdirectory(editor)
else()
    message(STATUS "BUILD_EDITOR=OFF: skipping editor/ (pass -DBUILD_EDITOR=ON to include)")
endif()

# ---------------------------------------------------------------------------
# CMake install() target — distributable packaging (AP-3)
# ---------------------------------------------------------------------------
# TEACHING NOTE — CMake install()
# ─────────────────────────────────────────────────────────────────────────────
# Running `cmake --install <build-dir> --prefix dist/` copies all installed
# files into a self-contained directory ready for redistribution.
#
# What gets installed:
#   bin/engine_sandbox.exe   — the main engine runtime
#   bin/cook.exe             — standalone asset cooker
#   bin/pak.exe              — PAK archive packager
#   bin/shaders/             — all HLSL shader source files (compiled at
#                              runtime by D3DCompileFromFile)
#   bin/Cooked/              — pre-cooked vertical-slice assets
#   bin/AssetRegistry.json   — asset registry for the cooked project
#   bin/lua55.dll            — Lua 5.5 runtime DLL (if present)
#   bin/engine_config.json   — default user-editable settings file
#
# Usage:
#   cmake --install build/windows-ninja-release-engine-only --prefix dist/
#   dist\bin\engine_sandbox.exe --headless --scene dynamic_sky
# ─────────────────────────────────────────────────────────────────────────────
if(WIN32 AND (ENGINE_ENABLE_D3D11 OR ENGINE_ENABLE_VULKAN))
    install(TARGETS engine_sandbox cook pak
        RUNTIME DESTINATION bin
    )
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/shaders/"
        DESTINATION bin/shaders
        FILES_MATCHING PATTERN "*.hlsl"
    )
    if(EXISTS "${CMAKE_SOURCE_DIR}/samples/vertical_slice_project/Cooked")
        install(DIRECTORY "${CMAKE_SOURCE_DIR}/samples/vertical_slice_project/Cooked/"
            DESTINATION bin/Cooked
        )
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/samples/vertical_slice_project/AssetRegistry.json")
        install(FILES "${CMAKE_SOURCE_DIR}/samples/vertical_slice_project/AssetRegistry.json"
            DESTINATION bin
        )
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/Lua/lua55.dll")
        install(FILES "${CMAKE_SOURCE_DIR}/Lua/lua55.dll"
            DESTINATION bin
        )
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/engine_config.json")
        install(FILES "${CMAKE_SOURCE_DIR}/engine_config.json"
            DESTINATION bin
        )
    endif()
endif()

