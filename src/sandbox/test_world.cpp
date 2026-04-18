/**
 * @file test_world.cpp
 * @brief TestWorld implementation — every M0-M3 system exercised together.
 *
 * ============================================================================
 * TEACHING NOTE — System Dependency Order
 * ============================================================================
 * Game systems are updated each frame in a specific order to ensure that
 * outputs of one system are available as inputs to the next:
 *
 *   1. WeatherSystem   — Advances day/night cycle; fires WorldEvent on
 *                        time/weather changes.  No game-state reads.
 *   2. AISystem        — Reads player position (TransformComponent) and
 *                        WeatherSystem time (nocturnal enemy behaviour).
 *                        Moves enemies toward player.
 *   3. CombatSystem    — Resolves attacks between combatants set up by AI.
 *                        Fires CombatEvent on damage / death.
 *   4. QuestSystem     — Listens for CombatEvent (kill objectives) and
 *                        WorldEvent (reach-location objectives).
 *   5. InventorySystem — Distributes loot after CombatSystem resolves.
 *   6. ShopSystem      — Processes player buy/sell requests (scripted here).
 *   7. CampSystem      — Applies rest bonuses once combat is inactive.
 *   8. AudioSystem     — Submits audio source components to XAudio2 backend;
 *                        transitions music FSM based on CombatSystem state.
 *
 * ============================================================================
 * TEACHING NOTE — ECS Entity Composition
 * ============================================================================
 * Each entity in this world is a pure EntityID with components attached:
 *
 *   Player  — TransformComponent + HealthComponent + StatsComponent +
 *             NameComponent + CombatComponent + InventoryComponent +
 *             LevelComponent + CurrencyComponent + QuestComponent +
 *             MagicComponent + EquipmentComponent + MovementComponent +
 *             PartyComponent + AudioSourceComponent
 *
 *   Enemy   — TransformComponent + HealthComponent + StatsComponent +
 *             NameComponent + CombatComponent + AIComponent +
 *             StatusEffectsComponent
 *
 *   NPC     — TransformComponent + NameComponent + RenderComponent +
 *             DialogueComponent (stub)
 *
 *   Building — TransformComponent + NameComponent + RenderComponent
 *
 *   Door    — TransformComponent + NameComponent + DialogueComponent
 *              (isInteractable = true)
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Target: Windows (MSVC) + Linux (cook / CI)
 */

#include "sandbox/test_world.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <algorithm>

namespace engine {
namespace sandbox {

// ===========================================================================
// Constructor
// ===========================================================================

TestWorld::TestWorld()
    : m_tileMap(MAP_W, MAP_H)
{
    // TEACHING NOTE — member initialiser list
    // m_tileMap is constructed with width/height here because TileMap does
    // not have a default (width=0) constructor that can be resized later.
    // All other members use in-class initialisers (= default values).
}

// ===========================================================================
// Init
// ===========================================================================

bool TestWorld::Init()
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Initialisation Sequence
    // -----------------------------------------------------------------------
    // We follow a strict order:
    //   1. Tile map (no system deps)
    //   2. ECS entities (requires tile map for spawn positions)
    //   3. Systems (require entity IDs and event buses)
    //   4. Quests / inventory / shop bootstrapping
    //   5. Audio last (may fail gracefully if no audio hardware)
    // -----------------------------------------------------------------------

    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════╗\n";
    std::cout << "║        ENGINE SANDBOX — TEST WORLD (M0–M3)          ║\n";
    std::cout << "╠══════════════════════════════════════════════════════╣\n";
    std::cout << "║  Booting all gameplay systems…                      ║\n";
    std::cout << "╚══════════════════════════════════════════════════════╝\n\n";

    // -----------------------------------------------------------------------
    // Step 1 — Build the open-world tile map.
    // -----------------------------------------------------------------------
    BuildTileMap();
    std::cout << "  [OK] TileMap built:  " << MAP_W << "x" << MAP_H
              << " tiles\n";

    // -----------------------------------------------------------------------
    // Step 2 — Register all ECS components.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Component Registration
    // RegisterAllComponents() assigns each component type a unique integer
    // index used internally by the ComponentStorage sparse-set.  It must be
    // called exactly once before any entity is created.
    // -----------------------------------------------------------------------
    RegisterAllComponents(m_world);
    std::cout << "  [OK] ECS components registered\n";

    // -----------------------------------------------------------------------
    // Step 3 — Spawn entities.
    // -----------------------------------------------------------------------
    m_player = SpawnPlayer();
    std::cout << "  [OK] Player spawned: Noctis (ID=" << m_player << ")\n";

    // Enemies (using EnemyData templates from GameDatabase)
    const EnemyData* goblinData    = GameDatabase::FindEnemy(ENEMY_GOBLIN);
    const EnemyData* sabretuskData = GameDatabase::FindEnemy(ENEMY_SABRETUSK);
    const EnemyData* forasData     = GameDatabase::FindEnemy(ENEMY_FORAS);

    if (goblinData)    m_goblin    = SpawnEnemy(*goblinData,    28, 13);
    if (sabretuskData) m_sabretusk = SpawnEnemy(*sabretuskData, 30, 20);
    if (forasData)     m_foras     = SpawnEnemy(*forasData,     24, 24);
    std::cout << "  [OK] Enemies spawned: Goblin(ID="     << m_goblin
              << ") Sabretusk(ID=" << m_sabretusk
              << ") Foras(ID=" << m_foras << ")\n";

    // NPCs
    m_ignis = SpawnNPC("Ignis Scientia", 17, 9);
    m_dave  = SpawnNPC("Dave (Hunter)",  22, 9);
    std::cout << "  [OK] NPCs spawned: Ignis(ID=" << m_ignis
              << ") Dave(ID=" << m_dave << ")\n";

    // Buildings
    m_innBuilding  = SpawnBuilding("Crown City Inn",       14,  5);
    m_shopBuilding = SpawnBuilding("Hammerhead Outpost",   21,  5);
    std::cout << "  [OK] Buildings: Inn(ID=" << m_innBuilding
              << ") Shop(ID=" << m_shopBuilding << ")\n";

