"""
AudioExporter – write audio arrays to WAV/OGG files.

TEACHING NOTE — Audio File Formats
=====================================
Games typically ship audio in one of two formats:
  - WAV  (uncompressed PCM) – no decode overhead; large files; preferred
         for short SFX where decode latency matters.
  - OGG  (Vorbis compressed) – smaller files; slight decode overhead;
         preferred for long music tracks.

We use scipy.io.wavfile for WAV (part of SciPy, always available) and
soundfile for OGG (optional; installed separately).
"""

from __future__ import annotations

import struct
import wave
from pathlib import Path
from typing import Literal

import numpy as np

__all__ = ["AudioExporter"]


class AudioExporter:
    """Export NumPy audio arrays to audio files.

    Parameters
    ----------
    sample_rate:
        Output sample rate in Hz.
    bit_depth:
        Bit depth for WAV output: 16 (default, compact) or 32 (lossless float).

    Example
    -------
    >>> exp = AudioExporter(sample_rate=44100, bit_depth=16)
    >>> path = exp.export(audio_array, "output/battle.wav")
    """

    def __init__(self, sample_rate: int = 44100, bit_depth: Literal[16, 32] = 16) -> None:
        self.sample_rate = sample_rate
        self.bit_depth = bit_depth

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def export(
        self,
        audio: np.ndarray,
        path: str | Path,
        fmt: Literal["wav", "ogg"] = "wav",
    ) -> Path:
        """Export *audio* to *path*.

        Parameters
        ----------
        audio:
            Mono ``(N,)`` or stereo ``(N, 2)`` float32 array with samples
            in the range [-1.0, 1.0].
        path:
            Output file path (extension can be omitted if *fmt* is given).
        fmt:
            Output format: ``"wav"`` or ``"ogg"``.

        Returns
        -------
        Path
            Written file path.
        """
        output = Path(path)
        output.parent.mkdir(parents=True, exist_ok=True)

        if fmt == "wav":
            return self._write_wav(audio, output)
        elif fmt == "ogg":
            return self._write_ogg(audio, output)
        else:
            raise ValueError(f"Unsupported format '{fmt}'. Choose 'wav' or 'ogg'.")

    def write_loop_points(self, path: str | Path, loop_start: int, loop_end: int) -> None:
        """Embed SMPL chunk loop-point metadata into a WAV file.

        The engine reads these markers to seamlessly loop music tracks.

        Parameters
        ----------
        path:
            Path to an existing WAV file.
        loop_start:
            Loop start in samples.
        loop_end:
            Loop end in samples.

        TEACHING NOTE — RIFF Chunk Format
        A WAV file is made of named chunks: ``fmt `` (format), ``data``
        (samples), and optionally ``smpl`` (sample metadata including loop
        points).  We append the ``smpl`` chunk after the ``data`` chunk.
        """
        path = Path(path)
        wav_bytes = path.read_bytes()

        # Build a minimal smpl chunk with one loop entry
        smpl_loop = struct.pack(
            "<IIIIII",
            0,          # Cue Point ID
            0,          # Type (0 = forward loop)
            loop_start,
            loop_end,
            0,          # Fraction
            0,          # Play count (0 = infinite)
        )
        smpl_chunk = struct.pack(
            "<4sI",
            b"smpl",
            36 + len(smpl_loop),  # chunk size
        )
        smpl_chunk += struct.pack(
            "<IIIIIIIII",
            0,              # manufacturer
            0,              # product
            1_000_000_000 // self.sample_rate,  # sample period (ns)
            69,             # MIDI unity note (A4)
            0,              # MIDI pitch fraction
            0,              # SMPTE format
            0,              # SMPTE offset
            1,              # num_sample_loops
            0,              # sampler data size
        )
        smpl_chunk += smpl_loop

        # Update RIFF size field (bytes 4-7) and append smpl chunk
        new_size = len(wav_bytes) - 8 + len(smpl_chunk)
        updated = (
            wav_bytes[:4]
            + struct.pack("<I", new_size)
            + wav_bytes[8:]
            + smpl_chunk
        )
        path.write_bytes(updated)

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _write_wav(self, audio: np.ndarray, path: Path) -> Path:
        """Write a WAV file using scipy if available, else Python's wave."""
        audio = self._sanitise(audio)
        try:
            from scipy.io import wavfile  # type: ignore[import]
            if self.bit_depth == 16:
                data = (np.clip(audio, -1.0, 1.0) * 32767.0).astype(np.int16)
            else:
                data = audio.astype(np.float32)
            wavfile.write(str(path), self.sample_rate, data)
        except ImportError:
            # Fallback: built-in wave module (mono int16 only)
            mono = audio if audio.ndim == 1 else audio[:, 0]
            data16 = (np.clip(mono, -1.0, 1.0) * 32767.0).astype(np.int16)
            with wave.open(str(path), "w") as wf:
                wf.setnchannels(1)
                wf.setsampwidth(2)
                wf.setframerate(self.sample_rate)
                wf.writeframes(data16.tobytes())
        return path

    def _write_ogg(self, audio: np.ndarray, path: Path) -> Path:
        """Write an OGG Vorbis file using soundfile."""
        try:
            import soundfile as sf  # type: ignore[import]
        except ImportError as exc:
            raise ImportError(
                "soundfile is required for OGG export.\n"
                "Install: pip install soundfile"
            ) from exc
        audio = self._sanitise(audio)
        sf.write(str(path), audio, self.sample_rate, format="OGG")
        return path

    @staticmethod
    def _sanitise(audio: np.ndarray) -> np.ndarray:
        """Convert to float32 and clip to [-1, 1]."""
        return np.clip(audio.astype(np.float32), -1.0, 1.0)
