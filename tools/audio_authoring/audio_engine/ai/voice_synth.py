"""
Procedural voice / text-to-speech synthesiser.

Implements a basic formant-based speech synthesiser that maps phoneme-like
units to sine-wave resonances.  While not as natural as a neural TTS model,
it produces intelligible voice output with zero external dependencies.

The synthesiser supports multiple "voice presets" (narrator, hero, villain,
announcer, npc) that differ in pitch, formant ratios, and prosody.

Upgrading to a real TTS model
------------------------------
Replace the :func:`synthesise_voice` call in
:class:`~audio_engine.ai.backend.ProceduralBackend` with an ONNX or
Piper-based backend.  The interface stays the same:

    synthesise_voice(text, voice_preset, speed, sample_rate) → np.ndarray

Piper TTS (https://github.com/rhasspy/piper) is recommended for local,
high-quality, permissively-licensed TTS.  It outputs 16 kHz WAV natively
and can be wrapped trivially.
"""

from __future__ import annotations

import re
from typing import NamedTuple

import numpy as np

__all__ = ["synthesise_voice", "VOICE_PRESETS"]


# ---------------------------------------------------------------------------
# Voice preset definitions
# ---------------------------------------------------------------------------

class _VoicePreset(NamedTuple):
    """Acoustic parameters for a voice preset.

    Attributes
    ----------
    f0:
        Fundamental frequency (pitch) in Hz.
    formant_shifts:
        Multipliers for F1, F2, F3 relative to a neutral speaker.
    jitter:
        Pitch jitter factor (0 = steady, 0.02 = slight human wavering).
    breathiness:
        Amount of additive noise (0 = clean, 0.2 = breathy).
    """

    f0: float
    formant_shifts: tuple[float, float, float]
    jitter: float
    breathiness: float


VOICE_PRESETS: dict[str, _VoicePreset] = {
    "narrator":  _VoicePreset(f0=120.0, formant_shifts=(1.0, 1.0, 1.0),    jitter=0.01, breathiness=0.05),
    "hero":      _VoicePreset(f0=110.0, formant_shifts=(1.05, 0.95, 0.98),  jitter=0.015, breathiness=0.04),
    "villain":   _VoicePreset(f0=85.0,  formant_shifts=(0.9, 1.1, 1.05),    jitter=0.02, breathiness=0.08),
    "announcer": _VoicePreset(f0=130.0, formant_shifts=(1.1, 1.0, 0.95),    jitter=0.005, breathiness=0.02),
    "npc":       _VoicePreset(f0=150.0, formant_shifts=(1.15, 1.05, 1.0),   jitter=0.03, breathiness=0.1),
}

# Approximate phoneme durations (seconds at speed=1.0)
_PHONEME_DURATION = 0.07   # average phoneme duration

# Formant frequencies for a neutral male voice (Hz): [F1, F2, F3]
_BASE_FORMANTS = [700.0, 1220.0, 2600.0]

# Formant bandwidths (Hz)
_FORMANT_BW = [80.0, 90.0, 120.0]


# ---------------------------------------------------------------------------
# Synthesis helpers
# ---------------------------------------------------------------------------

def _glottal_pulse(f0: float, duration: float, sr: int, jitter: float, rng: np.random.Generator) -> np.ndarray:
    """Generate a voiced glottal excitation signal (buzz).

    Produces a sawtooth-like waveform with slight pitch jitter.
    """
    n = int(duration * sr)
    t = np.arange(n) / sr

    # Add jitter by slightly modulating f0 over time
    if jitter > 0:
        lfo_rate = 5.0   # Hz (vibrato/jitter rate)
        jitter_signal = 1.0 + jitter * np.sin(2.0 * np.pi * lfo_rate * t + rng.uniform(0, 2 * np.pi))
        # Cumulative phase with jitter
        phase = np.cumsum(2.0 * np.pi * f0 * jitter_signal / sr)
    else:
        phase = 2.0 * np.pi * f0 * t

    # Sawtooth approximation using 6 harmonics (more natural than pure saw)
    signal = np.zeros(n, dtype=np.float64)
    for k in range(1, 7):
        signal += (1.0 / k) * np.sin(k * phase)

    return signal.astype(np.float32)