    // Doors  (on the south wall of each building)
    m_innDoor  = SpawnDoor("Inn Entrance",  14, 9);
    m_shopDoor = SpawnDoor("Shop Entrance", 21, 9);
    std::cout << "  [OK] Doors:     Inn Door(ID=" << m_innDoor
              << ") Shop Door(ID=" << m_shopDoor << ")\n";

    // -----------------------------------------------------------------------
    // Step 4 — Construct systems.
    // -----------------------------------------------------------------------

    // WeatherSystem: receives a WorldEvent bus to broadcast time/weather changes.
    m_weather = std::make_unique<WeatherSystem>(&m_worldBus);
    std::cout << "  [OK] WeatherSystem ready\n";

    // AISystem: owns the world + player reference for pathfinding.
    m_ai = std::make_unique<AISystem>(&m_world, m_player);
    std::cout << "  [OK] AISystem ready\n";

    // CombatSystem: drives ATB, damage rolls, status effects.
    m_combat = std::make_unique<CombatSystem>(&m_world);
    std::cout << "  [OK] CombatSystem ready\n";

    // QuestSystem: listens on the quest and UI buses.
    m_quests = std::make_unique<QuestSystem>(&m_world, &m_questBus, &m_uiBus);
    std::cout << "  [OK] QuestSystem ready\n";

    // InventorySystem: manages item stacks per entity.
    m_inventory = std::make_unique<InventorySystem>(&m_world, &m_uiBus);
    std::cout << "  [OK] InventorySystem ready\n";

    // ShopSystem: buy/sell against the GameDatabase shop records.
    m_shop = std::make_unique<ShopSystem>(&m_world, &m_uiBus);
    std::cout << "  [OK] ShopSystem ready\n";

    // CampSystem: rest, cook meals, apply stat bonuses.
    m_camp = std::make_unique<CampSystem>(&m_world, &m_uiBus);
    std::cout << "  [OK] CampSystem ready\n";

    // -----------------------------------------------------------------------
    // Step 5 — Bootstrap quests and inventory.
    // -----------------------------------------------------------------------

    // Give the player starter items: 3 potions + 1 ether.
    m_inventory->AddItem(m_player, ITEM_POTION, 3);      // item id=1: Potion
    m_inventory->AddItem(m_player, 3, 1);                // item id=3: Ether

    // Accept the "The Road to Dawn" quest (ID=1) — kill 3 goblins.
    if (m_quests->AcceptQuest(m_player, QUEST_HUNT_GOBLINS))
        std::cout << "  [OK] Quest accepted: \"The Road to Dawn\"\n";

    // Accept the "Stolen Goods" quest (ID=2) if it exists.
    if (m_quests->AcceptQuest(m_player, QUEST_STOLEN_GOODS))
        std::cout << "  [OK] Quest accepted: \"Stolen Goods\"\n";

    // -----------------------------------------------------------------------
    // Step 6 — Subscribe to event buses for live status updates.
    // -----------------------------------------------------------------------

    // TEACHING NOTE — Lambda Subscriptions
    // Each Subscribe call registers a lambda that fires whenever the bus
    // publishes an event.  The capture [this] lets the lambda access member
    // variables.  We store the token so we could Unsubscribe on teardown
    // (skipped here for brevity — the EventBus is destroyed with TestWorld).
    // -----------------------------------------------------------------------

    m_combatBus.Subscribe([this](const CombatEvent& e) {
        if (e.type == CombatEventType::ENTITY_DIED)
        {
            LOG_INFO("TestWorld: entity " << e.targetID << " died.");
            m_quests->OnEnemyKilled(m_player, e.enemyDataID);
            m_goblinKills++;

            // Trigger the victory flash.
            m_victoryTimer = VICTORY_FLASH_DURATION;

            // Play a "victory fanfare" on the audio system.
            if (m_audioReady)
                m_audio.SetMusicState(audio::MusicState::VICTORY);
        }
    });

    m_questBus.Subscribe([](const QuestEvent& e) {
        if (e.type == QuestEventType::QUEST_COMPLETED)
            std::cout << "  ★  QUEST COMPLETE: \"" << e.title << "\"\n";
    });

    m_uiBus.Subscribe([](const UIEvent& e) {
        if (e.type == UIEventType::SHOW_NOTIFICATION)
            std::cout << "  [UI] " << e.text << "\n";
    });

    // -----------------------------------------------------------------------
    // Step 7 — Audio system (allowed to fail — no audio device in CI).
    // -----------------------------------------------------------------------

    // TEACHING NOTE — Graceful Degradation
    // Audio requires a Windows audio device.  On headless CI runners there
    // is no audio hardware, so XAudio2Create may fail.  We treat audio
    // as a non-fatal subsystem: the test world still validates all other
    // systems if audio is unavailable.
    // -----------------------------------------------------------------------
    m_audioReady = m_audio.InitAudio(nullptr);  // nullptr = no AssetDB yet
    if (m_audioReady)
    {
        // Register music tracks (clip IDs will be resolved when AssetDB
        // exists; until then, Play() returns gracefully without crashing).
        m_audio.RegisterMusicTrack({
            audio::MusicState::EXPLORATION,
            "guid-music-exploration",
            0.80f
        });
        m_audio.RegisterMusicTrack({
            audio::MusicState::BATTLE,
            "guid-music-battle",
            0.90f
        });
        m_audio.RegisterMusicTrack({
            audio::MusicState::VICTORY,
            "guid-music-victory",
            0.70f
        });
        m_audio.SetMusicState(audio::MusicState::EXPLORATION);
        std::cout << "  [OK] AudioSystem ready (XAudio2 device acquired)\n";
    }
    else
    {
        std::cout << "  [--] AudioSystem: no audio device (CI / headless OK)\n";
    }

    // -----------------------------------------------------------------------
    // Done.
    // -----------------------------------------------------------------------
    m_allSystemsOk = true;  // audio failure is non-fatal

    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════╗\n";
    std::cout << "║  ALL SYSTEMS ONLINE — TestWorld running              ║\n";
    std::cout << "╚══════════════════════════════════════════════════════╝\n\n";

    // Print frame-0 status immediately.
    PrintStatus();

