"""
Parametric equalizer with shelf and peak filters.

Implements a cascade of second-order IIR (biquad) filter sections – the
industry-standard approach for parametric EQ. Each band can be independently
configured as a low-shelf, high-shelf, or peaking bell filter.

Reference: Audio EQ Cookbook by Robert Bristow-Johnson.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Literal

import numpy as np
from scipy.signal import sosfilt, sosfiltfilt  # type: ignore[import]

__all__ = ["EQBand", "EQ"]

BandType = Literal["low_shelf", "high_shelf", "peak", "low_pass", "high_pass"]


@dataclass
class EQBand:
    """A single EQ filter band."""

    freq: float
    gain_db: float = 0.0
    q: float = 0.707
    band_type: BandType = "peak"


def _biquad_coeffs(band: EQBand, sample_rate: int) -> np.ndarray:
    """Compute second-order section (SOS) coefficients for a single biquad band."""
    w0 = 2.0 * np.pi * band.freq / sample_rate
    cos_w0 = np.cos(w0)
    sin_w0 = np.sin(w0)
    alpha = sin_w0 / (2.0 * band.q)
    A = 10.0 ** (band.gain_db / 40.0)

    if band.band_type == "peak":
        b0 = 1.0 + alpha * A
        b1 = -2.0 * cos_w0
        b2 = 1.0 - alpha * A
        a0 = 1.0 + alpha / A
        a1 = -2.0 * cos_w0
        a2 = 1.0 - alpha / A
    elif band.band_type == "low_shelf":
        b0 = A * ((A + 1) - (A - 1) * cos_w0 + 2 * np.sqrt(A) * alpha)
        b1 = 2.0 * A * ((A - 1) - (A + 1) * cos_w0)
        b2 = A * ((A + 1) - (A - 1) * cos_w0 - 2 * np.sqrt(A) * alpha)
        a0 = (A + 1) + (A - 1) * cos_w0 + 2 * np.sqrt(A) * alpha
        a1 = -2.0 * ((A - 1) + (A + 1) * cos_w0)
        a2 = (A + 1) + (A - 1) * cos_w0 - 2 * np.sqrt(A) * alpha
    elif band.band_type == "high_shelf":
        b0 = A * ((A + 1) + (A - 1) * cos_w0 + 2 * np.sqrt(A) * alpha)
        b1 = -2.0 * A * ((A - 1) + (A + 1) * cos_w0)
        b2 = A * ((A + 1) + (A - 1) * cos_w0 - 2 * np.sqrt(A) * alpha)
        a0 = (A + 1) - (A - 1) * cos_w0 + 2 * np.sqrt(A) * alpha
        a1 = 2.0 * ((A - 1) - (A + 1) * cos_w0)
        a2 = (A + 1) - (A - 1) * cos_w0 - 2 * np.sqrt(A) * alpha
    elif band.band_type == "low_pass":
        b0 = (1.0 - cos_w0) / 2.0
        b1 = 1.0 - cos_w0
        b2 = (1.0 - cos_w0) / 2.0
        a0 = 1.0 + alpha
        a1 = -2.0 * cos_w0
        a2 = 1.0 - alpha
    elif band.band_type == "high_pass":
        b0 = (1.0 + cos_w0) / 2.0
        b1 = -(1.0 + cos_w0)
        b2 = (1.0 + cos_w0) / 2.0
        a0 = 1.0 + alpha
        a1 = -2.0 * cos_w0
        a2 = 1.0 - alpha
    else:
        raise ValueError(f"Unknown band_type '{band.band_type}'")

    return np.array([[b0 / a0, b1 / a0, b2 / a0, 1.0, a1 / a0, a2 / a0]])


class EQ:
    """Multi-band parametric equalizer."""

    def __init__(self, sample_rate: int = 44100, bands: list[EQBand] | None = None, zero_phase: bool = False) -> None:
        self.sample_rate = sample_rate
        self.zero_phase = zero_phase
        self._bands: list[EQBand] = list(bands) if bands else []

    def add_band(self, band: EQBand) -> "EQ":
        self._bands.append(band)
        return self

    def process(self, signal: np.ndarray) -> np.ndarray:
        if not self._bands:
            return signal.copy()
        sos = np.vstack([_biquad_coeffs(b, self.sample_rate) for b in self._bands])
        stereo = signal.ndim == 2
        sig_f64 = signal.astype(np.float64)
        filt_fn = sosfiltfilt if self.zero_phase else sosfilt
        if stereo:
            left = filt_fn(sos, sig_f64[:, 0])
            right = filt_fn(sos, sig_f64[:, 1])
            result = np.stack([left, right], axis=1)
        else:
            result = filt_fn(sos, sig_f64)
        return result.astype(np.float32)
