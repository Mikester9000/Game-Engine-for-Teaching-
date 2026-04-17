"""
AudioEngine — top-level façade for the Audio Authoring Tool.

Vendored from Mikester9000/Audio-Engine into tools/audio_authoring/.

TEACHING NOTE — Façade Pattern
================================
The AudioEngine class is a Façade: it hides the complexity of multiple
sub-systems (AI generator, sequencer, DSP, exporter) behind a single,
clean interface.

Design Pattern: Facade
  Problem:  The audio toolchain has many moving parts (synthesizer, DSP,
            AI generation, file export). Each has its own API.
  Solution: Create one class that orchestrates them all. Users only need
            to know AudioEngine, not MusicGenerator, AudioExporter, etc.
  Result:   Simple API for the common cases; sub-modules available for
            advanced use.

This pattern is used throughout game engines (e.g. Unreal's UGameInstance,
Unity's AudioSource) to give designers a clean entry point.

Usage:
------
>>> from audio_engine import AudioEngine
>>> engine = AudioEngine()
>>> engine.generate_track("battle", output_path="battle.wav")
>>> engine.generate_music("dark ambient dungeon loop 90 BPM",
...                       output_path="dungeon.wav", loopable=True)
>>> engine.generate_sfx_from_prompt("sword slash", output_path="slash.wav")
"""

from __future__ import annotations

import sys
from pathlib import Path
from typing import TYPE_CHECKING, Literal, Optional

# TEACHING NOTE — Graceful degradation
# NumPy is required for audio processing. If it's not installed, we provide
# a helpful error message with installation instructions.
try:
    import numpy as np
    _NUMPY_AVAILABLE = True
except ImportError:
    _NUMPY_AVAILABLE = False
    np = None  # type: ignore[assignment]

__all__ = ["AudioEngine"]


def _require_numpy() -> None:
    """Raise ImportError with installation help if NumPy is not available."""
    if not _NUMPY_AVAILABLE:
        raise ImportError(
            "NumPy is required for audio processing.\n"
            "Install it with:  pip install numpy\n"
            "Or install all tool deps:  pip install -r tools/audio_authoring/requirements.txt"
        )


