/**
 * @file animation_system.cpp
 * @brief AnimationSystem implementation — per-frame animator update.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 */

#include "engine/animation/animation_system.hpp"

#include <algorithm>
#include <iostream>

namespace engine {
namespace animation {

// ===========================================================================
// Asset registration
// ===========================================================================

void AnimationSystem::RegisterSkeleton(Skeleton skeleton)
{
    const std::string id = skeleton.Id();
    skeleton.Build();  // Pre-compute bind-pose world matrices + inverse bind matrices.
    m_skeletons.emplace(id, std::move(skeleton));
}

void AnimationSystem::RegisterClip(AnimClip clip)
{
    const std::string id = clip.Id();
    m_clips.emplace(id, std::move(clip));
}

void AnimationSystem::RegisterBlendTree(const std::string&         id,
                                         std::unique_ptr<BlendTree> blendTree)
{
    m_blendTrees.emplace(id, std::move(blendTree));
}

// ===========================================================================
// Accessors
// ===========================================================================

const Skeleton* AnimationSystem::GetSkeleton(const std::string& id) const
{
    auto it = m_skeletons.find(id);
    return (it != m_skeletons.end()) ? &it->second : nullptr;
}

const AnimClip* AnimationSystem::GetClip(const std::string& id) const
{
    auto it = m_clips.find(id);
    return (it != m_clips.end()) ? &it->second : nullptr;
}

const BlendTree* AnimationSystem::GetBlendTree(const std::string& id) const
{
    auto it = m_blendTrees.find(id);
    return (it != m_blendTrees.end()) ? it->second.get() : nullptr;
}

// ===========================================================================
// Per-frame update
// ===========================================================================

void AnimationSystem::Update(World& world, float deltaTime)
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — ECS Component Iteration
    // -----------------------------------------------------------------------
    // World::GetComponentsOfType<T>() returns a range over all components of
    // type T.  We call UpdateAnimator for each AnimatorComponent found.
    //
    // The ECS stores components in dense arrays (ComponentStorage<T>), so
    // this iteration has good cache locality — all AnimatorComponent data is
    // in one contiguous block of memory, minimising cache misses.
    // -----------------------------------------------------------------------
    auto& storage = world.GetComponentStorage<AnimatorComponent>();
    for (auto& animator : storage)
        UpdateAnimator(animator, deltaTime);
}

void AnimationSystem::UpdateAnimator(AnimatorComponent& animator, float deltaTime)
{
    // Skip paused / inactive animators.
    if (!animator.isPlaying)
        return;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Advancing Playback Time
    // -----------------------------------------------------------------------
    // We advance currentTime by deltaTime * playbackSpeed.
    // playbackSpeed of 1.0 = real-time; 2.0 = double speed; 0.5 = half speed.
    // -----------------------------------------------------------------------
    animator.currentTime += deltaTime * animator.playbackSpeed;

    // -----------------------------------------------------------------------
    // Resolve skeleton.
    // -----------------------------------------------------------------------
    if (animator.skeletonID.empty())
        return;

    const Skeleton* skeleton = GetSkeleton(animator.skeletonID);
    if (!skeleton)
        return;

    int jointCount = skeleton->JointCount();
    if (jointCount <= 0)
        return;

    // -----------------------------------------------------------------------
    // Evaluate animation (blend tree takes priority over single clip).
    // -----------------------------------------------------------------------
    std::vector<math::Mat4> localTransforms;
    localTransforms.reserve(static_cast<size_t>(jointCount));

    bool evaluated = false;

    if (!animator.blendTreeID.empty())
    {
        const BlendTree* tree = GetBlendTree(animator.blendTreeID);
        if (tree)
        {
            evaluated = tree->Evaluate(animator.currentTime, *skeleton, localTransforms);
        }
    }

    if (!evaluated && !animator.currentClipID.empty())
    {
        const AnimClip* clip = GetClip(animator.currentClipID);
        if (clip)
        {
            clip->Evaluate(animator.currentTime,
                           skeleton->GetBindPoseLocalMatrices(),
                           localTransforms);
            evaluated = true;
        }
    }

    if (!evaluated)
    {
        // No valid clip or blend tree — use bind pose.
        localTransforms = skeleton->GetBindPoseLocalMatrices();
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — World and Skin Matrices
    // -----------------------------------------------------------------------
    // Two more passes over the joint array:
    //   1. ComputeWorldTransforms: multiply local × parent (forward pass).
    //   2. ComputeSkinMatrices:    multiply world × invBind (per-joint).
    //
    // The resulting skin matrices are stored in animator.jointMatrices.
    // A future milestone (M4b) will upload these to a D3D11 constant buffer
    // for the skinned mesh vertex shader.
    // -----------------------------------------------------------------------
    std::vector<math::Mat4> worldTransforms;
    skeleton->ComputeWorldTransforms(localTransforms, worldTransforms);

    std::vector<math::Mat4> skinMatrices;
    skeleton->ComputeSkinMatrices(worldTransforms, skinMatrices);

    // Copy skin matrices into the AnimatorComponent's fixed-size array.
    int n = std::min(jointCount, kMaxJoints);
    for (int i = 0; i < n; ++i)
        animator.jointMatrices[i] = skinMatrices[static_cast<size_t>(i)];

    animator.jointCount = n;
}

} // namespace animation
} // namespace engine
