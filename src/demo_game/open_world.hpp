/**
 * @file open_world.hpp
 * @brief OpenWorld — demo open-world state for Demo_Game.
 *
 * ============================================================================
 * TEACHING NOTE — What is an Open World?
 * ============================================================================
 * An "open world" game loads a large continuous level with multiple distinct
 * biomes (areas) that transition seamlessly as the player moves through them.
 * Famous examples: The Witcher 3, Breath of the Wild, Final Fantasy XV.
 *
 * Key techniques used here (all reflected in the engine):
 *   • World streaming  — only load nearby cells; evict distant ones (M7).
 *   • Biome data       — JSON-driven area definitions (terrain, weather, music).
 *   • Teaching stations— landmark objects that demonstrate engine features.
 *   • Debug F1 overlay — developer jump-to-station and overlay toggle.
 *
 * ─── OpenWorld State Machine ────────────────────────────────────────────────
 *
 *   BOOT_MENU ──► LOADING ──► PLAYING ──► PAUSED ──► PLAYING
 *                                                 └──► BOOT_MENU (quit)
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2026
 * C++ Standard: C++17
 * Platform: Windows (demo_game) / headless CI
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

#include "demo_game/demo_quest_manager.hpp"

// ===========================================================================
// BiomeType — identifies the major biome regions of the open world
// ===========================================================================

/**
 * @enum BiomeType
 * @brief Identifies one of the five open-world biomes.
 *
 * TEACHING NOTE — Biomes as Enum Values
 * ──────────────────────────────────────
 * Each enum value maps to a distinct area in the open world with its own:
 *   • Sky colour and fog density (SkyRenderer + WeatherFx parameters).
 *   • Ambient music track (AudioSystem music FSM).
 *   • Terrain height variation and texture blend.
 *   • NPC/enemy spawn tables (QuestSystem + AISystem).
 */
enum class BiomeType : uint8_t
{
    GRASSLAND = 0, ///< Sunlit plains — tutorial/spawn area.
    FOREST    = 1, ///< Dense trees with dappled light — stealth enemies.
    SNOW      = 2, ///< Frozen highlands — weather hazards.
    DESERT    = 3, ///< Arid rocky landscape — heat shimmer effect.
    COAST     = 4, ///< Shoreline — water reflections and sea ambience.
};

/** @brief Return a human-readable name for a BiomeType. */
const char* BiomeName(BiomeType b) noexcept;

// ===========================================================================
// TeachingStation — a named in-world demonstration landmark
// ===========================================================================

/**
 * @struct TeachingStation
 * @brief Describes a single feature/gameplay demonstration station.
 *
 * TEACHING NOTE — Teaching Stations as Data-Shaped Records
 * ─────────────────────────────────────────────────────────
 * The current demo registers a small set of built-in teaching stations in
 * C++ at startup (see RegisterDefaultStations() in open_world.cpp) rather
 * than loading them from JSON.  We still keep each station as a plain data
 * struct so the design is easy to read today and easy to migrate to an
 * external-data loader in the future.
 *
 * The file Content/World/teaching_stations.json is the planned authoring
 * format for a fully data-driven version of the demo station list.
 */
struct TeachingStation
{
    std::string id;          ///< Unique identifier, e.g. "rendering_pbr".
    std::string displayName; ///< Human label shown in the F1 menu.
    std::string description; ///< One-line summary of what the station demos.
    BiomeType   biome;       ///< Which biome this station sits in.
    float       worldX = 0.f;///< World X coordinate of the station centre.
    float       worldZ = 0.f;///< World Z coordinate of the station centre.
    std::string sceneHint;   ///< Optional --scene argument to showcase it.
};

// ===========================================================================
// StationLesson — teaching content shown when a player interacts at a station
// ===========================================================================

