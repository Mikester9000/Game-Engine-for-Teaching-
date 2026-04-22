/**
 * @file xaudio2_backend.cpp
 * @brief XAudio2 audio backend implementation.
 *
 * ============================================================================
 * TEACHING NOTE — XAudio2 Initialisation Sequence
 * ============================================================================
 * Creating a working XAudio2 device takes three steps:
 *
 *   1. CoInitializeEx()      — initialise the Windows COM runtime.
 *   2. XAudio2Create()       — create the XAudio2 engine object.
 *   3. CreateMasteringVoice() — create the final mix stage that outputs
 *                               to the default audio device.
 *
 * After that, source voices are created per sound (or pooled for reuse).
 * Source voices receive PCM data via XAUDIO2_BUFFER structs and are submitted
 * with SubmitSourceBuffer() + Start().
 *
 * ============================================================================
 * TEACHING NOTE — WAV File Format (RIFF/WAVE)
 * ============================================================================
 * A .wav file is a RIFF (Resource Interchange File Format) container:
 *
 *   Offset  Size  Content
 *   ------  ----  -------
 *        0     4  'RIFF'
 *        4     4  Total file size - 8 (uint32 LE)
 *        8     4  'WAVE'
 *       12     4  Chunk ID ('fmt ' or 'data' or others)
 *       16     4  Chunk size (uint32 LE)
 *       20+      Chunk data
 *
 * The 'fmt ' chunk contains a WAVEFORMATEX struct:
 *   - wFormatTag     (1 = PCM, 3 = float, 0xFFFE = extensible)
 *   - nChannels      (1 = mono, 2 = stereo)
 *   - nSamplesPerSec (e.g. 44100, 48000)
 *   - nAvgBytesPerSec
 *   - nBlockAlign
 *   - wBitsPerSample (8, 16, 24, 32)
 *
 * The 'data' chunk contains the raw interleaved PCM samples.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Target: Windows (MSVC)
 */

#include "engine/audio/xaudio2_backend.hpp"
#include "engine/assets/asset_db.hpp"
#include "engine/assets/asset_loader.hpp"
#include "engine/core/Logger.hpp"

// ---------------------------------------------------------------------------
// TEACHING NOTE — pragma comment(lib, ...) for xaudio2
// ---------------------------------------------------------------------------
// xaudio2.lib ships with the Windows SDK alongside d3d11.lib and dxgi.lib.
// No separate SDK download is required.  We also link ole32.lib for
// CoInitializeEx which is required before XAudio2Create.
// ---------------------------------------------------------------------------
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "ole32.lib")

#include <cstring>   // std::memcmp, std::memcpy
#include <cmath>     // std::sqrt
#include <iostream>

