/**
 * @file nav_mesh.cpp
 * @brief Grid-based navigation mesh implementation.
 *
 * ============================================================================
 * TEACHING NOTE — A* on a Grid (Full Walkthrough)
 * ============================================================================
 *
 * The FindPath() function below implements classic A* on our grid nav mesh.
 * This is identical in structure to the A* in AISystem.cpp but is
 * ENGINE-LAYER code (no TileMap dependency) so it can be used by any
 * system that has a WalkabilityGrid.
 *
 * ─── Open Set (priority queue) ───────────────────────────────────────────
 *
 * The open set is a MIN-HEAP ordered by f = g + h.  C++ provides
 * std::priority_queue which is a MAX-HEAP by default.  We use
 * `std::greater<NavPathNode>` to invert it into a min-heap.
 *
 *   std::priority_queue<NavPathNode,
 *                       std::vector<NavPathNode>,
 *                       std::greater<NavPathNode>> openSet;
 *
 * ─── Closed Set ────────────────────────────────────────────────────────
 *
 * The closed set stores every cell we have FULLY PROCESSED (all neighbours
 * explored).  We use an unordered_set<uint64_t> (encoded coord) for O(1)
 * lookup.
 *
 * ─── g-cost map ────────────────────────────────────────────────────────
 *
 * gCost[coord] = the cheapest known cost to reach `coord` from `start`.
 * Initialised to infinity; updated whenever a cheaper path is found.
 *
 * ─── cameFrom map ──────────────────────────────────────────────────────
 *
 * cameFrom[coord] = the predecessor of `coord` on the cheapest path found
 * so far.  Used to reconstruct the path at the end.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 *
 * C++ Standard: C++17
 */

#include "nav_mesh.hpp"

#include <unordered_set>
#include <algorithm>   // std::reverse
#include <limits>      // std::numeric_limits


// ---------------------------------------------------------------------------
// BakeFromGrid
// ---------------------------------------------------------------------------

void NavMesh::BakeFromGrid(const WalkabilityGrid& grid, int width, int height)
{
    // TEACHING NOTE — Bounds checking during bake
    // ──────────────────────────────────────────────
    // The grid is provided by the game layer and might have inconsistent
    // dimensions.  We clamp to the provided width/height to avoid reading
    // out-of-bounds rows.
    m_width  = width;
    m_height = height;
    m_cells.assign(static_cast<size_t>(width * height), NavCell{});

    for (int y = 0; y < height; ++y) {
        if (y >= static_cast<int>(grid.size())) break;
        for (int x = 0; x < width; ++x) {
            if (x >= static_cast<int>(grid[static_cast<size_t>(y)].size())) break;
            m_cells[static_cast<size_t>(y * width + x)].walkable =
                grid[static_cast<size_t>(y)][static_cast<size_t>(x)];
        }
    }
}

void NavMesh::BakeEmpty(int width, int height)
{
    m_width  = width;
    m_height = height;
    m_cells.assign(static_cast<size_t>(width * height), NavCell{true});
}

void NavMesh::SetWalkable(int x, int y, bool walkable)
{
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) return;
    m_cells[static_cast<size_t>(y * m_width + x)].walkable = walkable;
}

bool NavMesh::IsWalkable(int x, int y) const
{
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) return false;
    return m_cells[static_cast<size_t>(y * m_width + x)].walkable;
}


// ---------------------------------------------------------------------------
// FindPath — A* on the grid
// ---------------------------------------------------------------------------