/**
 * @struct StationLesson
 * @brief Data-driven teaching explanation for a single teaching station.
 *
 * TEACHING NOTE — Data-Driven Lesson Content
 * ───────────────────────────────────────────
 * Storing lesson content in JSON (station_lessons.json) rather than hard-coded
 * strings allows course instructors to update the explanations, add code
 * pointers, and extend lessons without recompiling the engine.
 *
 * This is the same "authoring-time data, runtime consumption" pattern used
 * throughout the engine:
 *   • quest_bank.json    → QuestSystem
 *   • teaching_stations.json → OpenWorld
 *   • combat_config.json → ComboSystem
 *   • station_lessons.json  → OpenWorld (this struct)
 */
struct StationLesson
{
    std::string              stationID;      ///< Must match a TeachingStation::id.
    std::string              lessonTitle;    ///< Short heading shown in the lesson panel.
    std::string              lessonText;     ///< Multi-line explanation (may contain \n).
    std::vector<std::string> codePointers;  ///< File paths / class names to inspect.
    std::vector<std::string> relatedStations; ///< Optional cross-references.

    /// Returns true when this lesson has content (non-empty title + text).
    bool IsValid() const noexcept
    { return !lessonTitle.empty() && !lessonText.empty(); }
};

// ===========================================================================
// OpenWorldState — top-level state machine enum
// ===========================================================================

/**
 * @enum OpenWorldState
 * @brief The Demo_Game top-level state machine.
 *
 * TEACHING NOTE — State Machines for Game Flow
 * ─────────────────────────────────────────────
 * Every game needs a clear flow: title screen → loading → gameplay → paused.
 * A finite state machine (FSM) with an enum + switch statement is the simplest
 * correct implementation — easy to read, debug, and extend.
 *
 * More complex games use a class hierarchy (State pattern) or a pushdown
 * automaton (same as MenuStack).  For the demo we use the plain enum approach
 * because students can see the entire flow in one switch block.
 */
enum class OpenWorldState : uint8_t
{
    BOOT_MENU = 0, ///< Title screen / main boot menu before any loading.
    LOADING   = 1, ///< Loading world data (async streaming warmup).
    PLAYING   = 2, ///< Player is freely exploring the open world.
    PAUSED    = 3, ///< System/pause menu is open (game logic frozen).
    DEBUG_MENU= 4, ///< F1 developer overlay (stations, teleport, toggles).
};

// ===========================================================================
// DemoSaveState — minimal persistent state for Demo_Game save/load (M-DG-S2)
// ===========================================================================

/**
 * @struct DemoSaveState
 * @brief Snapshot of the Demo_Game session that can be persisted to disk.
 *
 * ============================================================================
 * TEACHING NOTE — What Goes Into a Save File?
 * ============================================================================
 * Every commercial game's save file answers: "what would a player lose if they
 * quit without saving?"  For Demo_Game the answer is:
 *
 *   • Where they are in the world          → playerX / playerZ / currentBiome.
 *   • How far they are through the quest   → questObjectiveIndex / questCompleted.
 *   • Which side activities they finished  → activities[*].progress.
 *
 * We deliberately omit:
 *   • Renderer state  — frame cap, preset, etc. are persisted in engine_config.json.
 *   • GameRuntime ECS — the full ECS World is saved by the engine's SaveSystem
 *                       (src/engine/save/save_system.hpp) which handles XP, Gil,
 *                       equipment, etc.  DemoSaveState is the lighter overlay that
 *                       the demo-specific quest/activity tracker needs.
 *
 * Format: a JSON file written to "SavedGames/demo_auto.json" (created on
 *         first save; the directory is created if it does not exist).
 *
 * ============================================================================
 * TEACHING NOTE — Versioned Save Files
 * ============================================================================
 * We embed a "version" integer so that future milestones can detect an old
 * save file and migrate it:
 *   • version == 1 → current layout.
 *   • version  < 1 → reject or migrate (no fields are known-good).
 *   • version  > 1 → forward-compatible unknown fields are ignored.
 *
 * This is the same pattern used by the engine's SaveSystem
 * (src/engine/save/save_schema.hpp, "version" field) and by shared JSON schemas.
 * ============================================================================
 */
struct DemoSaveState
{
    // ---- Schema version ----
    int version = 1; ///< Save-file schema version.  Increment on breaking changes.

    // ---- World position ----
    float     playerX     = 0.f;                ///< Player world X.
    float     playerZ     = 0.f;                ///< Player world Z.
    BiomeType currentBiome = BiomeType::GRASSLAND; ///< Active biome.

