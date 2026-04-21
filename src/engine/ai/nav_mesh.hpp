/**
 * @file nav_mesh.hpp
 * @brief Lightweight grid-based navigation mesh for enemy pathfinding.
 *
 * ============================================================================
 * TEACHING NOTE — What is a Nav Mesh?
 * ============================================================================
 *
 * A Navigation Mesh (nav mesh) is the data structure that answers the question
 * "Which areas of the world can an AI agent walk through?".  It has two jobs:
 *
 *   1. BAKING — convert a world representation (tile map, height map, 3D mesh)
 *      into a compact, queryable walkability graph.
 *
 *   2. PATHFINDING — given a baked nav mesh, find the shortest walkable path
 *      from point A to point B using A*.
 *
 * ─── Grid Nav Mesh vs. Polygon Nav Mesh ──────────────────────────────────
 *
 * TEACHING NOTE — Two common representations
 * ────────────────────────────────────────────
 * GRID NAV MESH (this file):
 *   • World divided into a regular grid of cells (walkable / blocked).
 *   • Simple to implement; A* on a grid is fast for small worlds.
 *   • Memory: width × height bytes.
 *   • Pathfinding: O(N log N) where N = number of cells in the grid.
 *
 * POLYGON NAV MESH (used in Unreal Engine, Unity, Recast):
 *   • World represented as a mesh of convex polygons (portals between them).
 *   • Much more memory efficient for large open worlds.
 *   • Path is a sequence of polygon-crossing portals (funnel algorithm gives
 *     the shortest straight path inside the corridor of polygons).
 *   • Used in every modern AAA open-world game including FFXV.
 *
 * For this educational engine we implement the GRID variant because:
 *   • The tile-based game world maps directly to a grid.
 *   • The algorithm is fully readable in ~100 lines.
 *   • It teaches the core A* concepts that polygon nav meshes also use.
 *   • Students can graduate to Recast/Detour as a next step.
 *
 * ─── Baking ──────────────────────────────────────────────────────────────
 *
 * TEACHING NOTE — Offline vs. Runtime Baking
 * ────────────────────────────────────────────
 * Professional engines bake the nav mesh OFFLINE (at content creation time)
 * and store the result in a cooked asset.  This allows complex polygon nav
 * meshes to be generated without impacting runtime frame time.
 *
 * Our grid nav mesh can be baked at RUNTIME from a `WalkabilityGrid` in
 * under 1 ms for a 256×256 cell map — small enough that it runs once at
 * level load.
 *
 * ─── Connection with AISystem (FSM) and BehaviourTree ────────────────────
 *
 * The existing AISystem.cpp uses A* directly on the TileMap.  NavMesh wraps
 * the same algorithm in a more general API:
 *   • NavMesh::BakeFromGrid()  — replaces TileMap dependency.
 *   • NavMesh::FindPath()      — same A* result.
 *   • BtAction nodes can call NavMesh::FindPath() via the blackboard.
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

#include <vector>
#include <cstdint>
#include <cmath>
#include <queue>
#include <unordered_map>

#include "../../engine/core/Types.hpp"


// ============================================================================
// NavCell — the basic unit of the nav mesh grid
// ============================================================================

/**
 * @struct NavCell
 * @brief One cell in the grid nav mesh.
 *
 * TEACHING NOTE — Minimal cell representation
 * ─────────────────────────────────────────────
 * We store only `walkable` here.  A more complete implementation might add:
 *   • `cost`  — extra traversal cost (mud = 2×, road = 0.5×).
 *   • `area`  — area type (grass, water, road) for material-specific SFX.
 *   • `slope` — vertical angle (steep slopes block slow NPCs but not agile ones).
 * Each of these is a one-field extension that students can add as exercises.
 */
struct NavCell {
    bool walkable = true;  ///< false = obstacle; A* treats it as impassable.
};


// ============================================================================
// NavPathNode — internal A* priority queue entry
// ============================================================================

/**
 * @struct NavPathNode
 * @brief A* open-set entry with f-cost ordering.
 *
 * TEACHING NOTE — Re-using the PathNode pattern from AISystem.hpp
 * ──────────────────────────────────────────────────────────────────
 * This is deliberately similar to the `PathNode` struct in AISystem.hpp.
 * We duplicate it here (rather than sharing) because NavMesh lives in the
 * engine layer (engine/ai/) and must NOT depend on game/ code.
 * Keeping the layers separate is more important than avoiding a small
 * struct duplication.
 */
struct NavPathNode {
    TileCoord coord;
    float     fCost = 0.0f;

    bool operator>(const NavPathNode& o) const { return fCost > o.fCost; }
};


// ============================================================================
// NavMesh — baked grid nav mesh with A* pathfinding
// ============================================================================

/**
 * @class NavMesh
 * @brief A lightweight grid-based navigation mesh.
 *
 * Workflow:
 *   1. Provide a walkability grid via BakeFromGrid().
 *   2. Query paths via FindPath().
 *
 * USAGE:
 * @code
 *   NavMesh navMesh;
 *
 *   // Bake from a 10×10 all-walkable grid.
 *   NavMesh::WalkabilityGrid grid(10, std::vector<bool>(10, true));
 *   navMesh.BakeFromGrid(grid, 10, 10);
 *
 *   // Find a path.
 *   auto path = navMesh.FindPath({0,0}, {9,9});
 *   // path = [{0,0}, {1,1}, ..., {9,9}]  (diagonal steps if allowDiag=true)
 * @endcode
 */
