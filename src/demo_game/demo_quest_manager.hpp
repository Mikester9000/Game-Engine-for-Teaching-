/**
 * @file demo_quest_manager.hpp
 * @brief Lightweight quest and activity tracker for Demo_Game.
 *
 * ============================================================================
 * TEACHING NOTE — Two Quest Systems, One Game
 * ============================================================================
 * Demo_Game has two quest systems operating side-by-side:
 *
 *   1. QuestSystem (src/game/systems/QuestSystem.hpp/.cpp) — the full ECS-
 *      driven production quest system inside GameRuntime.  It reads
 *      quest_bank.json at startup, awards XP/Gil rewards, fires Lua callbacks,
 *      and drives NPC dialogue.  It is available only when the GameRuntime is
 *      active (windowed PLAYING state).
 *
 *   2. DemoQuestManager (this file) — a thin, headless-safe overlay tracker
 *      that exposes the demo-specific main quest and three side activities to
 *      the GDI HUD overlay and to the CI acceptance test (test 7).  It has
 *      zero D3D11 / Win32 / ECS dependency, so it runs in headless mode too.
 *
 * The two systems are connected by DemoQuestManager listening to the same
 * conceptual events (station visited, enemy killed, item collected) that
 * OpenWorld forwards via its public Notify*() methods.  The production
 * QuestSystem independently handles the same events through the EventBus.
 *
 * TEACHING NOTE — Observer Pattern
 * ──────────────────────────────────
 * DemoQuestManager is an Observer; OpenWorld is the Subject.  When the
 * player teleports to a station, OpenWorld calls:
 *
 *   m_quests.NotifyStationVisited(stationID);
 *
 * Both DemoQuestManager and (indirectly) QuestSystem react to the same event.
 * In a production engine all observers subscribe through an EventBus<T>
 * (see src/engine/core/EventBus.hpp); here a direct method call is used for
 * clarity.
 *
 * ============================================================================
 * TEACHING NOTE — Data-Driven Activity Definitions
 * ============================================================================
 * Activities are defined in C++ defaults (RegisterDefaults) that are always
 * valid, plus an optional JSON override from
 * Content/World/demo_activities.json (loaded via TryLoadFromJSON, available
 * when ENGINE_ENABLE_JSON is defined).  The same fallback pattern is used
 * everywhere in the engine (see open_world.cpp TryLoadStationsFromJSON).
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2026
 * C++ Standard: C++17
 * Platform: Windows (demo_game) / headless CI (ENGINE_ENABLE_D3D11 not required)
 */

#pragma once

#include <string>
#include <vector>
#include <set>
#include <cstdint>

// ===========================================================================
// DemoActivityType — the three side-activity gameplay verbs
// ===========================================================================

/**
 * @enum DemoActivityType
 * @brief Identifies which kind of event advances a side activity.
 *
 * TEACHING NOTE — Typed Activity Verbs
 * ──────────────────────────────────────
 * Encoding the activity type as an enum (rather than a magic string) lets the
 * compiler enforce exhaustive switch coverage and avoids typo bugs.  Each
 * type maps to one Notify*() method on DemoQuestManager:
 *
 *   STATION_INTERACT → NotifyStationVisited()  (interact via E key; unique-station
 *                       deduplicated unless specificStationID is set)
 *   COMBAT_CHALLENGE → NotifyEnemyKilled()
 *   ITEM_COLLECTION  → NotifyItemCollected()
 *
 * TEACHING NOTE — Sentinel Value
 * ────────────────────────────────
 * INVALID (= 0) is the first entry so that zero-initialised `DemoActivityType`
 * variables produce a detectable, obviously-wrong value rather than silently
 * falling through to STATION_INTERACT.  The pattern matches other engine enums
 * (e.g., BiomeType::NONE in open_world.hpp).
 */
enum class DemoActivityType : uint8_t
{
    INVALID           = 0, ///< Zero-init sentinel — must not appear in a registered activity.
    STATION_INTERACT,      ///< Interact (press E) at N stations — any unique station if
                           ///<  specificStationID is empty; only that specific station otherwise.
    COMBAT_CHALLENGE,      ///< Defeat N enemies in the open world.
    ITEM_COLLECTION,       ///< Collect N items (Engine Crystals) at stations.
    COMBO_PRACTICE,        ///< Complete the combo-practice mini-game at a station.
                           ///< Triggered by NotifyChallengePassed() after the player inputs
                           ///< the correct key sequence shown in the challenge overlay.
};

// ===========================================================================
// DemoActivity — one side-activity definition + live progress
// ===========================================================================