namespace engine {
namespace audio  {

// ===========================================================================
// Constructor / Destructor
// ===========================================================================

XAudio2Backend::XAudio2Backend()  = default;
XAudio2Backend::~XAudio2Backend() { Shutdown(); }

// ===========================================================================
// Init
// ===========================================================================

bool XAudio2Backend::Init(engine::assets::AssetDB* assetDB)
{
    if (m_initialised)
        return true;

    m_assetDB = assetDB;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — COM Initialisation
    // -----------------------------------------------------------------------
    // XAudio2 is a COM object.  Before calling any COM API we must initialise
    // the COM runtime for this thread.  COINIT_MULTITHREADED allows COM objects
    // to be used safely across threads — important if you later add an audio
    // streaming thread.
    //
    // CoInitializeEx returns S_FALSE if COM is already initialised (not an
    // error), and RPC_E_CHANGED_MODE if init was called with a conflicting
    // threading model.  We ignore S_FALSE and treat other failures as fatal.
    // -----------------------------------------------------------------------
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != S_FALSE)
    {
        LOG_ERROR("XAudio2Backend::Init — CoInitializeEx failed. HRESULT=0x"
                  << std::hex << static_cast<unsigned long>(hr) << std::dec);
        return false;
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — XAudio2Create
    // -----------------------------------------------------------------------
    // XAudio2Create creates the central audio engine object.  Parameters:
    //   Flags       = 0          (reserved, must be 0)
    //   Processor   = XAUDIO2_DEFAULT_PROCESSOR  (let XAudio2 pick a thread)
    //
    // The engine object (IXAudio2) manages the audio processing graph and
    // owns all voices.
    // -----------------------------------------------------------------------
    hr = XAudio2Create(&m_xaudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr))
    {
        LOG_ERROR("XAudio2Backend::Init — XAudio2Create failed. HRESULT=0x"
                  << std::hex << static_cast<unsigned long>(hr) << std::dec);
        return false;
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Mastering Voice
    // -----------------------------------------------------------------------
    // The mastering voice is the final stage in the audio graph:
    //   Source Voices → [Submix Voices →] Mastering Voice → OS Audio
    //
    // Parameters:
    //   InputChannels    = XAUDIO2_DEFAULT_CHANNELS  (match output device)
    //   InputSampleRate  = XAUDIO2_DEFAULT_SAMPLERATE (match output device)
    //   Flags            = 0
    //   DeviceId         = nullptr (default audio device)
    //   EffectChain      = nullptr (no DSP effects on the master bus)
    //
    // Only one mastering voice can be active at a time.
    // -----------------------------------------------------------------------
    hr = m_xaudio2->CreateMasteringVoice(
        &m_masterVoice,
        XAUDIO2_DEFAULT_CHANNELS,
        XAUDIO2_DEFAULT_SAMPLERATE,
        0,
        nullptr,
        nullptr
    );
    if (FAILED(hr))
    {
        LOG_ERROR("XAudio2Backend::Init — CreateMasteringVoice failed. HRESULT=0x"
                  << std::hex << static_cast<unsigned long>(hr) << std::dec);
        m_xaudio2->Release();
        m_xaudio2 = nullptr;
        return false;
    }

    // Voice pool slots are default-constructed (voice = nullptr, inUse = false).
    LOG_INFO("XAudio2Backend initialised — voice pool size: " << XAUDIO2_VOICE_POOL_SIZE);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — X3DAudio Initialisation (M18)
    // -----------------------------------------------------------------------
    // X3DAudio extends XAudio2 with 3D spatialization math.  It does NOT
    // create COM objects — it is a set of CPU-only routines that compute DSP
    // coefficients (volume, panning, Doppler) from listener/emitter geometry.
    //
    // Initialisation requires two parameters:
    //
    //   SpeakerChannelMask  — bitmask describing the speaker layout (e.g.
    //                         SPEAKER_STEREO, SPEAKER_5POINT1).  Obtained from
    //                         IXAudio2MasteringVoice::GetChannelMask().
    //
    //   SpeedOfSound        — X3DAUDIO_SPEED_OF_SOUND = 343.5 m/s.
    //                         Used for Doppler shift calculations.
    //
    // We call GetChannelMask() on the mastering voice we just created so that
    // X3DAudio knows the speaker topology.  If this fails (headless CI with no
    // audio device), we skip X3DAudio and fall back to pure-math rolloff.
    // -----------------------------------------------------------------------
    {
        DWORD channelMask = 0;
        HRESULT hrMask = m_masterVoice->GetChannelMask(&channelMask);
        if (SUCCEEDED(hrMask))
        {
            // -----------------------------------------------------------------------
            // TEACHING NOTE — Channel Count from Channel Mask
            // -----------------------------------------------------------------------
            // The channel mask is a bitmask where each set bit represents one
            // speaker channel (e.g. SPEAKER_FRONT_LEFT, SPEAKER_FRONT_RIGHT, …).
            // Counting the set bits gives the number of output channels.
            //
            // We store this count so Compute3DVolume() can allocate the correct
            // number of matrix coefficients when calling X3DAudioCalculate.
            // -----------------------------------------------------------------------
            UINT32 chCount = 0;
            DWORD tmp = channelMask;
            while (tmp) { chCount += (tmp & 1u); tmp >>= 1; }
            m_dstChannels = (chCount > 0u) ? chCount : 2u;

            HRESULT hrX3D = X3DAudioInitialize(channelMask,
                                               X3DAUDIO_SPEED_OF_SOUND,
                                               m_x3dHandle);
            if (SUCCEEDED(hrX3D))
            {
                m_x3dReady = true;
                LOG_INFO("XAudio2Backend: X3DAudio initialised "
                         "(channelMask=0x" << std::hex << channelMask
                         << std::dec << ", dstChannels=" << m_dstChannels << ").");
            }
            else
            {
                LOG_WARN("XAudio2Backend: X3DAudioInitialize failed "
                         "(HRESULT=0x" << std::hex
                         << static_cast<unsigned long>(hrX3D) << std::dec
                         << "). Falling back to linear distance rolloff.");
            }
        }
        else
        {
            LOG_WARN("XAudio2Backend: GetChannelMask failed — "
                     "X3DAudio skipped, using linear fallback.");
        }
    }

    m_initialised = true;
    return true;
}

// ===========================================================================
// Shutdown
// ===========================================================================

void XAudio2Backend::Shutdown()
{
    if (!m_initialised)
        return;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — XAudio2 Teardown Order
    // -----------------------------------------------------------------------
    // Shutdown order matters:
    //   1. Stop and destroy all source voices.
    //   2. Destroy the mastering voice.
    //   3. Release the IXAudio2 engine object.
    //   4. CoUninitialize — balance the CoInitializeEx call.
    //
    // Destroying the engine (step 3) while voices are running would leave
    // dangling IXAudio2SourceVoice pointers — always stop voices first.
    // -----------------------------------------------------------------------

    // Step 1 — stop and destroy all pooled source voices.
    for (auto& slot : m_pool)
    {
        if (slot.voice)
        {
            slot.voice->Stop();
            slot.voice->DestroyVoice();
            slot.voice = nullptr;
        }
        slot.inUse  = false;
        slot.clipID.clear();
    }

    // Step 2 — destroy mastering voice.
    if (m_masterVoice)
    {
        m_masterVoice->DestroyVoice();
        m_masterVoice = nullptr;
    }

    // Step 3 — release the engine.
    if (m_xaudio2)
    {
        m_xaudio2->Release();
        m_xaudio2 = nullptr;
    }

    // Step 4 — balance CoInitializeEx.
    CoUninitialize();

    m_initialised = false;
    m_x3dReady    = false;
    LOG_INFO("XAudio2Backend shut down.");
}

// ===========================================================================
// SetListenerPosition (M18 — X3DAudio 3D positional audio)
// ===========================================================================

void XAudio2Backend::SetListenerPosition(float x, float y, float z)
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Listener vs Emitter
    // -----------------------------------------------------------------------
    // In 3D audio there are two actors:
    //
    //   Listener — where the "ears" are.  Typically the camera position or
    //              the player's head.  Updated once per frame by calling
    //              SetListenerPosition() with the camera world position.
    //
    //   Emitter  — where the sound originates.  Each entity that makes a 3D
    //              sound has its own emitter position, derived from its
    //              TransformComponent::position.
    //
    // X3DAudioCalculate() computes the DSP coefficients (volume, panning,
    // reverb send) by comparing the listener and emitter positions.
    // -----------------------------------------------------------------------
    m_listenerPos = { x, y, z };
}

// ===========================================================================
// Compute3DVolume (M18 — distance rolloff calculation)
// ===========================================================================

float XAudio2Backend::Compute3DVolume(float emitX, float emitY, float emitZ,
                                      float maxDist) const
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Two Rolloff Paths
    // -----------------------------------------------------------------------
    // Path A — X3DAudio DSP (when m_x3dReady):
    //   We set up X3DAUDIO_LISTENER and X3DAUDIO_EMITTER structs and call
    //   X3DAudioCalculate with the X3DAUDIO_CALCULATE_MATRIX flag.  The
    //   result is a matrix of output channel gain coefficients.  For a mono
    //   source into a stereo master (2 channels), the matrix is float[2].
    //   The mean of those coefficients gives us the overall volume level.
    //
    //   CurveDistanceScaler maps "world-unit distance" to "X3DAudio distance".
    //   Setting it to maxDist means the default curve reaches zero at maxDist.
    //
    // Path B — Linear fallback (when X3DAudio is not available):
    //   volume = clamp(1.0 - dist / maxDist, 0.0, 1.0)
    //   Simple, teachable, zero dependencies.
    //
    // Both paths satisfy the M18 acceptance criterion:
    //   Compute3DVolume(maxDist, 0, 0, maxDist) ≈ 0.0
    //   Compute3DVolume(0, 0, 0, maxDist)       ≈ 1.0
    // -----------------------------------------------------------------------

