/**
 * @file physics_world.cpp
 * @brief PhysicsWorld — Jolt Physics engine integration.
 *
 * ============================================================================
 * TEACHING NOTE — Jolt Physics Initialisation Order
 * ============================================================================
 * Jolt Physics requires a specific initialisation sequence that may look
 * verbose at first but each step has a purpose:
 *
 * 1. JPH::RegisterDefaultAllocator()
 *    Registers Jolt's default malloc/free-based memory allocator.
 *    You can replace this with a custom allocator (arena, pool) for
 *    improved performance in a shipping title.
 *
 * 2. JPH::Factory::sInstance = new JPH::Factory()
 *    Creates the global type factory used to look up RTTI for shapes,
 *    bodies, and constraints.  Required before any Jolt object is created.
 *
 * 3. JPH::RegisterTypes()
 *    Registers all built-in Jolt types (BoxShape, SphereShape, etc.)
 *    with the factory.  Must come after step 2.
 *
 * 4. TempAllocatorImpl
 *    A fixed-size scratch allocator Jolt uses for per-frame temporary work
 *    (contact manifolds, constraint solver working sets).  10 MB is
 *    sufficient for most games.
 *
 * 5. JobSystemThreadPool
 *    Jolt's multi-threaded job system.  Uses std::thread internally.
 *    We allow it to use all available hardware threads for maximum throughput.
 *    On a single-core CI machine it falls back gracefully to 1 thread.
 *
 * 6. BroadPhaseLayerInterface + Object layer filters
 *    These small classes tell Jolt which object layers exist and which
 *    pairs should be tested for collision.  Our two-layer setup
 *    (NON_MOVING and MOVING) is the standard minimal configuration.
 *    They are defined in physics_impl.hpp so character_controller.cpp
 *    can also access them.
 *
 * 7. PhysicsSystem::Init(...)
 *    Allocates the internal body array (up to maxBodies), body pair cache,
 *    contact cache, and constraint solver buffers.
 *
 * ─── Memory Budget ──────────────────────────────────────────────────────────
 * The numbers below are tuned for a small demo scene:
 *   maxBodies            = 65 536   (plenty for an open-world zone)
 *   numBodyMutexes       = 0        (0 = auto-choose)
 *   maxBodyPairs         = 65 536
 *   maxContactConstraints= 10 240
 *   tempAllocatorSize    = 10 MB
 * Scale these up for a shipping title with thousands of rigid bodies.
 *
 * ─── Reference ──────────────────────────────────────────────────────────────
 * Jolt Physics GitHub: https://github.com/jrouwe/JoltPhysics
 * Jolt "Hello World" sample: JoltPhysics/Samples/HelloWorld/HelloWorld.cpp
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Platform: All (Jolt is cross-platform)
 */

#ifdef ENGINE_ENABLE_PHYSICS

// ---------------------------------------------------------------------------
// TEACHING NOTE — Internal header
// ---------------------------------------------------------------------------
// physics_impl.hpp provides the PhysicsWorldImpl struct definition, the
// Jolt layer-filter classes, and all Jolt core headers.  It is only
// included by physics/ .cpp files (never by external code).
// ---------------------------------------------------------------------------
#include "engine/physics/physics_impl.hpp"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>

// ---------------------------------------------------------------------------
// Engine headers
// ---------------------------------------------------------------------------
#include "engine/physics/physics_world.hpp"
#include "engine/core/Logger.hpp"

#include <thread>    // std::thread::hardware_concurrency
#include <cstdarg>   // va_list (Jolt trace callback)
#include <cstdio>    // std::vsnprintf
#include <algorithm> // std::max

// ===========================================================================
// Local helpers — Jolt trace / assert callbacks
// ===========================================================================
namespace {

// TEACHING NOTE — Jolt Trace / Assert callbacks
// Jolt calls JPH_Trace (a global function pointer) for internal log messages.
// In debug builds Jolt also calls JPH_AssertFailed on assertion failures.
// We route them through our Logger so all output goes to one place.
static void JoltTraceImpl(const char* fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    LOG_DEBUG("[Jolt] " << buf);
}

#ifdef JPH_ENABLE_ASSERTS
static bool JoltAssertFailed(const char* expr, const char* msg,
                              const char* file, JPH::uint line)
{
    LOG_ERROR("[Jolt assert] " << expr
              << (msg ? std::string(" — ") + msg : "")
              << " (" << file << ":" << line << ")");
    return true;  // true = break into debugger if available
}
#endif

} // anonymous namespace

