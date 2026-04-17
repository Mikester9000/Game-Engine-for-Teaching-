/**
 * @file SceneEditor.hpp
 * @brief 2D scene/map editor widget — entity placement + JSON save.
 *
 * =============================================================================
 * TEACHING NOTE — Custom Painting with QWidget
 * =============================================================================
 * Qt lets you draw anything inside a QWidget by overriding paintEvent().
 * Inside paintEvent() you create a QPainter and call drawing commands:
 *   painter.drawRect(), painter.drawLine(), painter.drawText(), etc.
 * This is how game editors render their 2D viewports (before switching to
 * OpenGL / Vulkan for 3D).
 *
 * The painting model:
 *   1. Qt calls paintEvent() when the widget needs to be redrawn.
 *   2. You call update() to schedule a repaint (never call paintEvent directly).
 *   3. QPainter works in logical coordinates; Qt maps them to device pixels.
 *
 * TEACHING NOTE — Scene / Map Data Format
 * The scene is saved as a JSON file following shared/schemas/scene.schema.json.
 * The format is intentionally simple for teaching:
 *   {
 *     "$schema": "../../shared/schemas/scene.schema.json",
 *     "version": "1.0.0",
 *     "name": "MyScene",
 *     "entities": [
 *       { "id": "<uuid>", "name": "Player", "x": 100, "y": 200,
 *         "components": { "Transform": {...}, "Sprite": {...} } }
 *     ]
 *   }
 *
 * =============================================================================
 */

#pragma once

#include <QWidget>
#include <QString>
#include <QPointF>
#include <vector>

/**
 * @brief Data for a single entity placed in the scene.
 *
 * TEACHING NOTE — Simple Data Structures
 * We use a plain struct (no encapsulation) for teaching clarity.
 * In a production editor, each entity would have a full component list
 * and a GUID-based identity.
 */
struct SceneEntity
{
    QString id;      ///< UUID v4 string (e.g. "550e8400-e29b-41d4-a716-446655440000")
    QString name;    ///< Human-readable name (e.g. "Player", "Chest_01")
    float   x = 0;  ///< World X position (pixels or engine units)
    float   y = 0;  ///< World Y position
};

/**
 * @class SceneEditor
 * @brief A 2D canvas widget that lets you place named entities and save them.
 *
 * Controls (implemented in mouse event handlers):
 *   • Left click on empty space — place a new entity.
 *   • Left click on entity     — select it.
 *   • Right click              — delete selected entity (TODO).
 *   • Mouse drag               — pan the view (TODO).
 *
 * Saving writes a JSON file following shared/schemas/scene.schema.json.
 */
class SceneEditor : public QWidget
{
    Q_OBJECT

public:
    explicit SceneEditor(QWidget* parent = nullptr);
    ~SceneEditor() override = default;

    // ---- Scene I/O -----------------------------------------------------------

    /**
     * @brief Save the current scene to @p filePath as JSON.
     * @return true on success; false on file write failure.
     */
    bool saveScene(const QString& filePath) const;

    /**
     * @brief Load a scene from a JSON file previously saved by saveScene().
     * @return true on success.
     */
    bool loadScene(const QString& filePath);

    /** @brief Clear all entities and reset to an empty scene. */
    void newScene();

protected:
    // ---------------------------------------------------------------------------
    // TEACHING NOTE — Qt Event Handlers (virtual overrides)
    // QWidget declares many virtual methods that you override to handle events:
    //   paintEvent     — draw the widget
    //   mousePressEvent — mouse button down
    //   mouseReleaseEvent — mouse button up
    //   keyPressEvent  — key down
    //   resizeEvent    — widget resized
    // Overriding these is safe; Qt calls them automatically.
    // ---------------------------------------------------------------------------

    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    // ---------------------------------------------------------------------------
    // Helper to generate a simple UUID-like string (not cryptographically random,
    // but unique enough for a teaching project)
    // ---------------------------------------------------------------------------
    static QString generateId();

    // ---- Data ----------------------------------------------------------------
    std::vector<SceneEntity> m_entities;         ///< All entities in the scene
    int                      m_selectedIdx = -1; ///< Index of selected entity (-1 = none)
    QString                  m_sceneName = "Untitled";  ///< Name shown in save file
    QString                  m_filePath;         ///< Last save path (empty = not yet saved)

    // Grid drawing settings
    static constexpr int kGridSize = 32;         ///< Pixels per grid cell
};
