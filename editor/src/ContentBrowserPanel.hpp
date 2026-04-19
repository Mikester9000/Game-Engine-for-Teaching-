/**
 * @file ContentBrowserPanel.hpp
 * @brief Content browser panel -- file tree using std::filesystem.
 *
 * =============================================================================
 * TEACHING NOTE -- std::filesystem vs QFileSystemModel
 * =============================================================================
 * The Qt version used QFileSystemModel paired with a QTreeView widget.
 * QFileSystemModel:
 *   - Monitors the directory asynchronously (background thread).
 *   - Emits dataChanged/rowsInserted signals when the file system changes.
 *   - Qt owns the model; the view subscribes to it.
 *
 * Without Qt we use C++17 std::filesystem:
 *   - std::filesystem::directory_iterator walks directory entries.
 *   - We build our own tree every frame (or on demand) and render it with
 *     ImGui::TreeNode / ImGui::TreePop.
 *
 * Trade-off: QFileSystemModel provides live background updates; our approach
 * refreshes manually (press R or call Refresh()).  For a teaching editor this
 * is simpler and has no hidden threads.
 *
 * =============================================================================
 * TEACHING NOTE -- ImGui Tree Nodes
 * =============================================================================
 * ImGui has no "tree widget" -- it builds trees from individual calls:
 *
 *   if (ImGui::TreeNode("Folder"))        // draws an expandable node
 *   {
 *       ImGui::TreeNode("child file");    // leaf inside the folder
 *       ImGui::TreePop();                 // MUST be called if TreeNode() returned true
 *   }
 *
 * TreeNodeEx() extends TreeNode() with ImGuiTreeNodeFlags for leaf styling,
 * default-open, selected highlight, and more.
 *
 * =============================================================================
 */

#pragma once

#include <string>
#include <functional>
#include <filesystem>
#include <vector>

/**
 * @class ContentBrowserPanel
 * @brief An ImGui panel that shows a project folder as an expandable file tree.
 *
 * File types are filtered to engine-relevant extensions.
 * Double-clicking a file invokes an optional callback.
 */
class ContentBrowserPanel
{
public:
    ContentBrowserPanel() = default;

    /**
     * @brief Set the root directory to display.
     * @param path Absolute directory path.  Empty string hides the panel.
     */
    void SetRootPath(const std::string& path);

    /** @brief Returns the current root path. */
    const std::string& GetRootPath() const { return m_rootPath; }

    /**
     * @brief Set the callback invoked when a file is double-clicked.
     * @param cb Receives the absolute file path as a UTF-8 std::string.
     */
    void SetFileSelectedCallback(std::function<void(const std::string&)> cb)
    {
        m_fileSelectedCallback = std::move(cb);
    }

    /**
     * @brief Render the panel for this frame.
     * Must be called between ImGui::NewFrame() and ImGui::Render().
     */
    void Render();

    /** @brief Force a directory re-scan on the next Render() call. */
    void Refresh() { m_dirty = true; }

private:
    // Renders a directory node recursively using ImGui TreeNode
    void RenderDirectory(const std::filesystem::path& dir, bool forceExpand = false);

    // Returns true if the extension is an engine-relevant asset type
    static bool IsAssetFile(const std::filesystem::path& path);

    // ---- State -------------------------------------------------------------
    std::string m_rootPath;
    bool        m_dirty = true;   ///< Re-scan the directory on next Render()

    std::function<void(const std::string&)> m_fileSelectedCallback;
};
