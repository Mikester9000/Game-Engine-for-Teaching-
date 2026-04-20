/**
 * @file world_partition.hpp
 * @brief WorldPartition — spatial grid that maps world positions to cell IDs.
 *
 * ============================================================================
 * TEACHING NOTE — What is World Partitioning?
 * ============================================================================
 * An open world is too large to keep entirely in memory.  Game engines divide
 * the world into a regular grid of rectangular "cells".  At any moment only
 * the cells near the camera are loaded; cells that are far away are evicted.
 *
 * This technique is called *World Partitioning* (or "streaming grid").
 * Final Fantasy XV divides Lucis into cells of roughly 256 × 256 metres.
 * The active set is typically a 3×3 block of cells centred on the player —
 * nine cells total, loaded asynchronously as the player moves.
 *
 * ─── Vocabulary ─────────────────────────────────────────────────────────────
 *   CellCoord  — 2D integer address in the grid, e.g. (0,0), (1,-2).
 *   CellId     — Linearised uint32_t key derived from a CellCoord.  Suitable
 *                as a hash-map key or array index.
 *   CellSize   — World-space side length of one cell (default: 256 m).
 *   StreamRadius — How many cell-widths around the viewpoint to consider
 *                  "in range" (default: 1, giving a 3×3 block).
 *
 * ─── Coordinate System ──────────────────────────────────────────────────────
 * The grid is axis-aligned with the XZ plane (Y is up, same as the rest of
 * the engine).  Cell (cx, cz) covers the world-space square:
 *
 *   x ∈ [cx * CellSize, (cx+1) * CellSize)
 *   z ∈ [cz * CellSize, (cz+1) * CellSize)
 *
 * ─── TODO (M7 full implementation) ──────────────────────────────────────────
 *   • Frustum cull: discard cells outside the camera frustum before handing
 *     them to the streaming manager.  Reduces unnecessary loads when the
 *     camera looks away from certain directions.
 *   • Variable cell sizes: allow larger cells for distant LODs.
 *   • Query variant that accepts a bounding sphere instead of a point.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Platform: All
 */

#pragma once

#include "engine/math/math_types.hpp"   // engine::math::Vec3

#include <vector>   // std::vector
#include <cstdint>  // uint32_t, int32_t

namespace engine {
namespace world {

// ===========================================================================
// CellCoord
// ===========================================================================

/**
 * @brief 2D integer address of a world cell in the streaming grid.
 *
 * TEACHING NOTE — Signed integers for cell coordinates
 * ─────────────────────────────────────────────────────
 * We use int32_t (not uint32_t) so negative coordinates are natural.
 * A world origin at (0,0) lets cells extend in all four directions:
 * (-1,-1) is south-west of origin, (1,1) is north-east, etc.
 */
struct CellCoord
{
    int32_t cx = 0;   ///< Cell column (X axis, East positive).
    int32_t cz = 0;   ///< Cell row    (Z axis, North positive).

    bool operator==(const CellCoord& o) const noexcept
    {
        return cx == o.cx && cz == o.cz;
    }