std::vector<TileCoord> NavMesh::FindPath(TileCoord start, TileCoord goal,
                                          bool allowDiagonals) const
{
    if (!IsReady()) return {};

    // Trivial case: already at goal.
    if (start.tileX == goal.tileX && start.tileY == goal.tileY) return { start };

    // Guard: start or goal outside grid / blocked.
    if (!IsWalkable(start.tileX, start.tileY)) return {};
    if (!IsWalkable(goal.tileX,  goal.tileY))  return {};

    // -----------------------------------------------------------------------
    // TEACHING NOTE — A* data structures
    // -----------------------------------------------------------------------
    // openSet    — cells to evaluate, ordered by f = g + h (min-heap).
    // closed     — cells fully processed (no need to revisit).
    // gCost      — cheapest known cost from `start` to each cell.
    // cameFrom   — predecessor map for path reconstruction.
    // -----------------------------------------------------------------------
    using PQ = std::priority_queue<NavPathNode,
                                   std::vector<NavPathNode>,
                                   std::greater<NavPathNode>>;
    PQ openSet;
    std::unordered_set<uint64_t>         closed;
    std::unordered_map<uint64_t, float>  gCost;
    std::unordered_map<uint64_t, TileCoord> cameFrom;

    const uint64_t startKey = EncodeCoord(start);
    gCost[startKey]         = 0.0f;
    openSet.push({ start, HeuristicManhattan(start, goal) });

    // Cardinal neighbours (always) and diagonal (optional).
    static const TileCoord kCardinal[4] = {
        {  0,  1 }, {  0, -1 }, {  1,  0 }, { -1,  0 }
    };
    static const TileCoord kDiagonal[4] = {
        {  1,  1 }, {  1, -1 }, { -1,  1 }, { -1, -1 }
    };
    static constexpr float kCardinalCost  = 1.0f;
    static constexpr float kDiagonalCost  = 1.41421356f;  // √2

    while (!openSet.empty()) {
        const NavPathNode cur = openSet.top();
        openSet.pop();

        const uint64_t curKey = EncodeCoord(cur.coord);

        // Skip if already processed (stale duplicate in the open set).
        if (closed.count(curKey)) continue;
        closed.insert(curKey);

        // Goal reached — reconstruct and return.
        if (cur.coord.tileX == goal.tileX && cur.coord.tileY == goal.tileY) {
            return ReconstructPath(cameFrom, start, goal);
        }

        const float curG = gCost.count(curKey) ? gCost.at(curKey) : 0.0f;

        // ── Expand neighbours ───────────────────────────────────────────────
        auto processNeighbour = [&](TileCoord offset, float stepCost) {
            TileCoord nb = { cur.coord.tileX + offset.tileX,
                             cur.coord.tileY + offset.tileY };
            if (!IsWalkable(nb.tileX, nb.tileY)) return;

            // TEACHING NOTE — Diagonal blocking (corner cutting)
            // ──────────────────────────────────────────────────────
            // When diagonal movement is allowed we block "corner cutting":
            // moving diagonally through the gap between two orthogonally
            // adjacent walls.  This prevents the agent clipping corners.
            if (std::abs(offset.tileX) + std::abs(offset.tileY) == 2) {
                // diagonal step — check both orthogonal neighbours are clear.
                if (!IsWalkable(cur.coord.tileX + offset.tileX, cur.coord.tileY) ||
                    !IsWalkable(cur.coord.tileX, cur.coord.tileY + offset.tileY))
                    return;
            }

            const uint64_t nbKey = EncodeCoord(nb);
            if (closed.count(nbKey)) return;

            const float tentativeG = curG + stepCost;
            const float knownG     = gCost.count(nbKey)
                                   ? gCost.at(nbKey)
                                   : std::numeric_limits<float>::max();

            if (tentativeG < knownG) {
                gCost[nbKey]    = tentativeG;
                cameFrom[nbKey] = cur.coord;

                const float h = allowDiagonals
                              ? HeuristicChebyshev(nb, goal)
                              : HeuristicManhattan(nb, goal);
                openSet.push({ nb, tentativeG + h });
            }
        };

        for (const auto& d : kCardinal)
            processNeighbour(d, kCardinalCost);

        if (allowDiagonals) {
            for (const auto& d : kDiagonal)
                processNeighbour(d, kDiagonalCost);
        }
    }

    return {};  // no path found
}


// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

/*static*/
uint64_t NavMesh::EncodeCoord(TileCoord c)
{
    return (static_cast<uint64_t>(c.tileX) << 32) |
            static_cast<uint64_t>(static_cast<uint32_t>(c.tileY));
}

/*static*/
float NavMesh::HeuristicManhattan(TileCoord a, TileCoord b)
{
    return static_cast<float>(std::abs(a.tileX - b.tileX) +
                               std::abs(a.tileY - b.tileY));
}

/*static*/
float NavMesh::HeuristicChebyshev(TileCoord a, TileCoord b)
{
    // TEACHING NOTE — Chebyshev distance for 8-directional grids
    // ─────────────────────────────────────────────────────────────
    // On an 8-directional grid, one diagonal step costs √2 (or 1 if you
    // use uniform costs).  The Chebyshev distance max(|dx|, |dy|) is
    // admissible because in the best case you move diagonally to close
    // both axes simultaneously.
    const int dx = std::abs(a.tileX - b.tileX);
    const int dy = std::abs(a.tileY - b.tileY);
    return static_cast<float>((dx > dy) ? dx : dy);
}

/*static*/
std::vector<TileCoord> NavMesh::ReconstructPath(
    const std::unordered_map<uint64_t, TileCoord>& cameFrom,
    TileCoord start, TileCoord goal)
{
    std::vector<TileCoord> path;
    TileCoord cur = goal;

    while (!(cur.tileX == start.tileX && cur.tileY == start.tileY)) {
        path.push_back(cur);
        const uint64_t key = EncodeCoord(cur);
        auto it = cameFrom.find(key);
        if (it == cameFrom.end()) break;  // safety guard
        cur = it->second;
    }
    path.push_back(start);

    // TEACHING NOTE — Reversing the reconstructed path
    // ──────────────────────────────────────────────────
    // We built the path from goal → start by following parent pointers.
    // std::reverse() gives us start → goal order without allocating a
    // second vector.
    std::reverse(path.begin(), path.end());
    return path;
}