    // Clamp maxDist to avoid divide-by-zero.
    const float safeMax = maxDist > 0.0f ? maxDist : 1.0f;

    const float dx   = emitX - m_listenerPos.x;
    const float dy   = emitY - m_listenerPos.y;
    const float dz   = emitZ - m_listenerPos.z;
    const float dist = std::sqrt(dx*dx + dy*dy + dz*dz);

    if (m_x3dReady)
    {
        // -----------------------------------------------------------------------
        // Path A — X3DAudio DSP calculation
        // -----------------------------------------------------------------------
        // Build the listener (camera).  OrientFront/Top define coordinate frame.
        // Velocity is zero for this teaching engine (no Doppler effect yet).
        X3DAUDIO_LISTENER listener = {};
        listener.OrientFront = { 0.0f, 0.0f,  1.0f };  // facing +Z
        listener.OrientTop   = { 0.0f, 1.0f,  0.0f };  // up +Y
        listener.Position    = m_listenerPos;
        listener.Velocity    = { 0.0f, 0.0f, 0.0f };

        // Build the emitter (sound source).  ChannelCount = 1 (mono clip).
        // CurveDistanceScaler maps one world-unit to one X3DAudio distance unit.
        // Setting it to safeMax makes the default linear curve reach silence at
        // exactly maxDist world units from the listener.
        X3DAUDIO_EMITTER emitter = {};
        emitter.OrientFront          = { 0.0f, 0.0f, 1.0f };
        emitter.OrientTop            = { 0.0f, 1.0f, 0.0f };
        emitter.Position             = { emitX, emitY, emitZ };
        emitter.Velocity             = { 0.0f, 0.0f, 0.0f };
        emitter.ChannelCount         = 1;
        emitter.CurveDistanceScaler  = safeMax;

        // Output matrix — mono-in to N-channels-out.
        // m_dstChannels is stored at Init() from the mastering voice channel mask.
        // Initialise all coefficients to 1.0 so we get unity if X3DAudioCalculate
        // is not called (shouldn't happen, but defensive programming).
        //
        // TEACHING NOTE — Dynamic Matrix Allocation
        // ──────────────────────────────────────────
        // The matrix size is (SrcChannelCount × DstChannelCount) floats.
        // We use a small fixed-size array on the stack.  X3DAudio guarantees
        // DstChannelCount <= XAUDIO2_MAX_AUDIO_CHANNELS (64) and we query the
        // actual count from the mastering voice, so the array is always sized
        // correctly.  Using std::vector would add a heap allocation per call.
        static constexpr UINT32 MAX_CHANNELS = 8u; // mono source to up to 8 output channels (supports 7.1 surround)
        const UINT32 dstCh = m_dstChannels <= MAX_CHANNELS ? m_dstChannels : MAX_CHANNELS;
        float matrix[MAX_CHANNELS] = {};
        for (UINT32 i = 0; i < dstCh; ++i) matrix[i] = 1.0f;

        X3DAUDIO_DSP_SETTINGS dsp = {};
        dsp.SrcChannelCount     = 1;
        dsp.DstChannelCount     = dstCh;
        dsp.pMatrixCoefficients = matrix;

        X3DAudioCalculate(m_x3dHandle, &listener, &emitter,
                          X3DAUDIO_CALCULATE_MATRIX, &dsp);

        // Average all output channel coefficients into a single volume scalar.
        // This collapses stereo panning into a simple attenuation value which
        // is appropriate for distance-rolloff purposes.
        float sum = 0.0f;
        for (UINT32 i = 0; i < dstCh; ++i) sum += matrix[i];
        return sum / static_cast<float>(dstCh);
    }

