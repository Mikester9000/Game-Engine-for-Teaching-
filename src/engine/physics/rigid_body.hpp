/**
 * @file rigid_body.hpp
 * @brief RigidBodyCreator — helpers for creating Jolt bodies from ECS components.
 *
 * ============================================================================
 * TEACHING NOTE — Rigid Bodies in Game Physics
 * ============================================================================
 * A rigid body is an idealised object that:
 *   • Has a fixed shape (no deformation).
 *   • Has mass, moment of inertia, and a centre of mass.
 *   • Responds to forces, torques, and collisions according to Newton's laws.
 *
 * In FFXV, rigid bodies are used for:
 *   • Debris and destructible props (crates, barrels, rocks).
 *   • Enemy ragdolls after death.
 *   • Physics-based puzzle objects.
 *   • Projectiles (grenades, spells with AoE hits).
 *
 * ─── Static vs Dynamic ──────────────────────────────────────────────────────
 *   Static  — infinite mass; never moves under physics.
 *             Examples: floors, walls, terrain meshes.
 *             Cost: zero solver work per frame (broadphase only).
 *
 *   Dynamic — finite mass; accelerated by gravity and collisions.
 *             Examples: debris, physics-driven enemies, rolling boulders.
 *             Cost: one integration step + constraint solve per body per frame.
 *
 * ─── ECS Integration ────────────────────────────────────────────────────────
 * The RigidBodyCreator bridges the ECS and the physics simulation:
 *   1. An entity gets a RigidBodyComponent + ColliderComponent.
 *   2. A physics setup pass reads those components and calls
 *      RigidBodyCreator::CreateFromComponents(), which creates the Jolt body
 *      and writes the resulting bodyID back into RigidBodyComponent::bodyID.
 *   3. Each frame, the physics system calls Step(), then reads positions
 *      via PhysicsWorld::GetPosition(bodyID) and writes them back into
 *      TransformComponent so the renderer sees updated positions.
 *
 * ─── Sync Direction ─────────────────────────────────────────────────────────
 *   Physics → ECS:   After Step(), copy positions/rotations to TransformComponent.
 *   ECS → Physics:   When gameplay teleports an entity (e.g. warp-strike),
 *                    call PhysicsWorld::SetPosition(bodyID, newPos).
 *
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

namespace engine {
namespace physics {

// ===========================================================================
// RigidBodyCreator
// ===========================================================================

/**
 * @class RigidBodyCreator
 * @brief Utility class for creating Jolt bodies from shape descriptors.
 *
 * TEACHING NOTE — Single Responsibility
 * RigidBodyCreator is a stateless utility class (all methods are static).
 * It has one responsibility: translating shape type + parameters + position
 * into a Jolt body ID.  It does NOT own the PhysicsWorld or any body lifetime.
 *
 * Keeping factory logic separate from PhysicsWorld keeps each class small
 * and focused — easier to test, easier to read.
 */
class RigidBodyCreator
{
public:
    // -----------------------------------------------------------------------
    // Shape descriptor
    // -----------------------------------------------------------------------

    /**
     * @enum ShapeType
     * @brief Collision shape to generate for a body.
     *
     * TEACHING NOTE — Choosing a Collision Shape
     *
     *   Box      — fastest; best for crates, pillars, terrain patches.
     *   Sphere   — spheres roll smoothly; best for cannonballs, orbs.
     *   Capsule  — smooth sliding; best for characters, NPCs, props.
     *
     * In a shipping game you would also support:
     *   ConvexHull   — arbitrary convex mesh (imported from .obj/.fbx).
     *   MeshShape    — concave mesh for complex terrain (static only).
     *   HeightField  — optimised terrain shape (height map grid).
     */
    enum class ShapeType : uint8_t
    {
        Box     = 0,   ///< Axis-aligned bounding box.
        Sphere  = 1,   ///< Sphere.
        Capsule = 2,   ///< Upright capsule (cylinder + two hemispheres).
    };

    /**
     * @struct Descriptor
     * @brief Everything needed to create one rigid body.
     */
    struct Descriptor
    {
        ShapeType  shapeType   = ShapeType::Box;

        // Shape dimensions
        math::Vec3 halfExtents { 0.5f, 0.5f, 0.5f };  ///< Box: half-size per axis
        float      radius      = 0.5f;                 ///< Sphere / capsule radius
        float      halfHeight  = 0.85f;                ///< Capsule: half-height of cylinder

        // Body properties
        math::Vec3 position    { 0.0f, 0.0f, 0.0f };  ///< Initial world position
        float      mass        = 1.0f;                 ///< Mass in kg (ignored if isStatic)
        bool       isStatic    = false;                ///< True = immovable
        bool       useGravity  = true;                 ///< False = kinematic (no gravity)
    };

    // -----------------------------------------------------------------------
    // Factory method
    // -----------------------------------------------------------------------

    /**
     * @brief Create a Jolt body from a Descriptor and add it to the world.
     *
     * @param world  The PhysicsWorld to add the body into.
     * @param desc   Shape and property description.
     * @return       Opaque body ID (uint32_t) or kInvalidBodyID on failure.
     */
    static uint32_t Create(PhysicsWorld& world, const Descriptor& desc);

    /**
     * @brief Remove and destroy a body from the physics world.
     *
     * Call this when the owning entity is destroyed or its physics is removed.
     *
     * @param world  The PhysicsWorld the body lives in.
     * @param bodyID Body ID returned by Create().
     */
    static void Destroy(PhysicsWorld& world, uint32_t bodyID);

    // -----------------------------------------------------------------------
    // Per-frame sync helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Copy physics position into an output Vec3.
     *
     * Call this each frame after PhysicsWorld::Step() to propagate physics
     * state back to the ECS TransformComponent.
     *
     * @param world  The PhysicsWorld.
     * @param bodyID The body to query.
     * @param outPos Receives the world-space position.
     */
    static void SyncPositionFromPhysics(const PhysicsWorld& world,
                                        uint32_t            bodyID,
                                        math::Vec3&         outPos);

    /**
     * @brief Push a new position from ECS into the physics body.
     *
     * Use when gameplay directly moves an entity (warp-strike, respawn, etc.)
     * so the physics body follows.
     *
     * @param world  The PhysicsWorld.
     * @param bodyID The body to move.
     * @param pos    New world-space position.
     */
    static void PushPositionToPhysics(PhysicsWorld&      world,
                                      uint32_t           bodyID,
                                      const math::Vec3&  pos);
};

} // namespace physics
} // namespace engine
