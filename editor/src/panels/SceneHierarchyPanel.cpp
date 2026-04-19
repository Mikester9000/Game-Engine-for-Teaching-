/**
 * @file SceneHierarchyPanel.cpp
 * @brief Scene Hierarchy Panel implementation (M6).
 *
 * =============================================================================
 * TEACHING NOTE — Immediate-mode entity list
 * =============================================================================
 * In a retained-mode GUI (Qt, WPF) the hierarchy is a tree widget that you
 * populate once and update via signals.  In Dear ImGui the list is rebuilt
 * every frame from the current entity vector.  This sounds wasteful, but
 * ImGui draws only what is visible (virtual scrolling) and rebuilding the
 * list typically takes < 1 µs for scenes with hundreds of entities.
 *
 * The key lesson: immediate mode trades memory (no widget objects) for CPU
 * time (rebuild every frame) — and the CPU cost is negligible at editor scale.
 *
 * =============================================================================
 * TEACHING NOTE — ImGui::Selectable + context menus
 * =============================================================================
 * ImGui::Selectable("label", isSelected) renders a highlighted row.
 * ImGui::BeginPopupContextItem() opens a right-click popup attached to the
 * last item — this is how all mainstream editors implement context menus.
 *
 * =============================================================================
 */

#include "SceneHierarchyPanel.hpp"

#include <imgui.h>
#include <Guid.hpp>
#include <cstring>
#include <string>

// ---------------------------------------------------------------------------
// Render (main entry point)
// ---------------------------------------------------------------------------