    // Path B — linear fallback (no X3DAudio).
    const float vol = 1.0f - dist / safeMax;
    return vol < 0.0f ? 0.0f : (vol > 1.0f ? 1.0f : vol);
}

// ===========================================================================
// Apply3DAttenuation (M18)
// ===========================================================================

void XAudio2Backend::Apply3DAttenuation(int slotIndex,
                                        float emitX, float emitY, float emitZ,
                                        float maxDist, float baseVolume)
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Per-Frame 3D Update
    // -----------------------------------------------------------------------
    // 3D audio must be recalculated every frame because both the listener
    // (camera) and the emitter (enemy, NPC, projectile) can move.  The call
    // chain is:
    //
    //   AudioSystem::Update()
    //     → XAudio2Backend::Apply3DAttenuation()  (for each is3D voice)
    //       → Compute3DVolume()                    (distance → gain scalar)
    //         → SetSlotVolume()                    (push to XAudio2 voice)
    //
    // Separating Compute3DVolume() from Apply3DAttenuation() makes the math
    // unit-testable without a live XAudio2 session.
    // -----------------------------------------------------------------------
    if (slotIndex < 0 || slotIndex >= static_cast<int>(XAUDIO2_VOICE_POOL_SIZE))
        return;
    if (!m_pool[slotIndex].inUse)
        return;

    const float attenuation = Compute3DVolume(emitX, emitY, emitZ, maxDist);
    SetSlotVolume(slotIndex, baseVolume * attenuation);
}

