"""
State-variable filter for synthesizer voice shaping.

TEACHING NOTE — Low-Pass Filters
===================================
A filter removes certain frequencies from an audio signal.
  - Low-pass  filter (LPF): keeps low freqs, cuts high freqs (makes sound "dark/muffled")
  - High-pass filter (HPF): keeps high freqs, cuts low freqs (thins out bass)
  - Band-pass filter (BPF): keeps a band around the cutoff frequency

The cutoff frequency sets the "knee" point.
Q (resonance) amplifies frequencies near the cutoff – classic synth "wah" sound.

This uses a Butterworth SOS design from scipy, which is the industry standard
for audio filtering.
"""

from __future__ import annotations

from typing import Literal

import numpy as np
from scipy.signal import butter, sosfilt

__all__ = ["SynthFilter"]

FilterMode = Literal["lowpass", "highpass", "bandpass"]


class SynthFilter:
    """Butterworth filter for synthesizer voice shaping.

    Parameters
    ----------
    sample_rate:
        Audio sample rate in Hz.
    mode:
        Filter mode: ``"lowpass"``, ``"highpass"``, or ``"bandpass"``.
    cutoff:
        Filter cutoff frequency in Hz.
    q:
        Resonance / quality factor.  Higher Q → sharper, more resonant peak.
    order:
        Filter order (1–4). Higher order = steeper roll-off.

    Example
    -------
    >>> filt = SynthFilter(sample_rate=44100, mode="lowpass", cutoff=2000.0)
    >>> filtered = filt.process(audio_array)
    """

    def __init__(
        self,
        sample_rate: int = 44100,
        mode: FilterMode = "lowpass",
        cutoff: float = 2000.0,
        q: float = 0.707,
        order: int = 2,
    ) -> None:
        self.sample_rate = sample_rate
        self.mode = mode
        self.cutoff = float(np.clip(cutoff, 10.0, sample_rate / 2.0 - 1.0))
        self.q = max(0.1, float(q))
        self.order = max(1, min(4, order))
        self._sos = self._design()

    def _design(self) -> np.ndarray:
        nyq = self.sample_rate / 2.0
        if self.mode == "bandpass":
            # Compute bandwidth from Q: bw = fc / Q
            bw = self.cutoff / self.q
            low = max(10.0, self.cutoff - bw / 2.0) / nyq
            high = min(0.9999, self.cutoff + bw / 2.0) / nyq
            return butter(self.order, [low, high], btype="bandpass", output="sos")
        else:
            btype = "low" if self.mode == "lowpass" else "high"
            return butter(self.order, self.cutoff / nyq, btype=btype, output="sos")

    def process(self, signal: np.ndarray) -> np.ndarray:
        """Apply the filter to *signal*.

        Parameters
        ----------
        signal:
            Mono (N,) or stereo (N, 2) float32/float64 array.

        Returns
        -------
        np.ndarray
            Filtered signal, same shape and dtype as input.
        """
        dtype = signal.dtype
        sig = signal.astype(np.float64)
        if sig.ndim == 2:
            left = sosfilt(self._sos, sig[:, 0])
            right = sosfilt(self._sos, sig[:, 1])
            result = np.stack([left, right], axis=1)
        else:
            result = sosfilt(self._sos, sig)
        return result.astype(dtype)
