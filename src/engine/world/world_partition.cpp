/**
 * @file world_partition.cpp
 * @brief WorldPartition implementation — grid coordinate conversion + queries.
 *
 * ============================================================================
 * TEACHING NOTE — Keeping math in the .cpp
 * ============================================================================
 * The formulas in this file are short but non-obvious (floor division for
 * negative coordinates; Chebyshev radius enumeration).  Placing them here
 * keeps the header clean and the logic documented in one place.
 * ============================================================================
 */

#include "engine/world/world_partition.hpp"

#include <cmath>     // std::floor
#include <cstdint>   // int32_t

namespace engine {
namespace world {

// ===========================================================================
// Construction
// ===========================================================================

WorldPartition::WorldPartition(float cellSize, int streamRadius)
    : m_cellSize(cellSize)
    , m_streamRadius(streamRadius)
{
    // TEACHING NOTE — Defensive defaults
    // ────────────────────────────────────
    // Guard against nonsensical inputs without throwing exceptions.  A cell
    // size of 0 would cause division-by-zero in WorldToCell(); clamp to 1.
    if (m_cellSize <= 0.0f)
        m_cellSize = 1.0f;
    if (m_streamRadius < 0)
        m_streamRadius = 0;
}

// ===========================================================================
// Coordinate queries
// ===========================================================================

CellCoord WorldPartition::WorldToCell(const engine::math::Vec3& worldPos) const noexcept
{
    // TEACHING NOTE — Correct grid quantisation
    // ───────────────────────────────────────────
    // Dividing by cellSize and truncating (int cast) works for positive X/Z
    // but breaks for negatives:
    //   (int)(-0.001f / 256.0f) == 0    ← WRONG, should be cell -1
    //   (int)(-256.5f / 256.0f) == -1   ← accidentally correct
    //   (int)(-257.0f / 256.0f) == -1   ← WRONG, should be cell -2
    //
    // std::floor() handles all cases correctly:
    //   floor(-0.001f / 256.0f)  == -1  ← correct
    //   floor(-256.5f / 256.0f)  == -2  ← correct
    //
    // The cast to int32_t is safe because world coordinates are expected to
    // be within ±8 million metres (8388607 cells), well within int32_t range.

    const int32_t cx = static_cast<int32_t>(std::floor(worldPos.x / m_cellSize));
    const int32_t cz = static_cast<int32_t>(std::floor(worldPos.z / m_cellSize));
    return { cx, cz };
}

engine::math::Vec3 WorldPartition::CellCentre(CellCoord coord) const noexcept
{
    // The cell (cx, cz) spans [cx*size, (cx+1)*size) in X and Z.
    // Its centre is at (cx + 0.5) * size.
    const float cx = static_cast<float>(coord.cx);
    const float cz = static_cast<float>(coord.cz);
    return {
        (cx + 0.5f) * m_cellSize,   // X
        0.0f,                        // Y (cells are 2D; Y is irrelevant)
        (cz + 0.5f) * m_cellSize    // Z
    };
}

// ===========================================================================
// Streaming queries
// ===========================================================================

std::vector<CellCoord>
WorldPartition::GetCellsNearPosition(const engine::math::Vec3& viewPos) const
{
    // TEACHING NOTE — Chebyshev (square) neighbourhood
    // ──────────────────────────────────────────────────
    // For streaming radius R, the desired set is every cell (cx, cz) such that
    //   max(|origin.cx - cx|, |origin.cz - cz|) <= R
    //
    // That gives (2R+1)² cells:
    //   R=0 →  1 cell   (just the origin)
    //   R=1 →  9 cells  (3×3 patch)
    //   R=2 → 25 cells  (5×5 patch)
    //
    // We iterate (origin.cx − R) .. (origin.cx + R) in X
    // and       (origin.cz − R) .. (origin.cz + R) in Z.

    const CellCoord origin = WorldToCell(viewPos);
    const int R = m_streamRadius;

    std::vector<CellCoord> result;
    result.reserve(static_cast<size_t>((2 * R + 1) * (2 * R + 1)));

    for (int dz = -R; dz <= R; ++dz)
    {
        for (int dx = -R; dx <= R; ++dx)
        {
            result.push_back({
                origin.cx + static_cast<int32_t>(dx),
                origin.cz + static_cast<int32_t>(dz)
            });
        }
    }

    return result;
}

} // namespace world
} // namespace engine
