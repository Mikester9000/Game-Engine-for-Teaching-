"""
game_state_map.py – Mapping between game states and audio assets.

TEACHING NOTE — Asset Manifest Pattern
========================================
Every audio asset needed by the C++ engine is described here as a typed
Python dataclass.  This single source of truth is used by:
  1. :class:`~audio_engine.integration.AssetPipeline`  – to generate/cook all assets
  2. The C++ ``AudioSystem`` (via ``MUSIC_MANIFEST``, ``SFX_MANIFEST``, etc.)
     to know which filenames to load

Having the manifest in Python allows us to validate it (types, completeness)
before the C++ engine ever runs.  This is the "fail fast" principle.

Mapping to game states
-----------------------
Each ``MusicAsset.game_state`` string matches a game state name that the
C++ ``AudioSystem::OnStateChange()`` method understands:
  "exploring"  → open world traversal
  "combat"     → enemy engaged
  "boss"       → boss fight
  "menu"       → main menu / pause
  "camp"       → rest / camp screen
  "cutscene"   → cinematic
  "victory"    → battle won
  "game_over"  → player died
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Optional

__all__ = [
    "MusicAsset",
    "SFXAsset",
    "VoiceAsset",
    "MUSIC_MANIFEST",
    "SFX_MANIFEST",
    "VOICE_MANIFEST",
]


# ---------------------------------------------------------------------------
# Data classes
# ---------------------------------------------------------------------------

@dataclass
class MusicAsset:
    """Descriptor for one music track.

    Attributes
    ----------
    game_state:
        C++ game state identifier (e.g. ``"exploring"``).
    filename:
        Output filename (relative to the ``music/`` sub-directory).
    prompt:
        Natural-language prompt for the AI generator.
    duration:
        Target duration in seconds.
    loopable:
        Whether the track should loop seamlessly.
    """
    game_state: str
    filename:   str
    prompt:     str
    duration:   float = 60.0
    loopable:   bool  = True


@dataclass
class SFXAsset:
    """Descriptor for one sound effect.

    Attributes
    ----------
    event:
        C++ event key used to trigger this SFX (e.g. ``"sword_hit"``).
    filename:
        Output filename (relative to ``sfx/`` sub-directory).
    prompt:
        Natural-language description of the sound.
    duration:
        Duration in seconds.
    pitch_hz:
        Optional base pitch in Hz for the synthesiser.
    """
    event:    str
    filename: str
    prompt:   str
    duration: float           = 0.5
    pitch_hz: Optional[float] = None


@dataclass
class VoiceAsset:
    """Descriptor for one voice line.

    Attributes
    ----------
    key:
        C++ voice event key (e.g. ``"level_up"``).
    filename:
        Output filename (relative to ``voice/`` sub-directory).
    text:
        Text to synthesise.
    voice:
        Voice preset identifier (``"narrator"``, ``"hero"``, etc.).
    """
    key:      str
    filename: str
    text:     str
    voice:    str = "narrator"


# ---------------------------------------------------------------------------
# Music Manifest
# ---------------------------------------------------------------------------

MUSIC_MANIFEST: list[MusicAsset] = [
    MusicAsset(
        game_state="exploring",
        filename="music_exploring.wav",
        prompt="calm ambient open-world exploration, 90 BPM, orchestral strings and choir",
        duration=90.0,
        loopable=True,
    ),
    MusicAsset(
        game_state="combat",
        filename="music_combat.wav",
        prompt="epic orchestral battle theme, 140 BPM, brass and percussion, minor key",
        duration=60.0,
        loopable=True,
    ),
    MusicAsset(
        game_state="boss",
        filename="music_boss.wav",
        prompt="intense boss fight theme, 160 BPM, Phrygian mode, electric guitar and orchestra",
        duration=90.0,
        loopable=True,
    ),
    MusicAsset(
        game_state="menu",
        filename="music_menu.wav",
        prompt="calm introspective main menu theme, 80 BPM, piano and strings",
        duration=60.0,
        loopable=True,
    ),
    MusicAsset(
        game_state="camp",
        filename="music_camp.wav",
        prompt="peaceful campfire rest scene, acoustic guitar and ambient nature sounds",
        duration=60.0,
        loopable=True,
    ),
    MusicAsset(
        game_state="victory",
        filename="music_victory.wav",
        prompt="triumphant victory fanfare, 120 BPM, major key, brass and strings",
        duration=15.0,
        loopable=False,
    ),
    MusicAsset(
        game_state="game_over",
        filename="music_game_over.wav",
        prompt="somber game over theme, slow piano, minor key",
        duration=15.0,
        loopable=False,
    ),
    MusicAsset(
        game_state="cutscene",
        filename="music_cutscene.wav",
        prompt="cinematic cutscene music, emotional, orchestral, 100 BPM",
        duration=60.0,
        loopable=False,
    ),
]


# ---------------------------------------------------------------------------
# SFX Manifest
# ---------------------------------------------------------------------------

SFX_MANIFEST: list[SFXAsset] = [
    SFXAsset(event="sword_hit",      filename="sfx_sword_hit.wav",      prompt="sharp metal sword hit impact",         duration=0.3),
    SFXAsset(event="sword_miss",     filename="sfx_sword_miss.wav",     prompt="sword whoosh miss",                    duration=0.25),
    SFXAsset(event="magic_cast",     filename="sfx_magic_cast.wav",     prompt="magical spell cast shimmer",           duration=0.6,  pitch_hz=880.0),
    SFXAsset(event="enemy_death",    filename="sfx_enemy_death.wav",    prompt="enemy defeat thud impact",             duration=0.4),
    SFXAsset(event="player_hurt",    filename="sfx_player_hurt.wav",    prompt="player take damage grunt hit",         duration=0.3),
    SFXAsset(event="level_up",       filename="sfx_level_up.wav",       prompt="triumphant level up bell chime",       duration=1.0,  pitch_hz=660.0),
    SFXAsset(event="item_pickup",    filename="sfx_item_pickup.wav",    prompt="item pickup coin collect chime",       duration=0.3,  pitch_hz=1047.0),
    SFXAsset(event="menu_select",    filename="sfx_menu_select.wav",    prompt="soft UI menu confirm click",           duration=0.1,  pitch_hz=440.0),
    SFXAsset(event="menu_cancel",    filename="sfx_menu_cancel.wav",    prompt="soft UI menu cancel click lower",      duration=0.1,  pitch_hz=330.0),
    SFXAsset(event="camp_fire",      filename="sfx_camp_fire.wav",      prompt="crackling campfire ambient loop",      duration=2.0),
    SFXAsset(event="quest_complete", filename="sfx_quest_complete.wav", prompt="quest completion fanfare chime",       duration=1.5,  pitch_hz=523.0),
]


# ---------------------------------------------------------------------------
# Voice Manifest
# ---------------------------------------------------------------------------

VOICE_MANIFEST: list[VoiceAsset] = [
    VoiceAsset(key="level_up",       filename="voice_level_up.wav",       text="Level up!",                      voice="narrator"),
    VoiceAsset(key="quest_start",    filename="voice_quest_start.wav",    text="New quest added.",                voice="narrator"),
    VoiceAsset(key="quest_complete", filename="voice_quest_complete.wav", text="Quest complete.",                 voice="narrator"),
    VoiceAsset(key="party_low_hp",   filename="voice_party_low_hp.wav",   text="Warning: party HP is low.",      voice="narrator"),
    VoiceAsset(key="game_over",      filename="voice_game_over.wav",       text="Game over.",                     voice="narrator"),
]
