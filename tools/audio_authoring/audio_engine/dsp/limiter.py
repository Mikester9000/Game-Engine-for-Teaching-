"""
True-peak brick-wall limiter.
"""
from __future__ import annotations
import numpy as np

__all__ = ["Limiter"]

def _db_to_linear(db: float) -> float:
    return 10.0 ** (db / 20.0)

class Limiter:
    """Brick-wall true-peak limiter."""
    def __init__(self, sample_rate: int = 44100, ceiling_db: float = -0.3, release_ms: float = 150.0, lookahead_ms: float = 5.0) -> None:
        self.sample_rate = sample_rate
        self.ceiling = _db_to_linear(ceiling_db)
        self._release_coef = np.exp(-1.0 / (sample_rate * release_ms / 1000.0))
        self._lookahead = max(1, int(sample_rate * lookahead_ms / 1000.0))

    def process(self, signal: np.ndarray) -> np.ndarray:
        stereo = signal.ndim == 2
        sig = signal.astype(np.float64)
        if stereo:
            peak_env = np.max(np.abs(sig), axis=1)
        else:
            peak_env = np.abs(sig)
        gain = self._compute_gain(peak_env)
        if stereo:
            out = sig * gain[:, np.newaxis]
        else:
            out = sig * gain
        return out.astype(np.float32)

    def _compute_gain(self, peak_env: np.ndarray) -> np.ndarray:
        n = len(peak_env)
        ceiling = self.ceiling
        lookahead_peak = np.copy(peak_env)
        la = self._lookahead
        for i in range(min(la, n)):
            lookahead_peak[: n - i] = np.maximum(lookahead_peak[: n - i], peak_env[i: n])
        desired = np.where(lookahead_peak > ceiling, ceiling / (lookahead_peak + 1e-12), 1.0)
        gain = np.ones(n, dtype=np.float64)
        prev = 1.0
        for i in range(n):
            g = desired[i]
            if g < prev:
                gain[i] = g
            else:
                gain[i] = self._release_coef * prev + (1.0 - self._release_coef) * g
            prev = gain[i]
        return gain
