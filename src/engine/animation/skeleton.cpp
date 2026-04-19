/**
 * @file skeleton.cpp
 * @brief Runtime skeleton implementation.
 *
 * ============================================================================
 * TEACHING NOTE — Mat4 Inversion (Fast 4×4 Rigid-Body Inverse)
 * ============================================================================
 * A general 4×4 matrix inversion is expensive.  However, TRS (translation +
 * rotation + scale) matrices have special structure that allows a cheap
 * "pseudo-inverse":
 *
 * For a pure rotation matrix R (orthogonal): R^{-1} = R^T (transpose).
 * For a TRS matrix M = T · R · S:
 *   M^{-1} = S^{-1} · R^T · T^{-1}
 *
 * We compute this analytically here to avoid a full Gaussian elimination.
 * (If the scale is non-uniform the formula is slightly more complex but still
 *  much cheaper than a generic inversion.)
 *
 * Reference: Lengyel, "Mathematics for 3D Game Programming", §4.3.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 */

#include "engine/animation/skeleton.hpp"

#include <algorithm>
#include <cmath>

namespace engine {
namespace animation {

// ---------------------------------------------------------------------------
// Internal helpers — not exposed in the header
// ---------------------------------------------------------------------------

namespace {

// ---------------------------------------------------------------------------
// TEACHING NOTE — Fast TRS Matrix Inversion
// ---------------------------------------------------------------------------
// For a TRS matrix built from a unit-quaternion rotation and uniform/non-uniform
// scale, the inverse can be computed without a general LU-decompose.
//
// Method:
//   1. Extract the scale from the column lengths.
//   2. Divide each rotation column by its scale to get the pure rotation matrix.
//   3. Transpose the rotation block (R^{-1} = R^T for orthogonal matrices).
//   4. Compute the inverse translation from the transposed rotation and
//      original translation.
//
// If the matrix has zero scale on any axis (degenerate) we fall back to the
// identity to avoid a divide-by-zero.
// ---------------------------------------------------------------------------
math::Mat4 InvertTRS(const math::Mat4& mat)
{
    // Extract scale from column lengths.
    auto len = [&](int col) {
        float x = mat.m[0][col], y = mat.m[1][col], z = mat.m[2][col];
        return std::sqrt(x*x + y*y + z*z);
    };

    float sx = len(0), sy = len(1), sz = len(2);

    // Guard against degenerate scales.
    if (sx < math::kEps || sy < math::kEps || sz < math::kEps)
        return math::Mat4::Identity();

    float isx = 1.0f / (sx * sx);
    float isy = 1.0f / (sy * sy);
    float isz = 1.0f / (sz * sz);

    // Build the inverse: transpose the rotation block, divided by scale squared.
    // This is equivalent to (S^-2 · R^T) for the upper 3×3.
    math::Mat4 inv = math::Mat4::Identity();

    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
        {
            float iscale = (col == 0 ? isx : (col == 1 ? isy : isz));
            inv.m[row][col] = mat.m[col][row] * iscale;
        }

    // Inverse translation: -R^{-1} · T (where T is in the 4th row of row-major mat).
    float tx = mat.m[3][0], ty = mat.m[3][1], tz = mat.m[3][2];
    inv.m[3][0] = -(inv.m[0][0]*tx + inv.m[1][0]*ty + inv.m[2][0]*tz);
    inv.m[3][1] = -(inv.m[0][1]*tx + inv.m[1][1]*ty + inv.m[2][1]*tz);
    inv.m[3][2] = -(inv.m[0][2]*tx + inv.m[1][2]*ty + inv.m[2][2]*tz);

    return inv;
}

} // anonymous namespace

// ===========================================================================
// Skeleton — public methods
// ===========================================================================

int Skeleton::AddJoint(const std::string& name,
                       int                parentIndex,
                       const math::Vec3&  translation,
                       const math::Quat&  rotation,
                       const math::Vec3&  scale)
{
    // TEACHING NOTE — Topological invariant
    // We require that the parent already exists when a child is added.
    // This means joints are added in breadth-first or depth-first order from
    // the root — guaranteeing that m_joints is already in topological sort
    // order when Build() runs.
    Joint j;
    j.index            = static_cast<int>(m_joints.size());
    j.name             = name;
    j.parentIndex      = parentIndex;
    j.bindTranslation  = translation;
    j.bindRotation     = rotation.Normalized();
    j.bindScale        = scale;

    m_joints.push_back(std::move(j));
    return m_joints.back().index;
}

void Skeleton::Build()
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Building Bind-Pose World Matrices
    // -----------------------------------------------------------------------
    // We compute the bind-pose world-space matrix for each joint in a single
    // forward pass, then invert it to get invBindMatrix.
    //
    // local[i] = TRS(bindTranslation, bindRotation, bindScale) for joint i.
    // world[i] = local[i] * world[parent[i]]   (parent == Identity for root)
    //
    // The bind-pose local matrices are the bind-pose TRS matrices.
    // -----------------------------------------------------------------------

