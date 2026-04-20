/**
 * @file EditorApp.cpp
 * @brief Top-level editor application implementation (M6 updated).
 */

#include "EditorApp.hpp"

#include <imgui.h>
#include <string>
#include <filesystem>

// Windows native file dialogs and shell execution
#include <windows.h>
#include <shellapi.h>   // ShellExecuteW, ShellExecuteExW, SHELLEXECUTEINFOW
#include <commdlg.h>    // OPENFILENAMEW, GetSaveFileNameW, GetOpenFileNameW
#include <shobjidl.h>   // IFileOpenDialog (modern COM-based dialog)
#include <shlobj.h>     // SHBrowseForFolderW (legacy folder picker)

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

EditorApp::EditorApp()
{
    // TEACHING NOTE -- Wiring panels with non-owning pointers
    // SceneHierarchyPanel and InspectorPanel need a pointer to m_sceneEditor
    // so they can read/write the shared entity list and selection state.
    // We wire them here after m_sceneEditor is constructed (member init order
    // guarantees m_sceneEditor is fully constructed before m_hierarchy and
    // m_inspector, but their constructors receive nullptr; we call SetScenePanel
    // explicitly to be safe regardless of declaration order).
    m_hierarchy.SetScenePanel(&m_sceneEditor);
    m_inspector.SetScenePanel(&m_sceneEditor);

    m_statusMessage = "Ready";
}

// ---------------------------------------------------------------------------
// Render -- called every frame
// ---------------------------------------------------------------------------

