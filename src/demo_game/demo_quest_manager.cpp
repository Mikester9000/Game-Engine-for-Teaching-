/**
 * @file demo_quest_manager.cpp
 * @brief Implementation of DemoQuestManager — Demo_Game quest & activity loop.
 *
 * ============================================================================
 * TEACHING NOTE — Data-Driven Defaults + JSON Override
 * ============================================================================
 * RegisterDefaults() establishes a valid game state in pure C++.  The
 * optional TryLoadFromJSON() call in Init() can then replace those defaults
 * with content-authored JSON data.  This "safe fallback" pattern means:
 *
 *   • The game always launches (no crash if JSON is absent).
 *   • Designers iterate on quests by editing JSON, not recompiling.
 *   • CI headless tests always have data to validate.
 *
 * The same pattern is used in open_world.cpp (RegisterDefaultStations /
 * TryLoadStationsFromJSON) and combo_system.cpp (inline defaults /
 * LoadConfig).
 *
 * ============================================================================
 * TEACHING NOTE — Observer Pattern Without EventBus
 * ============================================================================
 * DemoQuestManager is notified via direct method calls from OpenWorld:
 *
 *   OpenWorld::TeleportToStation()  → DemoQuestManager::NotifyStationVisited()
 *   OpenWorld::NotifyEnemyKilled()  → DemoQuestManager::NotifyEnemyKilled()
 *   OpenWorld::NotifyItemCollected()→ DemoQuestManager::NotifyItemCollected()
 *
 * For teaching purposes the direct call is clearer than an EventBus<T>
 * subscription.  In a production engine you would use EventBus so that any
 * number of systems (QuestSystem, AchievementSystem, AnalyticsSystem) can
 * all react to the same event without coupling to OpenWorld.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2026
 * C++ Standard: C++17
 */

#include "demo_game/demo_quest_manager.hpp"

#include <iostream>   // std::cout for headless CI log output
#include <algorithm>  // std::count_if

// ---------------------------------------------------------------------------
// Conditional JSON include — same guard pattern as open_world.cpp
// ---------------------------------------------------------------------------
#ifdef ENGINE_ENABLE_JSON
#  include <fstream>
#  include <nlohmann/json.hpp>
#endif

// ===========================================================================
// DemoActivity helpers
// ===========================================================================

void DemoActivity::Advance(int n) noexcept
{
    if (completed)
        return; // idempotent once complete

    progress += n;
    if (progress >= required)
    {
        progress  = required; // clamp
        completed = true;
        std::cout << "[DemoQuestManager] Activity complete: " << title << "\n";
    }
}

// ===========================================================================
// DemoMainQuest helpers
// ===========================================================================

const DemoQuestObjective* DemoMainQuest::CurrentObjective() const noexcept
{
    if (completed)
        return nullptr;
    if (currentObjective < static_cast<int>(objectives.size()))
        return &objectives[currentObjective];
    return nullptr;
}

void DemoMainQuest::AdvanceObjective() noexcept
{
    if (completed || objectives.empty())
        return;

    if (currentObjective < static_cast<int>(objectives.size()))
    {
        objectives[currentObjective].done = true;
        std::cout << "[DemoQuestManager] Objective done: "
                  << objectives[currentObjective].description << "\n";
        ++currentObjective;
    }

    if (currentObjective >= static_cast<int>(objectives.size()))
    {
        completed = true;
        std::cout << "[DemoQuestManager] Main quest complete: " << title << "\n";
    }
}

// ===========================================================================
// DemoQuestManager — lifecycle
// ===========================================================================

bool DemoQuestManager::Init(const std::string& jsonPath)
{
    // Always establish safe in-code defaults first.
    RegisterDefaults();

    // Attempt to overlay from JSON (no-op if file absent or JSON disabled).
    if (!jsonPath.empty())
        TryLoadFromJSON(jsonPath);

    std::cout << "[DemoQuestManager] Initialised — main quest \""
              << m_mainQuest.title << "\" ("
              << m_mainQuest.objectives.size() << " objectives), "
              << m_activities.size() << " side activities.\n";
    return true;
}

