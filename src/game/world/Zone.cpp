/**
 * @file Zone.cpp
 * @brief Implementation of the Zone class — lifecycle, spawning, and updates.
 *
 * ============================================================================
 * TEACHING NOTE — Implementation File Structure
 * ============================================================================
 *
 * This file implements the methods declared in Zone.hpp.  The organisation:
 *
 *   1. Includes
 *   2. Static member initialisation  (s_emptyString)
 *   3. Zone::Load()                  (zone setup)
 *   4. Zone::Unload()                (cleanup)
 *   5. Zone::Update()                (per-frame logic)
 *   6. Zone::SpawnEnemies()          (batch spawn)
 *   7. Zone::SpawnNPCs()             (NPC creation)
 *   8. Zone::SpawnOneEnemy()         (single-entity creation)
 *   9. Zone::AddSpawnPoint()
 *  10. Zone::GetName()
 *  11. Zone::RegisterDefaultSpawnPoints()
 *  12. Zone::FindPlayerSpawn()
 *  13. Zone::PruneDeadEntities()
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 */

// ---------------------------------------------------------------------------
// Own header (must be first to catch self-inclusion errors)
// ---------------------------------------------------------------------------
#include "Zone.hpp"

// ---------------------------------------------------------------------------
// Standard library
// ---------------------------------------------------------------------------
#include <algorithm>   // std::remove_if, std::find
#include <random>      // std::mt19937, std::random_device
#include <cassert>     // assert()
#include <stdexcept>   // std::runtime_error (not used directly, for reference)

// ---------------------------------------------------------------------------
// Engine headers
// ---------------------------------------------------------------------------
#include "../../engine/core/Logger.hpp"  // LOG_INFO, LOG_WARN, LOG_DEBUG


// ===========================================================================
// Compile-time sanity check
// ===========================================================================

// TEACHING NOTE — static_assert
// ──────────────────────────────
// `static_assert` evaluates a condition at compile time and fails the build
// with a friendly message if the condition is false.
// Here we verify that uint32_t and EntityID are the same size so storing
// EntityIDs in uint32_t vectors is safe.
static_assert(sizeof(uint32_t) == sizeof(EntityID),
    "Zone.cpp: uint32_t and EntityID must be the same size for safe casting.");


// ===========================================================================
// Static member initialisation
// ===========================================================================

// TEACHING NOTE — Static Member Variables with Non-trivial Types
// ─────────────────────────────────────────────────────────────────
// Static data members of class types (like std::string) must be defined in
// exactly ONE translation unit (.cpp file).  Writing `= ""` here provides
// both the definition and its initial value.  Without this line, the linker
// would report an "undefined reference to Zone::s_emptyString" error.
const std::string Zone::s_emptyString = "";


// ===========================================================================
// Zone::Load
// ===========================================================================

/**
 * @brief Load the zone from a ZoneData configuration record.
 *
 * TEACHING NOTE — Defensive Programming
 * ──────────────────────────────────────
 * We validate inputs at the start of Load() and return false on any failure.
 * This is "defensive programming": assume inputs might be invalid and check
 * them explicitly rather than trusting callers.
 *
 * Benefits:
 *   • Clear error messages in the log (LOG_WARN identifies the problem).
 *   • Zone remains in a valid "unloaded" state on failure.
 *   • Callers can detect failure via the return value.
 */
