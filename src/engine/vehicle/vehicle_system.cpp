/**
 * @file vehicle_system.cpp
 * @brief VehicleSystem — wheel-ray suspension vehicle physics implementation.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2025
 */

#ifdef ENGINE_ENABLE_PHYSICS

#include "engine/vehicle/vehicle_system.hpp"
#include "engine/physics/physics_world.hpp"
#include "engine/core/Logger.hpp"

#include <cmath>      // std::sin, std::cos, std::tan, std::abs, std::max, std::min
#include <algorithm>  // std::max, std::min, std::copysign

namespace engine {
namespace vehicle {

using physics::PhysicsWorld;
using physics::RaycastHit;
using physics::kInvalidBodyID;
using math::Vec3;

// ============================================================================
// Implementation helpers (file-internal)
// ============================================================================
namespace {

// TEACHING NOTE — Gravity constant
// We match Jolt's default gravity (9.81 m/s² downward).
// Using a named constant here makes it easy to tune for an "arcade-y"
// feel (lower gravity = floatier) without hunting for magic numbers.
constexpr float kGravity        = 9.81f;

// TEACHING NOTE — Wheelbase
// The distance between the front and rear axles.  Used in the Ackermann
// steering approximation:   yawRate = speed * tan(steerAngle) / wheelbase.
// Matches wheelRestPositions: front Z = +1.4, rear Z = -1.4 → 2.8 m apart.
constexpr float kWheelbase      = 2.8f;   // metres between front and rear axles

// Chassis half-extents for the Jolt collision box body.
// 2 m wide, 0.4 m tall, 4 m long — a compact luxury sedan (Regalia-ish).
const Vec3 kChassisHalfExtents  { 1.0f, 0.4f, 2.0f };
constexpr float kChassisMass    = 1200.0f;  // kg (overridden by vehicleMass)

// Maximum vertical velocity magnitude (m/s).
// Clamped to prevent the spring from over-correcting in a single frame.
constexpr float kMaxVerticalVel = 20.0f;

/// Rotate a vehicle-local offset by the chassis yaw angle.
/// Returns the world-space offset from the chassis centre.
inline Vec3 LocalToWorld(const Vec3& localOffset, float yaw)
{
    // TEACHING NOTE — 2D rotation in the XZ plane
    // A rotation around the Y axis by angle `yaw` maps:
    //   x_world =  x_local * cos(yaw) + z_local * sin(yaw)
    //   z_world = -x_local * sin(yaw) + z_local * cos(yaw)
    // The y component is unchanged (vertical is world-up Y).
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);
    return {
        localOffset.x * c + localOffset.z * s,
        localOffset.y,
       -localOffset.x * s + localOffset.z * c
    };
}

/// Dot product of two Vec3s (convenience wrapper).
inline float Dot3(const Vec3& a, const Vec3& b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

} // anonymous namespace

// ============================================================================
// VehicleSystem::Init
// ============================================================================
void VehicleSystem::Init(World& world, PhysicsWorld& physicsWorld)
{
    LOG_INFO("VehicleSystem::Init — creating chassis bodies for vehicle entities.");

    // TEACHING NOTE — Iterating with World::View<A, B>()
    // View scans all living entities and calls the lambda only for those
    // that have BOTH VehicleComponent and TransformComponent.  This is an
    // O(n_entities) pass — efficient for a small number of vehicles.
    world.View<VehicleComponent, TransformComponent>(
        [&](EntityID eid, VehicleComponent& vc, TransformComponent& tc)
        {
            if (vc.physicsBodyID != kInvalidBodyID)
            {
                // TEACHING NOTE — Guard against double-Init
                // If Init() is called twice (e.g. on level reload) without
                // an intervening Shutdown(), we skip body creation to avoid
                // orphaned physics bodies and stale IDs.
                LOG_WARN("VehicleSystem::Init: entity " << eid
                         << " already has a physics body — skipping.");
                return;
            }

            // Create a kinematic box body at the chassis position.
            // TEACHING NOTE — Kinematic vs Dynamic body
            // A dynamic body is simulated by Jolt's constraint solver (forces,
            // impulses, gravity apply).  A kinematic body is moved explicitly by
            // the application each frame — it has a position and velocity but
            // is not subject to forces from the solver.
            //
            // We use a DYNAMIC body (isStatic=false) with a large mass so that
            // other physics objects (crates, debris) can react to being hit by
            // the car.  We then override its velocity every frame from our
            // spring-damper calculation, effectively making it "semi-kinematic".
            const Vec3 initPos { tc.position.x, tc.position.y, tc.position.z };
            const uint32_t bodyID = physicsWorld.CreateBox(
                initPos,
                kChassisHalfExtents,
                vc.vehicleMass,
                /*isStatic=*/ false);

            if (bodyID == kInvalidBodyID)
            {
                LOG_ERROR("VehicleSystem::Init: PhysicsWorld::CreateBox failed "
                          "for entity " << eid << ".");
                return;
            }

            vc.physicsBodyID = bodyID;
            LOG_INFO("VehicleSystem::Init: created chassis body " << bodyID
                     << " for vehicle entity " << eid << ".");
        });
}

// ============================================================================
// VehicleSystem::Update
// ============================================================================
void VehicleSystem::Update(World& world,
                            PhysicsWorld& physicsWorld,
                            float dt)
{
    // Guard against zero/negative dt (can occur on the first frame).
    if (dt <= 0.0f) return;

    world.View<VehicleComponent, TransformComponent>(
        [&](EntityID /*eid*/, VehicleComponent& vc, TransformComponent& tc)
        {
            // ----------------------------------------------------------------
            // STEP 1 — Build the current world-space position from Transform.
            // ----------------------------------------------------------------
            // TEACHING NOTE — TransformComponent::position uses the game-layer
            // Vec3 (defined in Types.hpp) while engine::math::Vec3 is the math
            // library type.  They are structurally identical (x, y, z floats)
            // but are distinct C++ types.  We copy the fields manually.
            Vec3 pos { tc.position.x, tc.position.y, tc.position.z };

            // ----------------------------------------------------------------
            // STEP 2 — Wheel-ray suspension
            // ----------------------------------------------------------------
            // TEACHING NOTE — Why cast from ABOVE the rest position?
            // The ray must start above the expected contact point so it can
            // detect both compressed and extended suspension.  We start
            // suspensionRestLength + 0.2 m above the wheel rest attachment
            // and cast down for 2 × restLength + 0.3 m.  This window covers:
            //   • Full compression: wheel hits at ~0.1 m below attachment
            //   • Full extension:   wheel hits at ~restLength + 0.2 m below
            // If the ray misses entirely, the wheel is in the air.
            float totalSuspForceY = 0.0f;
            int   groundedCount   = 0;

            for (int i = 0; i < 4; ++i)
            {
                auto& ws = vc.wheelStates[i];

                // Save last frame's compression for the damping derivative.
                ws.prevCompression = ws.compression;

                // Compute wheel attachment point in world space.
                const Vec3 localRest = vc.wheelRestPositions[i];
                const Vec3 worldOffset = LocalToWorld(localRest, vc.yaw);

                // Ray origin: directly above the wheel rest point.
                const float rayStartY = pos.y + worldOffset.y
                                        + vc.suspensionRestLength + 0.2f;
                const Vec3  rayOrigin {
                    pos.x + worldOffset.x,
                    rayStartY,
                    pos.z + worldOffset.z
                };
                const Vec3  rayDir    { 0.0f, -1.0f, 0.0f };
                const float rayLen    = vc.suspensionRestLength * 2.0f + 0.3f;

                RaycastHit hit;
                if (physicsWorld.Raycast(rayOrigin, rayDir, rayLen, hit))
                {
                    // TEACHING NOTE — Compression calculation
                    // hitDist is measured from rayOrigin.  We subtract the
                    // 0.2 m offset that we added to the ray origin so that
                    // compression = 0 when the wheel is exactly at rest length.
                    const float adjustedDist = hit.distance - 0.2f;
                    const float compression  =
                        vc.suspensionRestLength - adjustedDist;

                    ws.compression  = std::max(0.0f, compression);
                    ws.contactPoint = hit.position;
                    ws.isGrounded   = (ws.compression > 0.0f);

                    if (ws.isGrounded)
                    {
                        // TEACHING NOTE — Spring-damper force
                        // Derivative of compression ≈ finite difference:
                        //   dC/dt ≈ (C_now - C_prev) / dt
                        // The damping term opposes the spring velocity (it
                        // reduces oscillation but can also go negative when
                        // the wheel is rebounding — this is correct behaviour).
                        const float compressionVel =
                            (ws.compression - ws.prevCompression) / dt;
                        const float suspForce =
                            vc.springStiffness * ws.compression
                            - vc.springDamping * compressionVel;

                        // Only add positive (upward) forces; negative spring
                        // forces would push the car INTO the ground, which is
                        // incorrect for a suspension-only model.
                        totalSuspForceY += std::max(0.0f, suspForce);
                        ++groundedCount;
                    }
                }
                else
                {
                    ws.compression  = 0.0f;
                    ws.isGrounded   = false;
                }
            }

            // ----------------------------------------------------------------
            // STEP 3 — Gravity and suspension integration
            // ----------------------------------------------------------------
            // TEACHING NOTE — Semi-implicit integration
            // We update velocity first, then integrate position:
            //   v(t+dt) = v(t) + a(t) * dt
            //   p(t+dt) = p(t) + v(t+dt) * dt
            // This is *semi-implicit Euler* (also called symplectic Euler).
            // It is more energy-conservative than explicit Euler and is the
            // standard for game physics.

            // Gravity (always applied).
            vc.velocity.y -= kGravity * dt;

            // Net suspension acceleration = total upward force / mass.
            if (groundedCount > 0)
            {
                const float suspAccY = (totalSuspForceY / vc.vehicleMass) * dt;
                vc.velocity.y += suspAccY;
            }

            // Clamp vertical velocity to prevent tunnelling or explosion.
            vc.velocity.y = std::max(-kMaxVerticalVel,
                            std::min( kMaxVerticalVel, vc.velocity.y));

            // ----------------------------------------------------------------
            // STEP 4 — Throttle / brake drive force (only when grounded)
            // ----------------------------------------------------------------
            // TEACHING NOTE — Forward axis from yaw
            // The vehicle's forward direction in world space is a unit vector
            // in the XZ plane rotated by the current yaw angle:
            //   fwd.x = sin(yaw),  fwd.z = cos(yaw)
            // (Z+ is world "forward" at yaw=0; positive yaw rotates right.)
            const float cosYaw = std::cos(vc.yaw);
            const float sinYaw = std::sin(vc.yaw);
            const Vec3  fwd    { sinYaw,  0.0f, cosYaw };
            const Vec3  right  { cosYaw,  0.0f, -sinYaw };

            if (groundedCount >= 2)
            {
                // Project current velocity onto the forward direction.
                const float fwdSpeed = Dot3(vc.velocity, fwd);

                // TEACHING NOTE — Velocity-servo drive
                // We use a simple proportional controller: the drive force is
                // proportional to the difference between the desired speed and
                // the current forward speed.  This prevents the car from
                // accelerating past maxSpeed and gives natural-feeling
                // deceleration when the throttle is released.
                const float desiredSpeed = vc.throttleInput * vc.maxSpeed;
                const float speedError   = desiredSpeed - fwdSpeed;
                const float driveAccel   = (vc.driveForce / vc.vehicleMass)
                                           * (speedError / vc.maxSpeed);
                vc.velocity.x += fwd.x * driveAccel * dt;
                vc.velocity.z += fwd.z * driveAccel * dt;

                // TEACHING NOTE — Brake force
                // Braking applies a deceleration force opposing current motion.
                // We multiply by a fixed deceleration (~0.8g) scaled by input.
                if (vc.brakeInput > 0.0f && std::abs(fwdSpeed) > 0.1f)
                {
                    const float brakeDecel = 8.0f * vc.brakeInput;
                    const float brakeSign  = (fwdSpeed > 0.0f) ? 1.0f : -1.0f;
                    vc.velocity.x -= fwd.x * brakeSign * brakeDecel * dt;
                    vc.velocity.z -= fwd.z * brakeSign * brakeDecel * dt;
                }
            }
            else
            {
                // TEACHING NOTE — Airborne throttle
                // When all wheels are off the ground (e.g. cresting a hill
                // or mid-jump) we apply a much smaller drive force so the car
                // doesn't rocket away in mid-air.
                const float fwdSpeed     = Dot3(vc.velocity, fwd);
                const float desiredSpeed = vc.throttleInput * vc.maxSpeed;
                const float speedError   = desiredSpeed - fwdSpeed;
                const float airDriveAcc  = (vc.driveForce / vc.vehicleMass)
                                           * (speedError / vc.maxSpeed) * 0.05f;
                vc.velocity.x += fwd.x * airDriveAcc * dt;
                vc.velocity.z += fwd.z * airDriveAcc * dt;
            }

            // ----------------------------------------------------------------
            // STEP 5 — Lateral drag (tyre grip model)
            // ----------------------------------------------------------------
            // TEACHING NOTE — Lateral drag simulates tyre friction
            // A real car resists sideways sliding because the tyres provide
            // a large cornering force.  We approximate this by reducing the
            // lateral velocity component each frame.
            //
            // The grip factor is high when grounded (gripFactor ≈ 0.90 at 60 fps)
            // and near-zero when airborne (gripFactor ≈ 0.01) — cars slip freely
            // in the air.  The effective lateral deceleration per frame is:
            //
            //   lateralV_next = lateralV * (1 - gripFactor)
            //
            // With gripFactor = 0.90 and dt = 1/60, the car loses 90% of its
            // lateral velocity in one frame — very stiff grip.  Reduce to 0.5–0.7
            // for a drift-car feel.
            {
                const float gripFactor = (groundedCount >= 2) ? 0.90f : 0.01f;
                const float lateralVel = Dot3(vc.velocity, right);
                vc.velocity.x -= right.x * lateralVel * gripFactor;
                vc.velocity.z -= right.z * lateralVel * gripFactor;
            }

            // ----------------------------------------------------------------
            // STEP 6 — Steering (Ackermann yaw rate)
            // ----------------------------------------------------------------
            // TEACHING NOTE — Ackermann Steering Approximation
            // In a real car, each wheel has a slightly different turn radius
            // (Ackermann geometry).  We simplify to a bicycle model:
            //
            //   yawRate = (forwardSpeed / wheelbase) * tan(steerAngle)
            //
            // The steer angle scales linearly with steerInput, but we reduce
            // it at high speed (speed-sensitive steering) so the car doesn't
            // spin out at highway speed (common in most racing games).
            if (groundedCount >= 2)
            {
                const float fwdSpeed = Dot3(vc.velocity, fwd);

                // Speed-sensitive steering: full angle below 5 m/s, reduces
                // to 30% at maxSpeed.  Prevents snap oversteer at high speed.
                const float speedRatio  = std::min(1.0f,
                                          std::abs(fwdSpeed) / vc.maxSpeed);
                const float steerScale  = 1.0f - 0.7f * speedRatio;
                const float steerAngle  = vc.steerInput
                                          * vc.maxSteerAngle
                                          * steerScale;
                // yawRate = forwardSpeed * tan(steer) / wheelbase
                const float yawRate     = fwdSpeed
                                          * std::tan(steerAngle)
                                          / kWheelbase;
                vc.yaw += yawRate * dt;
            }

            // ----------------------------------------------------------------
            // STEP 7 — Fuel consumption
            // ----------------------------------------------------------------
            // TEACHING NOTE — Fuel model
            // Fuel only drains when the vehicle is occupied AND throttle is
            // applied.  In FFXV running out of fuel strands the party on the
            // road and triggers the Cindy tow-truck scene.
            if (vc.isOccupied && vc.fuel > 0.0f)
            {
                const float drain = std::abs(vc.throttleInput)
                                    * vc.fuelConsumption * dt;
                vc.fuel = std::max(0.0f, vc.fuel - drain);
                if (vc.fuel == 0.0f)
                    vc.needsFuel = true;
            }

            // ----------------------------------------------------------------
            // STEP 8 — Integrate position
            // ----------------------------------------------------------------
            pos.x += vc.velocity.x * dt;
            pos.y += vc.velocity.y * dt;
            pos.z += vc.velocity.z * dt;

            // ----------------------------------------------------------------
            // STEP 9 — Sync physics body position
            // ----------------------------------------------------------------
            // TEACHING NOTE — Why sync instead of reading from physics?
            // We are driving the body kinematics from our own spring-damper
            // calculations, so the Jolt body's computed position would be
            // stale after our velocity override.  We write our integrated
            // position back into Jolt so its broadphase AABB and collision
            // queries (from other raycasts/body pairs) see the correct car
            // position.
            if (vc.physicsBodyID != kInvalidBodyID)
            {
                physicsWorld.SetPosition(vc.physicsBodyID, pos);
                physicsWorld.SetLinearVelocity(vc.physicsBodyID, vc.velocity);
            }

            // ----------------------------------------------------------------
            // STEP 10 — Write position and heading back to TransformComponent.
            // ----------------------------------------------------------------
            tc.position.x = pos.x;
            tc.position.y = pos.y;
            tc.position.z = pos.z;

            // TEACHING NOTE — Storing yaw in TransformComponent::rotation
            // TransformComponent::rotation is a game-layer Vec3 of Euler angles
            // in DEGREES (pitch=x, yaw=y, roll=z).  We convert our radian yaw
            // to degrees and write it to the Y channel.  The renderer (and
            // CameraSystem) uses TransformComponent::Forward() which reads
            // rotation.y in degrees internally.
            //
            // We store the canonical radian yaw in VehicleComponent so that
            // steering updates are precise (no rounding from degree conversion).
            //
            // TEACHING NOTE — Using engine::math::kPi (single source of truth)
            // Rather than hard-coding 180 / 3.14159..., we use the named
            // constant from math_types.hpp.  This ensures every file uses the
            // same precision for π.
            constexpr float kRadToDeg = 180.0f / engine::math::kPi;
            tc.rotation.y = vc.yaw * kRadToDeg;

            // Update the speed field (horizontal only — no vertical component).
            vc.speed = std::sqrt(vc.velocity.x * vc.velocity.x
                               + vc.velocity.z * vc.velocity.z);
        }); // View lambda end
}

// ============================================================================
// VehicleSystem::Shutdown
// ============================================================================
void VehicleSystem::Shutdown(World& world, PhysicsWorld& physicsWorld)
{
    LOG_INFO("VehicleSystem::Shutdown — destroying chassis bodies.");

    world.View<VehicleComponent, TransformComponent>(
        [&](EntityID eid, VehicleComponent& vc, TransformComponent& /*tc*/)
        {
            if (vc.physicsBodyID != kInvalidBodyID)
            {
                physicsWorld.DestroyBody(vc.physicsBodyID);
                LOG_INFO("VehicleSystem::Shutdown: destroyed body "
                         << vc.physicsBodyID << " for entity " << eid << ".");
                vc.physicsBodyID = kInvalidBodyID;
            }
        });
}

} // namespace vehicle
} // namespace engine

#endif // ENGINE_ENABLE_PHYSICS
