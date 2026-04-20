/**
 * @file GameStreamingManager.cpp
 * @brief GameStreamingManager implementation — Zone lifecycle wiring.
 *
 * ============================================================================
 * TEACHING NOTE — M7 Full Streaming Pipeline
 * ============================================================================
 * This file brings together every subsystem introduced in M7:
 *
 *   WorldStreamingManager (engine layer)
 *     Provides: Init, Update, AsyncLoader, WorldPartition, state machine.
 *
 *   Zone (game layer)
 *     Provides: Load, SpawnEnemies, SpawnNPCs, Unload.
 *
 *   AssetLoader (engine layer)
 *     Provides: LoadRaw(guid) → raw bytes from a cooked .level file.
 *
 *   CellData (engine/world)
 *     Provides: Data-transfer-object between worker-thread parsing and
 *     main-thread spawning.
 *
 * The full streaming pipeline (per cell, one load cycle):
 *
 *   Main thread              Worker thread
 *   ─────────────────        ──────────────────────────────────────────────
 *   Update() enqueues        OnLoadCell():
 *   LoadJob for cellId   →     1. Look up GUID in m_cellGuids
 *                               2. If loader: LoadRaw(guid) → bytes
 *                               3. Parse JSON → CellData (ENGINE_ENABLE_JSON)
 *                               4. Store in m_pendingData (under m_pendingMtx)
 *                               5. Return true
 *                          ↓
 *   PumpCompletions()    ←   Worker posts CompletedJob to m_completed
 *   fires OnCellLoaded():
 *     1. Retrieve CellData from m_pendingData
 *     2. Build ZoneData from CellData
 *     3. Construct Zone, call Zone::Load(zoneData)
 *     4. Zone::SpawnEnemies(*m_world)
 *     5. Zone::SpawnNPCs(*m_world)
 *     6. Call base class OnCellLoaded → marks cell as LOADED
 *
 *   EvictCells() fires OnEvictCell():
 *     1. Zone::Unload(*m_world) → destroys all tracked ECS entities
 *     2. Erase zone + zoneData from maps
 *
 * ============================================================================
 */

#include "game/world/GameStreamingManager.hpp"
#include "engine/core/Logger.hpp"     // LOG_INFO, LOG_WARN, LOG_ERROR

#ifdef ENGINE_ENABLE_JSON
#include <nlohmann/json.hpp>          // JSON deserialisation for .level files
#endif

#include <utility>   // std::move

// ===========================================================================
// Init
// ===========================================================================

bool GameStreamingManager::Init(World&                        world,
                                 engine::assets::AssetLoader*  assetLoader,
                                 float                         cellSize,
                                 int                           streamRadius)
{
    // Store game-layer references before calling base Init().
    m_assetLoader = assetLoader;

    // Base class Init stores the World* in m_world (protected member) and
    // starts the AsyncLoader worker thread.
    // TEACHING NOTE — Base class chain
    // ─────────────────────────────────
    // We pass &world as the World* parameter to WorldStreamingManager::Init().
    // The base class stores it in m_world (protected) so both OnCellLoaded()
    // and OnEvictCell() can dereference it for ECS operations.
    return WorldStreamingManager::Init(cellSize, streamRadius, &world);
}

// ===========================================================================
// Registration helpers
// ===========================================================================

void GameStreamingManager::RegisterCellGuid(uint32_t cellId, const std::string& guid)
{
    m_cellGuids[cellId] = guid;
    LOG_INFO("GameStreamingManager: registered GUID " << guid
             << " for cellId=" << cellId);
}

void GameStreamingManager::RegisterCellData(uint32_t                  cellId,
                                             engine::world::CellData   data)
{
    data.valid = true;   // Pre-registered data is always considered valid.
    m_preRegistered[cellId] = std::move(data);
    LOG_INFO("GameStreamingManager: pre-registered CellData for cellId=" << cellId);
}

// ===========================================================================
// OnLoadCell — worker thread
// ===========================================================================

