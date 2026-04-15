/**
 * @file Game.cpp
 * @brief Implementation of the main game application class.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 */

#include "Game.hpp"

#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>
#include <cmath>

// ---------------------------------------------------------------------------
// Instance (Meyers Singleton)
// ---------------------------------------------------------------------------

Game& Game::Instance()
{
    static Game instance;
    return instance;
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void Game::Init()
{
    LOG_INFO("=== Game Init ===");

    // 1. Init engine singletons.
    if (!m_renderer.Init()) {
        LOG_CRITICAL("TerminalRenderer init failed.");
        return;
    }
    InputSystem::Get().Init();
    LuaEngine::Get().Init(&m_world);

    // 2. Register all ECS component types.
    RegisterAllComponents();

    // 3. Construct systems.
    // TEACHING NOTE — Dependency Injection via Constructor
    // Each system receives only the objects it needs, via constructor.
    // CombatSystem uses EventBus internally via its singleton, so it only
    // needs the World pointer.  Other systems need a UIEvent bus to show
    // notifications to the player.
    m_combat    = std::make_unique<CombatSystem>(&m_world);
    m_inventory = std::make_unique<InventorySystem>(&m_world,
                      &EventBus<UIEvent>::Instance());
    m_quests    = std::make_unique<QuestSystem>(&m_world,
                      &EventBus<QuestEvent>::Instance(),
                      &EventBus<UIEvent>::Instance());
    m_magic     = std::make_unique<MagicSystem>(&m_world, &LuaEngine::Get());
    m_shop      = std::make_unique<ShopSystem>(&m_world,
                      &EventBus<UIEvent>::Instance());
    m_camp      = std::make_unique<CampSystem>(&m_world,
                      &EventBus<UIEvent>::Instance());
    m_weather   = std::make_unique<WeatherSystem>(
                      &EventBus<WorldEvent>::Instance());

    // 4. Create player and party.
    InitPlayer();
    InitParty();

    m_ai = std::make_unique<AISystem>(&m_world, m_playerID);

    // 5. Load the world map and starting zone.
    m_worldMap.Load();
    LoadStartingZone();

    // 6. Load Lua scripts.
    LoadScripts();

    m_currentState = GameState::MAIN_MENU;
    m_isRunning    = true;

    LOG_INFO("=== Game Init Complete ===");
}

// ---------------------------------------------------------------------------
// RegisterAllComponents
// ---------------------------------------------------------------------------

void Game::RegisterAllComponents()
{
    // TEACHING NOTE — Component Registration
    // Before any entity can use a component type, the ECS World must know about
    // it.  RegisterComponent<T>() allocates the component pool (a packed array)
    // and assigns T a unique component-type ID.  Calling View<A, B>() later
    // queries only entities that have BOTH A and B — no overhead for entities
    // that have neither.  This is the core benefit of an ECS architecture.
    m_world.RegisterComponent<TransformComponent>();
    m_world.RegisterComponent<HealthComponent>();
    m_world.RegisterComponent<StatsComponent>();
    m_world.RegisterComponent<NameComponent>();
    m_world.RegisterComponent<RenderComponent>();
    m_world.RegisterComponent<MovementComponent>();
    m_world.RegisterComponent<CombatComponent>();
    m_world.RegisterComponent<InventoryComponent>();
    m_world.RegisterComponent<QuestComponent>();
    m_world.RegisterComponent<DialogueComponent>();
    m_world.RegisterComponent<AIComponent>();
    m_world.RegisterComponent<PartyComponent>();
    m_world.RegisterComponent<MagicComponent>();
    m_world.RegisterComponent<EquipmentComponent>();
    m_world.RegisterComponent<StatusEffectsComponent>();
    m_world.RegisterComponent<LevelComponent>();
    m_world.RegisterComponent<CurrencyComponent>();
    m_world.RegisterComponent<SkillsComponent>();
    m_world.RegisterComponent<CampComponent>();
    m_world.RegisterComponent<VehicleComponent>();
    LOG_INFO("All 20 component types registered.");
}

// ---------------------------------------------------------------------------
// InitPlayer
// ---------------------------------------------------------------------------

void Game::InitPlayer()
{
    m_playerID = m_world.CreateEntity();

    // Name.
    auto& name = m_world.AddComponent<NameComponent>(m_playerID);
    name.name  = "Noctis";

    // Position.
    auto& tc      = m_world.AddComponent<TransformComponent>(m_playerID);
    tc.position   = {5.0f, 0.0f, 5.0f};

    // HP / MP.
    auto& hc  = m_world.AddComponent<HealthComponent>(m_playerID);
    hc.maxHp  = 300;
    hc.hp     = 300;
    hc.maxMp  = 100;
    hc.mp     = 100;

    // Base stats.
    auto& sc    = m_world.AddComponent<StatsComponent>(m_playerID);
    sc.strength = 15;
    sc.defence  = 10;
    sc.magic    = 12;
    sc.speed    = 14;
    sc.spirit   = 10;
    sc.luck     = 8;

    // Render (ASCII symbol).
    auto& rc       = m_world.AddComponent<RenderComponent>(m_playerID);
    rc.symbol      = '@';
    rc.colorPair   = 1;
    rc.isVisible   = true;

    // Level.
    auto& lc  = m_world.AddComponent<LevelComponent>(m_playerID);
    lc.level  = 1;

    // Currency — the authoritative Gil balance lives in CurrencyComponent.
    // TEACHING NOTE — Single Source of Truth
    // Gil is stored ONLY in CurrencyComponent.  InventoryComponent no longer
    // carries a gil field (that was removed to eliminate a desync bug where
    // buying items could deduct from one field but leave the other unchanged).
    auto& cc  = m_world.AddComponent<CurrencyComponent>(m_playerID);
    cc.gil    = 500;

    // Inventory — items only; currency is tracked separately above.
    m_world.AddComponent<InventoryComponent>(m_playerID);

    // Equipment.
    m_world.AddComponent<EquipmentComponent>(m_playerID);

    // Quests.
    m_world.AddComponent<QuestComponent>(m_playerID);

    // Magic — the MagicComponent stores known spells and cast state only.
    // MP is tracked by HealthComponent (hc.mp / hc.maxMp), not MagicComponent.
    m_world.AddComponent<MagicComponent>(m_playerID);

    // Camp.
    m_world.AddComponent<CampComponent>(m_playerID);

    // Skills.
    m_world.AddComponent<SkillsComponent>(m_playerID);

    // Movement.
    m_world.AddComponent<MovementComponent>(m_playerID);

    // Give starting items: 5 potions, 2 hi-potions, 1 Phoenix Down.
    m_inventory->AddItem(m_playerID, 23, 5);  // Potions
    m_inventory->AddItem(m_playerID, 24, 2);  // Hi-Potions
    m_inventory->AddItem(m_playerID, 26, 1);  // Phoenix Down

    // Learn starting spells: Fire, Thunder.
    m_magic->LearnSpell(m_playerID, 1);  // Fire
    m_magic->LearnSpell(m_playerID, 7);  // Thunder

    // Equip Engine Blade (itemID=10).
    m_inventory->AddItem(m_playerID, 10, 1);
    m_inventory->EquipItem(m_playerID, 10);

    // Accept first quest.
    m_quests->AcceptQuest(m_playerID, 1);  // "The Road to Dawn"

    LOG_INFO("Player 'Noctis' created (entity " + std::to_string(m_playerID) + ").");
}

// ---------------------------------------------------------------------------
// InitParty
// ---------------------------------------------------------------------------

void Game::InitParty()
{
    // Create a party entity to hold PartyComponent.
    m_partyEntity = m_world.CreateEntity();
    auto& party   = m_world.AddComponent<PartyComponent>(m_partyEntity);

    // TEACHING NOTE — Party members are regular entities with all the same
    // components as the player.  The PartyComponent on the "party entity"
    // just holds their IDs.

    auto makePartyMember = [&](const std::string& memberName, char symbol,
                                int hp, int strength, int defence,
                                int magic, int speed) -> EntityID {
        EntityID id = m_world.CreateEntity();

        auto& n = m_world.AddComponent<NameComponent>(id);
        n.name  = memberName;

        auto& tc    = m_world.AddComponent<TransformComponent>(id);
        tc.position = {5.0f, 0.0f, 5.0f};

        auto& hc  = m_world.AddComponent<HealthComponent>(id);
        hc.maxHp  = hp;
        hc.hp     = hp;
        hc.maxMp  = 80;
        hc.mp     = 80;

        auto& sc    = m_world.AddComponent<StatsComponent>(id);
        sc.strength = strength;
        sc.defence  = defence;
        sc.magic    = magic;
        sc.speed    = speed;

        auto& rc     = m_world.AddComponent<RenderComponent>(id);
        rc.symbol    = symbol;
        rc.colorPair = 2;

        m_world.AddComponent<LevelComponent>(id);
        m_world.AddComponent<InventoryComponent>(id);
        m_world.AddComponent<EquipmentComponent>(id);
        m_world.AddComponent<MagicComponent>(id);
        m_world.AddComponent<CombatComponent>(id);
        m_world.AddComponent<SkillsComponent>(id);

        return id;
    };

    EntityID gladio  = makePartyMember("Gladiolus", 'G', 400, 22, 18, 8, 10);
    EntityID ignis   = makePartyMember("Ignis",     'I', 250, 12, 10, 20, 15);
    EntityID prompto = makePartyMember("Prompto",   'P', 270, 14, 8,  14, 20);

    // Register in party.
    party.members[0]  = m_playerID;
    party.members[1]  = gladio;
    party.members[2]  = ignis;
    party.members[3]  = prompto;
    party.memberCount = 4;

    LOG_INFO("Party formed: Noctis, Gladiolus, Ignis, Prompto.");
}

// ---------------------------------------------------------------------------
// LoadStartingZone
// ---------------------------------------------------------------------------

void Game::LoadStartingZone()
{
    ZoneData data;
    data.id               = 1;
    data.name             = "Hammerhead Outskirts";
    data.description      = "A dusty outpost in the Leide region.";
    data.recommendedLevel = 1;
    data.enemyIDs         = {1, 2, 3};  // ID 1=Goblin, 2=Sabertusk, 3=Zu (from GameData)
    data.npcIDs           = {};
    data.shopIDs          = {1};
    data.tileWidth        = 40;
    data.tileHeight       = 40;
    data.isDungeon        = false;
    data.dangerLevel      = 1;

    m_currentZone = std::make_unique<Zone>();
    m_currentZone->Load(data);
    m_currentZone->SpawnEnemies(m_world);
    m_currentZone->SpawnNPCs(m_world);
    LOG_INFO("Starting zone loaded: " + data.name);
}

// ---------------------------------------------------------------------------
// LoadScripts
// ---------------------------------------------------------------------------

void Game::LoadScripts()
{
    auto& lua = LuaEngine::Get();
    lua.LoadScript("scripts/main.lua");
    lua.LoadScript("scripts/quests.lua");
    lua.LoadScript("scripts/enemies.lua");
}

// ---------------------------------------------------------------------------
// Run — the main loop
// ---------------------------------------------------------------------------

void Game::Run()
{
    // TEACHING NOTE — Fixed-Timestep Game Loop
    // ──────────────────────────────────────────
    // The game loop runs as fast as possible but caps to 60 FPS by sleeping
    // any remaining frame time.  `dt` (delta-time) is the real elapsed time
    // in seconds since the last frame; it's passed to systems like CombatSystem
    // and AISystem so they tick at the right real-world rate regardless of
    // frame rate.  The 0.1 s clamp prevents "spiral-of-death" — if the game
    // hitches for a second (e.g. the debugger pauses), dt would normally be
    // 1.0 s and physics/AI would explode; clamping limits the damage.
    using Clock    = std::chrono::steady_clock;
    using Duration = std::chrono::duration<float>;

    constexpr float TARGET_FPS  = 60.0f;
    constexpr float FRAME_TIME  = 1.0f / TARGET_FPS;

    auto lastTime = Clock::now();

    while (m_isRunning) {
        auto now = Clock::now();
        float dt = Duration(now - lastTime).count();
        lastTime = now;
        // Clamp dt to prevent spiral-of-death when debugger pauses.
        if (dt > 0.1f) dt = 0.1f;

        // Poll input.
        int key = InputSystem::Get().PollInput();

        // Check global quit.
        if (key == 27 && m_currentState == GameState::MAIN_MENU) { // ESC
            m_isRunning = false;
            break;
        }

        // Dispatch to current state.
        switch (m_currentState) {
            case GameState::MAIN_MENU:
                UpdateMainMenu(key);
                RenderMainMenu();
                break;
            case GameState::EXPLORING:
                UpdateExploring(dt, key);
                RenderExploring();
                RenderHUD();
                break;
            case GameState::COMBAT:
                UpdateCombat(dt, key);
                RenderCombat();
                break;
            case GameState::DIALOGUE:
                UpdateDialogue(key);
                RenderDialogue();
                break;
            case GameState::INVENTORY:
                UpdateInventoryMenu(key);
                RenderInventoryMenu();
                break;
            case GameState::SHOP:
                UpdateShopMenu(key);
                RenderShopMenu();
                break;
            case GameState::CAMP:
                UpdateCampMenu(key);
                RenderCampMenu();
                break;
            case GameState::VEHICLE:
                UpdateVehicle(dt, key);
                RenderExploring();
                RenderHUD();
                break;
            case GameState::GAME_OVER:
                m_renderer.DrawString(2, 10,
                    "GAME OVER  -  Press Enter to return to menu", 3);
                if (key == '\n' || key == '\r') SetState(GameState::MAIN_MENU);
                break;
        }

        m_renderer.Refresh();

        // Sleep remaining frame time.
        auto elapsed = Duration(Clock::now() - now).count();
        if (elapsed < FRAME_TIME) {
            std::this_thread::sleep_for(
                std::chrono::duration<float>(FRAME_TIME - elapsed));
        }
    }
}

// ---------------------------------------------------------------------------
// Shutdown
// ---------------------------------------------------------------------------

void Game::Shutdown()
{
    LOG_INFO("=== Game Shutdown ===");
    if (m_currentZone) m_currentZone->Unload(m_world);
    LuaEngine::Get().Shutdown();
    InputSystem::Get().Shutdown();
    m_renderer.Shutdown();
}

// ---------------------------------------------------------------------------
// SetState
// ---------------------------------------------------------------------------

void Game::SetState(GameState newState)
{
    LOG_DEBUG("State: " + std::to_string(static_cast<int>(m_currentState)) +
              " → " + std::to_string(static_cast<int>(newState)));
    m_currentState = newState;
    m_menuCursor   = 0;
}

// ---------------------------------------------------------------------------
// UpdateMainMenu
// ---------------------------------------------------------------------------

void Game::UpdateMainMenu(int key)
{
    const int MENU_ITEMS = 4; // New Game, Load, World Editor, Quit

    auto& input = InputSystem::Get();
    if (input.IsActionActive(InputAction::MOVE_UP))   m_menuCursor = (m_menuCursor - 1 + MENU_ITEMS) % MENU_ITEMS;
    if (input.IsActionActive(InputAction::MOVE_DOWN))  m_menuCursor = (m_menuCursor + 1) % MENU_ITEMS;

    if (key == '\n' || key == '\r' || input.IsActionActive(InputAction::CONFIRM)) {
        switch (m_menuCursor) {
            case 0: SetState(GameState::EXPLORING); break;
            case 1: LoadGame("savegame.txt"); SetState(GameState::EXPLORING); break;
            case 2: /* World editor would launch here */ break;
            case 3: m_isRunning = false; break;
        }
    }
}

// ---------------------------------------------------------------------------
// UpdateExploring
// ---------------------------------------------------------------------------

void Game::UpdateExploring(float dt, int key)
{
    auto& input = InputSystem::Get();

    // Player movement.
    int dx = 0, dz = 0;
    if (input.IsActionActive(InputAction::MOVE_UP))    dz = -1;
    if (input.IsActionActive(InputAction::MOVE_DOWN))  dz =  1;
    if (input.IsActionActive(InputAction::MOVE_LEFT))  dx = -1;
    if (input.IsActionActive(InputAction::MOVE_RIGHT)) dx =  1;

    if ((dx != 0 || dz != 0) && m_world.HasComponent<TransformComponent>(m_playerID)) {
        auto& tc  = m_world.GetComponent<TransformComponent>(m_playerID);
        int nx    = static_cast<int>(tc.position.x / TILE_SIZE) + dx;
        int nz    = static_cast<int>(tc.position.z / TILE_SIZE) + dz;

        TileMap& tmap = m_currentZone->GetTileMap();
        if (tmap.IsPassable(nx, nz)) {
            tc.position.x = static_cast<float>(nx) * TILE_SIZE;
            tc.position.z = static_cast<float>(nz) * TILE_SIZE;
            tc.isDirty    = true;

            // Quest: location reached.
            m_quests->OnLocationReached(m_playerID, m_worldMap.currentZoneID);

            // Check special tile.
            const Tile& tile = tmap.GetTile(nx, nz);
            if (tile.type == TileType::SHOP_TILE) {
                SetState(GameState::SHOP);
                m_shop->OpenShop(m_playerID, 1);
            } else if (tile.type == TileType::SAVE_POINT) {
                SaveGame("savegame.txt");
                // EventBus<UIEvent>::Instance() is always available (Meyers Singleton),
                // so no null check needed — just publish directly.
                UIEvent ev;
                ev.type = UIEvent::Type::SHOW_NOTIFICATION;
                ev.text = "Game saved!";
                EventBus<UIEvent>::Instance().Publish(ev);
            }
        }
    }

    // Open menu.
    if (input.IsActionActive(InputAction::OPEN_MENU)) {
        SetState(GameState::INVENTORY);
    }

    // Check enemy proximity → start combat.
    m_world.View<TransformComponent, AIComponent, HealthComponent>(
        [&](EntityID eid, TransformComponent& etc,
            AIComponent& ai, HealthComponent& ehc) {
            if (eid == m_playerID) return;
            if (ehc.isDead) return;
            if (ai.currentState == AIComponent::State::ATTACK) {
                // Start combat with this enemy.
                if (m_combatEnemies.empty()) {
                    m_combatEnemies = {eid};
                    m_combat->StartCombat(m_playerID, m_combatEnemies);
                    SetState(GameState::COMBAT);
                }
            }
        });
    m_combatEnemies.clear();

    // Update AI and weather.
    m_ai->Update(m_world, m_currentZone->GetTileMap(), dt);
    m_weather->Update(dt);

    // Call Lua update hook if defined.
    LuaEngine::Get().CallFunction("on_explore_update");
}

// ---------------------------------------------------------------------------
// UpdateCombat
// ---------------------------------------------------------------------------

void Game::UpdateCombat(float dt, int key)
{
    auto& input = InputSystem::Get();
    auto& combat = *m_combat;

    if (!combat.IsActive()) {
        // Combat ended.
        SetState(GameState::EXPLORING);
        return;
    }

    // Player action input (single key per frame).
    if (input.IsActionActive(InputAction::ATTACK)) {
        auto enemies = combat.GetActiveEnemies();
        if (!enemies.empty()) {
            combat.PlayerAttack(enemies[m_combatCursor % enemies.size()]);
        }
    } else if (key == 'i') {
        SetState(GameState::INVENTORY);
    } else if (key == 'f') {
        if (combat.PlayerFlee()) SetState(GameState::EXPLORING);
    } else if (input.IsActionActive(InputAction::MOVE_UP)) {
        int sz = static_cast<int>(combat.GetActiveEnemies().size());
        if (sz > 0) m_combatCursor = (m_combatCursor - 1 + sz) % sz;
    } else if (input.IsActionActive(InputAction::MOVE_DOWN)) {
        int sz = static_cast<int>(combat.GetActiveEnemies().size());
        if (sz > 0) m_combatCursor = (m_combatCursor + 1) % sz;
    }

    combat.Update(dt);
}

// ---------------------------------------------------------------------------
// UpdateDialogue
// ---------------------------------------------------------------------------

void Game::UpdateDialogue(int key)
{
    auto& input = InputSystem::Get();
    if (input.IsActionActive(InputAction::CONFIRM) || key == '\n') {
        SetState(GameState::EXPLORING);
    }
}

// ---------------------------------------------------------------------------
// UpdateInventoryMenu
// ---------------------------------------------------------------------------

void Game::UpdateInventoryMenu(int key)
{
    auto& input = InputSystem::Get();
    if (input.IsActionActive(InputAction::CANCEL) || key == 27) {
        SetState(m_combat->IsActive() ? GameState::COMBAT : GameState::EXPLORING);
    }
    if (input.IsActionActive(InputAction::MOVE_UP))   m_invCursor = std::max(0, m_invCursor - 1);
    if (input.IsActionActive(InputAction::MOVE_DOWN))  m_invCursor++;
    if (input.IsActionActive(InputAction::CONFIRM)) {
        // Use selected item.
        const auto& inv = m_world.GetComponent<InventoryComponent>(m_playerID);
        uint32_t cap = static_cast<uint32_t>(m_invCursor);
        if (cap < MAX_INV_SLOTS && !inv.slots[cap].IsEmpty()) {
            m_inventory->UseItem(m_playerID, inv.slots[cap].itemID);
        }
    }
}

// ---------------------------------------------------------------------------
// UpdateShopMenu
// ---------------------------------------------------------------------------

void Game::UpdateShopMenu(int key)
{
    auto& input = InputSystem::Get();
    if (input.IsActionActive(InputAction::CANCEL) || key == 27) {
        m_shop->CloseShop();
        SetState(GameState::EXPLORING);
    }
}

// ---------------------------------------------------------------------------
// UpdateCampMenu
// ---------------------------------------------------------------------------

void Game::UpdateCampMenu(int key)
{
    auto& input = InputSystem::Get();
    if (key == 'r') {
        m_camp->Rest(m_playerID);
    } else if (key == 'c') {
        auto recipes = m_camp->GetAvailableRecipes(m_playerID);
        if (!recipes.empty()) {
            m_camp->CookMeal(m_playerID, recipes[0]);
        }
    } else if (input.IsActionActive(InputAction::CANCEL) || key == 27) {
        m_camp->BreakCamp(m_playerID);
        SetState(GameState::EXPLORING);
    }
}

// ---------------------------------------------------------------------------
// UpdateVehicle
// ---------------------------------------------------------------------------

void Game::UpdateVehicle(float dt, int key)
{
    (void)dt;
    auto& input = InputSystem::Get();
    if (key == 'v') SetState(GameState::EXPLORING);  // exit vehicle
    // Vehicle movement would go here.
}

// ---------------------------------------------------------------------------
// RenderMainMenu
// ---------------------------------------------------------------------------

void Game::RenderMainMenu()
{
    auto& r = m_renderer;
    r.Clear();

    int w = r.GetWidth();
    int row = 3;

    // ASCII art title.
    r.DrawString(w/2 - 20, row++, "  _______ _____ _   _  ___  _       ______  ___   _   _______ ___   _______ _   _  ", 1);
    r.DrawString(w/2 - 20, row++, " |  _____|_   _| \\ | |/ _ \\| |     |  ____|/ _ \\ | \\ | |_   _/ _ \\ |  _____|  \\| | ", 1);
    r.DrawString(w/2 - 20, row++, " | |___    | | |  \\| / /_\\ | |     | |_  / /_\\ \\|  \\| | | |/ /_\\ \\| |_    |  \\| | ", 1);
    r.DrawString(w/2 - 20, row++, " |  ___|   | | | . ` |  _  | |     |  _| |  _  || .   | | ||  _  ||  _|   | . ` | ", 1);
    r.DrawString(w/2 - 20, row++, " | |      _| |_| |\\  | | | | |____ | |   | | | || |\\  | | || | | || |____ | |\\  | ", 1);
    r.DrawString(w/2 - 20, row++, " |_|     |_____|_| \\_|_| |_|______|_|   |_| |_||_| \\_| |_||_| |_||_______||_| \\_| ", 1);
    row += 2;
    r.DrawString(w/2 - 8, row++, "Educational Game Engine", 2);
    row += 2;

    const char* items[] = {"New Game", "Load Game", "World Editor", "Quit"};
    for (int i = 0; i < 4; ++i) {
        int cp = (i == m_menuCursor) ? 3 : 1;
        std::string prefix = (i == m_menuCursor) ? " > " : "   ";
        r.DrawString(w/2 - 8, row + i * 2, prefix + items[i], cp);
    }

    row += 9;
    r.DrawString(w/2 - 20, row, "Arrow Keys/WASD: Navigate   Enter: Select   ESC: Quit", 2);
}

// ---------------------------------------------------------------------------
// RenderExploring
// ---------------------------------------------------------------------------

void Game::RenderExploring()
{
    auto& r = m_renderer;
    r.Clear();

    TileMap& tmap = m_currentZone->GetTileMap();

    // Camera: center on player.
    int camX = 0, camZ = 0;
    if (m_world.HasComponent<TransformComponent>(m_playerID)) {
        const auto& tc = m_world.GetComponent<TransformComponent>(m_playerID);
        camX = static_cast<int>(tc.position.x / TILE_SIZE) - r.GetWidth() / 2;
        camZ = static_cast<int>(tc.position.z / TILE_SIZE) - (r.GetHeight() - 6) / 2;
    }

    // Draw visible tiles.
    int screenH = r.GetHeight() - 6;
    int screenW = r.GetWidth();
    for (int sy = 0; sy < screenH; ++sy) {
        for (int sx = 0; sx < screenW; ++sx) {
            int wx = camX + sx;
            int wz = camZ + sy;
            const Tile& tile = tmap.GetTile(wx, wz);
            if (!tile.isVisible && !tile.isExplored) continue;
            // Map tile type to a simple color-pair index for the renderer.
            int cp;
            if (!tile.isVisible) {
                cp = 8; // dim grey for explored-but-not-visible tiles
            } else {
                switch (tile.type) {
                    case TileType::WALL:       cp = CP_WALL;    break;
                    case TileType::GRASS:      cp = CP_GRASS;   break;
                    case TileType::WATER:      cp = CP_HEAL;    break;
                    case TileType::SHOP_TILE:
                    case TileType::INN_TILE:   cp = CP_MENU;    break;
                    default:                   cp = CP_DEFAULT; break;
                }
            }
            r.DrawChar(sx, sy, GetTileChar(tile.type), cp);
        }
    }

    // Draw entities.
    m_world.View<TransformComponent, RenderComponent>(
        [&](EntityID eid, TransformComponent& tc, RenderComponent& rc) {
            if (!rc.isVisible) return;
            int sx = static_cast<int>(tc.position.x / TILE_SIZE) - camX;
            int sy = static_cast<int>(tc.position.z / TILE_SIZE) - camZ;
            if (sx >= 0 && sx < screenW && sy >= 0 && sy < screenH) {
                r.DrawChar(sx, sy, rc.symbol, rc.colorPair);
            }
        });
}

// ---------------------------------------------------------------------------
// RenderCombat
// ---------------------------------------------------------------------------

void Game::RenderCombat()
{
    auto& r = m_renderer;
    r.Clear();

    int w = r.GetWidth();
    int h = r.GetHeight();

    r.DrawString(w/2 - 4, 0, "--- COMBAT ---", 3);

    // Draw player party (left side).
    int row = 2;
    if (m_world.HasComponent<HealthComponent>(m_playerID) &&
        m_world.HasComponent<NameComponent>(m_playerID))
    {
        const auto& hc = m_world.GetComponent<HealthComponent>(m_playerID);
        const auto& nc = m_world.GetComponent<NameComponent>(m_playerID);
        r.DrawString(2, row++, nc.name + "  HP: " + std::to_string(hc.hp) +
                     "/" + std::to_string(hc.maxHp) +
                     "  MP: " + std::to_string(hc.mp) +
                     "/" + std::to_string(hc.maxMp), 2);
    }

    // Draw enemies (right side).
    auto enemies = m_combat->GetActiveEnemies();
    row = 2;
    for (int i = 0; i < static_cast<int>(enemies.size()); ++i) {
        EntityID eid = enemies[i];
        if (!m_world.HasComponent<HealthComponent>(eid)) continue;
        const auto& hc = m_world.GetComponent<HealthComponent>(eid);
        std::string name = m_world.HasComponent<NameComponent>(eid)
            ? m_world.GetComponent<NameComponent>(eid).name : "Enemy";
        std::string prefix = (i == m_combatCursor) ? " > " : "   ";
        r.DrawString(w - 30, row++, prefix + name + "  HP: " +
                     std::to_string(hc.hp) + "/" + std::to_string(hc.maxHp),
                     (i == m_combatCursor) ? 3 : 1);
    }

    // Action menu at bottom.
    r.DrawString(2, h - 4, "[ A ] Attack   [ I ] Item   [ F ] Flee", 1);
    r.DrawString(2, h - 3, "[ UP/DOWN ] Select enemy target", 1);
}

// ---------------------------------------------------------------------------
// RenderDialogue
// ---------------------------------------------------------------------------

void Game::RenderDialogue()
{
    auto& r = m_renderer;
    r.Clear();
    r.DrawString(2, r.GetHeight() - 5, "[Press Enter to continue]", 2);
}

// ---------------------------------------------------------------------------
// RenderInventoryMenu
// ---------------------------------------------------------------------------

void Game::RenderInventoryMenu()
{
    auto& r = m_renderer;
    r.Clear();
    r.DrawString(2, 0, "== INVENTORY ==", 3);

    if (!m_world.HasComponent<InventoryComponent>(m_playerID)) return;
    const auto& inv = m_world.GetComponent<InventoryComponent>(m_playerID);

    int row = 2;
    for (uint32_t i = 0; i < MAX_INV_SLOTS && i < 20; ++i) {
        if (inv.slots[i].IsEmpty()) continue;
        const ItemData* item = GameDatabase::FindItem(inv.slots[i].itemID);
        std::string name  = item ? item->name : "Unknown";
        std::string line  = (static_cast<int>(i) == m_invCursor ? "> " : "  ") +
                            name + " x" + std::to_string(inv.slots[i].quantity);
        r.DrawString(2, row++, line, static_cast<int>(i) == m_invCursor ? 3 : 1);
    }

    r.DrawString(2, r.GetHeight() - 3,
        "ESC: Back   Enter: Use item   UP/DOWN: Navigate", 1);
}

// ---------------------------------------------------------------------------
// RenderShopMenu
// ---------------------------------------------------------------------------

void Game::RenderShopMenu()
{
    auto& r = m_renderer;
    r.Clear();

    uint32_t sid = m_shop->activeShopID;
    const ShopData* shop = GameDatabase::FindShop(sid);
    r.DrawString(2, 0, "== SHOP: " + (shop ? shop->name : "?") + " ==", 3);

    if (!shop) return;
    int row = 2;
    for (uint32_t itemID : shop->itemIDs) {
        const ItemData* item = GameDatabase::FindItem(itemID);
        if (!item) continue;
        uint32_t price = m_shop->GetBuyPrice(sid, itemID);
        r.DrawString(2, row++, item->name + "  " + std::to_string(price) + " Gil", 1);
    }

    r.DrawString(2, r.GetHeight() - 3, "ESC: Leave shop", 1);
}

// ---------------------------------------------------------------------------
// RenderCampMenu
// ---------------------------------------------------------------------------

void Game::RenderCampMenu()
{
    auto& r = m_renderer;
    r.Clear();
    r.DrawString(2, 0, "== CAMP ==", 3);
    r.DrawString(2, 2, "[ R ] Rest and level up", 1);
    r.DrawString(2, 3, "[ C ] Cook a meal", 1);
    r.DrawString(2, 4, "[ ESC ] Break camp", 1);
}

// ---------------------------------------------------------------------------
// RenderHUD
// ---------------------------------------------------------------------------

void Game::RenderHUD()
{
    auto& r   = m_renderer;
    int   h   = r.GetHeight();
    int   w   = r.GetWidth();

    // HP / MP bar.
    if (m_world.HasComponent<HealthComponent>(m_playerID)) {
        const auto& hc = m_world.GetComponent<HealthComponent>(m_playerID);
        std::string hpStr = "HP " + std::to_string(hc.hp) + "/" + std::to_string(hc.maxHp);
        std::string mpStr = "MP " + std::to_string(hc.mp) + "/" + std::to_string(hc.maxMp);
        r.DrawString(0, h - 2, hpStr, 2);
        r.DrawString(14, h - 2, mpStr, 4);
    }

    // Level display.
    if (m_world.HasComponent<LevelComponent>(m_playerID)) {
        const auto& lc = m_world.GetComponent<LevelComponent>(m_playerID);
        r.DrawString(30, h - 2, "LV " + std::to_string(lc.level), 3);
    }

    // Time / weather (top-right).
    std::string timeStr = m_weather->GetTimeString() + " Day " +
                          std::to_string(m_weather->GetDayNumber());
    r.DrawString(w - static_cast<int>(timeStr.size()) - 2, 0, timeStr, 2);

    // Active quest hint (bottom).
    auto activeQuests = m_quests->GetActiveQuests(m_playerID);
    if (!activeQuests.empty() && !activeQuests[0]->objectives.empty()) {
        const std::string& obj = activeQuests[0]->objectives[0].description;
        r.DrawString(0, h - 1, "Quest: " + obj, 6);
    }

    // Controls reminder.
    r.DrawString(0, h - 3, "WASD: Move  A: Attack  I: Items  M: Menu  C: Camp", 1);
}

// ---------------------------------------------------------------------------
// SaveGame
// ---------------------------------------------------------------------------

void Game::SaveGame(const std::string& filename) const
{
    // TEACHING NOTE — Simple Text-Based Save System
    // We write key=value pairs to a plain text file.  This is easy to debug
    // (you can open the save file in a text editor) and human-readable, but
    // slow and fragile for large games.  A production engine would use a
    // binary format or a database (e.g. SQLite) for speed and reliability.
    std::ofstream out(filename);
    if (!out) { LOG_ERROR("SaveGame: cannot open " + filename); return; }

    out << "playerHP=" << m_world.GetComponent<HealthComponent>(m_playerID).hp << "\n";
    out << "playerMP=" << m_world.GetComponent<HealthComponent>(m_playerID).mp << "\n";
    out << "playerLevel=" << m_world.GetComponent<LevelComponent>(m_playerID).level << "\n";
    out << "playerGil=" << m_world.GetComponent<CurrencyComponent>(m_playerID).gil << "\n";
    out << m_worldMap.Serialize();
    LOG_INFO("Game saved to " + filename);
}

// ---------------------------------------------------------------------------
// LoadGame
// ---------------------------------------------------------------------------

void Game::LoadGame(const std::string& filename)
{
    std::ifstream in(filename);
    if (!in) { LOG_WARN("LoadGame: file not found: " + filename); return; }

    std::string line;
    std::string worldData;
    while (std::getline(in, line)) {
        if (line.substr(0, 8) == "playerHP") {
            m_world.GetComponent<HealthComponent>(m_playerID).hp =
                std::stoi(line.substr(9));
        } else if (line.substr(0, 8) == "playerMP") {
            m_world.GetComponent<HealthComponent>(m_playerID).mp =
                std::stoi(line.substr(9));
        } else if (line.substr(0, 11) == "playerLevel") {
            m_world.GetComponent<LevelComponent>(m_playerID).level =
                static_cast<uint32_t>(std::stoul(line.substr(12)));
        } else if (line.substr(0, 9) == "playerGil") {
            m_world.GetComponent<CurrencyComponent>(m_playerID).gil =
                std::stoull(line.substr(10));
        } else {
            worldData += line + "\n";
        }
    }
    if (!worldData.empty()) m_worldMap.Deserialize(worldData);
    LOG_INFO("Game loaded from " + filename);
}