// ===========================================================================
// PhysicsWorld — public methods
// ===========================================================================

namespace engine {
namespace physics {

PhysicsWorld::PhysicsWorld()
    : m_impl(std::make_unique<detail::PhysicsWorldImpl>())
{}

PhysicsWorld::~PhysicsWorld()
{
    if (m_impl && m_impl->initialised)
        Shutdown();
}

// ---------------------------------------------------------------------------
bool PhysicsWorld::Init(float gravity)
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Step 1: Register the default allocator
    // -----------------------------------------------------------------------
    // Jolt's RegisterDefaultAllocator() sets up the global allocator function
    // pointers (Allocate, Free, AlignedAllocate, AlignedFree) to the standard
    // malloc/free equivalents.  In a shipping engine you would plug in a
    // custom pool allocator here for better cache performance.
    // -----------------------------------------------------------------------
    JPH::RegisterDefaultAllocator();

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Step 2: Trace / assert callbacks
    // -----------------------------------------------------------------------
    JPH::Trace = JoltTraceImpl;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = JoltAssertFailed;)

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Step 3: Create the type factory
    // -----------------------------------------------------------------------
    if (!JPH::Factory::sInstance)
        JPH::Factory::sInstance = new JPH::Factory();

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Step 4: Register all built-in Jolt types
    // -----------------------------------------------------------------------
    JPH::RegisterTypes();

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Step 5: Temp allocator (10 MB scratch pool)
    // -----------------------------------------------------------------------
    constexpr size_t kTempAllocBytes = 10u * 1024u * 1024u;
    m_impl->tempAllocator =
        std::make_unique<JPH::TempAllocatorImpl>(kTempAllocBytes);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Step 6: Job system
    // -----------------------------------------------------------------------
    // Use hardware_concurrency()-1 threads; clamp to at least 1.
    const int numThreads =
        std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1);

    m_impl->jobSystem = std::make_unique<JPH::JobSystemThreadPool>(
        JPH::cMaxPhysicsJobs,
        JPH::cMaxPhysicsBarriers,
        numThreads
    );

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Step 7: PhysicsSystem
    // -----------------------------------------------------------------------
    constexpr JPH::uint kMaxBodies             = 65536u;
    constexpr JPH::uint kNumBodyMutexes        = 0u;
    constexpr JPH::uint kMaxBodyPairs          = 65536u;
    constexpr JPH::uint kMaxContactConstraints = 10240u;

    m_impl->physicsSystem = std::make_unique<JPH::PhysicsSystem>();
    m_impl->physicsSystem->Init(
        kMaxBodies,
        kNumBodyMutexes,
        kMaxBodyPairs,
        kMaxContactConstraints,
        m_impl->broadPhaseLayerInterface,
        m_impl->objectVsBroadPhaseFilter,
        m_impl->objectLayerPairFilter
    );

    m_impl->physicsSystem->SetGravity(JPH::Vec3(0.0f, -gravity, 0.0f));

    m_impl->initialised = true;

    LOG_INFO("[PhysicsWorld] Initialised — gravity=" << gravity
             << " m/s², threads=" << numThreads);
    return true;
}

// ---------------------------------------------------------------------------
void PhysicsWorld::Step(float dt, int substeps)
{
    if (!m_impl->initialised) return;

    // TEACHING NOTE — Physics Step and Sub-Steps
    // A single physics step with large dt can cause "tunnelling".
    // Using substeps=1 at 60 FPS (dt=0.0167 s) is stable for typical objects.
    m_impl->physicsSystem->Update(
        dt,
        substeps,
        m_impl->tempAllocator.get(),
        m_impl->jobSystem.get()
    );
}

// ---------------------------------------------------------------------------
void PhysicsWorld::Shutdown()
{
    if (!m_impl->initialised) return;

    LOG_INFO("[PhysicsWorld] Shutting down.");

    // TEACHING NOTE — Shutdown Order
    // Remove all bodies before destroying the PhysicsSystem.
    auto& bi = m_impl->BodyInterface();
    JPH::BodyIDVector allBodies;
    m_impl->physicsSystem->GetBodies(allBodies);
    for (const JPH::BodyID& id : allBodies)
    {
        bi.RemoveBody(id);
        bi.DestroyBody(id);
    }

    m_impl->physicsSystem.reset();
    m_impl->jobSystem.reset();
    m_impl->tempAllocator.reset();

    JPH::UnregisterTypes();
    if (JPH::Factory::sInstance)
    {
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }

    m_impl->initialised = false;
}