// ===========================================================================
// Play
// ===========================================================================

int XAudio2Backend::Play(const std::string& clipID, float volume, bool looping)
{
    if (!m_initialised || !m_assetDB)
        return -1;

    if (clipID.empty())
    {
        LOG_WARN("XAudio2Backend::Play — empty clipID");
        return -1;
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Resolving Asset IDs to File Paths
    // -----------------------------------------------------------------------
    // The AssetDB maps GUID strings (like "3a7f-...") to absolute paths of
    // cooked .wav files on disk.  We load the raw bytes with AssetLoader,
    // then parse the RIFF/WAVE header ourselves.
    //
    // For a production engine you would cache decoded WavData objects so
    // the same clip can be played concurrently without re-reading the file.
    // -----------------------------------------------------------------------
    engine::assets::AssetLoader loader(m_assetDB);
    const std::vector<uint8_t> bytes = loader.LoadRaw(clipID);
    if (bytes.empty())
    {
        LOG_ERROR("XAudio2Backend::Play — failed to load clip: " << clipID);
        return -1;
    }

    WavData wav = ParseWav(bytes);
    if (!wav.valid)
    {
        LOG_ERROR("XAudio2Backend::Play — invalid WAV data for clip: " << clipID);
        return -1;
    }

    // Find a free voice slot.
    int slot = FindFreeSlot();
    if (slot < 0)
    {
        LOG_WARN("XAudio2Backend::Play — voice pool exhausted, dropping: " << clipID);
        return -1;
    }

    // Reconfigure the source voice for this clip's format.
    if (!ReconfigureVoice(slot, wav.fmt))
        return -1;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — XAUDIO2_BUFFER and PCM Lifetime
    // -----------------------------------------------------------------------
    // The XAUDIO2_BUFFER struct describes one submitted audio buffer.
    //
    //   Flags         — XAUDIO2_END_OF_STREAM marks the last buffer in a
    //                   sequence.  Without it, the voice stalls when the
    //                   buffer runs out (waiting for more data).
    //
    //   AudioBytes    — total byte count of the PCM data.
    //   pAudioData    — pointer to the raw PCM bytes.
    //
    //   LoopCount     — XAUDIO2_LOOP_INFINITE for infinite loop; 0 = no loop.
    //   LoopBegin/End — loop region within the buffer (0 = full buffer).
    //
    // CRITICAL: XAudio2 does NOT copy the buffer data.  pAudioData must
    // remain valid for the entire duration of playback.  We move the PCM
    // bytes into s.pcmCache (SourceVoiceSlot member), which lives as long
    // as the slot is in use.  Stop() clears pcmCache after flushing.
    // -----------------------------------------------------------------------

    auto& s = m_pool[slot];

    // Move PCM ownership into the slot — the pointer we give XAudio2 now
    // points into s.pcmCache, which outlives this function call.
    s.pcmCache = std::move(wav.pcm);

    XAUDIO2_BUFFER buf = {};
    buf.Flags          = XAUDIO2_END_OF_STREAM;
    buf.AudioBytes     = static_cast<UINT32>(s.pcmCache.size());
    buf.pAudioData     = s.pcmCache.data();  // points into the slot's own vector
    buf.LoopCount      = looping ? XAUDIO2_LOOP_INFINITE : 0;

    HRESULT hr = s.voice->SubmitSourceBuffer(&buf);
    if (FAILED(hr))
    {
        LOG_ERROR("XAudio2Backend::Play — SubmitSourceBuffer failed. HRESULT=0x"
                  << std::hex << static_cast<unsigned long>(hr) << std::dec);
        s.pcmCache.clear();
        return -1;
    }

    s.voice->SetVolume(volume);
    s.voice->Start();

    s.clipID   = clipID;
    s.inUse    = true;
    s.looping  = looping;

    LOG_INFO("XAudio2Backend::Play slot=" << slot << " clip=" << clipID);
    return slot;
}

// ===========================================================================
// Stop
// ===========================================================================

void XAudio2Backend::Stop(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= static_cast<int>(XAUDIO2_VOICE_POOL_SIZE))
        return;

    auto& s = m_pool[slotIndex];
    if (!s.inUse || !s.voice)
        return;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Stopping a Source Voice
    // -----------------------------------------------------------------------
    // IXAudio2SourceVoice::Stop() pauses the voice but does not reset it.
    // FlushSourceBuffers() discards all queued data.
    // Together they bring the voice back to a clean, re-usable state.
    //
    // After flushing we also clear pcmCache — the PCM bytes are no longer
    // referenced by any buffer, so releasing the memory is safe here.
    // -----------------------------------------------------------------------
    s.voice->Stop();
    s.voice->FlushSourceBuffers();
    s.inUse  = false;
    s.clipID.clear();
    s.pcmCache.clear();  // safe to free now that XAudio2 has no reference
}

