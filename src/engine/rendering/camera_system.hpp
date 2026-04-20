/**
 * @file camera_system.hpp
 * @brief Third-person follow camera — M8.3 Camera System.
 *
 * ============================================================================
 * TEACHING NOTE — Camera System Architecture
 * ============================================================================
 * The CameraSystem is the bridge between gameplay (entity positions) and
 * rendering (view/projection matrices uploaded to the GPU).
 *
 * Design responsibilities:
 *   1. Find the active CameraComponent in the ECS World each frame.
 *   2. Read the target entity's TransformComponent for the "look-at" point.
 *   3. Compute the camera's world position from target + orbit offset.
 *   4. Build the view matrix: lookAt(cameraPos, targetPos, worldUp).
 *   5. Build the projection matrix: perspective(fov, aspect, near, far).
 *   6. Write both matrices back into CameraComponent for the renderer.
 *
 * ─── Third-Person Follow Camera ─────────────────────────────────────────────
 * A third-person camera sits behind and above the player.  In Final Fantasy XV
 * the camera trails Noctis as he runs, orbits when the player rotates the
 * stick, and auto-centers after a few seconds of inactivity.  Here we
 * implement the simplest version: a fixed-offset orbiting follow camera with
 * yaw (horizontal) and pitch (vertical) control.
 *
 * Camera world position:
 *   right   = { cos(yaw),  0, -sin(yaw) }   (horizontal orbit axis)
 *   up      = { 0, 1, 0 }                   (world up)
 *   back    = { sin(yaw), -sin(pitch), cos(yaw)*cos(pitch) } (behind-and-up)
 *   camPos  = targetPos + right*offset.x + worldUp*offset.y + back*|offset.z|
 *
 * View matrix = lookAt(camPos, targetPos, worldUp).
 *
 * ─── LookAt Matrix (Row-Major, D3D11 convention) ────────────────────────────
 * The view matrix transforms world coordinates into camera space.  In D3D11
 * (row-major, left-multiply) it is built from three basis vectors:
 *
 *   forward = normalize(target - eye)
 *   right   = normalize(cross(worldUp, forward))
 *   up      = cross(forward, right)
 *
 *   V = | right.x   right.y   right.z  -dot(right,  eye) |
 *       | up.x      up.y      up.z     -dot(up,     eye) |
 *       | forward.x forward.y forward.z -dot(forward,eye) |
 *       | 0         0         0          1                |
 *
 * ─── Perspective Projection ─────────────────────────────────────────────────
 * Maps the view frustum to the canonical clip cube [-1,1]×[-1,1]×[0,1].
 * (D3D11 uses a 0-to-1 depth range, unlike OpenGL's -1-to-1.)
 *
 *   f  = 1 / tan(fov/2)           (focal length)
 *   P[0][0] = f / aspect
 *   P[1][1] = f
 *   P[2][2] = far / (far - near)
 *   P[2][3] = 1
 *   P[3][2] = -near*far / (far - near)
 *   (all other entries = 0)
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2025
 * C++ Standard: C++17
 * Platform: Windows (engine_sandbox)
 */

#pragma once

#include "engine/ecs/ECS.hpp"  // World, CameraComponent, TransformComponent

namespace engine {
namespace rendering {

// ===========================================================================
// CameraSystem
// ===========================================================================

/**
 * @class CameraSystem
 * @brief Computes view and projection matrices from the active CameraComponent.
 *
 * Usage (from GameRuntime):
 * @code
 *   engine::rendering::CameraSystem cam;
 *
 *   // --- Init ---
 *   EntityID cameraEntity = world.CreateEntity();
 *   auto& cc = world.AddComponent<CameraComponent>(cameraEntity);
 *   cc.targetEntityID = playerID;
 *   cc.fovDegrees     = 60.0f;
 *
 *   // --- Per frame ---
 *   cam.Update(world, backBufferWidth, backBufferHeight);
 *   // CameraComponent::viewMatrix / projMatrix are now ready for the renderer.
 * @endcode
 */
class CameraSystem
{
public:
    CameraSystem()  = default;
    ~CameraSystem() = default;

    // Non-copyable (stateless but keeps the pattern consistent).
    CameraSystem(const CameraSystem&)            = delete;
    CameraSystem& operator=(const CameraSystem&) = delete;

    // =========================================================================
    // Per-frame update
    // =========================================================================

    /**
     * @brief Update the active camera's view/projection matrices.
     *
     * Finds the first entity with an active CameraComponent, reads the
     * target entity's TransformComponent, and writes updated view and
     * projection matrices back into the CameraComponent.
     *
     * @param world         ECS World containing camera + target entities.
     * @param backBufferW   Current back-buffer width in pixels  (for aspect).
     * @param backBufferH   Current back-buffer height in pixels (for aspect).
     */
    void Update(World& world, uint32_t backBufferW, uint32_t backBufferH);

    // =========================================================================
    // Mouse orbit (interactive windowed mode)
    // =========================================================================

    /**
     * @brief Apply a yaw/pitch delta from mouse movement.
     *
     * Called by the input mapper when the user drags with the right mouse
     * button.  The delta is accumulated and applied during the next Update().
     *
     * @param world      ECS World (to find active camera).
     * @param deltaYaw   Horizontal orbit delta in radians.
     * @param deltaPitch Vertical orbit delta in radians.
     */
    void ApplyOrbitDelta(World& world, float deltaYaw, float deltaPitch);

private:
    // -----------------------------------------------------------------------
    // Static matrix builders
    // -----------------------------------------------------------------------

    /**
     * @brief Build a row-major lookAt view matrix (D3D11 convention).
     *
     * @param eye     Camera world position.
     * @param target  Point the camera looks at.
     * @param up      World-space up vector (usually {0,1,0}).
     * @return Row-major 4×4 view matrix.
     */
    static engine::math::Mat4 BuildLookAt(const engine::math::Vec3& eye,
                                          const engine::math::Vec3& target,
                                          const engine::math::Vec3& up);

    /**
     * @brief Build a row-major perspective projection matrix (D3D11 NDC).
     *
     * D3D11 depth range is [0, 1] (not OpenGL's [-1, 1]).
     *
     * @param fovY      Vertical field-of-view in radians.
     * @param aspect    Aspect ratio (width / height).
     * @param nearZ     Near clip plane distance.
     * @param farZ      Far clip plane distance.
     * @return Row-major 4×4 perspective projection matrix.
     */
    static engine::math::Mat4 BuildPerspective(float fovY, float aspect,
                                               float nearZ, float farZ);
};

} // namespace rendering
} // namespace engine
