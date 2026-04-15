/**
 * @file WorldMap.hpp
 * @brief Overworld graph — named zones and travel connections.
 *
 * ============================================================================
 * TEACHING NOTE — World Map as a Graph
 * ============================================================================
 *
 * In many RPGs the "world map" is not a free-roam 3D space but a simplified
 * GRAPH: nodes are named locations (towns, dungeons, outposts) and edges are
 * travel routes between them.
 *
 * This design has several advantages:
 *   1. Clear scope — each Zone loads/unloads independently.
 *   2. Fast travel — "teleport to node" is trivial.
 *   3. Quest gating — lock edges until the player completes prerequisites.
 *
 * Data structure choice: std::unordered_map<uint32_t, WorldNode> gives
 * O(1) average lookup by zone ID, which is common (every player movement
 * checks the map).
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

#include "../../engine/core/Types.hpp"
#include "../../engine/core/Logger.hpp"
#include "../GameData.hpp"

// ---------------------------------------------------------------------------
// WorldNode — one location on the world map
// ---------------------------------------------------------------------------

/**
 * @struct WorldNode
 * @brief A single named location in the overworld.
 *
 * TEACHING NOTE — Graph Node vs. Zone
 * ─────────────────────────────────────
 * A WorldNode is a LIGHTWEIGHT DESCRIPTOR — it holds the zone ID and a
 * 2D position for drawing on the map screen, but NOT the full TileMap.
 * The TileMap lives inside the Zone object (loaded on demand).
 *
 * Keeping the descriptor light means ALL nodes can be in memory at once
 * (for the map screen UI) without loading every TileMap simultaneously.
 */
struct WorldNode {
    uint32_t             zoneID   = 0;  ///< Matches ZoneData::id in GameData.
    std::string          name;          ///< Display name on the world map.
    int                  mapX    = 0;   ///< X position on the world-map graphic.
    int                  mapY    = 0;   ///< Y position on the world-map graphic.
    bool                 isLocked = false;  ///< True = not yet accessible.
    std::vector<uint32_t> connectedZoneIDs; ///< Zones reachable directly.
};

// ---------------------------------------------------------------------------
// WorldMap class
// ---------------------------------------------------------------------------

/**
 * @class WorldMap
 * @brief Manages the overworld graph: all zones and their travel connections.
 *
 * USAGE EXAMPLE:
 * @code
 *   WorldMap wmap;
 *   wmap.Load();
 *
 *   // Get all connected locations from current zone:
 *   for (const WorldNode* n : wmap.GetConnected(wmap.currentZoneID)) {
 *       std::cout << "Can travel to: " << n->name << "\n";
 *   }
 *
 *   // Travel to zone 2 (Leide Plains):
 *   if (wmap.Travel(2)) {
 *       // Load zone 2 content...
 *   }
 * @endcode
 */
class WorldMap {
public:
    /// The zone the player is currently in.
    uint32_t currentZoneID = 1;

    // -----------------------------------------------------------------------
    // Initialisation
    // -----------------------------------------------------------------------

    /**
     * @brief Build all zone nodes and connections from GameData.
     *
     * Populates the internal node map.  Call once at startup.
     *
     * TEACHING NOTE — Data-Driven World Definition
     * ──────────────────────────────────────────────
     * Zone metadata comes from GameDatabase::GetZones() (conceptually — here
     * we define the graph inline).  In a shipped game this would be loaded
     * from a data file or database, allowing level designers to add zones
     * without recompiling C++.
     */
    void Load();

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------

    /**
     * @brief Return the node for a given zone ID.
     * @return Pointer into the internal map, or nullptr if not found.
     */
    const WorldNode* GetNode(uint32_t zoneID) const;

    /**
     * @brief Return all nodes directly connected to `zoneID`.
     *
     * TEACHING NOTE — Adjacency List Representation
     * ─────────────────────────────────────────────
     * Each WorldNode stores connectedZoneIDs (an adjacency list).
     * This is more memory-efficient than an adjacency matrix for sparse
     * graphs (most zones connect to 2–4 others, not all others).
     */
    std::vector<const WorldNode*> GetConnected(uint32_t zoneID) const;

    /// Return all nodes (for rendering the map screen).
    const std::unordered_map<uint32_t, WorldNode>& GetAllNodes() const;

    // -----------------------------------------------------------------------
    // Travel
    // -----------------------------------------------------------------------

    /**
     * @brief Attempt fast-travel to `targetZoneID`.
     *
     * Requirements:
     *  - The target zone must be directly connected to currentZoneID, OR
     *  - Fast-travel to any unlocked zone is permitted (open-world style).
     *
     * @return true if travel succeeded (currentZoneID updated).
     */
    bool Travel(uint32_t targetZoneID);

    /**
     * @brief Unlock a zone, making it accessible for travel.
     *
     * Called when the player discovers a new area or completes a quest.
     */
    void UnlockZone(uint32_t zoneID);

    // -----------------------------------------------------------------------
    // Serialization
    // -----------------------------------------------------------------------

    /**
     * @brief Serialize the world map state (current zone, locked zones) to text.
     *
     * Format: "currentZone=N\nlocked=id1,id2,...\n"
     *
     * TEACHING NOTE — Simple Text Serialization
     * ──────────────────────────────────────────
     * We use plain text (not binary) for portability and debuggability.
     * Players and modders can open save files in a text editor.  For large
     * amounts of data, binary formats (FlatBuffers, protobuf) are faster.
     */
    std::string Serialize() const;

    /**
     * @brief Restore world map state from serialized string.
     */
    void Deserialize(const std::string& data);

private:
    std::unordered_map<uint32_t, WorldNode> m_nodes;
};