/**
 * @struct DemoActivity
 * @brief Defines and tracks a single Demo_Game side activity.
 *
 * TEACHING NOTE — Definition vs Instance
 * ────────────────────────────────────────
 * Storing both definition (title, required) and runtime state (progress,
 * completed) in one struct is the simplest approach for a demo-scale system.
 *
 * A production system separates "ActivityDefinition" (immutable, loaded from
 * a database once) from "ActivityInstance" (mutable, one per player, stored
 * in the save slot).  We collapse them here so students can see the whole
 * picture without flipping between files.
 */
struct DemoActivity
{
    std::string      id;           ///< Unique identifier, e.g. "lesson_reader".
    std::string      title;        ///< Short label shown in the quest HUD.
    std::string      description;  ///< Flavour text describing the task.
    DemoActivityType type;         ///< Which Notify*() event advances this activity.
    std::string      specificStationID; ///< If non-empty (STATION_INTERACT only): only
                                        ///< this station ID advances the activity.
    int              required = 1; ///< Event count needed to mark complete.
    int              progress = 0; ///< Events accumulated so far.
    bool             completed = false; ///< True once progress >= required.

    /// Returns true when the activity is finished.
    bool IsComplete() const noexcept { return progress >= required; }

    /**
     * @brief Add n progress units, clamping at required.
     *
     * Idempotent once completed == true.
     *
     * @param n  Units to add (default 1).
     */
    void Advance(int n = 1) noexcept;
};

// ===========================================================================
// DemoQuestObjective — one step in the Demo_Game main quest
// ===========================================================================

/**
 * @struct DemoQuestObjective
 * @brief A single sequential objective within the main quest.
 *
 * TEACHING NOTE — Linear Objective Chains
 * ─────────────────────────────────────────
 * The main quest uses a simple ordered list.  Only the CURRENT objective is
 * active at any time.  This "funnel" design (Witcher 3, FFXV, God of War) is
 * the most common commercial pattern for story quests: clear progress,
 * minimal UI cognitive load.
 *
 * Branching objectives would use a DAG; for a teaching demo the linear chain
 * is unambiguous and teaches the same API surface.
 */
struct DemoQuestObjective
{
    std::string description; ///< Text shown in the HUD, e.g. "Visit the Combat station".
    std::string stationID;   ///< Triggering station id; empty = not station-driven.
    bool        done = false; ///< True once this objective is satisfied.
};

// ===========================================================================
// DemoMainQuest — the one main quest for Demo_Game
// ===========================================================================

/**
 * @struct DemoMainQuest
 * @brief Definition + runtime state of the Demo_Game main quest.
 *
 * The main quest ("Tour of the Engine") guides the player through three
 * consecutive teaching stations so they can see the engine features in context.
 *
 * TEACHING NOTE — Main Quest as Tutorial
 * ─────────────────────────────────────────
 * A tutorial disguised as a quest is a commercial design technique: the player
 * feels in control ("I'm on a quest") while the designer ensures they visit
 * the key content points.  The objectives route the player past the most
 * visually impactful teaching stations: Quests & Dialogue → Combat → PBR.
 */
struct DemoMainQuest
{
    std::string                     id;
    std::string                     title;
    std::string                     description;
    std::vector<DemoQuestObjective> objectives;
    int                             currentObjective = 0; ///< Index into objectives[].
    bool                            completed = false;

    /**
     * @brief Return the currently active objective, or nullptr if the quest
     *        is already complete.
     */
    const DemoQuestObjective* CurrentObjective() const noexcept;

    /**
     * @brief Mark the current objective done and move to the next.
     *
     * Sets completed = true and logs to stdout when all objectives are done.
     */
    void AdvanceObjective() noexcept;
};

// ===========================================================================
// DemoQuestManager — manages the main quest + three side activities
// ===========================================================================

/**
 * @class DemoQuestManager
 * @brief Manages the Demo_Game quest/activity gameplay loop.
 *
 * Lifecycle:
 *   Init() → NotifyStationVisited() / NotifyEnemyKilled() / NotifyItemCollected()
 *          → query via GetMainQuest() / GetActivities()
 *
 * The class is deliberately dependency-free (no D3D11, Win32, ECS) so it
 * compiles and runs in headless CI mode without a GPU.
 *
 * TEACHING NOTE — Single Responsibility
 * ───────────────────────────────────────
 * DemoQuestManager has one job: track the demo quest and activity state.
 * It does NOT:
 *   • render anything   — rendering is handled by demo_main.cpp overlays
 *   • tick time         — OpenWorld calls Notify*() at the right moments
 *   • reward XP/Gil     — the production QuestSystem does this via ECS
 *
 * This separation keeps each class small enough to understand at a glance —
 * an important teaching goal.
 */
