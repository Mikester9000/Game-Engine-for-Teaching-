/**
 * @file formation_system.hpp
 * @brief Party / squad formation system for the educational RPG engine.
 *
 * ============================================================================
 * TEACHING NOTE — Formation Systems in Action RPGs
 * ============================================================================
 *
 * Formation systems control the RELATIVE POSITIONS of a squad of characters
 * around a designated LEADER.  In FFXV, Noctis's party (Ignis, Prompto,
 * Gladio) maintain dynamic formations as they run through the world.
 *
 * ─── Key Concepts ──────────────────────────────────────────────────────────
 *
 *   SLOT — a named position relative to the leader (e.g. "left-wing",
 *           "right-wing", "rear").  Each follower occupies exactly one slot.
 *
 *   FORMATION OFFSET — a 2D (x, z) displacement from the leader's position
 *   *in the leader's local space*.  When the leader turns, all offsets rotate
 *   with them so the formation stays visually consistent.
 *
 *   STEERING — each follower runs toward their DESIRED SLOT POSITION (world
 *   space) using a seek steering behaviour.  Formation systems do NOT
 *   teleport characters — they just provide target positions and let the
 *   AI/physics layer move toward them.
 *
 * ─── Formation Types ─────────────────────────────────────────────────────
 *
 *   LINE    — followers stand to the left and right of the leader.
 *             Best for standing/idle in a corridor.
 *
 *        [F1] [LEADER] [F2] [F3]
 *
 *   V_SHAPE — followers fan behind-left and behind-right of the leader,
 *             like a flock of migrating birds.  Provides good visibility
 *             for all members (nobody blocked by the leader).
 *
 *        [F1]          [F3]
 *             [LEADER]
 *             [F2]
 *
 *   CIRCLE  — followers form a protective ring around the leader.
 *             Used for defending a VIP NPC.
 *
 *             [F1]
 *         [F4]  [F2]
 *             [F3]
 *
 * ─── Local vs. World Space ────────────────────────────────────────────────
 *
 * TEACHING NOTE — The critical transform step
 * ─────────────────────────────────────────────
 * Formation offsets are defined in the leader's LOCAL space (forward = +Z,
 * right = +X, up = +Y).  To get the WORLD space target for each follower
 * we must rotate the offset by the leader's yaw angle:
 *
 *   worldTarget.x = leader.x + cos(yaw) * offset.x − sin(yaw) * offset.z
 *   worldTarget.z = leader.z + sin(yaw) * offset.x + cos(yaw) * offset.z
 *
 * This is just a 2D rotation matrix applied to the (x, z) components.
 * The y component is taken directly from the leader (followers stay at
 * the same height — terrain adherence is handled by physics).
 *
 * ─── Slot Assignment ──────────────────────────────────────────────────────
 *
 * TEACHING NOTE — Optimal slot assignment via cost matrix
 * ─────────────────────────────────────────────────────────
 * For N followers and N slots, the OPTIMAL assignment minimises the total
 * travel distance (Hungarian algorithm, O(N³)).  For N ≤ 4 (typical party
 * size) we use a simpler nearest-slot greedy assignment, which gives
 * good results in practice.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 *
 * C++ Standard: C++17
 */

#pragma once

#include <vector>
#include <cmath>
#include <cstdint>
#include <limits>

#include "../../engine/core/Types.hpp"
#include "../../engine/ecs/ECS.hpp"


// ============================================================================
// FormationType — formation shape selector
// ============================================================================

/**
 * @enum FormationType
 * @brief Selects the geometric layout of follower slots around the leader.
 *
 * TEACHING NOTE — Data-driven formation switching
 * ──────────────────────────────────────────────────
 * Rather than hard-coding positions, each FormationType has a corresponding
 * BuildSlots() helper that generates the offset list.  Switching formation
 * is then a single call:
 *
 *   system.SetFormation(leaderID, followers, FormationType::V_SHAPE);
 *
 * This is the Strategy design pattern: the formation TYPE controls how
 * offsets are computed, but the assignment and world-space transform logic
 * is identical for all types.
 */
enum class FormationType : uint8_t {
    LINE    = 0,   ///< Side-by-side to the right of the leader.
    V_SHAPE = 1,   ///< Fan behind the leader (classic military V).
    CIRCLE  = 2,   ///< Ring around the leader.
};


// ============================================================================
// FormationSlot — describes one slot and which entity occupies it
// ============================================================================

/**
 * @struct FormationSlot
 * @brief A single named slot in a formation.
 *
 * @field slotIndex   Index (0 = first follower slot, etc.).
 * @field offsetX     Local-space X offset from the leader.
 * @field offsetZ     Local-space Z offset from the leader.
 * @field entityID    Which entity currently owns this slot (NULL_ENTITY = vacant).
 *
 * TEACHING NOTE — Why local-space offsets?
 * ──────────────────────────────────────────
 * Storing world-space positions would require updating ALL slot positions
 * every time the leader moves.  Storing local-space offsets means slot
 * positions only need to be recomputed when we call GetWorldPosition(),
 * which is typically once per frame per follower.
 */
