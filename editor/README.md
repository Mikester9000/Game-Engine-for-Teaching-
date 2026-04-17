# Creation Suite Editor

A Qt 6 Widgets-based level editor for the **Game Engine for Teaching** monorepo.
Inspired by Final Fantasy XV's creation toolchain.

## Features

- **Project Browser** — Open a project folder, navigate its `Content/` files
- **Content Browser** — Tree view showing assets by type (textures, audio, scenes, scripts)
- **Scene Editor** — 2D canvas for placing named entities, saving scene JSON
- **JSON save/load** — Uses `shared/schemas/scene.schema.json` format
- **Cook integration** — "Build → Cook Assets" runs `cook_assets.py`

## Prerequisites

| Requirement | Version | Notes |
|-------------|---------|-------|
| Qt 6        | 6.5+    | Widgets + Core components |
| CMake       | 3.16+   | |
| MSVC        | 2019+   | Visual Studio 17 2022 recommended |

## Building

```bat
:: From repo root — with Qt installed at C:\Qt\6.x.x\msvc2019_64:
cmake --preset windows-debug -DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\msvc2019_64"
cmake --build --preset windows-debug

:: Or manually:
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64 ^
         -DBUILD_EDITOR=ON ^
         -DCMAKE_PREFIX_PATH="C:\Qt\6.6.0\msvc2019_64"
cmake --build . --config Debug
```

The editor executable is at `build/windows-debug/editor/Debug/creation-suite-editor.exe`.

## Usage

1. Launch `creation-suite-editor.exe`
2. **File → Open Project…** — select the `samples/vertical_slice_project/` folder
3. The Content Browser shows `Content/Maps/MainTown.scene.json`
4. Double-click `MainTown.scene.json` — loads the scene into the editor
5. Left-click on the canvas to place new entities
6. **File → Save Scene** — saves to a `.scene.json` file
7. **Build → Cook Assets** — runs `cook_assets.py` in the project folder

## Architecture

```
MainWindow (QMainWindow)
├── setupMenuBar()     — File | Build | Help menus
├── setupToolBar()     — Quick-access toolbar buttons
├── setupDockWidgets() — ContentBrowser in a left dock panel
├── setupStatusBar()   — Persistent project name label
│
├── ContentBrowser (QDockWidget → QTreeView + QFileSystemModel)
│   Emits: fileSelected(absolutePath)
│
└── SceneEditor (QWidget — central widget)
    ├── paintEvent()      — draws grid + entity boxes
    ├── mousePressEvent() — place / select entity
    ├── keyPressEvent()   — Delete = remove selected entity
    ├── saveScene()       — QJsonDocument → .scene.json file
    └── loadScene()       — .scene.json file → QJsonDocument → entities
```

## Adding new panels

1. Create `editor/src/MyPanel.hpp` + `MyPanel.cpp`
2. Derive from `QWidget` or `QDockWidget`; add `Q_OBJECT` in private
3. In `MainWindow::setupDockWidgets()`, create and dock it
4. Add `src/MyPanel.cpp` to `EDITOR_SOURCES` in `editor/CMakeLists.txt`

## Shared schema

Scenes are saved using `shared/schemas/scene.schema.json`.  The engine
reads the same format from `Cooked/Maps/`.  The cook script copies scenes
from `Content/Maps/` to `Cooked/Maps/` with optional validation.