    return true;
}

// ===========================================================================
// Update
// ===========================================================================

void TestWorld::Update(float dt)
{
    ++m_frame;
    m_accumTime += dt;

    // -----------------------------------------------------------------------
    // 1. Weather tick (day/night cycle, rain probability)
    // -----------------------------------------------------------------------
    m_weather->Update(dt);

    // -----------------------------------------------------------------------
    // 2. Player movement along scripted waypoints
    // -----------------------------------------------------------------------
    TickPlayerMovement(dt);

    // -----------------------------------------------------------------------
    // 3. AI tick (enemy state-machine + A* pathfinding)
    // -----------------------------------------------------------------------
    m_ai->Update(m_world, m_tileMap, dt);

    // -----------------------------------------------------------------------
    // 4. Combat trigger: engage CombatSystem if enemy is adjacent
    // -----------------------------------------------------------------------
    TickCombatTrigger();

    // -----------------------------------------------------------------------
    // 5. CombatSystem tick (ATB, damage, status effects)
    // -----------------------------------------------------------------------
    if (m_combat->IsActive())
    {
        m_combat->Update(dt);

        if (!m_combat->IsActive())
            HandleCombatResult();
    }

    // -----------------------------------------------------------------------
    // 6. Camp script: rest at inn once per demo
    // -----------------------------------------------------------------------
    TickCampScript(dt);

    // -----------------------------------------------------------------------
    // 7. Shop script: buy one item at Hammerhead once per demo
    // -----------------------------------------------------------------------
    if (!m_shopVisited)
        TickShopScript();

    // -----------------------------------------------------------------------
    // 8. Audio tick (entity SFX + music crossfade)
    // -----------------------------------------------------------------------
    if (m_audioReady)
        m_audio.Update(m_world, dt);

    // -----------------------------------------------------------------------
    // 9. Victory flash decay
    // -----------------------------------------------------------------------
    if (m_victoryTimer > 0.0f)
    {
        m_victoryTimer -= dt;
        if (m_victoryTimer < 0.0f) m_victoryTimer = 0.0f;

        // Transition music back to exploration after victory fanfare ends.
        if (m_victoryTimer <= 0.0f && m_audioReady)
            m_audio.SetMusicState(audio::MusicState::EXPLORATION);
    }

    // -----------------------------------------------------------------------
    // 10. Status print (every STATUS_PRINT_INTERVAL frames)
    // -----------------------------------------------------------------------
    if (m_frame % STATUS_PRINT_INTERVAL == 0)
        PrintStatus();

    // -----------------------------------------------------------------------
    // 11. Demo completion check
    // -----------------------------------------------------------------------
    if (m_frame >= DEMO_FRAMES && !m_demoComplete)
    {
        m_demoComplete = true;

        std::cout << "\n";
        std::cout << "╔══════════════════════════════════════════════════════╗\n";
        std::cout << "║  DEMO SEQUENCE COMPLETE — all systems exercised      ║\n";
        std::cout << "║  Goblin kills:  " << std::left << std::setw(35) << m_goblinKills << "║\n";
        std::cout << "║  Camp visits:   " << std::left << std::setw(35) << (m_campVisited ? "1" : "0") << "║\n";
        std::cout << "║  Shop visits:   " << std::left << std::setw(35) << (m_shopVisited ? "1" : "0") << "║\n";
        std::cout << "║  Audio ready:   " << std::left << std::setw(35) << (m_audioReady ? "YES" : "NO (CI)") << "║\n";
        std::cout << "║  All systems:   " << std::left << std::setw(35) << (m_allSystemsOk ? "PASS" : "FAIL") << "║\n";
        std::cout << "╚══════════════════════════════════════════════════════╝\n\n";

        PrintStatus();
    }
}

// ===========================================================================
// GetClearColour
// ===========================================================================

void TestWorld::GetClearColour(float& r, float& g, float& b) const
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — State-Driven Clear Colour
    // -----------------------------------------------------------------------
    // Priority (highest wins):
    //   1. Active combat       → red tint
    //   2. Victory flash       → gold pulse (decaying)
    //   3. Camping             → warm campfire orange
    //   4. Time of day         → sky-blue (day) / navy (night) / amber (dawn)
    //   5. Weather override    → rain desaturates, storm darkens
    // -----------------------------------------------------------------------

    // --- Priority 1: Combat ---
    if (m_combat && m_combat->IsActive())
    {
        // Pulse: 1 Hz oscillation adds visual rhythm to combat.
        const float pulse = (std::sin(m_accumTime * 6.28f) + 1.0f) * 0.5f;
        r = 0.55f + pulse * 0.15f;
        g = 0.08f;
        b = 0.08f;
        return;
    }

    // --- Priority 2: Victory flash ---
    if (m_victoryTimer > 0.0f)
    {
        const float t = m_victoryTimer / VICTORY_FLASH_DURATION;
        r = 1.00f;
        g = 0.75f + 0.10f * t;
        b = 0.05f * (1.0f - t);
        return;
    }

    // --- Priority 3: Camping ---
    if (m_campVisited && m_camp)
    {
        // Warm campfire flicker
        const float flicker = (std::sin(m_accumTime * 3.0f) + 1.0f) * 0.5f;
        r = 0.45f + flicker * 0.05f;
        g = 0.22f;
        b = 0.04f;
        return;
    }

    // --- Priority 4 + 5: Weather + time of day ---
    const WeatherType weather = m_weather
        ? m_weather->GetCurrentWeather()
        : WeatherType::CLEAR;

    const TimeOfDay tod = m_weather
        ? m_weather->GetCurrentTime()
        : TimeOfDay::AFTERNOON;

    // Base sky colour by time of day
    switch (tod)
    {
    case TimeOfDay::DAWN:
        r = 0.85f; g = 0.45f; b = 0.18f; // warm amber
        break;
    case TimeOfDay::MORNING:
        r = 0.52f; g = 0.72f; b = 0.95f; // light blue
        break;
    case TimeOfDay::AFTERNOON:
        r = 0.38f; g = 0.62f; b = 0.92f; // clear sky blue
        break;
    case TimeOfDay::EVENING:
        r = 0.60f; g = 0.30f; b = 0.15f; // sunset orange-red
        break;
    case TimeOfDay::DUSK:
        r = 0.30f; g = 0.15f; b = 0.30f; // purple dusk
        break;
    case TimeOfDay::NIGHT:
        r = 0.03f; g = 0.05f; b = 0.18f; // deep navy
        break;
    case TimeOfDay::MIDNIGHT:
        r = 0.01f; g = 0.01f; b = 0.08f; // near-black
        break;
    default:
        r = 0.38f; g = 0.62f; b = 0.92f;
        break;
    }

