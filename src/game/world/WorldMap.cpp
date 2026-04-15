/**
 * @file WorldMap.cpp
 * @brief Implementation of the overworld zone graph.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 */

#include "WorldMap.hpp"

#include <sstream>

// ---------------------------------------------------------------------------
// Load — build the zone graph
// ---------------------------------------------------------------------------

void WorldMap::Load()
{
    m_nodes.clear();

    // TEACHING NOTE — We define zones as data, not code. Each zone has an
    // ID, a display name, a position on the world-map screen, and a list of
    // connected zone IDs (adjacency list).
    //
    // Zone IDs match GameData.hpp ZoneData::id entries.

    auto addNode = [&](uint32_t id, const std::string& name,
                       int x, int y, std::vector<uint32_t> connections) {
        WorldNode node;
        node.zoneID            = id;
        node.name              = name;
        node.mapX              = x;
        node.mapY              = y;
        node.connectedZoneIDs  = std::move(connections);
        node.isLocked          = false;
        m_nodes[id]            = std::move(node);
    };

    // ── Leide Region ────────────────────────────────────────────────────────
    addNode(1,  "Hammerhead",          10, 30, {2, 3});
    addNode(2,  "Leide Plains",        15, 28, {1, 3, 4});
    addNode(3,  "Longwythe Peak",      12, 22, {1, 2, 5});
    addNode(4,  "Coernix Station",     20, 25, {2, 6});
    addNode(5,  "Malmalam Thicket",    10, 18, {3, 7});

    // ── Duscae Region ───────────────────────────────────────────────────────
    addNode(6,  "Prairie Outpost",     28, 22, {4, 7, 8});
    addNode(7,  "Duscae Crossroads",   22, 18, {5, 6, 9});
    addNode(8,  "Fociaugh Hollow",     32, 20, {6, 10});  // dungeon

    // ── Cleigne Region ──────────────────────────────────────────────────────
    addNode(9,  "Lestallum",           22, 12, {7, 10, 11});
    addNode(10, "Meldacio HQ",         36, 15, {8, 9, 12});

    // ── Altissia / Late Game ─────────────────────────────────────────────────
    addNode(11, "Old Lestallum",       18, 8,  {9, 12});
    addNode(12, "Rock of Ravatogh",    42, 10, {10, 11}); // locked initially

    // Lock late-game zones.
    m_nodes[12].isLocked = true;

    LOG_INFO("WorldMap loaded: " + std::to_string(m_nodes.size()) + " zones.");
}

// ---------------------------------------------------------------------------
// GetNode
// ---------------------------------------------------------------------------

const WorldNode* WorldMap::GetNode(uint32_t zoneID) const
{
    auto it = m_nodes.find(zoneID);
    return (it != m_nodes.end()) ? &it->second : nullptr;
}

// ---------------------------------------------------------------------------
// GetConnected
// ---------------------------------------------------------------------------

std::vector<const WorldNode*> WorldMap::GetConnected(uint32_t zoneID) const
{
    std::vector<const WorldNode*> result;
    const WorldNode* node = GetNode(zoneID);
    if (!node) return result;

    for (uint32_t id : node->connectedZoneIDs) {
        const WorldNode* nb = GetNode(id);
        if (nb) result.push_back(nb);
    }
    return result;
}

// ---------------------------------------------------------------------------
// GetAllNodes
// ---------------------------------------------------------------------------

const std::unordered_map<uint32_t, WorldNode>& WorldMap::GetAllNodes() const
{
    return m_nodes;
}

// ---------------------------------------------------------------------------
// Travel
// ---------------------------------------------------------------------------

bool WorldMap::Travel(uint32_t targetZoneID)
{
    const WorldNode* target = GetNode(targetZoneID);
    if (!target) {
        LOG_WARN("Travel: zone " + std::to_string(targetZoneID) + " not found.");
        return false;
    }

    if (target->isLocked) {
        LOG_INFO("Travel: zone " + target->name + " is locked.");
        return false;
    }

    currentZoneID = targetZoneID;
    LOG_INFO("Travelled to: " + target->name);
    return true;
}

// ---------------------------------------------------------------------------
// UnlockZone
// ---------------------------------------------------------------------------

void WorldMap::UnlockZone(uint32_t zoneID)
{
    auto it = m_nodes.find(zoneID);
    if (it == m_nodes.end()) return;
    it->second.isLocked = false;
    LOG_INFO("Zone unlocked: " + it->second.name);
}

// ---------------------------------------------------------------------------
// Serialize
// ---------------------------------------------------------------------------

std::string WorldMap::Serialize() const
{
    std::ostringstream oss;
    oss << "currentZone=" << currentZoneID << "\n";

    oss << "locked=";
    bool first = true;
    for (const auto& [id, node] : m_nodes) {
        if (node.isLocked) {
            if (!first) oss << ",";
            oss << id;
            first = false;
        }
    }
    oss << "\n";
    return oss.str();
}

// ---------------------------------------------------------------------------
// Deserialize
// ---------------------------------------------------------------------------

void WorldMap::Deserialize(const std::string& data)
{
    std::istringstream iss(data);
    std::string line;

    while (std::getline(iss, line)) {
        if (line.substr(0, 12) == "currentZone=") {
            currentZoneID = static_cast<uint32_t>(std::stoul(line.substr(12)));
        } else if (line.substr(0, 7) == "locked=") {
            std::string ids = line.substr(7);
            std::istringstream idStream(ids);
            std::string tok;
            // First unlock all, then re-lock from the saved list.
            for (auto& [id, node] : m_nodes) node.isLocked = false;

            while (std::getline(idStream, tok, ',')) {
                if (!tok.empty()) {
                    uint32_t id = static_cast<uint32_t>(std::stoul(tok));
                    auto it = m_nodes.find(id);
                    if (it != m_nodes.end()) it->second.isLocked = true;
                }
            }
        }
    }

    LOG_INFO("WorldMap deserialized. Current zone: " + std::to_string(currentZoneID));
}
