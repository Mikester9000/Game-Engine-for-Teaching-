"""
Scale – Western and modal scale definitions.

Scales are expressed as semitone intervals relative to the root.
"""

from __future__ import annotations

__all__ = ["Scale", "ScaleLibrary"]

# Mapping of note name → MIDI number for octave 4
_NOTE_NAMES: dict[str, int] = {
    "C": 60, "C#": 61, "Db": 61, "D": 62, "D#": 63, "Eb": 63,
    "E": 64, "F": 65, "F#": 66, "Gb": 66, "G": 67, "G#": 68,
    "Ab": 68, "A": 69, "A#": 70, "Bb": 70, "B": 71,
}


def midi_to_freq(midi_note: int) -> float:
    """Convert a MIDI note number to frequency in Hz (A4 = 440 Hz)."""
    return 440.0 * 2.0 ** ((midi_note - 69) / 12.0)


class Scale:
    """A musical scale rooted on *root* using *intervals*.

    Parameters
    ----------
    root:
        Root note name (e.g. ``"A"``, ``"C#"``).
    intervals:
        Semitone offsets from the root (root = 0).
    octave:
        Octave number (4 = middle octave).
    """

    def __init__(self, root: str, intervals: list[int], octave: int = 4) -> None:
        if root not in _NOTE_NAMES:
            raise ValueError(f"Unknown note '{root}'")
        self.root = root
        self.intervals = intervals
        self.octave = octave
        self._root_midi = _NOTE_NAMES[root] + (octave - 4) * 12

    @property
    def notes(self) -> list[int]:
        """Return MIDI note numbers for one octave of the scale."""
        return [self._root_midi + i for i in self.intervals]

    @property
    def frequencies(self) -> list[float]:
        """Return frequencies (Hz) for one octave of the scale."""
        return [midi_to_freq(n) for n in self.notes]

    def degree(self, n: int) -> float:
        """Return the frequency of scale degree *n* (1-indexed, wraps octaves)."""
        octave_shift, idx = divmod(n - 1, len(self.intervals))
        midi = self._root_midi + self.intervals[idx] + octave_shift * 12
        return midi_to_freq(midi)

    def __repr__(self) -> str:  # pragma: no cover
        return f"Scale(root='{self.root}', octave={self.octave})"


class ScaleLibrary:
    """Pre-built scale factory."""

    # interval patterns
    _PATTERNS: dict[str, list[int]] = {
        "major":           [0, 2, 4, 5, 7, 9, 11],
        "natural_minor":   [0, 2, 3, 5, 7, 8, 10],
        "harmonic_minor":  [0, 2, 3, 5, 7, 8, 11],
        "melodic_minor":   [0, 2, 3, 5, 7, 9, 11],
        "pentatonic_major":[0, 2, 4, 7, 9],
        "pentatonic_minor":[0, 3, 5, 7, 10],
        "dorian":          [0, 2, 3, 5, 7, 9, 10],
        "phrygian":        [0, 1, 3, 5, 7, 8, 10],
        "lydian":          [0, 2, 4, 6, 7, 9, 11],
        "mixolydian":      [0, 2, 4, 5, 7, 9, 10],
        "locrian":         [0, 1, 3, 5, 6, 8, 10],
        "whole_tone":      [0, 2, 4, 6, 8, 10],
        "blues":           [0, 3, 5, 6, 7, 10],
        "chromatic":       list(range(12)),
    }

    @classmethod
    def get(cls, name: str, root: str = "C", octave: int = 4) -> Scale:
        """Create a named scale.

        Parameters
        ----------
        name:
            Scale type (e.g. ``"major"``, ``"harmonic_minor"``).
        root:
            Root note name.
        octave:
            Octave number (default 4).
        """
        if name not in cls._PATTERNS:
            available = ", ".join(sorted(cls._PATTERNS))
            raise KeyError(f"Unknown scale '{name}'. Available: {available}")
        return Scale(root, cls._PATTERNS[name], octave)

    @classmethod
    def available(cls) -> list[str]:
        """Return sorted list of available scale names."""
        return sorted(cls._PATTERNS.keys())
