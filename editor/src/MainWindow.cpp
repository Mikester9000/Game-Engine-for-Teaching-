/**
 * @file MainWindow.cpp
 * @brief Creation Suite Editor — top-level application window implementation.
 *
 * =============================================================================
 * TEACHING NOTE — Separation of Declaration and Definition
 * =============================================================================
 * Header (.hpp) declares *what* the class looks like (interface).
 * Implementation (.cpp) defines *how* each method works.
 * This separation keeps compile times fast: only files that include the
 * header need recompiling when the interface changes; implementation changes
 * only recompile this one .cpp.
 *
 * =============================================================================
 */

#include "MainWindow.hpp"
#include "ContentBrowser.hpp"
#include "SceneEditor.hpp"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QToolBar>
#include <QDockWidget>
#include <QStatusBar>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    // TEACHING NOTE — setWindowTitle
    // setWindowTitle sets the text shown in the OS title bar.
    // It also appears in the Windows taskbar and Alt-Tab switcher.
    setWindowTitle("Creation Suite Editor — Game Engine for Teaching");

    // Set a comfortable default window size (1280x720 = HD, common for editors)
    resize(1280, 720);

    // ---------------------------------------------------------------------------
    // TEACHING NOTE — Qt Widget Hierarchy
    // We create the scene editor first because QMainWindow::setCentralWidget()
    // requires a widget.  The central widget gets all the space not occupied by
    // menus, toolbars, status bar, and dock widgets.
    // ---------------------------------------------------------------------------
    m_sceneEditor = new SceneEditor(this);   // 'this' = parent → Qt owns it
    setCentralWidget(m_sceneEditor);

    // Build the rest of the UI
    setupMenuBar();
    setupToolBar();
    setupDockWidgets();
    setupStatusBar();

    // Restore the previous window geometry (position, size, dock layout)
    QSettings settings;
    restoreGeometry(settings.value("mainWindow/geometry").toByteArray());
    restoreState(settings.value("mainWindow/state").toByteArray());
}

// ---------------------------------------------------------------------------
// Private setup helpers
// ---------------------------------------------------------------------------

void MainWindow::setupMenuBar()
{
    // TEACHING NOTE — QMenuBar / QMenu / QAction
    // Qt menus follow a three-level hierarchy:
    //   QMenuBar → QMenu → QAction
    // A QAction represents a command (with an optional keyboard shortcut,
    // icon, and tooltip).  The same QAction can appear in a menu AND a toolbar.
    // When triggered, it emits the triggered() signal, which we connect to a slot.

    // ---- File menu ----
    QMenu* fileMenu = menuBar()->addMenu("&File");

    QAction* openAction = fileMenu->addAction("&Open Project…");
    openAction->setShortcut(QKeySequence::Open);
    openAction->setStatusTip("Open a game project folder");
    // TEACHING NOTE — connect() wires a signal to a slot.
    // Syntax: connect(sender, &SenderClass::signal, receiver, &ReceiverClass::slot);
    // Lambda form: connect(sender, &Signal, [=]{ ... });
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenProject);

    QAction* saveAction = fileMenu->addAction("&Save Scene");
    saveAction->setShortcut(QKeySequence::Save);
    saveAction->setStatusTip("Save the current scene to disk");
    connect(saveAction, &QAction::triggered, this, &MainWindow::onSaveScene);

    fileMenu->addSeparator();

    QAction* exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &MainWindow::onExit);

    // ---- Build menu ----
    QMenu* buildMenu = menuBar()->addMenu("&Build");

    QAction* cookAction = buildMenu->addAction("&Cook Assets");
    cookAction->setShortcut(Qt::CTRL | Qt::Key_B);
    cookAction->setStatusTip("Run the asset cook script for the current project");
    connect(cookAction, &QAction::triggered, this, &MainWindow::onCookAssets);

    // ---- Help menu ----
    QMenu* helpMenu = menuBar()->addMenu("&Help");
    QAction* aboutAction = helpMenu->addAction("&About");
    connect(aboutAction, &QAction::triggered, this, [this] {
        QMessageBox::about(this, "About Creation Suite Editor",
            "<h3>Creation Suite Editor v1.0</h3>"
            "<p>Part of the <b>Game Engine for Teaching</b> monorepo.</p>"
            "<p>A minimal Qt Widgets editor that demonstrates how to build "
            "game development tools: project browser, asset content browser, "
            "scene/map editor with JSON save.</p>"
            "<p>Study the source under <code>editor/src/</code>.</p>");
    });
}

