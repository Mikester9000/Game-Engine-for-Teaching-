"""
High-level sound effect (SFX) generation interface.

:class:`SFXGen` generates sound effects from text prompts or structured
parameters and exports them to audio files.

Usage
-----
>>> from audio_engine.ai import SFXGen
>>> gen = SFXGen(sample_rate=44100)
>>> audio = gen.generate("sharp explosion impact")
>>> gen.generate_to_file("laser zap 0.3 seconds", "laser.wav")
"""

from __future__ import annotations

from pathlib import Path
from typing import Literal

import numpy as np

from audio_engine.ai.backend import InferenceBackend, BackendRegistry
from audio_engine.ai.prompt import PromptParser, SFXPlan
from audio_engine.export.audio_exporter import AudioExporter

__all__ = ["SFXGen"]


class SFXGen:
    """High-level SFX generation with prompt parsing and export.

    Parameters
    ----------
    sample_rate:
        Audio sample rate in Hz.
    backend:
        Backend name or :class:`~audio_engine.ai.backend.InferenceBackend`
        instance.  Defaults to ``"procedural"``.
    seed:
        Random seed for reproducibility.

    Example
    -------
    >>> gen = SFXGen(sample_rate=44100, seed=0)
    >>> audio = gen.generate("coin collect pickup")
    """

    def __init__(
        self,
        sample_rate: int = 44100,
        backend: str | InferenceBackend = "procedural",
        seed: int | None = None,
    ) -> None:
        self.sample_rate = sample_rate
        self._parser = PromptParser()
        self._exporter = AudioExporter(sample_rate=sample_rate)

        if isinstance(backend, str):
            self._backend = BackendRegistry.get(backend, sample_rate=sample_rate, seed=seed)
        else:
            self._backend = backend

    def generate(
        self,
        prompt: str,
        duration: float = 1.0,
        pitch_hz: float | None = None,
    ) -> np.ndarray:
        """Generate a sound effect from a text prompt.

        Parameters
        ----------
        prompt:
            Natural-language description, e.g. ``"whoosh impact 0.5s"``.
        duration:
            Default duration in seconds (overridden by prompt if detected).
        pitch_hz:
            Optional base pitch override in Hz.

        Returns
        -------
        np.ndarray
            Mono float32 audio array.
        """
        plan = self._parser.parse_sfx(prompt, duration=duration)
        if pitch_hz is not None:
            plan.pitch_hz = pitch_hz
        return self._generate_from_plan(plan)

    def generate_from_plan(self, plan: SFXPlan) -> np.ndarray:
        """Generate SFX from a pre-built :class:`~audio_engine.ai.prompt.SFXPlan`.

        Parameters
        ----------
        plan:
            SFX generation plan.

        Returns
        -------
        np.ndarray
            Mono float32 audio array.
        """
        return self._generate_from_plan(plan)

    def generate_to_file(
        self,
        prompt: str,
        output_path: str | Path,
        duration: float = 1.0,
        pitch_hz: float | None = None,
    ) -> Path:
        """Generate SFX and save to *output_path*.

        Parameters
        ----------
        prompt:
            Natural-language description.
        output_path:
            Output WAV file path.
        duration:
            Default duration in seconds.
        pitch_hz:
            Optional base pitch override in Hz.

        Returns
        -------
        Path
            Written file path.
        """
        audio = self.generate(prompt, duration=duration, pitch_hz=pitch_hz)
        return self._exporter.export(audio, output_path, fmt="wav")

    def _generate_from_plan(self, plan: SFXPlan) -> np.ndarray:
        return self._backend.generate_sfx_audio(
            sfx_type=plan.sfx_type,
            duration=plan.duration,
            pitch_hz=plan.pitch_hz,
        )
