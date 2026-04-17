"""asset_pipeline.py – pre-generate the complete audio asset library."""
from __future__ import annotations
import json, time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable
from audio_engine.integration.game_state_map import MUSIC_MANIFEST, SFX_MANIFEST, VOICE_MANIFEST, MusicAsset, SFXAsset, VoiceAsset
__all__ = ["AssetPipeline", "GenerationManifest"]

@dataclass
class GenerationManifest:
    output_dir: str
    music: list = field(default_factory=list)
    sfx: list = field(default_factory=list)
    voice: list = field(default_factory=list)
    errors: list = field(default_factory=list)
    total_duration_seconds: float = 0.0
    def summary(self):
        n_ok = len(self.music) + len(self.sfx) + len(self.voice)
        return f"Asset pipeline complete — {self.output_dir}\n  Music: {len(self.music)}, SFX: {len(self.sfx)}, Voice: {len(self.voice)}, Total: {n_ok} in {self.total_duration_seconds:.1f}s"
    def to_json(self):
        return json.dumps({"output_dir": self.output_dir, "music": self.music, "sfx": self.sfx, "voice": self.voice, "errors": self.errors, "total_duration_seconds": self.total_duration_seconds}, indent=2)

class AssetPipeline:
    """Pre-generate all audio assets required by the game engine."""
    def __init__(self, sample_rate=44100, seed=None, progress_callback=None, skip_existing=True):
        self.sample_rate = sample_rate
        self.seed = seed
        self.progress_callback = progress_callback or (lambda msg: None)
        self.skip_existing = skip_existing
    def generate_all(self, output_dir):
        output_dir = Path(output_dir)
        for sub in ("music", "sfx", "voice"):
            (output_dir / sub).mkdir(parents=True, exist_ok=True)
        manifest = GenerationManifest(output_dir=str(output_dir))
        t_start = time.monotonic()
        for asset in MUSIC_MANIFEST:
            path = output_dir / "music" / asset.filename
            if self.skip_existing and path.exists():
                manifest.music.append({"game_state": asset.game_state, "file": str(path), "status": "skipped"})
                continue
            try:
                self._generate_music(asset, path)
                manifest.music.append({"game_state": asset.game_state, "file": str(path), "status": "ok"})
            except Exception as exc:
                manifest.errors.append(f"music/{asset.filename}: {exc}")
        for asset in SFX_MANIFEST:
            path = output_dir / "sfx" / asset.filename
            if self.skip_existing and path.exists():
                manifest.sfx.append({"event": asset.event, "file": str(path), "status": "skipped"})
                continue
            try:
                self._generate_sfx(asset, path)
                manifest.sfx.append({"event": asset.event, "file": str(path), "status": "ok"})
            except Exception as exc:
                manifest.errors.append(f"sfx/{asset.filename}: {exc}")
        for asset in VOICE_MANIFEST:
            path = output_dir / "voice" / asset.filename
            if self.skip_existing and path.exists():
                manifest.voice.append({"key": asset.key, "file": str(path), "status": "skipped"})
                continue
            try:
                self._generate_voice(asset, path)
                manifest.voice.append({"key": asset.key, "file": str(path), "status": "ok"})
            except Exception as exc:
                manifest.errors.append(f"voice/{asset.filename}: {exc}")
        manifest.total_duration_seconds = time.monotonic() - t_start
        (output_dir / "manifest.json").write_text(manifest.to_json(), encoding="utf-8")
        return manifest
    def verify(self, output_dir):
        output_dir = Path(output_dir)
        missing, present = [], []
        for asset in MUSIC_MANIFEST:
            rel = f"music/{asset.filename}"
            (present if (output_dir / rel).exists() else missing).append(rel)
        for asset in SFX_MANIFEST:
            rel = f"sfx/{asset.filename}"
            (present if (output_dir / rel).exists() else missing).append(rel)
        for asset in VOICE_MANIFEST:
            rel = f"voice/{asset.filename}"
            (present if (output_dir / rel).exists() else missing).append(rel)
        return {"present": present, "missing": missing}
    def _generate_music(self, asset, output_path):
        from audio_engine.ai.music_gen import MusicGen
        from audio_engine.render.offline_bounce import OfflineBounce
        from audio_engine.export.audio_exporter import AudioExporter
        gen = MusicGen(sample_rate=self.sample_rate, seed=self.seed)
        audio = gen.generate(prompt=asset.prompt, duration=asset.duration, loopable=asset.loopable)
        bounce = OfflineBounce(sample_rate=self.sample_rate, target_lufs=-16.0, ceiling_db=-1.0)
        mastered = bounce.process(audio)
        exporter = AudioExporter(sample_rate=self.sample_rate, bit_depth=16)
        exporter.export(mastered, output_path, fmt="wav")
    def _generate_sfx(self, asset, output_path):
        from audio_engine.ai.sfx_gen import SFXGen
        from audio_engine.export.audio_exporter import AudioExporter
        gen = SFXGen(sample_rate=self.sample_rate, seed=self.seed)
        audio = gen.generate(prompt=asset.prompt, duration=asset.duration, pitch_hz=asset.pitch_hz)
        AudioExporter(sample_rate=self.sample_rate, bit_depth=16).export(audio, output_path, fmt="wav")
    def _generate_voice(self, asset, output_path):
        from audio_engine.ai.voice_gen import VoiceGen
        from audio_engine.export.audio_exporter import AudioExporter
        voice_sr = 22050
        gen = VoiceGen(sample_rate=voice_sr, seed=self.seed)
        audio = gen.generate(asset.text, voice=asset.voice)
        AudioExporter(sample_rate=voice_sr, bit_depth=16).export(audio, output_path, fmt="wav")
