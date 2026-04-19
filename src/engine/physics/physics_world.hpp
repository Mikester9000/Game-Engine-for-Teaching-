/**
 * @file physics_world.hpp
 * @brief PhysicsWorld — Jolt Physics engine wrapper.
 *
 * ============================================================================
 * TEACHING NOTE — What is a Physics Engine?
 * ============================================================================
 * A physics engine simulates the behaviour of physical objects in a virtual
 * world.  For a game like Final Fantasy XV, physics handles:
 *
 *   • Gravity          — characters and objects fall to the ground.
 *   • Collision        — characters don't walk through walls or floors.
 *   • Raycasts         — "line of sight" queries; ground detection.
 *   • Rigid bodies     — dynamic objects (crates, boulders) react to forces.
 *   • Character capsule — the player moves along slopes and over ledges.
 *   • Hit volumes      — attack shapes overlap hurt shapes for combat.
 *
 * ─── Why Jolt Physics? ──────────────────────────────────────────────────────
 * Jolt Physics (MIT licence, by Jorrit Rouwe) is a modern deterministic
 * multi-threaded physics library used in Horizon Forbidden West.  It is:
 *
 *   • Cross-platform: Windows, Linux, macOS, consoles.
 *   • Cache-friendly: uses a SoA (Structure of Arrays) internal layout.
 *   • Deterministic: fixed-precision simulation for replay / network sync.
 *   • Well-documented: the source ships with extensive comments.
 *
 * ─── Architecture: Wrapper Pattern ─────────────────────────────────────────
 * PhysicsWorld wraps the raw Jolt API behind a simple engine-friendly
 * interface.  Engine code (ECS systems, gameplay) uses PhysicsWorld methods
 * (uint32_t body IDs, math::Vec3 positions) without seeing Jolt types.
 * Only the physics .cpp files include <Jolt/…> headers.
 *
 * This is the *Facade* design pattern — one of the most useful tools for
 * managing third-party library coupling.
 *
 * ─── Body ID Strategy ───────────────────────────────────────────────────────
 * Jolt uses JPH::BodyID, which is a uint32_t holding an index+sequence.
 * We expose that uint32_t directly in the public API so callers never
 * need to include Jolt headers.  0xFFFFFFFF is the "invalid" sentinel.
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

// ---------------------------------------------------------------------------
// Forward declarations — pImpl pattern keeps Jolt headers out of this header.
// ---------------------------------------------------------------------------
// TEACHING NOTE — pImpl (Pointer to IMPLementation) Pattern
// By forward-declaring PhysicsWorldImpl and storing it as a unique_ptr<Impl>,
// callers who #include this header never see the Jolt types.  This means:
//   1. Compile times are shorter — Jolt headers are only parsed in .cpp files.
//   2. The public interface is stable — changing Jolt internals doesn't force
//      a recompile of all consumers of this header.
//   3. The ABI is stable — adding fields to Impl doesn't change sizeof(World).
// ---------------------------------------------------------------------------
namespace engine::physics::detail { struct PhysicsWorldImpl; }

namespace engine {
namespace physics {

// ===========================================================================
// Constants
// ===========================================================================

/// Sentinel value: the body ID returned / expected when no body is present.
static constexpr uint32_t kInvalidBodyID = 0xFFFFFFFFu;

// ===========================================================================
// RaycastHit
// ===========================================================================

/**
 * @struct RaycastHit
 * @brief Output of a successful ray cast query.
 *
 * TEACHING NOTE — Raycast Anatomy
 * A raycast fires a mathematical ray (origin + direction × length) through the
 * physics world and returns the *first* solid surface it touches.  The result
 * contains:
 *   • distance   — how far along the ray the hit occurred (0 = at origin).
 *   • position   — world-space contact point.
 *   • normal     — outward surface normal at the contact point.
 *   • bodyID     — which body was hit (uint32_t = Jolt BodyID opaque value).
 *
 * Typical uses:
 *   • Ground detection for character controller.
 *   • Projectile hit detection.
 *   • Line-of-sight checks for AI.
 */
