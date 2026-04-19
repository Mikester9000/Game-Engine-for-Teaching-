/**
 * @file rigid_body.cpp
 * @brief RigidBodyCreator implementation.
 *
 * TEACHING NOTE — Thin Wrapper
 * RigidBodyCreator delegates directly to PhysicsWorld's factory methods.
 * Its value is the Descriptor struct — a single point that aggregates all
 * parameters, making call sites readable:
 *
 *   RigidBodyCreator::Descriptor d;
 *   d.shapeType  = RigidBodyCreator::ShapeType::Box;
 *   d.halfExtents = { 0.5f, 0.5f, 0.5f };
 *   d.position    = { 0.0f, 5.0f, 0.0f };
 *   d.mass        = 2.0f;
 *   uint32_t id = RigidBodyCreator::Create(world, d);
 *
 * Compare this to a raw PhysicsWorld call which requires choosing between
 * CreateBox / CreateSphere / CreateCapsule at the call site.  The Descriptor
 * approach is especially useful when the shape type comes from a data file.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Platform: All
 */

#include "engine/physics/rigid_body.hpp"
#include "engine/physics/physics_world.hpp"

namespace engine {
namespace physics {

uint32_t RigidBodyCreator::Create(PhysicsWorld& world, const Descriptor& desc)
{
    switch (desc.shapeType)
    {
    case ShapeType::Box:
        return world.CreateBox(
            desc.position, desc.halfExtents, desc.mass, desc.isStatic);

    case ShapeType::Sphere:
        return world.CreateSphere(
            desc.position, desc.radius, desc.mass, desc.isStatic);

    case ShapeType::Capsule:
        return world.CreateCapsule(
            desc.position, desc.halfHeight, desc.radius, desc.mass, desc.isStatic);

    default:
        return kInvalidBodyID;
    }
}

void RigidBodyCreator::Destroy(PhysicsWorld& world, uint32_t bodyID)
{
    world.DestroyBody(bodyID);
}

void RigidBodyCreator::SyncPositionFromPhysics(const PhysicsWorld& world,
                                               uint32_t            bodyID,
                                               math::Vec3&         outPos)
{
    outPos = world.GetPosition(bodyID);
}

void RigidBodyCreator::PushPositionToPhysics(PhysicsWorld&     world,
                                             uint32_t          bodyID,
                                             const math::Vec3& pos)
{
    world.SetPosition(bodyID, pos);
}

} // namespace physics
} // namespace engine