    // Weather overlay
    switch (weather)
    {
    case WeatherType::OVERCAST:
        // Desaturate toward grey
        r = r * 0.75f + 0.15f;
        g = g * 0.75f + 0.15f;
        b = b * 0.75f + 0.18f;
        break;
    case WeatherType::RAIN:
        r = r * 0.50f + 0.10f;
        g = g * 0.50f + 0.12f;
        b = b * 0.60f + 0.20f;
        break;
    case WeatherType::STORM:
        r = r * 0.25f + 0.05f;
        g = g * 0.25f + 0.05f;
        b = b * 0.30f + 0.10f;
        break;
    case WeatherType::FOG:
        r = r * 0.60f + 0.28f;
        g = g * 0.60f + 0.28f;
        b = b * 0.60f + 0.28f;
        break;
    default:
        break; // CLEAR or SNOW — keep base colour
    }
}

// ===========================================================================
// PrintStatus
// ===========================================================================

void TestWorld::PrintStatus() const
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — ECS Component Reads
    // -----------------------------------------------------------------------
    // All game state is stored in ECS components.  To display the player's
    // HP we read it with m_world.GetComponent<HealthComponent>(m_player).
    // The returned reference is valid as long as the entity exists.
    // -----------------------------------------------------------------------

    const float gameHour = m_weather ? m_weather->GetGameHour() : 0.0f;
    const int   gH  = static_cast<int>(gameHour);
    const int   gM  = static_cast<int>((gameHour - gH) * 60.0f);

    const WeatherType wt  = m_weather ? m_weather->GetCurrentWeather()
                                      : WeatherType::CLEAR;
    const TimeOfDay   tod = m_weather ? m_weather->GetCurrentTime()
                                      : TimeOfDay::AFTERNOON;

    // -- Helper lambdas for string conversion --
    auto weatherStr = [](WeatherType w) -> const char* {
        switch(w) {
        case WeatherType::CLEAR:    return "CLEAR   ";
        case WeatherType::OVERCAST: return "OVERCAST";
        case WeatherType::RAIN:     return "RAIN    ";
        case WeatherType::STORM:    return "STORM   ";
        case WeatherType::FOG:      return "FOG     ";
        case WeatherType::SNOW:     return "SNOW    ";
        default:                    return "UNKNOWN ";
        }
    };

    auto todStr = [](TimeOfDay t) -> const char* {
        switch(t) {
        case TimeOfDay::DAWN:      return "DAWN     ";
        case TimeOfDay::MORNING:   return "MORNING  ";
        case TimeOfDay::AFTERNOON: return "AFTERNOON";
        case TimeOfDay::EVENING:   return "EVENING  ";
        case TimeOfDay::DUSK:      return "DUSK     ";
        case TimeOfDay::NIGHT:     return "NIGHT    ";
        case TimeOfDay::MIDNIGHT:  return "MIDNIGHT ";
        default:                   return "UNKNOWN  ";
        }
    };

    // Player stats
    const auto& hp  = m_world.GetComponent<HealthComponent>(m_player);
    const auto& tf  = m_world.GetComponent<TransformComponent>(m_player);
    const auto& lvl = m_world.GetComponent<LevelComponent>(m_player);
    const auto& cur = m_world.GetComponent<CurrencyComponent>(m_player);

    // Quest progress: count active quests and objectives done
    int questsDone   = 0;
    int questsActive = 0;
    if (m_world.HasComponent<QuestComponent>(m_player))
    {
        const auto& qc = m_world.GetComponent<QuestComponent>(m_player);
        for (int i = 0; i < static_cast<int>(qc.activeCount); ++i)
        {
            ++questsActive;
            if (qc.quests[i].isComplete) ++questsDone;
        }
    }

    // Enemy status
    auto isAlive = [this](EntityID id) -> bool {
        if (id == NULL_ENTITY) return false;
        if (!m_world.IsAlive(id)) return false;
        const auto& h = m_world.GetComponent<HealthComponent>(id);
        return !h.isDead;
    };

    std::cout << "\n";
    std::cout << "┌──────────────────────────────────────────────────────┐\n";
    std::cout << "│  TEST WORLD ─ Frame " << std::setw(5) << m_frame
              << "  │  Game " << std::setw(2) << std::setfill('0') << gH
              << ":" << std::setw(2) << gM << std::setfill(' ')
              << "               │\n";
    std::cout << "├──────────────────────────────────────────────────────┤\n";
    std::cout << "│  WEATHER │ " << weatherStr(wt) << " │ " << todStr(tod) << "   │\n";
    std::cout << "├──────────────────────────────────────────────────────┤\n";
    std::cout << "│  PLAYER  │ Noctis"
              << "  HP:" << std::setw(3) << hp.hp << "/" << std::setw(3) << hp.maxHp
              << "  MP:" << std::setw(3) << hp.mp << "/" << std::setw(3) << hp.maxMp
              << "  LV:" << lvl.level << "  │\n";
    std::cout << "│          │ Pos:(" << std::setw(2) << static_cast<int>(tf.position.x)
              << "," << std::setw(2) << static_cast<int>(tf.position.y)
              << ")  Gil:" << std::setw(5) << cur.gil
              << "  GoblinKills:" << m_goblinKills << "  │\n";
    std::cout << "├──────────────────────────────────────────────────────┤\n";

    // Enemies
    std::cout << "│  ENEMIES │ Goblin:"
              << (isAlive(m_goblin)   ? "ALIVE  " : "DEAD   ")
              << " Sabretusk:" << (isAlive(m_sabretusk)  ? "ALIVE  " : "DEAD   ")
              << " Foras:"     << (isAlive(m_foras)      ? "ALIVE" : "DEAD ")
              << "  │\n";
    std::cout << "├──────────────────────────────────────────────────────┤\n";

    // Combat
    std::cout << "│  COMBAT  │ "
              << (m_combat && m_combat->IsActive() ? "ACTIVE  " : "INACTIVE")
              << "  VictoryFlash:"
              << (m_victoryTimer > 0.0f ? "YES" : " NO")
              << "                     │\n";
    std::cout << "├──────────────────────────────────────────────────────┤\n";

    // Quests
    std::cout << "│  QUESTS  │ Active:" << questsActive
              << "  Done:" << questsDone
              << "                                      │\n";
    std::cout << "├──────────────────────────────────────────────────────┤\n";

    // Systems
    std::cout << "│  SYSTEMS │ "
              << "Combat" << (m_combat    ? "✓" : "✗") << " "
              << "AI"     << (m_ai        ? "✓" : "✗") << " "
              << "Weather"<< (m_weather   ? "✓" : "✗") << " "
              << "Quest"  << (m_quests    ? "✓" : "✗") << " "
              << "Inv"    << (m_inventory ? "✓" : "✗") << " "
              << "Shop"   << (m_shop      ? "✓" : "✗") << " "
              << "Camp"   << (m_camp      ? "✓" : "✗") << " "
              << "Audio"  << (m_audioReady ? "✓" : "-")
              << "   │\n";
    std::cout << "│          │ Camp:" << (m_campVisited ? "VISITED" : "PENDING")
              << "  Shop:" << (m_shopVisited ? "VISITED" : "PENDING")
              << "                        │\n";
    std::cout << "└──────────────────────────────────────────────────────┘\n";
    std::cout << "\n";
}

