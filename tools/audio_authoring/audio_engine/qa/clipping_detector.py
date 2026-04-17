"""Clipping detector for audio QA."""
from __future__ import annotations
from dataclasses import dataclass, field
import numpy as np
__all__ = ["ClippingDetector", "ClippingReport"]

@dataclass
class ClippingReport:
    has_clipping: bool
    clipped_samples: int
    clip_ratio: float
    peak_level: float
    peak_dbfs: float
    clipped_samples_per_channel: dict = field(default_factory=dict)
    def summary(self) -> str:
        if not self.has_clipping:
            return f"OK – no clipping (peak {self.peak_dbfs:.1f} dBFS)"
        return f"CLIPPING DETECTED – {self.clipped_samples} samples ({self.clip_ratio*100:.2f}%), peak {self.peak_dbfs:.2f} dBFS"

class ClippingDetector:
    """Detect digital clipping in audio arrays."""
    def __init__(self, threshold: float = 0.999) -> None:
        if not 0.0 < threshold <= 1.0:
            raise ValueError("threshold must be in (0, 1]")
        self.threshold = threshold
    def detect(self, audio: np.ndarray) -> ClippingReport:
        sig = np.asarray(audio, dtype=np.float32)
        abs_sig = np.abs(sig)
        peak = float(np.max(abs_sig))
        peak_dbfs = 20.0 * np.log10(peak) if peak > 1e-12 else -120.0
        per_channel: dict = {}
        if sig.ndim == 2:
            total_clipped = 0
            for ch in range(sig.shape[1]):
                ch_clips = int(np.sum(abs_sig[:, ch] >= self.threshold))
                per_channel[ch] = ch_clips
                total_clipped += ch_clips
            total_samples = sig.shape[0] * sig.shape[1]
        else:
            total_clipped = int(np.sum(abs_sig >= self.threshold))
            total_samples = len(sig)
        clip_ratio = total_clipped / max(total_samples, 1)
        return ClippingReport(has_clipping=total_clipped > 0, clipped_samples=total_clipped,
                              clip_ratio=clip_ratio, peak_level=peak, peak_dbfs=peak_dbfs,
                              clipped_samples_per_channel=per_channel)
