/**
 * @file animation_system.hpp
 * @brief ECS AnimationSystem — drives AnimatorComponent each frame.
 *
 * ============================================================================
 * TEACHING NOTE — How AnimationSystem Fits in the ECS
 * ============================================================================
 * AnimationSystem is an ECS *system*: it iterates over every entity that has
 * an AnimatorComponent and advances its animation state.
 *
 * Per frame the system does:
 *   1. Advance currentTime by (deltaTime * playbackSpeed).
 *   2. Evaluate the blend tree (or direct clip) at the new currentTime.
 *      This fills an array of local-space joint matrices.
 *   3. Ask the skeleton to compute world-space joint matrices.
 *   4. Compute skin matrices (world × invBind) for GPU skinning.
 *   5. Store the skin matrices in AnimatorComponent::jointMatrices.
 *      The render system reads these to upload to a D3D11 constant buffer.
 *
 * TEACHING NOTE — Separation of Animation and Rendering
 * AnimationSystem only writes to CPU-side arrays.  It does NOT upload
 * anything to the GPU.  The D3D11 skinning render pass (M4b) will read
 * AnimatorComponent::jointMatrices and upload them to a constant buffer.
 * This separation keeps the animation system platform-independent (it runs
 * identically on Windows and Linux).
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 */

#pragma once

#include "engine/animation/skeleton.hpp"
#include "engine/animation/anim_clip.hpp"
#include "engine/animation/blend_tree.hpp"
#include "engine/math/math_types.hpp"
#include "engine/ecs/ECS.hpp"

#include <string>
#include <unordered_map>
#include <memory>

namespace engine {
namespace animation {

// ===========================================================================
// AnimationSystem
// ===========================================================================

/**
 * @class AnimationSystem
 * @brief ECS system that evaluates animations each frame and fills joint matrices.
 *
 * TEACHING NOTE — Asset Registries Inside Systems
 * The system owns maps from ID strings → Skeleton / AnimClip / BlendTree.
 * This matches the pattern in all other game systems (InventorySystem,
 * QuestSystem, etc.) where the system owns the "database" of definitions.
 *
 * At M7 (world streaming) these maps will migrate to the AssetDB.  For M4
 * they live here to keep things simple.
 */
class AnimationSystem
{
public:
    AnimationSystem()  = default;
    ~AnimationSystem() = default;

    // Non-copyable — owns unique_ptr resources.
    AnimationSystem(const AnimationSystem&)            = delete;
    AnimationSystem& operator=(const AnimationSystem&) = delete;

    // -----------------------------------------------------------------------
    // Asset registration
    // -----------------------------------------------------------------------

    /**
     * @brief Register a skeleton by its ID string.
     *
     * @param skeleton  Skeleton to register (moved in; system takes ownership).
     *
     * TEACHING NOTE — std::move
     * We take the skeleton by value and move it into the map.  This avoids a
     * deep copy of the joint array.  After the call, the caller's skeleton
     * object is in a valid-but-unspecified state (effectively empty).
     */
    void RegisterSkeleton(Skeleton skeleton);

    /**
     * @brief Register an animation clip by its ID string.
     */
    void RegisterClip(AnimClip clip);

    /**
     * @brief Register a blend tree (ownership transferred).
     * @param id         ID string the animator will reference.
     * @param blendTree  BlendTree to register (unique_ptr; ownership transferred).
     */
    void RegisterBlendTree(const std::string& id,
                           std::unique_ptr<BlendTree> blendTree);

    // -----------------------------------------------------------------------
    // Per-frame update
    // -----------------------------------------------------------------------

    /**
     * @brief Advance all AnimatorComponents and compute joint matrices.
     *
     * @param world      The ECS World containing AnimatorComponent entities.
     * @param deltaTime  Time elapsed since the last frame in seconds.
     *
     * TEACHING NOTE — ECS Update Pattern
     * We call World::GetComponents<AnimatorComponent>() to obtain every
     * entity with an AnimatorComponent, then update each one.  This matches
     * the pattern used by CombatSystem, AISystem, etc.
     */
    void Update(World& world, float deltaTime);

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------

    const Skeleton*   GetSkeleton(const std::string& id) const;
    const AnimClip*   GetClip(const std::string& id)     const;
    const BlendTree*  GetBlendTree(const std::string& id) const;

private:
    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Update one AnimatorComponent: advance time, evaluate, compute matrices.
     */
    void UpdateAnimator(AnimatorComponent& animator, float deltaTime);

    // -----------------------------------------------------------------------
    // Asset stores
    // -----------------------------------------------------------------------

    std::unordered_map<std::string, Skeleton>                  m_skeletons;
    std::unordered_map<std::string, AnimClip>                  m_clips;
    std::unordered_map<std::string, std::unique_ptr<BlendTree>> m_blendTrees;
};

} // namespace animation
} // namespace engine