bool Zone::Load(const ZoneData& data) {
    // Guard: don't double-load
    if (m_isLoaded) {
        LOG_WARN("Zone::Load: zone '" << data.name << "' is already loaded");
        return false;
    }

    // Validate dimensions
    if (data.tileWidth == 0 || data.tileHeight == 0) {
        LOG_WARN("Zone::Load: ZoneData '" << data.name
                 << "' has zero dimensions (" << data.tileWidth
                 << "x" << data.tileHeight << ")");
        return false;
    }

    LOG_INFO("Zone::Load: loading zone '" << data.name
             << "' (" << data.tileWidth << "x" << data.tileHeight << ")");

    // Store a pointer to the static database record
    // TEACHING NOTE: We store a pointer rather than a copy because ZoneData
    // lives in GameDatabase (static storage) and is never destroyed.
    // Copying a ZoneData would duplicate the enemy/NPC/shop ID vectors
    // unnecessarily — the pointer gives us O(1) access at zero cost.
    m_zoneData = &data;

    // ── Build the TileMap ─────────────────────────────────────────────────
    // TEACHING NOTE: We use placement via assignment here rather than
    // constructing a new TileMap because Zone already owns m_tileMap as a
    // member.  std::move assigns a freshly-generated map in one step without
    // a temporary heap allocation.
    if (data.isDungeon) {
        // Dungeons get procedurally generated layouts
        m_tileMap = TileMap(data.tileWidth, data.tileHeight);
        m_tileMap.GenerateDungeon(); // Uses random seed
        LOG_DEBUG("Zone::Load: generated dungeon tilemap for '" << data.name << "'");
    } else {
        // Outdoor/overworld zones get a simple bordered room
        // A real engine would load a hand-authored map from a file here.
        m_tileMap = TileMap(data.tileWidth, data.tileHeight);
        m_tileMap.GenerateEmptyRoom(data.tileWidth, data.tileHeight);
        LOG_DEBUG("Zone::Load: generated empty room tilemap for '" << data.name << "'");
    }

    // ── Determine player spawn position ──────────────────────────────────
    FindPlayerSpawn();

    // ── Register spawn points for enemies ────────────────────────────────
    RegisterDefaultSpawnPoints();

    m_isLoaded = true;
    LOG_INFO("Zone::Load: zone '" << data.name << "' loaded successfully");
    return true;
}


// ===========================================================================
// Zone::Unload
// ===========================================================================

/**
 * @brief Destroy all entities spawned by this zone and reset state.
 *
 * TEACHING NOTE — Entity Cleanup Order
 * ──────────────────────────────────────
 * We destroy entities before clearing the ID lists, so we don't lose track
 * of which entities need cleanup.  The order:
 *   1. Destroy all enemy entities.
 *   2. Destroy all NPC entities.
 *   3. Destroy all chest entities.
 *   4. Clear all ID lists.
 *   5. Clear spawn points.
 *   6. Reset flags and pointers.
 *
 * Using separate loops for each category keeps the code readable and
 * makes it easy to skip cleanup of one category if needed.
 */
void Zone::Unload(World& world) {
    if (!m_isLoaded) {
        LOG_WARN("Zone::Unload: zone is not loaded");
        return;
    }

    const std::string name = m_zoneData ? m_zoneData->name : "(unnamed)";
    LOG_INFO("Zone::Unload: unloading zone '" << name << "'");

    // Destroy enemy entities
    for (uint32_t eid : m_enemyEntities) {
        if (eid != NULL_ENTITY) {
            world.DestroyEntity(static_cast<EntityID>(eid));
        }
    }
    m_enemyEntities.clear();

    // Destroy NPC entities
    for (uint32_t eid : m_npcEntities) {
        if (eid != NULL_ENTITY) {
            world.DestroyEntity(static_cast<EntityID>(eid));
        }
    }
    m_npcEntities.clear();

    // Destroy chest entities
    for (uint32_t eid : m_chestEntities) {
        if (eid != NULL_ENTITY) {
            world.DestroyEntity(static_cast<EntityID>(eid));
        }
    }
    m_chestEntities.clear();

    // Clear spawn points
    m_spawnPoints.clear();

    // Reset state
    m_zoneData      = nullptr;
    m_isLoaded      = false;
    m_updateTimer   = 0.0f;
    m_playerSpawnX  = 1;
    m_playerSpawnY  = 1;

    LOG_INFO("Zone::Unload: zone '" << name << "' unloaded");
}


// ===========================================================================
// Zone::Update
// ===========================================================================

