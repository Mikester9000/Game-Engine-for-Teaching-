/**
 * @file ik_solver.cpp
 * @brief Inverse Kinematics solver implementations.
 *
 * Implements:
 *   • TwoBoneIK::SolveMidPosition  — Law of Cosines analytical solution
 *   • TwoBoneIK::AlignRotation     — Shortest-arc quaternion helper
 *   • FABRIKSolver                 — FABRIK iterative N-joint solver
 *
 * ============================================================================
 * TEACHING NOTE — Two-Bone IK: Step-by-Step
 * ============================================================================
 *
 * Imagine a two-bone arm:  root (shoulder) → mid (elbow) → end (wrist).
 *
 *              pole
 *               ↑
 *         mid●  │
 *          / \  │
 *         /   \ │
 *  root ●       ● end (target)
 *
 * Given: root, mid, end positions, target position, pole-vector hint.
 * Find:  new mid position such that end snaps to target.
 *
 * Step 1 — Measure bone lengths.
 *   a = |mid  – root|  (upper arm)
 *   b = |end  – mid |  (forearm)
 *
 * Step 2 — Clamp target distance.
 *   d = clamp(|target – root|, 0.001, a + b – ε)
 *   (If target is beyond full reach, the arm stretches as far as it can.)
 *
 * Step 3 — Law of Cosines: angle at root.
 *   cos(α) = (a² + d² – b²) / (2 a d)
 *   α      = acos(clamp(…, –1, 1))
 *
 * Step 4 — Construct the bent-elbow direction.
 *   dir     = normalize(target – root)   (points from root to target)
 *   perpDir = component of poleDir perpendicular to dir
 *   (This is what makes the elbow bend in the desired direction.)
 *
 * Step 5 — New mid position.
 *   midPos_new = root + a * (cos(α) * dir + sin(α) * perpDir)
 *
 * ============================================================================
 * TEACHING NOTE — FABRIK: Step-by-Step
 * ============================================================================
 *
 * Chain: j[0] (root) --- j[1] --- j[2] --- j[3] (end effector)
 * Bone lengths: L[0] = |j[1]-j[0]|, L[1] = |j[2]-j[1]|, L[2] = |j[3]-j[2]|
 *
 * Per-iteration:
 *
 *   Forward pass (end to root):
 *     j[3] = target
 *     for i = 2 downto 0:
 *       direction = normalize(j[i] – j[i+1])
 *       j[i]      = j[i+1] + direction * L[i]
 *
 *   Backward pass (root to end):
 *     j[0] = originalRoot
 *     for i = 1 to N-1:
 *       direction = normalize(j[i] – j[i-1])
 *       j[i]      = j[i-1] + direction * L[i-1]
 *
 *   Stop when |j[N-1] – target| < tolerance.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 */

#include "engine/animation/ik_solver.hpp"

#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <cassert>

