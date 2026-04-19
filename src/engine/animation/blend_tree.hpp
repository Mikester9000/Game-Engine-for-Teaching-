/**
 * @file blend_tree.hpp
 * @brief Animation blend tree — weighted blending of clips.
 *
 * ============================================================================
 * TEACHING NOTE — What is a Blend Tree?
 * ============================================================================
 * A blend tree is a graph of *nodes* that produce arrays of local joint
 * transforms.  Leaf nodes sample individual animation clips.  Interior nodes
 * blend the outputs of their children with weights.
 *
 * Example:
 *
 *         LinearBlend (weight=0.3)
 *        /                      \
 *   ClipNode("idle")        ClipNode("walk")
 *
 * If weight = 0.3:
 *   output[i] = 0.7 * idle[i] + 0.3 * walk[i]  (for every joint i)
 *
 * In a production engine the blend tree would include 1D/2D parametric blends,
 * additive layers, IK override nodes, and state-machine-driven transitions.
 * Here we implement the two essential building blocks: ClipNode and LinearBlend.
 *
 * TEACHING NOTE — Design Pattern (Composite)
 * The blend tree uses the *Composite* design pattern: BlendNode is an
 * abstract base, ClipNode and LinearBlendNode are concrete leaf/branch types.
 * AnimationSystem only sees the BlendNode interface, so adding new node types
 * (e.g. AdditiveNode, MaskNode) requires no changes to AnimationSystem.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 */

#pragma once

#include "engine/animation/anim_clip.hpp"
#include "engine/animation/skeleton.hpp"
#include "engine/math/math_types.hpp"

#include <memory>
#include <string>
#include <vector>

namespace engine {
namespace animation {

// ===========================================================================
// BlendNode — abstract base
// ===========================================================================

/**
 * @class BlendNode
 * @brief Abstract node in the blend tree.
 *
 * TEACHING NOTE — Pure Virtual Interface
 * Declaring Evaluate() as pure virtual forces every subclass to provide an
 * implementation.  The caller (AnimationSystem) never needs to know which
 * concrete type it is holding.
 */
class BlendNode
{
public:
    virtual ~BlendNode() = default;

    /**
     * @brief Evaluate the node and fill outLocalTransforms.
     *
     * @param time           Current playback time in seconds.
     * @param skeleton       The skeleton being animated (for joint count / bind pose).
     * @param outLocalTransforms  Output: one local-space Mat4 per joint.
     */
    virtual void Evaluate(float                   time,
                           const Skeleton&         skeleton,
                           std::vector<math::Mat4>& outLocalTransforms) const = 0;
};

// ===========================================================================
// ClipNode — leaf: plays a single AnimClip
// ===========================================================================

/**
 * @class ClipNode
 * @brief Leaf node that plays a single animation clip.
 *
 * TEACHING NOTE — Leaf Nodes Hold Pointers, Not Copies
 * ClipNode holds a raw pointer (non-owning) to an AnimClip.  Ownership of
 * AnimClip objects stays in the asset manager / AnimationSystem.  This
 * avoids expensive copies and keeps the blend tree lightweight.
 */
class ClipNode : public BlendNode
{
public:
    /**
     * @param clip  The animation clip to play (non-owning; must outlive this node).
     */
    explicit ClipNode(const AnimClip* clip) : m_clip(clip) {}

    void Evaluate(float                   time,
                   const Skeleton&         skeleton,
                   std::vector<math::Mat4>& outLocalTransforms) const override;

    void SetClip(const AnimClip* clip) { m_clip = clip; }
    const AnimClip* Clip() const { return m_clip; }

private:
    const AnimClip* m_clip = nullptr;
};

// ===========================================================================
// LinearBlendNode — blend two child nodes by weight
// ===========================================================================

/**
 * @class LinearBlendNode
 * @brief Interior node that linearly blends two children by weight.
 *
 * TEACHING NOTE — Per-Joint Matrix Blending
 * We cannot directly lerp 4×4 matrices — they would not remain valid TRS
 * matrices after blending (their rows would no longer be orthonormal).
 *
 * Correct approach: decompose each joint's matrix back to TRS, blend the
 * components individually (lerp translation and scale, slerp rotation), then
 * rebuild the matrix.
 *
 * This decompose-blend-recompose approach is what every AAA engine uses for
 * additive blending.  It is slightly more expensive than matrix lerp but
 * produces geometrically correct results.
 *
 * weight = 0.0 → 100% childA
 * weight = 1.0 → 100% childB
 * weight = 0.5 → 50/50 blend
 */
class LinearBlendNode : public BlendNode
{
public:
    /**
     * @param childA  First input node (non-owning).
     * @param childB  Second input node (non-owning).
     * @param weight  Blend weight in [0, 1].  0 = all A, 1 = all B.
     */
    LinearBlendNode(BlendNode* childA, BlendNode* childB, float weight = 0.5f)
        : m_childA(childA), m_childB(childB), m_weight(weight) {}

    void Evaluate(float                   time,
                   const Skeleton&         skeleton,
                   std::vector<math::Mat4>& outLocalTransforms) const override;

    void  SetWeight(float w) { m_weight = w; }
    float GetWeight()  const { return m_weight; }

private:
    BlendNode* m_childA = nullptr;
    BlendNode* m_childB = nullptr;
    float      m_weight = 0.5f;  ///< 0 = all A, 1 = all B
};

// ===========================================================================
// BlendTree — owns a hierarchy of BlendNodes
// ===========================================================================

/**
 * @class BlendTree
 * @brief Container that owns a tree of blend nodes and exposes the root.
 *
 * TEACHING NOTE — Ownership Model
 * BlendTree owns all its nodes via unique_ptr.  The caller builds the tree
 * using CreateNode<T>(...), which returns a raw pointer for wiring, while
 * BlendTree retains ownership.  The tree is destroyed when BlendTree goes
 * out of scope.
 */
class BlendTree
{
public:
    BlendTree() = default;

    /**
     * @brief Create and own a new blend node of type T.
     * @param args  Constructor arguments forwarded to T.
     * @return Raw pointer to the new node (valid for wiring; do not delete).
     */
    template<typename T, typename... Args>
    T* CreateNode(Args&&... args)
    {
        auto node = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = node.get();
        m_nodes.push_back(std::move(node));
        return raw;
    }

    /**
     * @brief Set the root node of the tree.
     * @param root  A node previously created via CreateNode.
     */
    void SetRoot(BlendNode* root) { m_root = root; }

    /**
     * @brief Evaluate the blend tree at the given playback time.
     *
     * @param time            Playback time in seconds.
     * @param skeleton        The skeleton being animated.
     * @param outLocalTransforms  Output local transforms (one per joint).
     * @return false if no root node has been set.
     */
    bool Evaluate(float                   time,
                  const Skeleton&         skeleton,
                  std::vector<math::Mat4>& outLocalTransforms) const;

    BlendNode* Root() const { return m_root; }

private:
    std::vector<std::unique_ptr<BlendNode>> m_nodes;
    BlendNode*                              m_root = nullptr;
};

} // namespace animation
} // namespace engine
