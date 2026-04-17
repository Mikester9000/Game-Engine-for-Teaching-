"""
tests/test_audio_engine.py — Tests for the AudioEngine façade.

TEACHING NOTE — What to test in a façade class
================================================
A façade is tested at the boundary: does it accept the right inputs,
return the right types, and delegate to sub-systems correctly?
We use numpy arrays (the return type of every generation method) to
verify shape and dtype without relying on specific audio content.
"""

import tempfile
from pathlib import Path

import numpy as np
import pytest

from audio_engine import AudioEngine


# ---------------------------------------------------------------------------
# Fixture
# ---------------------------------------------------------------------------

@pytest.fixture
def engine():
    """Return an AudioEngine with a fixed seed for reproducible tests."""
    return AudioEngine(sample_rate=8000, seed=42)


# ---------------------------------------------------------------------------
# Construction
# ---------------------------------------------------------------------------

def test_instantiation_default():
    """AudioEngine can be constructed with default parameters."""
    ae = AudioEngine()
    assert ae.sample_rate == 44100
    assert ae.bit_depth == 16


def test_instantiation_custom():
    """AudioEngine accepts custom sample_rate and bit_depth."""
    ae = AudioEngine(sample_rate=22050, bit_depth=32, seed=0)
    assert ae.sample_rate == 22050
    assert ae.bit_depth == 32


# ---------------------------------------------------------------------------
# generate_track
# ---------------------------------------------------------------------------

def test_generate_track_returns_ndarray(engine):
    """generate_track returns a numpy array."""
    audio = engine.generate_track("battle", bars=2)
    assert isinstance(audio, np.ndarray)


def test_generate_track_shape_stereo(engine):
    """generate_track returns a stereo (N, 2) array."""
    audio = engine.generate_track("battle", bars=2)
    assert audio.ndim == 2
    assert audio.shape[1] == 2


def test_generate_track_length(engine):
    """generate_track duration is proportional to bar count."""
    audio_2 = engine.generate_track("battle", bars=2)
    audio_4 = engine.generate_track("battle", bars=4)
    # 4-bar track is exactly twice as long as 2-bar track
    assert audio_4.shape[0] == 2 * audio_2.shape[0]


def test_generate_track_known_style(engine):
    """generate_track works for every listed style without error."""
    for style in AudioEngine.available_styles():
        audio = engine.generate_track(style, bars=1)
        assert audio.shape[0] > 0


# ---------------------------------------------------------------------------
# generate_music
# ---------------------------------------------------------------------------

def test_generate_music_returns_ndarray(engine):
    """generate_music returns a numpy array."""
    audio = engine.generate_music("calm ambient", duration=0.5)
    assert isinstance(audio, np.ndarray)


def test_generate_music_duration(engine):
    """generate_music honours the duration parameter."""
    sr = engine.sample_rate
    audio = engine.generate_music("battle", duration=1.0)
    # Allow 5 % tolerance
    expected = sr * 1.0
    assert abs(audio.shape[0] - expected) <= expected * 0.05


# ---------------------------------------------------------------------------
# generate_sfx_from_prompt
# ---------------------------------------------------------------------------

def test_generate_sfx_returns_array(engine):
    """generate_sfx_from_prompt returns a 1-D numpy array."""
    audio = engine.generate_sfx_from_prompt("explosion", duration=0.5)
    assert isinstance(audio, np.ndarray)
    assert audio.ndim == 1


# ---------------------------------------------------------------------------
# generate_voice
# ---------------------------------------------------------------------------

def test_generate_voice_returns_array(engine):
    """generate_voice returns a numpy array."""
    audio = engine.generate_voice("hello world")
    assert isinstance(audio, np.ndarray)


# ---------------------------------------------------------------------------
# export / _save
# ---------------------------------------------------------------------------

def test_export_to_wav(engine, tmp_path):
    """Exported WAV file exists and is non-empty."""
    audio = engine.generate_track("menu", bars=1)
    out = tmp_path / "test.wav"
    result = engine.export(audio, out, fmt="wav")
    assert result.exists()
    assert result.stat().st_size > 44  # WAV header is 44 bytes


def test_generate_track_saves_file(engine, tmp_path):
    """generate_track writes a file when output_path is supplied."""
    out = tmp_path / "track.wav"
    engine.generate_track("battle", bars=1, output_path=out)
    assert out.exists()
    assert out.stat().st_size > 44


# ---------------------------------------------------------------------------
# Introspection
# ---------------------------------------------------------------------------

def test_available_styles_is_list():
    """available_styles() returns a non-empty list of strings."""
    styles = AudioEngine.available_styles()
    assert isinstance(styles, list)
    assert len(styles) > 0
    assert all(isinstance(s, str) for s in styles)


def test_available_voices_is_list():
    """available_voices() returns a non-empty list of strings."""
    voices = AudioEngine.available_voices()
    assert isinstance(voices, list)
    assert len(voices) > 0
    assert all(isinstance(v, str) for v in voices)
