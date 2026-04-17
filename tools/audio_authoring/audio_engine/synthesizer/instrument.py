"""
Instrument – combines oscillator + envelope + filter into a playable voice.

TEACHING NOTE — Instrument Design Pattern
==========================================
An Instrument is a composition of:
  1. Oscillator (waveform generator)
  2. ADSR Envelope (amplitude shaper)
  3. Filter      (tone shaper / optional)

This "chain of processors" pattern appears in every synthesizer.
The Instrument is used by the Sequencer to render individual notes.

Real-world parallel: Moog Modular, Roland Juno, Native Instruments Kontakt
all use this same signal chain.  The difference is sophistication.
"""

from __future__ import annotations

import numpy as np

from audio_engine.synthesizer.oscillator import Oscillator
from audio_engine.synthesizer.envelope import ADSREnvelope
from audio_engine.synthesizer.filter import SynthFilter

__all__ = ["Instrument", "InstrumentLibrary"]


class Instrument:
    """A single synthesizer voice: oscillator → envelope → optional filter.

    Parameters
    ----------
    waveform:
        Oscillator waveform (``"sine"``, ``"square"``, ``"sawtooth"``,
        ``"triangle"``, ``"noise"``).
    attack:
        Attack time in seconds.
    decay:
        Decay time in seconds.
    sustain:
        Sustain level (0.0 – 1.0).
    release:
        Release time in seconds.
    filter_cutoff:
        Low-pass filter cutoff in Hz.  ``None`` = no filter.
    filter_mode:
        Filter mode (``"lowpass"``, ``"highpass"``, ``"bandpass"``).
    sample_rate:
        Audio sample rate in Hz.

    Example
    -------
    >>> inst = Instrument(waveform="sawtooth", attack=0.005, decay=0.2, sustain=0.6)
    >>> audio = inst.render(frequency=440.0, duration=1.0)
    """

    def __init__(
        self,
        waveform: str = "sine",
        attack: float = 0.01,
        decay: float = 0.1,
        sustain: float = 0.7,
        release: float = 0.2,
        filter_cutoff: float | None = None,
        filter_mode: str = "lowpass",
        sample_rate: int = 44100,
    ) -> None:
        self.sample_rate = sample_rate
        self._osc = Oscillator(sample_rate=sample_rate, waveform=waveform)
        self._env = ADSREnvelope(
            attack=attack,
            decay=decay,
            sustain=sustain,
            release=release,
            sample_rate=sample_rate,
        )
        self._filter: SynthFilter | None = None
        if filter_cutoff is not None:
            self._filter = SynthFilter(
                sample_rate=sample_rate,
                mode=filter_mode,  # type: ignore[arg-type]
                cutoff=filter_cutoff,
            )

    def render(self, frequency: float, duration: float, amplitude: float = 1.0) -> np.ndarray:
        """Render one note as a mono float32 buffer.

        Parameters
        ----------
        frequency:
            Note frequency in Hz.
        duration:
            Note duration in seconds.
        amplitude:
            Peak amplitude (0.0 – 1.0).

        Returns
        -------
        np.ndarray
            Mono float32 array of shape ``(N,)`` where N ≈ duration × sample_rate.
        """
        raw = self._osc.generate(frequency, duration, amplitude=1.0)
        env = self._env.render(duration)

        # Ensure same length (float rounding can differ by 1 sample)
        n = min(len(raw), len(env))
        shaped = raw[:n] * env[:n]

        if self._filter is not None:
            shaped = self._filter.process(shaped)

        return (shaped * amplitude).astype(np.float32)


class InstrumentLibrary:
    """Factory for standard game-ready instrument presets.

    TEACHING NOTE — Flyweight Pattern
    All presets are built on-demand from lightweight parameter sets.
    Each call to :meth:`get` creates a new :class:`Instrument` instance.
    This avoids shared mutable state between tracks.
    """

    _PRESETS: dict[str, dict] = {
        "piano": dict(waveform="triangle", attack=0.005, decay=0.3, sustain=0.6, release=0.4),
        "strings": dict(waveform="sawtooth", attack=0.15, decay=0.1, sustain=0.8, release=0.5, filter_cutoff=4000.0),
        "bass": dict(waveform="sawtooth", attack=0.01, decay=0.15, sustain=0.7, release=0.1, filter_cutoff=800.0),
        "lead": dict(waveform="square", attack=0.01, decay=0.05, sustain=0.8, release=0.15),
        "pad": dict(waveform="sine", attack=0.5, decay=0.2, sustain=0.9, release=1.0),
        "drum_kick": dict(waveform="sine", attack=0.001, decay=0.15, sustain=0.0, release=0.1),
        "drum_snare": dict(waveform="noise", attack=0.001, decay=0.1, sustain=0.0, release=0.05),
        "choir": dict(waveform="sine", attack=0.3, decay=0.2, sustain=0.85, release=0.6),
        "brass": dict(waveform="sawtooth", attack=0.05, decay=0.1, sustain=0.75, release=0.2),
        "flute": dict(waveform="sine", attack=0.08, decay=0.1, sustain=0.8, release=0.2, filter_cutoff=8000.0),
        "guitar": dict(waveform="triangle", attack=0.005, decay=0.3, sustain=0.4, release=0.3),
        "crystal": dict(waveform="triangle", attack=0.01, decay=0.5, sustain=0.3, release=0.8),
    }

    @classmethod
    def get(cls, name: str, sample_rate: int = 44100) -> Instrument:
        """Return a new :class:`Instrument` for preset *name*.

        Parameters
        ----------
        name:
            Preset name (e.g. ``"piano"``, ``"strings"``, ``"bass"``).
        sample_rate:
            Audio sample rate.

        Raises
        ------
        KeyError
            If *name* is not a known preset.
        """
        if name not in cls._PRESETS:
            available = ", ".join(sorted(cls._PRESETS))
            raise KeyError(f"Unknown preset '{name}'. Available: {available}")
        params = dict(cls._PRESETS[name])
        return Instrument(sample_rate=sample_rate, **params)

    @classmethod
    def available(cls) -> list[str]:
        """Return sorted list of available preset names."""
        return sorted(cls._PRESETS.keys())
