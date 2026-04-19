/**
 * @file SceneEditorPanel.cpp
 * @brief 2D scene/map editor panel implementation.
 *
 * =============================================================================
 * TEACHING NOTE -- What this file teaches
 * =============================================================================
 *  1. ImGui child windows + DrawList for custom 2D rendering
 *  2. Immediate-mode mouse picking (click → entity selection)
 *  3. Immediate-mode popup input (replacing QInputDialog)
 *  4. JSON scene I/O with nlohmann-json (replacing Qt's QJsonDocument)
 *  5. Guid.hpp for UUID generation (replacing QUuid)
 *
 * =============================================================================
 */

#include "SceneEditorPanel.hpp"

#include <imgui.h>
#include <nlohmann/json.hpp>
#include <Guid.hpp>   // shared/runtime/Guid.hpp

#include <fstream>
#include <filesystem>
#include <cmath>
#include <cstring>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace fs  = std::filesystem;
using    json = nlohmann::json;

// ---------------------------------------------------------------------------
// Render (main entry point)
// ---------------------------------------------------------------------------

void SceneEditorPanel::Render()
{
    // TEACHING NOTE -- ImGui window with no close button
    // Passing nullptr as p_open means there is no close (X) button.
    // ImGuiWindowFlags_NoCollapse keeps the panel always expanded.
    ImGui::Begin("Scene Editor", nullptr, ImGuiWindowFlags_NoCollapse);

    // Scene name in the title area
    ImGui::Text("Scene: %s", m_sceneName.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("New"))
        NewScene();

    ImGui::SameLine();
    // TEACHING NOTE — M6: entity list moved to SceneHierarchyPanel
    // The entity sidebar that was here has been extracted into its own dockable
    // SceneHierarchyPanel so users can position it anywhere in the layout.
    // This panel is now canvas-only — the authoritative scene state (entity list
    // + selection index) is still owned here and shared via GetEntities() /
    // GetSelectedIdx() etc.
    ImGui::TextDisabled("  Left-click: place entity   |   Delete: remove selected");

    ImGui::Separator();

    // Full-width canvas (no column split — hierarchy is a separate panel)
    RenderCanvas();

    // Pending "New Entity" popup
    HandleEntityPopup();

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Canvas
// ---------------------------------------------------------------------------

void SceneEditorPanel::RenderCanvas()
{
    // TEACHING NOTE -- ImGui::BeginChild for a scrollable/bordered sub-region
    // BeginChild("id", size, border, flags) creates a clipped sub-region.
    //   size = (0,0) means "fill remaining content area".
    //   ImGuiChildFlags_Border draws a 1-pixel border around the canvas.
    // Everything drawn inside is clipped to this region.
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    canvasSize.y -= 4;  // small margin at the bottom

    ImGui::BeginChild("##canvas", canvasSize, ImGuiChildFlags_Border,
                      ImGuiWindowFlags_NoScrollbar);

    // Record canvas origin (top-left corner in screen coordinates)
    const ImVec2 origin    = ImGui::GetCursorScreenPos();
    const float  canvasW   = canvasSize.x;
    const float  canvasH   = canvasSize.y;

    // TEACHING NOTE -- ImDrawList
    // ImGui::GetWindowDrawList() returns the draw list of the current window.
    // Commands added to a draw list are rendered in order (painter's algorithm).
    // All coordinates are in *screen* space (pixels from top-left of the OS window),
    // so we offset by 'origin' to convert from canvas-local to screen coordinates.
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ---- Draw background ---------------------------------------------------
    dl->AddRectFilled(origin,
                      ImVec2(origin.x + canvasW, origin.y + canvasH),
                      IM_COL32(30, 30, 30, 255));  // dark grey background

    // ---- Draw grid ---------------------------------------------------------
    const ImU32 gridColour = IM_COL32(55, 55, 55, 255);
    for (float x = std::fmod(0.f, kGridSize); x < canvasW; x += kGridSize)
        dl->AddLine(ImVec2(origin.x + x, origin.y),
                    ImVec2(origin.x + x, origin.y + canvasH), gridColour);
    for (float y = std::fmod(0.f, kGridSize); y < canvasH; y += kGridSize)
        dl->AddLine(ImVec2(origin.x, origin.y + y),
                    ImVec2(origin.x + canvasW, origin.y + y), gridColour);

    // ---- Draw entities -----------------------------------------------------
    for (int i = 0; i < static_cast<int>(m_entities.size()); ++i)
    {
        const SceneEntity& ent = m_entities[static_cast<size_t>(i)];
        const bool selected    = (i == m_selectedIdx);

        const ImVec2 tl(origin.x + ent.x - kEntitySize,
                        origin.y + ent.y - kEntitySize);
        const ImVec2 br(origin.x + ent.x + kEntitySize,
                        origin.y + ent.y + kEntitySize);

        // Fill
        ImU32 fillCol = selected
            ? IM_COL32(255, 180,  0, 220)   // yellow-orange when selected
            : IM_COL32( 80, 160, 255, 200); // blue when normal
        dl->AddRectFilled(tl, br, fillCol, 3.f);  // 3px corner radius

        // Border
        ImU32 borderCol = selected
            ? IM_COL32(255, 220, 50, 255)
            : IM_COL32( 40, 110, 200, 255);
        dl->AddRect(tl, br, borderCol, 3.f,
                    ImDrawFlags_None, selected ? 2.f : 1.f);

        // Entity name label
        dl->AddText(ImVec2(tl.x, tl.y - 14.f),
                    IM_COL32(220, 220, 220, 255),
                    ent.name.c_str());
    }

    // ---- Help text when empty ----------------------------------------------
    if (m_entities.empty())
    {
        const char* hint = "Left-click to place an entity.\n"
                           "Select an entity, then press Delete to remove it.\n"
                           "Use File > Save Scene to write the JSON file.";
        ImVec2 textSize = ImGui::CalcTextSize(hint);
        ImVec2 textPos(origin.x + canvasW * 0.5f - textSize.x * 0.5f,
                       origin.y + canvasH * 0.5f - textSize.y * 0.5f);
        dl->AddText(textPos, IM_COL32(100, 100, 100, 255), hint);
    }

    // ---- Handle mouse input ------------------------------------------------
    // TEACHING NOTE -- IsWindowHovered + GetMousePos
    // IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) returns
    // true when the canvas child window is hovered, even if another widget is
    // active (e.g., a drag is in progress).
    // GetMousePos() returns screen-space coordinates; subtract origin to get
    // canvas-local coordinates.
    const bool hovered = ImGui::IsWindowHovered();
    const ImVec2 mousePos = ImGui::GetMousePos();
    const float  mx = mousePos.x - origin.x;
    const float  my = mousePos.y - origin.y;

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        // Test each entity for a hit (same picking logic as the Qt version)
        bool hit = false;
        for (int i = 0; i < static_cast<int>(m_entities.size()); ++i)
        {
            const SceneEntity& ent = m_entities[static_cast<size_t>(i)];
            if (std::abs(mx - ent.x) < kEntitySize &&
                std::abs(my - ent.y) < kEntitySize)
            {
                m_selectedIdx = i;
                hit = true;
                break;
            }
        }
        if (!hit)
        {
            // No entity hit -- prepare to create a new one via popup
            m_selectedIdx       = -1;
            m_pendingClickX     = mx;
            m_pendingClickY     = my;
            m_openEntityPopup   = true;
            std::snprintf(m_nameBuffer, sizeof(m_nameBuffer),
                          "Entity_%zu", m_entities.size() + 1);
        }
    }

    // Delete selected entity with the Delete key
    // TEACHING NOTE -- IsKeyPressed vs IsKeyDown
    // IsKeyPressed() returns true ONCE on the frame the key goes down.
    // IsKeyDown()    returns true every frame while the key is held.
    // For a destructive action like Delete we want IsKeyPressed().
    if (hovered && ImGui::IsKeyPressed(ImGuiKey_Delete) && m_selectedIdx >= 0)
    {
        m_entities.erase(m_entities.begin() + m_selectedIdx);
        m_selectedIdx = -1;
    }

    ImGui::EndChild();  // "##canvas"
}

