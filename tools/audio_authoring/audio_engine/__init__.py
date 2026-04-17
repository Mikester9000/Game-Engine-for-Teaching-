"""
Audio Authoring Tool — vendored from Mikester9000/Audio-Engine
==============================================================

TEACHING NOTE
=============
This package is vendored from https://github.com/Mikester9000/Audio-Engine
into the monorepo under tools/audio_authoring/.

It provides a Python API for:
  - Generating music tracks via AI (MusicGen, SFXGen, VoiceGen)
  - Procedural audio composition (Sequencer, Note, InstrumentLibrary)
  - DSP processing (filters, reverb, compression)
  - Exporting cooked audio banks (WAV, OGG)
  - Quality assurance (loudness metering, clipping detection, loop analysis)

The tool produces cooked artifacts in Cooked/Audio/ that the C++ engine
loads at runtime. Python is the "compiler" (authoring/cooking); C++ is
the runtime (playback).

Shared schema: shared/schemas/audio_bank.schema.json

Quick start:
    from audio_engine import AudioEngine
    engine = AudioEngine()
    engine.generate_track("battle", bars=8, output_path="battle.wav")
"""

from audio_engine.engine import AudioEngine

__all__ = ["AudioEngine"]
__version__ = "2.0.0"