/**
 * @brief Per-frame update: prune dead entities and tick respawn timers.
 *
 * TEACHING NOTE — Periodic vs Every-Frame Updates
 * ──────────────────────────────────────────────────
 * We use `m_updateTimer` to perform heavy operations (like scanning all
 * enemy entities for death) only every HEAVY_UPDATE_INTERVAL seconds
 * rather than every frame.
 *
 * This is a common optimisation pattern:
 *   Light operations  → every frame   (spawn timer countdown)
 *   Heavy operations  → every N seconds (entity validity scan)
 *
 * For an educational engine with small zones, even heavy operations are
 * fast.  But the pattern is worth demonstrating: at production scale with
 * 100 zones each with 50 enemies, every-frame scans add up.
 */
void Zone::Update(World& world, float dt) {
    if (!m_isLoaded) return;

    // ── Accumulate time ───────────────────────────────────────────────────
    m_updateTimer += dt;

    // TEACHING NOTE: Periodic heavy update every 1 second.
    // In production, this interval would be tuned based on profiling.
    const float HEAVY_UPDATE_INTERVAL = 1.0f;
    if (m_updateTimer >= HEAVY_UPDATE_INTERVAL) {
        m_updateTimer = 0.0f;
        PruneDeadEntities(world);
    }

    // ── Tick respawn timers ───────────────────────────────────────────────
    // TEACHING NOTE: We iterate spawn points (not enemy entities) for respawn.
    // Each SpawnPoint tracks its own respawn countdown independently.
    for (SpawnPoint& sp : m_spawnPoints) {
        if (!sp.isAlive) {
            // Enemy at this point is dead; count down to respawn
            if (sp.respawnTimer > 0.0f) {
                sp.respawnTimer -= dt;
            } else {
                // Timer expired: respawn the enemy
                uint32_t newID = SpawnOneEnemy(sp, world);
                if (newID != NULL_ENTITY) {
                    sp.entityID  = newID;
                    sp.isAlive   = true;
                    m_enemyEntities.push_back(newID);
                    LOG_DEBUG("Zone::Update: respawned enemy (dataID="
                              << sp.enemyDataID << ") at (" << sp.x
                              << ", " << sp.y << ")");
                }
            }
        } else {
            // Check if the live entity has been killed (entity no longer alive)
            if (!world.GetEntityManager().IsAlive(static_cast<EntityID>(sp.entityID))) {
                sp.isAlive      = false;
                sp.respawnTimer = sp.respawnTime;
                sp.entityID     = 0;
            }
        }
    }
}


// ===========================================================================
// Zone::SpawnEnemies
// ===========================================================================

/**
 * @brief Spawn all registered enemies.
 *
 * TEACHING NOTE — Batch Spawn vs On-Demand Spawn
 * ─────────────────────────────────────────────────
 * SpawnEnemies() spawns ALL enemies at once when the zone loads.
 * An alternative is "lazy spawn": only spawn enemies the player is near
 * (within some activation radius).  Lazy spawn reduces ECS entity count
 * but requires distance checks every frame.
 *
 * For this educational engine, batch spawn on load is simpler and the
 * entity count is small.  A production open-world game would use lazy spawn.
 */
void Zone::SpawnEnemies(World& world) {
    if (!m_isLoaded) {
        LOG_WARN("Zone::SpawnEnemies: zone not loaded");
        return;
    }

    int spawnCount = 0;
    for (SpawnPoint& sp : m_spawnPoints) {
        if (!sp.isAlive) {
            uint32_t id = SpawnOneEnemy(sp, world);
            if (id != NULL_ENTITY) {
                sp.entityID = id;
                sp.isAlive  = true;
                m_enemyEntities.push_back(id);
                ++spawnCount;
            }
        }
    }

    LOG_INFO("Zone::SpawnEnemies: spawned " << spawnCount << " enemies in zone '"
             << (m_zoneData ? m_zoneData->name : "?") << "'");
}


// ===========================================================================
// Zone::SpawnNPCs
// ===========================================================================

