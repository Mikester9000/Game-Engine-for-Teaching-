"""
Quality Assurance (QA) module for audio assets.

Provides automated analysis tools to validate exported audio files and
in-memory arrays against industry standards:

- :class:`LoudnessMeter` – EBU R128 / ITU-R BS.1770 integrated loudness
- :class:`ClippingDetector` – detect digital over-threshold clipping events
- :class:`LoopAnalyzer` – detect click/discontinuity at loop boundaries

These checks are designed to be run as a final gate before committing audio
assets to a game project, ensuring consistent loudness, clean exports, and
seamless looping.

Usage
-----
>>> from audio_engine.qa import LoudnessMeter, ClippingDetector, LoopAnalyzer
>>> meter = LoudnessMeter(sample_rate=44100)
>>> lufs = meter.integrated_loudness(audio)
>>> clips = ClippingDetector().detect(audio)
>>> loop_ok = LoopAnalyzer(sample_rate=44100).is_seamless(audio)
"""

from audio_engine.qa.loudness_meter import LoudnessMeter
from audio_engine.qa.clipping_detector import ClippingDetector
from audio_engine.qa.loop_analyzer import LoopAnalyzer

__all__ = ["LoudnessMeter", "ClippingDetector", "LoopAnalyzer"]
