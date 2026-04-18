/**
 * @file test_world.hpp
 * @brief TestWorld — an in-engine demonstration of every M3 gameplay system.
 *
 * ============================================================================
 * TEACHING NOTE — Why a Test World?
 * ============================================================================
 * A *test world* is an in-engine scene that boots all implemented systems at
 * once so a developer can verify the whole stack is functional in a running
 * debug or release build.  It is not a level designed for players — it is a
 * living integration test.
 *
 * The test world demonstrates:
 *   • Open world  — 40×30 tile map with plains, roads, forest, and ruins.
 *   • Character   — Noctis (player entity) with full component suite.
 *   • Movement    — Entity position updates each frame; printed to console.
 *   • Doors       — Interactable door entities in the building perimeter.
 *   • Buildings   — Crown City Inn + Hammerhead Outpost (camp + shop).
 *   • Enemies     — Goblin / Wyvern / Tonberry with AI state machines.
 *   • Combat      — ATB-based CombatSystem engaged when enemies close range.
 *   • AI          — FSM + A* pathfinding (AISystem) chases player.
 *   • Weather     — Day/night cycle + probabilistic weather FSM.
 *   • Quests      — "Hunt the Goblins" kill-objective tracked live.
 *   • Inventory   — Player starts with potions; drops loot on enemy death.
 *   • Camp        — Player rests at the Inn; HP/MP restored; meal buff.
 *   • Shop        — Hammerhead shop BuyItem demo; gil transaction.
 *   • Audio       — XAudio2 backend initialised; music FSM wired.
 *
 * ============================================================================
 * TEACHING NOTE — Rendered Visual Feedback
 * ============================================================================
 * The D3D11 renderer cannot yet draw geometry, but it can clear the back-
 * buffer to any colour.  TestWorld maps game state to a clear-colour:
 *
 *   Day + Clear weather   → sky-blue (0.40, 0.60, 0.90)
 *   Night                 → deep navy (0.03, 0.05, 0.18)
 *   Dawn / Dusk           → warm amber (0.90, 0.50, 0.20)
 *   Rain                  → slate (0.25, 0.32, 0.42)
 *   Active combat         → red tint (0.70, 0.12, 0.12)
 *   Victory               → gold pulse (1.00, 0.85, 0.10)
 *   Camp / resting        → campfire orange (0.50, 0.28, 0.06)
 *
 * As more rendering milestones (M3 texture, M4 animation, M5 physics) land,
 * replace the clear-colour logic here with actual draw calls.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Target: Windows (MSVC) + Linux (for cook / CI builds)
 */

#pragma once

// ---------------------------------------------------------------------------
// Engine headers (platform-independent)
// ---------------------------------------------------------------------------
#include "engine/ecs/ECS.hpp"
#include "engine/core/EventBus.hpp"
#include "engine/core/Logger.hpp"
#include "engine/audio/audio_system.hpp"

// ---------------------------------------------------------------------------
// Game systems — pure C++17, no ncurses dependency
// ---------------------------------------------------------------------------
#include "game/systems/CombatSystem.hpp"
#include "game/systems/AISystem.hpp"
#include "game/systems/WeatherSystem.hpp"
#include "game/systems/QuestSystem.hpp"
#include "game/systems/InventorySystem.hpp"
#include "game/systems/ShopSystem.hpp"
#include "game/systems/CampSystem.hpp"
#include "game/world/TileMap.hpp"
#include "game/world/WorldMap.hpp"
#include "game/GameData.hpp"

#include <memory>
#include <string>
#include <array>

namespace engine {
namespace sandbox {

// ===========================================================================
// TestWorld
// ===========================================================================

/**
 * @class TestWorld
 * @brief Integration test scene that boots every M0–M3 system together.
 *
 * TEACHING NOTE — Integration Tests vs Unit Tests
 * ──────────────────────────────────────────────────
 * Unit tests check a single class in isolation.  Integration tests check
 * that multiple systems collaborate correctly.  TestWorld is an integration
 * test expressed as a *running game scene*, which has several advantages:
 *
 *   1. Real timing — systems run at variable dt, just as in shipping code.
 *   2. Cross-system events — QuestSystem reacts to CombatSystem kills via
 *      EventBus, just as in the final game.
 *   3. Visual confirmation — a developer watching the window can see state
 *      changes (red flash on combat entry, gold on victory) without reading
 *      raw log output.
 *
 * Usage:
 * @code
 *   TestWorld tw;
 *   if (!tw.Init()) return 1;
 *
 *   while (window.IsRunning()) {
 *       tw.Update(dt);
 *       float r, g, b;
 *       tw.GetClearColour(r, g, b);
 *       renderer->DrawFrame(r, g, b);
 *   }
 * @endcode
 */
class TestWorld
{
public:
    // -----------------------------------------------------------------------
    // Constructor / Destructor
    // -----------------------------------------------------------------------