/**
 * @brief Spawn non-hostile NPC entities for this zone.
 *
 * TEACHING NOTE — NPC Entity Components
 * ──────────────────────────────────────
 * NPCs require fewer components than enemies:
 *   • TransformComponent  — position on the map.
 *   • NameComponent       — display name for the dialogue system.
 *   • RenderComponent     — sprite to draw.
 *   • DialogueComponent   — conversation data (tree ID).
 *
 * NPCs do NOT have AIComponent (they are not autonomous) or
 * CombatComponent (they do not fight).  This is ECS composition in action:
 * NPCs and enemies are both "entities", but they have different
 * component sets and therefore different behaviours.
 */
void Zone::SpawnNPCs(World& world) {
    if (!m_isLoaded || !m_zoneData) {
        LOG_WARN("Zone::SpawnNPCs: zone not loaded");
        return;
    }

    // TEACHING NOTE: npcIDs in ZoneData are NPC "data IDs" from a
    // (hypothetical) NpcDatabase.  For this engine, each ID becomes one
    // NPC entity placed at a fixed position in the zone.
    // We distribute them evenly across the top half of passable floor tiles.

    const uint32_t mapW = m_tileMap.GetWidth();
    const uint32_t mapH = m_tileMap.GetHeight();

    int npcIndex = 0;
    for (uint32_t npcDataID : m_zoneData->npcIDs) {
        // Find a placement tile — simple: walk right along row 2 from left
        // TEACHING NOTE: A real engine loads NPC positions from a data file.
        int nx = 2 + npcIndex * 4;
        int ny = 2;

        // Clamp to map bounds and find a passable tile
        nx = std::min(nx, static_cast<int>(mapW) - 2);
        while (!m_tileMap.IsPassable(nx, ny) && ny < static_cast<int>(mapH) - 2) {
            ++ny;
        }

        // Create the NPC entity
        EntityID npcEntity = world.CreateEntity();
        if (npcEntity == NULL_ENTITY) {
            LOG_WARN("Zone::SpawnNPCs: entity pool exhausted");
            break;
        }

        // TransformComponent — world position (tile units)
        {
            TransformComponent t;
            t.position = Vec3(static_cast<float>(nx) * TILE_SIZE,
                              0.0f,
                              static_cast<float>(ny) * TILE_SIZE);
            world.AddComponent<TransformComponent>(npcEntity, t);
        }

        // NameComponent — display label
        {
            NameComponent n;
            n.displayName = "NPC_" + std::to_string(npcDataID);
            n.internalID  = "npc_" + std::to_string(npcDataID);
            world.AddComponent<NameComponent>(npcEntity, n);
        }

        // RenderComponent — ASCII icon 'N' for NPC
        {
            RenderComponent r;
            r.isVisible = true;
            r.zOrder    = 1.0f;
            world.AddComponent<RenderComponent>(npcEntity, r);
        }

        // DialogueComponent — links to conversation tree
        {
            DialogueComponent d;
            d.dialogueTreeID  = "dlg_npc_" + std::to_string(npcDataID);
            d.isInteractable  = true;
            d.interactRange   = 3.0f;
            world.AddComponent<DialogueComponent>(npcEntity, d);
        }

        m_npcEntities.push_back(static_cast<uint32_t>(npcEntity));
        ++npcIndex;

        LOG_DEBUG("Zone::SpawnNPCs: spawned NPC dataID=" << npcDataID
                  << " at (" << nx << ", " << ny << ") entity=" << npcEntity);
    }

    LOG_INFO("Zone::SpawnNPCs: spawned " << m_npcEntities.size()
             << " NPCs in zone '" << m_zoneData->name << "'");
}


// ===========================================================================
// Zone::SpawnOneEnemy  (private)
// ===========================================================================

/**
 * @brief Create one enemy entity from a SpawnPoint.
 *
 * @param sp     SpawnPoint describing the enemy type and position.
 * @param world  ECS World to create the entity in.
 * @return       The new EntityID, or NULL_ENTITY on failure.
 *
 * TEACHING NOTE — Component Assembly from Data
 * ──────────────────────────────────────────────
 * This function demonstrates how EnemyData drives component initialisation.
 * The pattern is:
 *   1. Look up the data record.
 *   2. Create a blank ECS entity.
 *   3. Build each component from the data fields.
 *   4. Attach the component to the entity.
 *
 * Every component is initialised to a known state derived from the data.
 * There are no "magic defaults" hidden deep in component structs — the intent
 * is explicit here where it can be read and understood.
 *
 * TEACHING NOTE — ECS World Registration
 * ────────────────────────────────────────
 * `World::AddComponent<T>()` will assert if T has not been registered via
 * `World::RegisterComponent<T>()`.  Registration is performed once at
 * engine startup.  If you encounter a registration assert here, check that
 * all component types used below are registered in the engine init code.
 */
