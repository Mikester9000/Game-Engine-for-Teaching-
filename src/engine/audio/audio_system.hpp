/**
 * @file audio_system.hpp
 * @brief ECS AudioSystem — drives XAudio2Backend from AudioSourceComponent data.
 *
 * ============================================================================
 * TEACHING NOTE — Layered Music FSM
 * ============================================================================
 * FF15 uses a layered music system.  The field music is one stem; entering
 * combat crossfades to the battle stem; winning crossfades to a victory sting
 * and back to the field stem.  We model this with a simple FSM:
 *
 *   EXPLORATION ──(enemy spotted)──► BATTLE
 *   BATTLE      ──(all enemies dead)──► VICTORY
 *   VICTORY     ──(sting ends)──► EXPLORATION
 *   ANY         ──(menu opened)──► MENU
 *   MENU        ──(menu closed)──► previous state
 *
 * Each state has one music clip GUID.  On transition we:
 *   1. Fade out the old voice over CROSSFADE_SECONDS.
 *   2. Fade in the new voice.
 *
 * ============================================================================
 * TEACHING NOTE — AudioSystem as ECS System
 * ============================================================================
 * AudioSystem is NOT a standard SystemBase (which updates all matching
 * entities) — it has two responsibilities:
 *
 *   a) ECS layer: iterate AudioSourceComponent-bearing entities and
 *      start/stop source voices in XAudio2Backend.
 *
 *   b) Music layer: manage the music FSM independently of entities.
 *
 * We inherit from SystemBase for consistency with the rest of the ECS but
 * also expose SetMusicState() for direct game-loop use.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Target: Windows (MSVC)
 */

#pragma once

#include "engine/audio/xaudio2_backend.hpp"
#include "engine/ecs/ECS.hpp"

#include <string>

namespace engine {
namespace assets { class AssetDB; }
namespace audio  {

// ===========================================================================
// MusicState — layered music FSM states
// ===========================================================================

/**
 * @enum MusicState
 * @brief Music layer FSM states matching FF15's dynamic music system.
 *
 * TEACHING NOTE — Music State Machine
 * ──────────────────────────────────────
 * The music state drives which audio stem is playing.  Transitions trigger
 * a crossfade rather than an abrupt cut to maintain musical continuity.
 *
 * NONE means no music is configured for the current zone (silence).
 */
enum class MusicState : uint8_t {
    NONE,          ///< No music — silence.
    EXPLORATION,   ///< Open-world traversal / town music.
    BATTLE,        ///< Active combat theme.
    VICTORY,       ///< Post-battle victory sting.
    MENU           ///< Menu / pause overlay music.
};

// ===========================================================================
// MusicTrack — associates a MusicState with an asset GUID
// ===========================================================================

/**
 * @struct MusicTrack
 * @brief Binds a MusicState to the cooked audio asset GUID for that state.
 */
struct MusicTrack {
    MusicState  state  = MusicState::NONE;
    std::string clipID;        ///< AssetDB GUID for the .wav music stem.
    float       volume = 0.8f; ///< Default volume for this stem.
};

// ===========================================================================
// AudioSystem
// ===========================================================================

/**
 * @class AudioSystem
 * @brief Drives XAudio2Backend from ECS components and manages music FSM.
 *
 * TEACHING NOTE — Dual Responsibility (by design)
 * ──────────────────────────────────────────────────
 * Real game engines often split entity SFX and music management into separate
 * systems.  Here we combine them to keep the audio subsystem contained in
 * two files.  When the engine grows, refactor into:
 *   - SFXSystem    — entity-level sounds only
 *   - MusicSystem  — music FSM only
 */
class AudioSystem : public SystemBase
{
public:
    // -----------------------------------------------------------------------
    // Lifecycle (SystemBase overrides)
    // -----------------------------------------------------------------------

    void Init()   override {}