void EditorApp::Render()
{
    // ---- Full-screen DockSpace ---------------------------------------------
    // TEACHING NOTE -- DockSpaceOverViewport
    // ImGui::DockSpaceOverViewport() creates a DockSpace that fills the entire
    // main viewport.  All other ImGui windows can be docked into this space.
    //
    // ImGuiDockNodeFlags_PassthruCentralNode lets the game/editor background
    // (the D3D11 clear colour) show through the undocked central area.
    // Without this flag the dockspace paints an opaque background over the
    // central region even when no window is docked there.
    //
    // TEACHING NOTE -- DockSpaceOverViewport API change (imgui 1.89.4+)
    // In imgui 1.89.4 the signature changed: the first argument switched from
    // (const ImGuiViewport*) to (ImGuiID dockspace_id).  The viewport moved to
    // the second argument.  vcpkg tag 2024.12.16 ships imgui 1.91.5, which uses
    // this newer signature.  Pass 0 to let ImGui auto-assign a stable ID.
    ImGui::DockSpaceOverViewport(
        0,
        ImGui::GetMainViewport(),
        ImGuiDockNodeFlags_PassthruCentralNode
    );

    // ---- Menu bar ----------------------------------------------------------
    RenderMenuBar();

    // ---- Panels ------------------------------------------------------------
    m_contentBrowser.Render();
    m_sceneEditor.Render();

    // TEACHING NOTE -- M6 new panels
    // These two panels share state with m_sceneEditor via the pointer set in
    // the constructor.  They are separate dockable ImGui windows -- the user
    // can drag them to any position in the DockSpace layout.
    m_hierarchy.Render();
    m_inspector.Render();

    // ---- About popup -------------------------------------------------------
    if (m_showAbout)
        ImGui::OpenPopup("About##popup");

    // TEACHING NOTE -- ImGui Modal Popups
    // OpenPopup() marks a popup as "open"; BeginPopupModal() renders it.
    // The popup blocks interaction with windows behind it (modal behaviour).
    // EndPopup() must always be called if BeginPopupModal returned true.
    if (ImGui::BeginPopupModal("About##popup", &m_showAbout,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Creation Suite Editor  v1.0  (M7)");
        ImGui::Separator();
        ImGui::TextUnformatted("Part of the Game Engine for Teaching monorepo.");
        ImGui::TextUnformatted("Dear ImGui (MIT) for UI.");
        ImGui::TextUnformatted("D3D11 (Windows SDK) for rendering.");
        ImGui::TextUnformatted("nlohmann-json (MIT) for scene files.");
        ImGui::Spacing();
        ImGui::TextUnformatted("M6 panels: Scene Hierarchy, Inspector, Play-in-Engine.");
        ImGui::TextUnformatted("M7.5: World Streaming debug overlay (View menu).");
        ImGui::Spacing();
        if (ImGui::Button("Close", ImVec2(120, 0)))
        {
            m_showAbout = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ---- M7.5: World Streaming debug overlay -------------------------------
    // TEACHING NOTE — Streaming minimap reference panel
    // ────────────────────────────────────────────────────
    // This floating window shows the colour legend for the streaming debug
    // minimap that WorldStreamingManager::DrawDebugOverlay() produces.
    //
    // In M7.5 the overlay is a reference panel only: the editor does not own
    // a live WorldStreamingManager instance (that lives in the game runtime).
    // To see the actual cell-state grid, call DrawDebugOverlay() from your
    // game loop and pass the ImDrawList + camera position.  Full editor
    // integration (live cell grid inside the editor viewport) is planned for
    // M8.7 when GameStreamingManager is wired to the D3D11 runtime.
    if (m_showStreamingOverlay)
    {
        ImGuiIO& imguiIO = ImGui::GetIO();
        const float overlayX = imguiIO.DisplaySize.x - 240.0f;
        const float overlayY = 40.0f;

        ImGui::SetNextWindowPos(ImVec2(overlayX, overlayY), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.75f);
        ImGui::SetNextWindowSize(ImVec2(230.0f, 200.0f), ImGuiCond_Always);

        const ImGuiWindowFlags kOverlayFlags =
            ImGuiWindowFlags_NoDecoration     |
            ImGuiWindowFlags_NoMove           |
            ImGuiWindowFlags_NoSavedSettings  |
            ImGuiWindowFlags_NoBringToFrontOnFocus;

        if (ImGui::Begin("##StreamingOverlay", nullptr, kOverlayFlags))
        {
            ImGui::TextUnformatted("World Streaming (M7.5)");
            ImGui::Separator();
            // TEACHING NOTE — DrawDebugOverlay usage
            // ────────────────────────────────────────
            // Call from your D3D11 game loop (M8.7):
            //   mgr.DrawDebugOverlay(ImGui::GetWindowDrawList(),
            //                        originX, originY, 20.f, cameraPos);
            ImGui::TextUnformatted("Legend (DrawDebugOverlay):");
            ImGui::Spacing();

            // Legend swatches
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 p = ImGui::GetCursorScreenPos();
            float ly = p.y;
            const float sw = 12.0f;

            auto swatch = [&](ImU32 col, const char* label)
            {
                dl->AddRectFilled(ImVec2(p.x, ly), ImVec2(p.x + sw, ly + sw), col);
                ImGui::SetCursorScreenPos(ImVec2(p.x + sw + 4.0f, ly));
                ImGui::TextUnformatted(label);
                ly += 16.0f;
            };

            swatch(IM_COL32(128,128,128,200), "Unloaded");
            swatch(IM_COL32(255,220,  0,200), "Loading");
            swatch(IM_COL32(  0,192, 64,200), "Loaded");
            swatch(IM_COL32(255, 64, 64,200), "Evicting");
        }
        ImGui::End();
    }

    // ---- Status bar --------------------------------------------------------
    RenderStatusBar();

    // Decay the transient status timer
    ImGuiIO& io = ImGui::GetIO();
    if (m_statusTimer > 0.f)
    {
        m_statusTimer -= io.DeltaTime;
        if (m_statusTimer <= 0.f)
            m_statusMessage = "Ready";
    }
}

// ---------------------------------------------------------------------------
// Menu bar
// ---------------------------------------------------------------------------

// TEACHING NOTE -- ImGui Menu Bar
// ImGui::BeginMainMenuBar() / EndMainMenuBar() create a menu bar anchored to
// the top of the main viewport (not inside any ImGui window).
// ImGui::BeginMenu("Label") opens a submenu; it returns true only while the
// menu is open, so the code inside the if() block runs only that frame.
// ImGui::MenuItem("Label", "Shortcut", &checked) handles checkboxes too.
void EditorApp::RenderMenuBar()
{
    if (!ImGui::BeginMainMenuBar())
        return;

    // ---- File menu ---------------------------------------------------------
    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Open Project...", "Ctrl+O"))
        {
            std::string dir = PickFolder("Open Project Folder");
            if (!dir.empty())
            {
                m_projectPath = dir;

                // Tell content browser to show Content/ sub-folder
                std::string contentDir = dir + "/Content";
                if (fs::exists(contentDir))
                    m_contentBrowser.SetRootPath(contentDir);
                else
                    m_contentBrowser.SetRootPath(dir);

                m_statusMessage = "Opened: " + dir;
                m_statusTimer   = 4.f;
            }
        }

        if (ImGui::MenuItem("New Scene", "Ctrl+N"))
        {
            m_sceneEditor.NewScene();
            m_statusMessage = "New scene created.";
            m_statusTimer   = 3.f;
        }

        // TEACHING NOTE -- Load Scene (M6)
        // Mirrors "Save Scene" but in the other direction: shows a file open
        // dialog, then calls SceneEditorPanel::LoadScene() to parse the JSON.
        if (ImGui::MenuItem("Load Scene...", "Ctrl+Shift+O"))
        {
            std::string filePath = PickOpenFile(
                "Load Scene", "Scene Files\0*.scene.json\0All Files\0*.*\0");
            if (!filePath.empty())
            {
                if (m_sceneEditor.LoadScene(filePath))
                {
                    m_statusMessage = "Loaded: " + filePath;
                    m_statusTimer   = 4.f;
                }
                else
                {
                    m_statusMessage = "Load failed: " + filePath;
                    m_statusTimer   = 5.f;
                }
            }
        }

        if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
        {
            if (m_projectPath.empty())
            {
                m_statusMessage = "No project open. Use File > Open Project first.";
                m_statusTimer   = 5.f;
            }
            else
            {
                std::string filePath = PickSaveFile(
                    "Save Scene", "Scene Files\0*.scene.json\0All Files\0*.*\0",
                    "scene.json");
                if (!filePath.empty())
                {
                    if (m_sceneEditor.SaveScene(filePath))
                    {
                        m_statusMessage = "Saved: " + filePath;
                        m_statusTimer   = 4.f;
                    }
                    else
                    {
                        m_statusMessage = "Save failed: " + filePath;
                        m_statusTimer   = 5.f;
                    }
                }
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Exit", "Alt+F4"))
        {
            // TEACHING NOTE -- Posting WM_QUIT from within ImGui
            // PostQuitMessage(0) posts a WM_QUIT to the Win32 message queue.
            // The main loop detects this and sets done = true, triggering cleanup.
            PostQuitMessage(0);
        }

        ImGui::EndMenu();
    }

    // ---- Build menu --------------------------------------------------------
    if (ImGui::BeginMenu("Build"))
    {
        if (ImGui::MenuItem("Cook Assets", "Ctrl+B"))
        {
            if (m_projectPath.empty())
            {
                m_statusMessage = "No project open.";
                m_statusTimer   = 4.f;
            }
            else
            {
                // TEACHING NOTE -- ShellExecuteW to run the cook script
                // ShellExecuteW launches an external process using the Windows
                // shell.  "open" + a .py file invokes the system Python interpreter.
                // For production you would use CreateProcessW to capture stdout/stderr.
                std::wstring wScript(m_projectPath.begin(), m_projectPath.end());
                wScript += L"\\cook_assets.py";
                ShellExecuteW(nullptr, L"open", L"python",
                              (L"\"" + wScript + L"\"").c_str(),
                              nullptr, SW_SHOWNORMAL);
                m_statusMessage = "Cook started...";
                m_statusTimer   = 4.f;
            }
        }

        ImGui::Separator();

        // TEACHING NOTE -- Play in Engine (M6)
        // "Play in Engine" saves the current scene to a temp .scene.json file
        // and launches engine_sandbox.exe with --scene pointing to that file.
        // This allows the designer to immediately test the scene in the runtime
        // engine without leaving the editor.
        //
        // The engine_sandbox.exe is expected to live in the same directory as
        // editor.exe (both are built into the same CMake output directory).
        if (ImGui::MenuItem("Play in Engine", "F5"))
        {
            LaunchPlayInEngine();
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Save scene to a temp file and launch engine_sandbox.exe.\n"
                "engine_sandbox.exe must be in the same folder as editor.exe.");
        }

        ImGui::EndMenu();
    }

    // ---- View menu ---------------------------------------------------------
    // TEACHING NOTE — M7.5: View menu with streaming overlay toggle
    // ──────────────────────────────────────────────────────────────
    // The View menu controls optional debug visualisations.  Adding toggles
    // here (rather than hardcoding them) makes it easy to add more overlays
    // in future milestones (e.g. physics bounding boxes, nav-mesh, AI states).
    //
    // ImGui::MenuItem with a bool* reference automatically renders a check-mark
    // next to the label when the value is true, and toggles it on click.
    if (ImGui::BeginMenu("View"))
    {
        ImGui::MenuItem("World Streaming Overlay", nullptr, &m_showStreamingOverlay);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Show the world streaming debug minimap.\n"
                "Cells: grey=Unloaded, yellow=Loading, green=Loaded, red=Evicting.\n"
                "White outline = camera's current cell.");
        }
        ImGui::EndMenu();
    }

    // ---- Help menu ---------------------------------------------------------
    if (ImGui::BeginMenu("Help"))
    {
        if (ImGui::MenuItem("About"))
            m_showAbout = true;
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

// ---------------------------------------------------------------------------
// Shared helper — wide-char to UTF-8
// ---------------------------------------------------------------------------

// TEACHING NOTE -- WideCharToMultiByte for UTF-8 conversion
// Windows internally uses UTF-16 (wide char) for all API strings.
// Our public API uses std::string (UTF-8), which is the cross-platform norm.
// WideCharToMultiByte(CP_UTF8, ...) converts UTF-16 → UTF-8.
// The two-pass pattern (first call returns required buffer size, second fills it)
// is required because UTF-8 and UTF-16 have variable-length encodings.
static std::string WideToUtf8(const std::wstring& ws)
{
    if (ws.empty()) return {};
    const int len = WideCharToMultiByte(
        CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string s(static_cast<size_t>(len - 1), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, ws.c_str(), -1, s.data(), len, nullptr, nullptr);
    return s;
}

// ---------------------------------------------------------------------------
// Play in Engine
// ---------------------------------------------------------------------------

// TEACHING NOTE -- Play in Engine implementation
// Steps:
//   1. Write the scene to a well-known temp path (%TEMP%\editor_scene.scene.json).
//   2. Find engine_sandbox.exe next to editor.exe (same output folder).
//   3. Launch via ShellExecuteExW with --scene <tempPath>.
//
// GetModuleFileNameW(nullptr, ...) returns the path of the running .exe.
// We strip the filename to get the directory, then look for engine_sandbox.exe.
//
// Production note: in a shipping editor you would write the scene to
// the project's Cooked/ folder and pass a project-relative path.  The temp
// path approach is simpler for a teaching demo.
void EditorApp::LaunchPlayInEngine()
{
    // Build temp scene path: %TEMP%\editor_preview.scene.json
    wchar_t tempDir[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempDir);
    std::wstring tempScene(tempDir);
    tempScene += L"editor_preview.scene.json";

    const std::string tempSceneNarrow = WideToUtf8(tempScene);

    if (!m_sceneEditor.SaveScene(tempSceneNarrow))
    {
        m_statusMessage = "Play in Engine: failed to save temp scene!";
        m_statusTimer   = 5.f;
        return;
    }

    // Find engine_sandbox.exe next to this editor.exe
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring exeDir(exePath);
    const auto lastSlash = exeDir.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos)
        exeDir = exeDir.substr(0, lastSlash);

    std::wstring sandboxExe = exeDir + L"\\engine_sandbox.exe";

    // Build command-line arguments
    std::wstring args = L"--scene \"" + tempScene + L"\"";

    // TEACHING NOTE -- ShellExecuteExW vs CreateProcessW
    // ShellExecuteExW is simpler but does not let us capture the output.
    // CreateProcessW gives full control (redirect stdout/stderr, wait for exit).
    // We use ShellExecuteExW here for brevity; swap to CreateProcessW if you
    // want to show the engine output in the editor's console panel.
    SHELLEXECUTEINFOW sei = {};
    sei.cbSize      = sizeof(sei);
    sei.fMask       = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb      = L"open";
    sei.lpFile      = sandboxExe.c_str();
    sei.lpParameters = args.c_str();
    sei.lpDirectory = exeDir.c_str();
    sei.nShow       = SW_SHOWNORMAL;

    if (ShellExecuteExW(&sei))
    {
        m_statusMessage = "Play in Engine: launched engine_sandbox.exe";
        m_statusTimer   = 4.f;
        if (sei.hProcess)
            CloseHandle(sei.hProcess);
    }
    else
    {
        m_statusMessage = "Play in Engine: engine_sandbox.exe not found next to editor.exe";
        m_statusTimer   = 6.f;
    }
}

// ---------------------------------------------------------------------------
// Status bar
// ---------------------------------------------------------------------------

// TEACHING NOTE -- Simulating a Status Bar with ImGui
// ImGui has no built-in "status bar" widget.  We simulate one by creating
// a small window pinned to the bottom of the viewport with no decorations.
// ImGuiWindowFlags_NoDecoration removes title bar, scrollbar, resize grip.
// ImGuiWindowFlags_NoMove prevents the user from dragging it away.
void EditorApp::RenderStatusBar()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    constexpr float barHeight     = 22.f;

    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - barHeight));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, barHeight));
    ImGui::SetNextWindowBgAlpha(0.85f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration    |
        ImGuiWindowFlags_NoMove          |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (ImGui::Begin("##statusbar", nullptr, flags))
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.f);  // vertical centre
        ImGui::TextUnformatted(m_statusMessage.c_str());

        // Show entity count + project path on the right side
        const int entCount = static_cast<int>(m_sceneEditor.GetEntities().size());
        std::string right;
        if (entCount > 0)
            right = std::to_string(entCount) + " entit" + (entCount == 1 ? "y" : "ies");
        if (!m_projectPath.empty())
        {
            if (!right.empty()) right += "   |   ";
            right += "Project: " + m_projectPath;
        }

        if (!right.empty())
        {
            float textWidth = ImGui::CalcTextSize(right.c_str()).x;
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - textWidth);
            ImGui::TextDisabled("%s", right.c_str());
        }
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Native file dialogs
// ---------------------------------------------------------------------------

