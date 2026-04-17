"""
High-level voice / text-to-speech (TTS) generation interface.

:class:`VoiceGen` synthesises speech from text using a local voice backend
(default: formant synthesiser; upgradeable to Piper/ONNX TTS).

Usage
-----
>>> from audio_engine.ai import VoiceGen
>>> gen = VoiceGen(sample_rate=22050)
>>> audio = gen.generate("The hero must find the crystal sword.", voice="narrator")
>>> gen.generate_to_file("Level complete!", "victory.wav", voice="announcer")
"""

from __future__ import annotations

from pathlib import Path

import numpy as np

from audio_engine.ai.backend import InferenceBackend, BackendRegistry
from audio_engine.ai.prompt import PromptParser, VoicePlan
from audio_engine.export.audio_exporter import AudioExporter

__all__ = ["VoiceGen"]


class VoiceGen:
    """High-level voice synthesis with local TTS.

    Parameters
    ----------
    sample_rate:
        Output sample rate in Hz.  22050 Hz is standard for TTS output.
    backend:
        Backend name or :class:`~audio_engine.ai.backend.InferenceBackend`
        instance.  Defaults to ``"procedural"``.
    seed:
        Random seed for reproducibility.

    Example
    -------
    >>> gen = VoiceGen()
    >>> audio = gen.generate("Welcome to the dungeon.", voice="narrator")
    """

    def __init__(
        self,
        sample_rate: int = 22050,
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
        text: str,
        voice: str = "narrator",
        speed: float = 1.0,
    ) -> np.ndarray:
        """Synthesise *text* as speech audio.

        Parameters
        ----------
        text:
            The text to speak.
        voice:
            Voice preset: ``"narrator"``, ``"hero"``, ``"villain"``,
            ``"announcer"``, or ``"npc"``.
        speed:
            Speech rate multiplier (1.0 = normal, 1.5 = faster).

        Returns
        -------
        np.ndarray
            Mono float32 audio array.
        """
        plan = self._parser.parse_voice(text, voice_preset=voice, speed=speed)
        return self._generate_from_plan(plan)

    def generate_from_plan(self, plan: VoicePlan) -> np.ndarray:
        """Generate voice from a pre-built :class:`~audio_engine.ai.prompt.VoicePlan`.

        Parameters
        ----------
        plan:
            Voice generation plan.

        Returns
        -------
        np.ndarray
            Mono float32 audio array.
        """
        return self._generate_from_plan(plan)

    def generate_to_file(
        self,
        text: str,
        output_path: str | Path,
        voice: str = "narrator",
        speed: float = 1.0,
    ) -> Path:
        """Synthesise speech and save to *output_path*.

        Parameters
        ----------
        text:
            Text to speak.
        output_path:
            Output WAV file path.
        voice:
            Voice preset name.
        speed:
            Speech rate multiplier.

        Returns
        -------
        Path
            Written file path.
        """
        audio = self.generate(text, voice=voice, speed=speed)
        return self._exporter.export(audio, output_path, fmt="wav")

    def available_voices(self) -> list[str]:
        """Return the list of available voice preset names."""
        from audio_engine.ai.voice_synth import VOICE_PRESETS

        return sorted(VOICE_PRESETS)

    def _generate_from_plan(self, plan: VoicePlan) -> np.ndarray:
        return self._backend.generate_voice_audio(
            text=plan.text,
            voice_preset=plan.voice_preset,
            speed=plan.speed,
        )
