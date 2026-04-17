"""
MusicGenerator – AI-assisted procedural music generation.

Uses a combination of:
  - Markov chain melody generation (trained on interval probabilities derived
    from the target musical style)
  - Rule-based harmonic accompaniment (chord progressions from the scale)
  - Style templates that define orchestration, tempo, and texture

The generated output is a fully-populated :class:`~audio_engine.composer.Sequencer`
ready to be rendered.

Supported styles
----------------
``"battle"``
    Fast, dramatic orchestral – inspired by cinematic RPG combat music.
    Tempo 140 BPM, minor key, full orchestra + percussion.

``"exploration"``
    Moderate-pace, wonder-filled – open world traversal feel.
    Tempo 90 BPM, major key, strings + choir + crystal synth.

``"ambient"``
    Slow, atmospheric – dungeon / menu music.
    Tempo 60 BPM, minor key, pad + choir + reverb.

``"boss"``
    Intense, layered – climactic encounter.
    Tempo 160 BPM, Phrygian, brass + strings + electric guitar.

``"victory"``
    Triumphant fanfare.
    Tempo 120 BPM, major key, brass + piano + strings.

``"menu"``
    Calm, introspective – main menu or loading screen.
    Tempo 80 BPM, major key, piano + strings.
"""

from __future__ import annotations

import random
from dataclasses import dataclass, field
from typing import Literal

import numpy as np

from audio_engine.composer.scale import Scale, ScaleLibrary
from audio_engine.composer.chord import ChordProgression
from audio_engine.composer.pattern import RhythmPattern
from audio_engine.composer.sequencer import Note, Sequencer
from audio_engine.synthesizer.instrument import InstrumentLibrary

__all__ = ["TrackStyle", "MusicGenerator"]

TrackStyle = Literal["battle", "exploration", "ambient", "boss", "victory", "menu"]


# ---------------------------------------------------------------------------
# Style definitions
# ---------------------------------------------------------------------------

@dataclass
class _StyleDef:
    bpm: float
    scale_name: str
    root: str
    octave: int
    progression_name: str
    instruments: list[str]           # instrument names for melody layer
    accompaniment: list[str]         # instruments for chord backing
    bass_instrument: str
    percussion_instrument: str | None
    melody_pattern: str              # name of a RhythmPattern classmethod
    chord_pattern: str
    bars: int = 8                    # number of bars to generate


_STYLE_DEFS: dict[str, _StyleDef] = {
    "battle": _StyleDef(
        bpm=140,
        scale_name="harmonic_minor",
        root="A",
        octave=4,
        progression_name="i_iv_v_i",
        instruments=["brass", "strings"],
        accompaniment=["strings", "choir"],
        bass_instrument="bass",
        percussion_instrument="percussion",
        melody_pattern="battle",
        chord_pattern="half_notes",
    ),
    "exploration": _StyleDef(
        bpm=90,
        scale_name="major",
        root="G",
        octave=4,
        progression_name="I_V_vi_IV",
        instruments=["flute", "strings"],
        accompaniment=["strings", "choir"],
        bass_instrument="bass",
        percussion_instrument=None,
        melody_pattern="eighth_notes",
        chord_pattern="half_notes",
    ),
    "ambient": _StyleDef(
        bpm=60,
        scale_name="natural_minor",
        root="E",
        octave=3,
        progression_name="i_VI_III_VII",
        instruments=["synth_pad", "choir"],
        accompaniment=["synth_pad"],
        bass_instrument="synth_pad",
        percussion_instrument=None,
        melody_pattern="ambient",
        chord_pattern="ambient",
    ),
    "boss": _StyleDef(
        bpm=160,
        scale_name="phrygian",
        root="D",
        octave=4,
        progression_name="i_bII_i_bVII",
        instruments=["brass", "electric_guitar"],
        accompaniment=["strings", "brass"],
        bass_instrument="bass",
        percussion_instrument="percussion",
        melody_pattern="battle",
        chord_pattern="four_on_the_floor",
    ),
    "victory": _StyleDef(
        bpm=120,
        scale_name="major",
        root="C",
        octave=4,
        progression_name="I_IV_V_I",
        instruments=["brass", "piano"],
        accompaniment=["strings", "choir"],
        bass_instrument="bass",
        percussion_instrument="percussion",
        melody_pattern="eight_notes",
        chord_pattern="half_notes",
        bars=4,
    ),
    "menu": _StyleDef(
        bpm=80,
        scale_name="major",
        root="F",
        octave=4,
        progression_name="I_V_vi_IV",
        instruments=["piano", "crystal_synth"],
        accompaniment=["strings", "synth_pad"],
        bass_instrument="bass",
        percussion_instrument=None,
        melody_pattern="eighth_notes",
        chord_pattern="half_notes",
    ),
}