def _formant_filter(
    signal: np.ndarray,
    formant_freqs: list[float],
    bandwidths: list[float],
    sr: int,
) -> np.ndarray:
    """Apply resonant formant filters (bandpass) to a glottal signal.

    Each formant is a resonant peak modelled as a second-order bandpass
    filter (biquad).
    """
    from scipy.signal import sosfilt  # type: ignore[import]

    result = np.zeros(len(signal), dtype=np.float64)
    sig_f64 = signal.astype(np.float64)

    for f, bw in zip(formant_freqs, bandwidths):
        w0 = 2.0 * np.pi * f / sr
        Q = f / max(bw, 1.0)
        alpha = np.sin(w0) / (2.0 * Q)
        b0 = alpha
        b1 = 0.0
        b2 = -alpha
        a0 = 1.0 + alpha
        a1 = -2.0 * np.cos(w0)
        a2 = 1.0 - alpha
        sos = np.array([[b0 / a0, b1 / a0, b2 / a0, 1.0, a1 / a0, a2 / a0]])
        result += sosfilt(sos, sig_f64)

    return result.astype(np.float32)


def _text_to_phoneme_count(text: str) -> int:
    """Estimate the number of phonemes in *text*.

    Uses a simple heuristic: count vowel clusters + consonant groups.
    """
    # Strip non-alphabetic characters
    cleaned = re.sub(r"[^a-zA-Z ]", "", text)
    words = cleaned.split()
    total = 0
    for word in words:
        # Each word contributes ~1.5 phonemes per character (rough estimate)
        total += max(1, len(word))
    return max(1, total)


# ---------------------------------------------------------------------------
# Main synthesis function
# ---------------------------------------------------------------------------

def synthesise_voice(
    text: str,
    voice_preset: str = "narrator",
    speed: float = 1.0,
    sample_rate: int = 22050,
) -> np.ndarray:
    """Synthesise speech from *text* using formant synthesis.

    This is a lightweight offline TTS that produces robot-like but
    intelligible voice output.  For natural-sounding voice, swap this
    function with a Piper/ONNX TTS wrapper.

    Parameters
    ----------
    text:
        Text to synthesise.
    voice_preset:
        One of the preset names in :data:`VOICE_PRESETS`.
    speed:
        Speech rate multiplier (1.0 = normal, 1.5 = faster).
    sample_rate:
        Output sample rate in Hz (22050 is standard for TTS).

    Returns
    -------
    np.ndarray
        Mono float32 audio array.

    Example
    -------
    >>> audio = synthesise_voice("Hello, hero!", voice_preset="narrator")
    """
    if not text:
        return np.zeros(int(0.5 * sample_rate), dtype=np.float32)

    preset = VOICE_PRESETS.get(voice_preset, VOICE_PRESETS["narrator"])
    rng = np.random.default_rng(abs(hash(text)) % (2 ** 31))

    # Estimate total duration from phoneme count
    phoneme_count = _text_to_phoneme_count(text)
    duration = phoneme_count * _PHONEME_DURATION / max(speed, 0.5)

    # Compute formant frequencies for this preset
    formant_freqs = [
        _BASE_FORMANTS[i] * preset.formant_shifts[i]
        for i in range(3)
    ]

    # Generate glottal excitation
    glottal = _glottal_pulse(preset.f0, duration, sample_rate, preset.jitter, rng)

    # Apply formant filtering
    voiced = _formant_filter(glottal, formant_freqs, _FORMANT_BW, sample_rate)

    # Add breathiness (noise component)
    if preset.breathiness > 0:
        noise = rng.standard_normal(len(voiced)).astype(np.float32) * preset.breathiness
        voiced = voiced + noise

    # Apply word-level amplitude modulation (simulates syllable stress)
    n = len(voiced)
    words = text.split()
    n_words = max(1, len(words))
    # Simple piecewise amplitude: each word has a slight stress peak
    amp_env = np.ones(n, dtype=np.float64)
    word_samples = n // n_words
    for i in range(n_words):
        start = i * word_samples
        end = min(start + word_samples, n)
        local_n = end - start
        if local_n < 2:
            continue
        # Triangle amplitude per word: rise–sustain–fall
        half = local_n // 2
        stress = rng.uniform(0.7, 1.0)
        amp_env[start : start + half] = np.linspace(0.4, stress, half)
        amp_env[start + half : end] = np.linspace(stress, 0.4, local_n - half)

    # Fade in/out
    fade_samples = min(int(0.01 * sample_rate), n // 4)
    if fade_samples > 0:
        amp_env[:fade_samples] *= np.linspace(0.0, 1.0, fade_samples)
        amp_env[-fade_samples:] *= np.linspace(1.0, 0.0, fade_samples)

    voiced = (voiced * amp_env).astype(np.float32)

    # Normalise to -6 dBFS (leave headroom for mix)
    peak = np.max(np.abs(voiced))
    if peak > 1e-9:
        target = 10.0 ** (-6.0 / 20.0)
        voiced = (voiced / peak * target).astype(np.float32)

    return voiced