    bool operator!=(const CellCoord& o) const noexcept
    {
        return !(*this == o);
    }
};

// ===========================================================================
// CellId helpers
// ===========================================================================

/**
 * @brief Encode a CellCoord as a single 32-bit integer key.
 *
 * TEACHING NOTE — Packing two int16_t values into a uint32_t
 * ──────────────────────────────────────────────────────────────
 * We store the upper 16 bits as cx and the lower 16 bits as cz.
 * This works for worlds up to ±32 767 cells in any direction
 * (≈ 8 388 km at 256 m/cell — far larger than any practical game world).
 *
 * Shifting and masking is preferred over bit-fields because the bit layout
 * is explicit and portable across compilers.
 *
 * @param coord  The cell coordinate to encode.
 * @return       A unique 32-bit key for the cell.
 */
[[nodiscard]] inline uint32_t CellIdFromCoord(CellCoord coord) noexcept
{
    // Cast to unsigned before shifting to avoid UB on negative values.
    const uint32_t ucx = static_cast<uint32_t>(static_cast<int16_t>(coord.cx));
    const uint32_t ucz = static_cast<uint32_t>(static_cast<int16_t>(coord.cz));
    return (ucx << 16u) | (ucz & 0xFFFFu);
}

/**
 * @brief Decode a cell ID back to its CellCoord.
 *
 * @param id  A cell ID produced by CellIdFromCoord().
 * @return    The corresponding CellCoord.
 */
[[nodiscard]] inline CellCoord CellCoordFromId(uint32_t id) noexcept
{
    const int16_t cx = static_cast<int16_t>((id >> 16u) & 0xFFFFu);
    const int16_t cz = static_cast<int16_t>( id         & 0xFFFFu);
    return { static_cast<int32_t>(cx), static_cast<int32_t>(cz) };
}

// ===========================================================================
// WorldPartition
// ===========================================================================

/**
 * @brief Spatial grid manager: maps world positions to streaming cell IDs.
 *
 * Responsibilities:
 *   1. Convert a world-space Vec3 (camera / player position) to a CellCoord.
 *   2. Enumerate all CellCoords within a given streaming radius.
 *   3. Expose metadata about cells (size, total registered count).
 *
 * WorldPartition does NOT own cells or assets — it is a pure query interface.
 * The WorldStreamingManager uses it to decide *which* cells to load or evict.
 *
 * TEACHING NOTE — Single Responsibility Principle
 * ─────────────────────────────────────────────────
 * WorldPartition knows only about the grid geometry: sizes, coordinates, and
 * neighbours.  It has no knowledge of Zone, AsyncLoader, or ECS.  This keeps
 * it easy to test in isolation — no renderer or ECS World is needed.
 */
class WorldPartition
{
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct a WorldPartition with a given cell size.
     *
     * @param cellSize      World-space side length of one cell (metres).
     *                      Default: 256 m (matches the M7 spec).
     * @param streamRadius  Chebyshev radius (in cells) to consider "nearby".
     *                      Radius 1 → 3×3 = 9 cells; radius 2 → 5×5 = 25.
     *                      Default: 1.
     */
    explicit WorldPartition(float cellSize     = 256.0f,
                            int   streamRadius = 1);

    ~WorldPartition() = default;

    // =========================================================================
    // Coordinate queries
    // =========================================================================

    /**
     * @brief Convert a world-space position to a CellCoord.
     *
     * Uses the XZ plane (Y is ignored — cells are 2D tiles).
     *
     * @param worldPos  Arbitrary world-space position.
     * @return          The CellCoord of the cell that contains worldPos.
     *
     * TEACHING NOTE — floor() for correct handling of negative coordinates
     * ─────────────────────────────────────────────────────────────────────
     * Dividing by cellSize and truncating with (int) works for positive values
     * but gives wrong results for negatives: (int)(-0.5f) == 0, not -1.
     * std::floor() handles both correctly.
     */
    [[nodiscard]] CellCoord WorldToCell(const engine::math::Vec3& worldPos) const noexcept;

    /**
     * @brief Return the world-space centre of a cell.
     *
     * Useful for computing distances and building debug visualisations.
     *
     * @param coord  The cell coordinate.
     * @return       Centre of the cell in world space (Y = 0).
     */
    [[nodiscard]] engine::math::Vec3 CellCentre(CellCoord coord) const noexcept;

    // =========================================================================
    // Streaming queries
    // =========================================================================

    /**
     * @brief Enumerate all cells within the streaming radius of a position.
     *
     * Returns a flat list of CellCoords in row-major order (Z outer, X inner).
     * The origin cell (containing viewPos) is always included.
     *
     * @param viewPos  Camera / player position in world space.
     * @return         List of cells to keep loaded.
     *
     * Example (radius=1, cell at origin):
     * @code
     *   // viewPos = (0,0,0)  → returns cells (-1,-1)...(1,1), 9 total.
     *   auto cells = partition.GetCellsNearPosition({ 0.0f, 0.0f, 0.0f });
     *   assert(cells.size() == 9);
     * @endcode
     *
     * TEACHING NOTE — Chebyshev distance
     * ─────────────────────────────────────
     * We use the Chebyshev (∞-norm) metric: max(|Δcx|, |Δcz|) ≤ radius.
     * This produces a square patch of cells — simpler and cheaper than a
     * circular (Euclidean) query, and standard in streaming engines.
     */
    [[nodiscard]] std::vector<CellCoord>
    GetCellsNearPosition(const engine::math::Vec3& viewPos) const;

    // =========================================================================
    // Accessors
    // =========================================================================

    /// World-space side length of one cell (metres).
    [[nodiscard]] float CellSize()      const noexcept { return m_cellSize;     }

    /// Streaming radius in cells.
    [[nodiscard]] int   StreamRadius()  const noexcept { return m_streamRadius; }

private:
    float m_cellSize;       ///< Side length of one cell in world units.
    int   m_streamRadius;   ///< How many cells in each direction to include.
};

} // namespace world
} // namespace engine
