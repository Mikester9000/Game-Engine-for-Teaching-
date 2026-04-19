/**
 * @file blend_tree.cpp
 * @brief Blend tree node evaluation — ClipNode, LinearBlendNode, BlendTree.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 */

#include "engine/animation/blend_tree.hpp"

#include <cmath>
#include <algorithm>
#include <cassert>

namespace engine {
namespace animation {

// ===========================================================================
// Local helpers — matrix TRS decompose/recompose for blending
// ===========================================================================

namespace {

// ---------------------------------------------------------------------------
// TEACHING NOTE — Extracting Translation from a Row-Major Mat4
// ---------------------------------------------------------------------------
// In row-major order the translation is stored in row 3 (m[3][0..2]).
// (In column-major OpenGL convention it would be in column 3 instead.)
// ---------------------------------------------------------------------------
math::Vec3 ExtractTranslation(const math::Mat4& m)
{
    return { m.m[3][0], m.m[3][1], m.m[3][2] };
}

// ---------------------------------------------------------------------------
// TEACHING NOTE — Extracting Scale from a Row-Major Mat4
// ---------------------------------------------------------------------------
// The scale components are the lengths of each row (column in column-major).
// Row 0 = X axis, row 1 = Y axis, row 2 = Z axis.
// ---------------------------------------------------------------------------
math::Vec3 ExtractScale(const math::Mat4& m)
{
    auto len = [](float a, float b, float c) { return std::sqrt(a*a + b*b + c*c); };
    return {
        len(m.m[0][0], m.m[0][1], m.m[0][2]),
        len(m.m[1][0], m.m[1][1], m.m[1][2]),
        len(m.m[2][0], m.m[2][1], m.m[2][2])
    };
}

// ---------------------------------------------------------------------------
// TEACHING NOTE — Extracting Rotation Quaternion from a Row-Major Mat4
// ---------------------------------------------------------------------------
// We divide out the scale to get the pure rotation matrix, then convert the
// 3×3 rotation matrix to a quaternion using Shepperd's method.
//
// Shepperd's method selects the formula that gives the best numerical
// precision based on the largest of the four candidate values
// (w², x², y², z²), avoiding near-zero denominators.
// ---------------------------------------------------------------------------
math::Quat ExtractRotation(const math::Mat4& mat, const math::Vec3& scale)
{
    // Guard against zero scale.
    float sx = (scale.x > math::kEps) ? scale.x : 1.0f;
    float sy = (scale.y > math::kEps) ? scale.y : 1.0f;
    float sz = (scale.z > math::kEps) ? scale.z : 1.0f;

    // Normalise each row to remove scale.
    float r00 = mat.m[0][0] / sx, r01 = mat.m[0][1] / sx, r02 = mat.m[0][2] / sx;
    float r10 = mat.m[1][0] / sy, r11 = mat.m[1][1] / sy, r12 = mat.m[1][2] / sy;
    float r20 = mat.m[2][0] / sz, r21 = mat.m[2][1] / sz, r22 = mat.m[2][2] / sz;

    float trace = r00 + r11 + r22;
    math::Quat q;

    if (trace > 0.0f)
    {
        float s = 0.5f / std::sqrt(trace + 1.0f);
        q.w = 0.25f / s;
        q.x = (r21 - r12) * s;
        q.y = (r02 - r20) * s;
        q.z = (r10 - r01) * s;
    }
    else if (r00 > r11 && r00 > r22)
    {
        float s = 2.0f * std::sqrt(1.0f + r00 - r11 - r22);
        q.w = (r21 - r12) / s;
        q.x = 0.25f * s;
        q.y = (r01 + r10) / s;
        q.z = (r02 + r20) / s;
    }
    else if (r11 > r22)
    {
        float s = 2.0f * std::sqrt(1.0f + r11 - r00 - r22);
        q.w = (r02 - r20) / s;
        q.x = (r01 + r10) / s;
        q.y = 0.25f * s;
        q.z = (r12 + r21) / s;
    }
    else
    {
        float s = 2.0f * std::sqrt(1.0f + r22 - r00 - r11);
        q.w = (r10 - r01) / s;
        q.x = (r02 + r20) / s;
        q.y = (r12 + r21) / s;
        q.z = 0.25f * s;
    }

    return q.Normalized();
}

} // anonymous namespace

// ===========================================================================
// ClipNode
// ===========================================================================

void ClipNode::Evaluate(float                   time,
                         const Skeleton&         skeleton,
                         std::vector<math::Mat4>& outLocalTransforms) const
{
    if (!m_clip)
    {
        // No clip — fall back to bind pose.
        outLocalTransforms = skeleton.GetBindPoseLocalMatrices();
        return;
    }

    m_clip->Evaluate(time,
                     skeleton.GetBindPoseLocalMatrices(),
                     outLocalTransforms);
}

// ===========================================================================
// LinearBlendNode
// ===========================================================================

void LinearBlendNode::Evaluate(float                   time,
                                 const Skeleton&         skeleton,
                                 std::vector<math::Mat4>& outLocalTransforms) const
{
    if (!m_childA || !m_childB)
    {
        if (m_childA) { m_childA->Evaluate(time, skeleton, outLocalTransforms); return; }
        if (m_childB) { m_childB->Evaluate(time, skeleton, outLocalTransforms); return; }
        outLocalTransforms = skeleton.GetBindPoseLocalMatrices();
        return;
    }

    int n = skeleton.JointCount();
    std::vector<math::Mat4> matA, matB;
    matA.reserve(static_cast<size_t>(n));
    matB.reserve(static_cast<size_t>(n));

    m_childA->Evaluate(time, skeleton, matA);
    m_childB->Evaluate(time, skeleton, matB);

    float w = std::max(0.0f, std::min(m_weight, 1.0f));
    float wA = 1.0f - w;
    float wB = w;

    outLocalTransforms.resize(static_cast<size_t>(n));

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Per-Joint TRS Blend
    // -----------------------------------------------------------------------
    // We cannot directly lerp matrices — that would produce shear / non-
    // uniform scale artifacts.  Instead we:
    //   1. Extract TRS from each matrix (decompose).
    //   2. Lerp translation and scale, slerp rotation.
    //   3. Rebuild the TRS matrix (recompose).
    //
    // This is the standard "decompose-blend-recompose" pattern used in
    // every production animation engine.
    // -----------------------------------------------------------------------
    for (int i = 0; i < n; ++i)
    {
        const math::Mat4& mA = matA[static_cast<size_t>(i)];
        const math::Mat4& mB = matB[static_cast<size_t>(i)];

        math::Vec3 scaleA = ExtractScale(mA);
        math::Vec3 scaleB = ExtractScale(mB);

        math::Vec3 transA = ExtractTranslation(mA);
        math::Vec3 transB = ExtractTranslation(mB);

        math::Quat rotA   = ExtractRotation(mA, scaleA);
        math::Quat rotB   = ExtractRotation(mB, scaleB);

        math::Vec3 blendedTrans = math::Vec3::Lerp(transA, transB, wB);
        math::Vec3 blendedScale = math::Vec3::Lerp(scaleA, scaleB, wB);
        math::Quat blendedRot   = math::Quat::Slerp(rotA,  rotB,   wB);

        outLocalTransforms[static_cast<size_t>(i)] =
            math::Mat4::TRS(blendedTrans, blendedRot, blendedScale);
    }
}

// ===========================================================================
// BlendTree
// ===========================================================================

bool BlendTree::Evaluate(float                   time,
                          const Skeleton&         skeleton,
                          std::vector<math::Mat4>& outLocalTransforms) const
{
    if (!m_root)
        return false;

    m_root->Evaluate(time, skeleton, outLocalTransforms);
    return true;
}

} // namespace animation
} // namespace engine
