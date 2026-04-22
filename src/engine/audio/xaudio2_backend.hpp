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
#include <x3daudio.h>

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
 *
 * TEACHING NOTE — PCM Buffer Lifetime
 * ──────────────────────────────────────
 * XAudio2 source voices operate asynchronously on an audio thread.  When
 * you call SubmitSourceBuffer, XAudio2 stores a raw pointer (pAudioData)
 * and continues reading from it on the audio thread until the buffer
 * finishes.  The calling code MUST keep the PCM bytes alive for at least
 * as long as the voice is playing.
 *
 * We solve this by storing the decoded PCM data directly in the slot.
 * When Play() allocates a slot, it moves the parsed WavData::pcm vector
 * here.  When Stop() frees the slot, the vector is cleared.
 */
struct SourceVoiceSlot {
    IXAudio2SourceVoice* voice    = nullptr;
    std::string          clipID;            ///< Active clip GUID (empty = free).
    bool                 inUse   = false;   ///< True while sound is playing.
    bool                 looping = false;   ///< True if the voice loops.

    /// Owns the PCM bytes referenced by the active XAUDIO2_BUFFER.
    /// Must not be cleared while the voice is still consuming the buffer.
    std::vector<uint8_t> pcmCache;
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

    /**
     * @brief Set volume on a specific source voice slot.
     *
     * Used by AudioSystem to implement music crossfading: ramp the incoming
     * stem from 0 → target and the outgoing stem from target → 0 each frame.
     *
     * @param slotIndex  Voice pool index returned by Play().
     * @param volume     Volume scalar [0.0 = silent, 1.0 = unity gain].
     */
    void SetSlotVolume(int slotIndex, float volume);

    /**
     * @brief Set the listener position used for 3D audio attenuation.
     *
     * Call this each frame with the camera (or player ear) world position.
     * All subsequent Apply3DAttenuation() calls will compute distance
     * relative to this position.
     *
     * @param x  World-space X coordinate of the listener.
     * @param y  World-space Y coordinate of the listener.
     * @param z  World-space Z coordinate of the listener.
     */
    void SetListenerPosition(float x, float y, float z);

    /**
     * @brief Compute the volume attenuation for a 3D emitter at a given position.
     *
     * Uses X3DAudio distance rolloff when X3DAudio was successfully initialised,
     * otherwise falls back to a linear inverse-distance formula.
     *
     * TEACHING NOTE — Distance Rolloff Curve
     * ────────────────────────────────────────
     * X3DAudio supports several built-in distance curves (linear, inverse,
     * inverse-square, etc.).  We use the default linear curve for clarity:
     *
     *   volume = clamp(1.0 - dist / maxDistance, 0.0, 1.0)
     *
     * At distance  = 0          → volume = 1.0 (full)
     * At distance  = maxDist/2  → volume = 0.5 (half)
     * At distance >= maxDist    → volume = 0.0 (inaudible)
     *
     * This satisfies the M18 acceptance criterion:
     *   volume ≤ 0.05 when emitter is placed at maxDistance.
     *
     * @param emitX     World-space X of the emitter.
     * @param emitY     World-space Y of the emitter.
     * @param emitZ     World-space Z of the emitter.
     * @param maxDist   Distance at which volume reaches 0 (world units).
     * @return          Volume scalar in [0.0, 1.0].
     */
    float Compute3DVolume(float emitX, float emitY, float emitZ, float maxDist) const;

    /**
     * @brief Apply 3D distance attenuation to an active source voice.
     *
     * Computes the volume for the given emitter position and sets it on the
     * source voice via SetSlotVolume.  Call once per frame for every 3D source
     * that is currently playing.
     *
     * @param slotIndex   Voice pool index returned by Play().
     * @param emitX       World-space X of the emitter entity.
     * @param emitY       World-space Y of the emitter entity.
     * @param emitZ       World-space Z of the emitter entity.
     * @param maxDist     Distance at which the voice becomes inaudible.
     * @param baseVolume  Base volume scalar (from AudioSourceComponent::volume).
     */
    void Apply3DAttenuation(int slotIndex,
                            float emitX, float emitY, float emitZ,
                            float maxDist, float baseVolume);

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
    // X3DAudio — 3D positional audio (M18)
    // -----------------------------------------------------------------------

    /// TEACHING NOTE — X3DAUDIO_HANDLE
    /// X3DAudio is a header-only math library that computes DSP parameters
    /// (volume, panning, Doppler) for 3D positioned audio.  The handle is
    /// initialised once with the speaker channel mask and speed of sound.
    /// It contains no COM objects; it is a plain POD struct (array of bytes).
    X3DAUDIO_HANDLE m_x3dHandle      = {};

    /// True after X3DAudioInitialize() succeeds.
    bool            m_x3dReady       = false;

    /// Number of output channels on the mastering voice.
    /// Used when building the DSP output matrix in Compute3DVolume().
    /// Stored at Init() time from IXAudio2MasteringVoice::GetChannelMask().
    UINT32          m_dstChannels    = 2;

    /// Cached listener world position (updated by SetListenerPosition).
    X3DAUDIO_VECTOR m_listenerPos    = {0.0f, 0.0f, 0.0f};

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
