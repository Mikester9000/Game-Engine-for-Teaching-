"""EBU R128 / ITU-R BS.1770 loudness meter."""
from __future__ import annotations
from dataclasses import dataclass
import numpy as np
from scipy.signal import sosfilt, butter
__all__ = ["LoudnessMeter", "LoudnessResult"]

@dataclass
class LoudnessResult:
    integrated_lufs: float
    true_peak_dbfs: float
    loudness_range_lu: float

def _k_weight_filter(sample_rate):
    fs = float(sample_rate)
    db, fc = 3.99985, 1681.97
    A = 10.0 ** (db / 40.0)
    w0 = 2.0 * np.pi * fc / fs
    cos_w0, sin_w0 = np.cos(w0), np.sin(w0)
    alpha = sin_w0 / 2.0 * np.sqrt((A + 1.0 / A) * (1.0 / 0.9 - 1.0) + 2.0)
    b0 = A * ((A+1) + (A-1)*cos_w0 + 2*np.sqrt(A)*alpha)
    b1 = -2*A * ((A-1) + (A+1)*cos_w0)
    b2 = A * ((A+1) + (A-1)*cos_w0 - 2*np.sqrt(A)*alpha)
    a0 = (A+1) - (A-1)*cos_w0 + 2*np.sqrt(A)*alpha
    a1 = 2 * ((A-1) - (A+1)*cos_w0)
    a2 = (A+1) - (A-1)*cos_w0 - 2*np.sqrt(A)*alpha
    stage1 = np.array([[b0/a0, b1/a0, b2/a0, 1.0, a1/a0, a2/a0]])
    sos2 = butter(2, 38.1 / (fs / 2.0), btype="high", output="sos")
    return np.vstack([stage1, sos2])

def _mean_square_blocks(signal, block_samples):
    step = block_samples // 4
    blocks = []
    i = 0
    while i + block_samples <= len(signal):
        blocks.append(np.mean(signal[i:i+block_samples] ** 2))
        i += step
    return np.array(blocks) if blocks else np.array([0.0])

class LoudnessMeter:
    """ITU-R BS.1770-4 / EBU R128 loudness meter."""
    def __init__(self, sample_rate=44100):
        self.sample_rate = sample_rate
        self._sos = _k_weight_filter(sample_rate)
        self._block_samples = int(sample_rate * 0.4)
    def measure(self, audio):
        return LoudnessResult(integrated_lufs=self.integrated_loudness(audio),
                              true_peak_dbfs=self.true_peak(audio),
                              loudness_range_lu=self.loudness_range(audio))
    def integrated_loudness(self, audio):
        kw = self._k_weight(audio)
        if kw.ndim == 2:
            ch_ms = [_mean_square_blocks(kw[:, ch], self._block_samples) for ch in range(kw.shape[1])]
            min_len = min(len(b) for b in ch_ms)
            block_ms = sum(b[:min_len] for b in ch_ms)
        else:
            block_ms = _mean_square_blocks(kw, self._block_samples)
        passing = block_ms[block_ms >= 1e-7]
        if len(passing) == 0:
            return -120.0
        rel_threshold = np.mean(passing) * 0.1
        gated = passing[passing >= rel_threshold]
        return float(-0.691 + 10.0 * np.log10(np.mean(gated))) if len(gated) else -120.0
    def true_peak(self, audio):
        sig = np.max(np.abs(audio.astype(np.float64)), axis=1) if audio.ndim == 2 else np.abs(audio.astype(np.float64))
        peak = float(np.max(sig))
        return 20.0 * np.log10(peak) if peak > 1e-12 else -120.0
    def loudness_range(self, audio):
        kw = self._k_weight(audio)
        block_3s = int(self.sample_rate * 3.0)
        mono = np.mean(kw, axis=1) if kw.ndim == 2 else kw
        st_blocks, i = [], 0
        while i + block_3s <= len(mono):
            ms = np.mean(mono[i:i+block_3s] ** 2)
            if ms > 1e-12:
                st_blocks.append(-0.691 + 10.0 * np.log10(ms))
            i += self.sample_rate
        if len(st_blocks) < 2:
            return 0.0
        return float(max(0.0, np.percentile(st_blocks, 95) - np.percentile(st_blocks, 10)))
    def _k_weight(self, audio):
        sig = audio.astype(np.float64)
        if sig.ndim == 2:
            return np.stack([sosfilt(self._sos, sig[:, ch]) for ch in range(sig.shape[1])], axis=1)
        return sosfilt(self._sos, sig)
