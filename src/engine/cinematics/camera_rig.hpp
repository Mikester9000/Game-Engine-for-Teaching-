/**
 * @file camera_rig.hpp
 * @brief CameraRig — keyframed camera path for cinematic cut-scenes.
 *
 * ============================================================================
 * TEACHING NOTE — What is a Camera Rig?
 * ============================================================================
 *
 * In film production a "camera rig" is the physical mount that holds and
 * moves a camera along a predetermined path (dolly, crane, steadicam).  In
 * a game engine a CameraRig is the data structure that encodes that path as
 * a sequence of keyframes.
 *
 * Each keyframe stores:
 *   • position  — where the camera is in world space.
 *   • lookAt    — the world-space point the camera is looking at.
 *   • fovDeg    — the vertical field-of-view in degrees.
 *   • time      — when this keyframe occurs (seconds from shot start).
 *
 * Between keyframes the CameraRig interpolates (see below).  The renderer
 * uses the evaluated position + lookAt to build a view matrix, and fovDeg
 * for the projection matrix.
 *
 * ─── Interpolation ──────────────────────────────────────────────────────────
 *
 * This implementation uses *linear interpolation* (Lerp) for position and
 * look-at, and linear interpolation for FOV.  In a production engine you
 * would replace this with a Catmull-Rom spline or Bezier spline to get
 * smooth "easing" curves, but the linear version is easier to teach and
 * debug.
 *
 * TEACHING NOTE — Lerp vs Spline for Camera Paths
 * Linear interpolation produces straight-line paths with sudden direction
 * changes at each keyframe.  A Catmull-Rom spline passes smoothly through
 * every control point (C1 continuity) — position and velocity are both
 * continuous.  To upgrade: replace Vec3::Lerp with a CatmullRom(p0,p1,p2,p3,t)
 * helper.  The rest of the CameraRig API stays the same.
 *
 * ─── Design Decisions ───────────────────────────────────────────────────────
 *
 * 1. KEYFRAME TIME — keyframes are stored in *shot-local* time (seconds from
 *    the start of this rig).  The CinematicSequencer maps global time to
 *    shot-local time by subtracting the shot start.  This makes it easy to
 *    reorder shots without changing keyframe data.
 *
 * 2. SORTED REQUIREMENT — keyframes MUST be added in ascending time order.
 *    AddKeyframe() asserts this in debug builds to catch authoring mistakes.
 *
 * 3. CLAMP AT ENDS — Evaluate() clamps t to [0, duration]: requesting t < 0
 *    returns the first keyframe; t > duration returns the last keyframe.
 *    This makes sequencer code simpler (no special-casing needed).
 *
 * ─── Usage example ───────────────────────────────────────────────────────────
 *
 * @code
 *   CameraRig rig;
 *   rig.AddKeyframe(0.0f, {0,5,-10}, {0,0,0}, 60.0f);  // opening position
 *   rig.AddKeyframe(2.0f, {5,3,-8},  {0,1,0}, 55.0f);  // pan right
 *   rig.AddKeyframe(4.0f, {10,2,-5}, {0,2,0}, 50.0f);  // push in
 *
 *   float shotTime = 1.5f;  // 1.5 s into the shot
 *   auto sample = rig.Evaluate(shotTime);
 *   // sample.position  is lerped between kf[0] and kf[1]
 *   // sample.lookAt    is lerped between kf[0] and kf[1]
 *   // sample.fovDeg    is lerped between kf[0] and kf[1]
 * @endcode
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2025
 * C++ Standard: C++17
 * Platform: Windows / Linux (no platform dependencies)
 */

#pragma once

#include "engine/math/math_types.hpp"  // Vec3

#include <vector>
#include <cassert>

namespace engine {
namespace cinematics {

// ===========================================================================
// CameraRig::Keyframe
// ===========================================================================

/**
 * @struct CameraKeyframe
 * @brief One point on a camera's animated path.
 *
 * TEACHING NOTE — Why separate position from lookAt?
 * Storing position AND a look-at target (instead of position + direction
 * vector) makes it trivially easy to keep a character "in frame" during a
 * pan: just interpolate the look-at target toward the character while
 * interpolating the camera position along the dolly path.  The view matrix
 * is then  lookAt(position, lookAtTarget, worldUp).
 */
struct CameraKeyframe
{
    float            time    = 0.0f;  ///< Shot-local time (seconds ≥ 0).
    engine::math::Vec3 position;      ///< Camera world position.
    engine::math::Vec3 lookAt;        ///< World-space look-at target.
    float            fovDeg  = 60.0f; ///< Vertical FoV in degrees.

