/**
 * @file camera_rig.cpp
 * @brief CameraRig implementation — keyframe lookup and linear interpolation.
 *
 * ============================================================================
 * TEACHING NOTE — Implementation Strategy
 * ============================================================================
 *
 * The two responsibilities of this file are:
 *
 *   1. AUTHORING: AddKeyframe() validates the insertion order and stores the
 *      keyframe.  Sorted order is required by Evaluate()'s binary search.
 *
 *   2. EVALUATION: Evaluate(t) performs a binary search to find the two
 *      keyframes that bracket t, then linearly interpolates between them.
 *
 * ─── Binary search for the bracket ─────────────────────────────────────────
 *
 * We use std::upper_bound with a lambda comparator to find the first
 * keyframe whose time is > t.  The keyframe immediately before it is the
 * lower bracket.
 *
 *   keyframes: [0.0, 1.0, 2.0, 3.0, 4.0]
 *   t = 1.5:
 *     upper_bound → iterator to 2.0 (index 2)
 *     lower = index 1 (time 1.0)
 *     upper = index 2 (time 2.0)
 *     alpha = (1.5 - 1.0) / (2.0 - 1.0) = 0.5
 *
 * TEACHING NOTE — std::upper_bound
 * std::upper_bound(begin, end, value, comp) returns an iterator to the
 * first element for which comp(value, element) is true.  Here comp is
 * (t < kf.time), so it finds the first keyframe with time > t — exactly
 * the upper bracket we need.  Time complexity: O(log N).
 *
 * ─── Edge cases ─────────────────────────────────────────────────────────────
 *
 *   • t ≤ first keyframe time  → return first keyframe (no interpolation).
 *   • t ≥ last keyframe time   → return last keyframe (no interpolation).
 *   • 0 or 1 keyframe          → return default / single keyframe.
 *   • upper == lower time      → alpha = 0 (avoid div-by-zero).
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2025
 * C++ Standard: C++17
 * Platform: Windows / Linux (no platform dependencies)
 */

#include "engine/cinematics/camera_rig.hpp"

#include <algorithm>  // std::upper_bound
#include <cmath>      // std::fabs

namespace engine {
namespace cinematics {

// ---------------------------------------------------------------------------
// CameraRig::AddKeyframe
// ---------------------------------------------------------------------------

void CameraRig::AddKeyframe(float time,
                             engine::math::Vec3 pos,
                             engine::math::Vec3 lookAt,
                             float fovDeg)
{
    // TEACHING NOTE — Ascending-time invariant
    // We assert rather than silently reorder because out-of-order keyframes
    // are almost always a content authoring bug.  Asserting catches the
    // mistake at the call site with a helpful stack trace, while silently
    // sorting would hide the bug and produce confusing camera jumps.
    if (!m_keyframes.empty())
    {
        assert(time >= m_keyframes.back().time &&
               "CameraRig: keyframes must be added in non-decreasing time order.");
    }

    m_keyframes.emplace_back(time, pos, lookAt, fovDeg);
}

// ---------------------------------------------------------------------------
// CameraRig::Evaluate
// ---------------------------------------------------------------------------

CameraRigSample CameraRig::Evaluate(float t) const
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Empty and single-keyframe guards
    // -----------------------------------------------------------------------
    // These guards keep the rest of the function's logic simple — we only
    // need to handle the general case (≥2 keyframes) below.
    // -----------------------------------------------------------------------

    if (m_keyframes.empty())
    {
        // Return default-constructed sample (position/lookAt = {0,0,0}, fov=60°).
        return {};
    }

    if (m_keyframes.size() == 1)
    {
        const auto& kf = m_keyframes.front();
        return { kf.position, kf.lookAt, kf.fovDeg };
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Clamp to valid range
    // -----------------------------------------------------------------------
    // Clamp t so that t < first keyframe time returns the first keyframe, and
    // t > last keyframe time returns the last keyframe.  This means the caller
    // never needs to guard for out-of-range values.
    // -----------------------------------------------------------------------

    const float tFirst = m_keyframes.front().time;
    const float tLast  = m_keyframes.back().time;

    if (t <= tFirst)
    {
        const auto& kf = m_keyframes.front();
        return { kf.position, kf.lookAt, kf.fovDeg };
    }
    if (t >= tLast)
    {
        const auto& kf = m_keyframes.back();
        return { kf.position, kf.lookAt, kf.fovDeg };
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Binary search for keyframe bracket
    // -----------------------------------------------------------------------
    // std::upper_bound finds the first keyframe whose time is strictly > t.
    // The keyframe before it is the lower bracket.
    //
    // Example with keyframes at t = {0, 1, 2, 3} and query t = 1.4:
    //   upper_bound → iterator to kf[2] (time=2.0)
    //   lower = kf[1] (time=1.0)
    //   upper = kf[2] (time=2.0)
    //   alpha = (1.4 - 1.0) / (2.0 - 1.0) = 0.4
    // -----------------------------------------------------------------------

    auto upperIt = std::upper_bound(
        m_keyframes.begin(), m_keyframes.end(), t,
        [](float queryTime, const CameraKeyframe& kf) {
            return queryTime < kf.time;
        });

    // upperIt can never be begin() here because t > tFirst.
    const CameraKeyframe& upper = *upperIt;
    const CameraKeyframe& lower = *(upperIt - 1);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Linear interpolation factor (alpha)
    // -----------------------------------------------------------------------
    // alpha ∈ [0,1] tells us how far between lower and upper we are.
    // We guard against a degenerate case where upper.time == lower.time
    // (two keyframes at the same instant) by returning alpha = 0, which
    // yields the lower keyframe.
    // -----------------------------------------------------------------------

    const float span = upper.time - lower.time;
    const float alpha = (span > 1e-7f) ? (t - lower.time) / span : 0.0f;

    CameraRigSample result;

    // TEACHING NOTE — Lerp for translation is correct here.
    // We use Vec3::Lerp for both position and look-at.  We do NOT slerp the
    // "direction" because we store a world-space LOOK-AT POINT, not a direction
    // quaternion.  Lerping a look-at point is fine — it smoothly moves the
    // target across the scene.
    result.position = engine::math::Vec3::Lerp(lower.position, upper.position, alpha);
    result.lookAt   = engine::math::Vec3::Lerp(lower.lookAt,   upper.lookAt,   alpha);

    // TEACHING NOTE — FOV is a scalar; plain linear interpolation is correct.
    // FOV changes feel like zoom in/out.  In film this is called a "dolly zoom"
    // (or "Vertigo effect") when combined with a camera that moves opposite to
    // the FOV change.
    result.fovDeg = lower.fovDeg + (upper.fovDeg - lower.fovDeg) * alpha;

    return result;
}

// ---------------------------------------------------------------------------
// CameraRig::Duration
// ---------------------------------------------------------------------------

float CameraRig::Duration() const
{
    if (m_keyframes.size() < 2)
        return 0.0f;
    return m_keyframes.back().time - m_keyframes.front().time;
}

// ---------------------------------------------------------------------------
// CameraRig::KeyframeCount
// ---------------------------------------------------------------------------

int CameraRig::KeyframeCount() const
{
    return static_cast<int>(m_keyframes.size());
}

} // namespace cinematics
} // namespace engine
