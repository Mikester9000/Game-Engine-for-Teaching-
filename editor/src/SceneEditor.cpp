/**
 * @file SceneEditor.cpp
 * @brief 2D scene/map editor widget implementation.
 *
 * =============================================================================
 * TEACHING NOTE — What this file teaches
 * =============================================================================
 *  1. Custom Qt widget painting (paintEvent + QPainter)
 *  2. Mouse interaction (mousePressEvent)
 *  3. JSON file I/O using Qt's built-in QJsonDocument classes
 *  4. The shared scene schema format (see shared/schemas/scene.schema.json)
 *
 * TEACHING NOTE — Qt JSON API
 * Qt 5/6 includes a built-in JSON API (no external library needed):
 *   QJsonDocument — top-level container (array or object)
 *   QJsonObject   — key/value dictionary
 *   QJsonArray    — ordered list of values
 *   QJsonValue    — a single JSON value (string, number, bool, null, obj, arr)
 *
 * Round-trip:
 *   QJsonDocument → QByteArray (toJson)  → write to file
 *   QByteArray    → QJsonDocument (fromJson) → navigate
 *
 * =============================================================================
 */

#include "SceneEditor.hpp"

#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QUuid>
#include <QFontMetrics>
#include <QApplication>
#include <QDateTime>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

SceneEditor::SceneEditor(QWidget* parent)
    : QWidget(parent)
{
    // TEACHING NOTE — setFocusPolicy
    // By default, most widgets don't accept keyboard focus.
    // Qt::StrongFocus means this widget gets focus when clicked, or when
    // Tab is pressed.  Required for keyPressEvent to fire.
    setFocusPolicy(Qt::StrongFocus);

    // TEACHING NOTE — setBackground
    // The background is set via the palette, not by drawing in paintEvent.
    // This lets Qt handle clearing the background efficiently before calling
    // our paintEvent.
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(40, 40, 40));   // Dark editor background
    setPalette(pal);

    setMinimumSize(400, 300);
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void SceneEditor::paintEvent(QPaintEvent* /*event*/)
{
    // TEACHING NOTE — QPainter
    // QPainter must be constructed on the stack inside paintEvent.
    // DO NOT cache or store it across calls — it is only valid during the
    // paint event.  Creating it on the stack also ensures its destructor
    // is called before the event handler returns (which flushes the paint).
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // ---- Draw background grid ------------------------------------------------
    // TEACHING NOTE — QPen
    // QPen controls line drawing: colour, width, style (solid/dashed/dotted).
    // QBrush controls fill: colour, pattern (solid/hatched/gradient).
    const QColor gridColor(60, 60, 60);
    painter.setPen(QPen(gridColor, 1));

    const int w = width();
    const int h = height();

    // Vertical grid lines
    for (int x = 0; x < w; x += kGridSize)
        painter.drawLine(x, 0, x, h);

    // Horizontal grid lines
    for (int y = 0; y < h; y += kGridSize)
        painter.drawLine(0, y, w, y);

    // ---- Draw entities -------------------------------------------------------
    // TEACHING NOTE — Drawing with loops
    // We iterate over all entities and draw each one.  For each entity we:
    //   1. Choose a fill colour (different for selected vs. normal).
    //   2. Draw a rectangle representing the entity.
    //   3. Draw the entity name above the rectangle.
    for (int i = 0; i < static_cast<int>(m_entities.size()); ++i)
    {
        const SceneEntity& ent = m_entities[static_cast<size_t>(i)];

        const bool selected = (i == m_selectedIdx);
        QColor fillColor  = selected ? QColor(255, 180, 0)   : QColor(100, 180, 255);
        QColor borderColor = selected ? QColor(255, 220, 50) : QColor(50, 130, 220);

        QRectF rect(ent.x - 16, ent.y - 16, 32, 32);  // 32×32 entity box

        // Fill
        painter.setBrush(QBrush(fillColor));
        painter.setPen(QPen(borderColor, selected ? 2 : 1));
        painter.drawRect(rect);

        // Entity name label
        painter.setPen(Qt::white);
        painter.setFont(QFont("Consolas", 8));
        painter.drawText(QPointF(ent.x - 20, ent.y - 20), ent.name);
    }

    // ---- Help text when empty -----------------------------------------------
    if (m_entities.empty())
    {
        painter.setPen(QColor(120, 120, 120));
        painter.setFont(QFont("Segoe UI", 12));
        painter.drawText(rect(), Qt::AlignCenter,
            "Left-click to place an entity.\n"
            "Select an entity, then press Delete to remove it.\n"
            "Use File → Save Scene to save as JSON.");
    }

    // ---- Scene name (top-left corner) ----------------------------------------
    painter.setPen(QColor(180, 180, 180));
    painter.setFont(QFont("Consolas", 10));
    painter.drawText(QPoint(8, 16), "Scene: " + m_sceneName);
}

// ---------------------------------------------------------------------------
// Mouse events
// ---------------------------------------------------------------------------

