/**
 * @file Zone.hpp
 * @brief Declares the Zone class — one explorable area of the game world.
 *
 * ============================================================================
 * TEACHING NOTE — Zone Architecture
 * ============================================================================
 *
 * A "Zone" (also called "Area", "Level", or "Scene" in different engines)
 * is the fundamental unit of game world organisation.  Everything the player
 * can explore at one time lives in one Zone:
 *   • The tilemap (geometry of walls, floors, doors, etc.).
 *   • All spawned enemy entities, NPC entities, and interactive objects.
 *   • Spawn rules defining where and what to spawn.
 *   • A reference to the static ZoneData configuration (from GameData.hpp).
 *
 * ZONE LIFECYCLE
 * ──────────────
 * 1. Load(ZoneData&):  Called when the player enters this zone.
 *    - Builds or loads the TileMap.
 *    - Registers spawn points for enemies and NPCs.
 *    - Does NOT spawn entities yet (that happens in SpawnEnemies/SpawnNPCs).
 *
 * 2. SpawnEnemies(World&):  Creates ECS entities for each spawn point.
 *    - Reads EnemyData from GameDatabase to fill components.
 *    - Attaches: TransformComponent, HealthComponent, StatsComponent,
 *      NameComponent, RenderComponent, AIComponent, CombatComponent.
 *
 * 3. SpawnNPCs(World&):  Creates ECS entities for non-hostile NPCs.
 *    - Attaches: TransformComponent, NameComponent, RenderComponent,
 *      DialogueComponent.
 *
 * 4. Update(World&, float dt):  Called every game frame while the zone
 *    is active.
 *    - Checks if respawn timers have elapsed.
 *    - Removes EntityIDs of dead/destroyed enemies from the tracking lists.
 *    - Can trigger ambient events (random encounters, day/night spawns).
 *
 * 5. Unload(World&):  Called when the player leaves this zone.
 *    - Destroys all enemy and NPC entities in the ECS.
 *    - Clears spawn point lists.
 *    - TileMap memory is released when the Zone object goes out of scope.
 *
 * OWNERSHIP MODEL
 * ───────────────
 * • Zone OWNS the TileMap (member variable).
 * • Zone OWNS the lists of EntityIDs it spawned.
 * • Zone does NOT own the ECS World — it is passed by reference.
 * • Zone holds a NON-OWNING pointer (const ZoneData*) to the static
 *   database record in GameDatabase.  The GameDatabase is never destroyed
 *   during gameplay, so the pointer is always valid.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 *
 * C++ Standard: C++17
 */

#pragma once

// ---------------------------------------------------------------------------
// Engine includes (path relative to this file's location in src/game/world/)
// ---------------------------------------------------------------------------
#include "TileMap.hpp"                    // TileMap, TileType, Tile
#include "../GameData.hpp"                // ZoneData, EnemyData, GameDatabase
#include "../../engine/ecs/ECS.hpp"       // World, EntityID, all components

// ---------------------------------------------------------------------------
// Standard library
// ---------------------------------------------------------------------------
#include <vector>   // std::vector — entity and spawn point lists
#include <list>     // std::list   — spawn point list (efficient insert/erase)
#include <cstdint>  // uint32_t, int32_t


// ===========================================================================
// SpawnPoint
// ===========================================================================

/**
 * @struct SpawnPoint
 * @brief Describes where and what to spawn when a zone is loaded.
 *
 * TEACHING NOTE — Data vs Runtime State
 * ──────────────────────────────────────
 * SpawnPoint is a lightweight *specification*: it says "spawn an enemy of
 * type enemyDataID at tile (x, y)".  It does NOT hold a live EntityID.
 *
 * Once SpawnEnemies() runs, each SpawnPoint is used to create an ECS entity.
 * The created entity's ID is stored separately in m_enemyEntities.
 *
 * Separating the specification (SpawnPoint) from the live instance (EntityID)
 * allows respawning: when an enemy dies, we don't need to remember where
 * it was; the SpawnPoint still knows.
 */
