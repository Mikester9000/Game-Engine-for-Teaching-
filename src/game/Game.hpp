/**
 * @file Game.hpp
 * @brief The main application class — owns all systems and drives the game loop.
 *
 * ============================================================================
 * TEACHING NOTE — The Application Class Pattern
 * ============================================================================
 *
 * A Game class (sometimes called Application, Engine, or GameManager) is the
 * top-level object that:
 *
 *  1. OWNS all subsystem objects (renderer, input, ECS world, systems).
 *  2. INITIALISES them in the correct dependency order.
 *  3. RUNS the main loop (a fixed-timestep update + render loop).
 *  4. SHUTS THEM DOWN in reverse order (RAII / destructor chain).
 *
 * It is NOT responsible for game-specific logic — that lives in Systems.
 * The Game class is the "glue" between the platform (OS, hardware) and the
 * game logic.
 *
 * ─── Singleton vs. Dependency Injection ─────────────────────────────────────
 *
 * Using a Singleton (Game::Instance()) for the top-level application class is
 * widely accepted: there is genuinely exactly one game running at a time.
 * However, all SUBSYSTEMS should be accessed through the Game object (not as
 * their own singletons when possible) so that unit tests can construct a Game
 * with mock systems.
 *
 * ─── Game States ─────────────────────────────────────────────────────────────
 *
 * The Game class contains a STATE MACHINE over GameState enum values.
 * Each Update() and Render() call dispatches to a state-specific function.
 * This keeps state transitions explicit and prevents "spaghetti" logic where
 * Update() checks dozens of boolean flags.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../engine/core/Types.hpp"
#include "../engine/core/Logger.hpp"
#include "../engine/core/EventBus.hpp"
#include "../engine/ecs/ECS.hpp"
#include "../engine/rendering/Renderer.hpp"
#include "../engine/input/InputSystem.hpp"
#include "../engine/scripting/LuaEngine.hpp"

#include "GameData.hpp"
#include "systems/CombatSystem.hpp"
#include "systems/InventorySystem.hpp"
#include "systems/QuestSystem.hpp"
#include "systems/MagicSystem.hpp"
#include "systems/ShopSystem.hpp"
#include "systems/CampSystem.hpp"
#include "systems/WeatherSystem.hpp"
#include "systems/AISystem.hpp"
#include "world/TileMap.hpp"
#include "world/Zone.hpp"
#include "world/WorldMap.hpp"

// ---------------------------------------------------------------------------
// Game class
// ---------------------------------------------------------------------------

/**
 * @class Game
 * @brief Singleton application class that owns all game systems.
 *
 * USAGE:
 * @code
 *   int main() {
 *       Game& g = Game::Instance();
 *       g.Init();
 *       g.Run();
 *       g.Shutdown();
 *   }
 * @endcode
 */
class Game {
public:
    /// Meyers-singleton: constructed once, lives for the program's duration.
    static Game& Instance();

    // Prevent copying (only one Game).
    Game(const Game&)            = delete;
    Game& operator=(const Game&) = delete;

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    /** @brief Set up all systems, create the player, load the first zone. */
    void Init();

    /** @brief Run the main loop until the player quits. */
    void Run();

    /** @brief Tear down all systems in reverse order. */
    void Shutdown();

    // -----------------------------------------------------------------------
    // State management
    // -----------------------------------------------------------------------

    void      SetState(GameState newState);
    GameState GetState() const { return m_currentState; }

    // -----------------------------------------------------------------------
    // System accessors (for Lua bindings and cross-system calls)
    // -----------------------------------------------------------------------

    World&           GetWorld()    { return m_world; }
    CombatSystem&    GetCombat()   { return *m_combat; }
    InventorySystem& GetInventory(){ return *m_inventory; }
    QuestSystem&     GetQuests()   { return *m_quests; }
    MagicSystem&     GetMagic()    { return *m_magic; }
    ShopSystem&      GetShop()     { return *m_shop; }
    CampSystem&      GetCamp()     { return *m_camp; }
    WeatherSystem&   GetWeather()  { return *m_weather; }
    AISystem&        GetAI()       { return *m_ai; }
    WorldMap&        GetWorldMap() { return m_worldMap; }
    Zone&            GetCurrentZone() { return *m_currentZone; }

    EntityID GetPlayerID() const  { return m_playerID; }
    EntityID GetPartyID()  const  { return m_partyEntity; }

    // -----------------------------------------------------------------------
    // Save / Load
    // -----------------------------------------------------------------------

    /** @brief Serialize all game state to a file. */
    void SaveGame(const std::string& filename) const;

    /** @brief Load game state from a file. */
    void LoadGame(const std::string& filename);

private:
    Game() = default;

    // -----------------------------------------------------------------------
    // Initialisation helpers
    // -----------------------------------------------------------------------

    void InitPlayer();
    void InitParty();
    void LoadStartingZone();
    void LoadScripts();
    void RegisterAllComponents();

    // -----------------------------------------------------------------------
    // Per-state update/render
    // -----------------------------------------------------------------------

    void UpdateMainMenu(int key);
    void UpdateExploring(float dt, int key);
    void UpdateCombat(float dt, int key);
    void UpdateDialogue(int key);
    void UpdateInventoryMenu(int key);
    void UpdateShopMenu(int key);
    void UpdateCampMenu(int key);
    void UpdateVehicle(float dt, int key);

    void RenderMainMenu();
    void RenderExploring();
    void RenderCombat();
    void RenderDialogue();
    void RenderInventoryMenu();
    void RenderShopMenu();
    void RenderCampMenu();
    void RenderHUD();

    // -----------------------------------------------------------------------
    // Member data
    // -----------------------------------------------------------------------

    // ECS world — owns all entities and component storage.
    World m_world;

    // Renderer (owns the ncurses terminal).
    TerminalRenderer m_renderer;

    // Game systems.
    std::unique_ptr<CombatSystem>    m_combat;
    std::unique_ptr<InventorySystem> m_inventory;
    std::unique_ptr<QuestSystem>     m_quests;
    std::unique_ptr<MagicSystem>     m_magic;
    std::unique_ptr<ShopSystem>      m_shop;
    std::unique_ptr<CampSystem>      m_camp;
    std::unique_ptr<WeatherSystem>   m_weather;
    std::unique_ptr<AISystem>        m_ai;

    // Zone and world map.
    std::unique_ptr<Zone> m_currentZone;
    WorldMap              m_worldMap;

    // Event buses.
    EventBus<CombatEvent> m_combatBus;
    EventBus<QuestEvent>  m_questBus;
    EventBus<WorldEvent>  m_worldBus;
    EventBus<UIEvent>     m_uiBus;

    // Game state.
    GameState m_currentState  = GameState::MAIN_MENU;
    EntityID  m_playerID      = NULL_ENTITY;
    EntityID  m_partyEntity   = NULL_ENTITY;
    bool      m_isRunning     = false;

    // UI cursor / menu state.
    int      m_menuCursor      = 0;
    int      m_invCursor       = 0;
    int      m_combatCursor    = 0;
    uint32_t m_activeShopID    = 0;

    // Pending UI message shown between states.
    std::string m_pendingMessage;
    bool        m_showingMessage = false;

    // Combat enemies for current encounter.
    std::vector<EntityID> m_combatEnemies;
};
