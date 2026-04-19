/**
 * @file anim_clip.cpp
 * @brief Animation clip evaluation — lerp/slerp keyframe sampling.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 */

#include "engine/animation/anim_clip.hpp"

#include <algorithm>
#include <cassert>

namespace engine {
namespace animation {

// ===========================================================================
// AnimChannel
// ===========================================================================

void AnimChannel::AddKeyframe(float time,
                               const math::Vec3& t,
                               const math::Quat& r,
                               const math::Vec3& s)
{
    Keyframe kf;
    kf.time        = time;
    kf.translation = t;
    kf.rotation    = r.Normalized();
    kf.scale       = s;

    // Insert in sorted order.
    auto it = std::lower_bound(m_keyframes.begin(), m_keyframes.end(), kf,
                               [](const Keyframe& a, const Keyframe& b)
                               { return a.time < b.time; });
    m_keyframes.insert(it, kf);
}

Keyframe AnimChannel::Evaluate(float t) const
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Handling Edge Cases
    // -----------------------------------------------------------------------
    // No keyframes → return the identity/zero keyframe at time t.
    // t <= first → clamp to first keyframe.
    // t >= last  → clamp to last  keyframe.
    // Otherwise  → binary search for the surrounding pair, then interpolate.
    // -----------------------------------------------------------------------
    if (m_keyframes.empty())
    {
        Keyframe kf;
        kf.time = t;
        return kf;
    }

    if (t <= m_keyframes.front().time)
        return m_keyframes.front();
    if (t >= m_keyframes.back().time)
        return m_keyframes.back();

    // Binary search for the first keyframe with time > t.
    // This gives us the "hi" keyframe; "lo" is one before it.
    auto it = std::upper_bound(m_keyframes.begin(), m_keyframes.end(), t,
                               [](float val, const Keyframe& kf)
                               { return val < kf.time; });

    const Keyframe& kfB = *it;
    const Keyframe& kfA = *(it - 1);

    float dt = kfB.time - kfA.time;
    float alpha = (dt > math::kEps) ? ((t - kfA.time) / dt) : 0.0f;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Lerp vs Slerp
    // Translation and scale are interpolated with lerp (straight-line blend).
    // Rotation uses slerp (arc on the 4-D unit quaternion sphere) to ensure
    // smooth, constant-speed rotation without the "spinning the long way"
    // artefact that naive lerp can produce.
    // -----------------------------------------------------------------------
    Keyframe result;
    result.time        = t;
    result.translation = math::Vec3::Lerp(kfA.translation, kfB.translation, alpha);
    result.rotation    = math::Quat::Slerp(kfA.rotation,    kfB.rotation,    alpha);
    result.scale       = math::Vec3::Lerp(kfA.scale,       kfB.scale,       alpha);
    return result;
}

// ===========================================================================
// AnimClip
// ===========================================================================

AnimChannel& AnimClip::AddChannel(int boneIndex, const std::string& boneName)
{
    m_channels.emplace_back(boneIndex, boneName);
    return m_channels.back();
}

void AnimClip::Evaluate(float                         time,
                        const std::vector<math::Mat4>& bindPoseLocals,
                        std::vector<math::Mat4>&       outLocalTransforms) const
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Wrapping Loopable Clips
    // -----------------------------------------------------------------------
    // For loopable clips we wrap the playback time so it stays within
    // [0, duration].  fmod() gives the remainder, e.g.:
    //   fmod(2.1, 2.0) = 0.1  → clip restarts after it reaches the end.
    //
    // For non-loopable clips we clamp t to [0, duration].
    // -----------------------------------------------------------------------
    float t = time;
    if (m_loopable && m_durationSeconds > math::kEps)
    {
        t = std::fmod(t, m_durationSeconds);
        if (t < 0.0f) t += m_durationSeconds;
    }
    else
    {
        // Clamp to [0, duration].
        t = std::max(0.0f, std::min(t, m_durationSeconds));
    }

    // Start from the bind-pose (default for joints with no channel in this clip).
    outLocalTransforms = bindPoseLocals;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Per-Channel Evaluation
    // -----------------------------------------------------------------------
    // For each channel in this clip, evaluate the keyframes at time t and
    // write the resulting TRS matrix into outLocalTransforms at the joint index.
    // -----------------------------------------------------------------------
    for (const auto& channel : m_channels)
    {
        int boneIdx = channel.BoneIndex();
        if (boneIdx < 0 || boneIdx >= static_cast<int>(outLocalTransforms.size()))
            continue;

        Keyframe kf = channel.Evaluate(t);

        // Build the local TRS matrix from the evaluated keyframe components.
        // TEACHING NOTE — TRS matrix order:
        //   Row-major D3D11: Scale · Rotation · Translation
        outLocalTransforms[static_cast<size_t>(boneIdx)] =
            math::Mat4::TRS(kf.translation, kf.rotation, kf.scale);
    }
}

} // namespace animation
} // namespace engine
