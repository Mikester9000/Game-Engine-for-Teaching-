# Creation Suite Editor

A **Dear ImGui** + D3D11 level editor for the **Game Engine for Teaching** monorepo.
Inspired by Final Fantasy XV's creation toolchain.

## Why Dear ImGui?

| Aspect | Qt 6 (old) | Dear ImGui (new) |
|--------|-----------|-----------------|
| Licence | LGPL 3.0 or commercial | **MIT** |
| Install | ~600 MB Qt installer | Zero (via vcpkg) |
| Rendering | Separate OS widget | Into engine's own D3D11 swap chain |
| UI model | Retained-mode | **Immediate-mode** (same as Unreal/Unity debug tools) |
| Learning value | Qt-specific patterns | Industry-standard editor UI pattern |

## Features

- **Content Browser** -- file tree showing `Content/` assets (textures, audio, scenes, scripts)
- **Scene Editor** -- 2D canvas for placing named entities, saving scene JSON
- **JSON save/load** -- uses `shared/schemas/scene.schema.json` format via nlohmann-json
- **Cook integration** -- Build > Cook Assets launches `cook_assets.py`
- **Dockable panels** -- drag panels to rearrange; layout saved to `.ini` file

## Prerequisites

| Requirement | Version | Notes |
|-------------|---------|-------|
| CMake       | 3.16+   | |
| MSVC        | 2019+   | Visual Studio 17 2022 recommended |
| vcpkg       | latest  | Install `imgui[docking-experimental,dx11-binding,win32-binding]` via `vcpkg.json` |

## Building

```bat
:: From repo root:
cmake --preset windows-debug -DBUILD_EDITOR=ON ^
      -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake"
cmake --build --preset windows-debug

:: Or manually:
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64 ^
         -DBUILD_EDITOR=ON ^
         -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake"
cmake --build . --config Debug
```

The editor executable is at `build/windows-debug/editor/Debug/creation-suite-editor.exe`.

## Usage

1. Launch `creation-suite-editor.exe`
2. **File > Open Project...** -- select the `samples/vertical_slice_project/` folder
3. The Content Browser shows the `Content/` folder tree
4. Double-click `Content/Maps/MainTown.scene.json` to load it into the scene editor
5. Left-click on the canvas to place new entities (a name popup appears)
6. **File > Save Scene** -- saves to a `.scene.json` file
7. **Build > Cook Assets** -- runs `cook_assets.py` in the project folder

## Architecture

```
main.cpp  (Win32 window + D3D11 device + ImGui init + message loop)
    |
    v
EditorApp  (DockSpaceOverViewport + main menu bar + status bar)
    |
    +-- ContentBrowserPanel  (ImGui::TreeNode + std::filesystem)
    |
    +-- SceneEditorPanel     (ImGui::BeginChild + ImDrawList + nlohmann-json)
```

### Immediate-mode render pattern

Every frame, `EditorApp::Render()` is called:

```cpp
ImGui_ImplDX11_NewFrame();      // D3D11 backend: sync GPU resources
ImGui_ImplWin32_NewFrame();     // Win32 backend: poll mouse/keyboard
ImGui::NewFrame();              // start ImGui frame

editorApp.Render();             // declare ALL panels, menus, widgets

ImGui::Render();                // compile draw lists
ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());  // execute on GPU
g_pSwapChain->Present(1, 0);   // vsync present
```

### Adding a new panel

1. Create `editor/src/MyPanel.hpp` + `MyPanel.cpp`
2. Add a member `MyPanel m_myPanel;` to `EditorApp`
3. Call `m_myPanel.Render()` from `EditorApp::Render()`
4. Add both `.cpp` + `.hpp` to `EDITOR_SOURCES` in `editor/CMakeLists.txt`

### Shared schema

Scenes use `shared/schemas/scene.schema.json`.  The engine reads the same
`.scene.json` files from `Cooked/Maps/`.  The cook script validates and copies
scenes from `Content/Maps/` to `Cooked/Maps/`.

## Licence note

- **Dear ImGui** -- MIT licence (see vcpkg/imgui)
- **nlohmann-json** -- MIT licence
- **D3D11 / DXGI** -- Windows SDK (royalty-free, ships with OS)
- All editor source code -- same licence as the rest of this repository