uint32_t Zone::SpawnOneEnemy(SpawnPoint& sp, World& world) {
    // Look up the enemy template
    const EnemyData* enemyData = GameDatabase::FindEnemy(sp.enemyDataID);
    if (!enemyData) {
        LOG_WARN("Zone::SpawnOneEnemy: no EnemyData found for id=" << sp.enemyDataID);
        return NULL_ENTITY;
    }

    // Validate spawn position on the tilemap
    if (!m_tileMap.IsPassable(sp.x, sp.y)) {
        LOG_WARN("Zone::SpawnOneEnemy: spawn position (" << sp.x << ", " << sp.y
                 << ") is not passable for enemy '" << enemyData->name << "'");
        return NULL_ENTITY;
    }

    // Create the entity
    EntityID entity = world.CreateEntity();
    if (entity == NULL_ENTITY) {
        LOG_WARN("Zone::SpawnOneEnemy: entity pool exhausted");
        return NULL_ENTITY;
    }

    // ── TransformComponent ────────────────────────────────────────────────
    // Convert tile coordinates to world-space position.
    // TEACHING NOTE: Multiplying by TILE_SIZE converts tile-grid units
    // (integer tile index) to continuous world-space units (float metres/pixels).
    {
        TransformComponent t;
        t.position = Vec3(static_cast<float>(sp.x) * TILE_SIZE,
                          0.0f,
                          static_cast<float>(sp.y) * TILE_SIZE);
        world.AddComponent<TransformComponent>(entity, t);
    }

    // ── HealthComponent ───────────────────────────────────────────────────
    // Both currentHP and maxHP start at the enemy's base HP value.
    // The combat system clamps currentHP to [0, maxHP] on damage.
    {
        HealthComponent h;
        h.currentHP = enemyData->hp;
        h.maxHP     = enemyData->hp;
        h.currentMP = enemyData->magic * 5; // Give enemies some MP for magic
        h.maxMP     = enemyData->magic * 5;
        h.regenRate = 0.0f; // Enemies don't passively regen HP
        world.AddComponent<HealthComponent>(entity, h);
    }

    // ── StatsComponent ────────────────────────────────────────────────────
    // Copy stats from EnemyData.  The spirit/agility/luck fields use the
    // enemy's magic and speed values as proxies.
    {
        StatsComponent s;
        s.strength  = enemyData->attack;
        s.defence   = enemyData->defense;
        s.magic     = enemyData->magic;
        s.spirit    = enemyData->defense;      // Spirit ≈ magic defence
        s.agility   = enemyData->speed;
        s.vitality  = enemyData->defense;      // Vitality drives HP scaling
        s.luck      = static_cast<int32_t>(enemyData->level); // Luck ≈ level

        // Set elemental resistances/weaknesses
        // TEACHING NOTE: A 150% value means the enemy takes 50% MORE damage
        // from that element (weakness).  50% means it takes half damage (resist).
        if (enemyData->weakness != ElementType::NONE) {
            switch (enemyData->weakness) {
                case ElementType::FIRE:      s.fireResist      = 150; break;
                case ElementType::ICE:       s.iceResist       = 150; break;
                case ElementType::LIGHTNING: s.lightningResist = 150; break;
                default: break;
            }
        }
        if (enemyData->resistance != ElementType::NONE) {
            switch (enemyData->resistance) {
                case ElementType::FIRE:      s.fireResist      = 50; break;
                case ElementType::ICE:       s.iceResist       = 50; break;
                case ElementType::LIGHTNING: s.lightningResist = 50; break;
                default: break;
            }
        }
        world.AddComponent<StatsComponent>(entity, s);
    }

    // ── NameComponent ─────────────────────────────────────────────────────
    {
        NameComponent n;
        n.displayName = enemyData->name;
        n.internalID  = "enemy_" + std::to_string(enemyData->id);
        world.AddComponent<NameComponent>(entity, n);
    }

    // ── RenderComponent ───────────────────────────────────────────────────
    // We use the enemy's iconChar (mapped to a sprite in a real renderer).
    {
        RenderComponent r;
        r.isVisible = true;
        r.zOrder    = 2.0f; // Enemies drawn above floor tiles
        r.tint      = Color::Red(); // Enemies have a red tint by default
        world.AddComponent<RenderComponent>(entity, r);
    }

    // ── MovementComponent ─────────────────────────────────────────────────
    {
        MovementComponent m;
        m.moveSpeed = static_cast<float>(enemyData->speed) * 0.5f;
        world.AddComponent<MovementComponent>(entity, m);
    }

    // ── CombatComponent ───────────────────────────────────────────────────
    {
        CombatComponent c;
        c.attackRate  = 1.0f;
        c.attackRange = 2;
        c.combatXP    = static_cast<int32_t>(enemyData->xpReward);
        // Boss enemies can warp-strike (advanced AI attack)
        c.canWarpStrike = enemyData->isBoss;
        c.attackElement = ElementType::NONE; // Default physical attack
        world.AddComponent<CombatComponent>(entity, c);
    }

    // ── AIComponent ───────────────────────────────────────────────────────
    // TEACHING NOTE: The AIComponent stores the AI's current state machine
    // state and configuration parameters.  The AI System reads this each
    // frame and transitions states based on player distance and combat logic.
    {
        AIComponent ai;
        ai.currentState = AIComponent::State::PATROL;

        // Scale sight range with enemy speed (faster enemies detect from further)
        ai.sightRange = 6.0f + static_cast<float>(enemyData->speed) * 0.3f;
        ai.hearRange  = 3.0f;
        ai.attackRange = 2.0f;
        ai.fleeHPPct   = enemyData->isBoss ? 0.0f : 0.15f; // Bosses never flee

        // Bosses are always active, nocturnal enemies get an activity window
        ai.isNocturnal = false;

        world.AddComponent<AIComponent>(entity, ai);
    }

    // ── StatusEffectsComponent ────────────────────────────────────────────
    // Start with no status effects active
    {
        StatusEffectsComponent se;
        world.AddComponent<StatusEffectsComponent>(entity, se);
    }

    // ── LevelComponent ────────────────────────────────────────────────────
    {
        LevelComponent lc;
        lc.level = enemyData->level;
        world.AddComponent<LevelComponent>(entity, lc);
    }

    LOG_DEBUG("Zone::SpawnOneEnemy: spawned '" << enemyData->name
              << "' (level " << enemyData->level << ") at ("
              << sp.x << ", " << sp.y << ") entity=" << entity);

    return static_cast<uint32_t>(entity);
}


