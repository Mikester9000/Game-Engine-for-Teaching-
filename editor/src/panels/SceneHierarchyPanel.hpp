/**
 * @file SceneHierarchyPanel.hpp
 * @brief Scene Hierarchy Panel — dockable entity tree for the editor (M6).
 *
 * =============================================================================
 * TEACHING NOTE — Scene Hierarchy Panel
 * =============================================================================
 * Every professional game editor has a "hierarchy" or "outliner" panel:
 *   • Unity  — "Hierarchy" window (left side by default).
 *   • Unreal — "Outliner" panel (top-right by default).
 *   • Godot  — "Scene" dock (left side).
 *
 * The hierarchy shows all entities in the current scene as a list (M6) or tree
 * (future — for parent/child relationships).  Clicking an entity selects it;
 * the selection is reflected in both the canvas and the InspectorPanel.
 *
 * =============================================================================
 * TEACHING NOTE — Shared State via Pointer
 * =============================================================================
 * SceneHierarchyPanel does NOT own the entity data.  Instead it holds a
 * pointer to SceneEditorPanel and reads/writes through its public accessor API:
 *
 *   m_scenePanel->GetEntities()     — read entity list
 *   m_scenePanel->GetSelectedIdx()  — read selection
 *   m_scenePanel->SetSelectedIdx()  — write selection
 *   m_scenePanel->DeleteEntity(i)   — delete entity
 *
 * This pattern (a "non-owning view" or "observer") avoids duplicating state
 * and keeps the hierarchy in sync with the canvas at zero extra cost.
 *
 * In a larger editor you might use a shared_ptr<SceneDocument> or an event bus
 * to decouple the panels further.  For a teaching project, the pointer approach
 * is deliberately simple and explicit.
 *
 * =============================================================================
 */

#pragma once

#include "../SceneEditorPanel.hpp"

/**
 * @class SceneHierarchyPanel
 * @brief An ImGui dockable panel that lists all scene entities.
 *
 * Features (M6):
 *   • Click to select an entity (reflected in InspectorPanel and canvas).
 *   • Double-click to rename an entity (in-place popup).
 *   • Right-click context menu: Rename, Delete.
 *   • "+" button at the top to add a new entity at the world origin.
 *   • Entity count shown in the panel header.
 */
class SceneHierarchyPanel
{
public:
    /// Construct the panel; @p scenePanel must outlive this object.
    explicit SceneHierarchyPanel(SceneEditorPanel* scenePanel = nullptr)
        : m_scenePanel(scenePanel) {}

    /**
     * @brief Attach (or re-attach) the SceneEditorPanel this hierarchy mirrors.
     * @param scenePanel  Pointer to the owning SceneEditorPanel (non-owning).
     */
    void SetScenePanel(SceneEditorPanel* scenePanel) { m_scenePanel = scenePanel; }

    /**
     * @brief Render the hierarchy panel for the current frame.
     *
     * Call inside the EditorApp::Render() ImGui frame.
     */
    void Render();

private:
    void RenderAddEntityButton();
    void RenderEntityList();
    void RenderRenamePopup();
    void RenderContextMenu(int idx);

    SceneEditorPanel* m_scenePanel = nullptr;  ///< Non-owning pointer to scene data

    // Rename popup state
    bool m_renamePopupOpen  = false;  ///< Open the rename popup next frame
    int  m_renameTargetIdx  = -1;     ///< Which entity is being renamed
    char m_renameBuffer[128] = {};    ///< ImGui::InputText buffer for rename
};