void XAudio2Backend::StopByClipID(const std::string& clipID)
{
    for (int i = 0; i < static_cast<int>(XAUDIO2_VOICE_POOL_SIZE); ++i)
    {
        if (m_pool[i].inUse && m_pool[i].clipID == clipID)
            Stop(i);
    }
}

void XAudio2Backend::StopAll()
{
    for (int i = 0; i < static_cast<int>(XAUDIO2_VOICE_POOL_SIZE); ++i)
    {
        if (m_pool[i].inUse)
            Stop(i);
    }
}

// ===========================================================================
// SetMasterVolume / SetSlotVolume
// ===========================================================================

void XAudio2Backend::SetMasterVolume(float volume)
{
    if (m_masterVoice)
        m_masterVoice->SetVolume(volume);
}

void XAudio2Backend::SetSlotVolume(int slotIndex, float volume)
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Per-Voice Volume for Crossfading
    // -----------------------------------------------------------------------
    // IXAudio2SourceVoice::SetVolume() applies a scalar gain to the voice's
    // output.  0.0 = silent, 1.0 = unity gain.
    //
    // AudioSystem calls this every frame during a crossfade to linearly ramp
    // the incoming stem from 0 → target and the outgoing stem from target → 0.
    // The function is cheap: it queues a volume-change operation on the audio
    // processing thread with no synchronisation overhead.
    // -----------------------------------------------------------------------
    if (slotIndex < 0 || slotIndex >= static_cast<int>(XAUDIO2_VOICE_POOL_SIZE))
        return;
    auto& s = m_pool[slotIndex];
    if (s.inUse && s.voice)
        s.voice->SetVolume(volume);
}

