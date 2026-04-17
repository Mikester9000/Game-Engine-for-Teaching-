"""
game_state_map.py – Maps engine GameState values to audio asset filenames.

TEACHING NOTE — Separation of Concerns
=======================================
This file is the single source of truth for *which* audio assets the game
engine expects. Both the Python asset pipeline (AssetPipeline) and the C++
AudioSystem share this naming convention:

  music_<game_state>.wav
  sfx_<event_key>.wav
  voice_<voice_key>.wav

If you add a new game state, add one entry here and the asset pipeline will
automatically generate the matching WAV file.

The C++ AudioSystem's ``VerifyManifest()`` method checks for these exact
filenames at runtime. If a file is missing, Init() returns ``false`` (the
game runs silently rather than crashing).
"""

from __future__ import annotations

from dataclasses import dataclass, field

__all__ = [
    "MusicAsset",
    "SFXAsset",
    "VoiceAsset",
    "MUSIC_MANIFEST",
    "SFX_MANIFEST",
    "VOICE_MANIFEST",
]


# ---------------------------------------------------------------------------
# Asset data classes
# ---------------------------------------------------------------------------

@dataclass
class MusicAsset:
    """Metadata for one background music track.

    Parameters
    ----------
    game_state:
        Human-readable label that matches the engine's GameState enum value
        (e.g. ``"main_menu"``).
    filename:
        WAV filename under ``assets/audio/music/``.
    prompt:
        Natural-language prompt for the AI generator.
    duration:
        Target duration in seconds.
    loopable:
        Whether the track should loop seamlessly.
    """

    game_state: str
    filename: str
    prompt: str
    duration: float = 60.0
    loopable: bool = True


@dataclass
class SFXAsset:
    """Metadata for one sound-effect asset.

    Parameters
    ----------
    event:
        Logical event key used by ``AudioSystem::PlaySFX(key)``.
        The filename is derived as ``sfx_<event>.wav``.
    filename:
        WAV filename under ``assets/audio/sfx/``.
    prompt:
        Natural-language prompt for the AI generator.
    duration:
        Target duration in seconds.
    pitch_hz:
        Optional pitch override in Hz.
    """

    event: str
    filename: str
    prompt: str
    duration: float = 1.0
    pitch_hz: float | None = None


@dataclass
class VoiceAsset:
    """Metadata for one voiced narrator/character line.

    Parameters
    ----------
    key:
        Logical voice key used by ``AudioSystem::PlayVoice(key)``.
        The filename is derived as ``voice_<key>.wav``.
    filename:
        WAV filename under ``assets/audio/voice/``.
    text:
        Text to synthesise.
    voice:
        Voice preset identifier (``"narrator"``, ``"hero"``, ``"villain"`` …).
    """

    key: str
    filename: str
    text: str
    voice: str = "narrator"


# ---------------------------------------------------------------------------
# Music manifest  (one track per GameState + named scenes)
# ---------------------------------------------------------------------------

MUSIC_MANIFEST: list[MusicAsset] = [
    MusicAsset(
        game_state="main_menu",
        filename="music_main_menu.wav",
        prompt="serene orchestral main menu theme 80 BPM",
        duration=60.0,
        loopable=True,
    ),
    MusicAsset(
        game_state="exploring",
        filename="music_exploring.wav",
        prompt="calm open-world exploration theme 90 BPM light strings and piano",
        duration=90.0,
        loopable=True,
    ),
    MusicAsset(
        game_state="combat",
        filename="music_combat.wav",
        prompt="intense action battle theme 140 BPM heavy percussion and brass",
        duration=60.0,
        loopable=True,
    ),
    MusicAsset(
        game_state="boss_combat",
        filename="music_boss_combat.wav",
        prompt="epic boss battle theme 160 BPM full orchestra choir",
        duration=90.0,
        loopable=True,
    ),
    MusicAsset(
        game_state="dialogue",
        filename="music_dialogue.wav",
        prompt="soft emotional dialogue underscore 70 BPM piano solo",
        duration=60.0,
        loopable=True,
    ),
    MusicAsset(
        game_state="vehicle",
        filename="music_vehicle.wav",
        prompt="adventurous driving road trip theme 120 BPM rock guitar",
        duration=90.0,
        loopable=True,
    ),
    MusicAsset(
        game_state="camping",
        filename="music_camping.wav",
        prompt="peaceful camping night ambient 60 BPM acoustic guitar and crickets",
        duration=120.0,
        loopable=True,
    ),
    MusicAsset(
        game_state="inventory",
        filename="music_inventory.wav",
        prompt="calm inventory screen ambient 75 BPM gentle synth pads",
        duration=60.0,
        loopable=True,
    ),
    MusicAsset(
        game_state="shopping",
        filename="music_shopping.wav",
        prompt="upbeat town market theme 100 BPM cheerful woodwinds",
        duration=60.0,
        loopable=True,
    ),
    MusicAsset(
        game_state="victory",
        filename="music_victory.wav",
        prompt="triumphant victory fanfare 120 BPM brass and strings",
        duration=15.0,
        loopable=False,
    ),
]