// ===========================================================================
// DemoQuestManager — event notifications
// ===========================================================================

void DemoQuestManager::NotifyStationVisited(const std::string& stationID)
{
    // -----------------------------------------------------------------------
    // 1. Main quest — check whether the current objective requires this station.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Sequential objective advance
    // We only advance the CURRENT objective, not all matching ones.  This
    // forces the player to visit stations in order, which gives the designer
    // full control over the narrative flow.
    // -----------------------------------------------------------------------
    if (!m_mainQuest.completed)
    {
        const DemoQuestObjective* obj = m_mainQuest.CurrentObjective();
        if (obj && !obj->stationID.empty() && obj->stationID == stationID)
        {
            m_mainQuest.AdvanceObjective();
        }
    }

    // -----------------------------------------------------------------------
    // 2. STATION_INTERACT activities — count station interacts.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Set-based deduplication + specificStationID filter
    // ─────────────────────────────────────────────────────────────────────
    // For activities with an empty specificStationID we count unique stations
    // using m_visitedStations (same "set membership" pattern as before).
    // For activities with a non-empty specificStationID we only count when
    // the interacted station matches that ID — a simple equality check.
    // We still use m_visitedStations for the "any unique station" activities
    // so one station can only count once per such activity.
    // -----------------------------------------------------------------------
    const bool wasInserted = m_visitedStations.insert(stationID).second;

    for (auto& a : m_activities)
    {
        if (a.type != DemoActivityType::STATION_INTERACT)
            continue;
        if (a.completed)
            continue;

        if (a.specificStationID.empty())
        {
            // Generic "interact at any N distinct stations" activity.
            if (wasInserted)
                a.Advance();
        }
        else
        {
            // Targeted "interact at this specific station" activity.
            if (a.specificStationID == stationID)
                a.Advance();
        }
    }
}

void DemoQuestManager::NotifyEnemyKilled()
{
    DemoActivity* combat = FindActivity(DemoActivityType::COMBAT_CHALLENGE);
    if (combat)
        combat->Advance();
}

void DemoQuestManager::NotifyItemCollected()
{
    DemoActivity* collect = FindActivity(DemoActivityType::ITEM_COLLECTION);
    if (collect)
        collect->Advance();
}

// ===========================================================================
// DemoQuestManager — queries
// ===========================================================================

int DemoQuestManager::CompletedActivities() const noexcept
{
    return static_cast<int>(
        std::count_if(m_activities.begin(), m_activities.end(),
                      [](const DemoActivity& a) { return a.completed; }));
}

int DemoQuestManager::TotalDefined() const noexcept
{
    // 1 main quest + N activities.  Always >= 1 after Init().
    return 1 + static_cast<int>(m_activities.size());
}

// ===========================================================================
// DemoQuestManager — private helpers
// ===========================================================================

DemoActivity* DemoQuestManager::FindActivity(DemoActivityType type) noexcept
{
    // TEACHING NOTE — Guard against INVALID sentinel
    // We never register activities with type INVALID, but the early-return here
    // prevents a hypothetical caller from accidentally matching an uninitialised
    // activity (which would have type INVALID from zero-initialisation).
    if (type == DemoActivityType::INVALID)
        return nullptr;    for (auto& a : m_activities)
        if (a.type == type)
            return &a;
    return nullptr;
}

// ---------------------------------------------------------------------------
// RegisterDefaults
// ---------------------------------------------------------------------------
// TEACHING NOTE — Inline Default Content
// ─────────────────────────────────────────────────────────────────────────────
// Hard-coding content in C++ defaults serves the same purpose as the built-in
// default stations in open_world.cpp: the game ships in a valid, playable
// state without any runtime content files.  Students can immediately run the
// demo and see quest progress without needing a cook step.
//
// The quest "Tour of the Engine" uses station visits as objectives so that:
//   1. Students can trigger the objectives from the F1 menu (teleport).
//   2. No enemy/inventory systems are required to *complete* the main quest.
//   3. Each objective routes the player to a visually distinct engine feature.
// ─────────────────────────────────────────────────────────────────────────────

