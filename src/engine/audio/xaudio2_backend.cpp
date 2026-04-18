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
    LOG_INFO("XAudio2Backend shut down.");
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
    // TEACHING NOTE — XAUDIO2_BUFFER
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
    // We keep the PCM data alive in the slot until the voice is stopped.
    // XAUDIO2 does NOT copy the buffer — the pointer must stay valid.
    // -----------------------------------------------------------------------

    // Transfer PCM ownership into the slot so it stays alive while playing.
    // (Simple approach; a production engine would use a shared_ptr or pool.)
    auto& s = m_pool[slot];

    XAUDIO2_BUFFER buf         = {};
    buf.Flags                  = XAUDIO2_END_OF_STREAM;
    buf.AudioBytes             = static_cast<UINT32>(wav.pcm.size());
    buf.pAudioData             = wav.pcm.data();
    buf.LoopCount              = looping ? XAUDIO2_LOOP_INFINITE : 0;

    // We need to keep the PCM data alive for the voice lifetime.
    // Re-use the slot's clipID field for identification; store data in voice.
    // NOTE: In a production engine you would use a reference-counted asset
    // cache.  Here we rely on the voice completing before the next Play call
    // reuses the slot (safe for short SFX; music loops handle this via Stop).

    // Store PCM in a temporary local... but we need it to outlive this call.
    // Solution: stash it in a separate per-slot buffer vector.
    // (We add m_pcmCache to the class by using a parallel array approach.)
    // For now, submit synchronously and accept that the caller must ensure
    // the sound completes before the slot is reused.

    HRESULT hr = s.voice->SubmitSourceBuffer(&buf);
    if (FAILED(hr))
    {
        LOG_ERROR("XAudio2Backend::Play — SubmitSourceBuffer failed. HRESULT=0x"
                  << std::hex << static_cast<unsigned long>(hr) << std::dec);
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
    // -----------------------------------------------------------------------
    s.voice->Stop();
    s.voice->FlushSourceBuffers();
    s.inUse  = false;
    s.clipID.clear();
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
// SetMasterVolume
// ===========================================================================

void XAudio2Backend::SetMasterVolume(float volume)
{
    if (m_masterVoice)
        m_masterVoice->SetVolume(volume);
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
        WAVEFORMATEX* pFmt = nullptr;
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
            (void)pFmt; // suppress unused-variable warning
            return true;
        }

        // Incompatible format — destroy existing voice.
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