    std::vector<math::Mat4> worldMats(m_joints.size(), math::Mat4::Identity());

    for (auto& j : m_joints)
    {
        math::Mat4 local = math::Mat4::TRS(j.bindTranslation,
                                            j.bindRotation,
                                            j.bindScale);

        if (j.parentIndex < 0)
        {
            // Root joint: world space = local space.
            worldMats[static_cast<size_t>(j.index)] = local;
        }
        else
        {
            // Child: chain through the parent's world matrix.
            // Row-major order: local × parent.
            worldMats[static_cast<size_t>(j.index)] =
                local * worldMats[static_cast<size_t>(j.parentIndex)];
        }

        j.bindWorldMatrix = worldMats[static_cast<size_t>(j.index)];
        j.invBindMatrix   = InvertTRS(j.bindWorldMatrix);
    }
}

void Skeleton::ComputeWorldTransforms(
    const std::vector<math::Mat4>& localTransforms,
    std::vector<math::Mat4>&       worldTransforms) const
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Forward Pass for World Transforms
    // -----------------------------------------------------------------------
    // Precondition: localTransforms[i] is the local-space matrix for joint i
    // (already incorporating the animated TRS, not the bind-pose TRS).
    //
    // We walk in index order (topologically sorted), so parent is always
    // computed before child.
    // -----------------------------------------------------------------------
    size_t n = m_joints.size();
    worldTransforms.resize(n, math::Mat4::Identity());

    for (size_t i = 0; i < n; ++i)
    {
        const Joint& j = m_joints[i];

        if (j.parentIndex < 0)
            worldTransforms[i] = localTransforms[i];
        else
            worldTransforms[i] = localTransforms[i]
                               * worldTransforms[static_cast<size_t>(j.parentIndex)];
    }
}

void Skeleton::ComputeSkinMatrices(
    const std::vector<math::Mat4>& worldTransforms,
    std::vector<math::Mat4>&       skinMatrices) const
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Skin Matrix Computation
    // -----------------------------------------------------------------------
    // In row-major (D3D11) convention, matrix multiplication `A * B` applies
    // A first (transforms points out of A's space) then B (into B's space).
    //
    // So:  invBind[i] * world[i]
    //   = (move vertex from bind-pose model space → joint-i local space)
    //     then (move from joint-i local space → current world space)
    //
    // Vertex shader reads this as:
    //   vertexWorld = Σ weight[i] * (skinMatrix[i] * vertexBindPose)
    // -----------------------------------------------------------------------
    size_t n = m_joints.size();
    skinMatrices.resize(n, math::Mat4::Identity());

    for (size_t i = 0; i < n; ++i)
    {
        // invBind × world → skin matrix (row-major convention)
        skinMatrices[i] = m_joints[i].invBindMatrix * worldTransforms[i];
    }
}

std::vector<math::Mat4> Skeleton::GetBindPoseLocalMatrices() const
{
    std::vector<math::Mat4> locals;
    locals.reserve(m_joints.size());

    for (const auto& j : m_joints)
        locals.push_back(math::Mat4::TRS(j.bindTranslation, j.bindRotation, j.bindScale));

    return locals;
}

const Joint* Skeleton::FindJoint(const std::string& name) const
{
    for (const auto& j : m_joints)
        if (j.name == name)
            return &j;
    return nullptr;
}

} // namespace animation
} // namespace engine
