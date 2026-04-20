/**
 * @file cell_data.hpp
 * @brief CellData — cooked cell descriptor loaded from a .level file.
 *
 * ============================================================================
 * TEACHING NOTE — Data Transfer Objects (DTOs) in a Pipeline
 * ============================================================================
 * CellData is a plain-old-data (POD) struct that acts as the handoff between
 * two stages of the streaming pipeline:
 *
 *   Stage 1 (worker thread)  — OnLoadCell() reads the .level file and
 *                               deserialises its JSON into a CellData.
 *   Stage 2 (main thread)    — OnCellLoaded() receives the CellData and
 *                               calls Zone::Load() + SpawnEnemies() to
 *                               create the actual ECS entities.
 *
 * CellData intentionally has NO virtual methods, NO pointers to heap-allocated
 * resources, and NO ECS types.  This makes it trivially copyable between
 * threads and easy to serialise/deserialise.
 *
 * ─── .level file format (JSON source, binary cooked) ─────────────────────────
 * Source cell descriptor (Content/Levels/*.cell.json):
 * {
 *   "zoneName":         "Duscae Grasslands",
 *   "tileWidth":        40,
 *   "tileHeight":       40,
 *   "isDungeon":        false,
 *   "dangerLevel":      2,
 *   "recommendedLevel": 10,
 *   "spawns": [
 *     { "enemyDataID": 1, "tileX": 5, "tileY": 7, "respawnTime": 45.0 },
 *     { "enemyDataID": 2, "tileX": 22, "tileY": 31, "respawnTime": 60.0 }
 *   ],
 *   "npcIDs":  [101, 102],
 *   "shopIDs": [5]
 * }
 *
 * The cook step (cook_assets.py cook_levels()) copies the source JSON into
 * Cooked/Levels/ without transformation.  A future cook step could convert
 * to a binary format for faster runtime parsing.
 *
 * ─── SpawnEntry ───────────────────────────────────────────────────────────────
 * Each SpawnEntry in CellData::spawns tells GameStreamingManager where to
 * place one enemy when the cell loads.  enemyDataID maps to a GameDatabase
 * entry that provides the enemy's stats, AI behaviour, loot table, etc.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17 (no platform-specific code)
 */

#pragma once

#include <cstdint>    // uint32_t
#include <string>     // std::string
#include <vector>     // std::vector

namespace engine {
namespace world {

// ===========================================================================
// SpawnEntry
// ===========================================================================

/**
 * @brief A single enemy spawn point within a streaming cell.
 *
 * TEACHING NOTE — Tile-coordinate spawn points
 * ────────────────────────────────────────────────
 * Spawn positions are stored in TILE coordinates (integer grid), not world
 * units.  At load time, GameStreamingManager converts tile coordinates to
 * world units: worldX = tileX * Zone::kTileSize + cellOriginX.
 * Tile coordinates are compact (int16 would suffice) and independent of
 * world scale, making them safe to store in cooked files without precision loss.
 */
struct SpawnEntry
{
    uint32_t enemyDataID   = 0;      ///< Index into GameDatabase::enemies[].
    int      tileX         = 0;      ///< Spawn tile column within the cell.
    int      tileY         = 0;      ///< Spawn tile row within the cell.
    float    respawnTime   = 30.0f;  ///< Seconds before this enemy respawns after death.
};

// ===========================================================================
// CellData
// ===========================================================================

/**
 * @brief All data needed to initialise a single streaming cell.
 *
 * Populated on the worker thread by GameStreamingManager::OnLoadCell().
 * Consumed on the main thread by GameStreamingManager::OnCellLoaded()
 * via Zone::Load() → Zone::SpawnEnemies() → Zone::SpawnNPCs().
 */
struct CellData
{
    // ---- Identity -----------------------------------------------------------
    std::string  zoneName;               ///< Human-readable name (e.g. "Duscae Grasslands").

    // ---- Tilemap dimensions -------------------------------------------------
    uint32_t     tileWidth        = 40;  ///< Tile columns in the cell.
    uint32_t     tileHeight       = 40;  ///< Tile rows in the cell.

    // ---- Gameplay metadata --------------------------------------------------
    bool         isDungeon        = false;  ///< True → no day/night FX inside this cell.
    int          dangerLevel      = 1;      ///< Suggested encounter difficulty 1–5.
    uint32_t     recommendedLevel = 1;      ///< Suggested player level.

    // ---- Spawn lists --------------------------------------------------------
    std::vector<SpawnEntry>  spawns;   ///< Enemy spawn points.
    std::vector<uint32_t>    npcIDs;   ///< NPC data IDs to instantiate.
    std::vector<uint32_t>    shopIDs;  ///< Shop IDs to make available in this cell.

    // ---- Parse status -------------------------------------------------------
    /// Set to true by the loader after successful deserialisation.
    /// GameStreamingManager treats valid==false as a load failure.
    bool         valid            = false;
};

} // namespace world
} // namespace engine
