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
project(EducationalGameEngine VERSION 1.0 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Monorepo top-level option: Build the Qt editor
# ---------------------------------------------------------------------------
# TEACHING NOTE — Optional Subprojects
# The editor requires Qt 6 to be installed.  On a machine without Qt the
# engine still builds and runs.  Set BUILD_EDITOR=ON to include the editor.
#
#   cmake .. -DBUILD_EDITOR=ON -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2019_64"
#
option(BUILD_EDITOR "Build the Qt-based Creation Suite editor" OFF)

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

# ---- Lua 5.4 (scripting — used by the terminal game target) ---------------
# Try pkg-config first (most Linux distros), fallback to manual search.
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(LUA lua5.4)
endif()

if(NOT LUA_FOUND)
    # Fallback: manual search.
    find_path(LUA_INCLUDE_DIRS lua.h
        PATHS /usr/include/lua5.4
              /usr/local/include/lua5.4
              /usr/include/lua
              /usr/local/include/lua)
    find_library(LUA_LIBRARIES
        NAMES lua5.4 lua54 lua
        PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu)
endif()

if(LUA_LIBRARIES AND LUA_INCLUDE_DIRS)
    message(STATUS "Found Lua: ${LUA_LIBRARIES}")
else()
    message(WARNING
        "Lua 5.4 not found. Install with: sudo apt install liblua5.4-dev\n"
        "  Scripting features will be disabled.")
    set(LUA_LIBRARIES "")
    set(LUA_INCLUDE_DIRS "")
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
        src/engine/assets/asset_db.cpp
        src/engine/assets/asset_loader.cpp
    )

    # -----------------------------------------------------------------------
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
        target_link_libraries(engine_sandbox PRIVATE d3d11.lib dxgi.lib)
    endif()

    if(ENGINE_ENABLE_VULKAN AND Vulkan_FOUND)
        target_link_libraries(engine_sandbox PRIVATE Vulkan::Vulkan)
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
    set(SANDBOX_DEFS WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)

    if(ENGINE_ENABLE_D3D11)
        list(APPEND SANDBOX_DEFS ENGINE_ENABLE_D3D11)
    endif()

    if(ENGINE_ENABLE_VULKAN AND Vulkan_FOUND)
        list(APPEND SANDBOX_DEFS ENGINE_ENABLE_VULKAN VK_USE_PLATFORM_WIN32_KHR)
    endif()

    target_compile_definitions(engine_sandbox PRIVATE ${SANDBOX_DEFS})

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

endif() # WIN32 AND (D3D11 OR Vulkan)

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
    # TEACHING NOTE — Qt Editor Subproject
    # The editor is a Qt 6 Widgets application that provides:
    #   • Project browser  — open a project folder, see its Content/ files
    #   • Content browser  — list assets by type
    #   • Scene editor     — place entities, save a scene/map JSON file
    # Qt must be installed and CMAKE_PREFIX_PATH must point to the Qt install.
    message(STATUS "BUILD_EDITOR=ON: including editor/ subproject")
    add_subdirectory(editor)
else()
    message(STATUS "BUILD_EDITOR=OFF: skipping editor/ (pass -DBUILD_EDITOR=ON to include)")
endif()

