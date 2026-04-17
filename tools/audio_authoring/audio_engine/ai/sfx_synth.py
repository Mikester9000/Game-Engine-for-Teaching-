"""
Procedural SFX synthesizer.

Generates sound effects using parametric synthesis techniques (additive,
FM, filtered noise, transient shaping).  Each SFX type has a tailored
recipe that produces a recognisable result without any external model or
sample library.

This module is used by :class:`~audio_engine.ai.backend.ProceduralBackend`
as the default SFX generation path.

SFX types supported
-------------------
``explosion``, ``footstep``, ``click``, ``beep``, ``hit``, ``impact``,
``whoosh``, ``laser``, ``coin``, ``pickup``, ``jump``, ``magic``, ``fire``,
``water``, ``wind``, ``ping``, ``generic``

Adding a new SFX type
---------------------
Define a function ``_sfx_<typename>(duration, pitch_hz, sr, rng)`` that
returns a mono float32 array, then add it to ``_SFX_FUNCTIONS``.
"""

from __future__ import annotations

from typing import Callable

import numpy as np

__all__ = ["synthesise_sfx"]


# ---------------------------------------------------------------------------
# Utility helpers
# ---------------------------------------------------------------------------

def _adsr(
    n_samples: int,
    sr: int,
    attack_ms: float = 5.0,
    decay_ms: float = 50.0,
    sustain: float = 0.7,
    release_ms: float = 100.0,
) -> np.ndarray:
    """Generate a normalised ADSR envelope of length *n_samples*."""
    a = max(1, int(sr * attack_ms / 1000.0))
    d = max(1, int(sr * decay_ms / 1000.0))
    r = max(1, int(sr * release_ms / 1000.0))
    s = max(0, n_samples - a - d - r)

    env = np.zeros(n_samples, dtype=np.float64)
    # Attack
    env[:a] = np.linspace(0.0, 1.0, a)
    # Decay
    end_d = min(a + d, n_samples)
    env[a:end_d] = np.linspace(1.0, sustain, end_d - a)
    # Sustain
    end_s = min(a + d + s, n_samples)
    env[end_d:end_s] = sustain
    # Release
    env[end_s:] = np.linspace(sustain, 0.0, n_samples - end_s)
    return env.astype(np.float32)


def _band_noise(n: int, lo: float, hi: float, sr: int, rng: np.random.Generator) -> np.ndarray:
    """Generate band-limited noise between *lo* and *hi* Hz."""
    from scipy.signal import butter, sosfilt  # type: ignore[import]

    noise = rng.standard_normal(n).astype(np.float64)
    nyq = sr / 2.0
    low = np.clip(lo / nyq, 1e-4, 0.999)
    high = np.clip(hi / nyq, 1e-4, 0.999)
    if high <= low:
        high = min(low + 0.01, 0.999)
    sos = butter(4, [low, high], btype="band", output="sos")
    return sosfilt(sos, noise).astype(np.float32)


def _sine(freq: float, duration: float, sr: int, amp: float = 1.0) -> np.ndarray:
    n = int(duration * sr)
    t = np.arange(n) / sr
    return (amp * np.sin(2.0 * np.pi * freq * t)).astype(np.float32)


def _exp_decay(n: int, decay_rate: float = 10.0) -> np.ndarray:
    """Exponential decay envelope from 1 to ≈0."""
    t = np.linspace(0.0, 1.0, n)
    return np.exp(-decay_rate * t).astype(np.float32)


# ---------------------------------------------------------------------------
# SFX type recipes
# ---------------------------------------------------------------------------

def _sfx_explosion(duration: float, pitch_hz: float | None, sr: int, rng: np.random.Generator) -> np.ndarray:
    """Low rumble + transient burst."""
    n = int(duration * sr)
    # Low-band noise burst
    noise = _band_noise(n, 30.0, 300.0, sr, rng)
    mid_noise = _band_noise(n, 300.0, 2000.0, sr, rng)
    env = _exp_decay(n, decay_rate=3.0 / max(duration, 0.1))
    return np.clip((0.7 * noise + 0.3 * mid_noise) * env, -1.0, 1.0)


def _sfx_footstep(duration: float, pitch_hz: float | None, sr: int, rng: np.random.Generator) -> np.ndarray:
    """Short transient click with low-mid body."""
    n = int(max(duration, 0.1) * sr)
    noise = _band_noise(n, 80.0, 2000.0, sr, rng)
    env = _adsr(n, sr, attack_ms=2.0, decay_ms=30.0, sustain=0.0, release_ms=20.0)
    return np.clip(noise * env * 0.8, -1.0, 1.0)