    // ---- Main quest ----
    int  questObjectiveIndex = 0;  ///< Index into DemoMainQuest::objectives.
    bool questCompleted      = false;

    // ---- Global visited stations ----
    // TEACHING NOTE — Single global set (not per-activity)
    // All STATION_INTERACT activities share one visited-station set
    // (m_visitedStations in OpenWorld).  We persist it once here at the
    // top level so the JSON mirrors the in-memory ownership model and
    // avoids any redundancy between activities.
    std::vector<std::string> globalVisitedStations;

    // ---- Side activities ----
    struct ActivitySave
    {
        std::string id;          ///< Must match DemoActivity::id.
        int         progress  = 0;
        bool        completed = false;
        // Note: per-activity visitedStations are NOT stored here.
        // The shared visited set is at globalVisitedStations above.
    };
    std::vector<ActivitySave> activities; ///< One entry per registered activity.
};



/**
 * @class OpenWorld
 * @brief Manages the Demo_Game open-world session.
 *
 * Lifecycle:
 *   Init() → Update(dt) per frame → Shutdown()
 *
 * In headless CI mode, Update() advances through a fixed number of frames
 * (kHeadlessFrames) and exits.  In windowed mode it runs until the player
 * quits.
 *
 * TEACHING NOTE — Headless Safety
 * ────────────────────────────────
 * OpenWorld does NOT call any Win32 or D3D11 API directly.  All rendering
 * state (clear colour, overlay text) is returned through accessor methods
 * so that demo_main.cpp can forward it to the renderer.  This keeps the
 * game logic testable without a GPU.
 */
class OpenWorld
{
public:
    OpenWorld();
    ~OpenWorld() = default;