struct RaycastHit
{
    bool          hit      = false;                     ///< True if the ray struck something.
    float         distance = 0.0f;                      ///< Distance from origin to hit point.
    math::Vec3    position { 0.0f, 0.0f, 0.0f };       ///< World-space hit position.
    math::Vec3    normal   { 0.0f, 1.0f, 0.0f };       ///< Surface normal at hit point.
    uint32_t      bodyID   = kInvalidBodyID;            ///< ID of the body that was hit.
};

// ===========================================================================
// PhysicsWorld
// ===========================================================================

/**
 * @class PhysicsWorld
 * @brief Owns and drives the Jolt Physics simulation.
 *
 * ============================================================================
 * TEACHING NOTE — Physics World Lifecycle
 * ============================================================================
 *
 * 1. Call Init() once at scene startup.
 *    This allocates the TempAllocator, JobSystem, and PhysicsSystem.
 *    Without Init() all other methods are undefined behaviour.
 *
 * 2. Each game frame, call Step(dt).
 *    dt is the elapsed seconds since the last frame.
 *    Jolt advances the simulation by exactly that time, stepping up to
 *    kMaxSubSteps sub-steps internally for stability.
 *
 * 3. After Step(), read body positions via GetPosition() and copy them
 *    into your ECS TransformComponent to update the visible scene.
 *
 * 4. Call Shutdown() when the scene is destroyed.
 *    This removes all bodies and frees the Jolt objects.
 *
 * ─── Object Layers ──────────────────────────────────────────────────────────
 * Jolt uses "object layers" to control which objects collide with which.
 * We define two layers:
 *
 *   kLayerNonMoving (0) — static geometry: floors, walls, terrain.
 *   kLayerMoving    (1) — dynamic objects: characters, projectiles, crates.
 *
 * Moving objects collide with everything.
 * Non-moving objects only collide with moving objects (not each other).
 * This is the standard two-layer setup described in the Jolt samples.
 *
 * ─── Thread Safety ──────────────────────────────────────────────────────────
 * PhysicsWorld is NOT thread-safe.  Create/destroy bodies and call Step()
 * from the same thread (typically the main game thread).  Jolt's *internal*
 * update is multi-threaded via its JobSystem; external calls are single-thread.
 *
 * ============================================================================
 */
class PhysicsWorld
{
public:
    // -----------------------------------------------------------------------
    // Object layer constants (exposed as uint values for ECS components)
    // -----------------------------------------------------------------------

    /// Object layer for static geometry (non-moving bodies: floors, walls).
    static constexpr uint32_t kLayerNonMoving = 0u;

    /// Object layer for dynamic objects (moving bodies: characters, crates).
    static constexpr uint32_t kLayerMoving    = 1u;

    // -----------------------------------------------------------------------
    // Simulation constants
    // -----------------------------------------------------------------------

    /// Default gravity magnitude (m/s²) — matches Earth gravity in FF15 style.
    static constexpr float kDefaultGravity = 9.81f;

    /// Maximum sub-steps per Step() call.  Prevents "tunnelling" at low FPS.
    static constexpr int kMaxSubSteps = 4;

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    PhysicsWorld();
    ~PhysicsWorld();

    // Non-copyable: one world per scene.
    PhysicsWorld(const PhysicsWorld&)            = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    /**
     * @brief Initialise the physics world.
     *
     * Allocates:
     *   - 10 MB TempAllocator (scratch memory for each Update pass).
     *   - JobSystemThreadPool (uses std::thread::hardware_concurrency()).
     *   - PhysicsSystem with up to 65 536 bodies, 1 024 body pairs,
     *     and 1 024 contact constraints.
     *
     * @param gravity  Downward acceleration (m/s²).  Default: 9.81.
     * @return true on success.
     */
    bool Init(float gravity = kDefaultGravity);

    /**
     * @brief Advance the simulation by dt seconds.
     *
     * Must be called every game frame with the elapsed time since the
     * previous frame.  Internally Jolt may split dt into multiple sub-steps
     * (up to kMaxSubSteps) for numerical stability.
     *
     * @param dt        Elapsed seconds (typically 1/60 for a 60 FPS game).
     * @param substeps  Override the number of sub-steps (0 = auto).
     */
    void Step(float dt, int substeps = 1);

    /**
     * @brief Release all bodies and shut down the physics system.
     */
    void Shutdown();

    // -----------------------------------------------------------------------
    // Body factory — creates rigid bodies
    // -----------------------------------------------------------------------