struct FormationSlot {
    int      slotIndex = 0;
    float    offsetX   = 0.0f;  ///< Local forward axis displacement (leader Z-forward).
    float    offsetZ   = 0.0f;  ///< Local right axis displacement (leader X-right).
    EntityID entityID  = NULL_ENTITY;
};


// ============================================================================
// FormationSystem
// ============================================================================

/**
 * @class FormationSystem
 * @brief Assigns follower entities to formation slots and computes their
 *        world-space target positions each frame.
 *
 * USAGE:
 * @code
 *   FormationSystem fs;
 *
 *   // Build a V-shape formation for Noctis + 3 party members.
 *   std::vector<EntityID> party = { ignis, prompto, gladio };
 *   fs.SetFormation(noctis, party, FormationType::V_SHAPE);
 *
 *   // Every frame:
 *   fs.Update(world, dt);
 *
 *   // Read each follower's desired position:
 *   Vec3 ignisDest = fs.GetDesiredPosition(ignis);
 * @endcode
 */
class FormationSystem {
public:

    FormationSystem() = default;

    // -----------------------------------------------------------------------
    // Setup
    // -----------------------------------------------------------------------

    /**
     * @brief Configure the formation: assign slots to followers.
     *
     * Builds a slot layout for @p type, then assigns each follower to the
     * nearest vacant slot (greedy assignment by current world-space distance).
     *
     * @param world      ECS World (used to read TransformComponents for slot
     *                   assignment scoring).
     * @param leaderID   The entity that the formation follows.
     * @param followers  List of follower entity IDs (typically 1–7).
     * @param type       Geometric layout to use.
     */
    void SetFormation(World& world, EntityID leaderID,
                      const std::vector<EntityID>& followers,
                      FormationType type);

    // -----------------------------------------------------------------------
    // Per-frame update
    // -----------------------------------------------------------------------

    /**
     * @brief Update desired positions for all followers this frame.
     *
     * Reads the leader's current TransformComponent (position + yaw rotation)
     * and computes the world-space target for every slot.
     *
     * TEACHING NOTE — Why not move entities here?
     * ─────────────────────────────────────────────
     * FormationSystem only WRITES TARGET POSITIONS — it does not modify
     * TransformComponent directly.  Movement toward targets is handled by the
     * physics/AI layer to avoid multiple systems fighting over the same
     * component.  This separation is the "tell, don't ask" principle applied
     * to ECS: systems communicate through data, not through direct calls.
     *
     * @param world  ECS World (reads leader TransformComponent).
     * @param dt     Delta time (reserved for future smooth slot transitions).
     */
    void Update(World& world, float dt);

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------

    /**
     * @brief Return the world-space position a follower should move toward.
     *
     * @param followerID  The follower entity.
     * @return            Desired world position.  Returns {0,0,0} if the
     *                    follower is not registered in any slot.
     */
    Vec3 GetDesiredPosition(EntityID followerID) const;

    /**
     * @brief Return the number of currently assigned slots.
     */
    int SlotCount() const { return static_cast<int>(m_slots.size()); }

    /**
     * @brief Return the current formation type.
     */
    FormationType GetType() const { return m_type; }

    /**
     * @brief Return the leader entity ID.
     */
    EntityID GetLeader() const { return m_leaderID; }

    // -----------------------------------------------------------------------
    // Static helpers — exported so tests can verify slot geometry
    // -----------------------------------------------------------------------

    /**
     * @brief Build the local-space slot offsets for a given formation type
     *        and follower count.
     *
     * @param type     Formation geometry.
     * @param count    Number of follower slots to generate.
     * @return         Vector of (offsetX, offsetZ) pairs, one per follower.
     *
     * TEACHING NOTE — Procedural slot generation
     * ────────────────────────────────────────────
     * Hardcoding positions for every possible squad size is error-prone.
     * Instead we GENERATE them procedurally:
     *   • LINE:    slots are evenly spaced along the X axis at a fixed Z.
     *   • V_SHAPE: alternating left/right at increasing Z-depth behind the leader.
     *   • CIRCLE:  evenly spaced around a circle of radius `spacing`.
     */
    static std::vector<std::pair<float,float>>
        BuildSlotOffsets(FormationType type, int count);

private:

    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Transform a local-space (x, z) offset to a world-space Vec3
     *        using the leader's position and yaw.
     *
     * @param leaderPos  Leader world position (x, y, z).
     * @param yaw        Leader yaw in radians (rotation around Y axis).
     * @param localX     Local X offset (positive = leader's right).
     * @param localZ     Local Z offset (positive = leader's forward).
     * @return           World-space Vec3 of the target position.
     */
    static Vec3 LocalToWorld(Vec3 leaderPos, float yaw,
                              float localX, float localZ);

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    EntityID                          m_leaderID = NULL_ENTITY;
    FormationType                     m_type     = FormationType::LINE;
    std::vector<FormationSlot>        m_slots;

    // Per-follower desired positions (world space), updated each frame by Update().
    // Key = entityID, Value = world-space Vec3.
    struct FollowerState {
        EntityID entityID      = NULL_ENTITY;
        Vec3     desiredPos    = {};
    };
    std::vector<FollowerState> m_followers;
};