// ===========================================================================
// Zone::AddSpawnPoint
// ===========================================================================

void Zone::AddSpawnPoint(const SpawnPoint& sp) {
    m_spawnPoints.push_back(sp);
    LOG_DEBUG("Zone::AddSpawnPoint: added spawn for enemyDataID=" << sp.enemyDataID
              << " at (" << sp.x << ", " << sp.y << ")");
}


// ===========================================================================
// Zone::GetName
// ===========================================================================

const std::string& Zone::GetName() const {
    return m_zoneData ? m_zoneData->name : s_emptyString;
}


// ===========================================================================
// Zone::RegisterDefaultSpawnPoints  (private)
// ===========================================================================

/**
 * @brief Populate m_spawnPoints from the ZoneData's enemyIDs list.
 *
 * TEACHING NOTE — Spawn Point Distribution Strategy
 * ──────────────────────────────────────────────────
 * We use a simple grid-walk approach to place spawn points on passable tiles:
 *   • Divide the map into a grid of cells (spawnCellW × spawnCellH).
 *   • Place one spawn per enemy ID, cycling through cells.
 *   • Within each cell, pick the first passable non-special tile.
 *
 * This produces a reasonably spread-out distribution without requiring
 * designer input.  A production game would load spawn positions from a
 * data file instead.
 *
 * Special tiles (STAIRS, CHEST, SAVE_POINT) are excluded from spawn
 * positions to avoid blocking critical interactions.
 */
