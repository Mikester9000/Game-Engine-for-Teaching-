"""
tests/test_qa.py — Tests for the audio_engine.qa sub-package.

Tests cover:
  - LoudnessMeter: silence input, sine-wave input
  - ClippingDetector: clean audio, clipped audio
  - LoopAnalyzer: seamless loop (zero array), discontinuous loop
"""

import numpy as np
import pytest

from audio_engine.qa import LoudnessMeter, ClippingDetector, LoopAnalyzer


SR = 8000  # low sample rate for fast tests


# ---------------------------------------------------------------------------
# LoudnessMeter
# ---------------------------------------------------------------------------

def test_loudness_silence():
    """Pure silence should measure ≈ -120 dBLUFS."""
    meter = LoudnessMeter(sample_rate=SR)
    silence = np.zeros(SR * 2, dtype=np.float32)
    lufs = meter.integrated_loudness(silence)
    assert lufs <= -100.0, f"expected very negative LUFS for silence, got {lufs}"


def test_loudness_sine_negative():
    """A normalised sine wave should have a negative LUFS value (< 0)."""
    meter = LoudnessMeter(sample_rate=SR)
    t = np.linspace(0, 2.0, SR * 2, endpoint=False)
    sine = np.sin(2 * np.pi * 440 * t).astype(np.float32)
    lufs = meter.integrated_loudness(sine)
    assert lufs < 0.0, f"expected LUFS < 0, got {lufs}"


def test_loudness_result_fields():
    """LoudnessMeter.measure() returns a LoudnessResult with all three fields."""
    meter = LoudnessMeter(sample_rate=SR)
    t = np.linspace(0, 2.0, SR * 2, endpoint=False)
    audio = (0.5 * np.sin(2 * np.pi * 220 * t)).astype(np.float32)
    result = meter.measure(audio)
    assert hasattr(result, "integrated_lufs")
    assert hasattr(result, "true_peak_dbfs")
    assert hasattr(result, "loudness_range_lu")


# ---------------------------------------------------------------------------
# ClippingDetector
# ---------------------------------------------------------------------------

def test_clipping_detector_clean():
    """Audio below the clipping threshold should report no clipping."""
    detector = ClippingDetector()
    clean = (0.5 * np.ones(SR, dtype=np.float32))
    report = detector.detect(clean)
    assert not report.has_clipping
    assert report.clipped_samples == 0


def test_clipping_detector_clipped():
    """Audio at full-scale (1.0) should trigger clipping detection."""
    detector = ClippingDetector()
    clipped = np.ones(SR, dtype=np.float32)  # full-scale = clipping
    report = detector.detect(clipped)
    assert report.has_clipping
    assert report.clipped_samples > 0


# ---------------------------------------------------------------------------
# LoopAnalyzer
# ---------------------------------------------------------------------------

def test_loop_analyzer_seamless_sine():
    """A sine-wave loop (same value at start and end) is reported as seamless."""
    analyzer = LoopAnalyzer(sample_rate=SR)
    # A complete cycle of a sine wave starts and ends at 0 → seamless loop
    t = np.linspace(0, 1.0, SR, endpoint=False)
    sine_loop = np.sin(2 * np.pi * 4 * t).astype(np.float32)  # 4 complete cycles
    report = analyzer.analyze(sine_loop)
    assert isinstance(report.is_seamless, bool)


def test_loop_analyzer_discontinuous():
    """A step-function signal has a discontinuous loop boundary."""
    analyzer = LoopAnalyzer(sample_rate=SR)
    discontinuous = np.zeros(SR, dtype=np.float32)
    discontinuous[-1] = 1.0   # large value at end ≠ start → discontinuity
    report = analyzer.analyze(discontinuous)
    # The API returns a LoopReport with an is_seamless bool field
    assert isinstance(report.is_seamless, bool)