// ---------------------------------------------------------------------------
// Body factory helpers
// ---------------------------------------------------------------------------

// TEACHING NOTE — Body Creation Pattern
// Every Jolt body is created in three steps:
//   1. Create a Shape (BoxShape, SphereShape, …).
//   2. Build BodyCreationSettings — position, rotation, layer, motion type.
//   3. Call BodyInterface::CreateAndAddBody().

uint32_t PhysicsWorld::CreateBox(math::Vec3 pos, math::Vec3 halfExtents,
                                  float mass, bool isStatic)
{
    if (!m_impl->initialised) return kInvalidBodyID;

    JPH::BoxShapeSettings shapeSettings(
        JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z));
    auto shapeResult = shapeSettings.Create();
    if (shapeResult.HasError()) return kInvalidBodyID;

    const JPH::ObjectLayer layer = isStatic
        ? static_cast<JPH::ObjectLayer>(kLayerNonMoving)
        : static_cast<JPH::ObjectLayer>(kLayerMoving);

    JPH::BodyCreationSettings settings(
        shapeResult.Get(),
        JPH::RVec3(pos.x, pos.y, pos.z),
        JPH::Quat::sIdentity(),
        isStatic ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic,
        layer
    );

    // TEACHING NOTE — Explicit Mass Override
    // By default Jolt derives mass from the shape's density (uniform 1000 kg/m³).
    // To match the caller-supplied mass we set CalculateInertia so Jolt still
    // computes a physically correct inertia tensor for this shape, but uses our
    // mass value instead of a density estimate.  Static bodies ignore mass.
    if (!isStatic && mass > 0.0f)
    {
        settings.mOverrideMassProperties =
            JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = mass;
    }

    JPH::BodyID bodyID = m_impl->BodyInterface().CreateAndAddBody(
        settings, JPH::EActivation::Activate);

    return bodyID.IsInvalid() ? kInvalidBodyID : bodyID.GetIndexAndSequenceNumber();
}

uint32_t PhysicsWorld::CreateSphere(math::Vec3 pos, float radius,
                                     float mass, bool isStatic)
{
    if (!m_impl->initialised) return kInvalidBodyID;

    JPH::SphereShapeSettings shapeSettings(radius);
    auto shapeResult = shapeSettings.Create();
    if (shapeResult.HasError()) return kInvalidBodyID;

    const JPH::ObjectLayer layer = isStatic
        ? static_cast<JPH::ObjectLayer>(kLayerNonMoving)
        : static_cast<JPH::ObjectLayer>(kLayerMoving);

    JPH::BodyCreationSettings settings(
        shapeResult.Get(),
        JPH::RVec3(pos.x, pos.y, pos.z),
        JPH::Quat::sIdentity(),
        isStatic ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic,
        layer
    );

    if (!isStatic && mass > 0.0f)
    {
        settings.mOverrideMassProperties =
            JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = mass;
    }

    JPH::BodyID bodyID = m_impl->BodyInterface().CreateAndAddBody(
        settings, JPH::EActivation::Activate);

    return bodyID.IsInvalid() ? kInvalidBodyID : bodyID.GetIndexAndSequenceNumber();
}

uint32_t PhysicsWorld::CreateCapsule(math::Vec3 pos, float halfHeight,
                                      float radius, float mass, bool isStatic)
{
    if (!m_impl->initialised) return kInvalidBodyID;

    JPH::CapsuleShapeSettings shapeSettings(halfHeight, radius);
    auto shapeResult = shapeSettings.Create();
    if (shapeResult.HasError()) return kInvalidBodyID;

    const JPH::ObjectLayer layer = isStatic
        ? static_cast<JPH::ObjectLayer>(kLayerNonMoving)
        : static_cast<JPH::ObjectLayer>(kLayerMoving);

    JPH::BodyCreationSettings settings(
        shapeResult.Get(),
        JPH::RVec3(pos.x, pos.y, pos.z),
        JPH::Quat::sIdentity(),
        isStatic ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic,
        layer
    );

    if (!isStatic && mass > 0.0f)
    {
        settings.mOverrideMassProperties =
            JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = mass;
    }

    JPH::BodyID bodyID = m_impl->BodyInterface().CreateAndAddBody(
        settings, JPH::EActivation::Activate);

    return bodyID.IsInvalid() ? kInvalidBodyID : bodyID.GetIndexAndSequenceNumber();
}