def _sfx_click(duration: float, pitch_hz: float | None, sr: int, rng: np.random.Generator) -> np.ndarray:
    """Short UI-style click."""
    n = int(max(duration, 0.05) * sr)
    freq = pitch_hz or 1200.0
    tone = _sine(freq, max(duration, 0.05), sr)
    env = _adsr(n, sr, attack_ms=1.0, decay_ms=20.0, sustain=0.0, release_ms=10.0)
    return np.clip(tone[:n] * env, -1.0, 1.0)


def _sfx_beep(duration: float, pitch_hz: float | None, sr: int, rng: np.random.Generator) -> np.ndarray:
    """Pure tone beep."""
    freq = pitch_hz or 880.0
    tone = _sine(freq, duration, sr)
    n = len(tone)
    env = _adsr(n, sr, attack_ms=5.0, decay_ms=10.0, sustain=1.0, release_ms=40.0)
    return np.clip(tone * env * 0.8, -1.0, 1.0)


def _sfx_hit(duration: float, pitch_hz: float | None, sr: int, rng: np.random.Generator) -> np.ndarray:
    """Impact hit with punch."""
    n = int(max(duration, 0.1) * sr)
    freq = pitch_hz or 150.0
    tone = _sine(freq, max(duration, 0.1), sr)
    noise = _band_noise(n, 200.0, 4000.0, sr, rng)
    env = _adsr(n, sr, attack_ms=2.0, decay_ms=80.0, sustain=0.0, release_ms=40.0)
    return np.clip((0.5 * tone[:n] + 0.5 * noise) * env, -1.0, 1.0)


def _sfx_impact(duration: float, pitch_hz: float | None, sr: int, rng: np.random.Generator) -> np.ndarray:
    return _sfx_hit(duration, pitch_hz, sr, rng)


def _sfx_whoosh(duration: float, pitch_hz: float | None, sr: int, rng: np.random.Generator) -> np.ndarray:
    """Rising/falling noise sweep."""
    n = int(max(duration, 0.2) * sr)
    noise = _band_noise(n, 500.0, 8000.0, sr, rng)
    # Bow-shaped amplitude
    t = np.linspace(0.0, np.pi, n)
    env = np.sin(t).astype(np.float32)
    return np.clip(noise * env * 0.9, -1.0, 1.0)


def _sfx_laser(duration: float, pitch_hz: float | None, sr: int, rng: np.random.Generator) -> np.ndarray:
    """Sci-fi downward frequency sweep."""
    n = int(max(duration, 0.2) * sr)
    start_freq = pitch_hz or 2000.0
    end_freq = start_freq * 0.2
    t = np.arange(n) / sr
    freq = np.linspace(start_freq, end_freq, n)
    phase = np.cumsum(2.0 * np.pi * freq / sr)
    wave = np.sin(phase).astype(np.float32)
    env = _exp_decay(n, decay_rate=4.0)
    return np.clip(wave * env, -1.0, 1.0)


def _sfx_coin(duration: float, pitch_hz: float | None, sr: int, rng: np.random.Generator) -> np.ndarray:
    """Classic coin collect chime (two-note ding)."""
    d = max(duration, 0.15) / 2.0
    n = int(d * sr)
    f1 = pitch_hz or 1047.0  # C6
    f2 = f1 * 1.5            # G6 (perfect fifth)
    t = np.arange(n) / sr
    env = _exp_decay(n, decay_rate=8.0)
    note1 = np.sin(2.0 * np.pi * f1 * t).astype(np.float32) * env
    note2 = np.sin(2.0 * np.pi * f2 * t).astype(np.float32) * env
    return np.concatenate([note1, note2]) * 0.9


def _sfx_pickup(duration: float, pitch_hz: float | None, sr: int, rng: np.random.Generator) -> np.ndarray:
    """Item pickup arpeggio."""
    return _sfx_coin(duration, pitch_hz, sr, rng)


def _sfx_jump(duration: float, pitch_hz: float | None, sr: int, rng: np.random.Generator) -> np.ndarray:
    """Rising frequency sweep jump sound."""
    n = int(max(duration, 0.2) * sr)
    start_freq = pitch_hz or 200.0
    end_freq = start_freq * 3.0
    t = np.arange(n) / sr
    freq = np.linspace(start_freq, end_freq, n)
    phase = np.cumsum(2.0 * np.pi * freq / sr)
    wave = np.sin(phase).astype(np.float32)
    env = _adsr(n, sr, attack_ms=5.0, decay_ms=100.0, sustain=0.0, release_ms=50.0)
    return np.clip(wave * env, -1.0, 1.0)


def _sfx_magic(duration: float, pitch_hz: float | None, sr: int, rng: np.random.Generator) -> np.ndarray:
    """Shimmering magic sparkle."""
    n = int(max(duration, 0.5) * sr)
    # Stacked detuned sines with random phases
    base = pitch_hz or 880.0
    signal = np.zeros(n, dtype=np.float64)
    for harmonic in [1.0, 1.5, 2.0, 3.0, 4.0]:
        freq = base * harmonic
        phase_offset = rng.uniform(0, 2 * np.pi)
        t = np.arange(n) / sr
        signal += np.sin(2.0 * np.pi * freq * t + phase_offset) / 5.0
    env = _adsr(n, sr, attack_ms=50.0, decay_ms=200.0, sustain=0.3, release_ms=200.0)
    return np.clip(signal * env, -1.0, 1.0).astype(np.float32)


