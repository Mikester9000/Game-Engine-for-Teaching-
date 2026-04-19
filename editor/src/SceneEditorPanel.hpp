/**
 * @file SceneEditorPanel.hpp
 * @brief 2D scene/map editor panel -- entity placement and JSON save/load.
 *
 * =============================================================================
 * TEACHING NOTE -- ImGui Custom Drawing with DrawList
 * =============================================================================
 * The Qt version painted the scene by overriding QWidget::paintEvent() and
 * using QPainter:
 *   painter.drawRect(...)
 *   painter.drawText(...)
 *
 * In Dear ImGui there is no paintEvent -- instead, every panel gets access to
 * an ImDrawList (a list of draw commands) via:
 *   ImDrawList* drawList = ImGui::GetWindowDrawList();
 *
 * You call AddRect(), AddLine(), AddText() etc. on the draw list directly.
 * Draw commands are batched and executed by the D3D11 backend at Render() time.
 *
 * The canvas is created with ImGui::BeginChild() -- a scrollable sub-region
 * within a parent ImGui window.  This is the standard pattern for any
 * custom-rendered viewport (scene editor, animation timeline, node graph ...).
 *
 * =============================================================================
 * TEACHING NOTE -- ImGui Popup for Text Input
 * =============================================================================
 * The Qt version used QInputDialog::getText() which blocks the event loop
 * until the user clicks OK or Cancel.
 *
 * ImGui is single-threaded and non-blocking.  To get text input:
 *   1. Set a flag when input is needed (m_pendingEntityName = true).
 *   2. On the NEXT frame, call ImGui::OpenPopup() + ImGui::BeginPopup().
 *   3. Inside the popup, call ImGui::InputText() and wait for the user.
 *   4. On confirmation, use the entered text and close the popup.
 *
 * This is the immediate-mode equivalent of a blocking dialog.
 *
 * =============================================================================
 * TEACHING NOTE -- JSON Scene Format
 * =============================================================================
 * The scene is saved as a JSON file following shared/schemas/scene.schema.json.
 * We use nlohmann-json (already in vcpkg.json) instead of Qt's QJsonDocument.
 * The format is identical -- the engine reads the same .scene.json files
 * regardless of whether they were saved by the Qt or the ImGui editor.
 *
 * =============================================================================
 * TEACHING NOTE -- ImGui Drag-Drop Target (canvas region)
 * =============================================================================
 * The scene canvas acts as a drop target for asset files dragged from the
 * ContentBrowserPanel.  When a "CONTENT_ASSET" payload is dropped onto the
 * canvas, a new entity is created at the canvas-relative drop position.
 *
 * The key pattern:
 *   1. Call ImGui::BeginChild("##canvas", ...) to create the canvas sub-region.
 *   2. After ImGui::EndChild(), the canvas is the "last item" in the parent
 *      window.  Calling ImGui::BeginDragDropTarget() immediately after
 *      EndChild() attaches the drop target to that last item (the canvas).
 *   3. Inside the target block, AcceptDragDropPayload("CONTENT_ASSET") returns
 *      a non-null pointer when the user releases the drag over the canvas.
 *   4. We read the file path from the payload, compute the drop position
 *      relative to the canvas origin, and create a SceneEntity there.
 *
 * This is the recommended ImGui drag-drop pattern for large "receiver" regions.
 *
 * =============================================================================
 */

#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

/**
 * @brief Data for a single entity placed in the scene.
 *
 * TEACHING NOTE -- Plain data struct (no Qt types)
 * The Qt version used QString for id and name.  Now we use std::string --
 * C++ standard library types require no external framework.
 *
 * The 'components' field stores optional ECS-style component data as a JSON
 * object, e.g.:
 *   components["HealthComponent"] = { {"hp", 100}, {"maxHp", 100} }
 *
 * This allows the editor to round-trip rich component data through scene files
 * without hard-coding every component type in SceneEntity.  The InspectorPanel
 * reads and writes this field to provide per-component property editing.
 */
struct SceneEntity
{
    std::string id;      ///< UUID v4 string (from Guid.hpp)
    std::string name;    ///< Human-readable entity name
    float       x = 0;  ///< World X position (pixels on canvas / world units)
    float       y = 0;  ///< World Y position
    float       z = 0;  ///< World Z position (depth; not shown on 2D canvas)

    // TEACHING NOTE — JSON component bag
    // Using nlohmann::json as a "property bag" lets the editor handle arbitrary
    // component types without recompiling.  New component types added to
    // ECS.hpp are immediately editable as long as the InspectorPanel knows their
    // field names (it uses a per-type table defined in InspectorPanel.cpp).
    nlohmann::json components = nlohmann::json::object();
};

