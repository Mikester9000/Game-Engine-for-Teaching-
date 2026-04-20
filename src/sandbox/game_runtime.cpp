/**
 * @file game_runtime.cpp
 * @brief D3D11 game loop driver implementation — M8.1 Gameplay Integration.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2025
 */

#include "sandbox/game_runtime.hpp"
#include "game/GameData.hpp"
#include "engine/core/Logger.hpp"

#include <algorithm> // std::min, std::max, std::move
#include <iostream>  // std::cout (CI acceptance output)
#include <cmath>     // std::sin, std::cos
#include <vector>    // std::vector (combat bridging)

namespace sandbox {

// ===========================================================================
// Constructor / Destructor
// ===========================================================================

GameRuntime::GameRuntime()
    : m_saveSystem("SavedGames/")
{
    LOG_INFO("GameRuntime created (M8 Gameplay Integration)");
}

GameRuntime::~GameRuntime()
{
    Shutdown();
}

// ===========================================================================
// Init
// ===========================================================================

bool GameRuntime::Init()
{
    LOG_INFO("GameRuntime::Init — registering components and spawning scene.");

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Component registration must happen before entity creation.
    // RegisterAllComponents() sets up one typed pool per component type in the
    // World.  Attempting to AddComponent<T> without registering T first would
    // assert in debug builds.
    // -----------------------------------------------------------------------
    RegisterAllComponents(m_world);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Open-world navigation grid.
    // The terminal AISystem uses a TileMap for A* pathfinding.  For the 3D
    // open world we create a 100×100 all-floor "flat plain" so enemies can
    // navigate freely without modification to AISystem.
    //
    // 100 tiles × TILE_SIZE(64) = 6400 world units per axis.
    // The player starts at the centre tile (50,50) → position (3200, 0, 3200).
    // -----------------------------------------------------------------------
    m_tileMap.GenerateEmptyRoom(100, 100);
    LOG_INFO("GameRuntime: navigation grid 100×100 tiles (all floor).");

    // -----------------------------------------------------------------------
    // Spawn player — Noctis Lucis Caelum
    // -----------------------------------------------------------------------
    const float kCentreX = 50.0f * TILE_SIZE;  // Tile 50 → world X
    const float kCentreZ = 50.0f * TILE_SIZE;  // Tile 50 → world Z

    m_playerID = m_world.CreateCharacter("Noctis", { kCentreX, 0.0f, kCentreZ });
    if (m_playerID == NULL_ENTITY)
    {
        LOG_ERROR("GameRuntime::Init: failed to create player entity.");
        return false;
    }

    // Customise player stats.
    {
        auto& hp = m_world.GetComponent<HealthComponent>(m_playerID);
        hp.hp    = 500;
        hp.maxHp = 500;
        hp.mp    = 150;
        hp.maxMp = 150;

        auto& st = m_world.GetComponent<StatsComponent>(m_playerID);
        st.strength = 30;
        st.defence  = 15;
        st.magic    = 20;
        st.speed    = 10;

        auto& cb = m_world.GetComponent<CombatComponent>(m_playerID);
        cb.attackRate      = 1.5f;   // 1.5 hits per second at full ATB
        cb.canWarpStrike   = true;

        auto& mv = m_world.GetComponent<MovementComponent>(m_playerID);
        mv.moveSpeed   = 4.0f * TILE_SIZE;  // ~256 units/s
        mv.sprintSpeed = 8.0f * TILE_SIZE;  // ~512 units/s

        auto& lv = m_world.GetComponent<LevelComponent>(m_playerID);
        lv.level = 5;

        // Gil (currency)
        auto& cur = m_world.AddComponent<CurrencyComponent>(m_playerID);
        cur.gil = 5000;

        // Magic — equippedSpell is a uint32_t spell ID (1 = Firaga)
        auto& mg = m_world.AddComponent<MagicComponent>(m_playerID);
        mg.equippedSpell = 1;  // Firaga
        mg.knownSpells.push_back(1);

        // Quest slot
        m_world.AddComponent<QuestComponent>(m_playerID);

        // Inventory
        m_world.AddComponent<InventoryComponent>(m_playerID);

        // Currency already exists from CreateCharacter? No — we add it above.
        // Equipment
        m_world.AddComponent<EquipmentComponent>(m_playerID);
    }

    // -----------------------------------------------------------------------
    // Spawn 3 enemies at scattered positions.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Enemy spawn positions
    // Each enemy is placed at a different tile offset from the centre so they
    // begin IDLE and must wander/chase before reaching the player.  The AI
    // system will transition IDLE → WANDERING within a few seconds, providing
    // observable state changes for the m8_gameplay acceptance test.
    //
    // We use the global Vec3 (from Types.hpp) here because CreateEnemy takes
    // const Vec3& — the game-layer Vec3, not engine::math::Vec3.  Both types
    // share the same {x,y,z} layout; the distinction exists only for namespace
    // separation between the game layer and the engine math library.
    const std::pair<std::string, Vec3> kEnemySpawns[] = {
        { "Goblin",     Vec3{ kCentreX + TILE_SIZE * 10, 0.0f, kCentreZ + TILE_SIZE * 5  } },
        { "Niffin",     Vec3{ kCentreX - TILE_SIZE * 8,  0.0f, kCentreZ - TILE_SIZE * 6  } },
        { "Anak Calf",  Vec3{ kCentreX + TILE_SIZE * 5,  0.0f, kCentreZ - TILE_SIZE * 10 } },
    };

    for (const auto& [name, pos] : kEnemySpawns)
    {
        const EntityID eid = m_world.CreateEnemy(name, pos);
        if (eid == NULL_ENTITY)
        {
            LOG_ERROR("GameRuntime::Init: failed to create enemy '" << name << "'.");
            continue;
        }

        // TEACHING NOTE — AISystem compares TileDistance (distance / TILE_SIZE)
        // against these fields, so they must stay in TILE units.  Do NOT
        // multiply by TILE_SIZE here; doing so would make ranges ~64× too large
        // (enemies would aggro from across the whole map).
        auto& ai = m_world.GetComponent<AIComponent>(eid);
        ai.sightRange  = 15.0f;   // 15 tiles  ≈ 960 world units
        ai.hearRange   =  8.0f;   //  8 tiles  ≈ 512 world units
        ai.attackRange =  1.5f;   //  1.5 tiles ≈  96 world units

        // Track initial AI states for transition detection.
        m_prevAIStates.push_back({ eid, static_cast<int>(ai.currentState) });

        LOG_INFO("GameRuntime: spawned enemy '" << name << "' at ("
                 << pos.x << ", " << pos.y << ", " << pos.z << ")");
    }

    // -----------------------------------------------------------------------
    // Create gameplay systems.
    // -----------------------------------------------------------------------
    m_combat   = std::make_unique<CombatSystem>(&m_world);
    m_ai       = std::make_unique<AISystem>(&m_world, m_playerID);
    m_weather  = std::make_unique<WeatherSystem>(&EventBus<WorldEvent>::Instance());
    m_quests   = std::make_unique<QuestSystem>(
                     &m_world,
                     &EventBus<QuestEvent>::Instance(),
                     &EventBus<UIEvent>::Instance());
    m_dialogue = std::make_unique<DialogueSystem>(&m_world);

    // -----------------------------------------------------------------------
    // Accept opening quest "The Road to Dawn" (quest ID 1).
    // -----------------------------------------------------------------------
    // TEACHING NOTE — QuestSystem::AcceptQuest()
    // This wires the player into the quest system.  The QuestComponent on
    // playerID is updated with the quest entry.  Objective progress is later
    // incremented by CombatSystem when enemies are killed.
    if (m_quests->AcceptQuest(m_playerID, 1))
    {
        LOG_INFO("GameRuntime: quest 1 'The Road to Dawn' accepted.");
    }

    // -----------------------------------------------------------------------
    // Create camera entity.
    // -----------------------------------------------------------------------
    m_cameraID = m_world.CreateEntity();
    auto& cam  = m_world.AddComponent<CameraComponent>(m_cameraID);
    cam.targetEntityID = m_playerID;
    cam.fovDegrees     = 60.0f;
    cam.offset         = { 0.0f, 3.0f * TILE_SIZE, -7.0f * TILE_SIZE };
    cam.isActive       = true;

    LOG_INFO("GameRuntime::Init complete — player=" << m_playerID
             << " camera=" << m_cameraID);
    return true;
}

// ===========================================================================
// Update
// ===========================================================================

void GameRuntime::Update(float dt)
{
    ++m_frameCount;
    m_gameTime += dt;

    // TEACHING NOTE — System update order (see header for rationale).

    // 1. Input — read keyboard state and write into player components.
    m_inputMapper.Update(m_world, m_playerID, dt);

    // 2. Weather — advance day/night cycle and probabilistic weather FSM.
    m_weather->Update(dt);

    // 3. AI — enemy decision making and movement.
    m_ai->Update(m_world, m_tileMap, dt);

    // 4. Track AI state transitions (for acceptance test).
    TrackAITransitions();

    // 5. Combat bridging — start encounter when enemies aggro; route player
    //    attack/spell intent from ECS component state into CombatSystem API.
    //
    // TEACHING NOTE — CombatSystem is stateful: Update() only does meaningful
    // work after StartCombat() has been called, and player actions must be
    // forwarded via explicit CombatSystem entry points (PlayerAttack /
    // PlayerCastSpell).  GameRuntime is the integration layer that observes
    // ECS state set by InputMapper and AISystem, then bridges those observations
    // into the imperative CombatSystem API.
    if (!m_combat->IsActive())
    {
        // Start an encounter the first frame any enemy enters ATTACK state
        // (i.e. they have closed to within attackRange of the player).
        bool anyEnemyAttacking = false;
        for (const auto& [eid, prevState] : m_prevAIStates)
        {
            (void)prevState;
            if (!m_world.IsAlive(eid)) continue;
            if (!m_world.HasComponent<AIComponent>(eid)) continue;
            if (m_world.GetComponent<AIComponent>(eid).currentState
                    == AIComponent::State::ATTACK)
            {
                anyEnemyAttacking = true;
                break;
            }
        }

        if (anyEnemyAttacking)
        {
            // Collect all still-living enemy IDs for the encounter.
            std::vector<EntityID> livingEnemies;
            for (const auto& [eid, prevState] : m_prevAIStates)
            {
                (void)prevState;
                if (m_world.IsAlive(eid))
                    livingEnemies.push_back(eid);
            }
            if (!livingEnemies.empty())
                m_combat->StartCombat(m_playerID, std::move(livingEnemies));
        }
    }
    else if (m_world.HasComponent<CombatComponent>(m_playerID))
    {
        // Combat is active — forward player attack/spell intent to CombatSystem.
        auto& playerCombat = m_world.GetComponent<CombatComponent>(m_playerID);

        // Assign nearest living enemy as target if none is set.
        if (playerCombat.currentTarget == NULL_ENTITY
            || !m_world.IsAlive(playerCombat.currentTarget))
        {
            for (const auto& [eid, prevState] : m_prevAIStates)
            {
                (void)prevState;
                if (m_world.IsAlive(eid))
                {
                    playerCombat.currentTarget = eid;
                    break;
                }
            }
        }

        if (playerCombat.isInCombat
            && playerCombat.currentTarget != NULL_ENTITY
            && m_world.IsAlive(playerCombat.currentTarget))
        {
            if (m_world.HasComponent<MagicComponent>(m_playerID)
                && m_world.GetComponent<MagicComponent>(m_playerID).isCasting)
            {
                // Spell ID 1 = Firaga (the sample project's equipped spell).
                m_combat->PlayerCastSpell(1, playerCombat.currentTarget);
            }
            else
            {
                m_combat->PlayerAttack(playerCombat.currentTarget);
            }
            // Clear intent flag; CombatSystem will enforce its own cooldown.
            playerCombat.isInCombat = false;
        }
    }

    // 6. CombatSystem tick — processes ATB timers, status effects, deaths.
    m_combat->Update(dt);

    // 7. Dialogue — update NPC interaction ranges.
    m_dialogue->Update(m_world, m_playerID, dt);

    // 8. Camera — compute view/proj matrices.
    m_camera.Update(m_world, m_backBufferW, m_backBufferH);

    // 9. HUD snapshot — extract game state for the renderer overlay.
    m_lastHudState = m_hud.ReadFromWorld(m_world, m_playerID,
                                          m_frameCount, m_gameTime);

    // 10. Print debug every 60 frames (CI log output).
    m_hud.PrintDebug(m_lastHudState, 60);
}

// ===========================================================================
// Shutdown
// ===========================================================================

void GameRuntime::Shutdown()
{
    m_dialogue.reset();
    m_quests.reset();
    m_weather.reset();
    m_ai.reset();
    m_combat.reset();
    LOG_INFO("GameRuntime shut down after " << m_frameCount << " frames.");
}

// ===========================================================================
// GetClearColour
// ===========================================================================

void GameRuntime::GetClearColour(float& r, float& g, float& b) const
{
    // TEACHING NOTE — Encoding game state in the clear colour.
    // We modulate a base sky colour with gameplay conditions so the CI
    // log output and the headless D3D11 back buffer both reflect live state.

    // Base: time-of-day tint from WeatherSystem.
    // WeatherSystem::GetGameHour() returns 0–24 (noon = 12).
    // We normalize to [0,1] and drive sky colour from it.
    const float hour   = m_weather ? m_weather->GetGameHour() : 12.0f;
    const float t      = hour / 24.0f;   // 0=midnight, 0.5=noon, 1=midnight again
    const float isDawn = std::max(0.0f, 1.0f - std::abs(t - 0.25f) * 8.0f); // ~6AM
    const float isDay  = std::max(0.0f, 1.0f - std::abs(t - 0.50f) * 2.5f); // noon
    const float isDusk = std::max(0.0f, 1.0f - std::abs(t - 0.83f) * 8.0f); // ~8PM

    // Day sky colour.
    float skyR = 0.2f * isDawn + 0.35f * isDay + 0.4f * isDusk + 0.02f;
    float skyG = 0.1f * isDawn + 0.55f * isDay + 0.2f * isDusk + 0.02f;
    float skyB = 0.05f* isDawn + 0.80f * isDay + 0.3f * isDusk + 0.05f;

    // Combat pulse: add a red flash when the player is in combat.
    if (m_lastHudState.isInCombat)
    {
        const float pulse = 0.5f + 0.5f * std::sin(m_gameTime * 4.0f);
        skyR = std::min(1.0f, skyR + 0.15f * pulse);
        skyG = std::max(0.0f, skyG - 0.05f * pulse);
        skyB = std::max(0.0f, skyB - 0.05f * pulse);
    }

    r = skyR;
    g = skyG;
    b = skyB;
}

// ===========================================================================
// TrackAITransitions (private)
// ===========================================================================

void GameRuntime::TrackAITransitions()
{
    // TEACHING NOTE — Transition detection via state comparison.
    // We store each enemy's previous AI state and compare each frame.
    // A difference means a state transition occurred.

    for (auto& [eid, prevStateInt] : m_prevAIStates)
    {
        if (!m_world.IsAlive(eid)) continue;
        if (!m_world.HasComponent<AIComponent>(eid)) continue;

        const auto& ai = m_world.GetComponent<AIComponent>(eid);
        const int   curStateInt = static_cast<int>(ai.currentState);

        if (curStateInt != prevStateInt)
        {
            ++m_aiStateTransitions;
            LOG_INFO("GameRuntime: AI entity " << eid
                     << " state transition " << prevStateInt
                     << " → " << curStateInt);
            prevStateInt = curStateInt;
        }
    }
}

} // namespace sandbox