// ---------------------------------------------------------------------------
// New-entity popup
// ---------------------------------------------------------------------------

void SceneEditorPanel::HandleEntityPopup()
{
    if (m_openEntityPopup)
    {
        ImGui::OpenPopup("New Entity##popup");
        m_openEntityPopup = false;
    }

    // TEACHING NOTE -- BeginPopupModal
    // BeginPopupModal renders a centered modal dialog.
    // ImGui::InputText writes into m_nameBuffer (a C-style char array).
    // ImGuiInputTextFlags_EnterReturnsTrue makes Enter commit the input.
    bool popupOpen = true;
    if (ImGui::BeginPopupModal("New Entity##popup", &popupOpen,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Entity name:");
        ImGui::SetNextItemWidth(240.f);

        // Auto-focus the text field when the popup first opens
        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();

        bool commit = ImGui::InputText(
            "##entityname", m_nameBuffer, sizeof(m_nameBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::Spacing();

        if (commit || ImGui::Button("Add", ImVec2(100, 0)))
        {
            const std::string name(m_nameBuffer);
            if (!name.empty())
            {
                SceneEntity ent;
                // TEACHING NOTE -- Guid::New() for UUID generation
                // Guid::New() from shared/runtime/Guid.hpp generates an RFC 4122
                // v4 UUID -- the same pattern used by the cook pipeline and asset
                // registry.  This replaces QUuid::createUuid() from the Qt version.
                ent.id   = Guid::New().ToString();
                ent.name = name;
                ent.x    = m_pendingClickX;
                ent.y    = m_pendingClickY;
                m_entities.push_back(ent);
                m_selectedIdx = static_cast<int>(m_entities.size()) - 1;
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

// ---------------------------------------------------------------------------
// Scene I/O
// ---------------------------------------------------------------------------

// TEACHING NOTE -- nlohmann-json for scene save
// nlohmann/json.hpp provides a single-header JSON library (MIT licence).
// json j = { {"key", value}, ... } builds a JSON object with initialiser lists.
// j.dump(4) returns a pretty-printed string with 4-space indentation.
// This replaces Qt's QJsonObject / QJsonDocument API.
bool SceneEditorPanel::SaveScene(const std::string& filePath) const
{
    // Build ISO-8601 timestamp (replaces QDateTime::currentDateTimeUtc())
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream tsStream;
    tsStream << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");

    json root;
    root["$schema"] = "../../shared/schemas/scene.schema.json";
    root["version"] = "1.0.0";
    root["name"]    = m_sceneName;
    root["meta"]    = {
        { "savedAt",       tsStream.str() },
        { "editorVersion", "1.0.0" },
        { "editorBackend", "Dear ImGui + D3D11" }
    };

    json entitiesArr = json::array();
    for (const auto& ent : m_entities)
    {
        json je;
        je["id"]   = ent.id;
        je["name"] = ent.name;
        je["transform"] = {
            { "x", ent.x },
            { "y", ent.y },
            { "z", ent.z }
        };
        // TEACHING NOTE — Persisting component data
        // Only write the "components" key when there is something to write.
        // This keeps scenes that only have positional data compact.
        if (!ent.components.empty())
            je["components"] = ent.components;

        entitiesArr.push_back(je);
    }
    root["entities"] = entitiesArr;

    // Ensure parent directory exists
    fs::create_directories(fs::path(filePath).parent_path());

    std::ofstream ofs(filePath);
    if (!ofs) return false;
    ofs << root.dump(4);
    return ofs.good();
}

// TEACHING NOTE -- nlohmann-json for scene load
// json::parse(stream) reads from any std::istream.
// j.value("key", default) safely reads a key with a fallback if missing.
// j.contains("key") tests whether a key exists before accessing it.
// This replaces QJsonDocument::fromJson() + QJsonObject::value().
bool SceneEditorPanel::LoadScene(const std::string& filePath)
{
    std::ifstream ifs(filePath);
    if (!ifs) return false;

    json root;
    try
    {
        root = json::parse(ifs);
    }
    catch (const json::exception&)
    {
        return false;  // malformed JSON
    }

    m_sceneName = root.value("name", "Untitled");

    m_entities.clear();
    if (root.contains("entities") && root["entities"].is_array())
    {
        for (const auto& entJson : root["entities"])
        {
            SceneEntity ent;
            ent.id   = entJson.value("id",   Guid::New().ToString());
            ent.name = entJson.value("name", "Entity");

            if (entJson.contains("transform") && entJson["transform"].is_object())
            {
                ent.x = entJson["transform"].value("x", 0.f);
                ent.y = entJson["transform"].value("y", 0.f);
                ent.z = entJson["transform"].value("z", 0.f);
            }

            // TEACHING NOTE — Loading component data
            // If the scene file has a "components" object, load it into
            // ent.components as raw JSON.  The InspectorPanel will parse it.
            if (entJson.contains("components") && entJson["components"].is_object())
                ent.components = entJson["components"];
            else
                ent.components = nlohmann::json::object();

            m_entities.push_back(ent);
        }
    }

    m_filePath    = filePath;
    m_selectedIdx = -1;
    return true;
}

void SceneEditorPanel::NewScene()
{
    m_entities.clear();
    m_selectedIdx = -1;
    m_sceneName   = "Untitled";
    m_filePath.clear();
}