class AudioEngine:
    """Unified Audio Engine façade.

    Parameters
    ----------
    sample_rate : int
        Audio sample rate in Hz (default 44 100 Hz — CD quality).
    bit_depth : int
        Bit depth for exported WAV files: 16 (default) or 32.
    seed : int or None
        Random seed for reproducible AI generation. None = random.

    TEACHING NOTE — Constructor Dependency Injection
    All dependencies (generator, exporter) are created here with sensible
    defaults. Advanced users can pass different values to customise behaviour.
    This is "constructor injection" — a dependency injection pattern.
    """

    def __init__(
        self,
        sample_rate: int = 44100,
        bit_depth: Literal[16, 32] = 16,
        seed: Optional[int] = None,
    ) -> None:
        _require_numpy()
        self.sample_rate = sample_rate
        self.bit_depth   = bit_depth
        self._seed       = seed

    # ------------------------------------------------------------------
    # Style-based track generation
    # ------------------------------------------------------------------

    def generate_track(
        self,
        style: str = "battle",
        bars: Optional[int] = None,
        output_path: Optional[str | Path] = None,
        fmt: Literal["wav", "ogg"] = "wav",
    ) -> "np.ndarray":  # type: ignore[name-defined]
        """Generate a complete music track using the AI composer.

        Parameters
        ----------
        style : str
            Musical style: "battle", "exploration", "ambient", "boss",
            "victory", or "menu".
        bars : int or None
            Number of bars. None → style default.
        output_path : str or Path or None
            If supplied, save to this file.
        fmt : str
            Output format: "wav" or "ogg".

        Returns
        -------
        np.ndarray
            Stereo float32 array shape (N, 2).

        TEACHING NOTE — Return types
        We return the NumPy array AND optionally save to file. This lets
        callers chain operations (e.g. apply more DSP) without re-reading
        the file from disk.
        """
        _require_numpy()
        # STUB: Returns silence until the real MusicGenerator is wired up.
        # Replace this with: from .ai.generator import MusicGenerator
        duration = (bars or 8) * 2.0   # 2 seconds per bar as default
        samples  = int(duration * self.sample_rate)
        audio    = np.zeros((samples, 2), dtype=np.float32)

        if output_path is not None:
            self._save(audio, output_path, fmt)
        return audio

    # ------------------------------------------------------------------
    # Prompt-driven generation
    # ------------------------------------------------------------------

    def generate_music(
        self,
        prompt: str,
        duration: float = 30.0,
        loopable: bool = False,
        output_path: Optional[str | Path] = None,
        fmt: Literal["wav", "ogg"] = "wav",
    ) -> "np.ndarray":  # type: ignore[name-defined]
        """Generate music from a natural-language prompt.

        Parameters
        ----------
        prompt : str
            E.g. "epic orchestral battle theme 140 BPM loopable".
        duration : float
            Target duration in seconds.
        loopable : bool
            Embed loop-point metadata in the exported WAV.
        output_path : str or Path or None
            Save path.
        fmt : str
            "wav" or "ogg".
        """
        _require_numpy()
        samples = int(duration * self.sample_rate)
        audio   = np.zeros((samples, 2), dtype=np.float32)  # STUB
        if output_path is not None:
            self._save(audio, output_path, fmt)
        return audio

    def generate_sfx_from_prompt(
        self,
        prompt: str,
        duration: float = 1.0,
        output_path: Optional[str | Path] = None,
    ) -> "np.ndarray":  # type: ignore[name-defined]
        """Generate a sound effect from a natural-language prompt."""
        _require_numpy()
        samples = int(duration * self.sample_rate)
        audio   = np.zeros(samples, dtype=np.float32)  # STUB mono
        if output_path is not None:
            self._save(audio, output_path, "wav")
        return audio

    def generate_voice(
        self,
        text: str,
        voice: str = "narrator",
        speed: float = 1.0,
        output_path: Optional[str | Path] = None,
    ) -> "np.ndarray":  # type: ignore[name-defined]
        """Synthesise speech from text using local TTS (stub)."""
        _require_numpy()
        samples = int(2.0 * 22050)  # 2 s at 22 kHz (voice rate)
        audio   = np.zeros(samples, dtype=np.float32)  # STUB
        if output_path is not None:
            self._save(audio, output_path, "wav")
        return audio

    # ------------------------------------------------------------------
    # Export
    # ------------------------------------------------------------------

    def export(
        self,
        audio: "np.ndarray",  # type: ignore[name-defined]
        path: str | Path,
        fmt: Literal["wav", "ogg"] = "wav",
    ) -> Path:
        """Export a NumPy audio array to a WAV or OGG file."""
        _require_numpy()
        return self._save(audio, path, fmt)

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _save(self, audio: "np.ndarray", path: str | Path, fmt: str) -> Path:  # type: ignore[name-defined]
        """Write audio to file using scipy.io.wavfile or soundfile.

        TEACHING NOTE — Graceful import
        We try to import scipy first (most common). If it's not available
        we fall back to a minimal WAV writer using Python's built-in wave
        module. This keeps the tool usable without heavy dependencies.
        """
        _require_numpy()
        output = Path(path)
        output.parent.mkdir(parents=True, exist_ok=True)

        if fmt == "wav":
            try:
                from scipy.io import wavfile  # type: ignore[import]
                # scipy expects int16 for 16-bit WAV
                if self.bit_depth == 16:
                    data = (audio * 32767).astype(np.int16)
                else:
                    data = audio.astype(np.float32)
                wavfile.write(str(output), self.sample_rate, data)
            except ImportError:
                # Fallback: use Python's built-in wave module (mono int16 only)
                import wave, struct
                mono = audio if audio.ndim == 1 else audio[:, 0]
                data16 = (mono * 32767).astype(np.int16)
                with wave.open(str(output), "w") as wf:
                    wf.setnchannels(1)
                    wf.setsampwidth(2)
                    wf.setframerate(self.sample_rate)
                    wf.writeframes(data16.tobytes())
        else:  # ogg
            try:
                import soundfile as sf  # type: ignore[import]
                sf.write(str(output), audio, self.sample_rate, format="OGG")
            except ImportError:
                raise ImportError(
                    "soundfile is required for OGG export.\n"
                    "Install: pip install soundfile"
                )
        return output

    # ------------------------------------------------------------------
    # Introspection
    # ------------------------------------------------------------------

    @staticmethod
    def available_styles() -> list[str]:
        """Return available music generation styles."""
        return ["battle", "exploration", "ambient", "boss", "victory", "menu"]

    @staticmethod
    def available_voices() -> list[str]:
        """Return available TTS voice presets."""
        return ["narrator", "hero", "villain", "announcer", "npc"]