def _sfx_fire(duration: float, pitch_hz: float | None, sr: int, rng: np.random.Generator) -> np.ndarray:
    """Crackling fire."""
    n = int(max(duration, 0.5) * sr)
    noise = _band_noise(n, 100.0, 1200.0, sr, rng)
    # Slow amplitude modulation to simulate flickering
    t = np.linspace(0.0, duration, n)
    mod = 0.6 + 0.4 * np.sin(2.0 * np.pi * 3.0 * t) * rng.uniform(0.8, 1.2, n)
    return np.clip((noise * mod).astype(np.float32), -1.0, 1.0)


def _sfx_water(duration: float, pitch_hz: float | None, sr: int, rng: np.random.Generator) -> np.ndarray:
    """Flowing water."""
    n = int(max(duration, 0.5) * sr)
    noise = _band_noise(n, 300.0, 3000.0, sr, rng)
    t = np.linspace(0.0, duration, n)
    mod = 0.7 + 0.3 * np.sin(2.0 * np.pi * 5.0 * t)
    return np.clip((noise * mod).astype(np.float32), -1.0, 1.0)


def _sfx_wind(duration: float, pitch_hz: float | None, sr: int, rng: np.random.Generator) -> np.ndarray:
    """Howling wind."""
    n = int(max(duration, 0.5) * sr)
    noise = _band_noise(n, 50.0, 800.0, sr, rng)
    t = np.linspace(0.0, duration, n)
    mod = 0.5 + 0.5 * np.abs(np.sin(2.0 * np.pi * 0.5 * t))
    return np.clip((noise * mod).astype(np.float32), -1.0, 1.0)


def _sfx_ping(duration: float, pitch_hz: float | None, sr: int, rng: np.random.Generator) -> np.ndarray:
    """UI notification ping."""
    return _sfx_beep(duration, pitch_hz or 1500.0, sr, rng)


def _sfx_generic(duration: float, pitch_hz: float | None, sr: int, rng: np.random.Generator) -> np.ndarray:
    """Generic 'blip' sound."""
    return _sfx_beep(duration, pitch_hz, sr, rng)


# ---------------------------------------------------------------------------
# SFX function registry
# ---------------------------------------------------------------------------

_SFX_FUNCTIONS: dict[str, Callable] = {
    "explosion": _sfx_explosion,
    "footstep":  _sfx_footstep,
    "click":     _sfx_click,
    "beep":      _sfx_beep,
    "hit":       _sfx_hit,
    "impact":    _sfx_impact,
    "whoosh":    _sfx_whoosh,
    "laser":     _sfx_laser,
    "coin":      _sfx_coin,
    "pickup":    _sfx_pickup,
    "jump":      _sfx_jump,
    "land":      _sfx_footstep,
    "door":      _sfx_hit,
    "open":      _sfx_whoosh,
    "close":     _sfx_click,
    "magic":     _sfx_magic,
    "spell":     _sfx_magic,
    "fire":      _sfx_fire,
    "water":     _sfx_water,
    "wind":      _sfx_wind,
    "ping":      _sfx_ping,
    "alert":     _sfx_ping,
    "notification": _sfx_ping,
    "generic":   _sfx_generic,
}


def synthesise_sfx(
    sfx_type: str,
    duration: float,
    pitch_hz: float | None = None,
    sample_rate: int = 44100,
    seed: int | None = None,
) -> np.ndarray:
    """Generate a sound effect using procedural synthesis.

    Parameters
    ----------
    sfx_type:
        SFX category keyword.  Unknown types fall back to ``"generic"``.
    duration:
        Target duration in seconds.
    pitch_hz:
        Optional base pitch frequency override in Hz.
    sample_rate:
        Audio sample rate in Hz.
    seed:
        RNG seed for reproducibility.

    Returns
    -------
    np.ndarray
        Mono float32 audio array.

    Example
    -------
    >>> audio = synthesise_sfx("explosion", duration=1.5)
    """
    rng = np.random.default_rng(seed)
    fn = _SFX_FUNCTIONS.get(sfx_type, _sfx_generic)
    audio = fn(duration, pitch_hz, sample_rate, rng)
    # Normalise to -3 dBFS
    peak = np.max(np.abs(audio))
    if peak > 1e-9:
        target = 10.0 ** (-3.0 / 20.0)
        audio = (audio / peak * target).astype(np.float32)
    return audio


def available_sfx_types() -> list[str]:
    """Return the list of recognised SFX type keywords."""
    return sorted(_SFX_FUNCTIONS)