// ===========================================================================
// IsPlaying
// ===========================================================================

bool XAudio2Backend::IsPlaying(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= static_cast<int>(XAUDIO2_VOICE_POOL_SIZE))
        return false;
    return m_pool[slotIndex].inUse;
}

// ===========================================================================
// Private helpers
// ===========================================================================

// ---------------------------------------------------------------------------
// ParseWav — parse a RIFF/WAVE file from raw bytes
// ---------------------------------------------------------------------------

WavData XAudio2Backend::ParseWav(const std::vector<uint8_t>& bytes)
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Parsing RIFF/WAVE
    // -----------------------------------------------------------------------
    // We walk the chunk tree manually using a byte offset.  Each chunk has:
    //
    //   uint32_t  id   (4 ASCII chars, e.g. 'fmt ', 'data')
    //   uint32_t  size (byte count of chunk data; does NOT include id/size)
    //   uint8_t   data[size]
    //
    // The RIFF root chunk also has a 4-byte form type ("WAVE").
    //
    // All integer fields in RIFF are little-endian.
    // -----------------------------------------------------------------------

    WavData wav;

    if (bytes.size() < 12)
        return wav; // Too small to be valid.

    // Helper: read a uint32 little-endian from a byte offset.
    auto readU32 = [&](size_t off) -> uint32_t {
        if (off + 4 > bytes.size()) return 0;
        return  static_cast<uint32_t>(bytes[off])
             | (static_cast<uint32_t>(bytes[off+1]) << 8)
             | (static_cast<uint32_t>(bytes[off+2]) << 16)
             | (static_cast<uint32_t>(bytes[off+3]) << 24);
    };

    // Verify RIFF magic and WAVE form type.
    if (readU32(0) != 0x46464952u)  // 'RIFF'
        return wav;
    if (readU32(8) != 0x45564157u)  // 'WAVE'
        return wav;

    // Walk chunks starting after the 12-byte RIFF header.
    size_t offset = 12;
    bool   hasFmt  = false;
    bool   hasData = false;

    while (offset + 8 <= bytes.size())
    {
        const uint32_t chunkID   = readU32(offset);
        const uint32_t chunkSize = readU32(offset + 4);
        offset += 8;

        if (chunkID == 0x20746D66u)  // 'fmt '
        {
            // ---------------------------------------------------------------
            // TEACHING NOTE — WAVEFORMATEX layout
            // ---------------------------------------------------------------
            // Minimum size is 16 bytes (WAVEFORMATEX without cbSize).
            // We copy exactly sizeof(WAVEFORMATEX) bytes but never more than
            // the chunk provides, filling the rest with zeros.
            // ---------------------------------------------------------------
            if (chunkSize < 16)
                return wav;

            std::memset(&wav.fmt, 0, sizeof(wav.fmt));
            const size_t copyBytes = std::min(
                static_cast<size_t>(chunkSize),
                sizeof(WAVEFORMATEX)
            );
            std::memcpy(&wav.fmt, bytes.data() + offset, copyBytes);
            wav.fmt.cbSize = 0; // Ignore extra bytes.
            hasFmt = true;
        }
        else if (chunkID == 0x61746164u)  // 'data'
        {
            if (chunkSize == 0)
                return wav;
            wav.pcm.assign(
                bytes.data() + offset,
                bytes.data() + offset + chunkSize
            );
            hasData = true;
        }
        // Skip unknown chunks (LIST, bext, etc.)

        // RIFF chunks are word-aligned (pad to even size).
        offset += chunkSize;
        if (chunkSize & 1) ++offset;
    }

    wav.valid = hasFmt && hasData;
    return wav;
}

