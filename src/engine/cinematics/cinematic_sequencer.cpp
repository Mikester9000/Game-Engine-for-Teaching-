/**
 * @file cinematic_sequencer.cpp
 * @brief CinematicSequencer implementation — shot advancement + camera write.
 *
 * ============================================================================
 * TEACHING NOTE — Implementation Overview
 * ============================================================================
 *
 * The sequencer has three responsibilities:
 *
 *   1. SHOT MANAGEMENT — AddShot() stores ordered ShotEntries.  Play() resets
 *      state.  SkipToShot() allows scrubbing for editor use.
 *
 *   2. TIME ADVANCEMENT — Tick(dt) increments m_shotTime.  When m_shotTime
 *      exceeds the shot's duration, the sequencer calls AdvanceShot() which
 *      increments m_currentShot, fires OnShotChanged, and checks for sequence
 *      completion (fires OnComplete).
 *
 *   3. ECS CAMERA WRITE — ApplyToCamera() evaluates the CameraRig at the
 *      current m_shotTime (remapped to rig-local time), writes the result to
 *      the active CameraComponent, and sets cinematicOverride=true.  When
 *      IsComplete(), it clears cinematicOverride so the follow camera resumes.
 *
 * ─── Time remapping ─────────────────────────────────────────────────────────
 *
 * A shot has a user-specified duration (shotDuration) and its rig has an
 * internal duration (rig.Duration()).  We remap shot time to rig time:
 *
 *   rigTime = (m_shotTime / shotDuration) * rig.Duration()
 *
 * This means:
 *   • A 3 s shot playing a 6 s rig → plays the rig at 0.5× speed.
 *   • A 6 s shot playing a 3 s rig → plays the rig at 2× speed.
 *   • A 3 s shot playing a 0-duration rig → always returns the rig's
 *     single static keyframe (static hold shot).
 *
 * TEACHING NOTE — Time stretch / compress
 * This remapping pattern is called "time stretch" in audio / video editing.
 * It lets the artist set the pacing (how long each shot takes) separately
 * from the motion design (how the camera moves along the rig).  In practice,
 * you author the rig at 1:1 speed then adjust the shot duration in the
 * sequence editor without re-authoring the rig.
 *
 * ─── Carry-over time ────────────────────────────────────────────────────────
 *
 * TEACHING NOTE — Leftover dt after a shot ends
 * When a frame's dt causes the shot to finish, there is usually leftover time
 * (m_shotTime > shotDuration).  We carry this over into the next shot:
 *
 *   remainder = m_shotTime - shotDuration
 *   m_shotTime = 0.0f
 *   advance shot
 *   m_shotTime += remainder
 *
 * Without carry-over, the beginning of each shot would be slightly short
 * (truncated by the frame boundary).  This is especially noticeable at high
 * frame rates where a single dt can be a significant fraction of a short shot.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2025
 * C++ Standard: C++17
 * Platform: Windows / Linux (no platform dependencies)
 */

#include "engine/cinematics/cinematic_sequencer.hpp"
#include "engine/core/Logger.hpp"

#include <cmath>    // std::abs
#include <algorithm> // std::max