// TEACHING NOTE -- IFileOpenDialog (modern Windows folder picker)
// The classic SHBrowseForFolderW dialog is old (Windows 3.1 era).
// The modern alternative is IFileOpenDialog with FOS_PICKFOLDERS set --
// it uses the Vista-style folder picker with favourites, breadcrumb nav, etc.
// This is the same approach used in modern Win32 applications.
std::string EditorApp::PickFolder(const char* /*title*/)
{
    std::string result;
    IFileOpenDialog* pFolderDialog = nullptr;

    HRESULT hr = CoCreateInstance(
        CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&pFolderDialog));

    if (SUCCEEDED(hr))
    {
        // Set options: pick folders (not files), don't show hidden items
        DWORD dwOptions;
        pFolderDialog->GetOptions(&dwOptions);
        pFolderDialog->SetOptions(dwOptions | FOS_PICKFOLDERS);

        if (SUCCEEDED(pFolderDialog->Show(nullptr)))
        {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(pFolderDialog->GetResult(&pItem)))
            {
                PWSTR pszPath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath)))
                {
                    // Convert wide-char path to UTF-8 std::string
                    int len = WideCharToMultiByte(
                        CP_UTF8, 0, pszPath, -1, nullptr, 0, nullptr, nullptr);
                    if (len > 0)
                    {
                        result.resize(static_cast<size_t>(len - 1));
                        WideCharToMultiByte(
                            CP_UTF8, 0, pszPath, -1,
                            result.data(), len, nullptr, nullptr);
                    }
                    CoTaskMemFree(pszPath);
                }
                pItem->Release();
            }
        }
        pFolderDialog->Release();
    }
    return result;
}