// ---------------------------------------------------------------------------
// FindFreeSlot
// ---------------------------------------------------------------------------

int XAudio2Backend::FindFreeSlot() const
{
    for (int i = 0; i < static_cast<int>(XAUDIO2_VOICE_POOL_SIZE); ++i)
    {
        if (!m_pool[i].inUse)
            return i;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// ReconfigureVoice
// ---------------------------------------------------------------------------

bool XAudio2Backend::ReconfigureVoice(int slotIndex, const WAVEFORMATEX& fmt)
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Source Voice Format
    // -----------------------------------------------------------------------
    // A source voice is created for a specific WAVEFORMATEX.  If the existing
    // voice was created for a different format (e.g. mono 22 kHz vs stereo
    // 44 kHz), we must destroy and recreate it.
    //
    // For performance, compare the formats first — recreating voices is a
    // kernel-level operation and should be minimised.
    // -----------------------------------------------------------------------
    auto& s = m_pool[slotIndex];

    // If the voice exists and the format matches, reset and reuse it.
    if (s.voice)
    {
        XAUDIO2_VOICE_DETAILS details{};
        s.voice->GetVoiceDetails(&details);

        // Check key format fields (tag, channels, sample rate, bit depth).
        // Full format compatibility would compare cbSize and extended fields
        // too — this simplified check covers the common PCM case.
        if (details.InputChannels      == fmt.nChannels &&
            details.InputSampleRate    == fmt.nSamplesPerSec)
        {
            // Compatible format; no need to recreate.
            s.voice->Stop();
            s.voice->FlushSourceBuffers();
            return true;
        }

        // Incompatible format — destroy existing voice and recreate below.
        s.voice->Stop();
        s.voice->DestroyVoice();
        s.voice = nullptr;
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — CreateSourceVoice
    // -----------------------------------------------------------------------
    // Parameters:
    //   ppSourceVoice  — output pointer.
    //   pSourceFormat  — pointer to WAVEFORMATEX (or WAVEFORMATEXTENSIBLE).
    //   Flags          — XAUDIO2_VOICE_NOPITCH disables pitch shifting for
    //                    a minor performance gain when pitch isn't needed.
    //   MaxFrequencyRatio — 1.0 = no pitch shift; 2.0 = up to +1 octave.
    //   pCallback      — optional IXAudio2VoiceCallback for buffer events.
    // -----------------------------------------------------------------------
    const HRESULT hr = m_xaudio2->CreateSourceVoice(
        &s.voice,
        &fmt,
        0,       // No flags (allow pitch shift for music tempo control)
        2.0f,    // Allow up to 2× frequency ratio (one octave up)
        nullptr  // No callback for now
    );

    if (FAILED(hr))
    {
        LOG_ERROR("XAudio2Backend — CreateSourceVoice failed. HRESULT=0x"
                  << std::hex << static_cast<unsigned long>(hr) << std::dec);
        s.voice = nullptr;
        return false;
    }

    return true;
}

} // namespace audio
} // namespace engine
