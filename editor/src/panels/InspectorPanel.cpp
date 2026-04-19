/**
 * @file InspectorPanel.cpp
 * @brief Entity Inspector / Property Editor implementation (M6).
 *
 * =============================================================================
 * TEACHING NOTE — Table-driven component definitions
 * =============================================================================
 * kComponentDefs below is a static table of every component type the inspector
 * knows about.  Each entry describes the component's display name and its
 * fields (name, type, optional drag speed/min/max).
 *
 * Adding support for a new component type:
 *   1. Add an entry to kComponentDefs with the component type name (must match
 *      the key used in SceneEntity::components, which must also match the name
 *      used by SceneSerialiser).
 *   2. Add fields in the "fields" initialiser list.
 *   3. Rebuild — no other changes needed.
 *
 * This approach deliberately avoids C++ reflection macros or template
 * metaprogramming so the code is easy for students to read and extend.
 *
 * =============================================================================
 * TEACHING NOTE — ImGui DragFloat / DragInt
 * =============================================================================
 * ImGui::DragFloat("label", &value, speed, min, max, "%.2f") renders a field
 * that the user can:
 *   • Drag left/right to change the value.
 *   • Double-click to type a value directly.
 *   • Ctrl+click for precise input.
 *
 * This is the standard ImGui pattern for numeric property editing — the same
 * interaction used in Unreal's Details panel and Unity's Inspector.
 *
 * =============================================================================
 */

#include "InspectorPanel.hpp"

#include <imgui.h>
#include <nlohmann/json.hpp>
#include <string>
#include <array>
#include <cstring>

using json = nlohmann::json;

// =============================================================================
// TEACHING NOTE — Component definition table
// Each ComponentDef describes one component type.
// Each FieldDef describes one field within a component.
// =============================================================================

enum class FieldType { Float, Int, Bool, String };

struct FieldDef
{
    const char* name;
    FieldType   type;
    float       defaultF  = 0.f;  ///< Default for float/int fields
    float       speed     = 0.5f; ///< DragFloat speed
    float       minVal    = 0.f;
    float       maxVal    = 0.f;  ///< 0 = no clamp
};

struct ComponentDef
{
    const char*              typeName;    ///< Must match SceneEntity::components key
    const char*              displayName; ///< Shown in the inspector header
    std::vector<FieldDef>    fields;
};

// ---------------------------------------------------------------------------
// Component definitions table
// ---------------------------------------------------------------------------
// TEACHING NOTE — Table-driven component definitions
// kComponentDefs is a `static const std::vector` — initialized once at first
// use (lazy static initialization, guaranteed thread-safe since C++11).
//
// Alternatives considered:
//   1. static constexpr std::array<ComponentDef, 6> — avoids heap allocation
//      but requires ComponentDef.fields to be a fixed-size array (e.g.
//      std::array<FieldDef, 9>) which makes the struct harder to read and
//      requires knowing each component's field count at compile time.
//   2. Code-generated lookup table — eliminates any allocation but adds a
//      build step.  Overkill for 6 components in a teaching project.
//
// For a shipping editor with hundreds of component types, a constexpr array
// (or code-generated table) would be the right call.  At 6 components this
// one-time heap allocation is unmeasurable.
// ---------------------------------------------------------------------------
static const std::vector<ComponentDef> kComponentDefs =
{
    {
        "HealthComponent", "Health",
        {
            { "hp",          FieldType::Int,   100.f, 1.f,  0.f, 9999.f },
            { "maxHp",       FieldType::Int,   100.f, 1.f,  1.f, 9999.f },
            { "mp",          FieldType::Int,    50.f, 1.f,  0.f, 9999.f },
            { "maxMp",       FieldType::Int,    50.f, 1.f,  1.f, 9999.f },
            { "regenRate",   FieldType::Float,   0.f, 0.1f, 0.f,  100.f },
            { "mpRegenRate", FieldType::Float,   2.f, 0.1f, 0.f,  100.f },
        }
    },
    {
        "StatsComponent", "Stats",
        {
            { "strength",     FieldType::Int,  10.f, 1.f, 0.f, 999.f },
            { "defence",      FieldType::Int,   5.f, 1.f, 0.f, 999.f },
            { "magic",        FieldType::Int,  10.f, 1.f, 0.f, 999.f },
            { "spirit",       FieldType::Int,   5.f, 1.f, 0.f, 999.f },
            { "speed",        FieldType::Int,  10.f, 1.f, 0.f, 999.f },
            { "luck",         FieldType::Int,   5.f, 1.f, 0.f, 999.f },
            { "vitality",     FieldType::Int,  10.f, 1.f, 0.f, 999.f },
            { "critRate",     FieldType::Int,   5.f, 1.f, 0.f, 100.f },
            { "critMultiplier", FieldType::Int, 200.f, 1.f, 100.f, 1000.f },
        }
    },
    {
        "RenderComponent", "Render",
        {
            { "spriteSheet",  FieldType::String, 0.f,  0.f, 0.f, 0.f },
            { "zOrder",       FieldType::Int,    0.f,  1.f, 0.f, 100.f },
            { "isVisible",    FieldType::Bool,   1.f,  0.f, 0.f, 0.f },
        }
    },
    {
        "LevelComponent", "Level",
        {
            { "level",     FieldType::Int, 1.f, 1.f, 1.f, 99.f  },
            { "currentXP", FieldType::Int, 0.f, 1.f, 0.f, 1e7f  },
        }
    },
    {
        "AnimatorComponent", "Animator",
        {
            { "skeletonID",    FieldType::String, 0.f, 0.f, 0.f, 0.f },
            { "currentClipID", FieldType::String, 0.f, 0.f, 0.f, 0.f },
            { "blendTreeID",   FieldType::String, 0.f, 0.f, 0.f, 0.f },
            { "playbackSpeed", FieldType::Float,  1.f, 0.01f, 0.f, 10.f },
        }
    },
    {
        "AIComponent", "AI",
        {
            { "sightRange",  FieldType::Float, 10.f, 0.1f, 0.f, 200.f },
            { "hearRange",   FieldType::Float,  5.f, 0.1f, 0.f, 200.f },
            { "attackRange", FieldType::Float,  2.f, 0.1f, 0.f, 50.f  },
            { "isNocturnal", FieldType::Bool,   0.f, 0.f,  0.f, 0.f   },
        }
    },
};

