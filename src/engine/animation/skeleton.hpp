/**
 * @file skeleton.hpp
 * @brief Runtime skeleton — joint hierarchy + bind pose + world-transform computation.
 *
 * ============================================================================
 * TEACHING NOTE — What is a Skeleton?
 * ============================================================================
 * A skeleton is a hierarchy of *joints* (also called bones).  Each joint has:
 *   1. A parent (except the root joint).
 *   2. A *bind-pose* transform: the joint's rest position relative to its parent.
 *   3. A *local* transform: the animated pose relative to its parent.
 *   4. A *world* transform: the final position / orientation in world space,
 *      computed by walking up the hierarchy:
 *        world[i] = local[i] × world[parent[i]]
 *
 * Skinned meshes use the world transform of each joint to deform the mesh:
 *   vertex_world = Σ (weight[i] × (worldTransform[i] × bindPoseInverse[i]) × vertex_local)
 *
 * The term "bind pose inverse" (also called the "inverse bind matrix") cancels
 * out the bind pose so that when the skeleton is in the rest position the mesh
 * vertices do not move.
 *
 * This file matches the shared schema:  shared/schemas/skeleton.schema.json
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
#include <cstdint>

namespace engine {
namespace animation {

// ===========================================================================
// Constants
// ===========================================================================

static constexpr int kMaxJoints = 64;  ///< Maximum joints per skeleton (fits in one CB)

// ===========================================================================
// Joint
// ===========================================================================

/**
 * @struct Joint
 * @brief One joint (bone) in the skeleton hierarchy.
 *
 * TEACHING NOTE — Local vs World Space
 * Joints are authored (and stored) in *local space* — relative to their parent.
 * At runtime we compute *world-space* transforms by multiplying child × parent
 * matrices up the hierarchy (see Skeleton::ComputeWorldTransforms).
 *
 * bindPose*        — rest-pose local transform (from the authoring tool).
 * bindPoseInverse  — the inverse of the bind-pose world transform, pre-computed
 *                    once and used every frame to "un-bind" before applying the
 *                    current animated pose.  This is the "inverse bind matrix"
 *                    used in skinning.
 */
struct Joint
{
    int         index       = -1;     ///< Index into Skeleton::joints (must equal position)
    std::string name;                 ///< Human-readable name (e.g. "spine_01")
    int         parentIndex = -1;     ///< -1 for the root joint

    // Bind-pose (rest-pose) local TRS components — stored separately for
    // blend-weight interpolation before building the matrix.
    math::Vec3  bindTranslation { 0.0f, 0.0f, 0.0f };
    math::Quat  bindRotation    { 0.0f, 0.0f, 0.0f, 1.0f };
    math::Vec3  bindScale       { 1.0f, 1.0f, 1.0f };

    // Pre-computed bind-pose world-space matrix (filled by Skeleton::Build).
    math::Mat4  bindWorldMatrix   = math::Mat4::Identity();

    // Pre-computed inverse of bindWorldMatrix — used in the skinning formula.
    // TEACHING NOTE — Inverse Bind Matrix
    // skinVertex = Σ weight[i] * (jointWorldMatrix[i] * invBindMatrix[i]) * localVertex
    // The invBindMatrix[i] transforms a vertex from "bind-pose model space"
    // into "joint local space".  Multiplying by jointWorldMatrix[i] then
    // brings it into "current world space".
    math::Mat4  invBindMatrix     = math::Mat4::Identity();
};

// ===========================================================================
// Skeleton
// ===========================================================================

/**
 * @class Skeleton
 * @brief Hierarchical collection of joints with bind-pose data.
 *
 * TEACHING NOTE — Topology Invariant
 * Joints are stored in a flat vector sorted such that a parent's index is
 * always less than its child's index.  This means we can compute world
 * transforms in a single forward pass (no recursion needed):
 *
 *   for i in 0..N:
 *     worldTransform[i] = localTransform[i] * worldTransform[parent[i]]
 *
 * This "topological sort" of the joint hierarchy is a common optimisation in
 * production skeletons.
 */