void PhysicsWorld::DestroyBody(uint32_t id)
{
    if (!m_impl->initialised || id == kInvalidBodyID) return;

    JPH::BodyID joltID(id);
    auto& bi = m_impl->BodyInterface();
    bi.RemoveBody(joltID);
    bi.DestroyBody(joltID);
}

// ---------------------------------------------------------------------------
// Body state accessors
// ---------------------------------------------------------------------------

math::Vec3 PhysicsWorld::GetPosition(uint32_t id) const
{
    if (!m_impl->initialised || id == kInvalidBodyID)
        return { 0.0f, 0.0f, 0.0f };

    JPH::RVec3 p = m_impl->BodyInterface().GetCenterOfMassPosition(JPH::BodyID(id));
    return { static_cast<float>(p.GetX()),
             static_cast<float>(p.GetY()),
             static_cast<float>(p.GetZ()) };
}

math::Quat PhysicsWorld::GetRotation(uint32_t id) const
{
    if (!m_impl->initialised || id == kInvalidBodyID)
        return { 0.0f, 0.0f, 0.0f, 1.0f };

    JPH::Quat q = m_impl->BodyInterface().GetRotation(JPH::BodyID(id));
    return { q.GetX(), q.GetY(), q.GetZ(), q.GetW() };
}

math::Vec3 PhysicsWorld::GetLinearVelocity(uint32_t id) const
{
    if (!m_impl->initialised || id == kInvalidBodyID)
        return { 0.0f, 0.0f, 0.0f };

    JPH::Vec3 v = m_impl->BodyInterface().GetLinearVelocity(JPH::BodyID(id));
    return { v.GetX(), v.GetY(), v.GetZ() };
}

void PhysicsWorld::SetLinearVelocity(uint32_t id, math::Vec3 vel)
{
    if (!m_impl->initialised || id == kInvalidBodyID) return;
    m_impl->BodyInterface().SetLinearVelocity(
        JPH::BodyID(id), JPH::Vec3(vel.x, vel.y, vel.z));
}

void PhysicsWorld::SetPosition(uint32_t id, math::Vec3 pos)
{
    if (!m_impl->initialised || id == kInvalidBodyID) return;
    m_impl->BodyInterface().SetPosition(
        JPH::BodyID(id),
        JPH::RVec3(pos.x, pos.y, pos.z),
        JPH::EActivation::Activate
    );
}

// ---------------------------------------------------------------------------
// Raycast
// ---------------------------------------------------------------------------
bool PhysicsWorld::Raycast(math::Vec3 origin, math::Vec3 direction,
                            float maxDist, RaycastHit& outHit) const
{
    if (!m_impl->initialised) return false;

    // TEACHING NOTE — Jolt RayCast
    // JPH::RRayCast direction vector length = the maximum ray distance.
    const JPH::RVec3 joltOrigin(origin.x, origin.y, origin.z);
    const JPH::Vec3  joltDir(direction.x * maxDist,
                              direction.y * maxDist,
                              direction.z * maxDist);

    JPH::RRayCast ray{ joltOrigin, joltDir };
    JPH::RayCastResult result;

    if (!m_impl->physicsSystem->GetNarrowPhaseQuery().CastRay(ray, result))
        return false;

    const float dist = result.mFraction * maxDist;
    outHit.hit      = true;
    outHit.distance = dist;
    outHit.bodyID   = result.mBodyID.GetIndexAndSequenceNumber();
    outHit.position = {
        origin.x + direction.x * dist,
        origin.y + direction.y * dist,
        origin.z + direction.z * dist
    };

    // Get surface normal via body lock (read-only).
    {
        JPH::BodyLockRead lock(m_impl->physicsSystem->GetBodyLockInterface(),
                               result.mBodyID);
        if (lock.Succeeded())
        {
            const JPH::Body& body = lock.GetBody();
            JPH::Vec3 n = body.GetWorldSpaceSurfaceNormal(
                result.mSubShapeID2,
                JPH::RVec3(outHit.position.x, outHit.position.y, outHit.position.z)
            );
            outHit.normal = { n.GetX(), n.GetY(), n.GetZ() };
        }
    }
    return true;
}

} // namespace physics
} // namespace engine

#endif // ENGINE_ENABLE_PHYSICS