    CameraKeyframe() = default;
    CameraKeyframe(float t,
                   engine::math::Vec3 pos,
                   engine::math::Vec3 target,
                   float fov)
        : time(t), position(pos), lookAt(target), fovDeg(fov)
    {}
};

// ===========================================================================
// CameraRig::Sample
// ===========================================================================

/**
 * @struct CameraRigSample
 * @brief Interpolated camera state returned by CameraRig::Evaluate().
 *
 * TEACHING NOTE — Output-value object pattern
 * Rather than writing results into mutable output parameters (old-style C),
 * we return a small struct.  This enables move semantics and makes call
 * sites self-documenting:
 *
 *   auto s = rig.Evaluate(t);
 *   UploadToRenderer(s.position, s.lookAt, s.fovDeg);
 */
struct CameraRigSample
{
    engine::math::Vec3 position;  ///< Interpolated camera position.
    engine::math::Vec3 lookAt;    ///< Interpolated look-at target.
    float              fovDeg = 60.0f; ///< Interpolated field-of-view (deg).
};

// ===========================================================================
// CameraRig
// ===========================================================================

/**
 * @class CameraRig
 * @brief A keyframed camera path for one cinematic shot.
 *
 * Stores an ordered list of CameraKeyframes and evaluates the camera state
 * at any shot-local time by linearly interpolating between adjacent frames.
 *
 * TEACHING NOTE — Value semantics
 * CameraRig is copyable and movable.  This lets the CinematicSequencer store
 * shots by value in a std::vector, which simplifies ownership and avoids
 * heap fragmentation from individual heap allocations per shot.
 */
class CameraRig
{
public:
    CameraRig()  = default;
    ~CameraRig() = default;

    // Copyable and movable (all members are value types).
    CameraRig(const CameraRig&)            = default;
    CameraRig& operator=(const CameraRig&) = default;
    CameraRig(CameraRig&&)                 = default;
    CameraRig& operator=(CameraRig&&)      = default;

    // =========================================================================
    // Authoring API
    // =========================================================================

    /**
     * @brief Append a keyframe to the end of the camera path.
     *
     * Keyframes MUST be added in strictly ascending time order.
     * Debug builds assert this condition; release builds clamp silently.
     *
     * @param time    Shot-local time in seconds (≥ previous keyframe time).
     * @param pos     Camera world position at this keyframe.
     * @param lookAt  World-space look-at target at this keyframe.
     * @param fovDeg  Vertical field-of-view in degrees at this keyframe.
     */
    void AddKeyframe(float time,
                     engine::math::Vec3 pos,
                     engine::math::Vec3 lookAt,
                     float fovDeg = 60.0f);

    // =========================================================================
    // Query
    // =========================================================================

    /**
     * @brief Evaluate the interpolated camera state at shot-local time t.
     *
     * If no keyframes have been added, returns a default-constructed sample
     * (position = 0, lookAt = 0, fov = 60°).
     *
     * @param t Shot-local time in seconds.  Clamped to [0, Duration()].
     * @return Interpolated CameraRigSample.
     *
     * TEACHING NOTE — Binary search for keyframe bracket
     * Finding the two keyframes that bracket time t is an O(log N) binary
     * search.  For typical shot lengths (5–30 keyframes) a linear scan would
     * be just as fast, but the binary search teaches the standard algorithm
     * and scales to very long procedurally-generated rigs.
     */
    [[nodiscard]] CameraRigSample Evaluate(float t) const;

    /**
     * @brief Total duration of this rig (= last keyframe time - first keyframe time).
     *
     * Returns 0 if fewer than 2 keyframes have been added.
     */
    [[nodiscard]] float Duration() const;

    /**
     * @brief Number of keyframes stored.
     */
    [[nodiscard]] int KeyframeCount() const;

private:
    std::vector<CameraKeyframe> m_keyframes;  ///< Ordered by ascending time.
};

} // namespace cinematics
} // namespace engine
