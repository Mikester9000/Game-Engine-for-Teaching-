/**
 * @file raycast.cpp
 * @brief Raycast and shape-cast query implementations.
 *
 * TEACHING NOTE — Thin Wrappers
 * CastRay and CastSphere are thin wrappers over PhysicsWorld methods and
 * Jolt's NarrowPhaseQuery API.  Keeping them separate from physics_world.cpp
 * demonstrates the Single Responsibility Principle: physics_world.cpp manages
 * body lifecycle; raycast.cpp handles query logic.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Platform: All
 */

#include "engine/physics/raycast.hpp"
#include "engine/physics/physics_world.hpp"

#ifdef ENGINE_ENABLE_PHYSICS
#include "engine/physics/physics_impl.hpp"
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#endif

namespace engine {
namespace physics {

bool CastRay(PhysicsWorld& world,
             math::Vec3    origin,
             math::Vec3    direction,
             float         maxDist,
             RaycastHit&   outHit)
{
    return world.Raycast(origin, direction, maxDist, outHit);
}

bool CastRayDown(PhysicsWorld& world,
                 math::Vec3    origin,
                 float         maxDist,
                 RaycastHit&   outHit)
{
    // Fire straight down (-Y direction).
    return world.Raycast(origin,
                         math::Vec3{ 0.0f, -1.0f, 0.0f },
                         maxDist,
                         outHit);
}

bool CastSphere(PhysicsWorld&  world,
                math::Vec3     origin,
                math::Vec3     direction,
                float          radius,
                float          maxDist,
                ShapeCastHit&  outHit)
{
    outHit = {};

#ifdef ENGINE_ENABLE_PHYSICS
    auto* impl = world.GetImpl();
    if (!impl) return false;

    // TEACHING NOTE — Sphere shape cast via Jolt
    // We create a temporary SphereShape and use the NarrowPhaseQuery's
    // CastShape() to sweep it along the ray.
    //
    // ShapeCastSettings controls:
    //   mBackFaceModeTriangles  — whether back faces count as hits (we disable).
    //   mUseShrunkenShapeAndConvexRadius — use exact sphere surface (on by default).
    JPH::SphereShape sphereShape(radius);
    sphereShape.SetEmbedded();  // prevent auto-deletion

    const JPH::RVec3 joltOrigin(origin.x, origin.y, origin.z);
    const JPH::Vec3  joltDir(direction.x * maxDist,
                              direction.y * maxDist,
                              direction.z * maxDist);

    JPH::RShapeCast shapeCast = JPH::RShapeCast::sFromWorldTransform(
        &sphereShape,
        JPH::Vec3::sReplicate(1.0f),  // scale = 1,1,1
        JPH::RMat44::sTranslation(joltOrigin),
        joltDir
    );

    JPH::ShapeCastSettings castSettings;
    castSettings.mBackFaceModeTriangles = JPH::EBackFaceMode::IgnoreBackFaces;

    JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
    impl->physicsSystem->GetNarrowPhaseQuery().CastShape(
        shapeCast,
        castSettings,
        joltOrigin,
        collector
    );

    if (!collector.HadHit())
        return false;

    const auto& hit = collector.mHit;
    const float dist = hit.mFraction * maxDist;

    outHit.hit          = true;
    outHit.distance     = dist;
    outHit.bodyID       = hit.mBodyID2.GetIndexAndSequenceNumber();
    outHit.contactPoint = {
        origin.x + direction.x * dist,
        origin.y + direction.y * dist,
        origin.z + direction.z * dist
    };
    // Contact normal from the shape cast result.
    outHit.contactNormal = {
        hit.mPenetrationAxis.GetX(),
        hit.mPenetrationAxis.GetY(),
        hit.mPenetrationAxis.GetZ()
    };
    return true;

#else
    (void)world; (void)origin; (void)direction; (void)radius; (void)maxDist;
    return false;
#endif
}

} // namespace physics
} // namespace engine
