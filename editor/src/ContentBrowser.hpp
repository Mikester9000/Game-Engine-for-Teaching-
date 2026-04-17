/**
 * @file ContentBrowser.hpp
 * @brief Content browser panel — shows project Content/ files as a tree.
 *
 * =============================================================================
 * TEACHING NOTE — Model/View Architecture in Qt
 * =============================================================================
 * Qt uses a Model-View separation for all list/tree/table displays:
 *
 *   Model  — owns and provides data (QAbstractItemModel or subclass)
 *   View   — displays data without owning it (QTreeView, QListView, etc.)
 *   Delegate — controls how individual items are drawn / edited
 *
 * For file-system trees, Qt provides QFileSystemModel — a ready-made model
 * that reflects the real file system.  We pair it with a QTreeView (the view)
 * to get a live-updating file browser with almost no custom code.
 *
 * This is fundamentally different from maintaining a list manually:
 *   • The model emits signals (dataChanged, rowsInserted…) when data changes.
 *   • The view redraws automatically — no polling required.
 *   • Multiple views can share the same model (e.g., list view + tree view).
 *
 * =============================================================================
 */

#pragma once

#include <QWidget>
#include <QString>

// Forward declarations
class QFileSystemModel;
class QTreeView;
class QLabel;

/**
 * @class ContentBrowser
 * @brief A panel widget that shows the project's Content/ folder as a tree.
 *
 * When the user double-clicks a file, this widget emits fileSelected(path)
 * so that the main window can open the appropriate editor for it.
 */
class ContentBrowser : public QWidget
{
    Q_OBJECT

public:
    explicit ContentBrowser(QWidget* parent = nullptr);
    ~ContentBrowser() override = default;

    /**
     * @brief Point the browser at a new directory root.
     * @param path Absolute path to the directory (usually Content/).
     */
    void setRootPath(const QString& path);

    /** @brief Returns the currently displayed root path. */
    QString rootPath() const { return m_rootPath; }

signals:
    // ---------------------------------------------------------------------------
    // TEACHING NOTE — Custom Signals
    // Signals are declared in the signals: section.  They have no body — Qt's
    // moc (Meta-Object Compiler) generates the implementation automatically.
    // To emit a signal, call: emit fileSelected(someString);
    // Any slot connected to this signal receives the path string.
    // ---------------------------------------------------------------------------

    /**
     * @brief Emitted when the user double-clicks a file in the tree.
     * @param absolutePath Absolute file path of the selected item.
     */
    void fileSelected(const QString& absolutePath);

private slots:
    /// Called when the user double-clicks a tree item.
    void onItemDoubleClicked(const QModelIndex& index);

private:
    QFileSystemModel* m_model  = nullptr;   ///< File system data model
    QTreeView*        m_tree   = nullptr;   ///< Tree view widget
    QLabel*           m_header = nullptr;   ///< Header label (shows current path)
    QString           m_rootPath;           ///< Absolute root path being shown
};
