/**
 * @file InspectorPanel.hpp
 * @brief Entity Inspector / Property Editor panel for the editor (M6).
 *
 * =============================================================================
 * TEACHING NOTE — Inspector / Property Editor Pattern
 * =============================================================================
 * Every mainstream game editor has an "inspector" or "details" panel:
 *   • Unity  — "Inspector" window (right side by default).
 *   • Unreal — "Details" panel (right side by default).
 *   • Godot  — "Inspector" dock (right side).
 *
 * The inspector shows the properties of the currently selected entity.
 * Each component is shown as a collapsible section with editable fields.
 *
 * =============================================================================
 * TEACHING NOTE — Reflection-free property editing
 * =============================================================================
 * Commercial engines usually use C++ reflection (run-time type information
 * about struct fields) to drive the inspector.  Unreal uses UPROPERTYs and a
 * generated reflection database; Unity uses C# reflection attributes.
 *
 * For our teaching engine, components don't have runtime reflection yet.
 * Instead, we use a TABLE-DRIVEN approach:
 *
 *   1. The editor stores component data as nlohmann::json blobs inside
 *      SceneEntity::components (a JSON object keyed by component type name).
 *   2. InspectorPanel has a hand-written table of "known component types" with
 *      their field names, types, and ranges.
 *   3. To add inspector support for a new component type, add an entry to the
 *      kComponentDefs table in InspectorPanel.cpp — no template magic needed.
 *
 * This approach is O(1) to understand for students and O(N) to extend (N = new
 * field definitions), which is exactly right for a teaching project.
 *
 * =============================================================================
 */

#pragma once

#include "../SceneEditorPanel.hpp"

/**
 * @class InspectorPanel
 * @brief An ImGui dockable panel that shows/edits the selected entity's components.
 *
 * Features (M6):
 *   • Editable entity name and UUID (UUID is read-only).
 *   • Transform (x, y, z) with drag-float inputs.
 *   • Per-component collapsible sections with appropriate input widgets.
 *   • "Add Component" popup listing supported component types.
 *   • "Remove" button per component (via right-click or X button).
 *   • Changes are written back into SceneEntity::components (JSON).
 */
class InspectorPanel
{
public:
    /// Construct the panel; @p scenePanel must outlive this object.
    explicit InspectorPanel(SceneEditorPanel* scenePanel = nullptr)
        : m_scenePanel(scenePanel) {}

    /**
     * @brief Attach (or re-attach) the SceneEditorPanel this inspector mirrors.
     */
    void SetScenePanel(SceneEditorPanel* scenePanel) { m_scenePanel = scenePanel; }

    /**
     * @brief Render the inspector panel for the current frame.
     */
    void Render();

private:
    void RenderNoSelection();
    void RenderEntityHeader(SceneEntity& ent);
    void RenderTransformSection(SceneEntity& ent);
    void RenderComponentSections(SceneEntity& ent);
    void RenderAddComponentButton(SceneEntity& ent);
    void RenderAddComponentPopup(SceneEntity& ent);

    SceneEditorPanel* m_scenePanel = nullptr;

    bool m_addCompPopupOpen = false;  ///< Open the "Add Component" popup next frame
};
