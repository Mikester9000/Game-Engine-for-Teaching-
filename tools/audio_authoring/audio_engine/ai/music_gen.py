"""
High-level music generation interface.

:class:`MusicGen` wraps an :class:`~audio_engine.ai.backend.InferenceBackend`
and exposes a simple API for generating music tracks from text prompts or
style presets.  It handles prompt parsing, generation, optional mastering,
and file export.

Usage
-----
>>> from audio_engine.ai import MusicGen
>>> gen = MusicGen(sample_rate=44100)
>>> audio = gen.generate("epic orchestral battle, 140 BPM, loopable")
>>> gen.generate_to_file("battle theme", "battle.wav", duration=60.0, loopable=True)
"""

from __future__ import annotations

from pathlib import Path
from typing import Literal

import numpy as np

from audio_engine.ai.backend import InferenceBackend, BackendRegistry
from audio_engine.ai.prompt import PromptParser, MusicPlan
from audio_engine.export.audio_exporter import AudioExporter

__all__ = ["MusicGen"]


class MusicGen:
    """High-level music generation with prompt parsing and export.

    Parameters
    ----------
    sample_rate:
        Audio sample rate in Hz.
    backend:
        Backend name or :class:`~audio_engine.ai.backend.InferenceBackend`
        instance.  Defaults to ``"procedural"`` (built-in synthesiser).
    seed:
        Random seed for reproducibility.
    apply_mastering:
        If ``True``, run the generated audio through the
        :class:`~audio_engine.render.OfflineBounce` mastering pipeline
        before returning/exporting.

    Example
    -------
    >>> gen = MusicGen(sample_rate=44100, seed=42)
    >>> audio = gen.generate("calm ambient exploration 90 BPM")
    """

    def __init__(
        self,
        sample_rate: int = 44100,
        backend: str | InferenceBackend = "procedural",
        seed: int | None = None,
        apply_mastering: bool = True,
    ) -> None:
        self.sample_rate = sample_rate
        self.apply_mastering = apply_mastering
        self._parser = PromptParser()
        self._exporter = AudioExporter(sample_rate=sample_rate)

        if isinstance(backend, str):
            self._backend = BackendRegistry.get(backend, sample_rate=sample_rate, seed=seed)
        else:
            self._backend = backend

    def generate(
        self,
        prompt: str,
        duration: float = 30.0,
        loopable: bool = False,
    ) -> np.ndarray:
        """Generate a music track from a text prompt.

        Parameters
        ----------
        prompt:
            Natural-language description, e.g.
            ``"dark orchestral boss theme 160 BPM"``.
        duration:
            Target duration in seconds.
        loopable:
            If ``True``, attempt to make the output loop seamlessly.

        Returns
        -------
        np.ndarray
            Stereo float32 array ``(N, 2)``.
        """
        plan = self._parser.parse_music(prompt, duration=duration)
        if loopable:
            plan.loopable = True
        return self._generate_from_plan(plan)

    def generate_from_plan(self, plan: MusicPlan) -> np.ndarray:
        """Generate music from a pre-built :class:`~audio_engine.ai.prompt.MusicPlan`.

        Parameters
        ----------
        plan:
            Generation plan with style, BPM, duration, etc.

        Returns
        -------
        np.ndarray
            Stereo float32 array ``(N, 2)``.
        """
        return self._generate_from_plan(plan)

    def generate_to_file(
        self,
        prompt: str,
        output_path: str | Path,
        duration: float = 30.0,
        loopable: bool = False,
        fmt: Literal["wav", "ogg"] = "wav",
    ) -> Path:
        """Generate music and save to *output_path*.

        Parameters
        ----------
        prompt:
            Natural-language description.
        output_path:
            Output file path.
        duration:
            Target duration in seconds.
        loopable:
            Whether to embed loop-point metadata (WAV only).
        fmt:
            Output format: ``"wav"`` or ``"ogg"``.

        Returns
        -------
        Path
            Written file path.
        """
        audio = self.generate(prompt, duration=duration, loopable=loopable)
        path = self._exporter.export(audio, output_path, fmt=fmt)

        if loopable and fmt == "wav":
            # Embed loop points at 0 → last sample
            total_samples = audio.shape[0]
            self._exporter.write_loop_points(path, 0, total_samples - 1)

        return path

    def _generate_from_plan(self, plan: MusicPlan) -> np.ndarray:
        """Internal: execute the plan and optionally master the result."""
        audio = self._backend.generate_music_audio(
            style=plan.style,
            duration=plan.duration,
            bpm=plan.bpm,
        )

        if self.apply_mastering:
            audio = self._master(audio)

        return audio

    def _master(self, audio: np.ndarray) -> np.ndarray:
        """Apply a lightweight mastering pass."""
        from audio_engine.render.offline_bounce import OfflineBounce

        bounce = OfflineBounce(
            sample_rate=self.sample_rate,
            target_lufs=-16.0,
            ceiling_db=-0.3,
            apply_master_eq=True,
            apply_compression=True,
        )
        return bounce.process(audio)
