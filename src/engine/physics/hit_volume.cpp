/**
 * @file hit_volume.cpp
 * @brief HitVolumeManager — AABB overlap combat hit detection.
 *
 * TEACHING NOTE — Why AABB and Not Physics Bodies?
 * Using Jolt rigid bodies for hit/hurt volumes would work but introduces
 * overhead: body creation, broadphase insertion, contact callbacks.
 * For teaching, AABB overlap is easier to understand and sufficient
 * for a melee combat demo.
 *
 * In a shipping game you might use:
 *   • Jolt CollideShape() with BoxShape — accurate OBB for rotated weapons.
 *   • Spatial hash grid — O(1) average lookup for many simultaneous volumes.
 *   • Physics layers ATTACK / HURT — let Jolt's broadphase do the filtering.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Platform: All
 */

#include "engine/physics/hit_volume.hpp"

#include <algorithm>  // std::find_if, std::remove_if
#include <cmath>      // std::fabs

namespace engine {
namespace physics {

// ---------------------------------------------------------------------------
HitVolumeID HitVolumeManager::Register(HitVolumeType type,
                                        EntityID      owner,
                                        math::Vec3    halfExtents)
{
    HitVolume v;
    v.id          = m_nextID++;
    v.type        = type;
    v.ownerEntity = owner;
    v.halfExtents = halfExtents;
    v.isActive    = false;   // caller must explicitly activate
    v.centre      = { 0.0f, 0.0f, 0.0f };

    m_volumes.push_back(v);
    return v.id;
}

// ---------------------------------------------------------------------------
void HitVolumeManager::Unregister(HitVolumeID id)
{
    // TEACHING NOTE — erase-remove idiom
    // std::remove_if moves matching elements to the end of the vector and
    // returns an iterator to the first "removed" element.  erase() then
    // removes them.  This is the idiomatic C++ way to remove from a vector.
    m_volumes.erase(
        std::remove_if(m_volumes.begin(), m_volumes.end(),
                       [id](const HitVolume& v){ return v.id == id; }),
        m_volumes.end()
    );
}

// ---------------------------------------------------------------------------
void HitVolumeManager::SetActive(HitVolumeID id, bool active)
{
    if (HitVolume* v = FindByID(id))
        v->isActive = active;
}

// ---------------------------------------------------------------------------
void HitVolumeManager::Update(HitVolumeID id, math::Vec3 centre)
{
    if (HitVolume* v = FindByID(id))
        v->centre = centre;
}

// ---------------------------------------------------------------------------
void HitVolumeManager::QueryOverlaps(HitVolumeID            attackID,
                                      std::vector<EntityID>& outEntities) const
{
    const HitVolume* attack = FindByID(attackID);
    if (!attack || attack->type != HitVolumeType::Attack || !attack->isActive)
        return;

    for (const HitVolume& v : m_volumes)
    {
        if (!v.isActive)                         continue;
        if (v.type != HitVolumeType::Hurt)       continue;
        if (v.ownerEntity == attack->ownerEntity) continue;  // can't self-hit

        if (OverlapAABB(*attack, v))
            outEntities.push_back(v.ownerEntity);
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

const HitVolume* HitVolumeManager::FindByID(HitVolumeID id) const
{
    for (const HitVolume& v : m_volumes)
        if (v.id == id) return &v;
    return nullptr;
}

HitVolume* HitVolumeManager::FindByID(HitVolumeID id)
{
    for (HitVolume& v : m_volumes)
        if (v.id == id) return &v;
    return nullptr;
}

bool HitVolumeManager::OverlapAABB(const HitVolume& a, const HitVolume& b)
{
    // TEACHING NOTE — AABB Overlap Test
    // Two AABBs overlap on all three axes simultaneously.
    // For each axis, the overlap condition is:
    //   |centreA - centreB| <= halfA + halfB
    // If this fails for ANY axis, the boxes are separated (no overlap).
    const float dx = std::fabs(a.centre.x - b.centre.x);
    const float dy = std::fabs(a.centre.y - b.centre.y);
    const float dz = std::fabs(a.centre.z - b.centre.z);

    return (dx <= a.halfExtents.x + b.halfExtents.x) &&
           (dy <= a.halfExtents.y + b.halfExtents.y) &&
           (dz <= a.halfExtents.z + b.halfExtents.z);
}

} // namespace physics
} // namespace engine