    TestWorld();
    ~TestWorld() = default;

    // Non-copyable (owns unique_ptr systems + ECS world).
    TestWorld(const TestWorld&)            = delete;
    TestWorld& operator=(const TestWorld&) = delete;

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    /**
     * @brief Initialise the test world.
     *
     * Builds the tile map, spawns all entities, wires every system,
     * and starts the first quest.
     *
     * @return true on success; false if a critical system failed to init.
     */
    bool Init();

    /**
     * @brief Advance the test world by one frame.
     *
     * Runs all systems in dependency order:
     *   WeatherSystem → AISystem → CombatSystem → QuestSystem →
     *   InventorySystem → ShopSystem → CampSystem → AudioSystem.
     *
     * Also ticks the movement simulation and prints a status snapshot
     * to stdout every STATUS_PRINT_INTERVAL frames.
     *
     * @param dt  Seconds since the previous frame.
     */
    void Update(float dt);

    /**
     * @brief Get the D3D11 clear colour for the current frame.
     *
     * Maps game state (weather, combat, camp, time-of-day) to an RGB triple
     * in [0,1] that the renderer uses as the background clear colour.
     *
     * @param r  Out: red channel   [0.0, 1.0]
     * @param g  Out: green channel [0.0, 1.0]
     * @param b  Out: blue channel  [0.0, 1.0]
     */
    void GetClearColour(float& r, float& g, float& b) const;

    /**
     * @brief Print a formatted status snapshot to stdout.
     *
     * Called automatically every STATUS_PRINT_INTERVAL frames; can also be
     * called manually to dump state on demand.
     */
    void PrintStatus() const;

    /**
     * @brief True once the scripted demo sequence is complete.
     *
     * In headless mode the caller should check this and exit 0.
     * In windowed mode the world continues running for free-play.
     */
    bool IsDemoComplete() const { return m_demoComplete; }

    // -----------------------------------------------------------------------
    // Accessors (for headless exit code logic)
    // -----------------------------------------------------------------------

    /** @return True if all required systems passed their boot checks. */
    bool AllSystemsOk() const { return m_allSystemsOk; }

private:
    // -----------------------------------------------------------------------
    // Map / world construction
    // -----------------------------------------------------------------------

    /** Build the 40×30 tile map with town, roads, forest, and ruins. */
    void BuildTileMap();

    /** Carve a filled rectangle of the given type into the tile map. */
    void FillRect(int x, int y, int w, int h, TileType type);

    /** Draw the outline of a rectangle with WALL tiles; fill interior. */
    void DrawBuilding(int x, int y, int w, int h,
                      int doorX, int doorY, TileType interiorType);

    // -----------------------------------------------------------------------
    // Entity factories
    // -----------------------------------------------------------------------

    /** Spawn the player entity (Noctis) and return its EntityID. */
    EntityID SpawnPlayer();

    /** Spawn an enemy entity using the given EnemyData template. */
    EntityID SpawnEnemy(const EnemyData& data, int tx, int ty);

    /** Spawn a static NPC with a name and position. */
    EntityID SpawnNPC(const std::string& name, int tx, int ty);

    /** Spawn a building prop entity with a name, position. */
    EntityID SpawnBuilding(const std::string& name, int tx, int ty);

    /** Spawn an interactable door entity at the given tile position. */
    EntityID SpawnDoor(const std::string& label, int tx, int ty);

    // -----------------------------------------------------------------------
    // System tick helpers
    // -----------------------------------------------------------------------

    /** Move the player along the scripted path. */
    void TickPlayerMovement(float dt);

    /** Engage combat if an enemy is adjacent to the player. */
    void TickCombatTrigger();

    /** Handle combat result: distribute XP/loot, advance quests. */
    void HandleCombatResult();

    /** Script: player walks to camp position and rests. */
    void TickCampScript(float dt);

    /** Script: player visits shop and buys one item. */
    void TickShopScript();

    // -----------------------------------------------------------------------
    // Constants
    // -----------------------------------------------------------------------

    static constexpr int   MAP_W         = 40;   ///< Tile map width.
    static constexpr int   MAP_H         = 30;   ///< Tile map height.
    static constexpr int   STATUS_PRINT_INTERVAL = 60; ///< Frames between status prints.