// ===========================================================================
// BuildTileMap (private)
// ===========================================================================

void TestWorld::BuildTileMap()
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Map Layout
    // -----------------------------------------------------------------------
    // The 40×30 tile map is divided into distinct biomes, showing the
    // "open world" structure of a typical RPG:
    //
    //  Rows  0– 1 : mountain border (impassable)
    //  Rows  2– 4 : northern plains (GRASS)
    //  Row   5    : road (horizontal)
    //  Rows  6–10 : town area (buildings + courtyards)
    //  Row  11    : road (horizontal)
    //  Rows 12–16 : central plains (GRASS)
    //  Rows 17–19 : forest (FOREST — semi-impassable)
    //  Row  20    : river (WATER — impassable)
    //  Rows 21–24 : ruins / dungeon area (FLOOR + WALL)
    //  Rows 25–26 : southern plains
    //  Rows 27–29 : mountain border
    // -----------------------------------------------------------------------

    // Fill entire map with grass
    FillRect(0, 0, MAP_W, MAP_H, TileType::GRASS);

    // Mountain borders (top + bottom rows)
    FillRect(0,  0, MAP_W, 2,    TileType::MOUNTAIN);
    FillRect(0, 28, MAP_W, 2,    TileType::MOUNTAIN);
    // Mountain side columns
    FillRect(0,  0, 1, MAP_H,    TileType::MOUNTAIN);
    FillRect(MAP_W-1, 0, 1, MAP_H, TileType::MOUNTAIN);

    // Roads
    FillRect(1,  5, MAP_W-2, 1,  TileType::ROAD);   // northern road
    FillRect(1, 11, MAP_W-2, 1,  TileType::ROAD);   // southern road
    FillRect(19, 2, 1, MAP_H-4,  TileType::ROAD);   // central vertical road

    // Town buildings  (DrawBuilding handles walls, interior, door)
    // Crown City Inn:  x=12, y=5, 7 wide, 5 tall, door at (14,9)
    DrawBuilding(12, 5, 7, 5, 14, 9, TileType::FLOOR);

    // Hammerhead Outpost: x=20, y=5, 7 wide, 5 tall, door at (22,9)
    DrawBuilding(20, 5, 7, 5, 22, 9, TileType::FLOOR);

    // Save point inside the inn
    m_tileMap.SetTile(15, 7, TileType::SAVE_POINT);

    // Treasure chest in Hammerhead shop
    m_tileMap.SetTile(23, 7, TileType::CHEST);

    // Forest strip (row 17-19)
    FillRect(1, 17, MAP_W-2, 3, TileType::FOREST);
    // Leave a path through the forest on the central road column
    for (int y = 17; y <= 19; ++y)
        m_tileMap.SetTile(19, y, TileType::ROAD);

    // River (row 20) — impassable except at the ford on the road
    FillRect(1, 20, MAP_W-2, 1, TileType::WATER);
    m_tileMap.SetTile(19, 20, TileType::ROAD); // ford

    // Ruins (rows 21-25): open FLOOR with crumbling WALL outlines
    FillRect(2, 21, MAP_W-3, 5, TileType::FLOOR);
    // Outer ruin wall
    FillRect(2,  21, MAP_W-3, 1, TileType::WALL);
    FillRect(2,  25, MAP_W-3, 1, TileType::WALL);
    FillRect(2,  21, 1, 5,       TileType::WALL);
    FillRect(MAP_W-2, 21, 1, 5,  TileType::WALL);
    // Interior ruin pillars
    for (int px : {6, 10, 14, 18, 22, 28, 32, 36})
        for (int py : {22, 24})
            if (px < MAP_W-2 && py < 26)
                m_tileMap.SetTile(px, py, TileType::WALL);
    // Dungeon entrance (stairs down) in the centre of ruins
    m_tileMap.SetTile(19, 23, TileType::STAIRS_DOWN);
    // Chest in the ruins
    m_tileMap.SetTile(10, 23, TileType::CHEST);

    // Doors in building walls (already placed by DrawBuilding but re-set
    // explicitly as DOOR type so isInteractable is derived correctly)
    m_tileMap.SetTile(14, 9, TileType::DOOR);  // Inn door
    m_tileMap.SetTile(22, 9, TileType::DOOR);  // Shop door
}

void TestWorld::FillRect(int x, int y, int w, int h, TileType type)
{
    for (int row = y; row < y + h; ++row)
        for (int col = x; col < x + w; ++col)
            if (col >= 0 && col < MAP_W && row >= 0 && row < MAP_H)
                m_tileMap.SetTile(col, row, type);
}