void SceneHierarchyPanel::Render()
{
    // TEACHING NOTE — Panel naming convention
    // The window title includes a "##" suffix to show the entity count without
    // changing the dockable panel identity.  ImGui uses the part after "##" as
    // an invisible ID, so two windows named "A##1" and "A##2" are distinct.
    // However, ImGui::Begin() always uses the FULL string as the tab title, so
    // we rebuild the title every frame to show the live entity count.
    if (!m_scenePanel)
    {
        ImGui::Begin("Scene Hierarchy", nullptr, ImGuiWindowFlags_NoCollapse);
        ImGui::TextDisabled("(no scene panel attached)");
        ImGui::End();
        return;
    }

    auto& entities = m_scenePanel->GetEntities();

    // Rebuild window title with entity count (changes are fine — ImGui does a
    // string compare to decide if the title bar needs repainting).
    char titleBuf[64];
    std::snprintf(titleBuf, sizeof(titleBuf),
                  "Scene Hierarchy  (%d)###SceneHierarchy",
                  static_cast<int>(entities.size()));

    ImGui::Begin(titleBuf, nullptr, ImGuiWindowFlags_NoCollapse);

    RenderAddEntityButton();
    ImGui::Separator();
    RenderEntityList();
    RenderRenamePopup();

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Add entity button
// ---------------------------------------------------------------------------

void SceneHierarchyPanel::RenderAddEntityButton()
{
    // TEACHING NOTE — Small inline buttons
    // SmallButton() renders a compact button that sits flush in text-height.
    // It is ideal for toolbar-style actions at the top of a panel.
    if (ImGui::SmallButton("+ Add Entity"))
    {
        auto& entities = m_scenePanel->GetEntities();
        SceneEntity ent;
        ent.id   = Guid::New().ToString();
        ent.name = "Entity_" + std::to_string(entities.size() + 1);
        ent.x    = 0.f;
        ent.y    = 0.f;
        ent.z    = 0.f;
        entities.push_back(ent);
        // Select the newly created entity
        m_scenePanel->SetSelectedIdx(static_cast<int>(entities.size()) - 1);
    }

    ImGui::SameLine();

    // Delete selected entity button
    const int selIdx = m_scenePanel->GetSelectedIdx();
    if (selIdx < 0)
        ImGui::BeginDisabled();

    if (ImGui::SmallButton("- Delete"))
    {
        m_scenePanel->DeleteEntity(selIdx);
    }

    if (selIdx < 0)
        ImGui::EndDisabled();
}

// ---------------------------------------------------------------------------
// Entity list
// ---------------------------------------------------------------------------

void SceneHierarchyPanel::RenderEntityList()
{
    auto& entities  = m_scenePanel->GetEntities();
    const int selIdx = m_scenePanel->GetSelectedIdx();

    // TEACHING NOTE — BeginChild for a scrollable sub-region
    // The entity list may be longer than the panel.  BeginChild with a fixed
    // size or (0,0) fills the remaining space and makes the region scrollable.
    ImGui::BeginChild("##hierarchy_list", ImVec2(0, 0),
                      ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

    for (int i = 0; i < static_cast<int>(entities.size()); ++i)
    {
        const SceneEntity& ent = entities[static_cast<size_t>(i)];
        const bool selected    = (i == selIdx);

        // Show a small icon character in front of the entity name to hint at
        // entity type (future: derive from component presence).
        // TEACHING NOTE — UTF-8 icons in ImGui
        // ImGui renders UTF-8 text natively.  Any glyph loaded in the font
        // atlas can be used here.  We use ASCII box-drawing fallbacks that
        // work with the default font.
        const int compCount = static_cast<int>(ent.components.size());
        char label[256];
        if (compCount > 0)
            std::snprintf(label, sizeof(label), "[E] %s  (%d comp)",
                          ent.name.c_str(), compCount);
        else
            std::snprintf(label, sizeof(label), "[E] %s", ent.name.c_str());

        // TEACHING NOTE — PushID / PopID for stable ImGui IDs
        // When rendering the same widget type (Selectable) multiple times in a
        // loop, ImGui needs a unique ID for each.  PushID(i) prefixes the ID
        // stack with the loop index so each row gets a distinct internal ID.
        ImGui::PushID(i);

        if (ImGui::Selectable(label, selected,
                              ImGuiSelectableFlags_AllowDoubleClick))
        {
            m_scenePanel->SetSelectedIdx(i);

            // Double-click opens rename popup
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                m_renamePopupOpen = true;
                m_renameTargetIdx = i;
                std::strncpy(m_renameBuffer, ent.name.c_str(),
                             sizeof(m_renameBuffer) - 1);
                m_renameBuffer[sizeof(m_renameBuffer) - 1] = '\0';
            }
        }

        // Right-click context menu per entity
        RenderContextMenu(i);

        // Tooltip on hover — show UUID + position
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            ImGui::BeginTooltip();
            ImGui::Text("ID: %s", ent.id.c_str());
            ImGui::Text("Pos: (%.1f, %.1f, %.1f)", ent.x, ent.y, ent.z);
            ImGui::Text("Components: %d", compCount);
            ImGui::EndTooltip();
        }

        ImGui::PopID();
    }

    if (entities.empty())
        ImGui::TextDisabled("(empty scene — use '+ Add Entity' or click on the canvas)");

    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// Context menu
// ---------------------------------------------------------------------------

void SceneHierarchyPanel::RenderContextMenu(int idx)
{
    // TEACHING NOTE — BeginPopupContextItem
    // BeginPopupContextItem() opens a right-click popup anchored to the last
    // rendered item.  It returns true only while the popup is open.
    // This is the standard ImGui pattern for per-item context menus.
    if (!ImGui::BeginPopupContextItem("##ctx"))
        return;

    auto& entities = m_scenePanel->GetEntities();

    ImGui::Text("Entity: %s", entities[static_cast<size_t>(idx)].name.c_str());
    ImGui::Separator();

    if (ImGui::MenuItem("Rename"))
    {
        m_scenePanel->SetSelectedIdx(idx);
        m_renamePopupOpen = true;
        m_renameTargetIdx = idx;
        std::strncpy(m_renameBuffer,
                     entities[static_cast<size_t>(idx)].name.c_str(),
                     sizeof(m_renameBuffer) - 1);
        m_renameBuffer[sizeof(m_renameBuffer) - 1] = '\0';
    }

    if (ImGui::MenuItem("Duplicate"))
    {
        SceneEntity copy = entities[static_cast<size_t>(idx)];
        copy.id    = Guid::New().ToString();
        copy.name += "_copy";
        copy.x    += 20.f;  // offset slightly so it is visible
        copy.y    += 20.f;
        entities.insert(entities.begin() + idx + 1, copy);
        m_scenePanel->SetSelectedIdx(idx + 1);
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Delete"))
        m_scenePanel->DeleteEntity(idx);

    ImGui::EndPopup();
}

// ---------------------------------------------------------------------------
// Rename popup
// ---------------------------------------------------------------------------

void SceneHierarchyPanel::RenderRenamePopup()
{
    if (m_renamePopupOpen)
    {
        ImGui::OpenPopup("Rename Entity##renamePopup");
        m_renamePopupOpen = false;
    }

    // TEACHING NOTE — Non-modal popup with SetNextWindowPos
    // Unlike BeginPopupModal, BeginPopup renders a floating (non-blocking)
    // window.  We use BeginPopupModal here for rename so the user must
    // explicitly confirm or cancel before continuing.
    bool open = true;
    if (ImGui::BeginPopupModal("Rename Entity##renamePopup", &open,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("New name:");
        ImGui::SetNextItemWidth(240.f);

        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();

        const bool enter = ImGui::InputText(
            "##rename", m_renameBuffer, sizeof(m_renameBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::Spacing();

        if (enter || ImGui::Button("OK", ImVec2(100, 0)))
        {
            auto& entities = m_scenePanel->GetEntities();
            if (m_renameTargetIdx >= 0 &&
                m_renameTargetIdx < static_cast<int>(entities.size()))
            {
                const std::string newName(m_renameBuffer);
                if (!newName.empty())
                    entities[static_cast<size_t>(m_renameTargetIdx)].name = newName;
            }
            m_renameTargetIdx = -1;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0)))
        {
            m_renameTargetIdx = -1;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
