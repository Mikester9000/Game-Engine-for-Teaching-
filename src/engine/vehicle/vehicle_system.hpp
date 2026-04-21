/**
 * @file vehicle_system.hpp
 * @brief VehicleSystem — wheel-ray suspension vehicle physics.
 *
 * ============================================================================
 * TEACHING NOTE — Vehicle Physics Overview
 * ============================================================================
 * FFXV's Regalia uses a technique called *wheel-ray suspension* (also known
 * as "ray-cast vehicle" physics).  Rather than modelling the full suspension
 * geometry as rigid bodies with joints, four short downward raycasts represent
 * the four wheels.  The gap between each ray's hit point and the wheel's
 * "rest position" gives the spring compression, from which a spring force is
 * computed.  This approach is used in virtually every AAA open-world game
 * (GTA series, Horizon, etc.) because:
 *
 *   • It is stable at large timesteps (no fast-spinning constraint solver).
 *   • It is easy to tune (just stiffness + damping per wheel).
 *   • Wheels never clip through ground or spin out of constraint.
 *   • It works on geometry that has no Jolt-collidable mesh (heightfields,
 *     procedural terrain) as long as you can raycast against it.
 *
 * ─── Spring-Damper Model ──────────────────────────────────────────────────
 * For each wheel, the vertical spring force is:
 *
 *   F_y = springStiffness * compression − springDamping * dCompression/dt
 *
 * where:
 *   compression     = suspensionRestLength − rayHitDistance   (>0 when compressed)
 *   dCompression/dt = (compression_thisFrame − compression_lastFrame) / dt
 *
 * All four suspension forces are summed and divided by vehicleMass to produce
 * a vertical acceleration that keeps the car above the ground.
 *
 * ─── Kinematic Drive ──────────────────────────────────────────────────────
 * Rather than modelling engine torque and tyre friction in full, we use a
 * "velocity-servo" drive model:
 *
 *   1. Compute targetSpeed = throttleInput * maxSpeed.
 *   2. Compute speedError  = targetSpeed − currentForwardSpeed.
 *   3. Apply driveForce = driveForce * speedError / maxSpeed (saturation).
 *   4. A lateral drag cancels sideways sliding (simulates tyre grip).
 *   5. Steering yaw rate ≈ forwardSpeed * tan(steerAngle) / wheelbase
 *      (Ackermann approximation — matches how most open-world games steer).
 *
 * ─── Collision Body ───────────────────────────────────────────────────────
 * VehicleSystem creates one Jolt box body for each vehicle chassis.  The body
 * is kinematic (not simulated by the solver); we move it each frame via
 * SetPosition so other physics objects can detect and react to the car.
 *
 * ─── Update Order ─────────────────────────────────────────────────────────
 *   Per frame (called from GameRuntime::Update or the vehicle_test scene):
 *   1. PhysicsWorld::Step(dt)     — advance Jolt simulation.
 *   2. VehicleSystem::Update(...) — for each VehicleComponent entity:
 *      a. Cast 4 wheel rays → compute spring forces.
 *      b. Apply gravity + suspension + drive + drag to velocity.
 *      c. Integrate position (pos += vel * dt).
 *      d. Update yaw from steer.
 *      e. Sync chassis box body position via PhysicsWorld::SetPosition.
 *      f. Write back to TransformComponent.
 *      g. Update speed for HUD / camera.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2025
 * C++ Standard: C++17
 * Platform: Windows (ENGINE_ENABLE_PHYSICS required)
 */

#pragma once

// TEACHING NOTE — Compile-time guard
// VehicleSystem depends on PhysicsWorld (Jolt Physics).  When built without
// Jolt (ENGINE_ENABLE_PHYSICS not defined), the system is compiled out entirely
// rather than failing with linker errors.  The same pattern is used for
// physics_world.cpp, rigid_body.cpp, etc.
#ifdef ENGINE_ENABLE_PHYSICS

#include "engine/ecs/ECS.hpp"
#include "engine/physics/physics_world.hpp"

namespace engine {
namespace vehicle {

// ===========================================================================
// VehicleSystem
// ===========================================================================

/**
 * @class VehicleSystem
 * @brief Drives all entities that have a VehicleComponent + TransformComponent.
 *
 * Lifecycle:
 *   Init(world, physicsWorld)  — creates chassis box bodies in Jolt.
 *   Update(world, physicsWorld, dt) — simulates one frame.
 *   Shutdown(world, physicsWorld)   — destroys chassis bodies.
 *
 * TEACHING NOTE — Stateless System Design
 * ─────────────────────────────────────────
 * VehicleSystem itself holds no per-vehicle state.  All vehicle state lives
 * in VehicleComponent (ECS data).  This follows the ECS principle: systems
 * contain logic, components contain data.  The system can be destroyed and
 * recreated without losing any simulation state.
 */
class VehicleSystem
{
public:
    VehicleSystem()  = default;
    ~VehicleSystem() = default;

    // Non-copyable (stateless but follows the project's copy-deletion pattern).
    VehicleSystem(const VehicleSystem&)            = delete;
    VehicleSystem& operator=(const VehicleSystem&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Create physics bodies for all entities with a VehicleComponent.
     *
     * Scans the World for entities that have both VehicleComponent and
     * TransformComponent, then creates a Jolt box body (chassis size
     * 2 × 0.8 × 4 m) at the entity's current position.  The body ID is
     * stored in VehicleComponent::physicsBodyID.
     *
     * @param world         ECS World containing vehicle entities.
     * @param physicsWorld  Physics simulation to create bodies in.
     */
    void Init(World& world, physics::PhysicsWorld& physicsWorld);

    /**
     * @brief Simulate one frame for all vehicle entities.
     *
     * Performs:
     *   1. Four wheel-ray suspension forces.
     *   2. Gravity + suspension spring integration.
     *   3. Throttle/brake drive force.
     *   4. Lateral drag (tyre grip model).
     *   5. Ackermann steering yaw rate.
     *   6. Fuel drain.
     *   7. Position integration and physics body sync.
     *   8. TransformComponent write-back.
     *
     * TEACHING NOTE — dt conventions
     * For a stable spring-damper at 60 FPS, springDamping should be tuned
     * at dt = 1/60 s.  Very large dt (> 1/10 s) will cause the spring to
     * overshoot.  In production games the physics step is fixed at 1/60 or
     * 1/120 s and the rendering uses interpolation for smooth visuals.
     *
     * @param world         ECS World.
     * @param physicsWorld  Physics world (for ray casts and body sync).
     * @param dt            Delta time in seconds (typically 1/60).
     */
    void Update(World& world, physics::PhysicsWorld& physicsWorld, float dt);

    /**
     * @brief Destroy all chassis physics bodies.
     *
     * @param world         ECS World.
     * @param physicsWorld  Physics world (bodies destroyed here).
     */
    void Shutdown(World& world, physics::PhysicsWorld& physicsWorld);
};

} // namespace vehicle
} // namespace engine

#endif // ENGINE_ENABLE_PHYSICS
