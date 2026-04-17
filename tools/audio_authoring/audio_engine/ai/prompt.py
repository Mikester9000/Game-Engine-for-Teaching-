"""
Prompt-to-Plan conversion for the AI audio generation pipeline.

Parses natural-language prompts and structured parameters into a
:class:`GenerationPlan` – a JSON-serialisable dataclass that describes
*what* should be generated before any audio synthesis occurs.

The prompt parser uses simple keyword matching and heuristics.  It is
designed as a pluggable stage: in future it can be replaced with an
LLM-based planner that produces more nuanced plans from free-form text.

Usage
-----
>>> from audio_engine.ai.prompt import PromptParser, GenerationPlan
>>> plan = PromptParser().parse_music("epic battle theme, 120 BPM, loopable")
>>> print(plan.style, plan.bpm, plan.loopable)
boss 120 True
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from typing import Literal

__all__ = [
    "GenerationPlan",
    "MusicPlan",
    "SFXPlan",
    "VoicePlan",
    "PromptParser",
]

AssetType = Literal["music", "sfx", "voice"]


@dataclass
class GenerationPlan:
    """Base plan common to all asset types.

    Attributes
    ----------
    asset_type:
        What type of asset to generate.
    prompt:
        Original user prompt (preserved for logging/debug).
    duration:
        Target duration in seconds.
    output_path:
        Output file path.
    sample_rate:
        Audio sample rate.
    """

    asset_type: AssetType
    prompt: str
    duration: float
    output_path: str
    sample_rate: int = 44100


@dataclass
class MusicPlan(GenerationPlan):
    """Plan for music generation.

    Attributes
    ----------
    style:
        Musical style preset to use (e.g. ``"battle"``, ``"ambient"``).
    bpm:
        Beats per minute.
    loopable:
        Whether to generate a seamless loop.
    bars:
        Number of bars (overrides duration if set).
    key:
        Musical key (e.g. ``"A"``).
    """

    asset_type: AssetType = field(default="music", init=False)
    style: str = "battle"
    bpm: float | None = None
    loopable: bool = False
    bars: int | None = None
    key: str = "A"


@dataclass
class SFXPlan(GenerationPlan):
    """Plan for sound effect generation.

    Attributes
    ----------
    sfx_type:
        SFX category hint (e.g. ``"explosion"``, ``"footstep"``, etc.).
    pitch_hz:
        Optional base pitch in Hz.
    """

    asset_type: AssetType = field(default="sfx", init=False)
    sfx_type: str = "generic"
    pitch_hz: float | None = None


@dataclass
class VoicePlan(GenerationPlan):
    """Plan for voice/TTS generation.

    Attributes
    ----------
    text:
        The text to speak.
    voice_preset:
        Voice preset identifier (e.g. ``"narrator"``, ``"hero"``,
        ``"villain"``).
    speed:
        Speech rate multiplier (1.0 = normal).
    """

    asset_type: AssetType = field(default="voice", init=False)
    text: str = ""
    voice_preset: str = "narrator"
    speed: float = 1.0


# ---------------------------------------------------------------------------
# Keyword tables for prompt parsing
# ---------------------------------------------------------------------------

_STYLE_KEYWORDS: dict[str, list[str]] = {
    "battle":      ["battle", "combat", "fight", "war", "action", "aggressive", "intense"],
    "boss":        ["boss", "epic", "climax", "final", "dramatic", "dark"],
    "exploration": ["explore", "exploration", "adventure", "journey", "travel", "world"],
    "ambient":     ["ambient", "calm", "atmosphere", "background", "atmospheric", "dungeon", "menu"],
    "victory":     ["victory", "triumph", "fanfare", "win", "success", "celebrate"],
    "menu":        ["menu", "title", "screen", "calm", "peaceful", "introspective"],
}

_SFX_KEYWORDS: list[str] = [
    "explosion", "footstep", "click", "beep", "hit", "impact",
    "whoosh", "laser", "coin", "pickup", "jump", "land",
    "door", "open", "close", "ping", "alert", "notification",
    "magic", "spell", "fire", "water", "wind", "rain",
]

_VOICE_PRESETS: dict[str, list[str]] = {
    "narrator":   ["narrator", "narrate", "story", "tale"],
    "hero":       ["hero", "protagonist", "player", "brave"],
    "villain":    ["villain", "antagonist", "evil", "dark"],
    "announcer":  ["announcer", "announcer", "system", "alert"],
    "npc":        ["npc", "character", "voice"],
}


def _extract_bpm(prompt: str) -> float | None:
    """Extract a BPM value from a prompt string."""
    m = re.search(r"\b(\d{2,3})\s*(?:bpm|BPM)\b", prompt)
    if m:
        return float(m.group(1))
    return None


def _extract_duration(prompt: str) -> float | None:
    """Extract a duration in seconds from a prompt string."""
    # e.g. "30 seconds", "2 min", "1.5 minutes"
    m = re.search(r"\b(\d+(?:\.\d+)?)\s*(s|sec|seconds?|m|min|minutes?)\b", prompt, re.IGNORECASE)
    if m:
        val = float(m.group(1))
        unit = m.group(2).lower()
        if unit.startswith("m"):
            val *= 60.0
        return val
    return None


def _best_match(prompt_lower: str, keyword_map: dict[str, list[str]], default: str) -> str:
    """Return the key whose keywords best match the prompt."""
    scores: dict[str, int] = {k: 0 for k in keyword_map}
    for key, words in keyword_map.items():
        for word in words:
            if word in prompt_lower:
                scores[key] += 1
    best = max(scores, key=lambda k: scores[k])
    return best if scores[best] > 0 else default


class PromptParser:
    """Parse free-form text prompts into structured generation plans.

    The parser uses keyword matching for style/type detection and regex for
    numeric parameters (BPM, duration).  It is intentionally simple and can
    be replaced with an LLM-based planner for more sophisticated prompts.

    Example
    -------
    >>> parser = PromptParser()
    >>> plan = parser.parse_music("dark battle theme 140 BPM loopable")
    >>> plan.style
    'battle'
    >>> plan.bpm
    140.0
    >>> plan.loopable
    True
    """

    def parse_music(
        self,
        prompt: str,
        duration: float = 30.0,
        output_path: str = "music.wav",
        sample_rate: int = 44100,
    ) -> MusicPlan:
        """Parse a music generation prompt.

        Parameters
        ----------
        prompt:
            Free-form description, e.g.
            ``"epic orchestral battle theme 120 BPM loopable"``.
        duration:
            Default duration if not found in prompt.
        output_path:
            Default output file path.
        sample_rate:
            Audio sample rate.

        Returns
        -------
        :class:`MusicPlan`
        """
        pl = prompt.lower()
        style = _best_match(pl, _STYLE_KEYWORDS, "exploration")
        bpm = _extract_bpm(prompt)
        dur = _extract_duration(prompt) or duration
        loopable = any(w in pl for w in ("loop", "loopable", "seamless", "repeating"))

        return MusicPlan(
            prompt=prompt,
            duration=dur,
            output_path=output_path,
            sample_rate=sample_rate,
            style=style,
            bpm=bpm,
            loopable=loopable,
        )

    def parse_sfx(
        self,
        prompt: str,
        duration: float = 1.0,
        output_path: str = "sfx.wav",
        sample_rate: int = 44100,
    ) -> SFXPlan:
        """Parse an SFX generation prompt.

        Parameters
        ----------
        prompt:
            Free-form description, e.g. ``"sharp explosion impact 0.5 seconds"``.
        duration:
            Default duration if not found in prompt.
        output_path:
            Default output file path.
        sample_rate:
            Audio sample rate.

        Returns
        -------
        :class:`SFXPlan`
        """
        pl = prompt.lower()
        sfx_type = "generic"
        for kw in _SFX_KEYWORDS:
            if kw in pl:
                sfx_type = kw
                break
        dur = _extract_duration(prompt) or duration

        return SFXPlan(
            prompt=prompt,
            duration=dur,
            output_path=output_path,
            sample_rate=sample_rate,
            sfx_type=sfx_type,
        )

    def parse_voice(
        self,
        text: str,
        voice_preset: str = "narrator",
        duration: float = 5.0,
        output_path: str = "voice.wav",
        sample_rate: int = 22050,
        speed: float = 1.0,
    ) -> VoicePlan:
        """Parse/create a voice synthesis plan.

        Parameters
        ----------
        text:
            The text to speak.
        voice_preset:
            Voice preset name.  If unrecognised, defaults to ``"narrator"``.
        duration:
            Estimated duration (used as a hint; TTS sets actual length).
        output_path:
            Default output file path.
        sample_rate:
            Audio sample rate (22050 Hz is typical for TTS output).
        speed:
            Speech rate multiplier.

        Returns
        -------
        :class:`VoicePlan`
        """
        preset_lower = voice_preset.lower()
        resolved = _best_match(preset_lower, _VOICE_PRESETS, "narrator")

        return VoicePlan(
            prompt=text,
            duration=duration,
            output_path=output_path,
            sample_rate=sample_rate,
            text=text,
            voice_preset=resolved,
            speed=speed,
        )
