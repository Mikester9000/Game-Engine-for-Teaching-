/**
 * @file terrain_collision.cpp
 * @brief BakeTerrainCollider — Jolt JPH::HeightFieldShape terrain body (M25).
 *
 * ============================================================================
 * TEACHING NOTE — JPH::HeightFieldShape
 * ============================================================================
 * Jolt's HeightFieldShape is the correct collision primitive for terrain:
 *   • Stores height samples in a compact format (8-bit precision by default).
 *   • Supports all Jolt query types: raycast, shape-cast, contact generation.
 *   • Internally uses a bounding-volume hierarchy (AABB tree) to quickly
 *     reject triangles during queries — O(log N) rather than O(N).
 *
 * Key constructor parameters:
 *   mHeightSamples  — flat float array, row-major, row=Z col=X
 *   mSampleCount    — must be a power of 2 (Jolt requirement)
 *   mOffset         — world-space translation applied BEFORE scale
 *   mScale          — (stepX, heightScale, stepZ) scale per cell
 *
 * After calling settings.Create() we get a JPH::ShapeRefC (reference-counted
 * pointer).  We embed it in a static BodyCreationSettings and hand it to
 * BodyInterface::CreateAndAddBody() — the same interface used for boxes and
 * spheres in physics_world.cpp.
 *
 * ============================================================================
 * TEACHING NOTE — Why Power-of-2 Sample Count?
 * ============================================================================
 * Jolt internally divides the height field into a quadtree.  Each level of
 * the quadtree groups 2×2 child blocks into a parent AABB.  A power-of-2
 * sample count guarantees that every level divides evenly, producing a
 * complete, balanced quadtree.  With an odd sample count, the last block
 * would be incomplete, requiring special-case logic that Jolt avoids.
 *
 * Valid values: 2, 4, 8, 16, 32, 64, 128, 256, …
 *
 * ============================================================================
 */

#ifdef ENGINE_ENABLE_PHYSICS

#include "engine/physics/terrain_collision.hpp"
#include "engine/physics/physics_world.hpp"
#include "engine/physics/physics_impl.hpp"
#include "engine/core/Logger.hpp"

// HeightFieldShape header — only available in Jolt-enabled builds
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

namespace engine {
namespace physics {

// ---------------------------------------------------------------------------
// Helper: check whether n is a power of 2 (n >= 2)
// ---------------------------------------------------------------------------
static bool IsPow2(int n) noexcept
{
    return (n >= 2) && ((n & (n - 1)) == 0);
}

// ===========================================================================
// BakeTerrainCollider
// ===========================================================================

uint32_t BakeTerrainCollider(
    PhysicsWorld& world,
    const float*  heights,
    int           sampleCount,
    float         worldSizeX,
    float         worldSizeZ,
    math::Vec3    origin)
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Validation
    // -----------------------------------------------------------------------
    // Guard against the most common mistakes:
    //   1. Null height array (would cause a segfault inside Jolt).
    //   2. Sample count that is not a power of 2 (Jolt asserts this internally
    //      in debug builds; in release builds it would produce wrong geometry).
    // -----------------------------------------------------------------------
    if (!heights)
    {
        LOG_ERROR("[BakeTerrainCollider] heights array is null");
        return PhysicsWorld::kInvalidBodyID;
    }
    if (!IsPow2(sampleCount))
    {
        LOG_ERROR("[BakeTerrainCollider] sampleCount must be a power of 2 (got %d)", sampleCount);
        return PhysicsWorld::kInvalidBodyID;
    }
    if (worldSizeX <= 0.0f || worldSizeZ <= 0.0f)
    {
        LOG_ERROR("[BakeTerrainCollider] worldSizeX and worldSizeZ must be > 0");
        return PhysicsWorld::kInvalidBodyID;
    }