struct SpawnPoint {
    int      x           = 0; ///< Tile X coordinate for the spawn location.
    int      y           = 0; ///< Tile Y coordinate for the spawn location.
    uint32_t enemyDataID = 0; ///< EnemyData.id from GameDatabase to spawn here.
    float    respawnTime = 30.0f; ///< Seconds after death before the enemy respawns.
    float    respawnTimer= 0.0f;  ///< Countdown timer (0 = ready to spawn).
    bool     isAlive     = false; ///< True while the spawned entity is alive.
    uint32_t entityID    = 0;     ///< Live ECS EntityID (0 if not yet spawned).
};


// ===========================================================================
// Zone Class
// ===========================================================================

/**
 * @class Zone
 * @brief One explorable area of the game world.
 *
 * TEACHING NOTE — Single Responsibility
 * ──────────────────────────────────────
 * Zone has one responsibility: manage the state of a single explorable area.
 * It does NOT:
 *   • Handle player input (that is the GameStateManager's job).
 *   • Render the map (that is the RenderSystem's job).
 *   • Implement combat logic (that is the CombatSystem's job).
 *
 * Zone *does*:
 *   • Own the TileMap geometry.
 *   • Track which entities it spawned (so it can clean them up on Unload).
 *   • Drive the spawn/respawn cycle for its enemies and NPCs.
 *
 * This single-responsibility design makes Zone easy to test in isolation:
 * you can call Load(), SpawnEnemies(), Unload() in a unit test without
 * needing a renderer or input system.
 */
class Zone {
public:
    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /**
     * @brief Default constructor — zone starts in an unloaded state.
     *
     * Call Load() before using any other methods.
     */
    Zone() = default;

    /**
     * @brief Destructor — does NOT automatically unload entities from the ECS.
     *
     * TEACHING NOTE — Explicit Unload
     * ────────────────────────────────
     * We do NOT automatically call Unload(world) in the destructor because
     * the destructor doesn't have access to the World reference.
     *
     * Rule of thumb: whenever a class manages external state (here, ECS entities),
     * provide an explicit cleanup method and call it before the object is destroyed.
     * Relying on destructors for state cleanup across system boundaries leads
     * to hard-to-trace bugs.
     */
    ~Zone() = default;

    // Moveable but not copyable (each Zone owns unique ECS entities).
    Zone(Zone&&) noexcept            = default;
    Zone& operator=(Zone&&) noexcept = default;
    Zone(const Zone&)                = delete;
    Zone& operator=(const Zone&)     = delete;


    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Load this zone from a ZoneData configuration record.
     *
     * Builds (or generates) the TileMap and registers spawn points.
     * Does NOT spawn any ECS entities — call SpawnEnemies() and SpawnNPCs()
     * after this method returns.
     *
     * @param data  Read-only reference to the zone's configuration.
     * @return      true on success; false if the zone could not be loaded
     *              (e.g. invalid dimensions in ZoneData).
     *
     * TEACHING NOTE — Separate Load from Construct
     * ─────────────────────────────────────────────
     * We do not do loading in the constructor because:
     *   1. Constructors cannot return error codes.
     *   2. Loading may fail (corrupt data, missing resources) — a failed
     *      load leaves the object in a valid "unloaded" state, which is
     *      better than a half-constructed object.
     *   3. Zones may be pre-allocated and loaded lazily (just-in-time when
     *      the player approaches).
     */
    bool Load(const ZoneData& data);

    /**
     * @brief Destroy all entities this zone created and reset its state.
     *
     * @param world  The ECS World that owns the entities.
     *
     * After this call, IsLoaded() returns false and the Zone can be safely
     * destroyed or reloaded with a different ZoneData.
     *
     * TEACHING NOTE — Cleanup Responsibility
     * ────────────────────────────────────────
     * Zone spawned these entities, so Zone is responsible for destroying them.
     * This is the "creator owns" principle: whoever allocates/creates a resource
     * is responsible for freeing/destroying it.
     */
    void Unload(World& world);