class Skeleton
{
public:
    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------

    Skeleton() = default;

    /**
     * @brief Add a joint and return its index.
     * @param name        Human-readable joint name.
     * @param parentIndex Parent joint index (-1 for root).
     * @param translation Bind-pose local translation.
     * @param rotation    Bind-pose local rotation.
     * @param scale       Bind-pose local scale.
     * @return Index of the new joint (== joints().size() - 1).
     */
    int AddJoint(const std::string& name,
                 int                parentIndex,
                 const math::Vec3&  translation = math::Vec3::Zero(),
                 const math::Quat&  rotation    = math::Quat::Identity(),
                 const math::Vec3&  scale       = math::Vec3::One());

    /**
     * @brief Pre-compute bind-pose world matrices and inverse bind matrices.
     *
     * Call this once after all joints have been added (or after loading from
     * JSON).  After Build(), invBindMatrix is valid on all joints.
     *
     * TEACHING NOTE — Why Pre-compute the Inverse?
     * The inverse bind matrix only changes if the skeleton's rest pose
     * changes (i.e. if you re-export from the DCC tool).  At runtime it is a
     * constant that can be baked into the skeleton asset, avoiding a matrix
     * inversion per joint per frame.
     */
    void Build();

    // -----------------------------------------------------------------------
    // World-transform computation (called every frame by AnimationSystem)
    // -----------------------------------------------------------------------

    /**
     * @brief Compute world-space transforms from an array of local TRS matrices.
     *
     * @param localTransforms  Array of [jointCount] local-space Mat4 (one per joint).
     * @param worldTransforms  Output array of [jointCount] world-space Mat4.
     *
     * TEACHING NOTE — Forward Pass
     * We walk the joints in index order (which is guaranteed topological).
     * For the root (parentIndex == -1) the world transform equals the local
     * transform.  For every other joint:
     *   world[i] = local[i] * world[parent[i]]
     *
     * Multiplying local × parent (not parent × local) is correct in D3D11
     * row-major convention where the child transform is applied FIRST (it is
     * in local space) and the parent is applied AFTER (it brings local into
     * world space).
     */
    void ComputeWorldTransforms(const std::vector<math::Mat4>& localTransforms,
                                std::vector<math::Mat4>&       worldTransforms) const;

    /**
     * @brief Build skin matrices: skinMatrix[i] = worldTransform[i] × invBindMatrix[i].
     *
     * These are the matrices uploaded to the D3D11 constant buffer for GPU skinning.
     *
     * TEACHING NOTE — Skin Matrix
     * The skin matrix combines two operations:
     *   invBindMatrix[i]  — moves the vertex from bind-pose model space into
     *                       joint-i's local space.
     *   worldTransform[i] — moves from joint-i's local space into current
     *                       world space.
     *
     * Together they implement:
     *   vertexWorld = Σ weight[i] * skinMatrix[i] * vertexBindPose
     */
    void ComputeSkinMatrices(const std::vector<math::Mat4>& worldTransforms,
                             std::vector<math::Mat4>&       skinMatrices) const;

    /**
     * @brief Return the local-space bind-pose matrices (one per joint).
     *
     * These serve as the reference pose when no animation is playing.
     */
    std::vector<math::Mat4> GetBindPoseLocalMatrices() const;

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------

    const std::vector<Joint>& Joints()     const { return m_joints; }
    int                       JointCount() const { return static_cast<int>(m_joints.size()); }

    /** @brief Find a joint by name; returns nullptr if not found. */
    const Joint* FindJoint(const std::string& name) const;

    const std::string& Name() const { return m_name; }
    const std::string& Id()   const { return m_id; }

    void SetName(const std::string& n) { m_name = n; }
    void SetId(const std::string& id)  { m_id   = id; }

private:
    std::string         m_id;
    std::string         m_name;
    std::vector<Joint>  m_joints;  ///< Topologically sorted: parent index < child index
};

} // namespace animation
} // namespace engine
