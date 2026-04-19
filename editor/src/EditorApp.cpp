/**
 * @file EditorApp.cpp
 * @brief Top-level editor application implementation.
 */

#include "EditorApp.hpp"

#include <imgui.h>
#include <string>
#include <filesystem>

// Windows native file dialogs
#include <windows.h>
#include <shobjidl.h>   // IFileOpenDialog (modern COM-based dialog)
#include <shlobj.h>     // SHBrowseForFolderW (legacy folder picker)

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

EditorApp::EditorApp()
{
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
    ImGui::DockSpaceOverViewport(
        ImGui::GetMainViewport(),
        ImGuiDockNodeFlags_PassthruCentralNode
    );

    // ---- Menu bar ----------------------------------------------------------
    RenderMenuBar();

    // ---- Panels ------------------------------------------------------------
    m_contentBrowser.Render();
    m_sceneEditor.Render();

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

        if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
        {
            if (m_projectPath.empty())
            {
                m_statusMessage = "No project open. Use File > Open Project first.";
                m_statusTimer   = 5.f;
            }
            else
            {
                std::string defaultDir = m_projectPath + "/Content/Maps";
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

        // Show project path on the right side
        if (!m_projectPath.empty())
        {
            const std::string label = "Project: " + m_projectPath;
            float textWidth = ImGui::CalcTextSize(label.c_str()).x;
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - textWidth);
            ImGui::TextDisabled("%s", label.c_str());
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
