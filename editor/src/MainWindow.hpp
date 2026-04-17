/**
 * @file MainWindow.hpp
 * @brief Creation Suite Editor — top-level application window.
 *
 * =============================================================================
 * TEACHING NOTE — QMainWindow
 * =============================================================================
 * QMainWindow provides a standard "application window" layout with:
 *   • Menu bar       — File, Edit, View, Build menus
 *   • Toolbar        — Quick-access tool buttons
 *   • Central widget — The primary editing area (scene editor canvas)
 *   • Dock widgets   — Panels that can be docked/undocked (content browser,
 *                      inspector, console)
 *   • Status bar     — Bottom bar for messages and progress indicators
 *
 * By deriving from QMainWindow (rather than QWidget), we get all of this
 * layout infrastructure for free and can focus on tool-specific logic.
 *
 * TEACHING NOTE — Q_OBJECT macro
 * Any class that uses Qt signals, slots, or properties MUST declare
 * Q_OBJECT in its private section.  This tells AUTOMOC to generate the
 * moc_*.cpp file that implements the meta-object protocol (reflection,
 * runtime type info, signal/slot wiring).
 *
 * =============================================================================
 */

#pragma once

#include <QMainWindow>
#include <QString>
#include <QLabel>

// Forward declarations — avoids including heavy Qt headers in this header
class ContentBrowser;
class SceneEditor;
class QDockWidget;
class QAction;

/**
 * @class MainWindow
 * @brief The main application window for the Creation Suite Editor.
 *
 * Layout:
 *   ┌──────────────────────────────────────────┐
 *   │  Menu Bar (File | View | Build | Help)   │
 *   ├──────────────────────────────────────────┤
 *   │  Toolbar                                 │
 *   ├──────────┬───────────────────────────────┤
 *   │ Content  │   Scene Editor (central)      │
 *   │ Browser  │                               │
 *   │ (dock)   │                               │
 *   ├──────────┴───────────────────────────────┤
 *   │  Status Bar                              │
 *   └──────────────────────────────────────────┘
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT  // MUST be first in private section when using signals/slots

public:
    /**
     * @brief Construct and initialise the main window.
     * @param parent Parent widget (nullptr for a top-level window).
     */
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    // ---------------------------------------------------------------------------
    // TEACHING NOTE — Qt Slots
    // Slots are ordinary member functions that can be connected to signals.
    // When a signal is emitted, all connected slots are called automatically.
    // The 'slots' keyword (here in the 'private slots:' section) is a Qt macro
    // that marks these methods for the meta-object system.
    // ---------------------------------------------------------------------------

    /// Called when File → Open Project is triggered.
    void onOpenProject();

    /// Called when File → Save Scene is triggered.
    void onSaveScene();

    /// Called when File → Exit is triggered.
    void onExit();

    /// Called when Build → Cook Assets is triggered.
    void onCookAssets();

    /// Called when the content browser selects a file.
    void onContentFileSelected(const QString& path);

private:
    // Setup helpers — called from the constructor
    void setupMenuBar();
    void setupToolBar();
    void setupDockWidgets();
    void setupStatusBar();

    // ---------------------------------------------------------------------------
    // Member variables
    // ---------------------------------------------------------------------------
    // TEACHING NOTE — Raw pointer vs. smart pointer in Qt
    // Qt uses a parent-child ownership model: when a parent widget is destroyed,
    // all its children are destroyed automatically.  This is why Qt code often
    // uses raw pointers for child widgets — the parent owns them.
    // For non-Qt resources, prefer std::unique_ptr / std::shared_ptr.
    // ---------------------------------------------------------------------------

    ContentBrowser* m_contentBrowser = nullptr;   ///< Content/file browser panel
    SceneEditor*    m_sceneEditor    = nullptr;   ///< Central scene editing canvas
    QDockWidget*    m_contentDock    = nullptr;   ///< Dock that holds the content browser
    QLabel*         m_statusLabel    = nullptr;   ///< Status bar label for messages

    QString         m_projectPath;   ///< Absolute path to the currently open project folder
};