    auto* impl = world.GetImpl();
    if (!impl || !impl->initialised)
    {
        LOG_ERROR("[BakeTerrainCollider] PhysicsWorld not initialised — call Init() first");
        return PhysicsWorld::kInvalidBodyID;
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — HeightFieldShapeSettings Configuration
    // -----------------------------------------------------------------------
    // mOffset — position of the (0, 0) grid corner in world space.
    //   We pass origin.y = 0 because height is encoded in mHeightSamples; the
    //   world Y of a sample is: origin.y + mScale.y * height[i].
    //   Setting mScale.y = 1.0 means height values are in metres directly.
    //
    // mScale — per-sample world stride:
    //   .x = worldSizeX / (sampleCount - 1)  [metres between adjacent X samples]
    //   .y = 1.0                              [height in metres (no scaling)]
    //   .z = worldSizeZ / (sampleCount - 1)  [metres between adjacent Z samples]
    //
    // mHeightSamples — a JPH::Array<float> populated from the caller's array.
    //   JPH::Array is Jolt's std::vector substitute; it uses the same layout.
    //
    // mSampleCount — the N in an N×N height grid.
    //   Jolt always assumes a square grid; this is mSampleCount × mSampleCount.
    // -----------------------------------------------------------------------
    float stepX = worldSizeX / static_cast<float>(sampleCount - 1);
    float stepZ = worldSizeZ / static_cast<float>(sampleCount - 1);

    JPH::HeightFieldShapeSettings settings;
    settings.mOffset      = JPH::Vec3(origin.x, origin.y, origin.z);
    settings.mScale       = JPH::Vec3(stepX, 1.0f, stepZ);
    settings.mSampleCount = static_cast<JPH::uint32>(sampleCount);

    int totalSamples = sampleCount * sampleCount;
    settings.mHeightSamples.resize(static_cast<size_t>(totalSamples));
    for (int i = 0; i < totalSamples; ++i)
        settings.mHeightSamples[static_cast<size_t>(i)] = heights[i];

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Shape Creation (Deferred Validation)
    // -----------------------------------------------------------------------
    // settings.Create() runs Jolt's validation pipeline:
    //   • Checks that sampleCount is a power of 2.
    //   • Compresses the height values to the internal bit-packed format.
    //   • Builds the bounding-volume quadtree.
    //
    // If validation fails, result.HasError() is true and result.GetError()
    // contains an error string.  We log it and return the invalid sentinel.
    // -----------------------------------------------------------------------
    JPH::ShapeSettings::ShapeResult result = settings.Create();
    if (result.HasError())
    {
        LOG_ERROR("[BakeTerrainCollider] HeightFieldShape creation failed: %s",
                  result.GetError().c_str());
        return PhysicsWorld::kInvalidBodyID;
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Static Body Creation
    // -----------------------------------------------------------------------
    // EMotionType::Static means the body has infinite mass and never moves.
    // The broad-phase layer NON_MOVING lets Jolt skip this body in the
    // moving-vs-moving collision pair generation — a significant optimisation
    // for large open-world terrains with many static patches.
    //
    // We place the body at (0,0,0) in world space because the origin is
    // already encoded in the HeightFieldShape mOffset above.
    // -----------------------------------------------------------------------
    JPH::BodyCreationSettings bodySettings(
        result.Get(),
        JPH::RVec3(0.0f, 0.0f, 0.0f),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Static,
        Layers::NON_MOVING
    );

    JPH::BodyInterface& bi = impl->BodyInterface();
    JPH::BodyID id = bi.CreateAndAddBody(bodySettings, JPH::EActivation::DontActivate);

    if (id.IsInvalid())
    {
        LOG_ERROR("[BakeTerrainCollider] BodyInterface::CreateAndAddBody failed — body limit reached?");
        return PhysicsWorld::kInvalidBodyID;
    }

    LOG_INFO("[BakeTerrainCollider] Terrain body created (id=%u, %d×%d samples)",
             id.GetIndexAndSequenceNumber(), sampleCount, sampleCount);

    // Return the opaque body ID (uint32_t)
    return id.GetIndexAndSequenceNumber();
}

} // namespace physics
} // namespace engine

#endif // ENGINE_ENABLE_PHYSICS