void DemoQuestManager::RegisterDefaults()
{
    m_mainQuest = {};
    m_activities.clear();
    m_visitedStations.clear();

    // ── Main quest — "Tour of the Engine" ────────────────────────────────────
    // TEACHING NOTE — Tour quest as interactive tutorial
    // ─────────────────────────────────────────────────────────────────────────
    // A "tour quest" is a guided walkthrough masquerading as gameplay.  The
    // player follows a sequence of stations, pressing E at each one to read
    // the teaching explanation.  This is the same pattern used in:
    //   • FFXV  — Ignis' cooking tutorial chains several in-world prompts.
    //   • Zelda:BotW — the plateau shrines force the player to interact with
    //     core mechanics before entering the open world.
    //   • DOOM Eternal — the "Doom Slayer's Fortress" doubles as a tutorial.
    //
    // Key design rule: the objective ONLY advances on Interact (E key).
    // Teleporting via F1 is navigation only — it does not advance the quest.
    // ─────────────────────────────────────────────────────────────────────────
    m_mainQuest.id          = "demo_main_quest";
    m_mainQuest.title       = "Tour of the Engine";
    m_mainQuest.description =
        "Walk up to each teaching station and press E to read the lesson. "
        "Use the F1 overlay to teleport to stations if needed, "
        "then press E when you arrive to continue the tour.";

    // Objective 1 — Quests & Dialogue station (demonstrates QuestSystem + DialogueSystem)
    {
        DemoQuestObjective obj;
        obj.description = "Press E at the Quests & Dialogue station";
        obj.stationID   = "quests_dialogue";
        m_mainQuest.objectives.push_back(obj);
    }
    // Objective 2 — Combat station (demonstrates ComboSystem)
    {
        DemoQuestObjective obj;
        obj.description = "Press E at the Action Combat station";
        obj.stationID   = "combat";
        m_mainQuest.objectives.push_back(obj);
    }
    // Objective 3 — PBR Rendering station (demonstrates full rendering pipeline)
    {
        DemoQuestObjective obj;
        obj.description = "Press E at the PBR Rendering station to complete the tour";
        obj.stationID   = "rendering_pbr";
        m_mainQuest.objectives.push_back(obj);
    }

    // ── Side activity 1 — Lesson Reader (teaching interaction) ───────────────
    // TEACHING NOTE — Generic teaching activity
    // Pressing E at any 3 distinct stations forces the player to read three
    // different engine lessons — breadth-first orientation before going deep.
    {
        DemoActivity a;
        a.id                = "lesson_reader";
        a.title             = "Lesson Reader";
        a.description       = "Press E at 3 different teaching stations to read their lessons.";
        a.type              = DemoActivityType::STATION_INTERACT;
        a.specificStationID = ""; // any unique station counts
        a.required          = 3;
        m_activities.push_back(a);
    }

    // ── Side activity 2 — Code Explorer: Combat (targeted teaching) ──────────
    // TEACHING NOTE — Targeted station activity
    // Specifically visiting the Combat station via Interact ensures the player
    // reads the ComboSystem lesson — the most complex action-game subsystem.
    // specificStationID restricts progress to ONLY the combat station.
    {
        DemoActivity a;
        a.id                = "code_explorer_combat";
        a.title             = "Code Explorer: Combat";
        a.description       = "Press E at the Action Combat station to study the ComboSystem.";
        a.type              = DemoActivityType::STATION_INTERACT;
        a.specificStationID = "combat"; // only combat station counts
        a.required          = 1;
        m_activities.push_back(a);
    }

    // ── Side activity 3 — Code Explorer: Rendering (targeted teaching) ───────
    // TEACHING NOTE — Targeted station activity (second variant)
    // Specifically visiting the PBR Rendering station ensures the player reads
    // the BRDF / IBL lesson — the most visually impressive subsystem.
    // Pairing two "code explorer" activities for different stations teaches the
    // breadth of the engine's rendering capabilities.
    {
        DemoActivity a;
        a.id                = "code_explorer_rendering";
        a.title             = "Code Explorer: Rendering";
        a.description       = "Press E at the PBR Rendering station to study the BRDF pipeline.";
        a.type              = DemoActivityType::STATION_INTERACT;
        a.specificStationID = "rendering_pbr"; // only rendering_pbr counts
        a.required          = 1;
        m_activities.push_back(a);
    }
}