void Zone::RegisterDefaultSpawnPoints() {
    if (!m_zoneData) return;

    m_spawnPoints.clear();

    const uint32_t mapW = m_tileMap.GetWidth();
    const uint32_t mapH = m_tileMap.GetHeight();

    if (mapW < 4 || mapH < 4) return; // Too small to place anything

    // Number of enemy types in this zone
    size_t numEnemyTypes = m_zoneData->enemyIDs.size();
    if (numEnemyTypes == 0) return;

    // How many spawn points per type?
    // TEACHING NOTE: We spawn 2 of each enemy type by default.
    // Boss zones should spawn 1 boss and leave the rest empty.
    const int SPAWNS_PER_TYPE = 2;

    // Build a list of candidate passable positions
    std::vector<std::pair<int,int>> candidates;
    candidates.reserve(mapW * mapH / 4);

    for (uint32_t y = 2; y < mapH - 2; ++y) {
        for (uint32_t x = 2; x < mapW - 2; ++x) {
            int ix = static_cast<int>(x);
            int iy = static_cast<int>(y);
            const Tile& t = m_tileMap.GetTile(ix, iy);

            // Only use plain floor tiles for spawning
            if (t.isPassable && t.type == TileType::FLOOR) {
                // Exclude player spawn area (a small bubble around the spawn point)
                int dx = ix - m_playerSpawnX;
                int dy = iy - m_playerSpawnY;
                if (dx * dx + dy * dy > 16) { // At least 4 tiles away from player
                    candidates.push_back({ix, iy});
                }
            }
        }
    }

    if (candidates.empty()) {
        LOG_WARN("Zone::RegisterDefaultSpawnPoints: no valid floor tiles found for spawning");
        return;
    }

    // Distribute spawn points using a seeded RNG based on zone ID
    // TEACHING NOTE: Using the zone ID as a seed ensures the same zone always
    // has the same enemy distribution — reproducible without saving spawn data.
    std::mt19937 rng(m_zoneData ? m_zoneData->id * 12345u + 67890u : 42u);
    std::shuffle(candidates.begin(), candidates.end(), rng);

    size_t candidateIdx = 0;
    for (size_t typeIdx = 0; typeIdx < numEnemyTypes; ++typeIdx) {
        uint32_t enemyID = m_zoneData->enemyIDs[typeIdx];

        // Check if this is a boss — bosses get only one spawn point
        const EnemyData* eData = GameDatabase::FindEnemy(enemyID);
        int spawnsForThis = (eData && eData->isBoss) ? 1 : SPAWNS_PER_TYPE;

        for (int s = 0; s < spawnsForThis; ++s) {
            if (candidateIdx >= candidates.size()) {
                candidateIdx = 0; // Wrap around if we run out of positions
            }

            auto [cx, cy] = candidates[candidateIdx++];

            SpawnPoint sp;
            sp.x            = cx;
            sp.y            = cy;
            sp.enemyDataID  = enemyID;
            sp.respawnTime  = (eData && eData->isBoss) ? 9999.0f : 30.0f;
            sp.respawnTimer = 0.0f;
            sp.isAlive      = false;
            sp.entityID     = 0;

            m_spawnPoints.push_back(sp);
        }
    }

    LOG_DEBUG("Zone::RegisterDefaultSpawnPoints: registered "
              << m_spawnPoints.size() << " spawn points in zone '"
              << (m_zoneData ? m_zoneData->name : "?") << "'");
}


// ===========================================================================
// Zone::FindPlayerSpawn  (private)
// ===========================================================================

