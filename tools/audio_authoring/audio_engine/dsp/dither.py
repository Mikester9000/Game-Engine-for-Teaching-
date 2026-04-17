"""Noise dithering for quantisation error shaping."""
from __future__ import annotations
from typing import Literal
import numpy as np
__all__ = ["dither"]
DitherType = Literal["tpdf", "rpdf", "none"]

def dither(signal, bit_depth=16, dither_type="tpdf", seed=None):
    """Apply noise dithering before bit-depth quantisation."""
    if dither_type == "none":
        return signal.copy()
    rng = np.random.default_rng(seed)
    lsb = 2.0 / (2 ** bit_depth)
    if dither_type == "tpdf":
        noise = (rng.random(signal.shape) - 0.5) + (rng.random(signal.shape) - 0.5)
    elif dither_type == "rpdf":
        noise = rng.random(signal.shape) - 0.5
    else:
        raise ValueError(f"Unknown dither_type '{dither_type}'")
    return (signal.astype(np.float64) + noise * lsb).astype(np.float32)