// TEACHING NOTE -- GetSaveFileName (classic Win32 save dialog)
// GetSaveFileNameW shows the standard "Save As" dialog. The filter string
// uses pairs of "Description\0*.ext\0" terminated by an extra \0.
// lpstrDefExt specifies the extension appended if the user omits it.
std::string EditorApp::PickSaveFile(const char* /*title*/, const char* filter,
                                    const char* defaultExt)
{
    // Convert narrow filter to wide
    // Count filter length including all embedded NULs
    size_t filterLen = 0;
    const char* p    = filter;
    while (*p || *(p + 1)) { ++filterLen; ++p; }
    filterLen += 2;  // trailing double-NUL

    std::wstring wFilter(filterLen, L'\0');
    for (size_t i = 0; i < filterLen; ++i)
        wFilter[i] = static_cast<wchar_t>(static_cast<unsigned char>(filter[i]));

    std::wstring wDefExt(defaultExt, defaultExt + strlen(defaultExt));

    wchar_t szFile[MAX_PATH] = {};

    OPENFILENAMEW ofn   = {};
    ofn.lStructSize     = sizeof(ofn);
    ofn.lpstrFilter     = wFilter.c_str();
    ofn.lpstrFile       = szFile;
    ofn.nMaxFile        = MAX_PATH;
    ofn.lpstrDefExt     = wDefExt.c_str();
    ofn.Flags           = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    std::string result;
    if (GetSaveFileNameW(&ofn))
    {
        int len = WideCharToMultiByte(
            CP_UTF8, 0, szFile, -1, nullptr, 0, nullptr, nullptr);
        if (len > 0)
        {
            result.resize(static_cast<size_t>(len - 1));
            WideCharToMultiByte(
                CP_UTF8, 0, szFile, -1, result.data(), len, nullptr, nullptr);
        }
    }
    return result;
}