namespace engine {
namespace animation {

// ===========================================================================
// TwoBoneIK — Analytical solver
// ===========================================================================

bool TwoBoneIK::SolveMidPosition(
    const math::Vec3& rootPos,
    const math::Vec3& midPos,
    const math::Vec3& endPos,
    const math::Vec3& target,
    const math::Vec3& poleDir,
    math::Vec3&       outMidPos)
{
    using math::Vec3;
    using math::kEps;

    // -----------------------------------------------------------------------
    // Step 1 — Measure bone lengths.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Use float for bone lengths.
    // These distances are in world units and remain constant throughout the
    // solve (bones are inextensible rigid segments).
    // -----------------------------------------------------------------------
    const float a = (midPos - rootPos).Length();   // upper bone
    const float b = (endPos - midPos).Length();    // lower bone

    // Guard: degenerate chain (zero-length bones).
    if (a < kEps || b < kEps)
        return false;

    // -----------------------------------------------------------------------
    // Step 2 — Measure target distance and clamp to reachable range.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Clamping target distance.
    // If the target is too close (d < |a-b|) or too far (d > a+b), the chain
    // cannot form a valid triangle.  We clamp d so:
    //   • If the target is farther than (a+b), stretch the chain in a line.
    //   • If the target is closer than |a-b|, the chain wraps back (rare).
    // -----------------------------------------------------------------------
    const float dRaw = (target - rootPos).Length();
    const float dMin = std::fabsf(a - b) + kEps;
    const float dMax = a + b - kEps;
    const float d    = std::max(dMin, std::min(dMax, dRaw));

    // -----------------------------------------------------------------------
    // Step 3 — Law of Cosines: angle at root (alpha).
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Clamping the cosine before acos().
    // Floating-point arithmetic can produce values slightly outside [-1, 1]
    // due to rounding.  acos() is undefined outside that range.  Clamping
    // prevents NaN at the cost of a few ULPs of error, which is undetectable
    // in animation.
    // -----------------------------------------------------------------------
    const float cosAlpha = std::max(-1.0f, std::min(1.0f,
        (a*a + d*d - b*b) / (2.0f * a * d)));
    const float alpha    = std::acos(cosAlpha);   // angle at root

    // -----------------------------------------------------------------------
    // Step 4 — Compute the two orthonormal directions.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Pole Vector Decomposition
    // The "pole vector" hints which side the elbow bends toward.  We cannot
    // use it directly because it may not be perpendicular to rootToTarget.
    // We must project out the component along rootToTarget to get the true
    // perpendicular direction.
    //
    //   perpDir = normalize( poleDir – (poleDir · rootToTarget) * rootToTarget )
    //
    // This is the standard Gram-Schmidt orthogonalization for two vectors.
    // If poleDir is parallel to rootToTarget (degenerate), we fall back to
    // an arbitrary perpendicular using the cross product with (0,1,0).
    // -----------------------------------------------------------------------
    const Vec3 rootToTarget = (target - rootPos).Normalized();

    Vec3 perpDir;
    {
        const float proj = poleDir.Dot(rootToTarget);
        const Vec3  perp = poleDir - rootToTarget * proj;
        const float perpLen = perp.Length();

        if (perpLen > kEps)
        {
            perpDir = perp / perpLen;
        }
        else
        {
            // Pole is (nearly) parallel to root→target.
            // Choose an arbitrary perpendicular to avoid collapse.
            Vec3 up = Vec3::Up();
            float upProj = up.Dot(rootToTarget);
            Vec3  upPerp = up - rootToTarget * upProj;
            if (upPerp.Length() > kEps)
            {
                perpDir = upPerp.Normalized();
            }
            else
            {
                // rootToTarget is vertical — use world right instead.
                Vec3 right = Vec3::Right();
                Vec3 rPerp = right - rootToTarget * right.Dot(rootToTarget);
                perpDir = (rPerp.Length() > kEps) ? rPerp.Normalized() : Vec3::Fwd();
            }
        }
    }

    // -----------------------------------------------------------------------
    // Step 5 — Compute new mid position.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Rotating the root→target axis by alpha.
    // The new elbow is at distance a from root, in the plane spanned by
    // rootToTarget and perpDir, at angle alpha from rootToTarget.
    //
    //   outMidPos = root + a * (cos(alpha) * rootToTarget + sin(alpha) * perpDir)
    //
    // cos(alpha) is the component along the target direction.
    // sin(alpha) is the component along the pole direction (the "bend").
    // -----------------------------------------------------------------------
    outMidPos = rootPos
              + rootToTarget * (a * std::cos(alpha))
              + perpDir       * (a * std::sin(alpha));

    return true;
}

// ---------------------------------------------------------------------------
// TwoBoneIK::AlignRotation
// ---------------------------------------------------------------------------

math::Quat TwoBoneIK::AlignRotation(const math::Vec3& srcDir,
                                     const math::Vec3& dstDir)
{
    using math::Vec3;
    using math::Quat;
    using math::kEps;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Shortest-Arc Rotation
    // -----------------------------------------------------------------------
    // We want the quaternion q such that q rotates srcDir to dstDir along the
    // shortest arc on the unit sphere.
    //
    // Algorithm:
    //   1. Dot product → cosine of angle between the vectors.
    //   2. Cross product → rotation axis (perpendicular to the plane).
    //   3. If dot ≈ 1, vectors are nearly identical → return identity.
    //   4. If dot ≈ -1, vectors are antiparallel → 180° around arbitrary axis.
    //
    // Reference: "Real-Time Rendering" (Möller et al.), §4.3.
    // -----------------------------------------------------------------------
    const Vec3 sn = srcDir.Normalized();
    const Vec3 dn = dstDir.Normalized();

    const float dot = sn.Dot(dn);

    // Already aligned.
    if (dot > 1.0f - kEps)
        return Quat::Identity();

    // Anti-parallel: 180° rotation around any perpendicular axis.
    if (dot < -1.0f + kEps)
    {
        Vec3 perp = sn.Cross(Vec3::Up());
        if (perp.Length() < kEps)
            perp = sn.Cross(Vec3::Right());
        return Quat::FromAxisAngle(perp.Normalized(),
                                   math::kPi);
    }

    // General case: axis = cross(src, dst), angle = acos(dot).
    const Vec3  axis  = sn.Cross(dn).Normalized();
    const float angle = std::acos(std::max(-1.0f, std::min(1.0f, dot)));
    return Quat::FromAxisAngle(axis, angle);
}

// ===========================================================================
// FABRIKSolver
// ===========================================================================

FABRIKSolver::FABRIKSolver(int maxIterations, float tolerance)
    : m_maxIterations(maxIterations)
    , m_tolerance(tolerance)
{
}

void FABRIKSolver::AddJoint(const math::Vec3& worldPos)
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Computing Bone Lengths on Add
    // -----------------------------------------------------------------------
    // When the second joint (and each subsequent joint) is added, we compute
    // the bone length as the distance to the previous joint.  This ensures
    // the chain is "at rest" (bone lengths are the bind-pose lengths) at
    // construction time.
    //
    // The bone at index i connects joint i to joint i+1.
    // m_boneLengths has one fewer entry than m_joints.
    // -----------------------------------------------------------------------
    if (!m_joints.empty())
    {
        float len = (worldPos - m_joints.back()).Length();
        m_boneLengths.push_back(len);
    }
    m_joints.push_back(worldPos);
}

void FABRIKSolver::SetJoint(int index, const math::Vec3& worldPos)
{
    assert(index >= 0 && index < static_cast<int>(m_joints.size()));
    m_joints[static_cast<size_t>(index)] = worldPos;
}

void FABRIKSolver::SetJoints(const std::vector<math::Vec3>& positions)
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Re-using the solver for a different pose each frame.
    // -----------------------------------------------------------------------
    // Callers typically reset the joint positions from the current animated
    // skeleton before solving, so FABRIK operates from the current frame's
    // pose rather than from a fixed bind pose.  This prevents the IK from
    // "snapping" visually and gives smoother results.
    // -----------------------------------------------------------------------
    assert(positions.size() == m_joints.size());
    m_joints = positions;
}

