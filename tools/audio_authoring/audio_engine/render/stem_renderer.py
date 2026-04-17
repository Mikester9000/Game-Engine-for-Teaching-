"""StemRenderer – export individual tracks as separate files."""
from __future__ import annotations
from pathlib import Path
from typing import Literal
import numpy as np
from audio_engine.export.audio_exporter import AudioExporter
__all__ = ["StemRenderer"]

class StemRenderer:
    """Render multi-track sequencer stems to disk."""
    def __init__(self, output_dir="stems", sample_rate=44100, bit_depth=16, fmt="wav"):
        self.output_dir = Path(output_dir)
        self.fmt = fmt
        self._exporter = AudioExporter(sample_rate=sample_rate, bit_depth=bit_depth)
    def render_stems(self, sequencer, prefix="", normalise_stems=True):
        self.output_dir.mkdir(parents=True, exist_ok=True)
        out_paths: dict = {}
        for track_name in sequencer._tracks:
            import copy
            single = copy.deepcopy(sequencer)
            for k in [k for k in single._tracks if k != track_name]:
                del single._tracks[k]
            stem_audio = single.render()
            if normalise_stems:
                peak = np.max(np.abs(stem_audio))
                if peak > 1e-9:
                    stem_audio = (stem_audio / peak * 0.707).astype(np.float32)
            safe_name = track_name.replace(" ", "_").replace("/", "_")
            filename = f"{prefix}{safe_name}.{self.fmt}" if prefix else f"{safe_name}.{self.fmt}"
            self._exporter.export(stem_audio, self.output_dir / filename, fmt=self.fmt)
            out_paths[track_name] = self.output_dir / filename
        return out_paths
    def render_mix(self, sequencer, filename="mix", normalise=True):
        self.output_dir.mkdir(parents=True, exist_ok=True)
        mix = sequencer.render()
        if normalise:
            peak = np.max(np.abs(mix))
            if peak > 1e-9:
                mix = (mix / peak * 10.0 ** (-0.3 / 20.0)).astype(np.float32)
        file_path = self.output_dir / f"{filename}.{self.fmt}"
        return self._exporter.export(mix, file_path, fmt=self.fmt)
