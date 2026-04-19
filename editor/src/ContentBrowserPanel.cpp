/**
 * @file ContentBrowserPanel.cpp
 * @brief Content browser panel implementation.
 */

#include "ContentBrowserPanel.hpp"

#include <imgui.h>
#include <algorithm>
#include <set>
#include <string>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Engine-relevant asset extensions (same set as the old Qt version)
// ---------------------------------------------------------------------------
static const std::set<std::string> kAssetExtensions = {
    // Textures
    ".png", ".jpg", ".jpeg", ".bmp", ".dds", ".tga",
    // Audio
    ".wav", ".ogg", ".mp3", ".bank",
    // Scenes / data
    ".json",
    // Lua scripts
    ".lua",
    // Meshes
    ".fbx", ".obj", ".gltf", ".glb",
    // Cooked animation assets
    ".skelc", ".animc",
};

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ContentBrowserPanel::SetRootPath(const std::string& path)
{
    m_rootPath = path;
    m_dirty    = true;
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void ContentBrowserPanel::Render()
{
    // TEACHING NOTE -- ImGui::Begin with a persistent panel name
    // ImGui identifies windows by their string label.  Using the same label
    // every frame re-opens the same window rather than creating a new one.
    // ImGuiWindowFlags_NoCollapse prevents the user from collapsing the panel
    // (which would hide the file tree entirely with no way to open it again
    //  without the ini file).
    ImGui::Begin("Content Browser", nullptr, ImGuiWindowFlags_NoCollapse);

    if (m_rootPath.empty() || !fs::exists(m_rootPath))
    {
        // TEACHING NOTE -- TextDisabled
        // ImGui::TextDisabled renders text in the theme's disabled colour
        // (grey in the dark theme).  Good for placeholder / hint text.
        ImGui::TextDisabled("No project open.");
        ImGui::TextDisabled("Use File > Open Project to select a folder.");
        ImGui::End();
        return;
    }

    // Header showing just the folder name (not the full path)
    ImGui::TextColored(ImVec4(1.f, 0.9f, 0.4f, 1.f), "%s",
                       fs::path(m_rootPath).filename().string().c_str());
    ImGui::Separator();

    // Refresh button
    if (ImGui::SmallButton("Refresh (R)") ||
        (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_R)))
    {
        m_dirty = true;
    }

    ImGui::Spacing();

    // Render the directory tree starting from the root
    RenderDirectory(fs::path(m_rootPath), /*forceExpand=*/true);

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Recursive directory rendering
// ---------------------------------------------------------------------------

// TEACHING NOTE -- Recursive ImGui tree with std::filesystem
// RenderDirectory() walks one level of a directory.  For each sub-directory
// it calls itself recursively (creates a nested TreeNode).
// For each file it creates a leaf node with ImGuiTreeNodeFlags_Leaf.
//
// std::filesystem::directory_iterator gives one entry per file/dir.
// We sort entries so directories come before files and both sets are
// sorted alphabetically -- matching the behaviour of Qt's QFileSystemModel
// with sorting enabled.
void ContentBrowserPanel::RenderDirectory(const fs::path& dir, bool forceExpand)
{
    std::error_code ec;

    // Collect and sort directory entries
    std::vector<fs::directory_entry> dirs, files;
    for (const auto& entry : fs::directory_iterator(dir, ec))
    {
        if (ec) continue;  // skip entries we cannot access
        if (entry.is_directory())
            dirs.push_back(entry);
        else if (entry.is_regular_file() && IsAssetFile(entry.path()))
            files.push_back(entry);
    }

    auto byName = [](const fs::directory_entry& a, const fs::directory_entry& b)
    {
        return a.path().filename() < b.path().filename();
    };
    std::sort(dirs.begin(),  dirs.end(),  byName);
    std::sort(files.begin(), files.end(), byName);

    // ---- Directories -------------------------------------------------------
    for (const auto& entry : dirs)
    {
        std::string folderName = entry.path().filename().string();

        ImGuiTreeNodeFlags folderFlags = ImGuiTreeNodeFlags_OpenOnArrow;
        if (forceExpand)
            folderFlags |= ImGuiTreeNodeFlags_DefaultOpen;

        if (ImGui::TreeNodeEx(folderName.c_str(), folderFlags))
        {
            RenderDirectory(entry.path());
            ImGui::TreePop();
        }
    }

    // ---- Files -------------------------------------------------------------
    for (const auto& entry : files)
    {
        std::string fileName = entry.path().filename().string();
        std::string filePath = entry.path().string();

        // TEACHING NOTE -- Leaf nodes and selection highlight
        // ImGuiTreeNodeFlags_Leaf creates a node with no expand arrow.
        // ImGuiTreeNodeFlags_SpanAvailWidth makes the clickable area fill the
        // full panel width (not just the text width) -- better UX.
        // When the user double-clicks, we call the registered callback.
        ImGuiTreeNodeFlags leafFlags =
            ImGuiTreeNodeFlags_Leaf       |
            ImGuiTreeNodeFlags_NoTreePushOnOpen |
            ImGuiTreeNodeFlags_SpanAvailWidth;

        ImGui::TreeNodeEx(fileName.c_str(), leafFlags);

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            if (m_fileSelectedCallback)
                m_fileSelectedCallback(filePath);
        }

        // Show the full path as a tooltip on hover
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("%s", filePath.c_str());
    }
}

// ---------------------------------------------------------------------------
// Asset extension filter
// ---------------------------------------------------------------------------

bool ContentBrowserPanel::IsAssetFile(const fs::path& path)
{
    // Convert extension to lower-case for case-insensitive comparison
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return kAssetExtensions.count(ext) > 0;
}