bool GameStreamingManager::OnLoadCell(uint32_t id, engine::world::CellCoord coord)
{
    // TEACHING NOTE — Worker-thread responsibilities
    // ────────────────────────────────────────────────
    // This method runs on the AsyncLoader worker thread.  Allowed operations:
    //   • File I/O via AssetLoader::LoadRaw() (thread-safe).
    //   • Pure data construction (CellData, ZoneData).
    //   • Writing m_pendingData under m_pendingMtx.
    //
    // Forbidden operations (would cause data races):
    //   • Calling world.CreateEntity() / world.AddComponent<T>() [ECS is not thread-safe].
    //   • Writing to m_zones / m_zoneDataStore.

    engine::world::CellData cellData;

    // --- Check pre-registered data first ---
    // Pre-registered data takes priority over file I/O (used in tests).
    const auto preIt = m_preRegistered.find(id);
    if (preIt != m_preRegistered.end())
    {
        cellData = preIt->second;   // Copies the pre-registered data.
    }
    else
    {
        // --- Try to load from cooked .level file ---
        const auto guidIt = m_cellGuids.find(id);
        if (guidIt != m_cellGuids.end() && m_assetLoader != nullptr)
        {
            // TEACHING NOTE — M7.2: AssetLoader integration
            // ──────────────────────────────────────────────
            // LoadRaw() reads the cooked bytes synchronously on the worker
            // thread.  This is the only blocking call in the streaming path —
            // and it runs off the main thread, so it does NOT stall rendering.
            const std::vector<uint8_t> bytes =
                m_assetLoader->LoadRaw(guidIt->second);

            if (bytes.empty())
            {
                LOG_WARN("GameStreamingManager: OnLoadCell: empty data for GUID "
                         << guidIt->second << " (cellId=" << id << ").");
                // Fall through — cellData remains default (valid=false).
            }
            else
            {
#ifdef ENGINE_ENABLE_JSON
                // TEACHING NOTE — JSON parsing on the worker thread
                // ──────────────────────────────────────────────────
                // Parsing JSON is CPU-bound (no I/O).  Doing it on the worker
                // thread means the main thread never sees this CPU cost.
                // This is a key advantage of the async pipeline.
                try
                {
                    const std::string jsonStr(
                        reinterpret_cast<const char*>(bytes.data()),
                        bytes.size());
                    const nlohmann::json j = nlohmann::json::parse(jsonStr);

                    cellData.zoneName         = j.value("zoneName",         std::string("Unknown Zone"));
                    cellData.tileWidth        = j.value("tileWidth",        40u);
                    cellData.tileHeight       = j.value("tileHeight",       40u);
                    cellData.isDungeon        = j.value("isDungeon",        false);
                    cellData.dangerLevel      = j.value("dangerLevel",      1);
                    cellData.recommendedLevel = j.value("recommendedLevel", 1u);

                    if (j.contains("spawns") && j["spawns"].is_array())
                    {
                        for (const auto& s : j["spawns"])
                        {
                            engine::world::SpawnEntry se;
                            se.enemyDataID = s.value("enemyDataID", 0u);
                            se.tileX       = s.value("tileX",       0);
                            se.tileY       = s.value("tileY",       0);
                            se.respawnTime = s.value("respawnTime", 30.0f);
                            cellData.spawns.push_back(se);
                        }
                    }

                    if (j.contains("npcIDs") && j["npcIDs"].is_array())
                    {
                        for (const auto& npc : j["npcIDs"])
                            cellData.npcIDs.push_back(npc.get<uint32_t>());
                    }

                    if (j.contains("shopIDs") && j["shopIDs"].is_array())
                    {
                        for (const auto& shop : j["shopIDs"])
                            cellData.shopIDs.push_back(shop.get<uint32_t>());
                    }

                    cellData.valid = true;
                    LOG_INFO("GameStreamingManager: parsed CellData for cell "
                             << coord.cx << "," << coord.cz
                             << ": zone='" << cellData.zoneName << "'"
                             << ", spawns=" << cellData.spawns.size());
                }
                catch (const std::exception& ex)
                {
                    LOG_ERROR("GameStreamingManager: JSON parse failed for cell "
                              << coord.cx << "," << coord.cz << ": " << ex.what());
                    cellData.valid = false;
                }
#else
                // ENGINE_ENABLE_JSON not defined — cannot parse.
                // TEACHING NOTE — Graceful degradation without JSON support
                // ─────────────────────────────────────────────────────────
                // Without nlohmann-json available (e.g. building without vcpkg),
                // we create a default CellData with the zone name set from the
                // cell coordinate.  No entities will be spawned.
                cellData.zoneName = "Cell_" + std::to_string(coord.cx)
                                  + "_" + std::to_string(coord.cz);
                cellData.valid    = true;
                LOG_WARN("GameStreamingManager: ENGINE_ENABLE_JSON not defined — "
                         "skipping JSON parse for cell " << coord.cx << "," << coord.cz);
#endif
            }
        }
        else
        {
            // No GUID registered and no pre-registered data.
            // TEACHING NOTE — Procedural fallback
            // ──────────────────────────────────────
            // For cells without a cooked data file, we generate a default
            // CellData with a procedural zone name.  This allows the
            // streaming system to function (cells load and unload cleanly)
            // without requiring every cell to have an asset file.
            cellData.zoneName = "Cell_" + std::to_string(coord.cx)
                              + "_" + std::to_string(coord.cz);
            cellData.valid    = true;
        }
    }

    // --- Store in pending data (under mutex) for main thread to consume ---
    {
        std::lock_guard<std::mutex> lock(m_pendingMtx);
        m_pendingData[id] = std::move(cellData);
    }

    return true;   // LoadJob completion result.
}

