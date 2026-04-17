"""Loop analyser – detect discontinuities at audio loop points."""
from __future__ import annotations
from dataclasses import dataclass
import numpy as np
__all__ = ["LoopAnalyzer", "LoopReport"]

@dataclass
class LoopReport:
    is_seamless: bool
    amplitude_jump: float
    amplitude_jump_db: float
    start_near_zero: bool
    end_near_zero: bool
    boundary_correlation: float
    def summary(self) -> str:
        status = "SEAMLESS" if self.is_seamless else "CLICK DETECTED"
        return f"{status} – jump {self.amplitude_jump_db:.1f} dB, correlation {self.boundary_correlation:.3f}"

class LoopAnalyzer:
    """Analyse audio for loop-boundary click artefacts."""
    def __init__(self, sample_rate=44100, window_ms=10.0, jump_threshold=0.05, zero_cross_window_ms=2.0):
        self.sample_rate = sample_rate
        self.window_samples = max(1, int(sample_rate * window_ms / 1000.0))
        self.jump_threshold = jump_threshold
        self._zc_window = max(1, int(sample_rate * zero_cross_window_ms / 1000.0))

    def analyze(self, audio: np.ndarray, loop_start=None, loop_end=None) -> LoopReport:
        mono = np.mean(audio, axis=1).astype(np.float64) if audio.ndim == 2 else audio.astype(np.float64)
        n = len(mono)
        loop_start = int(np.clip(loop_start or 0, 0, n - 1))
        loop_end = int(np.clip(loop_end if loop_end is not None else n - 1, 0, n - 1))
        amplitude_jump = float(abs(mono[loop_end] - mono[loop_start]))
        amplitude_jump_db = 20.0 * np.log10(amplitude_jump) if amplitude_jump > 1e-9 else -120.0
        start_near_zero = self._near_zero(mono, loop_start)
        end_near_zero = self._near_zero(mono, loop_end)
        w = self.window_samples
        pre_win = mono[max(0, loop_end - w): loop_end + 1]
        post_win = mono[loop_start: min(n, loop_start + w)]
        corr = self._window_correlation(pre_win, post_win)
        is_seamless = amplitude_jump < self.jump_threshold and corr > 0.7
        return LoopReport(is_seamless=is_seamless, amplitude_jump=amplitude_jump,
                          amplitude_jump_db=amplitude_jump_db, start_near_zero=start_near_zero,
                          end_near_zero=end_near_zero, boundary_correlation=corr)

    def _near_zero(self, signal, idx):
        w = self._zc_window
        region = signal[max(0, idx - w): min(len(signal), idx + w + 1)]
        return len(region) >= 2 and bool(np.any(region[:-1] * region[1:] <= 0.0))

    @staticmethod
    def _window_correlation(a, b):
        n = min(len(a), len(b))
        if n < 2:
            return 0.0
        a, b = a[:n] - np.mean(a[:n]), b[:n] - np.mean(b[:n])
        norm = np.sqrt(np.sum(a ** 2) * np.sum(b ** 2))
        return float(np.dot(a, b) / norm) if norm > 1e-12 else 0.0
