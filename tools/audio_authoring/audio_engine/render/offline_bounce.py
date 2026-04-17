"""OfflineBounce – deterministic render + mastering pipeline."""
from __future__ import annotations
from pathlib import Path
from typing import Literal
import numpy as np
from audio_engine.dsp.eq import EQ, EQBand
from audio_engine.dsp.compressor import Compressor
from audio_engine.dsp.limiter import Limiter
from audio_engine.dsp.dither import dither
from audio_engine.export.audio_exporter import AudioExporter
__all__ = ["OfflineBounce"]

def _lufs_loudness(signal, sample_rate):
    mono = np.mean(signal, axis=1) if signal.ndim == 2 else signal.astype(np.float32)
    mean_sq = np.mean(mono.astype(np.float64) ** 2)
    return 10.0 * np.log10(mean_sq) if mean_sq > 1e-12 else -120.0

class OfflineBounce:
    """Production render/mastering pipeline."""
    def __init__(self, sample_rate=44100, bit_depth=16, target_lufs=-16.0, ceiling_db=-0.3, apply_master_eq=True, apply_compression=True):
        self.sample_rate = sample_rate
        self.target_lufs = target_lufs
        self._exporter = AudioExporter(sample_rate=sample_rate, bit_depth=bit_depth)
        self._bit_depth = bit_depth
        self._eq = self._build_master_eq() if apply_master_eq else None
        self._compressor = Compressor(sample_rate=sample_rate, threshold_db=-24.0, ratio=2.0, attack_ms=30.0, release_ms=200.0, knee_db=6.0, makeup_gain_db=1.0) if apply_compression else None
        self._limiter = Limiter(sample_rate=sample_rate, ceiling_db=ceiling_db)
    def _build_master_eq(self):
        eq = EQ(self.sample_rate)
        eq.add_band(EQBand(30.0, gain_db=0.0, q=0.707, band_type="high_pass"))
        eq.add_band(EQBand(120.0, gain_db=-1.5, q=0.707, band_type="low_shelf"))
        eq.add_band(EQBand(10000.0, gain_db=+1.5, q=0.707, band_type="high_shelf"))
        return eq
    def process(self, audio):
        sig = audio.astype(np.float32)
        if self._eq is not None:
            sig = self._eq.process(sig)
        if self._compressor is not None:
            sig = self._compressor.process(sig)
        if self.target_lufs is not None:
            current_lufs = _lufs_loudness(sig, self.sample_rate)
            gain_db = np.clip(self.target_lufs - current_lufs, -40.0, 20.0)
            sig = (sig.astype(np.float64) * 10.0 ** (gain_db / 20.0)).astype(np.float32)
        sig = self._limiter.process(sig)
        if self._bit_depth == 16:
            sig = dither(sig, bit_depth=16)
        return sig
    def process_and_export(self, audio, path, fmt="wav", loop_start=None, loop_end=None):
        mastered = self.process(audio)
        out_path = self._exporter.export(mastered, path, fmt=fmt)
        if loop_start is not None and fmt == "wav":
            if loop_end is None:
                raise ValueError("loop_end must be provided when loop_start is set")
            self._exporter.write_loop_points(out_path, loop_start, loop_end)
        return out_path