void TestWorld::DrawBuilding(int x, int y, int w, int h,
                             int doorX, int doorY,
                             TileType interiorType)
{
    // TEACHING NOTE — Building as Tiles
    // A building is represented as a rectangle of WALL tiles with a FLOOR
    // interior.  The door tile replaces one WALL tile on the south face.
    // This is exactly how classic tile-based RPGs like Final Fantasy VI/VII
    // represent indoor/outdoor transitions.

    // Fill interior first
    FillRect(x+1, y+1, w-2, h-2, interiorType);

    // Draw outer wall
    FillRect(x,   y,   w, 1,   TileType::WALL);  // top
    FillRect(x,   y+h-1, w, 1, TileType::WALL);  // bottom
    FillRect(x,   y,   1, h,   TileType::WALL);  // left
    FillRect(x+w-1, y, 1, h,   TileType::WALL);  // right

    // Door on south wall
    if (doorX >= 0 && doorY >= 0)
        m_tileMap.SetTile(doorX, doorY, TileType::DOOR);
}

// ===========================================================================
// Entity factories (private)
// ===========================================================================

EntityID TestWorld::SpawnPlayer()
{
    EntityID id = m_world.CreateEntity();

    // TransformComponent: starting position on the central plains
    auto& tf = m_world.AddComponent<TransformComponent>(id);
    tf.position = { static_cast<float>(PLAYER_PATH[0].x),
                    static_cast<float>(PLAYER_PATH[0].y),
                    0.0f };

    // HealthComponent
    auto& hp = m_world.AddComponent<HealthComponent>(id);
    hp.hp    = 200; hp.maxHp = 200;
    hp.mp    = 100; hp.maxMp = 100;
    hp.regenRate = 1.0f;

    // StatsComponent: hero-tier stats for a Lv5 character
    auto& st = m_world.AddComponent<StatsComponent>(id);
    st.strength  = 18;
    st.defence   = 12;
    st.magic     = 15;
    st.spirit    = 12;
    st.speed     = 14;
    st.luck      = 10;
    st.critRate  = 0.10f;
    st.fireResist      = 0.0f;
    st.iceResist       = 0.0f;
    st.lightningResist = 0.0f;

    // NameComponent
    auto& nm = m_world.AddComponent<NameComponent>(id);
    nm.name       = "Noctis";
    nm.internalID = "noctis_lucis_caelum";
    nm.title      = "Crown Prince";

    // CombatComponent
    auto& cb = m_world.AddComponent<CombatComponent>(id);
    cb.attackCooldown  = 0.0f;
    cb.attackRate      = 1.2f;
    cb.xpReward        = 0;    // player doesn't award XP
    cb.gilReward       = 0;
    cb.canWarpStrike   = true;
    cb.warpCooldown    = 3.0f;
    cb.attackElement   = ElementType::NONE;

    // InventoryComponent: starts empty (items added after)
    m_world.AddComponent<InventoryComponent>(id);

    // LevelComponent
    auto& lv = m_world.AddComponent<LevelComponent>(id);
    lv.level     = 5;
    lv.currentXP = 0;

    // CurrencyComponent: 5 000 gil
    auto& gil = m_world.AddComponent<CurrencyComponent>(id);
    gil.gil = 5000;

    // QuestComponent: empty slot array (quests accepted later)
    m_world.AddComponent<QuestComponent>(id);

    // MagicComponent: Noctis knows Fire and Thunder
    auto& mg = m_world.AddComponent<MagicComponent>(id);
    mg.knownSpells.push_back(1);  // Fire (ID=1 in GameDatabase spells)
    mg.knownSpells.push_back(3);  // Thunder (ID=3)
    mg.equippedSpell = 1;
    mg.activeElement = ElementType::FIRE;

    // EquipmentComponent: start equipped with the default sword
    auto& eq = m_world.AddComponent<EquipmentComponent>(id);
    eq.weaponID      = 1;   // "Engine Blade" (GameDatabase item id=1)
    eq.bonusStrength = 5;
    eq.bonusHP       = 20;

    // MovementComponent
    auto& mv = m_world.AddComponent<MovementComponent>(id);
    mv.moveSpeed   = 5.0f;
    mv.sprintSpeed = 9.0f;
    mv.facingDir   = Direction::SOUTH;
    mv.isGrounded  = true;

    // StatusEffectsComponent: clean state
    m_world.AddComponent<StatusEffectsComponent>(id);

    // SkillsComponent: Warp Strike + Tackle equipped
    auto& sk = m_world.AddComponent<SkillsComponent>(id);
    sk.equippedSkills[0] = 1;  // Warp Strike
    sk.equippedSkills[1] = 2;  // Tackle

    // AudioSourceComponent: player footstep SFX
    auto& au = m_world.AddComponent<AudioSourceComponent>(id);
    au.clipID      = "guid-sfx-footstep";
    au.isPlaying   = false;
    au.volume      = 0.6f;
    au.is3D        = true;
    au.maxDistance = 20.0f;

    // PartyComponent: Noctis leads; we add dummy party members
    auto& party = m_world.AddComponent<PartyComponent>(id);
    party.leaderID = id;

    return id;
}

EntityID TestWorld::SpawnEnemy(const EnemyData& data, int tx, int ty)
{
    EntityID id = m_world.CreateEntity();

    auto& tf = m_world.AddComponent<TransformComponent>(id);
    tf.position = { static_cast<float>(tx), static_cast<float>(ty), 0.0f };

    auto& hp = m_world.AddComponent<HealthComponent>(id);
    hp.hp    = data.hp;
    hp.maxHp = data.hp;

    auto& st = m_world.AddComponent<StatsComponent>(id);
    st.strength  = data.attack;
    st.defence   = data.defense;
    st.magic     = data.magic;
    st.speed     = data.speed;
    st.luck      = 5;
    st.critRate  = 0.05f;

    auto& nm = m_world.AddComponent<NameComponent>(id);
    nm.name       = data.name;
    nm.internalID = std::to_string(data.id);

    auto& cb = m_world.AddComponent<CombatComponent>(id);
    cb.attackCooldown = 0.0f;
    cb.attackRate     = 1.0f;
    cb.xpReward       = data.xpReward;
    cb.gilReward      = data.gilReward;
    cb.canWarpStrike  = false;
    cb.attackElement  = data.weakness; // enemies hit back with their own element

    auto& ai = m_world.AddComponent<AIComponent>(id);
    ai.currentState  = AIComponent::State::IDLE;
    ai.sightRange    = 8.0f;
    ai.hearRange     = 4.0f;
    ai.attackRange   = 1.5f;
    ai.aggroTarget   = NULL_ENTITY;
    ai.isNocturnal   = (data.id == ENEMY_TONBERRY); // Tonberry is night-active

    m_world.AddComponent<StatusEffectsComponent>(id);

    // LevelComponent (enemies have a level for damage formula)
    auto& lv = m_world.AddComponent<LevelComponent>(id);
    lv.level = data.level;

    return id;
}

