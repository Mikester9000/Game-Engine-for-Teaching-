/**
 * @file character_controller.hpp
 * @brief CharacterController — capsule-based kinematic character controller.
 *
 * ============================================================================
 * TEACHING NOTE — What is a Character Controller?
 * ============================================================================
 * In games like FFXV, the player character (Noctis) needs special collision
 * behaviour that a plain rigid body cannot provide cleanly:
 *
 *   • Gravity   — character falls under gravity when airborne.
 *   • Step-up   — smoothly climbs over small ledges (0.25 m) without
 *                 bouncing or getting stuck.
 *   • Slopes    — slides along sloped surfaces up to a max angle; stops
 *                 on steep slopes (e.g. vertical walls).
 *   • No spin   — remains upright; never tumbles like a dynamic body.
 *   • Jump      — player-initiated vertical impulse.
 *   • Grounded  — accurate IsGrounded() query (for jump gating, footstep
 *                 sounds, landing animations).
 *
 * A plain rigid body would spin when bumped, bounce off walls unpredictably,
 * and require complex constraint fixes to stay upright.
 *
 * ─── Jolt's CharacterVirtual ─────────────────────────────────────────────
 * Jolt Physics provides JPH::CharacterVirtual, a "virtual" (non-simulated)
 * character that has no mass in the solver but uses shape-cast queries to
 * detect and respond to obstacles each Update() call.
 *
 *   vs. JPH::Character (rigid body with constraints):
 *     • CharacterVirtual: cheaper, more predictable, easier to tune.
 *     • Character (rigid body): reacts to explosions/physics, but wobbles.
 *   FFXV-style games almost always use the virtual type for the player.
 *
 * ─── CharacterVirtualSettings key fields ─────────────────────────────────
 *   shape             — CapsuleShape (halfHeight, radius).
 *   maxSlopeAngle     — Walk angle limit (default 50°); steeper = slide.
 *   maxStrength       — Force the character can exert on dynamic bodies.
 *   mass              — Used for push-back on dynamic bodies.
 *   predictiveContactDistance — How far ahead to probe for contacts.
 *   penetrationRecoverySpeed  — How fast to resolve overlap (0 = instant).
 *
 * ─── pImpl Pattern ──────────────────────────────────────────────────────
 * CharacterController uses a pImpl pointer (Impl forward-declared here) so
 * Jolt headers are included only in character_controller.cpp, keeping
 * compile times short for all other engine files.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Platform: All (Jolt is cross-platform)
 */

#pragma once

#include "engine/math/math_types.hpp"

#include <cstdint>
#include <memory>

namespace engine {
namespace physics {

// Forward declarations — no Jolt headers needed in this file.
class PhysicsWorld;

namespace detail { struct CharacterImpl; }

// ===========================================================================
// CharacterController
// ===========================================================================

/**
 * @class CharacterController
 * @brief Drives the player (or NPC) capsule through the physics world.
 *
 * ============================================================================
 * TEACHING NOTE — CharacterController Lifecycle
 * ============================================================================
 *
 * 1. Call Init() once per entity.  Creates the JPH::CharacterVirtual and
 *    positions it at startPos.
 *
 * 2. Each frame:
 *    a. Compute a desired velocity from player input (WASD + sprint/jump).
 *    b. Call Update(world, dt, desiredVelocity, wantJump).
 *       Internally this calls CharacterVirtual::Update() which:
 *         i.  Applies gravity to the vertical velocity component.
 *         ii. Shape-casts the capsule in the movement direction.
 *         iii.Steps over ledges up to mMaxStepHeight (default 0.4 m).
 *         iv. Slides along walls and slopes.
 *         v.  Resolves penetration with mPenetrationRecoverySpeed.
 *    c. Read GetPosition() and write it into TransformComponent for rendering.
 *    d. Use IsGrounded() to gate jump triggers and landing animations.
 *
 * 3. Call Shutdown() when the entity is destroyed (removes the body).
 *
 * ─── Units ──────────────────────────────────────────────────────────────
 * Jolt uses SI units (metres, seconds, kg).  FFXV's open world is in metres.
 * Typical parameters:
 *   halfHeight  = 0.85 m   (1.7 m total character height)
 *   radius      = 0.3  m   (0.6 m diameter capsule)
 *   jumpImpulse = 5.0  m/s  (gives ~1.3 m peak jump height)
 *
 * ============================================================================
 */
class CharacterController
{
public:
    // -----------------------------------------------------------------------
    // Configuration constants
    // -----------------------------------------------------------------------

    /// Capsule half-height of the cylinder section (metres).
    static constexpr float kDefaultHalfHeight = 0.85f;

    /// Capsule radius (metres).
    static constexpr float kDefaultRadius = 0.30f;

    /// Gravity magnitude (m/s²) applied per-frame when airborne.
    static constexpr float kGravity = 9.81f;

    /// Vertical velocity applied on Jump().
    static constexpr float kJumpImpulse = 5.0f;

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    CharacterController();
    ~CharacterController();

    CharacterController(const CharacterController&)            = delete;
    CharacterController& operator=(const CharacterController&) = delete;

    /**
     * @brief Create and position the character capsule.
     *
     * @param world       The PhysicsWorld to create the character in.
     * @param startPos    Initial world-space foot position (centre of capsule base).
     * @param halfHeight  Cylinder half-height (metres).
     * @param radius      Capsule radius (metres).
     * @return True on success.
     */
    bool Init(PhysicsWorld& world,
              math::Vec3   startPos,
              float        halfHeight = kDefaultHalfHeight,
              float        radius     = kDefaultRadius);

    /**
     * @brief Remove the character from the physics world.
     * @param world  The PhysicsWorld the character was created in.
     */
    void Shutdown(PhysicsWorld& world);

    // -----------------------------------------------------------------------
    // Per-frame update
    // -----------------------------------------------------------------------

    /**
     * @brief Advance the character simulation by dt seconds.
     *
     * @param world          The PhysicsWorld.
     * @param dt             Elapsed seconds since last frame.
     * @param movementInput  Desired XZ velocity (m/s).  Y component ignored.
     * @param jump           If true and IsGrounded(), apply a jump impulse.
     */
    void Update(PhysicsWorld&    world,
                float            dt,
                math::Vec3       movementInput,
                bool             jump = false);

    // -----------------------------------------------------------------------
    // State queries
    // -----------------------------------------------------------------------

    /**
     * @brief Get the current world-space position (centre of capsule).
     */
    math::Vec3 GetPosition() const;

    /**
     * @brief Returns true if the character is standing on solid ground.
     *
     * TEACHING NOTE — IsGrounded states
     * Jolt's CharacterVirtual reports one of three states:
     *   OnGround       — standing on a surface ≤ maxSlopeAngle.
     *   OnSteepGround  — standing on a slope > maxSlopeAngle (slides down).
     *   InAir          — no contact below.
     * We return true only for OnGround (suitable for jump gating).
     */
    bool IsGrounded() const;

    /**
     * @brief Get the current velocity of the character (m/s).
     */
    math::Vec3 GetVelocity() const;

    /**
     * @brief Teleport the character to a new position.
     * @param world  The PhysicsWorld.
     * @param pos    New world-space position.
     */
    void SetPosition(PhysicsWorld& world, math::Vec3 pos);

private:
    std::unique_ptr<detail::CharacterImpl> m_impl;  ///< pImpl — Jolt objects inside.
    math::Vec3 m_velocity { 0.0f, 0.0f, 0.0f };     ///< Accumulated velocity (m/s).
    bool       m_initialised = false;
};

} // namespace physics
} // namespace engine