// =============================================================================
// InspectorPanel::Render
// =============================================================================

void InspectorPanel::Render()
{
    ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoCollapse);

    if (!m_scenePanel)
    {
        ImGui::TextDisabled("(no scene panel attached)");
        ImGui::End();
        return;
    }

    SceneEntity* ent = m_scenePanel->GetSelectedEntity();
    if (!ent)
    {
        RenderNoSelection();
        ImGui::End();
        return;
    }

    RenderEntityHeader(*ent);
    ImGui::Separator();
    RenderTransformSection(*ent);
    ImGui::Separator();
    RenderComponentSections(*ent);
    RenderAddComponentButton(*ent);
    RenderAddComponentPopup(*ent);

    ImGui::End();
}

// ---------------------------------------------------------------------------
// No-selection placeholder
// ---------------------------------------------------------------------------

void InspectorPanel::RenderNoSelection()
{
    // TEACHING NOTE — Disabled text style
    // ImGui::TextDisabled() renders grey text — conventionally used for
    // placeholder / hint text when there is nothing to show.
    ImGui::Spacing();
    ImGui::TextDisabled("Select an entity in the Scene Hierarchy");
    ImGui::TextDisabled("or click on the canvas to create one.");
}

// ---------------------------------------------------------------------------
// Entity header: name + UUID
// ---------------------------------------------------------------------------

void InspectorPanel::RenderEntityHeader(SceneEntity& ent)
{
    // TEACHING NOTE — InputText with a fixed-size buffer
    // ImGui::InputText writes into a C-style char array.  We must copy
    // std::string → char[] before the widget and char[] → std::string after.
    char nameBuf[128];
    std::strncpy(nameBuf, ent.name.c_str(), sizeof(nameBuf) - 1);
    nameBuf[sizeof(nameBuf) - 1] = '\0';

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Name");
    ImGui::SameLine(80.f);
    ImGui::SetNextItemWidth(-1.f);  // fill remaining width

    if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf)))
        ent.name = nameBuf;

    // UUID is read-only — GUIDs are stable identifiers that must not change.
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("ID");
    ImGui::SameLine(80.f);
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputText("##id", const_cast<char*>(ent.id.c_str()),
                     ent.id.size() + 1,
                     ImGuiInputTextFlags_ReadOnly);
}

// ---------------------------------------------------------------------------
// Transform section
// ---------------------------------------------------------------------------

void InspectorPanel::RenderTransformSection(SceneEntity& ent)
{
    // TEACHING NOTE — CollapsingHeader
    // CollapsingHeader renders a tree node that can be collapsed.
    // ImGuiTreeNodeFlags_DefaultOpen keeps it open on first render.
    // The section state is persisted to the imgui.ini file automatically.
    if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    // TEACHING NOTE — Label column alignment trick
    // We use ImGui::Columns(2) + SetColumnWidth(0, 80) to create a two-column
    // layout: label on the left, input widget filling the right.
    // An alternative is ImGui::AlignTextToFramePadding() + SameLine(80.f).
    ImGui::PushID("transform");

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("X");
    ImGui::SameLine(40.f);
    ImGui::SetNextItemWidth(-1.f);
    ImGui::DragFloat("##x", &ent.x, 0.5f);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Y");
    ImGui::SameLine(40.f);
    ImGui::SetNextItemWidth(-1.f);
    ImGui::DragFloat("##y", &ent.y, 0.5f);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Z");
    ImGui::SameLine(40.f);
    ImGui::SetNextItemWidth(-1.f);
    ImGui::DragFloat("##z", &ent.z, 0.5f);

    ImGui::PopID();
}