# ---------------------------------------------------------------------------
# SFX manifest  (one file per gameplay event)
# ---------------------------------------------------------------------------

SFX_MANIFEST: list[SFXAsset] = [
    SFXAsset(
        event="combat_hit",
        filename="sfx_combat_hit.wav",
        prompt="sharp sword hit impact",
        duration=0.4,
    ),
    SFXAsset(
        event="combat_miss",
        filename="sfx_combat_miss.wav",
        prompt="weapon whoosh miss",
        duration=0.3,
    ),
    SFXAsset(
        event="spell_cast",
        filename="sfx_spell_cast.wav",
        prompt="magical spell casting energy surge",
        duration=0.8,
    ),
    SFXAsset(
        event="spell_hit",
        filename="sfx_spell_hit.wav",
        prompt="magic explosion impact",
        duration=0.6,
    ),
    SFXAsset(
        event="level_up",
        filename="sfx_level_up.wav",
        prompt="triumphant level up chime sparkle",
        duration=1.5,
    ),
    SFXAsset(
        event="quest_complete",
        filename="sfx_quest_complete.wav",
        prompt="quest complete success fanfare short",
        duration=2.0,
    ),
    SFXAsset(
        event="item_pickup",
        filename="sfx_item_pickup.wav",
        prompt="coin item pickup collect",
        duration=0.5,
    ),
    SFXAsset(
        event="item_equip",
        filename="sfx_item_equip.wav",
        prompt="equip gear metal click clank",
        duration=0.4,
    ),
    SFXAsset(
        event="ui_confirm",
        filename="sfx_ui_confirm.wav",
        prompt="menu confirm select click",
        duration=0.2,
    ),
    SFXAsset(
        event="ui_cancel",
        filename="sfx_ui_cancel.wav",
        prompt="menu cancel back soft click",
        duration=0.2,
    ),
    SFXAsset(
        event="door_open",
        filename="sfx_door_open.wav",
        prompt="wooden door creak open",
        duration=0.8,
    ),
    SFXAsset(
        event="enemy_death",
        filename="sfx_enemy_death.wav",
        prompt="enemy defeat death sound",
        duration=0.7,
    ),
    SFXAsset(
        event="player_death",
        filename="sfx_player_death.wav",
        prompt="hero down defeated low dramatic sting",
        duration=1.5,
    ),
    SFXAsset(
        event="camp_rest",
        filename="sfx_camp_rest.wav",
        prompt="camp fire crackle rest peaceful",
        duration=2.0,
    ),
]


# ---------------------------------------------------------------------------
# Voice manifest  (narrator/character voiced lines)
# ---------------------------------------------------------------------------

VOICE_MANIFEST: list[VoiceAsset] = [
    VoiceAsset(
        key="welcome",
        filename="voice_welcome.wav",
        text="Welcome, traveller. Your journey begins now.",
        voice="narrator",
    ),
    VoiceAsset(
        key="level_up",
        filename="voice_level_up.wav",
        text="You have grown stronger.",
        voice="narrator",
    ),
    VoiceAsset(
        key="boss_intro",
        filename="voice_boss_intro.wav",
        text="A powerful enemy approaches. Prepare for battle.",
        voice="narrator",
    ),
    VoiceAsset(
        key="quest_complete",
        filename="voice_quest_complete.wav",
        text="Quest complete. Well done.",
        voice="narrator",
    ),
    VoiceAsset(
        key="game_over",
        filename="voice_game_over.wav",
        text="Your journey ends here... for now.",
        voice="narrator",
    ),
    VoiceAsset(
        key="camp_rest",
        filename="voice_camp_rest.wav",
        text="Rest well. Tomorrow brings new challenges.",
        voice="narrator",
    ),
]