# ---------------------------------------------------------------------------
# Markov chain melody generator
# ---------------------------------------------------------------------------

# Transition probabilities for melodic intervals (in scale degrees).
# The key is the last interval taken; value is weighted choices for next.
_MARKOV_TRANSITIONS: dict[int, list[tuple[int, float]]] = {
    0:  [(1, 0.35), (2, 0.25), (-1, 0.2), (3, 0.1), (-2, 0.1)],
    1:  [(1, 0.3), (2, 0.2), (-1, 0.25), (0, 0.15), (3, 0.1)],
    2:  [(1, 0.25), (-1, 0.3), (-2, 0.2), (2, 0.15), (0, 0.1)],
    3:  [(-1, 0.3), (-2, 0.25), (1, 0.2), (-3, 0.15), (0, 0.1)],
    -1: [(-1, 0.3), (1, 0.25), (-2, 0.2), (0, 0.15), (2, 0.1)],
    -2: [(-1, 0.35), (1, 0.25), (-3, 0.15), (0, 0.15), (2, 0.1)],
    -3: [(1, 0.35), (-1, 0.25), (2, 0.2), (-2, 0.1), (0, 0.1)],
}
_DEFAULT_TRANSITIONS: list[tuple[int, float]] = [
    (1, 0.3), (-1, 0.25), (2, 0.2), (-2, 0.15), (0, 0.1),
]


def _markov_melody(
    scale: Scale,
    num_notes: int,
    start_degree: int = 1,
    rng: random.Random | None = None,
    octave_range: int = 2,
) -> list[float]:
    """Generate a melody as a list of frequencies using a Markov chain.

    Parameters
    ----------
    scale:
        Source scale.
    num_notes:
        Number of notes to generate.
    start_degree:
        Scale degree (1-indexed) to begin on.
    rng:
        Random instance for reproducibility.
    octave_range:
        Number of octaves to span.
    """
    if rng is None:
        rng = random.Random()

    scale_size = len(scale.intervals)
    total_degrees = scale_size * octave_range

    degree = start_degree
    last_interval = 0
    melody: list[float] = []

    for _ in range(num_notes):
        melody.append(scale.degree(degree))
        transitions = _MARKOV_TRANSITIONS.get(last_interval, _DEFAULT_TRANSITIONS)
        choices, weights = zip(*transitions)
        raw = rng.choices(list(choices), list(weights))[0]
        new_degree = degree + raw
        # Clamp to valid range
        new_degree = max(1, min(new_degree, total_degrees))
        last_interval = new_degree - degree
        degree = new_degree

    return melody


# ---------------------------------------------------------------------------
# Main generator class
# ---------------------------------------------------------------------------

