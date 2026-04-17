"""Composer sub-package – scales, chords, sequencer, and patterns."""

from audio_engine.composer.scale import Scale, ScaleLibrary
from audio_engine.composer.chord import Chord, ChordProgression
from audio_engine.composer.pattern import RhythmPattern
from audio_engine.composer.sequencer import Sequencer, Note

__all__ = [
    "Scale",
    "ScaleLibrary",
    "Chord",
    "ChordProgression",
    "RhythmPattern",
    "Sequencer",
    "Note",
]