    /**
     * @brief Create an axis-aligned box body.
     *
     * TEACHING NOTE — Box Shape
     * halfExtents is the half-size along each axis, so a 1×1×1 cube has
     * halfExtents = (0.5, 0.5, 0.5).  This is the convention used by most
     * physics engines (Jolt, Bullet, PhysX).
     *
     * @param pos          World-space centre position.
     * @param halfExtents  Half-size on each axis (metres).
     * @param mass         Mass in kilograms.  Ignored when isStatic = true.
     * @param isStatic     True = immovable geometry; false = dynamic body.
     * @return Opaque body ID (uint32_t) or kInvalidBodyID on failure.
     */
    uint32_t CreateBox(math::Vec3 pos, math::Vec3 halfExtents,
                       float mass, bool isStatic);

    /**
     * @brief Create a sphere body.
     *
     * @param pos      World-space centre.
     * @param radius   Sphere radius (metres).
     * @param mass     Mass (kg).
     * @param isStatic True = static.
     * @return Opaque body ID or kInvalidBodyID.
     */
    uint32_t CreateSphere(math::Vec3 pos, float radius,
                          float mass, bool isStatic);

    /**
     * @brief Create a vertical capsule body (used for characters / NPCs).
     *
     * TEACHING NOTE — Capsule Shape for Characters
     * A capsule is a cylinder with hemispherical caps.  It is the standard
     * shape for character controllers because:
     *   • No sharp edges → slides smoothly off walls and slopes.
     *   • The rounded bottom → naturally steps up small ledges.
     *   • Stable standing orientation → doesn't tip over.
     *
     * halfHeight is the half-length of the *cylindrical* part only.
     * The total height = 2 * (halfHeight + radius).
     *
     * @param pos         World-space centre (bottom of the capsule).
     * @param halfHeight  Half-height of the cylindrical section (metres).
     * @param radius      Capsule radius (metres).
     * @param mass        Mass (kg).
     * @param isStatic    True = static.
     * @return Opaque body ID or kInvalidBodyID.
     */
    uint32_t CreateCapsule(math::Vec3 pos, float halfHeight, float radius,
                           float mass, bool isStatic);

    /**
     * @brief Remove and destroy a body from the simulation.
     * @param id  Body ID returned by a previous Create* call.
     */
    void DestroyBody(uint32_t id);

    // -----------------------------------------------------------------------
    // Body state accessors
    // -----------------------------------------------------------------------

    /// Get world-space centre position of a body.
    math::Vec3 GetPosition(uint32_t id) const;

    /// Get world-space orientation as a quaternion.
    math::Quat GetRotation(uint32_t id) const;

    /// Get linear velocity (metres/second).
    math::Vec3 GetLinearVelocity(uint32_t id) const;

    /// Set linear velocity (useful for applying impulses).
    void SetLinearVelocity(uint32_t id, math::Vec3 vel);

    /// Teleport a body to a new position (bypasses collision resolution).
    void SetPosition(uint32_t id, math::Vec3 pos);

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------

    /**
     * @brief Fire a ray and return the first solid surface hit.
     *
     * @param origin     Ray start (world space).
     * @param direction  Normalised direction vector.
     * @param maxDist    Maximum ray length (metres).
     * @param outHit     Populated on success.
     * @return True if something was hit; outHit is valid only when true.
     */
    bool Raycast(math::Vec3 origin, math::Vec3 direction,
                 float maxDist, RaycastHit& outHit) const;

    // -----------------------------------------------------------------------
    // Internal accessor (for CharacterController — in physics/ only)
    // -----------------------------------------------------------------------

    /**
     * @brief Access the underlying Jolt PhysicsSystem.
     *
     * TEACHING NOTE — Why Expose Internals?
     * CharacterController and HitVolumeManager need access to Jolt's
     * BodyInterface and NarrowPhaseQuery.  Rather than duplicating every
     * needed method on PhysicsWorld, we expose the Impl pointer to classes
     * that live in the same physics/ module.  External code (ECS systems,
     * gameplay) must NOT call this method.
     */
    detail::PhysicsWorldImpl* GetImpl() const { return m_impl.get(); }

private:
    std::unique_ptr<detail::PhysicsWorldImpl> m_impl;  ///< pImpl — all Jolt objects live here.
};

} // namespace physics
} // namespace engine