namespace engine {
namespace cinematics {

// ===========================================================================
// Authoring
// ===========================================================================

void CinematicSequencer::AddShot(CameraRig rig, float duration, std::string label)
{
    // TEACHING NOTE — Clamp duration to a small positive minimum.
    // A zero-duration shot would cause Tick() to spin-advance through all
    // remaining shots in a single frame (infinite loop risk).  We clamp to
    // 1 ms minimum and log a warning so the content author is alerted.
    if (duration <= 0.0f)
    {
        LOG_WARN("CinematicSequencer::AddShot — shot duration <= 0; clamped to 0.001 s.");
        duration = 0.001f;
    }
    m_shots.emplace_back(std::move(rig), duration, std::move(label));
}

void CinematicSequencer::AddAudioEvent(float t, std::string clipID)
{
    // TEACHING NOTE — Authoring guard
    // AddAudioEvent() must be called after AddShot() because events are stored
    // per-shot.  Calling it with no shots is a logic error in the authoring
    // code (not a runtime user error), so we log an error and return.
    if (m_shots.empty())
    {
        LOG_ERROR("CinematicSequencer::AddAudioEvent — no shots added; call AddShot() first.");
        return;
    }

    ShotEntry& shot = m_shots.back();

    // TEACHING NOTE — Clamping event time to shot duration
    // If an author accidentally sets an event time past the shot duration we
    // clamp it to the last valid frame rather than silently discarding it.
    // The warning tells them something is off without crashing.
    const float clampedT = std::max(0.0f, std::min(t, shot.duration));
    if (std::abs(clampedT - t) > 1e-6f)
    {
        LOG_WARN("CinematicSequencer::AddAudioEvent — event time clamped to shot duration.");
    }

    // TEACHING NOTE — Maintaining the "sorted by time" invariant on insert
    // Tick() can make multiple audio events eligible within a single update
    // (e.g. when dt is large or a shot is very short).  Iterating a time-sorted
    // list guarantees deterministic dispatch order regardless of the authoring
    // sequence.  std::lower_bound gives O(log N) search; since a shot typically
    // has only a handful of events the constant factor is negligible.
    //
    // Because eventFired is a parallel array we must insert its flag at the
    // exact same index so the correspondence between audioEvents[i] and
    // eventFired[i] is always preserved.
    const auto insertIt = std::lower_bound(
        shot.audioEvents.begin(),
        shot.audioEvents.end(),
        clampedT,
        [](const auto& ev, float time) { return ev.time < time; });

    const auto insertIndex = static_cast<std::ptrdiff_t>(
        insertIt - shot.audioEvents.begin());

    shot.audioEvents.insert(insertIt, {clampedT, std::move(clipID)});
    shot.eventFired.insert(shot.eventFired.begin() + insertIndex, 0u);
}

// ===========================================================================
// Callbacks
// ===========================================================================

void CinematicSequencer::SetOnShotChanged(std::function<void(int)> cb)
{
    m_onShotChanged = std::move(cb);
}

void CinematicSequencer::SetOnComplete(std::function<void()> cb)
{
    m_onComplete = std::move(cb);
}

void CinematicSequencer::SetOnAudioEvent(std::function<void(const std::string&)> cb)
{
    m_onAudioEvent = std::move(cb);
}

// ===========================================================================
// Playback control
// ===========================================================================

void CinematicSequencer::Play()
{
    if (m_shots.empty())
    {
        LOG_WARN("CinematicSequencer::Play — no shots added; nothing to play.");
        m_complete = true;
        return;
    }

    // TEACHING NOTE — Resetting audio event fired flags on Play()
    // Each call to Play() starts the sequence from scratch, so all audio
    // events must be eligible to fire again.  We reset the fired flags for
    // every shot so events reliably trigger even if the sequence is replayed.
    for (ShotEntry& shot : m_shots)
    {
        for (uint8_t& fired : shot.eventFired)
            fired = false;
    }

    m_currentShot = 0;
    m_shotTime    = 0.0f;
    m_playing     = true;
    m_complete    = false;

    // Fire the initial shot-changed callback (shot 0 starts).
    if (m_onShotChanged)
        m_onShotChanged(m_currentShot);
}

void CinematicSequencer::Stop()
{
    m_playing = false;
    // NOTE: we do NOT set m_complete here — Stop() is an interrupt, not
    // a natural completion.  The caller can test IsPlaying() == false.
}

void CinematicSequencer::SkipToShot(int shotIndex)
{
    // TEACHING NOTE — Bounds clamping for editor scrubbing
    // Editor timeline scrubbers often call SkipToShot() with an arbitrary
    // index.  Clamping here prevents out-of-bounds access and makes the
    // API safe to call even before AddShot() has been called.
    if (m_shots.empty()) return;

    const int clamped = std::max(0, std::min(shotIndex,
                                             static_cast<int>(m_shots.size()) - 1));
    m_currentShot = clamped;
    m_shotTime    = 0.0f;
    m_playing     = true;
    m_complete    = false;

    // Reset fired flags for the target shot so audio events can re-fire
    // when the shot is replayed from the beginning.
    ShotEntry& shot = m_shots[static_cast<size_t>(m_currentShot)];
    for (uint8_t& fired : shot.eventFired)
        fired = false;

    if (m_onShotChanged)
        m_onShotChanged(m_currentShot);
}

// ===========================================================================
// Per-frame update
// ===========================================================================

void CinematicSequencer::Tick(float dt)
{
    if (!m_playing || m_complete)
        return;

    if (m_currentShot < 0 || m_currentShot >= static_cast<int>(m_shots.size()))
    {
        // Should not happen after Play() but guard defensively.
        m_complete = true;
        m_playing  = false;
        return;
    }

    m_shotTime += dt;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Timed audio event dispatch
    // -----------------------------------------------------------------------
    // After advancing m_shotTime we check each unfired audio event in the
    // current shot.  If m_shotTime has reached or passed the event's time,
    // we mark it fired and invoke m_onAudioEvent.
    //
    // We check BEFORE the carry-over loop so that events fire at the
    // correct shot-local time, not the carry-over time of the next shot.
    // Events that were declared at exactly t=shotDuration will fire here
    // (m_shotTime >= event.time) before the carry-over advances the shot.
    // -----------------------------------------------------------------------
    if (m_currentShot >= 0 && m_currentShot < static_cast<int>(m_shots.size()))
    {
        ShotEntry& curShot = m_shots[static_cast<size_t>(m_currentShot)];
        for (size_t i = 0; i < curShot.audioEvents.size(); ++i)
        {
            if (!curShot.eventFired[i] &&
                m_shotTime >= curShot.audioEvents[i].time)
            {
                curShot.eventFired[i] = true;
                if (m_onAudioEvent)
                    m_onAudioEvent(curShot.audioEvents[i].clipID);
            }
        }
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Carry-over loop
    // -----------------------------------------------------------------------
    // We use a loop rather than a single if-check because in theory a very
    // large dt (e.g. first frame after a debugger pause) could span multiple
    // shots.  The loop ensures every intermediate OnShotChanged fires and the
    // sequencer lands in the correct final shot.
    // -----------------------------------------------------------------------
    while (m_currentShot < static_cast<int>(m_shots.size()))
    {
        const float shotDur = m_shots[static_cast<size_t>(m_currentShot)].duration;

        if (m_shotTime < shotDur)
            break;  // Still within this shot.

        // Compute carry-over before advancing.
        const float remainder = m_shotTime - shotDur;

        ++m_currentShot;
        m_shotTime = remainder;  // carry leftover into next shot

        if (m_currentShot >= static_cast<int>(m_shots.size()))
        {
            // All shots finished.
            m_playing  = false;
            m_complete = true;
            m_shotTime = 0.0f;

            if (m_onComplete)
                m_onComplete();

            return;
        }

        // Reset audio event fired flags for the new shot so events
        // that fall within the carry-over time range can still fire.
        if (m_currentShot < static_cast<int>(m_shots.size()))
        {
            ShotEntry& nextShot = m_shots[static_cast<size_t>(m_currentShot)];
            for (uint8_t& fired : nextShot.eventFired)
                fired = false;

            // Fire any audio events in the new shot that fall within
            // the carry-over time (m_shotTime already advanced by remainder).
            for (size_t i = 0; i < nextShot.audioEvents.size(); ++i)
            {
                if (!nextShot.eventFired[i] &&
                    m_shotTime >= nextShot.audioEvents[i].time)
                {
                    nextShot.eventFired[i] = true;
                    if (m_onAudioEvent)
                        m_onAudioEvent(nextShot.audioEvents[i].clipID);
                }
            }
        }

        // Fire shot-change event.
        if (m_onShotChanged)
            m_onShotChanged(m_currentShot);
    }
}

// ===========================================================================
// ECS camera write
// ===========================================================================

void CinematicSequencer::ApplyToCamera(World& world) const
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Why iterate rather than cache the camera entity ID?
    // -----------------------------------------------------------------------
    // We call world.View<CameraComponent>() each frame instead of caching
    // the entity ID at Play() time.  Caching would be slightly faster, but
    // the camera entity can be destroyed and re-created during scene load.
    // Iterating each frame is safer and the overhead is negligible (there
    // is usually exactly one camera entity).
    // -----------------------------------------------------------------------

    const CameraRigSample sample = CurrentSample();

    world.View<CameraComponent>(
        [&](EntityID /*entity*/, CameraComponent& cam)
        {
            if (!cam.isActive) return;

            if (m_complete)
            {
                // TEACHING NOTE — Releasing cinematic control
                // When the sequence is done we clear cinematicOverride so that
                // CameraSystem reverts to follow-camera mode.  This gives the
                // player control back automatically without any extra game code.
                cam.cinematicOverride = false;
                return;
            }

            // Write cinematic state into the component.
            cam.cinematicOverride = true;
            cam.cinematicEyePos  = sample.position;
            cam.cinematicLookAt  = sample.lookAt;
            cam.fovDegrees       = sample.fovDeg;
        });
}

// ===========================================================================
// Query
// ===========================================================================

bool CinematicSequencer::IsPlaying()  const { return m_playing;  }
bool CinematicSequencer::IsComplete() const { return m_complete; }

int CinematicSequencer::CurrentShotIndex() const
{
    return m_currentShot;
}

float CinematicSequencer::ShotLocalTime() const
{
    return m_shotTime;
}

int CinematicSequencer::ShotCount() const
{
    return static_cast<int>(m_shots.size());
}

// ===========================================================================
// CurrentSample
// ===========================================================================

CameraRigSample CinematicSequencer::CurrentSample() const
{
    if (m_currentShot < 0 || m_currentShot >= static_cast<int>(m_shots.size()))
        return {};  // default sample

    const ShotEntry& shot = m_shots[static_cast<size_t>(m_currentShot)];

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Time remapping (shot time → rig time)
    // -----------------------------------------------------------------------
    // shot.duration  is the playback duration set by the sequence author.
    // rig.Duration() is the time span spanned by the rig's keyframes.
    // We remap [0, shot.duration] → [0, rig.Duration()].
    //
    // If rig.Duration() is 0 (static rig / single keyframe), rigTime = 0
    // so Evaluate(0) returns the one keyframe consistently.
    // -----------------------------------------------------------------------
    const float rigDur = shot.rig.Duration();
    float rigTime = 0.0f;

    if (rigDur > 1e-7f && shot.duration > 1e-7f)
    {
        rigTime = (m_shotTime / shot.duration) * rigDur;
    }

    return shot.rig.Evaluate(rigTime);
}

} // namespace cinematics
} // namespace engine