class NavMesh {
public:

    /**
     * @brief Row-major 2D grid of walkability flags.
     *
     * Access pattern: grid[row][col] = grid[y][x].
     * This matches the TileMap::IsPassable(x, y) convention.
     */
    using WalkabilityGrid = std::vector<std::vector<bool>>;

    // -----------------------------------------------------------------------
    // Baking
    // -----------------------------------------------------------------------

    /**
     * @brief Bake the nav mesh from a pre-computed walkability grid.
     *
     * This is the main entry point for runtime baking at level load.
     * The grid dimensions must match @p width and @p height exactly.
     *
     * @param grid         walkability[row][col] (row = y, col = x).
     * @param width        Number of cells along the X axis.
     * @param height       Number of cells along the Z axis (rows).
     *
     * TEACHING NOTE — Bake = "offline" pre-process
     * ──────────────────────────────────────────────
     * After baking, FindPath() is read-only and thread-safe.  You can
     * query paths from multiple AI threads simultaneously.  In a production
     * engine the bake step would also compute:
     *   • Portal graph (for polygon nav meshes via Recast).
     *   • Hierarchical clusters (HPA* for very large maps).
     *   • Pre-computed distance fields (for swarm AI movement).
     */
    void BakeFromGrid(const WalkabilityGrid& grid, int width, int height);

    /**
     * @brief Convenience bake: all cells walkable (useful for unit tests).
     *
     * @param width   Grid width (X).
     * @param height  Grid height (Z).
     */
    void BakeEmpty(int width, int height);

    /**
     * @brief Mark a specific cell as walkable or blocked.
     *
     * Allows incremental updates (e.g. a door closes at runtime).
     * Inexpensive — O(1).
     */
    void SetWalkable(int x, int y, bool walkable);

    // -----------------------------------------------------------------------
    // Pathfinding
    // -----------------------------------------------------------------------

    /**
     * @brief Find the shortest walkable path from @p start to @p goal.
     *
     * Implements A* with Manhattan-distance heuristic on a 4-directional
     * (cardinal) grid.  If @p allowDiagonals is true, 8-directional movement
     * is used with a diagonal cost of √2.
     *
     * @param start          Start tile coordinate (inclusive).
     * @param goal           Goal tile coordinate (inclusive).
     * @param allowDiagonals If true, diagonal movement is permitted.
     * @return               Ordered list of tile steps from start (inclusive)
     *                       to goal (inclusive).  Empty if no path exists.
     *
     * TEACHING NOTE — Diagonal movement and heuristic admissibility
     * ──────────────────────────────────────────────────────────────
     * With 4-directional movement, the optimal heuristic is Manhattan distance:
     *   h = |dx| + |dy|   (never overestimates on a 4-dir grid → admissible)
     *
     * With 8-directional movement, the Chebyshev distance is admissible:
     *   h = max(|dx|, |dy|)
     *
     * Using Manhattan distance with 8-directional movement slightly
     * overestimates (non-admissible), which makes A* faster but may not
     * return the absolute shortest path.  For game AI this trade-off is
     * usually acceptable.
     */
    std::vector<TileCoord> FindPath(TileCoord start, TileCoord goal,
                                    bool allowDiagonals = false) const;

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------

    /** @brief Returns true if the nav mesh has been baked. */
    bool IsReady() const { return m_width > 0 && m_height > 0; }

    /** @brief Returns the width (X) of the baked grid. */
    int Width()  const { return m_width; }

    /** @brief Returns the height (Z) of the baked grid. */
    int Height() const { return m_height; }

    /** @brief Returns true if cell (x, y) is within bounds and walkable. */
    bool IsWalkable(int x, int y) const;

private:

    // -----------------------------------------------------------------------
    // A* helpers
    // -----------------------------------------------------------------------

    /** Encode a TileCoord into a uint64_t for use as an unordered_map key. */
    static uint64_t EncodeCoord(TileCoord c);

    /** Manhattan-distance heuristic (4-directional). */
    static float HeuristicManhattan(TileCoord a, TileCoord b);

    /** Chebyshev-distance heuristic (8-directional). */
    static float HeuristicChebyshev(TileCoord a, TileCoord b);

    /** Reconstruct the path by following parent pointers from goal to start. */
    static std::vector<TileCoord> ReconstructPath(
        const std::unordered_map<uint64_t, TileCoord>& cameFrom,
        TileCoord start, TileCoord goal);

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    int               m_width  = 0;
    int               m_height = 0;

    // Flat row-major array of cells: m_cells[y * m_width + x].
    // TEACHING NOTE — Flat array vs. vector-of-vectors
    // A flat array has better cache locality than a vector-of-vectors because
    // all elements are contiguous in memory.  The A* inner loop accesses
    // neighbours sequentially; cache misses on a vector-of-vectors would
    // noticeably slow it down on large grids.
    std::vector<NavCell> m_cells;
};