    /**
     * @brief Update — process entity SFX + tick the music crossfade.
     *
     * @param world      The ECS world to iterate.
     * @param deltaTime  Seconds since last frame.
     *
     * TEACHING NOTE — Why pass World explicitly?
     * ─────────────────────────────────────────────
     * SystemBase::Update(float dt) does not give us the World pointer.
     * We override the extended version AudioSystem::Update(World&, float) and
     * leave the base version as a no-op to satisfy the interface.
     */
    void Update(float /*deltaTime*/) override {}

    /**
     * @brief Extended update called by the game loop with a World reference.
     *
     * @param world      ECS world for iterating AudioSourceComponent entities.
     * @param deltaTime  Seconds since last frame.
     */
    void Update(World& world, float deltaTime);

    // -----------------------------------------------------------------------
    // Initialisation
    // -----------------------------------------------------------------------

    /**
     * @brief Initialise the audio system and its XAudio2 backend.
     *
     * @param assetDB  Pointer to the loaded AssetDB.  Must outlive AudioSystem.
     * @return true on success.
     */
    bool InitAudio(engine::assets::AssetDB* assetDB);

    /**
     * @brief Shutdown all audio and release backend.
     */
    void ShutdownAudio();

    // -----------------------------------------------------------------------
    // Music FSM
    // -----------------------------------------------------------------------

    /**
     * @brief Register a music stem for a specific state.
     *
     * Call this once per state at scene load time to configure which clip
     * plays in each music state.
     *
     * @param track  MusicTrack binding a state to an asset GUID.
     */
    void RegisterMusicTrack(const MusicTrack& track);

    /**
     * @brief Transition the music FSM to a new state.
     *
     * If the new state differs from the current one, the old stem fades out
     * and the new stem fades in over CROSSFADE_SECONDS.
     *
     * @param newState  Target music state.
     */
    void SetMusicState(MusicState newState);

    /** @return The current music FSM state. */
    MusicState GetMusicState() const { return m_currentMusicState; }

    // -----------------------------------------------------------------------
    // Volume control
    // -----------------------------------------------------------------------

    /**
     * @brief Set the global SFX volume (affects all source voices except music).
     * @param volume  [0.0 = silent, 1.0 = unity gain].
     */
    void SetSFXVolume(float volume);

    /**
     * @brief Set the global music volume.
     * @param volume  [0.0 = silent, 1.0 = unity gain].
     */
    void SetMusicVolume(float volume);

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------

    /** @return Pointer to the underlying XAudio2 backend (for advanced use). */
    XAudio2Backend* GetBackend() { return &m_backend; }

private:
    // -----------------------------------------------------------------------
    // Music FSM helpers
    // -----------------------------------------------------------------------

    /// TEACHING NOTE — Crossfade duration in seconds.
    /// 1.5 s matches the feel of FF15's music transitions.
    static constexpr float CROSSFADE_SECONDS = 1.5f;

    /**
     * @brief Tick the volume crossfade.
     *
     * @param deltaTime  Seconds since last frame.
     */
    void TickCrossfade(float deltaTime);

    /**
     * @brief Find the MusicTrack registered for a given state.
     * @return Pointer to the track, or nullptr if not registered.
     */
    const MusicTrack* FindTrack(MusicState state) const;

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    XAudio2Backend            m_backend;

    // Music FSM
    MusicState                m_currentMusicState  = MusicState::NONE;
    MusicState                m_previousMusicState = MusicState::NONE;
    int                       m_musicSlot          = -1;  ///< Voice pool slot of current stem.
    int                       m_oldMusicSlot       = -1;  ///< Slot fading out.
    float                     m_crossfadeTimer     = 0.0f;
    bool                      m_crossfading        = false;
    float                     m_musicVolume        = 0.8f;
    float                     m_sfxVolume          = 1.0f;

    // Registered music tracks (one per MusicState).
    std::array<MusicTrack, 5> m_tracks{};  // indexed 0..4 matching MusicState enum
    bool                      m_tracksSet[5] = {};
};

} // namespace audio
} // namespace engine
