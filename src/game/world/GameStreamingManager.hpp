/**
 * @file GameStreamingManager.hpp
 * @brief Game-layer streaming manager — wires Zone lifecycle into the streaming system.
 *
 * ============================================================================
 * TEACHING NOTE — M7.1: Zone ↔ WorldStreamingManager Wiring
 * ============================================================================
 * WorldStreamingManager (engine layer) provides a generic streaming framework
 * with virtual hooks:
 *   • OnLoadCell()   — called on the worker thread for file I/O
 *   • OnCellLoaded() — called on the main thread after loading
 *   • OnEvictCell()  — called on the main thread to destroy a cell
 *
 * GameStreamingManager (game layer) overrides these hooks to call the real
 * Zone lifecycle methods:
 *   • OnLoadCell()   → deserialise the .level file into a CellData
 *   • OnCellLoaded() → Zone::Load() + Zone::SpawnEnemies() + Zone::SpawnNPCs()
 *   • OnEvictCell()  → Zone::Unload() (destroys all tracked ECS entities)
 *
 * This separation keeps the engine layer clean (no game-specific headers)
 * while the game layer can freely reference ECS, Zone, GameDatabase, etc.
 *
 * ─── Thread Safety ───────────────────────────────────────────────────────────
 * OnLoadCell() runs on the worker thread.  It writes the parsed CellData into
 * m_pendingData under m_pendingMtx.  OnCellLoaded() and OnEvictCell() run on
 * the main thread; they read/erase m_pendingData under m_pendingMtx.
 *
 * The Zone map (m_zones) and ZoneData store (m_zoneDataStore) are main-thread
 * only — never accessed from the worker thread.
 *
 * ─── M7.2: AssetLoader Integration ──────────────────────────────────────────
 * If an AssetLoader is provided to Init(), OnLoadCell() calls
 * AssetLoader::LoadRaw(cellGuid) to read the cooked .level file bytes and
 * parses them into a CellData using nlohmann-json (when ENGINE_ENABLE_JSON).
 *
 * Cells without a registered GUID get an empty CellData (no spawns).
 * This allows the streaming system to function in tests without a full
 * asset pipeline.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Platform: Windows (SANDBOX_SOURCES) + Linux (GAME_SOURCES)
 */

#pragma once

#include "engine/world/world_streaming.hpp"   // WorldStreamingManager (base)
#include "engine/world/cell_data.hpp"          // CellData, SpawnEntry

// Game-layer includes — world and entity systems.
#include "game/world/Zone.hpp"                 // Zone lifecycle methods
#include "game/GameData.hpp"                   // ZoneData, GameDatabase

// Optional: engine asset pipeline for reading .level files.
#include "engine/assets/asset_loader.hpp"      // AssetLoader::LoadRaw

#include <unordered_map>  // std::unordered_map
#include <mutex>          // std::mutex (guards m_pendingData)
#include <string>         // std::string
#include <cstdint>        // uint32_t

// Forward declaration — ECS World is a global-namespace class defined in ECS.hpp.
// Zone.hpp already includes ECS.hpp so it is fully available in GameStreamingManager.cpp.
// We do not need to redeclare it here.

/**
 * @class GameStreamingManager
 * @brief WorldStreamingManager subclass that integrates Zone loading into the
 *        streaming pipeline.
 *
 * Usage:
 * @code
 *   World world;
 *   RegisterAllComponents(world);
 *
 *   engine::assets::AssetDB    db;
 *   db.Load("Cooked/assetdb.json");
 *   engine::assets::AssetLoader loader(&db);
 *
 *   GameStreamingManager mgr;
 *   mgr.Init(world, &loader, 256.0f, 1);
 *
 *   // Register a GUID → cell ID mapping for real .level files:
 *   mgr.RegisterCellGuid(cellId, "b4a3c2d1-e5f6-4789-8012-a3b4c5d6e7f8");
 *
 *   // Or pre-register CellData directly (useful in tests):
 *   mgr.RegisterCellData(cellId, myCellData);
 *
 *   // Each frame:
 *   mgr.Update(cameraPos);
 * @endcode
 */
class GameStreamingManager : public engine::world::WorldStreamingManager
{
public:
    GameStreamingManager()  = default;
    ~GameStreamingManager() = default;

    GameStreamingManager(const GameStreamingManager&)            = delete;
    GameStreamingManager& operator=(const GameStreamingManager&) = delete;

    // =========================================================================
    // Initialisation
    // =========================================================================

