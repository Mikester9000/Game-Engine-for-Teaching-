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
 */

#pragma once

#include <string>
#include <vector>

/**
 * @brief Data for a single entity placed in the scene.
 *
 * TEACHING NOTE -- Plain data struct (no Qt types)
 * The Qt version used QString for id and name.  Now we use std::string --
 * C++ standard library types require no external framework.
 */
struct SceneEntity
{
    std::string id;      ///< UUID v4 string (from Guid.hpp)
    std::string name;    ///< Human-readable entity name
    float       x = 0;  ///< World X position
    float       y = 0;  ///< World Y position
};

/**
 * @class SceneEditorPanel
 * @brief An ImGui panel with a 2D canvas for placing entities, saving as JSON.
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
     * @brief Render the scene editor panel for this frame.
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

private:
    void RenderCanvas();
    void RenderEntityList();   ///< Side list showing entity names + selected highlight
    void HandleEntityPopup();  ///< Immediate-mode "New Entity" name popup

    // ---- Scene state -------------------------------------------------------
    std::vector<SceneEntity> m_entities;
    int    m_selectedIdx    = -1;
    std::string m_sceneName = "Untitled";
    std::string m_filePath;          ///< Last save path

    // ---- Canvas state -------------------------------------------------------
    static constexpr float kGridSize   = 32.f;   ///< Grid cell size in pixels
    static constexpr float kEntitySize = 16.f;   ///< Half-size of entity box

    // ---- New-entity popup state --------------------------------------------
    bool  m_openEntityPopup     = false;  ///< Open the popup next frame
    float m_pendingClickX       = 0;      ///< Canvas X of pending click
    float m_pendingClickY       = 0;      ///< Canvas Y of pending click
    char  m_nameBuffer[128]     = {};     ///< ImGui::InputText buffer
};
