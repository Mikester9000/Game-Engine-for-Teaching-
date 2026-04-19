/**
 * @file physics_impl.hpp
 * @brief Internal Jolt Physics implementation types — NOT a public header.
 *
 * ============================================================================
 * TEACHING NOTE — Internal Header Pattern
 * ============================================================================
 * This file is an *internal* header: it is included ONLY by physics/ .cpp
 * files, never by external engine code or game systems.
 *
 * Why have an internal header at all?
 * ------------------------------------
 * The PhysicsWorldImpl struct and the Jolt layer-filter classes must be
 * visible to *multiple* physics .cpp files:
 *   • physics_world.cpp   — defines PhysicsWorldImpl and the filter classes.
 *   • character_controller.cpp — accesses PhysicsWorldImpl members to call
 *     JPH::DefaultBroadPhaseLayerFilter and PhysicsSystem.
 *   • raycast.cpp         — accesses PhysicsWorldImpl::physicsSystem.
 *
 * Placing these definitions in a .cpp file would make them invisible to other
 * translation units.  Placing them in the public physics_world.hpp would
 * expose Jolt headers to ALL engine code — slow compile times.
 *
 * The internal-header pattern solves both problems:
 *   • Jolt headers are isolated to #include "physics_impl.hpp" in the .cpp.
 *   • All physics .cpp files share the same struct layout.
 *   • External code only sees the public physics_world.hpp (no Jolt types).
 *
 * Convention: internal headers live alongside the implementation .cpp files
 * and use the suffix _impl.hpp.  Do NOT include them from .hpp public headers.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Platform: All (included only from Jolt-enabled builds)
 */

#pragma once

#ifdef ENGINE_ENABLE_PHYSICS

// ---------------------------------------------------------------------------
// Jolt Physics core includes
// ---------------------------------------------------------------------------
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>

#include <memory>   // std::unique_ptr

// ===========================================================================
// Object layer constants
// ===========================================================================
namespace Layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING     = 1;
    static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
}

namespace BroadPhaseLayers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING{ 0 };
    static constexpr JPH::BroadPhaseLayer MOVING    { 1 };
    static constexpr JPH::uint            NUM_LAYERS{ 2 };
}

// ===========================================================================
// Broad-phase layer interface
// ===========================================================================
class EngineBroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface
{
public:
    EngineBroadPhaseLayerInterface()
    {
        m_objectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        m_objectToBroadPhase[Layers::MOVING]     = BroadPhaseLayers::MOVING;
    }

    JPH::uint GetNumBroadPhaseLayers() const override
    {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
    {
        JPH_ASSERT(layer < Layers::NUM_LAYERS);
        return m_objectToBroadPhase[layer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
    {
        switch ((JPH::BroadPhaseLayer::Type)layer)
        {
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:     return "MOVING";
        default: return "INVALID";
        }
    }
#endif

private:
    JPH::BroadPhaseLayer m_objectToBroadPhase[Layers::NUM_LAYERS];
};

// ===========================================================================
// Object-vs-broadphase layer filter
// ===========================================================================
class EngineObjectVsBroadPhaseLayerFilter final
    : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
    bool ShouldCollide(JPH::ObjectLayer layer,
                       JPH::BroadPhaseLayer bpLayer) const override
    {
        switch (layer)
        {
        case Layers::NON_MOVING: return bpLayer == BroadPhaseLayers::MOVING;
        case Layers::MOVING:     return true;
        default:                 return false;
        }
    }
};

// ===========================================================================
// Object-layer pair filter
// ===========================================================================
class EngineObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
{
public:
    bool ShouldCollide(JPH::ObjectLayer layer1,
                       JPH::ObjectLayer layer2) const override
    {
        switch (layer1)
        {
        case Layers::NON_MOVING: return layer2 == Layers::MOVING;
        case Layers::MOVING:     return true;
        default:                 return false;
        }
    }
};

// ===========================================================================
// PhysicsWorldImpl — pImpl containing all Jolt objects
// ===========================================================================
namespace engine::physics::detail {

struct PhysicsWorldImpl
{
    // -- Layer interface objects (must outlive PhysicsSystem) --
    EngineBroadPhaseLayerInterface      broadPhaseLayerInterface;
    EngineObjectVsBroadPhaseLayerFilter objectVsBroadPhaseFilter;
    EngineObjectLayerPairFilter         objectLayerPairFilter;

    // -- Core Jolt subsystems --
    std::unique_ptr<JPH::TempAllocatorImpl>   tempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> jobSystem;
    std::unique_ptr<JPH::PhysicsSystem>       physicsSystem;

    bool initialised = false;

    JPH::BodyInterface& BodyInterface()
    {
        return physicsSystem->GetBodyInterface();
    }
};

} // namespace engine::physics::detail

#endif // ENGINE_ENABLE_PHYSICS
