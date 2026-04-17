"""
DSP (Digital Signal Processing) post-processing utilities.

Provides production-quality audio processing building blocks:
- Parametric EQ
- Dynamic range compressor
- True-peak limiter
- Convolution reverb
- Sample-rate conversion (resampling)
- Noise dithering

These processors all operate on NumPy float32 arrays and are designed to be
chained together to form a flexible effects/mastering pipeline.

Example usage
-------------
>>> from audio_engine.dsp import EQ, Compressor, Limiter
>>> eq = EQ(sample_rate=44100)
>>> compressed = Compressor(sample_rate=44100).process(audio)
>>> limited = Limiter(sample_rate=44100).process(compressed)
"""

from audio_engine.dsp.eq import EQ
from audio_engine.dsp.compressor import Compressor
from audio_engine.dsp.limiter import Limiter
from audio_engine.dsp.reverb import ConvolutionReverb
from audio_engine.dsp.resample import resample
from audio_engine.dsp.dither import dither

__all__ = [
    "EQ",
    "Compressor",
    "Limiter",
    "ConvolutionReverb",
    "resample",
    "dither",
]
