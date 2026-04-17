"""Convolution reverb using a synthetic impulse response."""
from __future__ import annotations
from pathlib import Path
import numpy as np
from scipy.signal import fftconvolve, butter, sosfilt
__all__ = ["ConvolutionReverb"]

def _build_synthetic_ir(sample_rate, rt60, room_size, brightness, pre_delay_ms, rng):
    ir_length = max(int(rt60 * sample_rate * (0.5 + 0.5 * room_size)), 128)
    pre_delay_samples = int(pre_delay_ms * sample_rate / 1000.0)
    t = np.linspace(0.0, rt60, ir_length)
    decay = np.exp(-6.908 * t / rt60)
    noise = rng.standard_normal(ir_length) * decay
    cutoff = np.clip(800.0 + brightness * 16000.0, 100.0, sample_rate / 2.0 - 100.0)
    sos = butter(2, cutoff / (sample_rate / 2.0), btype="low", output="sos")
    noise = sosfilt(sos, noise)
    peak = np.max(np.abs(noise))
    if peak > 1e-9:
        noise /= peak
    if pre_delay_samples > 0:
        noise = np.concatenate([np.zeros(pre_delay_samples), noise])
    return noise.astype(np.float64)

class ConvolutionReverb:
    """Convolution-based room reverb."""
    def __init__(self, sample_rate=44100, rt60=1.5, room_size=0.5, wet=0.3, brightness=0.5, pre_delay_ms=20.0, seed=42):
        self.sample_rate = sample_rate
        self.wet = np.clip(wet, 0.0, 1.0)
        self._rng = np.random.default_rng(seed)
        self._ir = _build_synthetic_ir(sample_rate, rt60, room_size, brightness, pre_delay_ms, self._rng)

    def process(self, signal: np.ndarray) -> np.ndarray:
        stereo = signal.ndim == 2
        sig = signal.astype(np.float64)
        if stereo:
            left_wet = fftconvolve(sig[:, 0], self._ir, mode="full")[:len(sig)]
            right_wet = fftconvolve(sig[:, 1], self._ir, mode="full")[:len(sig)]
            wet_sig = np.stack([left_wet, right_wet], axis=1)
        else:
            wet_sig = fftconvolve(sig, self._ir, mode="full")[:len(sig)]
        result = self.wet * wet_sig + (1.0 - self.wet) * sig
        return result.astype(np.float32)
