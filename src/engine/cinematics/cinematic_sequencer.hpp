/**
 * @file cinematic_sequencer.hpp
 * @brief CinematicSequencer — timeline-based cut-scene orchestrator.
 *
 * ============================================================================
 * TEACHING NOTE — What is a Cinematic Sequencer?
 * ============================================================================
 *
 * A cut-scene in a game is a sequence of "shots" — each shot places the
 * camera at a specific position and animates it along a CameraRig path for a
 * fixed duration.  When one shot ends the sequencer "cuts" (or smoothly
 * transitions) to the next shot.
 *
 * Final Fantasy XV uses a similar system for "Royal Arms acquisition" and
 * "Chapter boss reveal" cinematics: the camera glides through a series of
 * authored positions before returning control to the player.
 *
 * This CinematicSequencer implements the following features:
 *
 *   • Shot list  — an ordered list of (CameraRig, duration) pairs.
 *   • Playback   — Tick(dt) advances the current shot; auto-advances.
 *   • Callbacks  — optional std::function<void()> for:
 *                    OnShotChanged(int newIndex) — called on every cut.
 *                    OnComplete()               — called when all shots finish.
 *   • ECS output — ApplyToCamera(World&) writes the interpolated camera
 *                  state into the active CameraComponent each frame.
 *
 * ─── Architecture ───────────────────────────────────────────────────────────
 *
 * TEACHING NOTE — Push vs Pull camera control
 *
 * The sequencer PUSHES camera data into the ECS CameraComponent.  This is a
 * deliberate design choice:
 *
 *   Push (this design): Sequencer writes viewPos/lookAt directly to CameraComponent.
 *   Pull: Renderer reads from Sequencer directly.
 *
 * Push is preferred because:
 *   1. The renderer already reads from CameraComponent — no renderer change needed.
 *   2. Gameplay code can blend the sequencer output with player input by
 *      reading and then overriding CameraComponent fields.
 *   3. The sequencer has zero coupling to the renderer; it is pure gameplay.
 *
 * ─── Shot and transition model ───────────────────────────────────────────────
 *
 *   Timeline:
 *     |--- shot 0 (dur=3s) ---|--- shot 1 (dur=2s) ---|--- shot 2 (dur=4s) ---|
 *     0                       3                       5                       9
 *
 *   m_shotTime counts from 0 within the current shot.  When m_shotTime
 *   reaches the shot's CameraRig duration (or explicit shot duration),
 *   the sequencer advances: resets m_shotTime = 0, increments m_currentShot.
 *
 * TEACHING NOTE — Shot duration vs rig duration
 * A CameraRig has its own duration (last keyframe time - first keyframe time).
 * The sequencer stores that duration explicitly in ShotEntry so that:
 *   1. You can play a rig faster or slower (stretch/compress time).
 *   2. A shot with a single static keyframe still has a meaningful duration.
 *
 * ─── Usage example ───────────────────────────────────────────────────────────
 *
 * @code
 *   CinematicSequencer seq;
 *
 *   // Build two shots.
 *   CameraRig intro;
 *   intro.AddKeyframe(0.0f, {0,5,-10}, {0,0,0}, 60.0f);
 *   intro.AddKeyframe(3.0f, {5,3,-8},  {0,1,0}, 55.0f);
 *   seq.AddShot(intro, 3.0f);
 *
 *   CameraRig reveal;
 *   reveal.AddKeyframe(0.0f, {5,3,-8},  {0,1,0}, 55.0f);
 *   reveal.AddKeyframe(2.0f, {10,2,-5}, {0,2,0}, 45.0f);
 *   seq.AddShot(reveal, 2.0f);
 *
 *   seq.SetOnComplete([](){ LOG_INFO("Cinematic done!"); });
 *
 *   seq.Play();
 *
 *   // Per-frame update (in GameRuntime):
 *   seq.Tick(dt);
 *   seq.ApplyToCamera(world);
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

#include "engine/cinematics/camera_rig.hpp"
#include "engine/ecs/ECS.hpp"              // World, CameraComponent

#include <vector>
#include <functional>  // std::function
#include <string>

namespace engine {
namespace cinematics {

// ===========================================================================
// ShotEntry
// ===========================================================================

/**
 * @struct ShotEntry
 * @brief One element in the sequencer's shot list.
 *
 * TEACHING NOTE — Separating rig from duration
 * Storing the duration alongside the rig (rather than reading rig.Duration())
 * lets the sequence author stretch or compress any shot in time without
 * modifying the rig's keyframes.  The local time fed to rig.Evaluate() is
 * remapped by (shotTime / shotDuration) * rig.Duration().
 */