    OpenWorld(const OpenWorld&)            = delete;
    OpenWorld& operator=(const OpenWorld&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Initialise the open world.
     *
     * Loads teaching station definitions, seeds the biome transition table,
     * sets the initial state to BOOT_MENU.
     *
     * @return true on success.
     */
    bool Init();

    /**
     * @brief Advance state by dt seconds.
     *
     * Runs the current-state update logic.  In headless mode increments the
     * frame counter and triggers automatic state transitions.
     *
     * @param dt         Delta time in seconds.
     * @param headless   True when running without a window (CI mode).
     */
    void Update(float dt, bool headless = false);

    /**
     * @brief Release all resources.
     */
    void Shutdown();

    // =========================================================================
    // Renderer feedback
    // =========================================================================

    /**
     * @brief Return the sky clear colour for the current biome.
     *
     * TEACHING NOTE — Biome-Driven Sky Colours
     * ──────────────────────────────────────────
     * Each biome has a characteristic sky colour that reinforces its identity:
     *   Grassland → warm daylight blue.
     *   Forest    → dim filtered green-grey.
     *   Snow      → pale icy blue.
     *   Desert    → hot orange-amber.
     *   Coast     → deep sea-blue cyan.
     */
    void GetClearColour(float& r, float& g, float& b) const;

    // =========================================================================
    // Queries
    // =========================================================================

    /// Returns the current FSM state.
    OpenWorldState  GetState()         const noexcept { return m_state; }
    /// Returns the current active biome.
    BiomeType       GetCurrentBiome()  const noexcept { return m_currentBiome; }
    /// Returns the total number of Update() calls completed.
    int             GetFrameCount()    const noexcept { return m_frameCount; }
    /// Returns true after the world has progressed past BOOT_MENU to PLAYING.
    bool            IsPlaying()        const noexcept { return m_state == OpenWorldState::PLAYING; }
    /// Returns true when the headless run has completed all validation frames.
    bool            IsHeadlessDone()   const noexcept { return m_headlessDone; }

    /// Returns the registered teaching stations.
    const std::vector<TeachingStation>& GetStations() const noexcept { return m_stations; }

    /// Returns the quest/activity manager (for HUD and CI queries).
    const DemoQuestManager& GetQuestManager() const noexcept { return m_quests; }

    // =========================================================================
    // Control
    // =========================================================================

    /**
     * @brief Advance the boot menu selection and start the game.
     *
     * In headless CI mode demo_main calls this automatically so that the
     * BOOT_MENU → LOADING → PLAYING transition is exercised without a real
     * keyboard.
     */
    void BootSelectNewGame();

    /**
     * @brief Teleport the player to a teaching station (by ID) — navigation only.
     *
     * Updates the player's world position to the station's coordinates and
     * sets the "nearest station" so that a subsequent Interact (E key) press
     * can pick it up.  Does NOT advance the quest or trigger lesson content.
     *
     * TEACHING NOTE — Separation of navigation and quest progression
     * ──────────────────────────────────────────────────────────────
     * Teleporting is a convenience shortcut (professor's remote control).
     * It must NOT auto-complete quest objectives, because that would remove the
     * player's agency and devalue the teaching experience.  Only an explicit
     * Interact action (E key) at the station counts as "visited" for the quest.
     *
     * @param stationID  Station identifier string (must match a registered station id).
     */
    void TeleportToStation(const std::string& stationID);

    /**
     * @brief Interact with the nearest teaching station (E key action).
     *
     * Checks whether the player is close enough to a station, advances the
     * main quest objective (if the station matches), advances teaching-oriented
     * side activities, and returns the lesson content for the station.
     *
     * TEACHING NOTE — Interact vs Teleport
     * ─────────────────────────────────────
     * Teleport brings the player to a station; Interact is the deliberate act
     * of engaging with it.  This mirrors commercial game design where proximity
     * is necessary but not sufficient — the player must also choose to interact.
     * (Compare: Dark Souls bonfires require pressing a button even when standing
     * on top of them; FFXV quest markers require pressing X to talk to NPCs.)
     *
     * @return The StationLesson for the nearest station, or an empty lesson
     *         if the player is not close enough to any station.
     */
    StationLesson InteractAtStation();

    /**
     * @brief Return the ID of the station the player is currently nearest to.
     *
     * Empty string if the player is not within interact range of any station.
     * Set by TeleportToStation(); in a real open world it would be computed
     * each frame from player world-space position vs. station positions.
     */
    const std::string& GetNearestStationID() const noexcept { return m_nearestStationID; }

    /**
     * @brief Retrieve a station lesson by station ID.
     *
     * Returns nullptr if the station has no lesson content loaded.
     * Lesson content is loaded by TryLoadLessonsFromJSON in Init().
     *
     * @param stationID  Station identifier.
     */
    const StationLesson* GetStationLesson(const std::string& stationID) const noexcept;

    /**
     * @brief Forward an enemy-kill event to the DemoQuestManager.
     *
     * Called by demo_main.cpp when the player defeats an enemy (e.g., via
     * GameRuntime combat) while the world is in PLAYING state.
     */
    void NotifyEnemyKilled();

    /**
     * @brief Forward an item-collect event to the DemoQuestManager.
     *
     * Called by demo_main.cpp when the player picks up an item (e.g., via
     * GameRuntime inventory) while the world is in PLAYING state.
     */
    void NotifyItemCollected();

    /**
     * @brief Attempt to load/override stations from teaching_stations.json.
     *
     * TEACHING NOTE — Data-Driven Station Loading
     * ─────────────────────────────────────────────
     * This method merges station data from the JSON content file on top of the
     * C++ defaults registered by RegisterDefaultStations().  It is compiled in
     * only when ENGINE_ENABLE_JSON is defined (nlohmann/json is available via
     * vcpkg).  When the JSON file is absent or malformed, the C++ defaults are
     * retained so the game always boots regardless of content file status.
     *
     * Exposed as public so headless acceptance tests can exercise the fallback
     * path with an intentionally bad file path.
     *
     * @param jsonPath  Absolute or relative path to teaching_stations.json.
     * @return true  if the JSON was parsed and stations updated,
     *         false if the file was missing or contained a parse error.
     */
    bool TryLoadStationsFromJSON(const std::string& jsonPath);

    /**
     * @brief Attempt to load/override lessons from station_lessons.json.
     *
     * Compiled only when ENGINE_ENABLE_JSON is defined.
     *
     * @param jsonPath  Absolute or relative path to station_lessons.json.
     * @return true if the JSON was parsed and lessons loaded,
     *         false if the file was missing or contained a parse error.
     */
    bool TryLoadLessonsFromJSON(const std::string& jsonPath);

    // =========================================================================
    // Save / Load — M-DG-S2
    // =========================================================================

    /**
     * @brief Persist the current session state to a JSON file.
     *
     * Compiled only when ENGINE_ENABLE_JSON is defined; is a no-op that
     * returns false on non-JSON builds.
     *
     * TEACHING NOTE — Save as a Side Effect of Interact
     * ──────────────────────────────────────────────────
     * Demo_Game saves automatically every time the player successfully
     * interacts at a teaching station.  This mirrors the "campfire save"
     * pattern in FFXV (save at camp) and "bonfire save" in Dark Souls:
     * the player trades a meaningful in-game action for a save point rather
     * than having an always-available menu-save (which removes tension).
     *
     * @param path  Path to the save file (default = "SavedGames/demo_auto.json").
     * @return true on success; false if the file could not be written.
     */
    bool SaveProgress(const std::string& path = "SavedGames/demo_auto.json") const;

    /**
     * @brief Restore session state from a JSON save file.
     *
     * Compiled only when ENGINE_ENABLE_JSON is defined; is a no-op that
     * returns false on non-JSON builds.
     *
     * On success the player is placed at the saved world position, the
     * DemoQuestManager is restored to the saved progress, and the state
     * machine transitions to PLAYING.  On any parse error the world remains
     * in the state it was in before the call.
     *
     * @param path  Path to the save file (default = "SavedGames/demo_auto.json").
     * @return true if the save was loaded and state restored successfully.
     */
    bool LoadProgress(const std::string& path = "SavedGames/demo_auto.json");

    /**
     * @brief Return true if a save file exists at the given path.
     *
     * Used by the boot menu to decide whether to enable the "Continue" item.
     * Works without ENGINE_ENABLE_JSON since it only checks file existence.
     *
     * @param path  Path to check (default = "SavedGames/demo_auto.json").
     */
    static bool SaveExists(const std::string& path = "SavedGames/demo_auto.json") noexcept;

    // =========================================================================
    // Constants
    // =========================================================================

    /// Number of Update() frames to run in headless CI mode before reporting PASS.
    static constexpr int kHeadlessFrames = 120;

    /// Radius (world-units) within which the player can interact with a station.
    static constexpr float kInteractRadius = 60.f;

private:
    // ---- FSM state ----
    OpenWorldState m_state     = OpenWorldState::BOOT_MENU;
    float          m_stateTime = 0.f;   ///< Time in the current state (seconds).

    // ---- World ----
    BiomeType      m_currentBiome = BiomeType::GRASSLAND;
    float          m_biomeBlend   = 0.f; ///< 0=fully in biome, 1=fully in next biome.

    // ---- Teaching stations ----
    std::vector<TeachingStation> m_stations;

    // ---- Station lessons (teaching content) ----
    std::map<std::string, StationLesson> m_lessons; ///< stationID → lesson content.

    // ---- Player position (set by TeleportToStation; would be live in full impl) ----
    float       m_playerX          = 0.f;   ///< Player world X.
    float       m_playerZ          = 0.f;   ///< Player world Z.
    std::string m_nearestStationID;          ///< Station within interact range (if any).

    // ---- Quest / activity tracker ----
    DemoQuestManager m_quests; ///< Tracks main quest + 3 side activities.

    // ---- Frame tracking ----
    int  m_frameCount    = 0;
    bool m_headlessDone  = false;

    // ---- Boot menu selection ----
    int  m_bootMenuSel   = 0; ///< 0=New Game, 1=Continue, 2=Settings, 3=Quit

    // ---- Helpers ----
    void RegisterDefaultStations();
    void UpdateBootMenu(float dt, bool headless);
    void UpdateLoading (float dt, bool headless);
    void UpdatePlaying (float dt, bool headless);
    void UpdatePaused  (float dt, bool headless);
};