/**
 * @class SceneEditorPanel
 * @brief An ImGui panel with a 2D canvas for placing entities, saving as JSON.
 *
 * M6 note: The entity list sidebar has been moved to SceneHierarchyPanel so
 * this panel is now canvas-only.  Use the public accessor API below to share
 * scene state with SceneHierarchyPanel and InspectorPanel.
 *
 * Controls:
 *   Left click on empty space -- opens "New Entity" name popup.
 *   Left click on entity      -- selects it.
 *   Delete key                -- removes the selected entity.
 */
class SceneEditorPanel
{
public:
    SceneEditorPanel() = default;

    /**
     * @brief Render the scene editor canvas panel for this frame.
     */
    void Render();

    /**
     * @brief Save the current scene to a JSON file.
     * @param filePath Absolute path to write the .scene.json file.
     * @return true on success.
     */
    bool SaveScene(const std::string& filePath) const;

    /**
     * @brief Load a scene from a .scene.json file.
     * @return true on success.
     */
    bool LoadScene(const std::string& filePath);

    /** @brief Clear all entities and reset to a new untitled scene. */
    void NewScene();

    // -------------------------------------------------------------------------
    // Shared-state accessors (used by SceneHierarchyPanel + InspectorPanel)
    // -------------------------------------------------------------------------
    // TEACHING NOTE — Accessor pattern for shared panel state
    // SceneEditorPanel owns the canonical scene data (entity list + selection).
    // SceneHierarchyPanel and InspectorPanel are given a pointer to this panel
    // and call these accessors to read/write shared state every frame.
    //
    // An alternative design is a shared SceneDocument struct owned by EditorApp.
    // The accessor approach is simpler for a teaching project where the number
    // of panels is small and well-defined.
    // -------------------------------------------------------------------------

    /// Mutable access to the entity list (hierarchy/inspector write through here).
    std::vector<SceneEntity>&       GetEntities()       { return m_entities; }
    const std::vector<SceneEntity>& GetEntities() const { return m_entities; }

    /// Currently selected entity index (-1 = none).
    int  GetSelectedIdx() const        { return m_selectedIdx; }
    void SetSelectedIdx(int idx)       { m_selectedIdx = idx;  }

    /// Scene name shown in the title area.
    const std::string& GetSceneName() const          { return m_sceneName; }
    void               SetSceneName(const std::string& n) { m_sceneName = n; }

    /// Pointer to the selected entity, or nullptr if nothing is selected.
    SceneEntity* GetSelectedEntity()
    {
        if (m_selectedIdx < 0 || m_selectedIdx >= static_cast<int>(m_entities.size()))
            return nullptr;
        return &m_entities[static_cast<size_t>(m_selectedIdx)];
    }

    /// Delete the entity at index idx and update selection.
    void DeleteEntity(int idx)
    {
        if (idx < 0 || idx >= static_cast<int>(m_entities.size())) return;
        m_entities.erase(m_entities.begin() + idx);
        // Keep selection in bounds.
        if (m_selectedIdx >= static_cast<int>(m_entities.size()))
            m_selectedIdx = static_cast<int>(m_entities.size()) - 1;
    }

private:
    void RenderCanvas();
    void HandleEntityPopup();  ///< Immediate-mode "New Entity" name popup

    // ---- Scene state -------------------------------------------------------
    std::vector<SceneEntity> m_entities;
    int    m_selectedIdx    = -1;
    std::string m_sceneName = "Untitled";
    std::string m_filePath;          ///< Last save path

    // ---- Canvas state -------------------------------------------------------
    static constexpr float kGridSize   = 32.f;   ///< Grid cell size in pixels
    static constexpr float kEntitySize = 16.f;   ///< Half-size of entity box

    // TEACHING NOTE — Canvas origin for drag-drop target
    // m_canvasOriginX/Y stores the screen-space top-left corner of the canvas
    // child window, recorded in RenderCanvas() and read by the drag-drop target
    // handler immediately after EndChild().  This lets us convert a screen-space
    // mouse position (ImGui::GetMousePos()) into canvas-local coordinates.
    float m_canvasOriginX = 0.f;  ///< Screen-space canvas origin X (updated every frame)
    float m_canvasOriginY = 0.f;  ///< Screen-space canvas origin Y (updated every frame)

    // ---- New-entity popup state --------------------------------------------
    bool  m_openEntityPopup     = false;  ///< Open the popup next frame
    float m_pendingClickX       = 0;      ///< Canvas X of pending click
    float m_pendingClickY       = 0;      ///< Canvas Y of pending click
    char  m_nameBuffer[128]     = {};     ///< ImGui::InputText buffer
};
