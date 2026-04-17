"""
audio_engine.integration – Game Engine for Teaching compatibility layer.

This sub-package bridges the Python Audio Engine with the C++ Game Engine
for Teaching (https://github.com/Mikester9000/Game-Engine-for-Teaching-).

Quick-start
-----------
Generate the complete audio asset library for the game engine:

    python -m audio_engine.cli generate-game-assets \\
        --output-dir ./game/assets/audio

Or from Python:

    from audio_engine.integration import AssetPipeline
    pipeline = AssetPipeline(sample_rate=44100, seed=42)
    manifest = pipeline.generate_all("./game/assets/audio")
    print(manifest.summary())

C++ integration
---------------
Copy ``AudioSystem.hpp`` (found next to this file) into your C++ project:

    cp <this package dir>/cpp/AudioSystem.hpp src/engine/audio/

See ``AudioSystem.hpp`` for full documentation and the wiring instructions
for ``Game.hpp`` / ``Game.cpp``.

Lua integration
---------------
Copy ``audio.lua`` into the game engine's ``scripts/`` directory:

    cp <this package dir>/lua/audio.lua scripts/

The script auto-wires into the existing Lua hooks
(``on_combat_start``, ``on_camp_rest``, ``on_level_up``, …).
"""

from audio_engine.integration.game_state_map import (
    MUSIC_MANIFEST,
    SFX_MANIFEST,
    VOICE_MANIFEST,
    MusicAsset,
    SFXAsset,
    VoiceAsset,
)
from audio_engine.integration.asset_pipeline import AssetPipeline, GenerationManifest

__all__ = [
    "AssetPipeline",
    "GenerationManifest",
    "MUSIC_MANIFEST",
    "SFX_MANIFEST",
    "VOICE_MANIFEST",
    "MusicAsset",
    "SFXAsset",
    "VoiceAsset",
]
