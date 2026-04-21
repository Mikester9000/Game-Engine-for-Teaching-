/**
 * @file formation_system.cpp
 * @brief Party / squad formation system implementation.
 *
 * ============================================================================
 * TEACHING NOTE — Implementation Overview
 * ============================================================================
 *
 * This file implements two main responsibilities:
 *
 *   1. SLOT GENERATION (BuildSlotOffsets) — compute local-space (x, z) offsets
 *      for each follower slot based on the chosen FormationType.
 *
 *   2. WORLD TRANSFORM (Update / LocalToWorld) — rotate the local offsets by
 *      the leader's yaw and add the leader's world position to get the
 *      world-space target for each follower.
 *
 * The maths involved is a 2D rotation matrix:
 *
 *   world_x = leader_x + cos(yaw)*local_x − sin(yaw)*local_z
 *   world_z = leader_z + sin(yaw)*local_x + cos(yaw)*local_z
 *
 * This is identical to transforming a direction vector by a rotation matrix
 * about the Y axis (which is what TransformComponent::Forward() does for
 * the entity's facing direction).
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 *
 * C++ Standard: C++17
 */

#include "formation_system.hpp"

#include <algorithm>  // std::find_if
#include <cmath>      // std::cos, std::sin, std::atan2

// ---------------------------------------------------------------------------
// BuildSlotOffsets — procedural slot layout per formation type
// ---------------------------------------------------------------------------

/**
 * TEACHING NOTE — Line Formation Layout
 * ─────────────────────────────────────────
 * Slots are placed to the RIGHT of the leader along the local X axis:
 *
 *   [LEADER] [F0] [F1] [F2] ...
 *
 * Each slot is `spacing` units further right than the previous.
 * Z offset is fixed at 0 (same depth as leader).
 *
 * Real AAA games often stagger the depth slightly per pair so the formation
 * looks less like a straight line when the leader turns.  We keep it simple
 * here for teachability.
 */
static constexpr float kFormationSpacing = 2.5f;  ///< Metres between slots.

std::vector<std::pair<float,float>>
FormationSystem::BuildSlotOffsets(FormationType type, int count)
{
    std::vector<std::pair<float,float>> offsets;
    offsets.reserve(static_cast<size_t>(count));

    if (count <= 0) return offsets;

    switch (type)
    {
    // -----------------------------------------------------------------------
    case FormationType::LINE:
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Centred line
    // We centre the line so that for N followers the rightmost slot is
    // +N/2 * spacing and the leftmost is -N/2 * spacing.  This keeps the
    // formation visually balanced around the leader.
    {
        const float totalWidth = static_cast<float>(count - 1) * kFormationSpacing;
        const float startX     = -totalWidth * 0.5f;  // leftmost slot X
        for (int i = 0; i < count; ++i) {
            offsets.push_back({ startX + static_cast<float>(i) * kFormationSpacing,
                                0.0f });
        }
        break;
    }

    // -----------------------------------------------------------------------
    case FormationType::V_SHAPE:
    // -----------------------------------------------------------------------
    // TEACHING NOTE — V-shape (echelon) layout
    // Alternating left/right behind the leader.  Each successive pair moves
    // one spacing unit further back (negative Z = behind leader).
    //
    //   depth 1: [F0 left]   [F1 right]
    //   depth 2: [F2 left]   [F3 right]
    //   ...
    //
    // An odd count means the last follower is at the back-centre (x=0).
    {
        for (int i = 0; i < count; ++i) {
            const int   depth = (i / 2) + 1;
            const float z     = -static_cast<float>(depth) * kFormationSpacing;
            const float x     = (i % 2 == 0)
                                  ? -static_cast<float>(depth) * kFormationSpacing * 0.5f
                                  :  static_cast<float>(depth) * kFormationSpacing * 0.5f;
            offsets.push_back({ x, z });
        }
        break;
    }

    // -----------------------------------------------------------------------
    case FormationType::CIRCLE:
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Evenly-spaced circle
    // Place N followers around a circle of radius `r`.  Each follower is at
    // angle  (2π / N) * i  radians from the leader.
    //
    //   slot_x = r * sin(angle)   (positive X = leader's right)
    //   slot_z = r * cos(angle)   (positive Z = leader's forward)
    //
    // TEACHING NOTE — Why sin/cos for circle offsets?
    // On a unit circle, (sin θ, cos θ) traces the circle starting from the
    // "forward" direction (θ=0 → x=0, z=1) and going clockwise when viewed
    // from above.  Starting from forward makes the first follower visually
    // "in front" of the leader, which looks better than starting from the right.
    {
        const float r        = kFormationSpacing * 1.5f;
        const float angleInc = 2.0f * 3.14159265f / static_cast<float>(count);
        for (int i = 0; i < count; ++i) {
            const float angle = static_cast<float>(i) * angleInc;
            offsets.push_back({ r * std::sin(angle), r * std::cos(angle) });
        }
        break;
    }

    default:
        break;
    }

    return offsets;
}


// ---------------------------------------------------------------------------
// SetFormation
// ---------------------------------------------------------------------------