bool FABRIKSolver::Solve(const math::Vec3& target)
{
    using math::Vec3;
    using math::kEps;

    const int N = static_cast<int>(m_joints.size());
    if (N < 2)
        return false;

    // -----------------------------------------------------------------------
    // Check reachability.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — If the target is farther than the full chain length,
    // the best we can do is point the chain straight toward the target.
    // We still run the backward pass which will do exactly this.
    // -----------------------------------------------------------------------
    const Vec3  rootPos     = m_joints[0];   // Remember root for backward pass.
    const float totalLen    = TotalLength();
    const float targetDist  = (target - rootPos).Length();

    // -----------------------------------------------------------------------
    // FABRIK main loop.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Each iteration does two passes:
    //   Forward:  snap end to target, propagate constraints backward.
    //   Backward: restore root, propagate constraints forward.
    // On each step we snap a joint toward the previous one while maintaining
    // bone length.  This is the key insight of FABRIK: it only uses direction
    // vectors and scalar distances — no matrices, no inverse Jacobians.
    // -----------------------------------------------------------------------
    bool converged = false;

    for (int iter = 0; iter < m_maxIterations; ++iter)
    {
        // ----------------------------------------------------------------
        // Forward pass: from end effector back to root.
        // ----------------------------------------------------------------
        m_joints[static_cast<size_t>(N - 1)] = target;

        for (int i = N - 2; i >= 0; --i)
        {
            const size_t si   = static_cast<size_t>(i);
            const size_t si1  = static_cast<size_t>(i + 1);
            const Vec3   dir  = (m_joints[si] - m_joints[si1]).Normalized();
            m_joints[si]      = m_joints[si1] + dir * m_boneLengths[si];
        }

        // ----------------------------------------------------------------
        // Backward pass: restore root, propagate toward end.
        // ----------------------------------------------------------------
        m_joints[0] = rootPos;

        for (int i = 1; i < N; ++i)
        {
            const size_t si   = static_cast<size_t>(i);
            const size_t si1  = static_cast<size_t>(i - 1);
            const Vec3   dir  = (m_joints[si] - m_joints[si1]).Normalized();
            m_joints[si]      = m_joints[si1] + dir * m_boneLengths[si - 1];
        }

        // ----------------------------------------------------------------
        // Convergence check.
        // ----------------------------------------------------------------
        // TEACHING NOTE — Check the END effector, not every joint.
        // We only care whether the last joint reached the target.
        // Individual intermediate joints don't need a tolerance check.
        // ----------------------------------------------------------------
        const float endError = (m_joints[static_cast<size_t>(N - 1)] - target).Length();
        if (endError < m_tolerance || targetDist > totalLen)
        {
            converged = true;
            break;
        }
    }

    return converged;
}

const math::Vec3& FABRIKSolver::GetJoint(int index) const
{
    assert(index >= 0 && index < static_cast<int>(m_joints.size()));
    return m_joints[static_cast<size_t>(index)];
}

int FABRIKSolver::JointCount() const
{
    return static_cast<int>(m_joints.size());
}

void FABRIKSolver::Reset()
{
    m_joints.clear();
    m_boneLengths.clear();
}

float FABRIKSolver::GetBoneLength(int index) const
{
    assert(index >= 0 && index < static_cast<int>(m_boneLengths.size()));
    return m_boneLengths[static_cast<size_t>(index)];
}

float FABRIKSolver::TotalLength() const
{
    float total = 0.0f;
    for (float len : m_boneLengths)
        total += len;
    return total;
}

} // namespace animation
} // namespace engine
