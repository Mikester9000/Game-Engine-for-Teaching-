/**
 * @file raycast.hpp
 * @brief Raycast and shape-cast queries against the PhysicsWorld.
 *
 * ============================================================================
 * TEACHING NOTE — Raycasting in Games
 * ============================================================================
 * A raycast is a fundamental building block in game physics.  It answers the
 * question "what is the first solid object in direction D from point O?"
 *
 * Common uses in an FFXV-style engine:
 *
 *   Ground detection    — Character controller fires a ray downward to find
 *                         the exact ground height for foot IK (planting feet
 *                         on uneven terrain).
 *
 *   Line of sight       — AI fires a ray between enemy and player to check
 *                         whether any obstacle blocks the view.
 *
 *   Shooting / targeting— A bullet fires a ray from the gun muzzle in the
 *                         aim direction.  The first hit is the target.
 *
 *   Hit detection       — Melee attack areas can be queries as box shape casts.
 *
 *   Camera collision    — The camera fires a ray from the player to the ideal
 *                         camera position; if it hits geometry the camera pulls
 *                         in to avoid clipping through walls.
 *
 * ─── RayCast vs ShapeCast ───────────────────────────────────────────────
 *   RayCast   — infinitely thin ray (zero radius).  Fastest query.
 *   ShapeCast — sweeps a shape (sphere, box, capsule) along a ray.
 *               Useful when you want a "thick" test (e.g. "is there room for
 *               the character capsule to walk here?").
 *
 * This file provides both.
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Platform: All (no platform-specific code)
 */

#pragma once

#include "engine/physics/physics_world.hpp"
#include "engine/math/math_types.hpp"

#include <cstdint>
#include <vector>

namespace engine {
namespace physics {

// ===========================================================================
// ShapeCastHit — output of a sphere/box shape sweep query
// ===========================================================================

/**
 * @struct ShapeCastHit
 * @brief Result of sweeping a shape through the world.
 *
 * TEACHING NOTE — Fraction vs Distance
 * Jolt returns a fraction in [0, 1] where 1 = full sweep length.
 * We convert to a world-space distance (metres) for convenience:
 *   distance = fraction × sweepLength
 */
struct ShapeCastHit
{
    bool       hit             = false;                   ///< True if the sweep hit something.
    float      distance        = 0.0f;                    ///< Distance from origin to first hit (m).
    math::Vec3 contactPoint    { 0.0f, 0.0f, 0.0f };     ///< World-space contact point.
    math::Vec3 contactNormal   { 0.0f, 1.0f, 0.0f };     ///< Surface normal at contact.
    uint32_t   bodyID          = kInvalidBodyID;          ///< Body that was hit.
};

// ===========================================================================
// Raycast helpers
// ===========================================================================

/**
 * @brief Fire a single ray through the physics world and return the first hit.
 *
 * TEACHING NOTE — Normalised Direction
 * The direction vector MUST be normalised (length == 1).  If you have a
 * direction from A to B, normalise it first:
 *   Vec3 dir = B - A;  float len = dir.Length();  dir = dir / len;
 * Then pass len as maxDist.
 *
 * @param world      The physics world to query.
 * @param origin     Ray start (world space).
 * @param direction  Normalised direction vector (must have length ~= 1).
 * @param maxDist    Maximum ray distance (metres).
 * @param outHit     Populated when hit == true.
 * @return True if a hit was found.
 */
bool CastRay(PhysicsWorld&     world,
             math::Vec3        origin,
             math::Vec3        direction,
             float             maxDist,
             RaycastHit&       outHit);

/**
 * @brief Fire a downward ray from origin to detect the ground below.
 *
 * TEACHING NOTE — Ground Detection Pattern
 * This helper encapsulates the most common raycast use-case in action games:
 * finding the ground height directly below a point.  It fires a ray in the
 * -Y direction and returns the hit distance.
 *
 * Use cases:
 *   • Foot IK: fire from each ankle, plant the foot at hit.position.
 *   • Camera: determine if camera has ground underneath.
 *   • NavMesh probe: snap a pathfinding node to the terrain surface.
 *
 * @param world    The physics world.
 * @param origin   Point to probe from (world space).
 * @param maxDist  How far down to search (metres).
 * @param outHit   Populated when hit == true.
 * @return True if ground was found within maxDist.
 */
bool CastRayDown(PhysicsWorld& world,
                 math::Vec3    origin,
                 float         maxDist,
                 RaycastHit&   outHit);

// ===========================================================================
// Shape cast helpers
// ===========================================================================

/**
 * @brief Sweep a sphere along a ray and return the first hit.
 *
 * TEACHING NOTE — Sphere Sweep for Thick Queries
 * A sphere sweep is equivalent to a "fat" ray — it tests whether a sphere of
 * the given radius can travel from origin in direction without hitting anything.
 * Use this when you need a "is there room for object of radius R" test.
 *
 * Example: Camera spring-arm
 *   Sweep a sphere of radius 0.3 m from the player's shoulder to the desired
 *   camera position.  If it hits, the camera position is the hit point.
 *
 * @param world      The physics world.
 * @param origin     Sweep start (sphere centre).
 * @param direction  Normalised sweep direction.
 * @param radius     Sphere radius (metres).
 * @param maxDist    Sweep length (metres).
 * @param outHit     Populated when hit == true.
 * @return True if the sphere hit something.
 */
bool CastSphere(PhysicsWorld&   world,
                math::Vec3      origin,
                math::Vec3      direction,
                float           radius,
                float           maxDist,
                ShapeCastHit&   outHit);

} // namespace physics
} // namespace engine