void MainWindow::setupToolBar()
{
    // TEACHING NOTE — QToolBar
    // Toolbars give quick access to frequently used actions.
    // QToolBar::addAction() adds the same QAction that lives in the menu —
    // the same object, not a copy — so triggering the menu or the toolbar
    // button both fire the same signal.
    QToolBar* toolbar = addToolBar("Main Toolbar");
    toolbar->setObjectName("mainToolbar");   // needed for restoreState()

    // Re-use the menu actions via findChild (alternative: save the QAction ptrs)
    // Here we create standalone toolbar-only buttons for simplicity:
    QAction* openBtn = toolbar->addAction("Open Project");
    connect(openBtn, &QAction::triggered, this, &MainWindow::onOpenProject);

    QAction* saveBtn = toolbar->addAction("Save Scene");
    connect(saveBtn, &QAction::triggered, this, &MainWindow::onSaveScene);

    toolbar->addSeparator();

    QAction* cookBtn = toolbar->addAction("Cook Assets");
    connect(cookBtn, &QAction::triggered, this, &MainWindow::onCookAssets);
}

void MainWindow::setupDockWidgets()
{
    // TEACHING NOTE — QDockWidget
    // Dock widgets are floating/dockable panels that can be:
    //   • Docked to any edge of the main window (left, right, top, bottom).
    //   • Floated as separate windows.
    //   • Tabbed together.
    // This makes the editor layout configurable — users can arrange panels
    // to match their workflow, just like Unreal or Unity.

    m_contentDock = new QDockWidget("Content Browser", this);
    m_contentDock->setObjectName("contentBrowserDock");  // for restoreState()

    m_contentBrowser = new ContentBrowser(m_contentDock);

    // TEACHING NOTE — ContentBrowser → MainWindow connection
    // The content browser emits fileSelected(path) when the user double-clicks
    // a file.  We connect it to a MainWindow slot to react to the selection.
    connect(m_contentBrowser, &ContentBrowser::fileSelected,
            this,              &MainWindow::onContentFileSelected);

    m_contentDock->setWidget(m_contentBrowser);

    // Add the dock to the left side of the main window
    addDockWidget(Qt::LeftDockWidgetArea, m_contentDock);
}

