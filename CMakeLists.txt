# ===========================================================================
# CMakeLists.txt — Educational Game Engine
# ===========================================================================
#
# TEACHING NOTE — Build System Design
# ─────────────────────────────────────
# CMake is the de-facto standard build-system generator for C++ projects.
# It does NOT compile code directly — it generates Makefiles (Linux),
# Xcode projects (macOS), or Visual Studio solutions (Windows) from a
# platform-neutral description.
#
# Key CMake concepts used here:
#   target_sources      — list of .cpp files to compile.
#   target_include_dirs — where the compiler looks for #include files.
#   target_link_libs    — external libraries to link against.
#   target_compile_opts — compiler warning/optimisation flags.
#
# TEACHING NOTE — Out-of-Source Builds
# ──────────────────────────────────────
# Always build in a separate directory (e.g., build/) so that generated
# files (object files, Makefiles) don't pollute the source tree:
#
#   mkdir build && cd build
#   cmake ..
#   make -j4
#

cmake_minimum_required(VERSION 3.16)
project(EducationalGameEngine VERSION 1.0 LANGUAGES CXX)

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
# Find required system libraries
# ---------------------------------------------------------------------------

# ncurses (for the terminal renderer).
# TEACHING NOTE — find_package(Curses) sets:
#   CURSES_LIBRARIES  — ncurses link flags
#   CURSES_INCLUDE_DIRS — header directory (usually /usr/include)
find_package(Curses REQUIRED)

# Lua 5.4 (for scripting).
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

# ---------------------------------------------------------------------------
# Engine sources
# ---------------------------------------------------------------------------

set(ENGINE_SOURCES
    src/engine/core/Logger.cpp
    src/engine/rendering/Renderer.cpp
    src/engine/input/InputSystem.cpp
    src/engine/scripting/LuaEngine.cpp
)

# ---------------------------------------------------------------------------
# Game sources
# ---------------------------------------------------------------------------

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

# ---------------------------------------------------------------------------
# Main executable
# ---------------------------------------------------------------------------

add_executable(game
    src/main.cpp
    ${ENGINE_SOURCES}
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

# ---------------------------------------------------------------------------
# Copy Lua scripts to build directory after build
# ---------------------------------------------------------------------------

add_custom_command(TARGET game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/scripts
        $<TARGET_FILE_DIR:game>/scripts
    COMMENT "Copying Lua scripts to build directory."
)

# ---------------------------------------------------------------------------
# Print summary
# ---------------------------------------------------------------------------

message(STATUS "=== Build Configuration ===")
message(STATUS "  CXX Standard : C++${CMAKE_CXX_STANDARD}")
message(STATUS "  ncurses      : ${CURSES_LIBRARIES}")
message(STATUS "  Lua          : ${LUA_LIBRARIES}")
message(STATUS "  Build type   : ${CMAKE_BUILD_TYPE}")
message(STATUS "===========================")