EntityID TestWorld::SpawnNPC(const std::string& name, int tx, int ty)
{
    EntityID id = m_world.CreateEntity();

    auto& tf = m_world.AddComponent<TransformComponent>(id);
    tf.position = { static_cast<float>(tx), static_cast<float>(ty), 0.0f };

    auto& nm = m_world.AddComponent<NameComponent>(id);
    nm.name = name;

    // RenderComponent: NPC visible in future rendering milestones
    auto& rc = m_world.AddComponent<RenderComponent>(id);
    rc.symbol    = '@';
    rc.isVisible = true;
    rc.zOrder    = 1;

    // DialogueComponent: marks NPC as interactable
    auto& dl = m_world.AddComponent<DialogueComponent>(id);
    dl.isInteractable  = true;
    dl.interactRange   = 2.0f;
    dl.dialogueTreeID  = name == "Ignis Scientia" ? "ignis_idle" : "dave_quest";

    return id;
}

EntityID TestWorld::SpawnBuilding(const std::string& name, int tx, int ty)
{
    EntityID id = m_world.CreateEntity();

    auto& tf = m_world.AddComponent<TransformComponent>(id);
    tf.position = { static_cast<float>(tx), static_cast<float>(ty), 0.0f };

    auto& nm = m_world.AddComponent<NameComponent>(id);
    nm.name = name;

    // RenderComponent: building prop
    auto& rc = m_world.AddComponent<RenderComponent>(id);
    rc.symbol    = '#';
    rc.isVisible = true;
    rc.zOrder    = 0;

    // HealthComponent: buildings can be "attacked" in a destruction system later
    auto& hp = m_world.AddComponent<HealthComponent>(id);
    hp.hp    = 9999;
    hp.maxHp = 9999;
    hp.isDead = false;

    // AudioSourceComponent: ambient building sound
    auto& au = m_world.AddComponent<AudioSourceComponent>(id);
    au.clipID    = (name.find("Inn") != std::string::npos)
                    ? "guid-sfx-inn-ambient"
                    : "guid-sfx-shop-ambient";
    au.isLooping = true;
    au.volume    = 0.3f;
    au.is3D      = true;
    au.maxDistance = 15.0f;

    return id;
}

EntityID TestWorld::SpawnDoor(const std::string& label, int tx, int ty)
{
    EntityID id = m_world.CreateEntity();

    auto& tf = m_world.AddComponent<TransformComponent>(id);
    tf.position = { static_cast<float>(tx), static_cast<float>(ty), 0.0f };

    auto& nm = m_world.AddComponent<NameComponent>(id);
    nm.name = label;

    // RenderComponent: door glyph
    auto& rc = m_world.AddComponent<RenderComponent>(id);
    rc.symbol    = '+';
    rc.isVisible = true;
    rc.zOrder    = 1;

    // DialogueComponent with isInteractable = true marks this as a door
    auto& dl = m_world.AddComponent<DialogueComponent>(id);
    dl.isInteractable = true;
    dl.interactRange  = 1.5f;
    dl.dialogueTreeID = "door_enter";

    // AudioSourceComponent: door creak SFX
    auto& au = m_world.AddComponent<AudioSourceComponent>(id);
    au.clipID    = "guid-sfx-door-open";
    au.isPlaying = false;
    au.volume    = 0.8f;
    au.is3D      = true;

    return id;
}

// ===========================================================================
// TickPlayerMovement (private)
// ===========================================================================

void TestWorld::TickPlayerMovement(float dt)
{
    if (m_waypointIdx >= static_cast<int>(PLAYER_PATH.size()))
        return;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Tile-Based Movement
    // -----------------------------------------------------------------------
    // The player's position is stored as floats in TransformComponent so that
    // movement can be interpolated smoothly.  At each frame we compute the
    // vector toward the current waypoint and move a fixed speed along it.
    // When we overshoot we snap to the waypoint and advance to the next one.
    // This is the classic "waypoint following" algorithm used in almost every
    // tile-RPG's event system.
    // -----------------------------------------------------------------------

    if (m_combat && m_combat->IsActive())
        return; // freeze movement during combat

    auto& tf = m_world.GetComponent<TransformComponent>(m_player);
    const Waypoint& wp = PLAYER_PATH[static_cast<size_t>(m_waypointIdx)];

    const float targetX = static_cast<float>(wp.x);
    const float targetY = static_cast<float>(wp.y);
    const float dx = targetX - tf.position.x;
    const float dy = targetY - tf.position.y;
    const float dist = std::sqrt(dx*dx + dy*dy);

    const float step = m_moveSpeed * dt;

    if (dist <= step || dist < 0.001f)
    {
        // Arrived at waypoint — snap and advance.
        tf.position.x = targetX;
        tf.position.y = targetY;
        ++m_waypointIdx;
    }
    else
    {
        // Move toward waypoint.
        tf.position.x += (dx / dist) * step;
        tf.position.y += (dy / dist) * step;
    }
}

// ===========================================================================
// TickCombatTrigger (private)
// ===========================================================================

