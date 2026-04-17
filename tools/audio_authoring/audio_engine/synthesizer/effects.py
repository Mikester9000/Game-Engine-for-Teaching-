"""
Synthesizer audio effects: chorus, delay, and distortion.

TEACHING NOTE — Audio Effects Chain
=====================================
Effects are applied as a post-processing chain on a rendered audio signal.
Common effects used in game music:
  - Chorus:      slight pitch/time variation creates a "thicker" sound
  - Delay/Echo:  repeating copies of the signal at a set interval
  - Distortion:  non-linear clipping creates harmonic richness

Real game engines (Wwise, FMOD) implement these as DSP nodes in a graph.
"""

from __future__ import annotations

import numpy as np

__all__ = ["chorus", "simple_delay", "soft_clip"]


def chorus(
    signal: np.ndarray,
    sample_rate: int = 44100,
    depth_ms: float = 3.0,
    rate_hz: float = 0.5,
    wet: float = 0.4,
) -> np.ndarray:
    """Apply a simple chorus effect.

    Parameters
    ----------
    signal:
        Mono float32 array.
    sample_rate:
        Audio sample rate in Hz.
    depth_ms:
        Maximum delay depth in milliseconds.
    rate_hz:
        LFO modulation rate in Hz.
    wet:
        Mix level of the chorus signal (0.0 = dry, 1.0 = full wet).

    Returns
    -------
    np.ndarray
        Mono float32 array of the same length as *signal*.
    """
    n = len(signal)
    depth_samples = int(depth_ms * sample_rate / 1000.0)
    t = np.arange(n, dtype=np.float64) / sample_rate
    mod = depth_samples * (0.5 + 0.5 * np.sin(2.0 * np.pi * rate_hz * t))
    out = np.copy(signal).astype(np.float64)
    for i in range(n):
        delay_int = int(mod[i])
        frac = mod[i] - delay_int
        idx0 = max(0, i - delay_int)
        idx1 = max(0, i - delay_int - 1)
        delayed = signal[idx0] * (1.0 - frac) + signal[idx1] * frac
        out[i] = signal[i] * (1.0 - wet) + delayed * wet
    return out.astype(np.float32)


def simple_delay(
    signal: np.ndarray,
    sample_rate: int = 44100,
    delay_ms: float = 250.0,
    feedback: float = 0.4,
    wet: float = 0.3,
) -> np.ndarray:
    """Apply a simple tape-style echo / delay.

    Parameters
    ----------
    signal:
        Mono float32 array.
    sample_rate:
        Audio sample rate in Hz.
    delay_ms:
        Delay time in milliseconds.
    feedback:
        Feedback amount (0.0 – 0.95). Higher = more echoes.
    wet:
        Mix level of the delayed signal.

    Returns
    -------
    np.ndarray
        Mono float32 array of the same length as *signal*.
    """
    n = len(signal)
    delay_samples = int(delay_ms * sample_rate / 1000.0)
    out = np.copy(signal).astype(np.float64)
    delay_buf = np.zeros(n + delay_samples, dtype=np.float64)
    delay_buf[:n] = signal

    feedback = float(np.clip(feedback, 0.0, 0.95))
    for i in range(n):
        delayed = delay_buf[i + delay_samples]
        out[i] = signal[i] * (1.0 - wet) + delayed * wet
        delay_buf[i] += out[i] * feedback

    return out.astype(np.float32)


def soft_clip(
    signal: np.ndarray,
    drive: float = 2.0,
    ceiling: float = 0.95,
) -> np.ndarray:
    """Apply soft-clipping / saturation distortion using a tanh curve.

    Parameters
    ----------
    signal:
        Audio array (any shape).
    drive:
        Pre-gain before the tanh (> 1 = more distortion).
    ceiling:
        Output ceiling amplitude.

    Returns
    -------
    np.ndarray
        Distorted signal of the same shape as *signal*.

    TEACHING NOTE — Soft vs Hard Clipping
    Hard clipping (np.clip) creates a harsh, digital distortion.
    Soft clipping (tanh) smoothly saturates – similar to tube amplifiers.
    This creates a warmer, more musical character.
    """
    drive = max(1.0, float(drive))
    ceiling = float(np.clip(ceiling, 0.0, 1.0))
    out = np.tanh(signal.astype(np.float64) * drive) / np.tanh(drive)
    return (out * ceiling).astype(np.float32)