struct ShotEntry
{
    CameraRig   rig;           ///< Camera path for this shot.
    float       duration;      ///< How long (in seconds) to play this shot.
    std::string label;         ///< Optional human-readable name (for debugging).

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Per-shot audio events
    // -----------------------------------------------------------------------
    // A cinematic shot can trigger audio clips at specific times (e.g. a
    // footstep sound at t=0.3 s, a sword clash at t=0.7 s).  Storing events
    // per-shot keeps them relative to the shot's local time, which makes
    // authoring intuitive: you think in shot-local seconds, not absolute
    // timeline seconds.  The sequencer fires each event once when
    // m_shotTime reaches event.time and resets the fired flags when the
    // shot restarts (e.g. SkipToShot / Play).
    // -----------------------------------------------------------------------

    /**
     * @struct AudioEventEntry
     * @brief A timed audio trigger within a cinematic shot.
     */
    struct AudioEventEntry
    {
        float       time;    ///< Shot-local trigger time (seconds, ≥ 0).
        std::string clipID;  ///< Asset ID of the audio clip to play.
    };

    std::vector<AudioEventEntry> audioEvents; ///< Audio events kept sorted by time (ascending); insertion maintains invariant.
    // TEACHING NOTE — Why uint8_t instead of bool?
    // std::vector<bool> is a specialization that packs bits into words.
    // Accessing an element returns a proxy object, not a real bool reference.
    // MSVC enforces this strictly: "for (bool& f : vec<bool>)" is a C2440
    // compile error.  Using uint8_t (0 = not fired, 1 = fired) avoids the
    // specialization and allows range-for with a real reference as intended.
    std::vector<uint8_t>         eventFired;  ///< Parallel fired-flag array (0=pending, 1=fired).

    ShotEntry() = default;
    ShotEntry(CameraRig r, float dur, std::string lbl = "")
        : rig(std::move(r)), duration(dur), label(std::move(lbl))
    {}
};

// ===========================================================================
// CinematicSequencer
// ===========================================================================

/**
 * @class CinematicSequencer
 * @brief Plays an ordered list of CameraRig shots, advancing on every cut.
 *
 * TEACHING NOTE — Event-driven design with std::function callbacks
 * Rather than polling IsComplete() every frame, callers can register
 * callbacks for events of interest.  std::function<void()> is the idiomatic
 * C++11 way to store a callable without exposing the implementation type.
 * Callbacks are optional (left unset = no-op).
 */
class CinematicSequencer
{
public:
    CinematicSequencer()  = default;
    ~CinematicSequencer() = default;

    // Non-copyable (std::function members are copyable but the semantics of
    // "two sequencers sharing callbacks" are confusing, so we delete copy).
    CinematicSequencer(const CinematicSequencer&)            = delete;
    CinematicSequencer& operator=(const CinematicSequencer&) = delete;

    // Movable — allows storing in containers.
    CinematicSequencer(CinematicSequencer&&)            = default;
    CinematicSequencer& operator=(CinematicSequencer&&) = default;

    // =========================================================================
    // Authoring API
    // =========================================================================

    /**
     * @brief Append a shot to the end of the sequence.
     *
     * @param rig       Camera path for this shot.
     * @param duration  How many seconds this shot plays before cutting to the next.
     * @param label     Optional debug label (e.g. "intro_pan", "reveal_boss").
     */
    void AddShot(CameraRig rig, float duration, std::string label = "");

    /**
     * @brief Add a timed audio event to the most recently added shot.
     *
     * When Tick() advances past @p t (shot-local seconds), the callback
     * registered via SetOnAudioEvent() is invoked exactly once with @p clipID.
     * The event fires at most once per playthrough of the shot; calling Play()
     * or SkipToShot() resets all fired flags.
     *
     * TEACHING NOTE — Why shot-local time?
     * Shot-local time makes authoring intuitive: "the sword clash sound fires
     * 0.3 seconds into this shot" regardless of where that shot falls on the
     * global timeline.  Absolute timestamps would require recalculating event
     * times every time the shot order changes in the editor.
     *
     * TEACHING NOTE — Invalid authoring input is logged and ignored
     * This sequencer API keeps authoring helpers lightweight by treating
     * "no current shot exists yet" as a recoverable misuse: the implementation
     * logs the problem and ignores the request instead of throwing.  The
     * declaration documents that behaviour so callers do not rely on exception
     * handling for normal editor/runtime validation.
     *
     * @param t       Shot-local time in seconds (clamped to [0, shot duration]).
     * @param clipID  Asset ID of the audio clip to trigger.
     * @note  If no shots have been added yet, the request is logged and ignored.
     */
    void AddAudioEvent(float t, std::string clipID);

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Register a callback invoked whenever the sequencer cuts to a new shot.
     *
     * TEACHING NOTE — Callback signature (int newShotIndex)
     * Passing the new shot index lets the game trigger dialogue or gameplay
     * events at specific shots without tightly coupling the sequencer to game
     * code.  The sequencer knows nothing about dialogue; it just fires an int.
     *
     * @param cb  Called with the index of the incoming shot.
     */
    void SetOnShotChanged(std::function<void(int)> cb);

