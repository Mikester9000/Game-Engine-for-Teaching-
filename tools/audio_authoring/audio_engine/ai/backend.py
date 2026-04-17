"""
Pluggable inference backend interface for local AI audio generation.

Defines the abstract base class :class:`InferenceBackend` that all local
model backends must implement.  Ships with a built-in
:class:`ProceduralBackend` that uses the engine's own synthesizer for
fully-offline, zero-dependency generation.

To add a real AI model backend:
1. Subclass :class:`InferenceBackend`
2. Implement :meth:`generate_music_audio`, :meth:`generate_sfx_audio`, and
   :meth:`generate_voice_audio`
3. Register it with :meth:`BackendRegistry.register`

Future backends could wrap:
- ONNX Runtime (MusicGen, Kokoro TTS, XTTS)
- ggml / llama.cpp style C++ models via ctypes
- Hugging Face Transformers (with `local_files_only=True`)
"""

from __future__ import annotations

from abc import ABC, abstractmethod

import numpy as np

__all__ = ["InferenceBackend", "ProceduralBackend", "BackendRegistry"]


class InferenceBackend(ABC):
    """Abstract base class for AI inference backends.

    All audio generation backends must implement this interface, allowing
    the higher-level generators (:class:`~audio_engine.ai.MusicGen`,
    :class:`~audio_engine.ai.SFXGen`, :class:`~audio_engine.ai.VoiceGen`)
    to remain backend-agnostic.

    Parameters
    ----------
    sample_rate:
        Output sample rate for all generated audio.
    """

    def __init__(self, sample_rate: int = 44100) -> None:
        self.sample_rate = sample_rate

    @property
    def name(self) -> str:
        """Human-readable backend name."""
        return self.__class__.__name__

    @abstractmethod
    def generate_music_audio(
        self,
        style: str,
        duration: float,
        bpm: float | None = None,
        **kwargs,
    ) -> np.ndarray:
        """Generate music audio.

        Parameters
        ----------
        style:
            Style preset (e.g. ``"battle"``, ``"ambient"``).
        duration:
            Target duration in seconds.
        bpm:
            Tempo override in BPM.  ``None`` = use style default.
        **kwargs:
            Backend-specific parameters.

        Returns
        -------
        np.ndarray
            Stereo float32 array ``(N, 2)``.
        """

    @abstractmethod
    def generate_sfx_audio(
        self,
        sfx_type: str,
        duration: float,
        pitch_hz: float | None = None,
        **kwargs,
    ) -> np.ndarray:
        """Generate a sound effect.

        Parameters
        ----------
        sfx_type:
            SFX category (e.g. ``"explosion"``, ``"footstep"``).
        duration:
            Target duration in seconds.
        pitch_hz:
            Optional base pitch in Hz.
        **kwargs:
            Backend-specific parameters.

        Returns
        -------
        np.ndarray
            Mono float32 array.
        """

    @abstractmethod
    def generate_voice_audio(
        self,
        text: str,
        voice_preset: str = "narrator",
        speed: float = 1.0,
        **kwargs,
    ) -> np.ndarray:
        """Generate voice/TTS audio.

        Parameters
        ----------
        text:
            Text to synthesise.
        voice_preset:
            Voice preset identifier.
        speed:
            Speech rate multiplier.
        **kwargs:
            Backend-specific parameters.

        Returns
        -------
        np.ndarray
            Mono float32 array.
        """

    def is_available(self) -> bool:
        """Return ``True`` if this backend's dependencies are installed."""
        return True


class ProceduralBackend(InferenceBackend):
    """Fully-local procedural synthesis backend (no external models needed).

    Uses the engine's built-in Markov-chain composer, synthesizer, and
    formant voice synthesiser.  This is the default fallback when no
    external AI model is available.

    It provides good-quality results for game prototyping and is guaranteed
    to work on any platform with only NumPy/SciPy installed.
    """

    def __init__(self, sample_rate: int = 44100, seed: int | None = None) -> None:
        super().__init__(sample_rate)
        self._seed = seed

    @property
    def name(self) -> str:
        return "procedural"

    def generate_music_audio(
        self,
        style: str,
        duration: float,
        bpm: float | None = None,
        **kwargs,
    ) -> np.ndarray:
        """Generate music using the Markov-chain composer."""
        from audio_engine.ai.generator import MusicGenerator

        gen = MusicGenerator(sample_rate=self.sample_rate, seed=self._seed)

        # Convert duration to bars using the style's default BPM
        from audio_engine.ai.generator import _STYLE_DEFS
        sdef = _STYLE_DEFS.get(style)
        effective_bpm = bpm or (sdef.bpm if sdef else 120.0)
        bar_duration = 60.0 / effective_bpm * 4  # 4/4 time
        bars = max(1, round(duration / bar_duration))

        return gen.generate_audio(style=style, bars=bars)

    def generate_sfx_audio(
        self,
        sfx_type: str,
        duration: float,
        pitch_hz: float | None = None,
        **kwargs,
    ) -> np.ndarray:
        """Generate a sound effect using procedural synthesis.

        Routes to a specialised synthesiser function based on *sfx_type*.
        """
        from audio_engine.ai.sfx_synth import synthesise_sfx

        return synthesise_sfx(
            sfx_type=sfx_type,
            duration=duration,
            pitch_hz=pitch_hz,
            sample_rate=self.sample_rate,
            seed=self._seed,
        )

    def generate_voice_audio(
        self,
        text: str,
        voice_preset: str = "narrator",
        speed: float = 1.0,
        **kwargs,
    ) -> np.ndarray:
        """Generate voice audio using a formant synthesiser."""
        from audio_engine.ai.voice_synth import synthesise_voice

        return synthesise_voice(
            text=text,
            voice_preset=voice_preset,
            speed=speed,
            sample_rate=self.sample_rate,
        )


class BackendRegistry:
    """Registry of available :class:`InferenceBackend` implementations.

    Allows dynamic registration and selection of backends by name.

    Example
    -------
    >>> BackendRegistry.register("my_model", MyModelBackend)
    >>> backend = BackendRegistry.get("my_model", sample_rate=44100)
    """

    _registry: dict[str, type[InferenceBackend]] = {
        "procedural": ProceduralBackend,
    }

    @classmethod
    def register(cls, name: str, backend_class: type[InferenceBackend]) -> None:
        """Register a new backend implementation.

        Parameters
        ----------
        name:
            Short identifier for the backend (e.g. ``"onnx_musicgen"``).
        backend_class:
            Class that implements :class:`InferenceBackend`.
        """
        cls._registry[name] = backend_class

    @classmethod
    def get(cls, name: str = "procedural", **kwargs) -> InferenceBackend:
        """Instantiate a backend by name.

        Parameters
        ----------
        name:
            Backend identifier.
        **kwargs:
            Passed to the backend constructor (e.g. ``sample_rate``).

        Returns
        -------
        :class:`InferenceBackend`
        """
        if name not in cls._registry:
            available = ", ".join(sorted(cls._registry))
            raise ValueError(f"Unknown backend '{name}'. Available: {available}")
        return cls._registry[name](**kwargs)

    @classmethod
    def available_backends(cls) -> list[str]:
        """Return sorted list of registered backend names."""
        return sorted(cls._registry)