class DemoQuestManager
{
public:
    DemoQuestManager()  = default;
    ~DemoQuestManager() = default;

    DemoQuestManager(const DemoQuestManager&)            = delete;
    DemoQuestManager& operator=(const DemoQuestManager&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Initialise with built-in defaults; optionally overlay from JSON.
     *
     * Must be called before any Notify* or query methods.
     *
     * @param jsonPath  Path to demo_activities.json (empty = defaults only,
     *                  or ENGINE_ENABLE_JSON is not defined).
     * @return true     Always (built-in defaults are always valid).
     */
    bool Init(const std::string& jsonPath = "");

    // =========================================================================
    // Event notifications (called by OpenWorld)
    // =========================================================================

    /**
     * @brief Notify that the player interacted (pressed E) at a teaching station.
     *
     * TEACHING NOTE — Interact-only quest advancement
     * ─────────────────────────────────────────────────
     * This method is called ONLY from OpenWorld::InteractAtStation() — i.e.,
     * only when the player deliberately presses E while near a station.
     * TeleportToStation() (navigation via F1 menu) does NOT call this method.
     *
     * This enforces the teaching-design principle: the player must engage with
     * a station intentionally, not just teleport to it.
     *
     * For STATION_INTERACT activities:
     *   • If specificStationID is empty  → any unique station interact counts.
     *   • If specificStationID is set    → only that station counts.
     * Unique-station deduplication is performed via m_visitedStations.
     *
     * @param stationID  Station identifier (must match a TeachingStation::id).
     */
    void NotifyStationVisited(const std::string& stationID);

    /**
     * @brief Notify that the player killed an enemy.
     *
     * Advances the Combat Challenge activity.
     * In a production engine this would arrive via EventBus<CombatEvent>.
     */
    void NotifyEnemyKilled();

    /**
     * @brief Notify that the player collected an item.
     *
     * Advances the Collector's Run activity.
     * In a production engine this would arrive via EventBus<InventoryEvent>.
     */
    void NotifyItemCollected();

    /**
     * @brief Notify that the player completed the combo-practice mini-game.
     *
     * Advances the "Combo Challenger" side activity (type COMBO_PRACTICE).
     * Called by OpenWorld::NotifyChallengePassed() when demo_main confirms
     * the player pressed the challenge key sequence in full.
     *
     * TEACHING NOTE — Mini-game → quest tracking decoupling
     * ──────────────────────────────────────────────────────
     * The combo challenge input logic lives in demo_main.cpp (UI layer) and
     * the activity tracking lives here (quest layer).  Neither knows about the
     * other's internals — they communicate only through this single Notify
     * method.  This mirrors the EventBus<T> subscriber/publisher contract used
     * in the production QuestSystem.
     */
    void NotifyChallengePassed();

    // =========================================================================
    // Queries
    // =========================================================================

    /// Returns the main quest (const reference; always valid after Init()).
    const DemoMainQuest& GetMainQuest() const noexcept { return m_mainQuest; }

    /// Returns the three side activities.
    const std::vector<DemoActivity>& GetActivities() const noexcept { return m_activities; }

    /// Returns the count of completed side activities (0–3 after Init).
    int CompletedActivities() const noexcept;

    /**
     * @brief Returns total definitions registered: 1 main quest + N activities.
     *
     * Used by the headless acceptance test to verify initialisation succeeded.
     */
    int TotalDefined() const noexcept;

    // =========================================================================
    // Constants
    // =========================================================================

    static constexpr int kExpectedActivities = 4; ///< Four side activities in the demo.

private:
    // ---- Runtime state ----
    DemoMainQuest             m_mainQuest;
    std::vector<DemoActivity> m_activities;
    std::set<std::string>     m_visitedStations; ///< Unique station IDs visited.

    // ---- Helpers ----

    /**
     * @brief Register the hard-coded default quest and activity definitions.
     *
     * TEACHING NOTE — Hard-coded fallback
     * ─────────────────────────────────────
     * Built-in defaults guarantee the game is always in a valid state, even
     * if Content/World/demo_activities.json is missing (e.g., first run, or
     * before the cook step has been run).  This is the same "safe defaults +
     * optional override" pattern used in open_world.cpp.
     */
    void RegisterDefaults();

    /**
     * @brief Attempt to load/override definitions from demo_activities.json.
     *
     * Compiled only when ENGINE_ENABLE_JSON is defined.
     *
     * @param jsonPath  Path to demo_activities.json.
     * @return true if the JSON was successfully parsed and applied.
     */
    bool TryLoadFromJSON(const std::string& jsonPath);

    /// Return a pointer to the first activity of the given type, or nullptr.
    DemoActivity* FindActivity(DemoActivityType type) noexcept;
};