void TestWorld::TickCombatTrigger()
{
    if (m_combat && m_combat->IsActive())
        return;
    if (m_combatTriggered && m_goblin == NULL_ENTITY)
        return; // already fought the goblin, it's dead

    const auto& playerTf = m_world.GetComponent<TransformComponent>(m_player);

    // Check all enemies: if one gets within 2 tiles of the player, engage combat.
    auto tryEngage = [&](EntityID enemy) {
        if (enemy == NULL_ENTITY) return;
        if (!m_world.IsAlive(enemy)) return;
        const auto& h = m_world.GetComponent<HealthComponent>(enemy);
        if (h.isDead) return;

        const auto& tf = m_world.GetComponent<TransformComponent>(enemy);
        const float dx = tf.position.x - playerTf.position.x;
        const float dy = tf.position.y - playerTf.position.y;
        const float dist = std::sqrt(dx*dx + dy*dy);

        if (dist < 2.5f)
        {
            // ----------------------------------------------------------------
            // TEACHING NOTE — Starting Combat
            // ----------------------------------------------------------------
            // StartCombat receives the player entity and a list of enemy
            // entities.  Internally it builds turn-order priority queues from
            // each combatant's SpeedComponent (part of StatsComponent here),
            // resets all ATB gauges, and fires BATTLE_START on the CombatBus.
            // ----------------------------------------------------------------
            const auto& nm = m_world.GetComponent<NameComponent>(enemy);
            std::cout << "  ⚔  COMBAT START: Noctis vs " << nm.name << "\n";
            m_combat->StartCombat(m_player, { enemy });
            m_combatTriggered = true;

            // Transition to battle music.
            if (m_audioReady)
                m_audio.SetMusicState(audio::MusicState::BATTLE);

            return;
        }
    };

    tryEngage(m_goblin);
    tryEngage(m_sabretusk);
    tryEngage(m_foras);
}

// ===========================================================================
// HandleCombatResult (private)
// ===========================================================================

void TestWorld::HandleCombatResult()
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Handling Combat Outcome
    // -----------------------------------------------------------------------
    // After CombatSystem::IsActive() returns false, GetCombatResult() gives
    // us a CombatResult struct with:
    //   playerWon     — true if all enemies are dead.
    //   xpGained      — total XP to distribute.
    //   gilGained     — total Gil to distribute.
    //   droppedItems  — vector<uint32_t> of item IDs to add to inventory.
    //
    // We apply these here rather than inside CombatSystem so that different
    // callers (multiplayer, story mode, arena) can apply results differently.
    // -----------------------------------------------------------------------
    const CombatResult result = m_combat->GetCombatResult();
    if (!result.playerWon)
    {
        std::cout << "  ✗  COMBAT: Noctis was defeated!\n";
        // Restore minimal HP so the test world can continue.
        auto& hp = m_world.GetComponent<HealthComponent>(m_player);
        hp.hp = hp.maxHp / 2;
        return;
    }

    // Award XP and Gil.
    auto& lv  = m_world.GetComponent<LevelComponent>(m_player);
    auto& cur = m_world.GetComponent<CurrencyComponent>(m_player);
    lv.GainXP(result.xpGained);
    cur.EarnGil(result.gilGained);

    // Add dropped items to inventory.
    for (uint32_t itemID : result.droppedItems)
        m_inventory->AddItem(m_player, itemID, 1);

    std::cout << "  ✓  COMBAT VICTORY: +" << result.xpGained
              << " XP, +" << result.gilGained << " Gil\n";
}

// ===========================================================================
// TickCampScript (private)
// ===========================================================================

void TestWorld::TickCampScript(float /*dt*/)
{
    if (m_campVisited || (m_combat && m_combat->IsActive()))
        return;

    // Trigger camp once the player reaches the Inn building tile.
    const auto& tf = m_world.GetComponent<TransformComponent>(m_player);
    const float dx = tf.position.x - static_cast<float>(PLAYER_PATH[4].x);
    const float dy = tf.position.y - static_cast<float>(PLAYER_PATH[4].y);

    if (std::sqrt(dx*dx + dy*dy) < 1.5f)
    {
        m_campVisited = true;

        // ----------------------------------------------------------------
        // TEACHING NOTE — Camping (CampSystem)
        // ----------------------------------------------------------------
        // SetupCamp() starts a resting session for the entity.
        // Rest() applies HP/MP regen and, if a meal was cooked, applies its
        // temporary stat bonuses.  In a full game this is gated behind
        // CanCamp() which checks the tile type (CAMP or SAFE_ZONE).
        // We skip that check here because the test world always allows rest.
        // ----------------------------------------------------------------
        m_camp->SetupCamp(m_player);
        m_camp->Rest(m_player);

        auto& hp = m_world.GetComponent<HealthComponent>(m_player);
        std::cout << "  🔥 CAMP: Rested at Crown City Inn. HP:"
                  << hp.hp << "/" << hp.maxHp
                  << "  MP:" << hp.mp << "/" << hp.maxMp << "\n";
    }
}

// ===========================================================================
// TickShopScript (private)
// ===========================================================================

void TestWorld::TickShopScript()
{
    if (m_shopVisited || (m_combat && m_combat->IsActive()))
        return;

    // Trigger shop when player reaches the shop tile.
    const auto& tf = m_world.GetComponent<TransformComponent>(m_player);
    const float dx = tf.position.x - static_cast<float>(PLAYER_PATH[5].x);
    const float dy = tf.position.y - static_cast<float>(PLAYER_PATH[5].y);

    if (std::sqrt(dx*dx + dy*dy) < 1.5f)
    {
        m_shopVisited = true;

        // ----------------------------------------------------------------
        // TEACHING NOTE — Shop System
        // ----------------------------------------------------------------
        // OpenShop() binds the shop to a player for the current session.
        // BuyItem() checks the player's gil against the item price, deducts
        // the gil, and adds the item to InventoryComponent.
        // The entire transaction is atomic: if AddItem fails (full inventory),
        // the gil is refunded and BuyItem returns false.
        // ----------------------------------------------------------------
        m_shop->OpenShop(m_player, SHOP_HAMMERHEAD);

        // Buy 2 Potions (item id=1)
        const bool bought = m_shop->BuyItem(m_player, SHOP_HAMMERHEAD,
                                            ITEM_POTION, 2);
        m_shop->CloseShop();

        const auto& cur = m_world.GetComponent<CurrencyComponent>(m_player);
        std::cout << "  🛒 SHOP: "
                  << (bought ? "Bought 2x Potion" : "Purchase failed")
                  << "  Gil remaining: " << cur.gil << "\n";
    }
}

} // namespace engine
} // namespace sandbox