    /**
     * @brief Initialise with an ECS World and optional AssetLoader.
     *
     * @param world         Reference to the live ECS World.  Must outlive mgr.
     * @param assetLoader   Optional.  When non-null, cells with registered GUIDs
     *                      load their .level file via AssetLoader::LoadRaw().
     *                      When null, only pre-registered CellData is used.
     * @param cellSize      World-space side length of one streaming cell.
     * @param streamRadius  Number of cells in each direction to keep loaded.
     * @return true on success.
     *
     * TEACHING NOTE — Optional AssetLoader
     * ──────────────────────────────────────
     * Accepting a nullable AssetLoader pointer allows the streaming system to
     * function in unit tests and headless CI without a full asset pipeline.
     * Tests can call RegisterCellData() to inject CellData directly, while
     * the production game path uses the AssetLoader for real file I/O.
     */
    bool Init(World&                       world,
              engine::assets::AssetLoader* assetLoader = nullptr,
              float                        cellSize     = 256.0f,
              int                          streamRadius = 1);

    // =========================================================================
    // Cell registration helpers
    // =========================================================================

    /**
     * @brief Associate a cell ID with an asset GUID for .level file loading.
     *
     * @param cellId  Cell ID from CellIdFromCoord (WorldPartition).
     * @param guid    Asset GUID registered in AssetRegistry.json and assetdb.json.
     *
     * TEACHING NOTE — Decoupled cell addressing
     * ─────────────────────────────────────────────
     * We store GUIDs separately from the WorldPartition so that cell
     * coordinates can change (e.g. the streaming radius grows) without
     * invalidating the GUID mapping.  The GUID is the stable identifier;
     * the cell ID is a derived hash used only at runtime.
     */
    void RegisterCellGuid(uint32_t cellId, const std::string& guid);

    /**
     * @brief Pre-register a CellData record for a cell (bypasses file I/O).
     *
     * Intended for tests and procedural generation.  If a CellData is
     * registered here, it is used as-is regardless of whether a GUID is
     * also registered.
     *
     * @param cellId  Cell ID.
     * @param data    CellData to use when this cell loads.
     */
    void RegisterCellData(uint32_t cellId, engine::world::CellData data);

protected:
    // =========================================================================
    // WorldStreamingManager hook overrides
    // =========================================================================

    /**
     * @brief Worker-thread hook — deserialise .level file into CellData.
     *
     * TEACHING NOTE — Thread safety of OnLoadCell
     * ────────────────────────────────────────────
     * This method runs on the AsyncLoader worker thread.  It must NOT touch:
     *   • The ECS World  (not thread-safe)
     *   • m_zones / m_zoneDataStore  (main-thread only)
     *
     * Safe operations:
     *   • Read m_cellGuids (read-only after Init())
     *   • Read m_preRegistered (read-only after RegisterCellData())
     *   • Call AssetLoader::LoadRaw()  (file I/O — thread-safe)
     *   • Write m_pendingData (under m_pendingMtx)
     */
    bool OnLoadCell(uint32_t id, engine::world::CellCoord coord) override;

    /**
     * @brief Main-thread hook — spawn Zone entities after load completes.
     *
     * Retrieves the CellData produced by OnLoadCell, builds a ZoneData from
     * it, then calls Zone::Load() → Zone::SpawnEnemies() → Zone::SpawnNPCs().
     */
    void OnCellLoaded(uint32_t id, engine::world::CellCoord coord, bool success) override;

    /**
     * @brief Main-thread hook — unload Zone and destroy all ECS entities.
     *
     * Calls Zone::Unload(world) which iterates the zone's tracked entity IDs
     * and calls world.DestroyEntity() for each one.
     */
    void OnEvictCell(uint32_t id, engine::world::CellCoord coord) override;

private:
    // =========================================================================
    // Member data — main-thread only (except where noted)
    // =========================================================================

    /// Optional asset loader for reading cooked .level files.
    engine::assets::AssetLoader* m_assetLoader = nullptr;

    // ---- Cell registration maps (written during setup, read-only after Init) ---

    /// Cell ID → asset GUID mapping.  Populated via RegisterCellGuid().
    std::unordered_map<uint32_t, std::string>                m_cellGuids;

    /// Cell ID → pre-registered CellData.  Populated via RegisterCellData().
    std::unordered_map<uint32_t, engine::world::CellData>    m_preRegistered;

    // ---- Pending data (written on worker thread, read on main thread) --------

    /// Mutex guarding m_pendingData for cross-thread access.
    std::mutex m_pendingMtx;

    /// CellData produced by OnLoadCell() and consumed by OnCellLoaded().
    std::unordered_map<uint32_t, engine::world::CellData>    m_pendingData;

    // ---- Active zones (main thread only) ------------------------------------

    /// Stable ZoneData storage keyed by cell ID.
    /// TEACHING NOTE — Stable storage is required because Zone stores a
    /// const ZoneData* — the ZoneData address must not change after Load().
    /// std::unordered_map guarantees stable element addresses on insert.
    std::unordered_map<uint32_t, ZoneData>  m_zoneDataStore;

    /// Live Zone instances keyed by cell ID.
    std::unordered_map<uint32_t, Zone>      m_zones;

    // ---- Static counter for auto-generated zone IDs -------------------------
    uint32_t m_nextZoneId = 1000;  ///< Auto-incremented zone ID for dynamic cells.
};