    /**
     * @brief Per-frame update for the zone.
     *
     * @param world  The ECS World (for querying entity health / destroy state).
     * @param dt     Delta time in seconds since the last frame.
     *
     * Handles:
     *   • Removing dead enemy EntityIDs from m_enemyEntities.
     *   • Ticking respawn timers on SpawnPoints.
     *   • Re-spawning enemies whose respawn timer has elapsed.
     *
     * TEACHING NOTE — Frame Update vs Event-Driven
     * ──────────────────────────────────────────────
     * Respawn timers are updated per-frame (polling), not via events.
     * For a large number of zones loaded simultaneously, an event-driven
     * approach (enemy death fires an event, zone subscribes) would be more
     * efficient.  For a single-zone game like this, polling is simpler.
     */
    void Update(World& world, float dt);

    /**
     * @brief Spawn enemy entities from the registered spawn points.
     *
     * @param world  The ECS World to create entities in.
     *
     * For each SpawnPoint in m_spawnPoints:
     *   1. Look up EnemyData from GameDatabase.
     *   2. Create an ECS entity with World::CreateEntity().
     *   3. Attach TransformComponent, HealthComponent, StatsComponent,
     *      NameComponent, RenderComponent, AIComponent, CombatComponent.
     *   4. Store the entity ID in both the SpawnPoint and m_enemyEntities.
     *
     * TEACHING NOTE — Component Initialisation
     * ──────────────────────────────────────────
     * Components are initialised from EnemyData fields, not from hard-coded
     * defaults.  Example:
     *   HealthComponent hp;
     *   hp.hp    = enemyData->hp;
     *   hp.maxHp = enemyData->hp;
     * This ensures the enemy's stats in-game exactly match the database.
     */
    void SpawnEnemies(World& world);

    /**
     * @brief Spawn NPC entities for the zone's configured NPC list.
     *
     * @param world  The ECS World to create entities in.
     *
     * Uses the npcIDs list from the ZoneData to determine which NPCs
     * to place and uses fixed positions from the zone layout.
     *
     * TEACHING NOTE — NPC Placement
     * ──────────────────────────────
     * In this simplified engine, NPC positions are distributed across
     * passable tiles algorithmically.  A production game would have hand-
     * authored NPC positions stored in the zone asset file.
     */
    void SpawnNPCs(World& world);


    // =========================================================================
    // Accessors
    // =========================================================================

    /// Return the mutable tilemap for this zone.
    TileMap& GetTileMap()       { return m_tileMap; }

    /// Return the const tilemap for this zone.
    const TileMap& GetTileMap() const { return m_tileMap; }

    /// Return a pointer to the static ZoneData record, or nullptr if unloaded.
    const ZoneData* GetZoneData() const { return m_zoneData; }

    /**
     * @brief Return the player spawn X tile coordinate.
     *
     * The player is placed at this tile when entering the zone.
     * Typically near STAIRS_UP or the zone entrance.
     */
    int GetPlayerSpawnX() const { return m_playerSpawnX; }

    /// Return the player spawn Y tile coordinate.
    int GetPlayerSpawnY() const { return m_playerSpawnY; }

    /// True if Load() has been called successfully.
    bool IsLoaded() const { return m_isLoaded; }

    /// Return the number of live enemy entities in this zone.
    size_t GetEnemyCount() const { return m_enemyEntities.size(); }

    /// Return all enemy entity IDs spawned by this zone.
    const std::vector<uint32_t>& GetEnemyEntities() const {
        return m_enemyEntities;
    }

    /// Return all NPC entity IDs spawned by this zone.
    const std::vector<uint32_t>& GetNPCEntities() const {
        return m_npcEntities;
    }

    /// Return a reference to the spawn point list for read or modification.
    std::list<SpawnPoint>& GetSpawnPoints() { return m_spawnPoints; }

    /**
     * @brief Manually add a spawn point to the zone.
     *
     * @param sp  SpawnPoint to add.
     *
     * Useful for scripted spawns triggered by quest events.
     */
    void AddSpawnPoint(const SpawnPoint& sp);

    /**
     * @brief Return the zone display name (empty string if unloaded).
     */
    const std::string& GetName() const;


private:
    // =========================================================================
    // Private Helpers
    // =========================================================================

