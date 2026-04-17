/**
 * @file main.cpp
 * @brief Creation Suite Editor — entry point.
 *
 * =============================================================================
 * TEACHING NOTE — Qt Application Entry Point
 * =============================================================================
 * Every Qt Widgets application starts with a QApplication object.
 * QApplication owns the event loop and manages global application state
 * (command-line arguments, default font, palette, etc.).
 *
 * The pattern is always:
 *   1. Create QApplication (parses argc/argv, initialises platform plugin).
 *   2. Create and show the main window.
 *   3. Call app.exec() — enters the event loop.  Returns when the last
 *      window is closed (or QApplication::quit() is called).
 *
 * TEACHING NOTE — WIN32 subsystem vs. console subsystem
 * On Windows, CMakeLists.txt adds the WIN32 keyword to add_executable so that
 * the linker uses the /SUBSYSTEM:WINDOWS entry point (no console window).
 * Qt expects this for GUI applications.  If you want a console for debug
 * output while the window is open, remove WIN32 from CMakeLists.txt.
 *
 * =============================================================================
 */

#include <QApplication>
#include "MainWindow.hpp"

int main(int argc, char* argv[])
{
    // ---------------------------------------------------------------------------
    // TEACHING NOTE — QApplication
    // QApplication must be constructed before any Qt object.
    // It reads argc/argv for Qt-specific options like -style, -platform, etc.
    // ---------------------------------------------------------------------------
    QApplication app(argc, argv);

    // Set application metadata (used by QSettings and about dialogs)
    QApplication::setApplicationName("Creation Suite Editor");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("GameEngineForTeaching");

    // ---------------------------------------------------------------------------
    // TEACHING NOTE — MainWindow
    // The MainWindow is the top-level widget.  Everything else (content browser,
    // scene editor, inspector) lives inside it as dock widgets or sub-widgets.
    // ---------------------------------------------------------------------------
    MainWindow window;
    window.show();  // Qt widgets start hidden; show() makes them visible.

    // ---------------------------------------------------------------------------
    // TEACHING NOTE — Event Loop
    // app.exec() blocks until the application exits.  Inside exec() Qt:
    //   1. Waits for OS events (mouse, keyboard, timers, network, etc.)
    //   2. Dispatches events to the appropriate widget via the virtual
    //      QWidget::event() → specific handler (mousePressEvent, keyPressEvent…)
    //   3. Processes any queued signals/slots (Qt's async connection mechanism).
    // ---------------------------------------------------------------------------
    return app.exec();
}