class MusicGenerator:
    """AI-assisted procedural music generator.

    Parameters
    ----------
    sample_rate:
        Audio sample rate in Hz.
    seed:
        Random seed for reproducibility (``None`` = non-deterministic).
    """

    def __init__(self, sample_rate: int = 44100, seed: int | None = None) -> None:
        self.sample_rate = sample_rate
        self._rng = random.Random(seed)

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def generate(
        self,
        style: TrackStyle = "battle",
        duration: float | None = None,
        bars: int | None = None,
    ) -> Sequencer:
        """Generate a complete multi-track :class:`~audio_engine.composer.Sequencer`.

        Parameters
        ----------
        style:
            Musical style preset.
        duration:
            Override total length in seconds (ignored if *bars* is set).
        bars:
            Override number of bars to generate.

        Returns
        -------
        :class:`~audio_engine.composer.Sequencer`
            Fully populated sequencer, ready to call ``.render()``.
        """
        if style not in _STYLE_DEFS:
            available = ", ".join(sorted(_STYLE_DEFS))
            raise ValueError(f"Unknown style '{style}'. Available: {available}")

        sdef = _STYLE_DEFS[style]
        effective_bars = bars if bars is not None else sdef.bars

        seq = Sequencer(bpm=sdef.bpm, sample_rate=self.sample_rate)
        scale = ScaleLibrary.get(sdef.scale_name, sdef.root, sdef.octave)

        bar_dur = seq.bar_duration

        # --- Build chord progression ---
        prog = ChordProgression(scale, sdef.progression_name)
        chords = prog.chords  # list of Chord objects (one per chord change)

        # --- Add instrument tracks ---
        chord_patt = getattr(RhythmPattern, sdef.chord_pattern, RhythmPattern.half_notes)()
        melody_patt = getattr(RhythmPattern, sdef.melody_pattern, RhythmPattern.eighth_notes)()

        for inst_name in sdef.instruments:
            inst = InstrumentLibrary.get(inst_name, self.sample_rate)
            pan = self._rng.uniform(-0.3, 0.3)
            seq.add_track(f"melody_{inst_name}", inst, pan=pan, volume=0.8)

        for inst_name in sdef.accompaniment:
            inst = InstrumentLibrary.get(inst_name, self.sample_rate)
            pan = self._rng.uniform(-0.5, 0.5)
            seq.add_track(f"chord_{inst_name}", inst, pan=pan, volume=0.6)

        bass_inst = InstrumentLibrary.get(sdef.bass_instrument, self.sample_rate)
        seq.add_track("bass", bass_inst, pan=0.0, volume=0.85)

        if sdef.percussion_instrument:
            perc_inst = InstrumentLibrary.get(sdef.percussion_instrument, self.sample_rate)
            seq.add_track("percussion", perc_inst, pan=0.0, volume=0.9)

        # --- Populate notes bar by bar ---
        for bar_idx in range(effective_bars):
            bar_onset = bar_idx * bar_dur
            chord = chords[bar_idx % len(chords)]

            # Melody layer
            mel_triggers = melody_patt.note_durations(bar_dur)
            num_mel_notes = len(mel_triggers)
            mel_freqs = _markov_melody(
                scale, num_mel_notes, start_degree=1, rng=self._rng, octave_range=2
            )

            for track_name_suffix in sdef.instruments:
                for (rel_onset, note_dur), freq in zip(mel_triggers, mel_freqs):
                    vel = self._rng.uniform(0.6, 1.0)
                    seq.add_note(
                        f"melody_{track_name_suffix}",
                        freq,
                        bar_onset + rel_onset,
                        note_dur * 0.9,
                        velocity=vel,
                    )

            # Chord layer
            chord_triggers = chord_patt.note_durations(bar_dur)
            for inst_name in sdef.accompaniment:
                for rel_onset, note_dur in chord_triggers:
                    for chord_freq in chord.frequencies:
                        # Drop chord tones by one octave for richness
                        seq.add_note(
                            f"chord_{inst_name}",
                            chord_freq * 0.5,
                            bar_onset + rel_onset,
                            note_dur * 0.85,
                            velocity=0.55,
                        )

            # Bass – root note of current chord
            bass_freq = chord.frequencies[0] * 0.25   # two octaves below
            for rel_onset, note_dur in chord_patt.note_durations(bar_dur):
                seq.add_note("bass", bass_freq, bar_onset + rel_onset, note_dur * 0.9, velocity=0.8)

            # Percussion
            if sdef.percussion_instrument:
                perc_patt = RhythmPattern.four_on_the_floor()
                for rel_onset, note_dur in perc_patt.note_durations(bar_dur):
                    # Use a fixed pitched 'hit' frequency for the drum voice
                    seq.add_note(
                        "percussion",
                        80.0,   # low thud
                        bar_onset + rel_onset,
                        min(note_dur, 0.15),
                        velocity=self._rng.uniform(0.7, 1.0),
                    )

        return seq

    # ------------------------------------------------------------------
    # Convenience: generate + render
    # ------------------------------------------------------------------

    def generate_audio(
        self,
        style: TrackStyle = "battle",
        bars: int | None = None,
    ) -> np.ndarray:
        """Generate and render audio as a stereo float32 NumPy array.

        Parameters
        ----------
        style:
            Musical style preset.
        bars:
            Number of bars (defaults to the style's default).

        Returns
        -------
        np.ndarray
            Shape ``(N, 2)`` stereo float32 array.
        """
        seq = self.generate(style=style, bars=bars)
        return seq.render()

    @staticmethod
    def available_styles() -> list[str]:
        """Return sorted list of available style names."""
        return sorted(_STYLE_DEFS.keys())
