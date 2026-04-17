"""
Oscillators – basic waveform generators for additive synthesis.

TEACHING NOTE — Additive Synthesis
====================================
All complex sounds can be built from simple sine waves added together.
Each oscillator generates one of the basic waveform types used in synthesis:
  - Sine:     purest tone; only the fundamental frequency
  - Square:   odd harmonics; harsh/buzzy; great for bass lines
  - Sawtooth: all harmonics; bright; great for strings/leads
  - Triangle: odd harmonics, attenuated; softer than square
  - Noise:    all frequencies; used for percussion/wind/explosions

The sample rate determines the number of audio samples per second (e.g. 44100 Hz).
"""

from __future__ import annotations

import numpy as np

__all__ = ["Oscillator"]


class Oscillator:
    """Band-limited wavetable oscillator.

    Generates a mono float32 sample buffer at the requested frequency and
    duration using the specified waveform type.

    Parameters
    ----------
    sample_rate:
        Audio sample rate in Hz (default 44 100).
    waveform:
        One of ``"sine"``, ``"square"``, ``"sawtooth"``, ``"triangle"``,
        ``"noise"``.

    TEACHING NOTE — Band-limiting
    To avoid aliasing (unwanted high-frequency artefacts), a real oscillator
    filters out harmonics above the Nyquist frequency (sample_rate / 2).
    Here we use a simplified poly-BLEP (polynomial Band-Limited Step) approach:
    the square/saw are produced via the sine sum up to Nyquist, which is
    accurate enough for game audio.
    """

    _WAVEFORMS = frozenset({"sine", "square", "sawtooth", "triangle", "noise"})

    def __init__(self, sample_rate: int = 44100, waveform: str = "sine") -> None:
        if waveform not in self._WAVEFORMS:
            raise ValueError(f"Unknown waveform '{waveform}'. Choose from: {sorted(self._WAVEFORMS)}")
        self.sample_rate = sample_rate
        self.waveform = waveform

    def generate(self, frequency: float, duration: float, amplitude: float = 1.0) -> np.ndarray:
        """Generate a mono float32 audio buffer.

        Parameters
        ----------
        frequency:
            Fundamental frequency in Hz.
        duration:
            Duration in seconds.
        amplitude:
            Output amplitude (0.0 – 1.0).

        Returns
        -------
        np.ndarray
            Mono float32 array of shape ``(N,)`` where N = int(duration * sample_rate).
        """
        n = max(1, int(duration * self.sample_rate))
        t = np.arange(n, dtype=np.float64) / self.sample_rate

        if self.waveform == "sine":
            signal = np.sin(2.0 * np.pi * frequency * t)
        elif self.waveform == "square":
            # Band-limited square via Fourier series (odd harmonics)
            signal = np.zeros(n, dtype=np.float64)
            max_harmonic = int(self.sample_rate / (2.0 * frequency)) if frequency > 0 else 1
            for k in range(1, max_harmonic + 1, 2):
                signal += np.sin(2.0 * np.pi * frequency * k * t) / k
            signal *= (4.0 / np.pi)
        elif self.waveform == "sawtooth":
            # Band-limited sawtooth via Fourier series (all harmonics)
            signal = np.zeros(n, dtype=np.float64)
            max_harmonic = int(self.sample_rate / (2.0 * frequency)) if frequency > 0 else 1
            for k in range(1, max_harmonic + 1):
                signal += ((-1.0) ** (k + 1)) * np.sin(2.0 * np.pi * frequency * k * t) / k
            signal *= (2.0 / np.pi)
        elif self.waveform == "triangle":
            # Band-limited triangle via Fourier series (odd harmonics, alternate sign)
            signal = np.zeros(n, dtype=np.float64)
            max_harmonic = int(self.sample_rate / (2.0 * frequency)) if frequency > 0 else 1
            for idx, k in enumerate(range(1, max_harmonic + 1, 2)):
                signal += ((-1.0) ** idx) * np.sin(2.0 * np.pi * frequency * k * t) / (k * k)
            signal *= (8.0 / (np.pi ** 2))
        elif self.waveform == "noise":
            rng = np.random.default_rng()
            signal = rng.standard_normal(n)

        peak = np.max(np.abs(signal))
        if peak > 1e-9:
            signal = signal / peak

        return (signal * amplitude).astype(np.float32)
