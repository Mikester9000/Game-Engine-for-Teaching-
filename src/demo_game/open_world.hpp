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
#include <cstdint>

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
 * TEACHING NOTE — Teaching Stations as Data
 * ──────────────────────────────────────────
 * Each station is defined in JSON (teaching_stations.json) and loaded at
 * startup.  Keeping station definitions in data (not code) lets a student
 * add a new station by editing JSON only — zero C++ changes needed.
 *
 * In-world: each station has a unique (worldX, worldZ) position and a
 * teleport marker visible in the F1 debug menu.
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
// OpenWorld — manages the demo game open-world session
// ===========================================================================

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
     * @brief Teleport the player to a teaching station (by ID).
     *
     * Used by the F1 debug menu.  If the station ID is not found, does nothing.
     *
     * @param stationID  Station identifier string from teaching_stations.json.
     */
    void TeleportToStation(const std::string& stationID);

    // =========================================================================
    // Constants
    // =========================================================================

    /// Number of Update() frames to run in headless CI mode before reporting PASS.
    static constexpr int kHeadlessFrames = 120;

private:
    // ---- FSM state ----
    OpenWorldState m_state     = OpenWorldState::BOOT_MENU;
    float          m_stateTime = 0.f;   ///< Time in the current state (seconds).

    // ---- World ----
    BiomeType      m_currentBiome = BiomeType::GRASSLAND;
    float          m_biomeBlend   = 0.f; ///< 0=fully in biome, 1=fully in next biome.

    // ---- Teaching stations ----
    std::vector<TeachingStation> m_stations;

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
