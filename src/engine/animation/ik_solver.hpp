/**
 * @file ik_solver.hpp
 * @brief Inverse Kinematics (IK) solvers — Two-Bone analytical + FABRIK N-joint.
 *
 * ============================================================================
 * TEACHING NOTE — What is Inverse Kinematics?
 * ============================================================================
 * Forward Kinematics (FK): given joint rotations, compute end-effector position.
 * Inverse Kinematics (IK): given a desired end-effector position, compute the
 * joint rotations needed to reach it.
 *
 * IK is used in FFXV-style games for:
 *   • Foot placement — keep feet planted on uneven terrain.
 *   • Hand IK — grip weapons, grab ledges, interact with the world.
 *   • Look-at — head/eye tracking toward a point of interest.
 *   • Aim IK — blend a character's spine toward an aim target.
 *
 * ============================================================================
 * TEACHING NOTE — Two Approaches
 * ============================================================================
 *
 * 1. TwoBoneIK — ANALYTICAL (exact, O(1), fixed 2-bone chain)
 *    Uses the Law of Cosines to compute the exact elbow/knee angle for a
 *    root→mid→tip chain.  Best for arms (shoulder→elbow→wrist) and legs
 *    (hip→knee→ankle) because those are always exactly two bones.
 *    A "pole vector" hint selects which side the middle joint bends toward.
 *
 * 2. FABRIKSolver — ITERATIVE (approximate, fast convergence, N bones)
 *    Forward And Backward Reaching Inverse Kinematics.  Works on chains of
 *    any length.  Each iteration does two linear passes (forward and backward)
 *    along the chain.  Typically converges in 1–5 iterations.
 *    Best for tails, spines, tentacles, and other multi-joint chains.
 *
 * ============================================================================
 * TEACHING NOTE — Output Format
 * ============================================================================
 * Both solvers output world-space positions of the solved joints.
 * The AnimationSystem can use these positions to compute the rotation
 * quaternion for each joint (rotate the current bone direction toward
 * the new bone direction using Quat::FromAxisAngle + cross product).
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Platform: All (no platform-specific code)
 */

#pragma once

#include "engine/math/math_types.hpp"

#include <vector>
#include <string>

namespace engine {
namespace animation {

// ===========================================================================
// TwoBoneIK — Analytical two-bone inverse kinematics
// ===========================================================================

/**
 * @struct TwoBoneIK
 * @brief Analytical IK solver for a fixed-length two-bone chain.
 *
 * TEACHING NOTE — When to Use TwoBoneIK
 * Use this for any chain that is EXACTLY two bones long:
 *   • Arms:  shoulder → elbow → wrist
 *   • Legs:  hip      → knee  → ankle
 *   • Claw:  knuckle  → finger_mid → finger_tip
 *
 * The analytical approach is O(1) and gives the mathematically exact answer
 * every frame without iteration.
 *
 * TEACHING NOTE — Law of Cosines
 * Given a triangle with sides a, b, c and angles α, β, γ opposite to them:
 *   c² = a² + b² – 2ab·cos(γ)
 * Rearranged to find an angle:
 *   cos(γ) = (a² + b² – c²) / (2ab)
 *
 * We use this to find the required angle at the root joint (α) and at the
 * mid joint (β) given the desired target distance.
 */
struct TwoBoneIK
{
    // -----------------------------------------------------------------------
    // SolveMidPosition
    // -----------------------------------------------------------------------

    /**
     * @brief Compute the world-space position of the mid joint to reach target.
     *
     * @param rootPos  World position of the root joint (shoulder/hip).
     * @param midPos   Current world position of the mid joint (elbow/knee).
     * @param endPos   Current world position of the end joint (wrist/ankle).
     * @param target   Desired world position for the end joint.
     * @param poleDir  Hint direction for the mid joint bend (e.g. forward for
     *                 knees, outward for elbows).  Does not need to be
     *                 perpendicular to the root→target axis; this function
     *                 projects it automatically.
     * @param outMidPos Output: new world position for the mid joint.
     *
     * @return true  if the target is reachable and outMidPos was computed.
     *         false if the chain is degenerate (zero-length bones) or the
     *               target is unreachable even after clamping.
     *
     * -----------------------------------------------------------------------
     * TEACHING NOTE — Law of Cosines Derivation
     * -----------------------------------------------------------------------
     * Let:
     *   a = |midPos  – rootPos|   (upper bone length)
     *   b = |endPos  – midPos |   (lower bone length)
     *   d = clamped(|target – rootPos|, 0.001, a+b)  (target distance)
     *
     * Angle at root (alpha):
     *   cos(alpha) = (a² + d² – b²) / (2·a·d)
     *   alpha      = acos(clamp(…, -1, 1))
     *
     * Angle at mid (beta):
     *   cos(beta)  = (a² + b² – d²) / (2·a·b)
     *
     * We then rotate the root→target axis by alpha using the pole vector as
     * the rotation axis to find the new mid joint position.
     * -----------------------------------------------------------------------
     */
    static bool SolveMidPosition(
        const math::Vec3& rootPos,
        const math::Vec3& midPos,
        const math::Vec3& endPos,
        const math::Vec3& target,
        const math::Vec3& poleDir,
        math::Vec3&       outMidPos);

    // -----------------------------------------------------------------------
    // Helper: build the rotation quaternion to align srcDir toward dstDir
    // (shortest arc rotation).  Returns Quat::Identity if already aligned.
    // -----------------------------------------------------------------------

