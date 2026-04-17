/**
 * @file ContentBrowser.cpp
 * @brief Content browser panel implementation.
 */

#include "ContentBrowser.hpp"

#include <QFileSystemModel>
#include <QTreeView>
#include <QVBoxLayout>
#include <QLabel>
#include <QDir>
#include <QHeaderView>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

ContentBrowser::ContentBrowser(QWidget* parent)
    : QWidget(parent)
{
    // ---------------------------------------------------------------------------
    // TEACHING NOTE — QVBoxLayout
    // Layouts manage the position and size of child widgets automatically.
    // QVBoxLayout stacks children vertically from top to bottom.
    // QHBoxLayout stacks them horizontally.
    // Layouts resize children when the parent is resized — no manual geometry.
    // ---------------------------------------------------------------------------
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);   // No padding; full width for the tree
    layout->setSpacing(2);

    // Header label
    m_header = new QLabel("No project open", this);
    m_header->setStyleSheet("font-weight: bold; padding: 4px;");
    layout->addWidget(m_header);

    // ---------------------------------------------------------------------------
    // TEACHING NOTE — QFileSystemModel
    // QFileSystemModel reads the file system asynchronously and populates a
    // tree model.  Key points:
    //   • setRootPath() tells the model where to start watching.
    //   • setNameFilters() restricts which files are shown.
    //   • setNameFilterDisables(false) hides (not greys out) filtered files.
    // The model column layout is: Name | Size | Type | Date Modified
    // We hide all columns except Name to keep the browser compact.
    // ---------------------------------------------------------------------------
    m_model = new QFileSystemModel(this);
    m_model->setRootPath(QDir::homePath());   // start at home; changed by setRootPath()

    // Only show asset file types relevant to the engine
    m_model->setNameFilters({
        "*.png", "*.jpg", "*.jpeg", "*.bmp",   // Textures
        "*.wav", "*.ogg", "*.mp3",             // Audio
        "*.scene.json", "*.json",              // Scenes / data
        "*.lua",                               // Lua scripts
        "*.fbx", "*.obj", "*.gltf", "*.glb",  // Meshes
        "*.skelc", "*.animc",                  // Cooked anim assets
        "*.bank"                               // Audio banks
    });
    m_model->setNameFilterDisables(false);    // Hide non-matching files

    // ---------------------------------------------------------------------------
    // TEACHING NOTE — QTreeView
    // QTreeView is the generic tree display widget.  We set the model on it and
    // set the root index to the directory we want to show.
    // sortByColumn(0, Qt::AscendingOrder) sorts alphabetically by name.
    // ---------------------------------------------------------------------------
    m_tree = new QTreeView(this);
    m_tree->setModel(m_model);
    m_tree->setRootIndex(m_model->index(QDir::homePath()));

    // Hide all columns except "Name" (column 0) — size/type/date are noise here
    m_tree->hideColumn(1);   // Size
    m_tree->hideColumn(2);   // Type
    m_tree->hideColumn(3);   // Date Modified

    m_tree->header()->setStretchLastSection(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setUniformRowHeights(true);    // Performance: all rows same height
    m_tree->sortByColumn(0, Qt::AscendingOrder);
    m_tree->setSortingEnabled(true);

    // TEACHING NOTE — connect() — item double-click → slot
    // QTreeView::doubleClicked(QModelIndex) is emitted when the user double-
    // clicks an item.  We connect it to our custom slot.
    connect(m_tree, &QTreeView::doubleClicked, this, &ContentBrowser::onItemDoubleClicked);

    layout->addWidget(m_tree);

    setLayout(layout);
    setMinimumWidth(220);   // Sensible minimum so the tree is readable
}

// ---------------------------------------------------------------------------
// Public methods
// ---------------------------------------------------------------------------

void ContentBrowser::setRootPath(const QString& path)
{
    if (path.isEmpty() || !QDir(path).exists())
        return;

    m_rootPath = path;

    // Update the model root and the tree's displayed root
    QModelIndex rootIndex = m_model->setRootPath(path);
    m_tree->setRootIndex(rootIndex);

    // Show just the last folder name in the header
    m_header->setText(QDir(path).dirName());
    m_header->setToolTip(path);   // Full path on hover
}

// ---------------------------------------------------------------------------
// Private slots
// ---------------------------------------------------------------------------

void ContentBrowser::onItemDoubleClicked(const QModelIndex& index)
{
    // TEACHING NOTE — QFileSystemModel::isDir() and filePath()
    // isDir() returns true if the index points to a directory.
    // filePath() returns the absolute path string for any index.
    if (m_model->isDir(index))
        return;   // Ignore folder double-clicks (they toggle expand/collapse)

    QString absolutePath = m_model->filePath(index);

    // TEACHING NOTE — emit keyword
    // 'emit' is a Qt macro that calls the signal function generated by moc.
    // All slots connected to fileSelected() will be called synchronously
    // (by default in a single-threaded Qt application).
    emit fileSelected(absolutePath);
}
