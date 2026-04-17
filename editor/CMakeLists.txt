# ===========================================================================
# editor/CMakeLists.txt — Qt 6 Widgets Creation Suite Editor
# ===========================================================================
#
# TEACHING NOTE — Qt + CMake Integration
# ──────────────────────────────────────
# Qt 6 provides first-class CMake support via find_package(Qt6 ...).
# The key concepts:
#
#   find_package(Qt6 REQUIRED COMPONENTS Widgets Core)
#       Locates the Qt6 installation.  CMake searches CMAKE_PREFIX_PATH,
#       the QTDIR environment variable, and common install locations.
#       On Windows you typically need to set CMAKE_PREFIX_PATH:
#         cmake .. -DCMAKE_PREFIX_PATH="C:/Qt/6.6.0/msvc2019_64"
#
#   set(CMAKE_AUTOUIC ON)  — Qt User Interface Compiler (auto-processes .ui files)
#   set(CMAKE_AUTOMOC ON)  — Qt Meta-Object Compiler  (auto-processes Q_OBJECT classes)
#   set(CMAKE_AUTORCC ON)  — Qt Resource Compiler     (auto-processes .qrc files)
#
# The three AUTO* properties make Qt development with CMake nearly seamless:
# you don't need to manually run moc/uic/rcc; CMake does it automatically.
#
# TEACHING NOTE — Qt Widgets Architecture
# ─────────────────────────────────────────
# Qt Widgets is the classic Qt UI framework (not QML / Qt Quick).
# It uses a C++ widget hierarchy with signal/slot connections for events.
# Good for desktop tools like level editors, asset browsers, and property panels.
# The architecture here follows the Model-View-Controller (MVC) pattern:
#   • MainWindow     — top-level window; hosts menus, toolbar, dock widgets
#   • ContentBrowser — tree/list view of Content/ folder (the "View")
#   • SceneEditor    — 2D canvas for entity placement (the "View")
#   • SceneModel     — data model for scene entities (the "Model")

cmake_minimum_required(VERSION 3.16)
project(CreationSuiteEditor VERSION 1.0 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# C++ Standard
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

# ---------------------------------------------------------------------------
# Qt Auto-processing
# ---------------------------------------------------------------------------
# TEACHING NOTE — Why these three lines matter
# Without AUTOMOC, every class that uses Q_OBJECT must be manually listed for
# the moc (Meta-Object Compiler) tool.  AUTOMOC scans source files for
# Q_OBJECT, Q_GADGET, etc. and runs moc automatically. AUTOUIC and AUTORCC do
# the same for .ui files and .qrc resource files respectively.
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ---------------------------------------------------------------------------
# Find Qt 6
# ---------------------------------------------------------------------------
# TEACHING NOTE — find_package and imported targets
# find_package(Qt6 REQUIRED COMPONENTS ...) creates imported targets like
# Qt6::Widgets that carry all required include paths and link flags.
# Using imported targets is strongly preferred over the older variable-based
# approach (Qt5_INCLUDE_DIRS, Qt5_LIBRARIES) because they are self-describing
# and work correctly in both Debug and Release.
find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

# ---------------------------------------------------------------------------
# Editor sources
# ---------------------------------------------------------------------------
set(EDITOR_SOURCES
    # Entry point — creates QApplication and shows MainWindow
    src/main.cpp

    # Main application window (menus, toolbar, dock widgets)
    src/MainWindow.cpp
    src/MainWindow.hpp

    # Content browser — shows files under a project's Content/ folder
    src/ContentBrowser.cpp
    src/ContentBrowser.hpp

    # Scene/map editor — 2D canvas for placing entities, saving scene JSON
    src/SceneEditor.cpp
    src/SceneEditor.hpp
)

add_executable(creation-suite-editor WIN32 ${EDITOR_SOURCES})

# ---------------------------------------------------------------------------
# Include paths
# ---------------------------------------------------------------------------
target_include_directories(creation-suite-editor PRIVATE
    src/
    # Shared runtime headers (GUID, VersionedFile, etc.)
    ${CMAKE_SOURCE_DIR}/shared/runtime
)

# ---------------------------------------------------------------------------
# Link Qt libraries
# ---------------------------------------------------------------------------
# TEACHING NOTE — Imported target vs. variable
# Qt6::Widgets transitively brings in Qt6::Core, Qt6::Gui, and all their
# include directories and link libraries, so we only need to list the
# top-level component(s) we directly use.
target_link_libraries(creation-suite-editor PRIVATE
    Qt6::Widgets
    Qt6::Core
)

# ---------------------------------------------------------------------------
# Windows-specific settings
# ---------------------------------------------------------------------------
if(WIN32)
    # TEACHING NOTE — WIN32 keyword in add_executable
    # The WIN32 keyword in add_executable changes the subsystem to WINDOWS
    # (GUI mode), which hides the console window — correct for a GUI editor.
    # (We added WIN32 directly in the add_executable call above.)

    # TEACHING NOTE — windeployqt
    # On Windows, Qt DLLs must be next to the executable.  The windeployqt
    # tool copies all required Qt DLLs, plugins, and QML files.
    # We run it automatically after building:
    add_custom_command(TARGET creation-suite-editor POST_BUILD
        COMMAND "${Qt6_DIR}/../../../bin/windeployqt.exe"
                "$<TARGET_FILE:creation-suite-editor>"
        COMMENT "Running windeployqt to copy Qt DLLs..."
        VERBATIM
    )
endif()

# ---------------------------------------------------------------------------
# Print summary
# ---------------------------------------------------------------------------
message(STATUS "=== Editor Build ===")
message(STATUS "  Qt version  : ${Qt6_VERSION}")
message(STATUS "  Qt prefix   : ${Qt6_DIR}")
message(STATUS "====================")