// ---------------------------------------------------------------------------
// Per-component sections
// ---------------------------------------------------------------------------

// Helper: render one field from a component's JSON blob.
static void RenderField(const char* compKey, const FieldDef& fd,
                        json& compJson)
{
    // Ensure the field exists in the JSON with the default if absent.
    if (!compJson.contains(fd.name))
    {
        if (fd.type == FieldType::String)
            compJson[fd.name] = "";
        else if (fd.type == FieldType::Bool)
            compJson[fd.name] = (fd.defaultF != 0.f);
        else
            compJson[fd.name] = fd.defaultF;
    }

    ImGui::PushID(fd.name);

    // Label in 120px column, widget fills the rest.
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(fd.name);
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(-1.f);

    // TEACHING NOTE — per-type widgets
    // Each FieldType maps to the most appropriate ImGui widget:
    //   Float  → DragFloat (drag to change, double-click to type)
    //   Int    → DragInt   (same, integer)
    //   Bool   → Checkbox  (toggle)
    //   String → InputText (free text)
    switch (fd.type)
    {
        case FieldType::Float:
        {
            float v = compJson[fd.name].get<float>();
            if (ImGui::DragFloat("##v", &v, fd.speed, fd.minVal, fd.maxVal, "%.2f"))
                compJson[fd.name] = v;
            break;
        }
        case FieldType::Int:
        {
            int v = compJson[fd.name].get<int>();
            if (ImGui::DragInt("##v", &v, fd.speed,
                               static_cast<int>(fd.minVal),
                               static_cast<int>(fd.maxVal)))
                compJson[fd.name] = v;
            break;
        }
        case FieldType::Bool:
        {
            bool v = compJson[fd.name].get<bool>();
            if (ImGui::Checkbox("##v", &v))
                compJson[fd.name] = v;
            break;
        }
        case FieldType::String:
        {
            std::string s = compJson[fd.name].get<std::string>();
            char buf[256];
            std::strncpy(buf, s.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText("##v", buf, sizeof(buf)))
                compJson[fd.name] = std::string(buf);
            break;
        }
    }

    ImGui::PopID();
    (void)compKey;  // suppress unused warning
}

void InspectorPanel::RenderComponentSections(SceneEntity& ent)
{
    // Iterate known component defs and render a section for each that exists
    // on this entity.
    for (const auto& cd : kComponentDefs)
    {
        if (!ent.components.contains(cd.typeName))
            continue;

        json& compJson = ent.components[cd.typeName];

        ImGui::PushID(cd.typeName);

        // Component header with a [X] remove button on the right side.
        bool keepOpen = true;
        const bool expanded = ImGui::CollapsingHeader(
            cd.displayName, &keepOpen, ImGuiTreeNodeFlags_DefaultOpen);

        if (!keepOpen)
        {
            // User clicked the [X] — remove the component.
            // TEACHING NOTE — json::erase removes a key from a JSON object.
            ent.components.erase(cd.typeName);
            ImGui::PopID();
            continue;
        }

        if (expanded)
        {
            for (const auto& fd : cd.fields)
                RenderField(cd.typeName, fd, compJson);
        }

        ImGui::PopID();
    }
}

// ---------------------------------------------------------------------------
// Add Component button + popup
// ---------------------------------------------------------------------------

void InspectorPanel::RenderAddComponentButton(SceneEntity& /*ent*/)
{
    ImGui::Spacing();
    ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - 160.f) * 0.5f);
    if (ImGui::Button("+ Add Component", ImVec2(160.f, 0.f)))
        m_addCompPopupOpen = true;
}

void InspectorPanel::RenderAddComponentPopup(SceneEntity& ent)
{
    if (m_addCompPopupOpen)
    {
        ImGui::OpenPopup("AddComponent##popup");
        m_addCompPopupOpen = false;
    }

    if (ImGui::BeginPopup("AddComponent##popup"))
    {
        ImGui::TextUnformatted("Add Component");
        ImGui::Separator();

        for (const auto& cd : kComponentDefs)
        {
            // Gray out component types already present.
            const bool alreadyHas = ent.components.contains(cd.typeName);
            if (alreadyHas)
            {
                // TEACHING NOTE — BeginDisabled / EndDisabled
                // Wrapping items in BeginDisabled/EndDisabled dims them and
                // prevents interaction without removing them from the layout.
                ImGui::BeginDisabled();
            }

            if (ImGui::Selectable(cd.displayName))
            {
                // Create component with default values.
                json defaults = json::object();
                for (const auto& fd : cd.fields)
                {
                    if (fd.type == FieldType::String)
                        defaults[fd.name] = "";
                    else if (fd.type == FieldType::Bool)
                        defaults[fd.name] = (fd.defaultF != 0.f);
                    else
                        defaults[fd.name] = fd.defaultF;
                }
                ent.components[cd.typeName] = defaults;
                ImGui::CloseCurrentPopup();
            }

            if (alreadyHas)
                ImGui::EndDisabled();
        }

        ImGui::EndPopup();
    }
}