void MainWindow::setupStatusBar()
{
    // TEACHING NOTE — Status Bar
    // The status bar shows transient messages (hover hints) and permanent info
    // (current project, zoom level, etc.).  QMainWindow already creates one;
    // we just add a permanent label to the right side.
    m_statusLabel = new QLabel("No project open");
    statusBar()->addPermanentWidget(m_statusLabel);
    statusBar()->showMessage("Ready", 3000);  // disappears after 3 s
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void MainWindow::onOpenProject()
{
    // TEACHING NOTE — QFileDialog
    // Qt provides platform-native file dialogs via QFileDialog.
    // getExistingDirectory() opens a directory picker — perfect for project folders.
    QString dir = QFileDialog::getExistingDirectory(
        this,
        "Open Project Folder",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
    );

    if (dir.isEmpty())
        return;  // User cancelled

    m_projectPath = dir;
    setWindowTitle(QString("Creation Suite Editor — %1").arg(QDir(dir).dirName()));

    // Tell the content browser to show this project's Content/ folder
    QString contentDir = dir + "/Content";
    if (QDir(contentDir).exists()) {
        m_contentBrowser->setRootPath(contentDir);
        m_statusLabel->setText(QDir(dir).dirName());
        statusBar()->showMessage(QString("Opened project: %1").arg(dir), 4000);
    } else {
        // Content/ doesn't exist yet — show the whole project folder instead
        m_contentBrowser->setRootPath(dir);
        m_statusLabel->setText(QDir(dir).dirName() + " (no Content/ folder)");
        statusBar()->showMessage("Project opened — no Content/ folder found. "
                                 "Create Content/ to organise assets.", 6000);
    }

    // Save the path for next launch
    QSettings settings;
    settings.setValue("project/lastPath", dir);
}

void MainWindow::onSaveScene()
{
    if (m_sceneEditor == nullptr)
        return;

    // TEACHING NOTE — QFileDialog::getSaveFileName
    // getSaveFileName() opens a standard "Save As" dialog.
    // The filter string "Scene Files (*.scene.json)" constrains the file
    // extension shown in the dialog, but the user can override it.
    QString defaultDir = m_projectPath.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        : m_projectPath + "/Content/Maps";

    QString filePath = QFileDialog::getSaveFileName(
        this,
        "Save Scene",
        defaultDir + "/untitled.scene.json",
        "Scene Files (*.scene.json);;JSON Files (*.json);;All Files (*)"
    );

    if (filePath.isEmpty())
        return;

    // Delegate saving to the SceneEditor
    if (m_sceneEditor->saveScene(filePath)) {
        statusBar()->showMessage(QString("Scene saved: %1").arg(filePath), 4000);
    } else {
        QMessageBox::critical(this, "Save Error",
            QString("Failed to save scene to:\n%1").arg(filePath));
    }
}

void MainWindow::onExit()
{
    // TEACHING NOTE — closeEvent vs. quit()
    // QApplication::quit() posts a QEvent::Quit and returns from exec().
    // close() on the main window calls closeEvent() first, which lets us
    // ask "save unsaved changes?" before exiting.
    close();
}

void MainWindow::onCookAssets()
{
    if (m_projectPath.isEmpty()) {
        QMessageBox::information(this, "No Project Open",
            "Open a project first (File → Open Project).");
        return;
    }

    // TEACHING NOTE — QProcess
    // QProcess lets us launch external programs from Qt.
    // Here we invoke the Python cook script that lives inside the project folder.
    // For production you'd want to show a progress bar and capture stdout/stderr.
    QString cookScript = m_projectPath + "/cook_assets.py";
    if (!QDir().exists(cookScript)) {
        // Fall back to the samples vertical slice cook script
        statusBar()->showMessage("No cook_assets.py found in project folder.", 5000);
        return;
    }

    QProcess* proc = new QProcess(this);
    proc->setWorkingDirectory(m_projectPath);
    proc->start("python", QStringList() << cookScript);
    connect(proc, &QProcess::finished, this, [this, proc](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0) {
            statusBar()->showMessage("Cook completed successfully.", 4000);
        } else {
            QMessageBox::warning(this, "Cook Failed",
                QString("cook_assets.py exited with code %1.\n\nOutput:\n%2")
                    .arg(exitCode)
                    .arg(proc->readAllStandardError()));
        }
        proc->deleteLater();
    });
    statusBar()->showMessage("Cooking assets…");
}

void MainWindow::onContentFileSelected(const QString& path)
{
    // TEACHING NOTE — reacting to signals from child widgets
    // When the user selects a file in the content browser we receive its path.
    // In a full editor you would open the appropriate sub-editor here:
    //   • .scene.json   → load and display in SceneEditor
    //   • .png / .jpg   → open in a texture viewer
    //   • .wav / .ogg   → open in an audio preview widget
    // For now we just show the path in the status bar as a teaching stub.
    statusBar()->showMessage(QString("Selected: %1").arg(path), 5000);

    // Load scene files directly into the scene editor
    if (path.endsWith(".scene.json", Qt::CaseInsensitive)) {
        m_sceneEditor->loadScene(path);
    }
}

// ---------------------------------------------------------------------------
// Event overrides
// ---------------------------------------------------------------------------

// TEACHING NOTE — closeEvent
// Overriding closeEvent() lets us intercept the window-close action (×
// button, Alt+F4, File → Exit).  We save the window layout before exiting.
// If we had unsaved changes, we would ask the user here.
// (closeEvent is declared in QWidget, so no forward declaration is needed
//  in MainWindow.hpp — it is already virtual in the base class.)