void FormationSystem::SetFormation(World& world,
                                   EntityID leaderID,
                                   const std::vector<EntityID>& followers,
                                   FormationType type)
{
    m_leaderID = leaderID;
    m_type     = type;

    const int count = static_cast<int>(followers.size());
    auto offsets    = BuildSlotOffsets(type, count);

    // Build slot structs.
    m_slots.clear();
    m_slots.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        FormationSlot s;
        s.slotIndex = i;
        s.offsetX   = offsets[static_cast<size_t>(i)].first;
        s.offsetZ   = offsets[static_cast<size_t>(i)].second;
        s.entityID  = NULL_ENTITY;
        m_slots.push_back(s);
    }

    // TEACHING NOTE — Greedy nearest-slot assignment
    // ──────────────────────────────────────────────────
    // For each follower we find the unclaimed slot whose WORLD-SPACE position
    // (based on the leader's CURRENT transform) is closest to the follower's
    // current position.  This minimises the initial repositioning distance
    // and avoids followers crossing paths.
    //
    // For small N (≤ 7) this O(N²) greedy approach is indistinguishable from
    // the O(N³) optimal Hungarian assignment in practice.

    // Read leader transform for initial slot scoring.
    float leaderX = 0.0f, leaderY = 0.0f, leaderZ = 0.0f, leaderYaw = 0.0f;
    if (world.HasComponent<TransformComponent>(leaderID)) {
        const auto& ltc = world.GetComponent<TransformComponent>(leaderID);
        leaderX   = ltc.position.x;
        leaderY   = ltc.position.y;
        leaderZ   = ltc.position.z;
        // Extract yaw from rotation.y (stored as radians in TransformComponent).
        leaderYaw = ltc.rotation.y;
    }

    // Mark all slots vacant.
    for (auto& s : m_slots) s.entityID = NULL_ENTITY;

    // Greedily assign each follower to the nearest vacant slot.
    for (EntityID fid : followers) {
        float fx = 0.0f, fz = 0.0f;
        if (world.HasComponent<TransformComponent>(fid)) {
            const auto& ftc = world.GetComponent<TransformComponent>(fid);
            fx = ftc.position.x;
            fz = ftc.position.z;
        }

        float    bestDist  = std::numeric_limits<float>::max();
        int      bestSlot  = -1;

        for (int si = 0; si < static_cast<int>(m_slots.size()); ++si) {
            if (m_slots[static_cast<size_t>(si)].entityID != NULL_ENTITY) continue;

            // Compute the world-space position of this slot.
            const Vec3 slotWorld = LocalToWorld(
                { leaderX, leaderY, leaderZ },
                leaderYaw,
                m_slots[static_cast<size_t>(si)].offsetX,
                m_slots[static_cast<size_t>(si)].offsetZ);

            const float dx   = slotWorld.x - fx;
            const float dz   = slotWorld.z - fz;
            const float dist = std::sqrt(dx*dx + dz*dz);

            if (dist < bestDist) {
                bestDist = dist;
                bestSlot = si;
            }
        }

        if (bestSlot >= 0)
            m_slots[static_cast<size_t>(bestSlot)].entityID = fid;
    }

    // Build follower state list.
    m_followers.clear();
    for (EntityID fid : followers) {
        FollowerState fs;
        fs.entityID   = fid;
        fs.desiredPos = {};
        m_followers.push_back(fs);
    }
}


// ---------------------------------------------------------------------------
// Update — compute world-space desired positions each frame
// ---------------------------------------------------------------------------

void FormationSystem::Update(World& world, float /*dt*/)
{
    if (m_leaderID == NULL_ENTITY) return;
    if (!world.HasComponent<TransformComponent>(m_leaderID)) return;

    const auto& ltc  = world.GetComponent<TransformComponent>(m_leaderID);
    const Vec3  lPos = { ltc.position.x, ltc.position.y, ltc.position.z };
    const float yaw  = ltc.rotation.y;

    // Compute world-space position for every slot.
    for (auto& slot : m_slots) {
        if (slot.entityID == NULL_ENTITY) continue;

        const Vec3 worldPos = LocalToWorld(lPos, yaw, slot.offsetX, slot.offsetZ);

        // Write into the follower state.
        for (auto& fs : m_followers) {
            if (fs.entityID == slot.entityID) {
                fs.desiredPos = worldPos;
                break;
            }
        }
    }
}


// ---------------------------------------------------------------------------
// GetDesiredPosition
// ---------------------------------------------------------------------------

Vec3 FormationSystem::GetDesiredPosition(EntityID followerID) const
{
    for (const auto& fs : m_followers) {
        if (fs.entityID == followerID)
            return fs.desiredPos;
    }
    return {};  // not registered — return origin
}


// ---------------------------------------------------------------------------
// LocalToWorld — 2D rotation matrix applied to a local-space offset
// ---------------------------------------------------------------------------

/*static*/
Vec3 FormationSystem::LocalToWorld(Vec3 leaderPos, float yaw,
                                    float localX, float localZ)
{
    // TEACHING NOTE — 2D Rotation Matrix
    // ────────────────────────────────────
    // A rotation by angle θ around the Y axis transforms (x, z) as:
    //
    //   world_x = cos(θ) * local_x − sin(θ) * local_z
    //   world_z = sin(θ) * local_x + cos(θ) * local_z
    //
    // We only rotate in XZ (horizontal plane) and copy Y unchanged.
    // This matches how FFXV formations stay flat on the ground plane while
    // the characters follow the terrain height via physics raycasts.

    const float c = std::cos(yaw);
    const float s = std::sin(yaw);

    return {
        leaderPos.x + c * localX - s * localZ,
        leaderPos.y,                            // same height as leader
        leaderPos.z + s * localX + c * localZ
    };
}