    /**
     * @brief Register default spawn points based on enemy IDs in ZoneData.
     *
     * Distributes spawn points pseudo-randomly across passable floor tiles.
     * Called from Load().
     */
    void RegisterDefaultSpawnPoints();

    /**
     * @brief Find a suitable passable tile to use as a player spawn point.
     *
     * Looks for a STAIRS_UP tile first; if not found, uses a random
     * interior floor tile.
     */
    void FindPlayerSpawn();

    /**
     * @brief Spawn one enemy entity from a SpawnPoint.
     *
     * @param sp     The spawn point configuration.
     * @param world  ECS World to create the entity in.
     * @return       The created EntityID (NULL_ENTITY on failure).
     */
    uint32_t SpawnOneEnemy(SpawnPoint& sp, World& world);

    /**
     * @brief Remove EntityIDs from m_enemyEntities that are no longer alive.
     *
     * TEACHING NOTE — Tombstone / Deferred Removal
     * ───────────────────────────────────────────────
     * We cannot remove dead entities inside the Update() loop while iterating
     * over m_enemyEntities (iterator invalidation).  Instead we use the
     * erase-remove idiom:
     *   entities.erase(
     *     std::remove_if(entities.begin(), entities.end(), isDead),
     *     entities.end()
     *   );
     *
     * This is O(n) in the size of the list — acceptable for a small zone.
     * A production engine might use a "graveyard" list and batch-remove.
     */
    void PruneDeadEntities(World& world);


    // =========================================================================
    // Member Variables
    // =========================================================================

    // ── Core State ────────────────────────────────────────────────────────

    TileMap           m_tileMap{0, 0};       ///< The zone's tile grid geometry.
    const ZoneData*   m_zoneData = nullptr;  ///< Non-owning pointer to static config.
    bool              m_isLoaded = false;    ///< True after a successful Load().

    // ── Entity Tracking ───────────────────────────────────────────────────

    /**
     * @brief EntityIDs of all enemies currently alive in this zone.
     *
     * TEACHING NOTE — std::vector<uint32_t> vs std::vector<EntityID>
     * ───────────────────────────────────────────────────────────────
     * We store uint32_t here rather than EntityID to avoid having to
     * include the full ECS header in Zone.hpp for just one typedef.
     * Since EntityID is `using EntityID = uint32_t;`, they are identical
     * at the binary level.  The static_assert in Zone.cpp double-checks this.
     */
    std::vector<uint32_t> m_enemyEntities;  ///< Live enemy ECS entity IDs.
    std::vector<uint32_t> m_npcEntities;    ///< Live NPC ECS entity IDs.
    std::vector<uint32_t> m_chestEntities;  ///< Chest ECS entity IDs.

    // ── Spawn System ──────────────────────────────────────────────────────

    /**
     * @brief All registered spawn points for enemy respawning.
     *
     * TEACHING NOTE — std::list for Spawn Points
     * ────────────────────────────────────────────
     * We use std::list (doubly-linked list) here because:
     *   • Spawn points can be added and removed at arbitrary positions
     *     (e.g. scripted spawns during quests) without invalidating other
     *     iterators.  std::vector would invalidate iterators on insert/erase.
     *   • We do not need random access (index-based) to spawn points.
     *   • The number of spawn points per zone is small (< 50), so cache
     *     efficiency of std::list vs std::vector is not a concern here.
     *
     * In a performance-critical system (MMO server with thousands of zones),
     * a sorted std::vector + tombstone approach would be faster.
     */
    std::list<SpawnPoint> m_spawnPoints;

    // ── Player Spawn Position ─────────────────────────────────────────────

    int m_playerSpawnX = 1; ///< Tile X where the player appears on zone enter.
    int m_playerSpawnY = 1; ///< Tile Y where the player appears on zone enter.

    // ── Timing ────────────────────────────────────────────────────────────

    float m_updateTimer = 0.0f; ///< Accumulator for periodic heavy updates.

    /// Empty string returned by GetName() when zone is unloaded.
    static const std::string s_emptyString;
};