    /// TEACHING NOTE — Demo sequence length
    /// We run 600 scripted frames (~10 simulated seconds at 60fps) that
    /// exercise every system in order.  After that the world runs in free-
    /// play so developers can inspect the result in the windowed mode.
    static constexpr int   DEMO_FRAMES   = 600;

    // -----------------------------------------------------------------------
    // ECS World
    // -----------------------------------------------------------------------

    World m_world;

    // -----------------------------------------------------------------------
    // Tile map
    // -----------------------------------------------------------------------

    TileMap m_tileMap;

    // -----------------------------------------------------------------------
    // Key entity IDs
    // -----------------------------------------------------------------------

    EntityID m_player       = NULL_ENTITY;
    EntityID m_goblin       = NULL_ENTITY;
    EntityID m_sabretusk    = NULL_ENTITY;
    EntityID m_foras        = NULL_ENTITY;   ///< Ruins daemon
    EntityID m_ignis        = NULL_ENTITY;   ///< Party NPC
    EntityID m_dave         = NULL_ENTITY;   ///< Quest-giver NPC
    EntityID m_innBuilding  = NULL_ENTITY;
    EntityID m_shopBuilding = NULL_ENTITY;
    EntityID m_innDoor      = NULL_ENTITY;
    EntityID m_shopDoor     = NULL_ENTITY;

    // -----------------------------------------------------------------------
    // Event buses
    // -----------------------------------------------------------------------

    EventBus<CombatEvent> m_combatBus;
    EventBus<QuestEvent>  m_questBus;
    EventBus<UIEvent>     m_uiBus;
    EventBus<WorldEvent>  m_worldBus;

    // -----------------------------------------------------------------------
    // Systems (unique_ptr to allow deferred construction with World&)
    // -----------------------------------------------------------------------

    std::unique_ptr<WeatherSystem>   m_weather;
    std::unique_ptr<AISystem>        m_ai;
    std::unique_ptr<CombatSystem>    m_combat;
    std::unique_ptr<QuestSystem>     m_quests;
    std::unique_ptr<InventorySystem> m_inventory;
    std::unique_ptr<ShopSystem>      m_shop;
    std::unique_ptr<CampSystem>      m_camp;
    audio::AudioSystem               m_audio;

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    int   m_frame          = 0;
    float m_accumTime      = 0.0f;    ///< Accumulated seconds (for movement).
    bool  m_combatTriggered = false;
    bool  m_shopVisited     = false;
    bool  m_campVisited     = false;
    bool  m_demoComplete    = false;
    bool  m_allSystemsOk    = false;
    bool  m_audioReady      = false;
    int   m_goblinKills     = 0;

    /// Victory flash timer (seconds remaining after enemy death).
    float m_victoryTimer   = 0.0f;
    static constexpr float VICTORY_FLASH_DURATION = 3.0f;

    // Quest IDs (from GameDatabase::GetQuests())
    static constexpr uint32_t QUEST_HUNT_GOBLINS  = 1;  // "The Road to Dawn" — Kill 3 Goblins
    static constexpr uint32_t QUEST_STOLEN_GOODS  = 2;  // "Stolen Goods" — side quest

    // Shop / enemy data IDs (from GameDatabase)
    static constexpr uint32_t ENEMY_GOBLIN    = 1;   // Tutorial enemy
    static constexpr uint32_t ENEMY_SABRETUSK = 2;   // Level 3 wolf
    static constexpr uint32_t ENEMY_FORAS     = 16;  // Ruins daemon
    static constexpr uint32_t SHOP_HAMMERHEAD = 1;
    static constexpr uint32_t ITEM_POTION     = 23;  // Potion (item id=23 in GameDB)

    // Scripted player waypoints (tile positions)
    struct Waypoint { int x; int y; };
    static constexpr std::array<Waypoint, 8> PLAYER_PATH = {{
        {20, 15},  // 0: start — open plains
        {20, 12},  // 1: approach road
        {16, 12},  // 2: on road — moves toward town
        {16,  8},  // 3: enters town
        {14,  8},  // 4: inn courtyard (triggers camp)
        {20,  8},  // 5: shop courtyard (triggers shop)
        {24, 16},  // 6: heads east toward ruins
        {24, 22},  // 7: arrives at ruins (triggers Wyvern combat)
    }};

    int   m_waypointIdx  = 0;    ///< Current target waypoint index.
    float m_moveSpeed    = 3.0f; ///< Tiles per second.
};

} // namespace sandbox
} // namespace engine