// TEACHING NOTE -- GetOpenFileName (classic Win32 open dialog)
// Mirrors PickSaveFile but uses OFN_FILEMUSTEXIST instead of OFN_OVERWRITEPROMPT.
// This is the standard "Open File" dialog used in all Win32 applications.
std::string EditorApp::PickOpenFile(const char* /*title*/, const char* filter)
{
    size_t filterLen = 0;
    const char* p    = filter;
    while (*p || *(p + 1)) { ++filterLen; ++p; }
    filterLen += 2;

    std::wstring wFilter(filterLen, L'\0');
    for (size_t i = 0; i < filterLen; ++i)
        wFilter[i] = static_cast<wchar_t>(static_cast<unsigned char>(filter[i]));

    wchar_t szFile[MAX_PATH] = {};

    OPENFILENAMEW ofn   = {};
    ofn.lStructSize     = sizeof(ofn);
    ofn.lpstrFilter     = wFilter.c_str();
    ofn.lpstrFile       = szFile;
    ofn.nMaxFile        = MAX_PATH;
    ofn.Flags           = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    std::string result;
    if (GetOpenFileNameW(&ofn))
    {
        int len = WideCharToMultiByte(
            CP_UTF8, 0, szFile, -1, nullptr, 0, nullptr, nullptr);
        if (len > 0)
        {
            result.resize(static_cast<size_t>(len - 1));
            WideCharToMultiByte(
                CP_UTF8, 0, szFile, -1, result.data(), len, nullptr, nullptr);
        }
    }
    return result;
}
