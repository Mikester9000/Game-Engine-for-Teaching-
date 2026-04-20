/**
 * @file camera_system.cpp
 * @brief Third-person follow camera implementation — M8.3 Camera System.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2025
 */

#include "engine/rendering/camera_system.hpp"
#include "engine/core/Logger.hpp"

#include <algorithm> // std::max, std::min
#include <cmath>     // std::sin, std::cos, std::tan, std::sqrt, std::abs
#include <array>

namespace engine {
namespace rendering {

using math::Vec3;
using math::Mat4;

// ===========================================================================
// Helper: vector operations not in math_types.hpp
// ===========================================================================
namespace {

/// Compute the length of a Vec3.
float Length(const Vec3& v) {
    return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}

/// Return the normalized (unit-length) version of v.
/// Returns v unchanged if length is near zero (degenerate vector).
Vec3 Normalize(const Vec3& v) {
    const float len = Length(v);
    if (len < 1e-6f) return v;
    return { v.x / len, v.y / len, v.z / len };
}

/// Cross product of a and b: a × b.
Vec3 Cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

/// Dot product of a and b.
float Dot(const Vec3& a, const Vec3& b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

/// Convert degrees to radians.
constexpr float DegToRad(float deg) {
    return deg * (3.14159265358979323846f / 180.0f);
}

} // namespace anonymous

// ===========================================================================
// CameraSystem::Update
// ===========================================================================
void CameraSystem::Update(World& world,
                           uint32_t backBufferW, uint32_t backBufferH)
{
    // TEACHING NOTE — Finding the active camera via World::View()
    // ──────────────────────────────────────────────────────────────
    // World::View<CameraComponent>() iterates ALL entities that have a
    // CameraComponent.  We take the first one with isActive=true.  In a
    // production engine you might maintain a "primary camera" pointer, but
    // iterating a small set of cameras is cheap and keeps this system
    // self-contained.

    const float aspect = (backBufferH > 0)
        ? static_cast<float>(backBufferW) / static_cast<float>(backBufferH)
        : 16.0f / 9.0f;

    world.View<CameraComponent>(
        [&](EntityID /*camEntity*/, CameraComponent& cam)
        {
            if (!cam.isActive) return;

            // Update the stored aspect ratio.
            cam.aspectRatio = aspect;

            // ----------------------------------------------------------------
            // Compute the camera's world position from the orbit parameters.
            // ----------------------------------------------------------------
            Vec3 targetPos { 0.0f, 0.0f, 0.0f };

            if (cam.targetEntityID != NULL_ENTITY &&
                world.IsAlive(cam.targetEntityID) &&
                world.HasComponent<TransformComponent>(cam.targetEntityID))
            {
                targetPos = world.GetComponent<TransformComponent>(
                    cam.targetEntityID).position;
            }

            // TEACHING NOTE — Orbit Camera Math
            // ──────────────────────────────────
            // We parameterise the camera's position in *spherical coordinates*
            // around the target:
            //
            //   yaw   = horizontal rotation (left/right in the XZ plane)
            //   pitch = vertical tilt        (up/down angle from horizontal)
            //
            // The camera "arm" vector (in world space) is:
            //
            //   armX = |offset.z| * sin(yaw)
            //   armY = |offset.z| * sin(pitch) + offset.y   ← fixed height
            //   armZ = |offset.z| * cos(yaw) * cos(pitch)
            //
            // We clamp pitch to [-π/3, π/3] to prevent the camera flipping.

            constexpr float kMaxPitch =  1.047f;  //  ~60 degrees
            constexpr float kMinPitch = -0.524f;  // ~-30 degrees

            cam.pitchRadians = std::max(kMinPitch,
                               std::min(kMaxPitch, cam.pitchRadians));

            const float armLen = std::abs(cam.offset.z);
            const float sinYaw = std::sin(cam.yawRadians);
            const float cosYaw = std::cos(cam.yawRadians);
            const float sinPitch = std::sin(cam.pitchRadians);
            const float cosPitch = std::cos(cam.pitchRadians);

            const Vec3 arm {
                 armLen * sinYaw  * cosPitch,
                 armLen * sinPitch + cam.offset.y,
                -armLen * cosYaw  * cosPitch
            };

            cam.worldPosition = targetPos - arm;  // camera behind target

            // ----------------------------------------------------------------
            // Build the view matrix.
            // ----------------------------------------------------------------
            cam.viewMatrix = BuildLookAt(cam.worldPosition, targetPos,
                                         { 0.0f, 1.0f, 0.0f });

            // ----------------------------------------------------------------
            // Build the projection matrix.
            // ----------------------------------------------------------------
            const float fovRad = DegToRad(cam.fovDegrees);
            cam.projMatrix = BuildPerspective(fovRad, cam.aspectRatio,
                                              cam.nearPlane, cam.farPlane);
        });
}

// ===========================================================================
// CameraSystem::ApplyOrbitDelta
// ===========================================================================
void CameraSystem::ApplyOrbitDelta(World& world,
                                    float deltaYaw, float deltaPitch)
{
    world.View<CameraComponent>(
        [&](EntityID, CameraComponent& cam)
        {
            if (!cam.isActive) return;
            cam.yawRadians   += deltaYaw;
            cam.pitchRadians += deltaPitch;
        });
}

// ===========================================================================
// CameraSystem::BuildLookAt  (row-major, D3D11 convention)
// ===========================================================================
Mat4 CameraSystem::BuildLookAt(const Vec3& eye, const Vec3& target, const Vec3& up)
{
    // TEACHING NOTE — Row-Major View Matrix: basis vectors in COLUMNS
    // ──────────────────────────────────────────────────────────────────
    // In D3D11 we multiply: transformedPos = mul(float4(pos,1), viewMatrix)
    // (row-vector × matrix convention).  For this multiplication form,
    // "camera-space X of a world position" requires:
    //
    //   clip.x = pos.x * m[0][0] + pos.y * m[1][0] + pos.z * m[2][0] + m[3][0]
    //          = dot(right, pos) - dot(right, eye)   = dot(right, pos - eye) ✓
    //
    // This means right.xyz must appear in the FIRST COLUMN (m[*][0]), not the
    // first row.  The same logic applies to up (column 1) and forward (column 2).
    //
    // Contrast with model/world matrices: there the convention is ROWS = axes
    // (see math_types.hpp Mat4 note).  The view matrix is the inverse of the
    // camera's world matrix; for an orthonormal matrix the inverse equals the
    // transpose, so world-matrix rows become view-matrix columns.
    //
    // Steps:
    //   1. forward = normalize(target - eye)
    //   2. right   = normalize(cross(forward, up))
    //   3. newUp   = cross(right, forward)     ← orthogonalise up
    //   4. Translation = -dot(right/newUp/forward, eye)

    const Vec3 forward = Normalize(target - eye);
    const Vec3 right   = Normalize(Cross(forward, up));
    const Vec3 newUp   = Cross(right, forward);

    // All 16 elements are explicitly assigned below; no need to start from
    // Identity() — zero-initialize instead to make the assignment self-evident.
    Mat4 m{};

    // Row 0 — right axis
    m.m[0][0] =  right.x;
    m.m[0][1] =  newUp.x;
    m.m[0][2] =  forward.x;
    m.m[0][3] =  0.0f;

    // Row 1 — up axis
    m.m[1][0] =  right.y;
    m.m[1][1] =  newUp.y;
    m.m[1][2] =  forward.y;
    m.m[1][3] =  0.0f;

    // Row 2 — forward axis
    m.m[2][0] =  right.z;
    m.m[2][1] =  newUp.z;
    m.m[2][2] =  forward.z;
    m.m[2][3] =  0.0f;

    // Row 3 — translation
    m.m[3][0] = -Dot(right,   eye);
    m.m[3][1] = -Dot(newUp,   eye);
    m.m[3][2] = -Dot(forward, eye);
    m.m[3][3] =  1.0f;

    return m;
}

// ===========================================================================
// CameraSystem::BuildPerspective  (row-major, D3D11 NDC depth [0, 1])
// ===========================================================================
Mat4 CameraSystem::BuildPerspective(float fovY, float aspect,
                                     float nearZ, float farZ)
{
    // TEACHING NOTE — D3D11 Perspective Projection (Row-Major)
    // ─────────────────────────────────────────────────────────
    // D3D11's clip-space depth range is [0, 1] (not OpenGL's [-1, 1]).
    // The perspective division maps:
    //   near  →  0
    //   far   →  1
    //
    // The matrix (column vector form, but stored row-major here):
    //
    //   [ f/a   0    0              0   ]
    //   [  0    f    0              0   ]
    //   [  0    0  far/(far-near)   1   ]   ← row 2, col 3 = 1 (not -1)
    //   [  0    0  -near*far/(far-near) 0 ]
    //
    // where  f = 1/tan(fovY/2),  a = aspectRatio.

    const float f    = 1.0f / std::tan(fovY * 0.5f);
    const float dInv = 1.0f / (farZ - nearZ);

    Mat4 m;  // zero-initialised by default constructor

    m.m[0][0] = f / aspect;
    m.m[1][1] = f;
    m.m[2][2] = farZ * dInv;
    m.m[2][3] = 1.0f;
    m.m[3][2] = -nearZ * farZ * dInv;

    return m;
}

} // namespace rendering
} // namespace engine
