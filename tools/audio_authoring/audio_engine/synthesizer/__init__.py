"""Synthesizer sub-package – oscillators, envelopes, filters, effects, instruments."""

from audio_engine.synthesizer.oscillator import Oscillator
from audio_engine.synthesizer.envelope import ADSREnvelope as Envelope
from audio_engine.synthesizer.filter import SynthFilter as Filter
from audio_engine.synthesizer.effects import chorus, simple_delay, soft_clip
from audio_engine.synthesizer.instrument import Instrument, InstrumentLibrary

# Re-export Effects as a convenience namespace
class Effects:
    """Namespace for synthesizer effects functions."""
    chorus = staticmethod(chorus)
    simple_delay = staticmethod(simple_delay)
    soft_clip = staticmethod(soft_clip)

__all__ = [
    "Oscillator",
    "Envelope",
    "Filter",
    "Effects",
    "Instrument",
    "InstrumentLibrary",
]
