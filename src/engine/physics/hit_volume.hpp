/**
 * @file hit_volume.hpp
 * @brief HitVolume — axis-aligned box overlap volumes for combat hit detection.
 *
 * ============================================================================
 * TEACHING NOTE — Physics-Based Hit Detection
 * ============================================================================
 * In FFXV, when Noctis swings his sword, the game doesn't trace a ray from
 * the blade tip — it creates a "hit volume" (also called an "attack box" or
 * "hitbox") that follows the sword through space.  Any "hurt volume" (the
 * defender's body region that can receive damage) that overlaps the attack
 * box is considered hit.
 *
 * This two-volume approach has several advantages over raycasting:
 *
 *   • Wide weapons — a broadsword is wide; a ray from the tip misses targets
 *     to the sides.  An attack box captures the full blade width.
 *
 *   • Simultaneous hits — one swing can hit multiple enemies at once without
 *     needing multiple ray tests.
 *
 *   • Multi-hit blocking — a shield can block only if its hurt volume overlaps
 *     the attacker's hit volume.
 *
 * ─── Types of volumes ───────────────────────────────────────────────────────
 *
 *   HitVolumeType::Attack — created by the attacker (sword, fist, AoE spell).
 *                           When active, any overlapping Hurt volume is hit.
 *
 *   HitVolumeType::Hurt   — permanent bounding region of a damageable body.
 *                           Multiple hurt zones per character (head, torso,
 *                           legs) enable location-based damage.
 *
 * ─── Implementation Note ───────────────────────────────────────────────────
 * We implement this with simple axis-aligned box (AABB) overlap tests in
 * C++ without creating Jolt rigid bodies.  For a shipping game you would use
 * Jolt's NarrowPhaseQuery::CollideShape() with BoxShape for accurate oriented
 * box collisions.  AABB is shown here because it is easy to understand and
 * sufficient for teaching.
 *
 * ─── Usage Example ─────────────────────────────────────────────────────────
 *   // Register permanent hurt volumes for each enemy.
 *   HitVolumeID hurtID = hvMgr.Register(HitVolumeType::Hurt, enemyEntity,
 *                                        {0.4f, 0.9f, 0.4f});
 *
 *   // On each melee frame, activate the attack volume for the player's sword.
 *   HitVolumeID atkID  = hvMgr.Register(HitVolumeType::Attack, playerEntity,
 *                                        {0.1f, 0.5f, 0.8f});   // sword shape
 *   hvMgr.SetActive(atkID, true);
 *
 *   // Each frame, update the world positions from entity transforms.
 *   hvMgr.Update(playerEntity, playerTransform.position + swordOffset);
 *
 *   // Query overlaps.
 *   std::vector<EntityID> hitEntities;
 *   hvMgr.QueryOverlaps(atkID, hitEntities);
 *
 *   // When swing finishes, remove the attack volume.
 *   hvMgr.Unregister(atkID);
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Platform: All (no platform-specific code — pure AABB math)
 */

#pragma once

#include "engine/math/math_types.hpp"
#include "engine/core/Types.hpp"    // EntityID

#include <cstdint>
#include <vector>

