/**
 * @file terrain_collision.hpp
 * @brief BakeTerrainCollider — register terrain heightmap as a Jolt physics body (M25).
 *
 * ============================================================================
 * TEACHING NOTE — Terrain Collision in a Physics Engine
 * ============================================================================
 * For an open-world game like FF15, the terrain collision surface must match
 * the visual terrain exactly — the player should land where they *see* the
 * ground, not where a proxy box approximates it.
 *
 * Jolt Physics provides JPH::HeightFieldShape specifically for this purpose:
 *   • Takes a flat array of float height samples and a world-space scale.
 *   • Internally stores heights at reduced bit precision (8-bit by default)
 *     for cache efficiency, while retaining float-precision at query time.
 *   • Supports raycasts, convex-cast queries, and contact generation with
 *     all other shape types (spheres, capsules, boxes).
 *   • Used internally in Jolt's own sample code for large outdoor terrains.
 *
 * The shape is created as a STATIC body (infinite mass, never moves) so the
 * simulation can treat it as immovable geometry and apply broad-phase optimisations.
 *
 * ============================================================================
 * TEACHING NOTE — Isolation from Jolt Headers
 * ============================================================================
 * This header is guarded by ENGINE_ENABLE_PHYSICS so it is only visible to
 * translation units that compile with Jolt available.  PhysicsWorld.hpp
 * already uses the same guard; terrain_collision.cpp includes physics_impl.hpp
 * which is the ONLY file that may include raw <Jolt/...> headers.
 *
 * External engine code (main.cpp, sandbox scenes) only needs to include this
 * header and call BakeTerrainCollider() — Jolt types never leak out.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Platform: Windows (requires Jolt Physics); gated by ENGINE_ENABLE_PHYSICS
 */

#pragma once

#ifdef ENGINE_ENABLE_PHYSICS

#include "engine/math/math_types.hpp"
#include <cstdint>

namespace engine {
namespace physics {

class PhysicsWorld; // forward declaration

// ===========================================================================
// BakeTerrainCollider
// ===========================================================================

/**
 * @brief Create a static JPH::HeightFieldShape body from heightmap data.
 *
 * ============================================================================
 * TEACHING NOTE — HeightFieldShape Sample Count
 * ============================================================================
 * Jolt's JPH::HeightFieldShapeSettings requires the sample count to be a
 * power of 2 (e.g. 2, 4, 8, 16, 32, …).  The function enforces this
 * internally by checking (sampleCount & (sampleCount - 1)) == 0.
 *
 * The heights array must contain exactly sampleCount * sampleCount floats
 * in row-major order (row = Z direction, column = X direction).
 *
 * ============================================================================
 * TEACHING NOTE — World-Space Placement
 * ============================================================================
 * The terrain body is placed at the world origin; the offset parameter moves
 * the bottom-left corner of the height field in world space.  This allows
 * multiple terrain patches to tile seamlessly: pass the top-left world
 * position of each streaming cell as the offset.
 *
 * @param world       The physics world to add the body to.
 * @param heights     Row-major height array (sampleCount × sampleCount floats).
 * @param sampleCount Grid dimension (must be a power of 2, e.g. 4, 8, 16).
 * @param worldSizeX  Total terrain extent along X in world-space metres.
 * @param worldSizeZ  Total terrain extent along Z in world-space metres.
 * @param origin      World-space position of the (0, 0) grid corner.
 * @return Opaque body ID on success; PhysicsWorld::kInvalidBodyID on failure.
 */
uint32_t BakeTerrainCollider(
    PhysicsWorld&     world,
    const float*      heights,
    int               sampleCount,
    float             worldSizeX,
    float             worldSizeZ,
    math::Vec3        origin);

} // namespace physics
} // namespace engine

#endif // ENGINE_ENABLE_PHYSICS