// ---------------------------------------------------------------------------
// TryLoadFromJSON
// ---------------------------------------------------------------------------
// TEACHING NOTE — JSON Override for Activity Definitions
// ─────────────────────────────────────────────────────────────────────────────
// When ENGINE_ENABLE_JSON is active, this method reads demo_activities.json
// and replaces the in-code defaults with content-authored data.
//
// The format is defined in
//   samples/vertical_slice_project/Content/World/demo_activities.json.
//
// If any field is missing or malformed, the C++ defaults are retained —
// the same "fail gracefully" rule used throughout the engine.
// ─────────────────────────────────────────────────────────────────────────────

bool DemoQuestManager::TryLoadFromJSON(const std::string& jsonPath)
{
#ifdef ENGINE_ENABLE_JSON
    std::ifstream file(jsonPath);
    if (!file.is_open())
        return false; // missing file is normal; silently keep defaults

    nlohmann::json root;
    try
    {
        file >> root;
    }
    catch (const std::exception& ex)
    {
        std::cout << "[DemoQuestManager] demo_activities.json parse error: "
                  << ex.what() << " — keeping C++ defaults.\n";
        return false;
    }

    // ---- Main quest ----
    if (root.contains("mainQuest") && root["mainQuest"].is_object())
    {
        const auto& mq = root["mainQuest"];
        DemoMainQuest loaded;
        loaded.id          = mq.value("id",          m_mainQuest.id);
        loaded.title       = mq.value("title",       m_mainQuest.title);
        loaded.description = mq.value("description", m_mainQuest.description);

        if (mq.contains("objectives") && mq["objectives"].is_array())
        {
            for (const auto& obj : mq["objectives"])
            {
                DemoQuestObjective o;
                o.description = obj.value("description", "");
                o.stationID   = obj.value("stationID",   "");
                if (!o.description.empty())
                    loaded.objectives.push_back(o);
            }
        }

        if (!loaded.title.empty() && !loaded.objectives.empty())
            m_mainQuest = std::move(loaded);
    }

    // ---- Activities ----
    if (root.contains("activities") && root["activities"].is_array())
    {
        // TEACHING NOTE — Explicit string-to-enum mapping
        // A separate static helper makes the dependency on DemoActivityType
        // explicit, prevents silent fallback for typos, and is easy to extend
        // when a new type is added (the compiler will warn on non-exhaustive
        // switches if all enum values are not handled).
        auto parseType = [](const std::string& s) -> DemoActivityType
        {
            if (s == "station_interact") return DemoActivityType::STATION_INTERACT;
            // Legacy alias kept for backward compat with existing JSON files.
            if (s == "station_scan")    return DemoActivityType::STATION_INTERACT;
            if (s == "combat_challenge") return DemoActivityType::COMBAT_CHALLENGE;
            if (s == "item_collection")  return DemoActivityType::ITEM_COLLECTION;
            // Unknown type → INVALID; the entry will be rejected below.
            return DemoActivityType::INVALID;
        };

        std::vector<DemoActivity> loaded;
        for (const auto& obj : root["activities"])
        {
            DemoActivity a;
            a.id                = obj.value("id",                "");
            a.title             = obj.value("title",             "");
            a.description       = obj.value("description",       "");
            a.type              = parseType(obj.value("type",    "station_interact"));
            a.specificStationID = obj.value("specificStationID", "");
            a.required          = obj.value("required",          1);

            if (!a.id.empty() && !a.title.empty() && a.required >= 1
                && a.type != DemoActivityType::INVALID)
                loaded.push_back(a);
        }

        if (!loaded.empty())
        {
            m_activities = std::move(loaded);
            std::cout << "[DemoQuestManager] Loaded " << m_activities.size()
                      << " activities from " << jsonPath << ".\n";
        }
    }

    return true;

#else
    (void)jsonPath;
    return false;
#endif // ENGINE_ENABLE_JSON
}