void SceneEditor::mousePressEvent(QMouseEvent* event)
{
    // TEACHING NOTE — Mouse Picking
    // "Picking" means figuring out what the user clicked on in the scene.
    // We test each entity's bounding rectangle against the click position.
    // In a 3D editor this becomes a ray-cast against geometry.

    const float mx = static_cast<float>(event->pos().x());
    const float my = static_cast<float>(event->pos().y());

    if (event->button() == Qt::LeftButton)
    {
        // Test if an existing entity was clicked
        for (int i = 0; i < static_cast<int>(m_entities.size()); ++i)
        {
            const SceneEntity& ent = m_entities[static_cast<size_t>(i)];
            if (std::abs(mx - ent.x) < 16 && std::abs(my - ent.y) < 16)
            {
                m_selectedIdx = i;
                update();   // TEACHING NOTE — update() schedules a repaint
                return;
            }
        }

        // Nothing hit — ask the user for a name and place a new entity
        bool ok = false;
        QString name = QInputDialog::getText(
            this, "New Entity",
            "Entity name:",
            QLineEdit::Normal,
            QString("Entity_%1").arg(m_entities.size() + 1),
            &ok
        );

        if (ok && !name.isEmpty())
        {
            SceneEntity ent;
            ent.id   = generateId();
            ent.name = name;
            ent.x    = mx;
            ent.y    = my;
            m_entities.push_back(ent);
            m_selectedIdx = static_cast<int>(m_entities.size()) - 1;
            update();
        }
    }
}

// ---------------------------------------------------------------------------
// Keyboard events
// ---------------------------------------------------------------------------

void SceneEditor::keyPressEvent(QKeyEvent* event)
{
    // TEACHING NOTE — Key handling
    // We only handle the Delete key here.  In a full editor you would also
    // handle Ctrl+Z (undo), Ctrl+C/V (copy/paste), etc.

    if (event->key() == Qt::Key_Delete && m_selectedIdx >= 0)
    {
        m_entities.erase(m_entities.begin() + m_selectedIdx);
        m_selectedIdx = -1;
        update();
    }
    else
    {
        // Pass unhandled keys to the base class so Qt can handle them
        QWidget::keyPressEvent(event);
    }
}

// ---------------------------------------------------------------------------
// Scene I/O — JSON using Qt's built-in JSON API
// ---------------------------------------------------------------------------

bool SceneEditor::saveScene(const QString& filePath) const
{
    // TEACHING NOTE — Qt JSON Write
    // We build a QJsonObject (dictionary) and populate it with the scene data.
    // QJsonDocument wraps the root object and writes it to bytes (toJson).

    QJsonObject root;

    // Shared schema reference (relative path for portability)
    root["$schema"] = "../../shared/schemas/scene.schema.json";
    root["version"] = "1.0.0";
    root["name"]    = m_sceneName;

    // Metadata
    QJsonObject meta;
    meta["savedAt"]    = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    meta["editorVersion"] = QApplication::applicationVersion();
    root["meta"] = meta;

    // Entities array
    QJsonArray entitiesArr;
    for (const auto& ent : m_entities)
    {
        QJsonObject entObj;
        entObj["id"]   = ent.id;
        entObj["name"] = ent.name;

        // TEACHING NOTE — Nested JSON objects
        // We put the position inside a "transform" sub-object so that
        // the engine can easily find and parse the Transform component.
        QJsonObject transform;
        transform["x"] = ent.x;
        transform["y"] = ent.y;
        transform["z"] = 0.0;
        entObj["transform"] = transform;

        entitiesArr.append(entObj);
    }
    root["entities"] = entitiesArr;

    // TEACHING NOTE — QJsonDocument::toJson()
    // toJson(QJsonDocument::Indented) writes human-readable JSON with indentation.
    // Use QJsonDocument::Compact for smaller files (e.g., cooked/shipped data).
    QJsonDocument doc(root);
    QByteArray jsonBytes = doc.toJson(QJsonDocument::Indented);

    // Ensure the parent directory exists
    QDir().mkpath(QFileInfo(filePath).absolutePath());

    // Write to file
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    file.write(jsonBytes);
    file.close();
    return true;
}

bool SceneEditor::loadScene(const QString& filePath)
{
    // TEACHING NOTE — Qt JSON Read
    // We read the file to bytes, parse with QJsonDocument::fromJson(), then
    // navigate the resulting QJsonObject / QJsonArray.

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (doc.isNull() || !doc.isObject())
        return false;   // Invalid JSON

    QJsonObject root = doc.object();

    m_sceneName = root.value("name").toString("Untitled");

    m_entities.clear();
    QJsonArray entitiesArr = root.value("entities").toArray();
    for (const QJsonValue& val : entitiesArr)
    {
        QJsonObject entObj = val.toObject();
        SceneEntity ent;
        ent.id   = entObj.value("id").toString(generateId());
        ent.name = entObj.value("name").toString("Entity");

        QJsonObject transform = entObj.value("transform").toObject();
        ent.x = static_cast<float>(transform.value("x").toDouble(0.0));
        ent.y = static_cast<float>(transform.value("y").toDouble(0.0));

        m_entities.push_back(ent);
    }

    m_filePath    = filePath;
    m_selectedIdx = -1;
    update();
    return true;
}

void SceneEditor::newScene()
{
    m_entities.clear();
    m_selectedIdx = -1;
    m_sceneName   = "Untitled";
    m_filePath.clear();
    update();
}

// ---------------------------------------------------------------------------
// ID generation
// ---------------------------------------------------------------------------

QString SceneEditor::generateId()
{
    // TEACHING NOTE — QUuid
    // QUuid generates RFC 4122 compliant UUIDs (128-bit unique identifiers).
    // createUuid() uses platform random source (CryptGenRandom on Windows).
    // toString(QUuid::WithoutBraces) returns "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx".
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
