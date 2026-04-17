"""Dynamic range compressor with attack/release envelope."""
from __future__ import annotations
import numpy as np
__all__ = ["Compressor"]

def _db_to_linear(db): return 10.0 ** (db / 20.0)
def _linear_to_db(linear): return 20.0 * np.log10(np.maximum(np.abs(linear), 1e-9))

class Compressor:
    """Feed-forward dynamic range compressor."""
    def __init__(self, sample_rate=44100, threshold_db=-18.0, ratio=4.0, attack_ms=10.0,
                 release_ms=100.0, knee_db=6.0, makeup_gain_db=0.0):
        self.sample_rate = sample_rate
        self.threshold_db = threshold_db
        self.ratio = ratio
        self.knee_db = knee_db
        self.makeup_gain_db = makeup_gain_db
        self._attack_coef = np.exp(-1.0 / (sample_rate * attack_ms / 1000.0))
        self._release_coef = np.exp(-1.0 / (sample_rate * release_ms / 1000.0))
        self._makeup_linear = _db_to_linear(makeup_gain_db)

    def process(self, signal):
        stereo = signal.ndim == 2
        sig = signal.astype(np.float64)
        if stereo:
            sidechain = np.mean(np.abs(sig), axis=1)
            gain = self._compute_gain(sidechain)
            out = sig * gain[:, np.newaxis] * self._makeup_linear
        else:
            gain = self._compute_gain(np.abs(sig))
            out = sig * gain * self._makeup_linear
        return out.astype(np.float32)

    def _compute_gain(self, envelope_input):
        n = len(envelope_input)
        level_db = _linear_to_db(envelope_input)
        half_knee = self.knee_db / 2.0
        gain_reduction_db = np.zeros(n, dtype=np.float64)
        for i in range(n):
            x = level_db[i]; overshoot = x - self.threshold_db
            if self.knee_db > 0 and overshoot > -half_knee and overshoot < half_knee:
                t = (overshoot + half_knee) / self.knee_db
                eff_ratio = 1.0 + (self.ratio - 1.0) * t
                gain_reduction_db[i] = overshoot * (1.0 - 1.0 / eff_ratio) * t
            elif overshoot > half_knee:
                gain_reduction_db[i] = overshoot * (1.0 - 1.0 / self.ratio)
        desired_gain = _db_to_linear(-gain_reduction_db)
        smoothed_gain = np.ones(n, dtype=np.float64)
        prev = 1.0
        for i in range(n):
            g = desired_gain[i]
            coef = self._attack_coef if g < prev else self._release_coef
            smoothed_gain[i] = coef * prev + (1.0 - coef) * g
            prev = smoothed_gain[i]
        return smoothed_gain