/**
 * @brief Find a suitable player spawn position in the loaded tilemap.
 *
 * TEACHING NOTE — Tile Searching Strategy
 * ──────────────────────────────────────────
 * We search for tile types in priority order:
 *   Priority 1: STAIRS_UP  (conventional entrance marker in dungeons)
 *   Priority 2: First passable FLOOR tile in the interior (safe fallback)
 *
 * The search scans row by row from top-left, which biases the spawn
 * toward the top-left of the map.  For outdoor zones this is fine;
 * for dungeons the STAIRS_UP tile is usually near the centre of the
 * first generated room.
 */
void Zone::FindPlayerSpawn() {
    const uint32_t mapW = m_tileMap.GetWidth();
    const uint32_t mapH = m_tileMap.GetHeight();

    // Pass 1: look for STAIRS_UP
    for (uint32_t y = 0; y < mapH; ++y) {
        for (uint32_t x = 0; x < mapW; ++x) {
            if (m_tileMap.GetTile(static_cast<int>(x), static_cast<int>(y)).type
                    == TileType::STAIRS_UP) {
                m_playerSpawnX = static_cast<int>(x);
                m_playerSpawnY = static_cast<int>(y);
                LOG_DEBUG("Zone::FindPlayerSpawn: found STAIRS_UP at ("
                          << x << ", " << y << ")");
                return;
            }
        }
    }

    // Pass 2: use the first interior passable floor tile
    for (uint32_t y = 1; y < mapH - 1; ++y) {
        for (uint32_t x = 1; x < mapW - 1; ++x) {
            int ix = static_cast<int>(x);
            int iy = static_cast<int>(y);
            if (m_tileMap.IsPassable(ix, iy)) {
                m_playerSpawnX = ix;
                m_playerSpawnY = iy;
                LOG_DEBUG("Zone::FindPlayerSpawn: using first passable tile at ("
                          << ix << ", " << iy << ")");
                return;
            }
        }
    }

    // Last resort: corner position (may be a wall, but avoids crashing)
    m_playerSpawnX = 1;
    m_playerSpawnY = 1;
    LOG_WARN("Zone::FindPlayerSpawn: no passable tile found — using (1, 1)");
}


// ===========================================================================
// Zone::PruneDeadEntities  (private)
// ===========================================================================

/**
 * @brief Remove EntityIDs from tracking lists whose entities are no longer alive.
 *
 * TEACHING NOTE — The Erase-Remove Idiom
 * ────────────────────────────────────────
 * In C++, removing elements from a std::vector while iterating is tricky.
 * The standard solution is the "erase-remove idiom":
 *
 *   v.erase(std::remove_if(v.begin(), v.end(), predicate), v.end());
 *
 * std::remove_if(begin, end, pred):
 *   Moves all elements for which pred returns TRUE to the END of the range
 *   and returns an iterator to the first "removed" element.
 *   (It does NOT actually remove them from the vector.)
 *
 * v.erase(first_removed, v.end()):
 *   Actually erases the "removed" elements.
 *
 * Combined, these two calls efficiently remove all matching elements in
 * O(n) time.
 *
 * WHY NOT ERASE INSIDE THE LOOP?
 *   Calling v.erase(it) inside a range-for loop invalidates 'it', causing
 *   undefined behaviour.  The erase-remove idiom avoids this completely.
 *
 * C++20 provides `std::erase_if(v, pred)` as a cleaner alternative.
 * We show the C++17 idiom here for compatibility.
 */
void Zone::PruneDeadEntities(World& world) {
    auto& em = world.GetEntityManager();

    // Prune dead enemies from tracking list
    m_enemyEntities.erase(
        std::remove_if(m_enemyEntities.begin(), m_enemyEntities.end(),
            [&em](uint32_t eid) {
                return !em.IsAlive(static_cast<EntityID>(eid));
            }),
        m_enemyEntities.end()
    );

    // Prune dead NPCs (NPCs rarely die, but guard just in case)
    m_npcEntities.erase(
        std::remove_if(m_npcEntities.begin(), m_npcEntities.end(),
            [&em](uint32_t eid) {
                return !em.IsAlive(static_cast<EntityID>(eid));
            }),
        m_npcEntities.end()
    );
}
