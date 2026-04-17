"""
RhythmPattern – discrete beat / step patterns for the sequencer.

Patterns are represented as lists of beat fractions at which a note should
be triggered within one bar.  A beat fraction of 0.0 is the downbeat; 1.0
is one bar later.
"""

from __future__ import annotations

__all__ = ["RhythmPattern"]


class RhythmPattern:
    """A rhythmic trigger pattern for one bar.

    Parameters
    ----------
    beats:
        List of beat positions (0–1 exclusive) within a bar.  Values outside
        [0, 1) are silently clamped.
    time_signature:
        Number of beats per bar (default 4 for 4/4 time).
    """

    def __init__(self, beats: list[float], time_signature: int = 4) -> None:
        self.time_signature = max(time_signature, 1)
        self.beats = sorted(set(max(0.0, min(b, 1.0 - 1e-9)) for b in beats))

    # ------------------------------------------------------------------
    # Queries
    # ------------------------------------------------------------------

    def note_durations(self, bar_duration: float) -> list[tuple[float, float]]:
        """Return list of ``(onset_seconds, duration_seconds)`` within one bar.

        Each note's duration extends to the next onset (or end of bar).

        Parameters
        ----------
        bar_duration:
            Total bar length in seconds.
        """
        if not self.beats:
            return []
        result: list[tuple[float, float]] = []
        onsets = [b * bar_duration for b in self.beats]
        for i, onset in enumerate(onsets):
            if i + 1 < len(onsets):
                duration = onsets[i + 1] - onset
            else:
                duration = bar_duration - onset
            result.append((onset, max(duration, 0.01)))
        return result

    # ------------------------------------------------------------------
    # Built-in patterns
    # ------------------------------------------------------------------

    @classmethod
    def four_on_the_floor(cls) -> "RhythmPattern":
        """Classic 4-beat kick pattern."""
        return cls([0.0, 0.25, 0.5, 0.75])

    @classmethod
    def waltz(cls) -> "RhythmPattern":
        """3/4 waltz pattern."""
        return cls([0.0, 1 / 3, 2 / 3], time_signature=3)

    @classmethod
    def syncopated(cls) -> "RhythmPattern":
        """Off-beat syncopated pattern."""
        return cls([0.0, 0.125, 0.375, 0.5, 0.625, 0.875])

    @classmethod
    def half_notes(cls) -> "RhythmPattern":
        """Two half-note hits."""
        return cls([0.0, 0.5])

    @classmethod
    def eighth_notes(cls) -> "RhythmPattern":
        """Eight even eighth-note hits."""
        return cls([i / 8 for i in range(8)])

    @classmethod
    def triplet(cls) -> "RhythmPattern":
        """Triplet feel."""
        return cls([0.0, 1 / 3, 2 / 3])

    @classmethod
    def battle(cls) -> "RhythmPattern":
        """Energetic battle music 16th-note subdivision."""
        return cls([0.0, 0.0625, 0.25, 0.3125, 0.5, 0.5625, 0.75, 0.8125])

    @classmethod
    def ambient(cls) -> "RhythmPattern":
        """Sparse ambient pattern – whole-note."""
        return cls([0.0])
