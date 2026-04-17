"""Synthesizer sub-package – oscillators, envelopes, filters, effects, instruments."""

from audio_engine.synthesizer.oscillator import Oscillator
from audio_engine.synthesizer.envelope import Envelope
from audio_engine.synthesizer.filter import Filter
from audio_engine.synthesizer.effects import Effects
from audio_engine.synthesizer.instrument import Instrument, InstrumentLibrary

__all__ = [
    "Oscillator",
    "Envelope",
    "Filter",
    "Effects",
    "Instrument",
    "InstrumentLibrary",
]