    /**
     * @brief Register a callback invoked when all shots have finished playing.
     *
     * Typical use: re-enable player input, fade in from black, etc.
     *
     * @param cb  Called with no arguments when the last shot completes.
     */
    void SetOnComplete(std::function<void()> cb);

    /**
     * @brief Register a callback invoked each time a timed audio event fires.
     *
     * The callback receives the clip ID string provided to AddAudioEvent().
     * This decouples the sequencer from the audio system: the game layer
     * registers this callback and calls AudioSystem::Play(clipID) in response.
     *
     * TEACHING NOTE — Decoupled audio triggering
     * The sequencer does not call AudioSystem directly because:
     *   1. The sequencer lives in engine/cinematics/ with no dependency on audio.
     *   2. The same callback mechanism works in headless tests (no XAudio2).
     *   3. The game layer can remap clip IDs (e.g. localisation, difficulty).
     *
     * @param cb  Called with the clipID of the triggered audio event.
     */
    void SetOnAudioEvent(std::function<void(const std::string&)> cb);

    // =========================================================================
    // Playback control
    // =========================================================================

    /**
     * @brief Start (or restart) playback from the first shot.
     */
    void Play();

    /**
     * @brief Immediately stop playback (does NOT fire OnComplete).
     */
    void Stop();

    /**
     * @brief Skip to a specific shot index.
     *
     * Useful for editor scrubbing.
     *
     * @param shotIndex  Index of the shot to jump to (0-based).
     */
    void SkipToShot(int shotIndex);

    // =========================================================================
    // Per-frame update
    // =========================================================================

    /**
     * @brief Advance the sequencer by dt seconds.
     *
     * Advances m_shotTime within the current shot.  When m_shotTime reaches
     * the shot's duration, cuts to the next shot (fires OnShotChanged) and
     * checks for sequence completion (fires OnComplete).
     *
     * TEACHING NOTE — Accumulated dt vs absolute time
     * We accumulate delta-time rather than storing an absolute timestamp so
     * that the sequencer works correctly even if the application is paused,
     * restarted at an offset, or if time is scaled.  Absolute time would
     * require the caller to manage a base timestamp.
     *
     * @param dt  Elapsed time in seconds since the last Tick().
     */
    void Tick(float dt);

    /**
     * @brief Write the current shot's interpolated camera state into the ECS.
     *
     * Finds the first entity with an active CameraComponent in `world` and
     * writes:
     *   • m_viewPos    (camera position in world space)
     *   • m_lookAt     (look-at target in world space)
     *   • fovDegrees   (vertical field-of-view)
     *
     * TEACHING NOTE — CameraComponent fields written by the sequencer
     * The CameraSystem reads cameraComp.viewPos and cameraComp.lookAt to
     * build the view matrix each frame, so the sequencer just writes those
     * two fields.  If no CameraComponent is found, this is a no-op (safe to
     * call even before the camera entity is spawned).
     *
     * @param world  ECS World containing at least one CameraComponent.
     */
    void ApplyToCamera(World& world) const;

    // =========================================================================
    // Query
    // =========================================================================

    /**
     * @brief Whether Tick() / Play() have been called and playback is active.
     */
    [[nodiscard]] bool IsPlaying()   const;

    /**
     * @brief Whether all shots have finished (or the shot list is empty).
     */
    [[nodiscard]] bool IsComplete()  const;

    /**
     * @brief Index of the shot currently playing (0-based).
     * @return -1 if no shots have been added or sequence has not started.
     */
    [[nodiscard]] int  CurrentShotIndex() const;

    /**
     * @brief Shot-local time elapsed within the current shot.
     */
    [[nodiscard]] float ShotLocalTime() const;

    /**
     * @brief Total number of shots in the sequence.
     */
    [[nodiscard]] int  ShotCount() const;

    /**
     * @brief Evaluate the current camera state (without writing to ECS).
     *
     * Returns a CameraRigSample at the current shot's local time.  Useful
     * for previewing in the editor without requiring an ECS World.
     */
    [[nodiscard]] CameraRigSample CurrentSample() const;

private:
    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    std::vector<ShotEntry> m_shots;        ///< Ordered list of shots.
    int                    m_currentShot = -1;   ///< Index into m_shots; -1 = not started.
    float                  m_shotTime    = 0.0f; ///< Time elapsed in the current shot.
    bool                   m_playing     = false;
    bool                   m_complete    = false;

    // -----------------------------------------------------------------------
    // Callbacks (optional — left as nullptr = no-op)
    // -----------------------------------------------------------------------

    std::function<void(int)> m_onShotChanged;  ///< Fired on every cut.
    std::function<void()>    m_onComplete;     ///< Fired when all shots finish.
    std::function<void(const std::string&)> m_onAudioEvent; ///< Fired on timed audio events.
};

} // namespace cinematics
} // namespace engine