namespace engine {
namespace physics {

// ===========================================================================
// Types
// ===========================================================================

/// Opaque handle returned by HitVolumeManager::Register().
using HitVolumeID = uint32_t;

/// Sentinel "no volume" ID.
static constexpr HitVolumeID kInvalidHitVolumeID = 0xFFFFFFFFu;

/**
 * @enum HitVolumeType
 * @brief Distinguishes attacker (sword) volumes from defender (body) volumes.
 */
enum class HitVolumeType : uint8_t
{
    Attack = 0,   ///< Offensive volume — triggers hit when overlapping a Hurt volume.
    Hurt   = 1,   ///< Defensive volume — receives hits from overlapping Attack volumes.
};

// ===========================================================================
// HitVolume (internal data — you rarely create these directly)
// ===========================================================================

/**
 * @struct HitVolume
 * @brief A single axis-aligned box volume registered with the manager.
 */
struct HitVolume
{
    HitVolumeID   id          = kInvalidHitVolumeID;
    HitVolumeType type        = HitVolumeType::Hurt;
    EntityID      ownerEntity = NULL_ENTITY;
    math::Vec3    centre      { 0.0f, 0.0f, 0.0f };
    math::Vec3    halfExtents { 0.5f, 0.5f, 0.5f };
    bool          isActive    = false;
};

// ===========================================================================
// HitVolumeManager
// ===========================================================================

/**
 * @class HitVolumeManager
 * @brief Tracks all hit and hurt volumes; queries for overlaps each frame.
 *
 * ============================================================================
 * TEACHING NOTE — Manager Ownership Model
 * ============================================================================
 * HitVolumeManager owns all registered volumes.  Callers receive a
 * HitVolumeID handle and use it to control the volume:
 *   Register()     — create a volume, get its ID.
 *   Unregister()   — destroy the volume by ID.
 *   SetActive()    — toggle whether the volume participates in queries.
 *   Update()       — move the volume to match the entity's transform.
 *   QueryOverlaps()— find all entities whose Hurt volumes overlap an Attack.
 *
 * The manager is intentionally simple (linear array of volumes with linear
 * overlap scan).  For a large open-world scene with hundreds of simultaneous
 * combat volumes you would use a spatial acceleration structure (BVH or grid).
 * ============================================================================
 */
class HitVolumeManager
{
public:
    HitVolumeManager() = default;

    // Non-copyable — there is one manager per combat scene.
    HitVolumeManager(const HitVolumeManager&)            = delete;
    HitVolumeManager& operator=(const HitVolumeManager&) = delete;

    /**
     * @brief Register a new hit volume.
     *
     * @param type        Attack or Hurt.
     * @param owner       Entity this volume belongs to.
     * @param halfExtents AABB half-extents (metres).
     * @return            A unique HitVolumeID handle.
     */
    HitVolumeID Register(HitVolumeType type,
                         EntityID      owner,
                         math::Vec3    halfExtents);

    /**
     * @brief Remove a hit volume by ID.
     * @param id  ID returned by Register().
     */
    void Unregister(HitVolumeID id);

    /**
     * @brief Enable or disable a volume for overlap queries.
     *
     * TEACHING NOTE — Activation toggle
     * Attack volumes should only be active during the relevant frames of the
     * attack animation (e.g. frames 8–18 of a 30-frame swing).  Deactivating
     * them for the rest of the swing avoids false hits on the recovery phase.
     *
     * @param id      Volume ID.
     * @param active  True = participates in queries; false = ignored.
     */
    void SetActive(HitVolumeID id, bool active);

    /**
     * @brief Move a volume to follow an entity's world-space position.
     *
     * Call this every frame after TransformComponent is updated.
     *
     * @param id     Volume ID.
     * @param centre New world-space centre of the volume.
     */
    void Update(HitVolumeID id, math::Vec3 centre);

    /**
     * @brief Find all entities whose Hurt volumes overlap the given Attack volume.
     *
     * TEACHING NOTE — AABB overlap test
     * Two AABBs [cA ± hA] and [cB ± hB] overlap when:
     *   |cA.x - cB.x| ≤ hA.x + hB.x   (for all three axes)
     * This is O(N²) over active volumes — fast enough for a small combat scene.
     *
     * @param attackID   ID of an Attack-type volume.
     * @param outEntities Receives the EntityID of each entity whose Hurt volume
     *                    overlaps the attack volume.  May contain duplicates if
     *                    an entity has multiple hurt volumes.
     */
    void QueryOverlaps(HitVolumeID             attackID,
                       std::vector<EntityID>&  outEntities) const;

    /// Return the number of currently registered volumes.
    int Count() const { return static_cast<int>(m_volumes.size()); }

private:
    std::vector<HitVolume> m_volumes;  ///< All registered volumes (unsorted).
    HitVolumeID            m_nextID = 0;

    /// Helper: find volume by ID; returns nullptr if not found.
    const HitVolume* FindByID(HitVolumeID id) const;
    HitVolume*       FindByID(HitVolumeID id);

    /// AABB overlap test between two volumes.
    static bool OverlapAABB(const HitVolume& a, const HitVolume& b);
};

} // namespace physics
} // namespace engine
