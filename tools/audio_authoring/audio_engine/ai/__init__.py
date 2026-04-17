"""
AI sub-package – local AI-assisted generation of music, SFX, and voice.

Quick-start
-----------
>>> from audio_engine.ai import MusicGen, SFXGen, VoiceGen

# Generate a battle track
>>> gen = MusicGen(sample_rate=44100, seed=42)
>>> audio = gen.generate("epic orchestral battle 140 BPM loopable")

# Generate a sound effect
>>> sfx = SFXGen().generate("explosion impact")

# Synthesise speech
>>> voice = VoiceGen().generate("The hero must find the crystal.", voice="narrator")
"""

from audio_engine.ai.generator import MusicGenerator, TrackStyle
from audio_engine.ai.music_gen import MusicGen
from audio_engine.ai.sfx_gen import SFXGen
from audio_engine.ai.voice_gen import VoiceGen
from audio_engine.ai.prompt import PromptParser, MusicPlan, SFXPlan, VoicePlan
from audio_engine.ai.backend import InferenceBackend, ProceduralBackend, BackendRegistry

__all__ = [
    # Legacy
    "MusicGenerator",
    "TrackStyle",
    # New high-level generators
    "MusicGen",
    "SFXGen",
    "VoiceGen",
    # Prompt / plan
    "PromptParser",
    "MusicPlan",
    "SFXPlan",
    "VoicePlan",
    # Backend
    "InferenceBackend",
    "ProceduralBackend",
    "BackendRegistry",
]
