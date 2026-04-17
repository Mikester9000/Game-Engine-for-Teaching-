"""Sample-rate conversion utility."""
from __future__ import annotations
import numpy as np
from scipy.signal import resample_poly
from math import gcd
__all__ = ["resample"]

def resample(signal: np.ndarray, orig_sr: int, target_sr: int) -> np.ndarray:
    if orig_sr == target_sr:
        return signal.copy()
    common = gcd(orig_sr, target_sr)
    up = target_sr // common
    down = orig_sr // common
    stereo = signal.ndim == 2
    sig_f64 = signal.astype(np.float64)
    if stereo:
        left = resample_poly(sig_f64[:, 0], up, down)
        right = resample_poly(sig_f64[:, 1], up, down)
        result = np.stack([left, right], axis=1)
    else:
        result = resample_poly(sig_f64, up, down)
    return result.astype(np.float32)
