/**
 * @file xaudio2_backend.hpp
 * @brief XAudio2 audio backend — Windows low-level audio driver.
 *
 * ============================================================================
 * TEACHING NOTE — Why XAudio2?
 * ============================================================================
 * XAudio2 is Microsoft's low-level audio API, included in every Windows
 * installation since Windows 8 (and available as a redistributable for Win 7).
 * It is the audio backbone of every modern Microsoft title including:
 *   - Final Fantasy XV (SQUARE ENIX used WASAPI / XAudio2 on PC)
 *   - Xbox first-party games
 *   - Most Unity / Unreal games on Windows
 *
 * XAudio2 sits just above the hardware:
 *
 *   App → XAudio2 Source Voices → Submix Voices → Mastering Voice → Speakers
 *
 * Advantages over legacy DirectSound / FMOD (for a teaching engine):
 *   • Ships with the Windows SDK — zero extra dependency.
 *   • Runs on GT610-era hardware (XAudio2 is CPU-only; GPU not involved).
 *   • Supports 3D positional audio via X3DAudio.
 *   • Minimal overhead: source voices submit raw PCM buffers directly.
 *
 * ============================================================================
 * TEACHING NOTE — Source Voice Pool
 * ============================================================================
 * Creating and destroying source voices is expensive (kernel transition).
 * Real engines pre-allocate a fixed pool of voices at init time and re-use
 * them.  Each slot tracks which clipID is playing so we can stop by ID.
 *
 * Pool size of 16 handles:
 *   • Up to 4 party-member SFX channels
 *   • Up to 6 enemy SFX channels
 *   • 2 ambient layers (wind, environment)
 *   • 2 music crossfade slots
 *   • 2 UI feedback slots
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Target: Windows (MSVC) — XAudio2 is Windows-only.
 * Requires: xaudio2.lib (Windows SDK — always present)
 */

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <windows.h>
#include <xaudio2.h>

#include <cstdint>
#include <string>
#include <vector>
#include <array>

namespace engine {
namespace assets { class AssetDB; }  // forward declaration
namespace audio  {

// ===========================================================================
// Constants
// ===========================================================================

/// TEACHING NOTE — Voice Pool Size
/// 16 simultaneous sounds covers most JRPG scenarios.  FF15 runs hundreds of
/// voices on high-end hardware; we limit to 16 for GT610-class compatibility.
static constexpr uint32_t XAUDIO2_VOICE_POOL_SIZE = 16;

// ===========================================================================
// WavData — parsed .wav file content
// ===========================================================================

/**
 * @struct WavData
 * @brief Parsed contents of a RIFF/WAVE file.
 *
 * TEACHING NOTE — RIFF/WAVE Format
 * ────────────────────────────────
 * A .wav file is a RIFF container with a "WAVE" form type.  The two
 * mandatory chunks are:
 *
 *   "fmt " — WAVEFORMATEX: sample rate, channels, bit depth, etc.
 *   "data" — Raw PCM samples.
 *
 * XAudio2 source voices are created with the fmt header and fed the data
 * chunk as an XAUDIO2_BUFFER.
 */
struct WavData {
    WAVEFORMATEX           fmt{};     ///< Audio format descriptor.
    std::vector<uint8_t>   pcm;       ///< Raw PCM sample bytes.
    bool                   valid = false; ///< True if parsing succeeded.
};

// ===========================================================================
// SourceVoiceSlot — one entry in the voice pool
// ===========================================================================

/**
 * @struct SourceVoiceSlot
 * @brief One pre-allocated source voice plus playback metadata.
 */
struct SourceVoiceSlot {
    IXAudio2SourceVoice* voice    = nullptr;
    std::string          clipID;            ///< Active clip GUID (empty = free).
    bool                 inUse   = false;   ///< True while sound is playing.
    bool                 looping = false;   ///< True if the voice loops.
};

// ===========================================================================
// XAudio2Backend
// ===========================================================================

/**
 * @class XAudio2Backend
 * @brief Manages the XAudio2 engine, mastering voice, and source voice pool.
 *
 * TEACHING NOTE — Backend vs System
 * ──────────────────────────────────
 * The *backend* owns the low-level XAudio2 objects and knows nothing about
 * the ECS or game state.  It exposes simple Play/Stop primitives.
 *
 * The *AudioSystem* (audio_system.hpp) is the ECS-aware layer that reads
 * AudioSourceComponent data and forwards commands to this backend.
 *
 * This separation means we could swap XAudio2 for FMOD/SDL_mixer without
 * changing any gameplay code — only the backend changes.
 */
class XAudio2Backend
{
public:
    XAudio2Backend();
    ~XAudio2Backend();

