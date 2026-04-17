"""
Chord – build chords and common chord progressions.
"""

from __future__ import annotations

from audio_engine.composer.scale import Scale, ScaleLibrary, midi_to_freq

__all__ = ["Chord", "ChordProgression"]


# Chord quality → intervals in semitones
_CHORD_QUALITIES: dict[str, list[int]] = {
    "major":      [0, 4, 7],
    "minor":      [0, 3, 7],
    "diminished": [0, 3, 6],
    "augmented":  [0, 4, 8],
    "sus2":       [0, 2, 7],
    "sus4":       [0, 5, 7],
    "major7":     [0, 4, 7, 11],
    "minor7":     [0, 3, 7, 10],
    "dominant7":  [0, 4, 7, 10],
    "major9":     [0, 4, 7, 11, 14],
    "minor9":     [0, 3, 7, 10, 14],
    "add9":       [0, 4, 7, 14],
}

# Named chord progressions: list of (scale_degree, quality)
_PROGRESSIONS: dict[str, list[tuple[int, str]]] = {
    # Classic
    "I_IV_V_I":        [(1, "major"), (4, "major"), (5, "major"), (1, "major")],
    "I_V_vi_IV":       [(1, "major"), (5, "major"), (6, "minor"), (4, "major")],
    "ii_V_I":          [(2, "minor"), (5, "major"), (1, "major")],
    # Minor / cinematic
    "i_bVII_bVI_bVII": [(1, "minor"), (7, "major"), (6, "major"), (7, "major")],
    "i_iv_v_i":        [(1, "minor"), (4, "minor"), (5, "minor"), (1, "minor")],
    "i_VI_III_VII":    [(1, "minor"), (6, "major"), (3, "major"), (7, "major")],
    # Jazz
    "ii_V_I_VI":       [(2, "minor7"), (5, "dominant7"), (1, "major7"), (6, "dominant7")],
    # Epic / battle
    "i_bII_i_bVII":    [(1, "minor"), (2, "major"), (1, "minor"), (7, "major")],
    "i_v_bVI_bVII":    [(1, "minor"), (5, "minor"), (6, "major"), (7, "major")],
}


class Chord:
    """A set of simultaneous notes forming a chord.

    Parameters
    ----------
    root_midi:
        MIDI number of the chord root.
    quality:
        Chord quality string (e.g. ``"major"``, ``"minor7"``).
    """

    def __init__(self, root_midi: int, quality: str = "major") -> None:
        if quality not in _CHORD_QUALITIES:
            available = ", ".join(sorted(_CHORD_QUALITIES))
            raise ValueError(f"Unknown chord quality '{quality}'. Available: {available}")
        self.root_midi = root_midi
        self.quality = quality
        self.intervals = _CHORD_QUALITIES[quality]

    @property
    def midi_notes(self) -> list[int]:
        """MIDI note numbers for each chord tone."""
        return [self.root_midi + i for i in self.intervals]

    @property
    def frequencies(self) -> list[float]:
        """Frequencies in Hz for each chord tone."""
        return [midi_to_freq(n) for n in self.midi_notes]

    def __repr__(self) -> str:  # pragma: no cover
        return f"Chord(root={self.root_midi}, quality='{self.quality}')"


class ChordProgression:
    """A sequence of chords derived from a scale.

    Parameters
    ----------
    scale:
        The scale to build chords from.
    progression_name:
        One of the built-in named progressions, or supply *chords* directly.
    chords:
        Explicit list of ``(scale_degree, quality)`` pairs, overrides
        *progression_name*.
    """

    def __init__(
        self,
        scale: Scale,
        progression_name: str | None = None,
        chords: list[tuple[int, str]] | None = None,
    ) -> None:
        self.scale = scale
        if chords is not None:
            self._chord_defs = chords
        elif progression_name is not None:
            if progression_name not in _PROGRESSIONS:
                available = ", ".join(sorted(_PROGRESSIONS))
                raise KeyError(
                    f"Unknown progression '{progression_name}'. Available: {available}"
                )
            self._chord_defs = _PROGRESSIONS[progression_name]
        else:
            raise ValueError("Supply either 'progression_name' or 'chords'.")

    @property
    def chords(self) -> list[Chord]:
        """Return the list of :class:`Chord` objects."""
        result: list[Chord] = []
        for degree, quality in self._chord_defs:
            # Use the scale degree frequency; convert back to the nearest MIDI
            freq = self.scale.degree(degree)
            # Approximate MIDI from freq
            import math
            midi = round(69 + 12 * math.log2(freq / 440.0))
            result.append(Chord(midi, quality))
        return result

    @classmethod
    def available(cls) -> list[str]:
        """Return sorted list of built-in progression names."""
        return sorted(_PROGRESSIONS.keys())