// ===========================================================================
// OnCellLoaded — main thread
// ===========================================================================

void GameStreamingManager::OnCellLoaded(uint32_t                      id,
                                         engine::world::CellCoord      coord,
                                         bool                           success)
{
    // TEACHING NOTE — Main-thread spawning
    // ──────────────────────────────────────
    // This callback runs on the main thread (via PumpMainThreadCompletions).
    // It is safe to:
    //   • Call world.CreateEntity() / world.AddComponent<T>()
    //   • Modify m_zones / m_zoneDataStore (main-thread-only maps)
    //
    // Do NOT call this from the worker thread.

    if (!success)
    {
        LOG_WARN("GameStreamingManager: OnCellLoaded: cell "
                 << coord.cx << "," << coord.cz << " reported load failure.");
        // Let base class clean up the state.
        WorldStreamingManager::OnCellLoaded(id, coord, false);
        return;
    }

    // --- Retrieve pending CellData (written by OnLoadCell on worker thread) ---
    engine::world::CellData cellData;
    {
        std::lock_guard<std::mutex> lock(m_pendingMtx);
        const auto it = m_pendingData.find(id);
        if (it != m_pendingData.end())
        {
            cellData = std::move(it->second);
            m_pendingData.erase(it);
        }
    }

    if (!cellData.valid)
    {
        LOG_WARN("GameStreamingManager: OnCellLoaded: no valid CellData for cell "
                 << coord.cx << "," << coord.cz
                 << " — treating load as failed.");
        // TEACHING NOTE — Invalid payloads must not enter the success path.
        // The CellData contract says valid == false represents a failed load
        // (e.g. parse failure, empty bytes, or missing asset data).  We
        // forward failure to the base streaming manager so the cell does not
        // transition to LOADED with an empty/default ZoneData.
        WorldStreamingManager::OnCellLoaded(id, coord, false);
        return;
    }

    // --- Build ZoneData from CellData ---
    // TEACHING NOTE — ZoneData lifetime
    // ───────────────────────────────────
    // Zone stores a const ZoneData* pointer.  We MUST keep the ZoneData alive
    // for as long as the Zone is active.  m_zoneDataStore (an unordered_map)
    // guarantees that element addresses are stable — inserting new entries
    // does not invalidate existing ones.
    ZoneData& zd = m_zoneDataStore[id];
    zd.id               = m_nextZoneId++;
    zd.name             = cellData.zoneName;
    zd.description      = "Streaming cell " + std::to_string(coord.cx)
                        + "," + std::to_string(coord.cz);
    zd.recommendedLevel = cellData.recommendedLevel;
    zd.isDungeon        = cellData.isDungeon;
    zd.tileWidth        = cellData.tileWidth;
    zd.tileHeight       = cellData.tileHeight;
    zd.dangerLevel      = cellData.dangerLevel;
    zd.npcIDs           = cellData.npcIDs;
    zd.shopIDs          = cellData.shopIDs;

    // TEACHING NOTE — Explicit vs. procedural spawn points
    // ──────────────────────────────────────────────────────
    // If the .level file contains explicit SpawnEntry records with authored
    // tile positions and respawn times, we pass them to Zone via AddSpawnPoint()
    // AFTER Zone::Load() — bypassing RegisterDefaultSpawnPoints() which would
    // distribute enemies randomly across floor tiles.
    //
    // If there are no explicit spawns, we fall back to populating zd.enemyIDs
    // so that RegisterDefaultSpawnPoints() in Zone::Load() produces procedural
    // spawn points.  This dual-path strategy lets hand-authored cells override
    // layout while unregistered cells still function correctly.
    if (cellData.spawns.empty())
    {
        // No explicit spawn data — leave placement to RegisterDefaultSpawnPoints.
        // (zd.enemyIDs stays empty; no spawn points will be registered.)
    }
    // Explicit spawn positions are added after Load() (see below).

    // --- Load Zone ---
    Zone& zone = m_zones[id];
    if (!zone.Load(zd))
    {
        LOG_WARN("GameStreamingManager: Zone::Load() failed for cell "
                 << coord.cx << "," << coord.cz);
    }

    // --- M7.2: Add explicit spawn points from .level data ---
    // TEACHING NOTE — Honouring authored spawn positions
    // ────────────────────────────────────────────────────────
    // Each SpawnEntry from the cooked .level file carries a tile position
    // (tileX, tileY) and a respawn timer.  We translate it directly into a
    // SpawnPoint and hand it to Zone::AddSpawnPoint(), which inserts it into
    // Zone::m_spawnPoints.  SpawnEnemies() then places the entity at exactly
    // that tile — matching the designer's intent rather than randomising.
    for (const auto& se : cellData.spawns)
    {
        SpawnPoint sp;
        sp.enemyDataID = se.enemyDataID;
        sp.x           = se.tileX;
        sp.y           = se.tileY;
        sp.respawnTime = se.respawnTime;
        zone.AddSpawnPoint(sp);
    }

    // --- Spawn entities on the main thread (ECS-safe) ---
    if (m_world != nullptr)
    {
        zone.SpawnEnemies(*m_world);
        zone.SpawnNPCs(*m_world);

        // TEACHING NOTE — M8.7: AnimatorComponent for D3D11 skinned-mesh pass
        // ─────────────────────────────────────────────────────────────────────
        // The D3D11 skinned-mesh vertex shader (skinned_mesh.vs.hlsl) reads a
        // joint-matrix constant buffer uploaded by GpuSkinningBuffer.  Any
        // entity that should be rendered with GPU skinning MUST have an
        // AnimatorComponent so AnimationSystem can write joint matrices each
        // frame.  We add a default component here for every enemy and NPC
        // spawned by the streaming manager.
        //
        // skeletonID / currentClipID are deliberately left as placeholder
        // strings ("skel_enemy_default", "clip_idle").  When real skeleton and
        // clip assets are cooked and registered with AnimationSystem, update
        // these IDs to the corresponding asset GUIDs.  The engine will then
        // evaluate real keyframe data instead of identity matrices.
        for (const uint32_t eid : zone.GetEnemyEntities())
        {
            if (!m_world->HasComponent<AnimatorComponent>(eid))
            {
                auto& anim        = m_world->AddComponent<AnimatorComponent>(eid);
                anim.skeletonID   = "skel_enemy_default";
                anim.currentClipID = "clip_idle";
                anim.isPlaying    = true;
            }
        }
        for (const uint32_t eid : zone.GetNPCEntities())
        {
            if (!m_world->HasComponent<AnimatorComponent>(eid))
            {
                auto& anim        = m_world->AddComponent<AnimatorComponent>(eid);
                anim.skeletonID   = "skel_npc_default";
                anim.currentClipID = "clip_idle_npc";
                anim.isPlaying    = true;
            }
        }

        LOG_INFO("GameStreamingManager: cell " << coord.cx << "," << coord.cz
                 << " spawned — zone='" << zd.name << "'"
                 << ", enemies=" << zone.GetEnemyEntities().size()
                 << ", npcs=" << zone.GetNPCEntities().size()
                 << " (AnimatorComponent attached to all)");
    }
    else
    {
        LOG_WARN("GameStreamingManager: m_world is null — skipping entity spawn "
                 "for cell " << coord.cx << "," << coord.cz);
    }

    // --- Notify base class → marks cell as LOADED in state machine ---
    WorldStreamingManager::OnCellLoaded(id, coord, true);
}