    // Non-copyable — owns COM objects.
    XAudio2Backend(const XAudio2Backend&)            = delete;
    XAudio2Backend& operator=(const XAudio2Backend&) = delete;

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    /**
     * @brief Initialise XAudio2, create the mastering voice and voice pool.
     *
     * @param assetDB  Pointer to the loaded AssetDB.  Used to resolve clip
     *                 GUIDs to cooked .wav file paths.  Must outlive backend.
     * @return true on success.
     */
    bool Init(engine::assets::AssetDB* assetDB);

    /**
     * @brief Stop all voices and release all XAudio2 objects.
     *        Safe to call even if Init() was never called.
     */
    void Shutdown();

    // -----------------------------------------------------------------------
    // Playback
    // -----------------------------------------------------------------------

    /**
     * @brief Start playing a sound.
     *
     * @param clipID   AssetDB GUID of the cooked .wav file.
     * @param volume   Volume scalar [0.0–1.0].
     * @param looping  If true, the voice loops until Stop() is called.
     * @return         Index of the allocated voice slot, or -1 on failure.
     */
    int  Play(const std::string& clipID, float volume = 1.0f, bool looping = false);

    /**
     * @brief Stop a specific voice slot (obtained from Play()).
     *
     * @param slotIndex  Value returned by Play().
     */
    void Stop(int slotIndex);

    /**
     * @brief Stop all source voices that are playing the given clip GUID.
     */
    void StopByClipID(const std::string& clipID);

    /**
     * @brief Stop every active source voice immediately.
     */
    void StopAll();

    /**
     * @brief Set volume on the mastering voice (master mix).
     *
     * @param volume  [0.0 = silent, 1.0 = unity gain].
     */
    void SetMasterVolume(float volume);

    // -----------------------------------------------------------------------
    // Query
    // -----------------------------------------------------------------------

    /** @return True if Init() succeeded and the backend is ready. */
    bool IsInitialised() const { return m_initialised; }

    /** @return True if the given slot index is currently playing. */
    bool IsPlaying(int slotIndex) const;

private:
    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Parse a RIFF/WAVE file from raw bytes into WavData.
     *
     * Used to decode cooked .wav assets loaded by AssetLoader.
     */
    static WavData ParseWav(const std::vector<uint8_t>& bytes);

    /**
     * @brief Find a free slot in the voice pool.
     * @return Index of free slot, or -1 if pool is full.
     */
    int FindFreeSlot() const;

    /**
     * @brief Destroy and recreate a source voice for a new audio format.
     *
     * TEACHING NOTE — Voice Reuse
     * ─────────────────────────────
     * A source voice is format-bound at creation time.  When a new clip has
     * a different format (e.g. different sample rate) we must destroy and
     * recreate the voice.  For clips with matching formats we simply reuse
     * the existing voice, which is cheaper.
     */
    bool ReconfigureVoice(int slotIndex, const WAVEFORMATEX& fmt);

    // -----------------------------------------------------------------------
    // XAudio2 objects
    // -----------------------------------------------------------------------

    IXAudio2*               m_xaudio2       = nullptr;
    IXAudio2MasteringVoice* m_masterVoice   = nullptr;

    // -----------------------------------------------------------------------
    // Voice pool
    // -----------------------------------------------------------------------

    std::array<SourceVoiceSlot, XAUDIO2_VOICE_POOL_SIZE> m_pool{};

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    engine::assets::AssetDB* m_assetDB      = nullptr;
    bool                     m_initialised  = false;
};

} // namespace audio
} // namespace engine