    /**
     * @brief Quaternion that rotates srcDir to align with dstDir.
     *
     * TEACHING NOTE — Shortest-Arc Rotation
     * Given two unit vectors, the rotation between them lies in the plane
     * they span.  The axis is their cross product (perpendicular to both),
     * and the angle is acos(dot(src, dst)).
     *
     * Edge cases handled:
     *   • src ≈ dst: return identity (no rotation needed).
     *   • src ≈ -dst: return 180° around a stable perpendicular axis.
     */
    static math::Quat AlignRotation(const math::Vec3& srcDir,
                                    const math::Vec3& dstDir);
};

// ===========================================================================
// FABRIKSolver — Iterative N-joint IK
// ===========================================================================

/**
 * @class FABRIKSolver
 * @brief Iterative IK solver for an N-joint chain using FABRIK.
 *
 * TEACHING NOTE — FABRIK Algorithm (Aristidou & Lasenby, 2011)
 * "Forward And Backward Reaching Inverse Kinematics"
 *
 * FABRIK represents each joint as a world-space position.  It converges by
 * alternating two linear passes:
 *
 *   Forward pass (end → root):
 *     Place the end effector at the target.
 *     Propagate constraints backward along the chain.
 *
 *   Backward pass (root → end):
 *     Restore the root to its original position.
 *     Propagate constraints forward along the chain.
 *
 * TEACHING NOTE — Why FABRIK is fast
 * Each forward and backward pass is O(N) linear work (just vector arithmetic).
 * Unlike Jacobian-based methods (which require matrix inversion), FABRIK
 * converges in 1–5 iterations for most game scenarios.  No trigonometry is
 * needed; only distance normalization.
 *
 * TEACHING NOTE — Bone Lengths
 * Bone lengths are computed automatically from the initial joint positions
 * passed to AddJoint().  They are fixed (inextensible) throughout solving.
 *
 * TEACHING NOTE — Convergence Criterion
 * Solving stops when |joints[N] – target| < tolerance, or after maxIterations
 * regardless of error.  For foot planting at 60 fps, 4 iterations is typical.
 */
class FABRIKSolver
{
public:
    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------

    /**
     * @param maxIterations Maximum iterations per Solve() call (default 10).
     * @param tolerance     Stop when end effector is within this distance of
     *                      the target (default 0.001 world units ≈ 1 mm).
     */
    explicit FABRIKSolver(int maxIterations = 10, float tolerance = 0.001f);

    // -----------------------------------------------------------------------
    // Chain setup — call AddJoint() for each joint in root→tip order,
    // then call Solve() to reach a target.
    // -----------------------------------------------------------------------

    /**
     * @brief Append a joint to the end of the chain.
     * @param worldPos  Bind-pose world position of the new joint.
     *
     * TEACHING NOTE — Call order matters.
     * Joints must be added root-first (joint 0 = root, joint N = end effector).
     * The bone length between joint i and joint i+1 is computed automatically.
     */
    void AddJoint(const math::Vec3& worldPos);

    /**
     * @brief Overwrite the world-space position of joint at index.
     *        Used to reset the chain to the current animated bind pose each
     *        frame before calling Solve().
     */
    void SetJoint(int index, const math::Vec3& worldPos);

    /**
     * @brief Overwrite all joint positions at once (must match chain length).
     */
    void SetJoints(const std::vector<math::Vec3>& positions);

    // -----------------------------------------------------------------------
    // Solving
    // -----------------------------------------------------------------------

    /**
     * @brief Run FABRIK to move the end effector toward target.
     *
     * @param target  Desired world position for the end joint (joint N-1).
     * @return true   if the end effector reached the target within tolerance.
     *         false  if it did not converge within maxIterations
     *                (e.g. target out of reach — end effector is placed as
     *                close as possible in a straight line toward target).
     *
     * -----------------------------------------------------------------------
     * TEACHING NOTE — Frame Usage Pattern
     * -----------------------------------------------------------------------
     * Typical per-frame usage:
     *
     *   // 1. Reset joints to current animated world positions.
     *   solver.SetJoints(animatedWorldPositions);
     *
     *   // 2. Solve toward the foot IK target on the terrain.
     *   bool reached = solver.Solve(footTargetOnTerrain);
     *
     *   // 3. Derive joint rotations from the solved positions.
     *   for (int i = 0; i < solver.JointCount()-1; ++i)
     *   {
     *       Vec3 newDir = (solver.GetJoint(i+1) - solver.GetJoint(i)).Normalized();
     *       // … compute Quat to rotate bone[i] toward newDir …
     *   }
     * -----------------------------------------------------------------------
     */
    bool Solve(const math::Vec3& target);

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------

    /** @brief World position of joint at index after the last Solve() call. */
    const math::Vec3& GetJoint(int index) const;

    /** @brief Number of joints in this chain (including root and end effector). */
    int JointCount() const;

    /** @brief Remove all joints (for re-use with a different chain). */
    void Reset();

    /**
     * @brief Pre-computed distance from joint i to joint i+1.
     *        Stored in m_boneLengths[i].  Computed in AddJoint().
     */
    float GetBoneLength(int index) const;

    /** @brief Total reach of the chain (sum of all bone lengths). */
    float TotalLength() const;

private:
    // -----------------------------------------------------------------------
    // TEACHING NOTE — FABRIK state
    // m_joints:      current world positions (modified each Solve() call)
    // m_bindJoints:  original bind-pose positions (to reset the chain)
    // m_boneLengths: fixed bone lengths (computed once in AddJoint)
    // -----------------------------------------------------------------------
    std::vector<math::Vec3> m_joints;        ///< Working joint positions
    std::vector<float>      m_boneLengths;   ///< bone[i] = |joint[i+1] - joint[i]|

    int   m_maxIterations = 10;
    float m_tolerance     = 0.001f;
};

} // namespace animation
} // namespace engine
