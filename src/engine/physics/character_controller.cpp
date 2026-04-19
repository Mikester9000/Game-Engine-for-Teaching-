/**
 * @file character_controller.cpp
 * @brief CharacterController — Jolt CharacterVirtual wrapper.
 *
 * ============================================================================
 * TEACHING NOTE — CharacterVirtual Deep Dive
 * ============================================================================
 *
 * JPH::CharacterVirtual solves character movement with a "predict, move,
 * resolve" loop:
 *
 *   1. Predict contacts: shape-cast the capsule in the desired move direction
 *      looking ahead by mPredictiveContactDistance (default 0.1 m).
 *
 *   2. Resolve penetration: if the capsule is already overlapping geometry,
 *      push it out along the contact normal at mPenetrationRecoverySpeed
 *      (default 1.0 m/s).
 *
 *   3. Step up: attempt to step over ledges up to mMaxStepHeight (we set
 *      this to 0.35 m = typical stair riser height in FFXV).
 *
 *   4. Move: apply the resolved velocity to the capsule position.
 *
 *   5. Classify ground: query the surface directly below the capsule to
 *      determine OnGround, OnSteepGround, or InAir state.
 *
 * The result is stable, jitter-free movement that handles typical game
 * geometry without manual tuning of per-contact friction coefficients.
 *
 * ─── Coordinate convention ──────────────────────────────────────────────
 * This engine uses Y-up.  Gravity is -Y.  Forward is +Z (like D3D/HLSL).
 * Jolt uses the same Y-up convention by default.
 *
 * ─── Layer used for the character ───────────────────────────────────────
 * The character body uses kLayerMoving so it collides with both static
 * geometry (floors, walls) and other moving bodies (dynamic crates).
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Platform: All
 */

#ifdef ENGINE_ENABLE_PHYSICS

#include "engine/physics/physics_impl.hpp"
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterMask.h>

#include "engine/physics/character_controller.hpp"
#include "engine/physics/physics_world.hpp"
#include "engine/core/Logger.hpp"

#include <cmath>    // std::sqrt
#include <algorithm>

// ---------------------------------------------------------------------------
// pImpl definition
// ---------------------------------------------------------------------------
namespace engine::physics::detail {

struct CharacterImpl
{
    JPH::Ref<JPH::CharacterVirtual> character;
};

} // namespace engine::physics::detail