// ===========================================================================
// OnEvictCell — main thread
// ===========================================================================

void GameStreamingManager::OnEvictCell(uint32_t                  id,
                                        engine::world::CellCoord  coord)
{
    // --- Unload Zone (destroys all tracked ECS entities) ---
    const auto zoneIt = m_zones.find(id);
    if (zoneIt != m_zones.end())
    {
        if (m_world != nullptr)
        {
            zoneIt->second.Unload(*m_world);
            LOG_INFO("GameStreamingManager: Zone::Unload() called for cell "
                     << coord.cx << "," << coord.cz);
        }
        m_zones.erase(zoneIt);
    }
    else
    {
        LOG_WARN("GameStreamingManager: OnEvictCell: no Zone found for cell "
                 << coord.cx << "," << coord.cz);
    }

    // Clean up stored ZoneData.
    m_zoneDataStore.erase(id);

    // TEACHING NOTE — Cleaning up staging data on evict/cancel
    // ──────────────────────────────────────────────────────────
    // If this cell was cancelled mid-load (EvictCells calls OnEvictCell for
    // LOADING cells too, since M7.3 fix), the worker may have already written
    // CellData into m_pendingData before the cancellation flag was checked.
    // OnCellLoaded will never fire for such a cell, so we clean up here to
    // prevent the staging map from growing unboundedly.
    {
        std::lock_guard<std::mutex> lock(m_pendingMtx);
        m_pendingData.erase(id);
    }

    // TEACHING NOTE — We do NOT call WorldStreamingManager::OnEvictCell().
    // The base class virtual method only logs; the actual state-machine
    // transition (erasing from m_cellStates + m_loadedCells) is done by the
    // non-virtual EvictCells() method AFTER this callback returns.
}
