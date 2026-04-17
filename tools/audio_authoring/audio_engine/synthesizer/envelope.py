"""
ADSR Envelope – Attack / Decay / Sustain / Release amplitude shaper.

TEACHING NOTE — ADSR Envelope
================================
Every synthesizer uses an envelope to shape how a note's volume evolves
over time.  The ADSR envelope has four stages:

  Attack  — time to ramp from 0 → peak amplitude when note is pressed
  Decay   — time to fall from peak → sustain level
  Sustain — level held while the note is held (0.0 – 1.0)
  Release — time to fall from sustain → 0 when note is released

This is the same model used in hardware synths (Moog, Roland Juno) and
every software synthesizer (Logic Pro's ES2, Ableton's Analog, etc.).
"""

from __future__ import annotations

import numpy as np

__all__ = ["ADSREnvelope"]


class ADSREnvelope:
    """ADSR amplitude envelope generator.

    Parameters
    ----------
    attack:
        Attack time in seconds.
    decay:
        Decay time in seconds.
    sustain:
        Sustain level (0.0 – 1.0).
    release:
        Release time in seconds.
    sample_rate:
        Audio sample rate in Hz.

    Example
    -------
    >>> env = ADSREnvelope(attack=0.01, decay=0.1, sustain=0.7, release=0.2)
    >>> curve = env.render(duration=1.0)  # float32 array of shape (44100,)
    """

    def __init__(
        self,
        attack: float = 0.01,
        decay: float = 0.1,
        sustain: float = 0.7,
        release: float = 0.2,
        sample_rate: int = 44100,
    ) -> None:
        self.attack = max(0.0, attack)
        self.decay = max(0.0, decay)
        self.sustain = float(np.clip(sustain, 0.0, 1.0))
        self.release = max(0.0, release)
        self.sample_rate = sample_rate

    def render(self, duration: float) -> np.ndarray:
        """Compute the ADSR envelope for a note of *duration* seconds.

        The note-off (release) is assumed to happen at
        ``duration - release`` seconds.  If the total duration is shorter
        than attack + decay, stages are truncated.

        Returns
        -------
        np.ndarray
            Mono float32 amplitude curve in the range [0.0, 1.0].
        """
        n = max(1, int(duration * self.sample_rate))
        env = np.zeros(n, dtype=np.float64)

        a_samples = int(self.attack * self.sample_rate)
        d_samples = int(self.decay * self.sample_rate)
        r_samples = int(self.release * self.sample_rate)

        # Note-off is at: total - release
        note_off = max(0, n - r_samples)

        # Attack
        a_end = min(a_samples, note_off)
        if a_end > 0:
            env[:a_end] = np.linspace(0.0, 1.0, a_end)

        # Decay
        d_start = a_end
        d_end = min(d_start + d_samples, note_off)
        if d_end > d_start:
            env[d_start:d_end] = np.linspace(1.0, self.sustain, d_end - d_start)

        # Sustain
        s_start = d_end
        if s_start < note_off:
            env[s_start:note_off] = self.sustain

        # Release
        if note_off < n:
            start_level = env[note_off] if note_off > 0 else self.sustain
            env[note_off:] = np.linspace(start_level, 0.0, n - note_off)

        return env.astype(np.float32)