namespace engine {
namespace physics {

CharacterController::CharacterController()
    : m_impl(std::make_unique<detail::CharacterImpl>())
{}

CharacterController::~CharacterController()
{
    // Jolt::Ref handles cleanup of the CharacterVirtual.
}

// ---------------------------------------------------------------------------
bool CharacterController::Init(PhysicsWorld& world,
                                math::Vec3   startPos,
                                float        halfHeight,
                                float        radius)
{
    if (!world.GetImpl())
    {
        LOG_ERROR("[CharacterController] PhysicsWorld not initialised.");
        return false;
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — CapsuleShape for the character
    // -----------------------------------------------------------------------
    // We create a CapsuleShape and wrap it in a RotatedTranslatedShape to
    // offset the capsule origin to the character's feet rather than its centre.
    //
    // Jolt's capsule is centred at the origin.  By translating it up by
    // (halfHeight + radius), the capsule's bottom hemisphere sits at Y=0,
    // which corresponds to the character's foot position.  This makes
    // GetPosition() return the foot position — convenient for ground
    // detection and surface snapping.
    // -----------------------------------------------------------------------
    JPH::CapsuleShapeSettings capsuleSettings(halfHeight, radius);
    auto capsuleResult = capsuleSettings.Create();
    if (capsuleResult.HasError())
    {
        LOG_ERROR("[CharacterController] CapsuleShape failed: "
                  << capsuleResult.GetError());
        return false;
    }

    // Offset so the bottom of the capsule is at position Y (foot level).
    const float capsuleCentreY = halfHeight + radius;
    JPH::RotatedTranslatedShapeSettings rtSettings(
        JPH::Vec3(0.0f, capsuleCentreY, 0.0f),
        JPH::Quat::sIdentity(),
        capsuleResult.Get()
    );
    auto rtResult = rtSettings.Create();
    if (rtResult.HasError())
    {
        LOG_ERROR("[CharacterController] RotatedTranslatedShape failed: "
                  << rtResult.GetError());
        return false;
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — CharacterVirtualSettings
    // -----------------------------------------------------------------------
    // Key tuning parameters:
    //   mMaxSlopeAngle             — 50° gives natural FF15 feel (walk most slopes).
    //   mMaxStrength               — Push force against dynamic bodies (500 N).
    //   mMass                      — Used for reaction forces only (not gravity).
    //   mPenetrationRecoverySpeed  — How quickly overlap is resolved (0.5 m/s).
    //   mPredictiveContactDistance — Probe ahead 0.1 m for smooth pre-contact.
    //   mMaxNumHits                — Max contacts per step (256 is generous).
    // -----------------------------------------------------------------------
    JPH::CharacterVirtualSettings settings;
    settings.mShape                   = rtResult.Get();
    settings.mMaxSlopeAngle           = JPH::DegreesToRadians(50.0f);
    settings.mMaxStrength             = 500.0f;
    settings.mMass                    = 80.0f;        // ~adult human mass
    settings.mPenetrationRecoverySpeed= 0.5f;
    settings.mPredictiveContactDistance= 0.1f;
    settings.mMaxNumHits              = 256u;
    settings.mUp                      = JPH::Vec3::sAxisY();

    auto& physSys = world.GetImpl()->physicsSystem;
    m_impl->character = new JPH::CharacterVirtual(
        &settings,
        JPH::RVec3(startPos.x, startPos.y, startPos.z),
        JPH::Quat::sIdentity(),
        0,               // userData
        physSys.get()
    );

    m_velocity    = { 0.0f, 0.0f, 0.0f };
    m_initialised = true;

    LOG_INFO("[CharacterController] Initialised at ("
             << startPos.x << ", " << startPos.y << ", " << startPos.z << ").");
    return true;
}

// ---------------------------------------------------------------------------
void CharacterController::Shutdown(PhysicsWorld& /*world*/)
{
    // TEACHING NOTE — CharacterVirtual cleanup
    // CharacterVirtual is reference-counted via JPH::Ref<>.
    // Resetting the Ref releases the reference; Jolt deletes the object
    // when the count reaches zero.  No explicit "remove from world" call
    // is needed because CharacterVirtual is not a true rigid body.
    m_impl->character = nullptr;
    m_initialised     = false;
}

// ---------------------------------------------------------------------------
void CharacterController::Update(PhysicsWorld&    world,
                                  float            dt,
                                  math::Vec3       movementInput,
                                  bool             jump)
{
    if (!m_initialised || !m_impl->character) return;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Applying Gravity
    // -----------------------------------------------------------------------
    // CharacterVirtual does NOT automatically apply gravity — that is the
    // game code's responsibility.  We accumulate a vertical velocity
    // component and add it to the horizontal input each frame.
    //
    // When grounded we reset the vertical velocity to a small negative value
    // (−0.1 m/s) to keep the character "pressed" against the ground, which
    // helps the ground detection stay reliable on gentle slopes.
    // -----------------------------------------------------------------------
    const bool grounded = IsGrounded();

    if (grounded)
    {
        // Reset vertical velocity (small downward value to stay grounded).
        m_velocity.y = -0.1f;

        if (jump)
        {
            // TEACHING NOTE — Jump Impulse
            // We set the vertical velocity directly to kJumpImpulse.
            // This is an instantaneous impulse (v = sqrt(2gh) for max height h).
            // At kJumpImpulse = 5.0 m/s and g = 9.81 m/s², peak height ≈ 1.27 m.
            m_velocity.y = kJumpImpulse;
        }
    }
    else
    {
        // Airborne: integrate gravity.
        m_velocity.y -= kGravity * dt;
    }

    // Combine horizontal input with vertical velocity.
    const JPH::Vec3 desiredVelocity(
        movementInput.x + 0.0f,   // X from input
        m_velocity.y,              // Y from gravity/jump accumulator
        movementInput.z + 0.0f    // Z from input
    );

    // -----------------------------------------------------------------------
    // TEACHING NOTE — CharacterVirtual::Update() parameters
    // -----------------------------------------------------------------------
    // deltaTime    — time step
    // gravity      — world-space gravity vector (overrides built-in gravity)
    // settings     — per-update overrides (nullptr = use creation settings)
    // inCenterOfMassTransform — used for broad-phase; pass identity
    // broadPhaseLayerFilter   — which broadphase layers to test
    // objectLayerFilter       — which object layers to test
    // bodyFilter              — per-body filter (nullptr = collide with all)
    // shapeFilter             — per-shape filter (nullptr = all shapes)
    // allocator               — temp allocator from the PhysicsSystem
    //
    // We pass nullptr for the optional filters — the character collides with
    // all static and moving bodies.
    // -----------------------------------------------------------------------
    auto& impl = *world.GetImpl();

    // TEACHING NOTE — BroadPhaseLayerFilter and ObjectLayerFilter
    // We want the character to collide with everything, so we use the
    // default "allow all" filters provided by Jolt.
    JPH::DefaultBroadPhaseLayerFilter bpFilter(
        impl.objectVsBroadPhaseFilter,
        static_cast<JPH::ObjectLayer>(PhysicsWorld::kLayerMoving)
    );
    JPH::DefaultObjectLayerFilter objFilter(
        impl.objectLayerPairFilter,
        static_cast<JPH::ObjectLayer>(PhysicsWorld::kLayerMoving)
    );

    m_impl->character->SetLinearVelocity(desiredVelocity);

    m_impl->character->Update(
        dt,
        impl.physicsSystem->GetGravity(),
        bpFilter,
        objFilter,
        JPH::BodyFilter{},         // collide with all bodies
        JPH::ShapeFilter{},        // collide with all shapes
        *impl.tempAllocator
    );

    // Update our cached velocity from the character's resolved velocity.
    const JPH::Vec3 resolved = m_impl->character->GetLinearVelocity();
    m_velocity = { resolved.GetX(), resolved.GetY(), resolved.GetZ() };
}

// ---------------------------------------------------------------------------
math::Vec3 CharacterController::GetPosition() const
{
    if (!m_initialised || !m_impl->character)
        return { 0.0f, 0.0f, 0.0f };

    JPH::RVec3 p = m_impl->character->GetPosition();
    return { static_cast<float>(p.GetX()),
             static_cast<float>(p.GetY()),
             static_cast<float>(p.GetZ()) };
}

bool CharacterController::IsGrounded() const
{
    if (!m_initialised || !m_impl->character)
        return false;

    // TEACHING NOTE — GroundState
    // EOnGround       → standing on a walkable surface
    // EOnSteepGround  → touching a surface too steep to walk (slope > maxSlopeAngle)
    // ENotSupported   → no contact (CharacterVirtualSettings::mSupportingVolume)
    // EInAir          → no ground contact at all
    return m_impl->character->GetGroundState()
           == JPH::CharacterVirtual::EGroundState::OnGround;
}

math::Vec3 CharacterController::GetVelocity() const
{
    return m_velocity;
}

void CharacterController::SetPosition(PhysicsWorld& /*world*/, math::Vec3 pos)
{
    if (!m_initialised || !m_impl->character) return;
    m_impl->character->SetPosition(JPH::RVec3(pos.x, pos.y, pos.z));
}

} // namespace physics
} // namespace engine

#endif // ENGINE_ENABLE_PHYSICS
