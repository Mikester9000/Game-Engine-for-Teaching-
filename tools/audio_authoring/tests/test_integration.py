"""
tests/test_integration.py — Tests for the audio_engine.integration sub-package.

Covers:
  - game_state_map: manifests are non-empty, asset fields are valid
  - AssetPipeline: instantiation, verify on empty dir
  - GenerationManifest: summary and to_json
"""

import json
from pathlib import Path

import pytest

from audio_engine.integration import AssetPipeline, GenerationManifest
from audio_engine.integration.game_state_map import (
    MUSIC_MANIFEST,
    SFX_MANIFEST,
    VOICE_MANIFEST,
    MusicAsset,
    SFXAsset,
    VoiceAsset,
)


# ---------------------------------------------------------------------------
# game_state_map tests
# ---------------------------------------------------------------------------

def test_music_manifest_not_empty():
    """MUSIC_MANIFEST must contain at least one asset."""
    assert len(MUSIC_MANIFEST) > 0


def test_sfx_manifest_not_empty():
    """SFX_MANIFEST must contain at least one asset."""
    assert len(SFX_MANIFEST) > 0


def test_voice_manifest_not_empty():
    """VOICE_MANIFEST must contain at least one asset."""
    assert len(VOICE_MANIFEST) > 0


def test_music_assets_have_required_fields():
    """Every MusicAsset has non-empty game_state, filename, and prompt."""
    for asset in MUSIC_MANIFEST:
        assert isinstance(asset, MusicAsset)
        assert asset.game_state, f"empty game_state in {asset}"
        assert asset.filename.endswith(".wav"), f"bad filename: {asset.filename}"
        assert asset.prompt, f"empty prompt in {asset}"
        assert asset.duration > 0


def test_sfx_assets_have_required_fields():
    """Every SFXAsset has a non-empty event key and .wav filename."""
    for asset in SFX_MANIFEST:
        assert isinstance(asset, SFXAsset)
        assert asset.event, f"empty event in {asset}"
        assert asset.filename == f"sfx_{asset.event}.wav", (
            f"filename mismatch: {asset.filename} != sfx_{asset.event}.wav"
        )


def test_voice_assets_have_required_fields():
    """Every VoiceAsset has a non-empty key, text, and .wav filename."""
    for asset in VOICE_MANIFEST:
        assert isinstance(asset, VoiceAsset)
        assert asset.key, f"empty key in {asset}"
        assert asset.text, f"empty text in {asset}"
        assert asset.filename == f"voice_{asset.key}.wav", (
            f"filename mismatch: {asset.filename} != voice_{asset.key}.wav"
        )


# ---------------------------------------------------------------------------
# AssetPipeline tests
# ---------------------------------------------------------------------------

def test_asset_pipeline_instantiation():
    """AssetPipeline can be constructed with default and custom parameters."""
    pipeline = AssetPipeline(sample_rate=44100, seed=42)
    assert pipeline.sample_rate == 44100
    assert pipeline.seed == 42


def test_asset_pipeline_verify_empty_dir(tmp_path):
    """verify() on an empty directory reports all assets as missing."""
    pipeline = AssetPipeline()
    result = pipeline.verify(tmp_path)
    assert "missing" in result
    assert "present" in result
    # All assets are missing from an empty dir
    n_expected = len(MUSIC_MANIFEST) + len(SFX_MANIFEST) + len(VOICE_MANIFEST)
    assert len(result["missing"]) == n_expected
    assert len(result["present"]) == 0


def test_asset_pipeline_verify_partial(tmp_path):
    """verify() correctly partitions present vs missing assets."""
    pipeline = AssetPipeline()
    # Create one music file
    music_dir = tmp_path / "music"
    music_dir.mkdir()
    first = MUSIC_MANIFEST[0].filename
    (music_dir / first).write_bytes(b"FAKE")

    result = pipeline.verify(tmp_path)
    assert f"music/{first}" in result["present"]
    # All others still missing
    assert len(result["missing"]) == (
        len(MUSIC_MANIFEST) + len(SFX_MANIFEST) + len(VOICE_MANIFEST) - 1
    )


# ---------------------------------------------------------------------------
# GenerationManifest tests
# ---------------------------------------------------------------------------

def test_generation_manifest_summary():
    """GenerationManifest.summary() returns a non-empty string."""
    m = GenerationManifest(output_dir="/tmp/audio")
    m.music  = [{"game_state": "combat", "file": "x.wav", "status": "ok"}]
    m.sfx    = [{"event": "hit", "file": "y.wav", "status": "ok"}]
    m.voice  = []
    summary  = m.summary()
    assert isinstance(summary, str)
    assert len(summary) > 0
    assert "Music:" in summary
    assert "SFX:" in summary


def test_generation_manifest_to_json():
    """GenerationManifest.to_json() produces valid JSON with expected keys."""
    m = GenerationManifest(output_dir="/tmp/audio")
    m.errors = ["oops"]
    data = json.loads(m.to_json())
    assert data["output_dir"] == "/tmp/audio"
    assert "music" in data
    assert "sfx" in data
    assert "voice" in data
    assert data["errors"] == ["oops"]
    assert "total_duration_seconds" in data
