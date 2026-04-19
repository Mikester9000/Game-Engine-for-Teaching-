/**
 * @file anim_clip.hpp
 * @brief Animation clip — per-bone keyframe channels with lerp/slerp evaluation.
 *
 * ============================================================================
 * TEACHING NOTE — What is an Animation Clip?
 * ============================================================================
 * An animation clip stores one continuous motion (e.g. "walk", "attack",
 * "idle").  It is divided into *channels*, one per joint that moves during
 * the clip.  Each channel holds a list of *keyframes* — sampled points in
 * time where the joint's TRS transform is recorded.
 *
 * At runtime, the AnimationSystem advances a *playback time* t (in seconds)
 * and asks each channel to *evaluate* — find the two surrounding keyframes
 * and interpolate between them:
 *
 *   translation: lerp  (straight-line; accurate for position/scale)
 *   rotation:    slerp (arc; accurate for orientation, no gimbal lock)
 *   scale:       lerp
 *
 * This file matches the shared schema:  shared/schemas/anim_clip.schema.json
 * Reference Python model:              tools/anim_authoring/animation_engine/model/__init__.py
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 */

#pragma once

#include "engine/math/math_types.hpp"

#include <string>
#include <vector>

namespace engine {
namespace animation {

// ===========================================================================
// Keyframe
// ===========================================================================

/**
 * @struct Keyframe
 * @brief One sampled TRS transform at a given time in a single channel.
 */
struct Keyframe
{
    float       time        = 0.0f;
    math::Vec3  translation { 0.0f, 0.0f, 0.0f };
    math::Quat  rotation    { 0.0f, 0.0f, 0.0f, 1.0f };
    math::Vec3  scale       { 1.0f, 1.0f, 1.0f };
};

// ===========================================================================
// AnimChannel
// ===========================================================================

/**
 * @class AnimChannel
 * @brief Keyframe data for a single joint.
 *
 * TEACHING NOTE — Sparse Channels
 * Not every joint moves in every animation clip.  We only store channels for
 * joints that actually have keyframes in a given clip.  Joints not present in
 * the channel list keep their bind-pose transform.
 */
class AnimChannel
{
public:
    AnimChannel() = default;
    AnimChannel(int boneIndex, std::string boneName)
        : m_boneIndex(boneIndex), m_boneName(std::move(boneName)) {}

    /**
     * @brief Add a keyframe to this channel (kept sorted by time).
     * @param time  Playback time in seconds.
     * @param t     Translation at this time.
     * @param r     Rotation at this time.
     * @param s     Scale at this time.
     */
    void AddKeyframe(float time,
                     const math::Vec3& t = math::Vec3::Zero(),
                     const math::Quat& r = math::Quat::Identity(),
                     const math::Vec3& s = math::Vec3::One());

    /**
     * @brief Evaluate the channel at time t via lerp/slerp.
     *
     * TEACHING NOTE — Binary Search + Lerp/Slerp
     * We binary-search for the two surrounding keyframes in O(log N) time.
     * For most clips N ≤ 30 keyframes per channel, so even a linear scan
     * would be fast enough, but binary search scales to dense motion-capture
     * data (hundreds of keyframes per channel) without modification.
     *
     * Clamping: t < first keyframe → return first keyframe.
     *           t > last  keyframe → return last  keyframe.
     * (Looping is handled at the AnimClip level by wrapping t before calling.)
     */
    Keyframe Evaluate(float t) const;

    int                        BoneIndex() const { return m_boneIndex; }
    const std::string&         BoneName()  const { return m_boneName;  }
    const std::vector<Keyframe>& Keyframes() const { return m_keyframes; }

private:
    int                  m_boneIndex = -1;
    std::string          m_boneName;
    std::vector<Keyframe> m_keyframes;  ///< Sorted by time (ascending)
};

// ===========================================================================
// AnimClip
// ===========================================================================

/**
 * @class AnimClip
 * @brief Complete animation clip with multiple per-bone channels.
 *
 * TEACHING NOTE — Clip Evaluation into Local Transforms
 * AnimClip::Evaluate(time, skeleton, outLocalTransforms) fills one Mat4 per
 * joint in the skeleton.  Joints that have no channel in this clip are left
 * as their bind-pose local matrix — so clips can be "sparse" (only animate
 * the joints they care about) and stacked with other clips in a blend tree.
 */
class AnimClip
{
public:
    AnimClip() = default;

    // -----------------------------------------------------------------------
    // Building the clip
    // -----------------------------------------------------------------------

    /**
     * @brief Add a channel for the given joint and return it.
     */
    AnimChannel& AddChannel(int boneIndex, const std::string& boneName);

    // -----------------------------------------------------------------------
    // Evaluation
    // -----------------------------------------------------------------------

    /**
     * @brief Evaluate the clip at `time` seconds, filling localTransforms.
     *
     * @param time            Playback time in seconds.
     * @param bindPoseLocals  Bind-pose local matrices (used for joints with no channel).
     * @param outLocalTransforms  Output: one local-space Mat4 per joint.
     *
     * TEACHING NOTE — How Sparse Channels Work
     * For each joint in the skeleton, we look up whether this clip has a
     * channel for that joint.  If yes, we evaluate it and build a TRS Mat4.
     * If no, we copy the bind-pose matrix.  This "fill from bind pose"
     * strategy lets you author a clip that only moves the legs, for example,
     * and blend it with a clip that only moves the arms.
     */
    void Evaluate(float                         time,
                  const std::vector<math::Mat4>& bindPoseLocals,
                  std::vector<math::Mat4>&       outLocalTransforms) const;

    // -----------------------------------------------------------------------
    // Accessors / setters
    // -----------------------------------------------------------------------

    const std::string& Id()       const { return m_id; }
    const std::string& Name()     const { return m_name; }
    float              Duration() const { return m_durationSeconds; }
    bool               Loopable() const { return m_loopable; }

    void SetId(const std::string& id)           { m_id              = id;  }
    void SetName(const std::string& name)        { m_name            = name; }
    void SetDuration(float seconds)              { m_durationSeconds = seconds; }
    void SetLoopable(bool loop)                  { m_loopable        = loop; }
    void SetSkeletonId(const std::string& skelId){ m_skeletonId      = skelId; }

    const std::vector<AnimChannel>& Channels() const { return m_channels; }

private:
    std::string               m_id;
    std::string               m_name;
    std::string               m_skeletonId;
    float                     m_durationSeconds = 1.0f;
    float                     m_fps             = 30.0f;
    bool                      m_loopable        = false;
    std::vector<AnimChannel>  m_channels;       ///< One entry per animated joint
};

} // namespace animation
} // namespace engine
