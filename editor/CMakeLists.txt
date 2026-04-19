# ===========================================================================
# editor/CMakeLists.txt -- Dear ImGui Creation Suite Editor
# ===========================================================================
#
# TEACHING NOTE -- Dear ImGui vs Qt Widgets
# ------------------------------------------
# Qt Widgets uses a RETAINED-MODE UI model:
#   * Widget objects (QMainWindow, QTreeView ...) persist between frames.
#   * They are updated via method calls (setText, setModel ...).
#   * Qt requires a multi-hundred-MB installation and LGPL/commercial licence.
#
# Dear ImGui uses an IMMEDIATE-MODE UI model:
#   * Every frame you call ImGui:: functions that BOTH define the UI AND
#     query input in one step.  There are no persistent widget objects.
#   * ImGui::Button("Save") returns true the frame it was clicked.
#   * Licence: MIT -- zero cost, no LGPL viral clauses, no install required.
#   * Industry usage: Unreal Engine tools, many AAA engine editors,
#     Valve/Epic internal tools.
#
# This migration means:
#   Before: find_package(Qt6 ...) -- requires Qt 6 installed + CMAKE_PREFIX_PATH
#   After:  find_package(imgui ...) -- provided by vcpkg, zero extra install
#
# TEACHING NOTE -- vcpkg Feature Flags
# --------------------------------------
# vcpkg supports "features" -- optional compile-time variants of a package.
# For imgui we enable three features in vcpkg.json:
#   docking-experimental -- enables the dockable-window API (ImGui::DockSpace etc.)
#   dx11-binding         -- adds imgui_impl_dx11.h/.cpp for D3D11 rendering
#   win32-binding        -- adds imgui_impl_win32.h/.cpp for Win32 window input
# vcpkg packages these as additional headers/sources exposed via the same
# imgui::imgui imported target.

cmake_minimum_required(VERSION 3.16)
project(CreationSuiteEditor VERSION 1.0 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# C++ Standard
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

# ---------------------------------------------------------------------------
# Dependencies (provided by vcpkg -- see vcpkg.json in the repo root)
# ---------------------------------------------------------------------------
# TEACHING NOTE -- find_package with vcpkg-installed libraries
# When CMake is configured with the vcpkg toolchain file
# (-DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake),
# find_package() automatically finds packages installed by vcpkg.
# Each package exposes CMake "imported targets" that carry include paths
# and linker flags automatically.
#   imgui::imgui               -- Dear ImGui core + D3D11 + Win32 backends
#   nlohmann_json::nlohmann_json -- header-only JSON library (already in vcpkg.json)
find_package(imgui         CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# Editor sources
# ---------------------------------------------------------------------------
set(EDITOR_SOURCES
    # Entry point -- creates Win32 window, D3D11 device, runs ImGui loop
    src/main.cpp

    # Top-level application: DockSpace layout, menu bar, project state
    src/EditorApp.cpp
    src/EditorApp.hpp

    # Content browser panel -- file tree via std::filesystem
    src/ContentBrowserPanel.cpp
    src/ContentBrowserPanel.hpp

    # Scene editor panel -- 2D entity canvas with JSON save/load
    src/SceneEditorPanel.cpp
    src/SceneEditorPanel.hpp

    # M6 -- Scene Hierarchy panel: entity list, rename, duplicate, delete
    src/panels/SceneHierarchyPanel.cpp
    src/panels/SceneHierarchyPanel.hpp

    # M6 -- Inspector panel: component property editor (table-driven)
    src/panels/InspectorPanel.cpp
    src/panels/InspectorPanel.hpp
)

add_executable(creation-suite-editor WIN32 ${EDITOR_SOURCES})

# ---------------------------------------------------------------------------
# Include paths
# ---------------------------------------------------------------------------
target_include_directories(creation-suite-editor PRIVATE
    src/
    src/panels/
    # Shared runtime headers -- Guid.hpp (UUID generation), VersionedFile.hpp
    ${CMAKE_SOURCE_DIR}/shared/runtime
)

# ---------------------------------------------------------------------------
# Link libraries
# ---------------------------------------------------------------------------
# TEACHING NOTE -- Linking D3D11 + DXGI
# d3d11.lib  -- Direct3D 11 device, context, swapchain.  Ships with the
#               Windows SDK; no extra SDK install beyond Visual Studio.
# dxgi.lib   -- DirectX Graphics Infrastructure: swap chain, adapters.
# Both are OS components (no DLL to ship alongside the editor).
# ImGui's D3D11 backend calls into these via device/context pointers
# we create in main.cpp.
target_link_libraries(creation-suite-editor PRIVATE
    imgui::imgui
    nlohmann_json::nlohmann_json
    d3d11
    dxgi
)

# ---------------------------------------------------------------------------
# Compiler settings
# ---------------------------------------------------------------------------
if(WIN32)
    # TEACHING NOTE -- UNICODE, WIN32_LEAN_AND_MEAN, NOMINMAX
    # UNICODE/_UNICODE  : Win32 API calls default to wide-char (wchar_t) variants.
    # WIN32_LEAN_AND_MEAN : excludes rarely-used headers from <windows.h>,
    #   dramatically reducing compile time.
    # NOMINMAX : prevents windows.h from defining min/max macros that clash
    #   with std::min / std::max.
    target_compile_definitions(creation-suite-editor PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

if(MSVC)
    target_compile_options(creation-suite-editor PRIVATE /W4 /WX-)
endif()

# ---------------------------------------------------------------------------
# Print summary
# ---------------------------------------------------------------------------
message(STATUS "=== Editor Build (Dear ImGui) ===")
message(STATUS "  imgui config : ${imgui_DIR}")
message(STATUS "=================================")
