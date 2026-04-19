/**
 * @file EditorApp.hpp
 * @brief Top-level editor application -- DockSpace, menu bar, project state.
 *
 * =============================================================================
 * TEACHING NOTE -- ImGui DockSpace Architecture
 * =============================================================================
 * Dear ImGui's docking system lets you create a layout of dockable panels,
 * exactly like Unreal Editor, Unity Editor, or any professional tool.
 *
 * The central concept is a DockSpace:
 *   ImGuiID dockId = ImGui::DockSpaceOverViewport();
 *
 * A DockSpace is a region of the window into which ImGui "windows" can be
 * docked (pinned) or float freely.  The first time the editor runs, panels
 * start floating.  The user drags them to dock positions; ImGui saves the
 * layout to creation-suite-editor.ini automatically.
 *
 * Compare to the Qt retained-mode approach where dock widgets were explicit
 * QDockWidget objects added to a QMainWindow via addDockWidget().
 * With ImGui there is no "dock widget" object -- any ImGui::Begin("Name")
 * window can be docked just by dragging it.
 *
 * =============================================================================
 * TEACHING NOTE -- Application State in Immediate-Mode UI
 * =============================================================================
 * In Qt, application state (current project path, status message ...) was
 * stored as member variables of QMainWindow and updated via signals/slots.
 *
 * In ImGui the same state lives as member variables of EditorApp.
 * There are no signals or slots -- each frame EditorApp::Render() reads the
 * state and decides what to draw.  When the user clicks a button, the state
 * is updated immediately (same frame, same call stack).
 *
 * This makes control flow much easier to follow for students:
 *   if (ImGui::MenuItem("Open Project..."))
 *   {
 *       // open file dialog -- happens RIGHT HERE, no callback needed
 *       ...
 *       m_projectPath = result;
 *   }
 *
 * =============================================================================
 */

#pragma once

#include <string>
#include "ContentBrowserPanel.hpp"
#include "SceneEditorPanel.hpp"

/**
 * @class EditorApp
 * @brief Owns and renders the entire editor: menu bar, DockSpace, and panels.
 *
 * Render() is called every frame from main.cpp after ImGui::NewFrame().
 */
class EditorApp
{
public:
    EditorApp();

    /**
     * @brief Render the entire editor UI for the current frame.
     *
     * Call order:
     *   1. Full-screen DockSpace (background window)
     *   2. Main menu bar
     *   3. ContentBrowserPanel::Render()
     *   4. SceneEditorPanel::Render()
     *   5. Status bar
     */
    void Render();

private:
    // ---- Helper methods ----------------------------------------------------
    void RenderMenuBar();
    void RenderStatusBar();

    // ---- File dialog helpers (Windows native via COMDLG32) -----------------
    // TEACHING NOTE -- Native file dialogs without Qt
    // Qt provides QFileDialog::getExistingDirectory() etc.
    // Without Qt we use the Win32 SHBrowseForFolderW API (folder picker) or
    // IFileOpenDialog COM interface (modern Windows file dialog).
    // These helpers wrap those calls and return UTF-8 std::string results.
    std::string PickFolder(const char* title);
    std::string PickSaveFile(const char* title, const char* filter,
                             const char* defaultExt);

    // ---- Member state ------------------------------------------------------
    ContentBrowserPanel  m_contentBrowser;   ///< Left dock: file tree
    SceneEditorPanel     m_sceneEditor;      ///< Center: scene canvas

    std::string  m_projectPath;      ///< Absolute path to currently open project
    std::string  m_statusMessage;    ///< Bottom status bar text
    float        m_statusTimer = 0;  ///< Seconds remaining to show transient status

    bool         m_showAbout  = false;  ///< Show the About popup this frame
};
