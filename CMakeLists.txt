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

# TEACHING NOTE — ENGINE_ENABLE_VULKAN
# Defaults to ON on Windows only.  Nothing prevents enabling it on Linux
# later (Vulkan is cross-platform), but for now we use it as the Windows path.
if(WIN32)
    option(ENGINE_ENABLE_VULKAN "Build the Vulkan engine_sandbox target" ON)
else()
    option(ENGINE_ENABLE_VULKAN "Build the Vulkan engine_sandbox target" OFF)
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

# ---- Vulkan SDK (Windows renderer) ----------------------------------------
# TEACHING NOTE — find_package(Vulkan)
# The Vulkan SDK installer (https://vulkan.lunarg.com/) sets the VULKAN_SDK
# environment variable.  CMake's built-in FindVulkan module uses that variable
# to locate vulkan-1.lib (Windows) and the vulkan/vulkan.h headers.
# The imported target Vulkan::Vulkan provides both include paths and link libs.
if(ENGINE_ENABLE_VULKAN)
    find_package(Vulkan REQUIRED)
    message(STATUS "Vulkan found: ${Vulkan_LIBRARIES}")
endif()

# ---------------------------------------------------------------------------
# Engine sources (shared kernel — platform-independent)
# ---------------------------------------------------------------------------

set(ENGINE_CORE_SOURCES
    src/engine/core/Logger.cpp
    src/engine/scripting/LuaEngine.cpp
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
# engine_sandbox target (Windows — Vulkan clear-screen demo)
# ---------------------------------------------------------------------------
# TEACHING NOTE — engine_sandbox
# This is the "Phase 1" rendering milestone described in the architecture plan.
# It demonstrates:
#   • Win32 window creation and message pump.
#   • Vulkan instance, surface, device, swapchain bootstrap.
#   • Per-frame acquire → record clear → submit → present.
#
# Build instructions (Windows, Vulkan SDK installed):
#   mkdir build && cd build
#   cmake .. -G "Visual Studio 17 2022" -A x64
#   cmake --build . --config Debug --target engine_sandbox
# ---------------------------------------------------------------------------
if(ENGINE_ENABLE_VULKAN)

    set(SANDBOX_SOURCES
        src/sandbox/main.cpp
        src/engine/platform/win32/Win32Window.cpp
        src/engine/rendering/vulkan/VulkanRenderer.cpp
        src/engine/rendering/vulkan/vulkan_pipeline.cpp
        src/engine/rendering/vulkan/vulkan_buffer.cpp
        src/engine/rendering/vulkan/vulkan_mesh.cpp
        src/engine/core/Logger.cpp
    )

    add_executable(engine_sandbox
        ${SANDBOX_SOURCES}
    )

    target_include_directories(engine_sandbox PRIVATE
        src/
        ${Vulkan_INCLUDE_DIRS}
    )

    target_link_libraries(engine_sandbox PRIVATE
        Vulkan::Vulkan
    )

    # TEACHING NOTE — VK_USE_PLATFORM_WIN32_KHR compile definition
    # This macro must be defined before including <vulkan/vulkan.h> so that
    # the Win32-specific surface types and functions are declared.
    # Passing it as a compile definition ensures it is set for every
    # translation unit in this target without relying on the include order.
    target_compile_definitions(engine_sandbox PRIVATE
        VK_USE_PLATFORM_WIN32_KHR
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )

    # TEACHING NOTE — SUBSYSTEM:CONSOLE
    # By default MSVC creates a GUI app (WinMain entry point, no console).
    # We explicitly request CONSOLE so that std::cout / std::cerr output
    # is visible in a terminal window — essential for a teaching project.
    if(MSVC)
        set_target_properties(engine_sandbox PROPERTIES
            LINK_FLAGS "/SUBSYSTEM:CONSOLE"
        )
    endif()

    # ---------------------------------------------------------------------------
    # TEACHING NOTE — Shader Compilation with glslc
    # ---------------------------------------------------------------------------
    # GLSL shaders cannot be loaded directly by Vulkan — they must be compiled
    # to SPIR-V first.  glslc is the reference compiler that ships with the
    # Vulkan SDK (usually at %VULKAN_SDK%/Bin/glslc.exe on Windows).
    #
    # We use CMake's add_custom_command to compile each shader automatically
    # as part of the build.  The compiled .spv files are then copied next to
    # the executable so the runtime can find them at the relative path
    # "shaders/<name>.spv".
    #
    # Teaching point: in a shipping game the shader compilation happens
    # offline (as part of the asset cook pipeline).  During development,
    # running glslc at build time speeds up the iteration loop.
    # ---------------------------------------------------------------------------
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

        # Compile each GLSL shader to SPIR-V.
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

        # Custom target that depends on all compiled .spv files.
        add_custom_target(engine_shaders DEPENDS ${SPV_OUTPUTS})

        # Make engine_sandbox depend on the shaders so they are compiled first.
        add_dependencies(engine_sandbox engine_shaders)

        # Copy compiled .spv files next to the executable after each build.
        # TEACHING NOTE — $<TARGET_FILE_DIR:engine_sandbox>
        # This CMake generator expression evaluates at build time to the
        # directory containing the built engine_sandbox executable.
        # On MSVC this is e.g. build/Debug/ or build/Release/.
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
            "glslc not found — triangle shaders will NOT be compiled.\n"
            "  Install the Vulkan SDK from https://vulkan.lunarg.com/ "
            "and ensure %VULKAN_SDK%/Bin is on your PATH."
        )
    endif() # GLSLC_EXECUTABLE

endif() # ENGINE_ENABLE_VULKAN

# ---------------------------------------------------------------------------
# Print summary
# ---------------------------------------------------------------------------

message(STATUS "=== Build Configuration ===")
message(STATUS "  CXX Standard     : C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build type       : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Terminal target  : ${ENGINE_ENABLE_TERMINAL}")
message(STATUS "  Vulkan target    : ${ENGINE_ENABLE_VULKAN}")
if(ENGINE_ENABLE_TERMINAL)
    message(STATUS "  ncurses          : ${CURSES_LIBRARIES}")
    message(STATUS "  Lua              : ${LUA_LIBRARIES}")
endif()
if(ENGINE_ENABLE_VULKAN)
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

