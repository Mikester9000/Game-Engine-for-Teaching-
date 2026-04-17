"""
Sequencer – assembles Notes into a multi-track audio buffer.
"""
from __future__ import annotations
from dataclasses import dataclass, field
from typing import NamedTuple
import numpy as np
from audio_engine.synthesizer.instrument import Instrument
__all__ = ["Note", "Sequencer"]

class Note(NamedTuple):
    frequency: float
    onset: float
    duration: float
    velocity: float = 1.0

@dataclass
class _Track:
    instrument: Instrument
    notes: list[Note] = field(default_factory=list)
    pan: float = 0.0
    volume: float = 1.0

class Sequencer:
    def __init__(self, bpm: float = 120.0, time_signature: int = 4, sample_rate: int = 44100) -> None:
        self.bpm = bpm
        self.time_signature = time_signature
        self.sample_rate = sample_rate
        self._tracks: dict[str, _Track] = {}

    def add_track(self, name: str, instrument: Instrument, pan: float = 0.0, volume: float = 1.0) -> None:
        self._tracks[name] = _Track(instrument=instrument, pan=pan, volume=volume)

    def add_note(self, track_name: str, frequency: float, onset: float, duration: float, velocity: float = 1.0) -> None:
        if track_name not in self._tracks:
            raise KeyError(f"Track '{track_name}' not found")
        self._tracks[track_name].notes.append(Note(frequency, onset, duration, velocity))

    def add_notes(self, track_name: str, notes: list[Note]) -> None:
        for note in notes:
            self.add_note(track_name, note.frequency, note.onset, note.duration, note.velocity)

    def clear_track(self, track_name: str) -> None:
        if track_name in self._tracks:
            self._tracks[track_name].notes.clear()

    @property
    def beat_duration(self) -> float:
        return 60.0 / self.bpm

    @property
    def bar_duration(self) -> float:
        return self.beat_duration * self.time_signature

    def beats_to_seconds(self, beats: float) -> float:
        return beats * self.beat_duration

    def render(self, duration: float | None = None) -> np.ndarray:
        if duration is None:
            duration = self._total_duration()
        if duration <= 0:
            return np.zeros((0, 2), dtype=np.float32)
        total_samples = int(duration * self.sample_rate)
        mix_left = np.zeros(total_samples, dtype=np.float64)
        mix_right = np.zeros(total_samples, dtype=np.float64)
        for track in self._tracks.values():
            for note in track.notes:
                note_samples = int(note.duration * self.sample_rate)
                if note_samples <= 0:
                    continue
                rendered = track.instrument.render(note.frequency, note.duration)
                rendered = rendered * note.velocity * track.volume
                onset_sample = int(note.onset * self.sample_rate)
                end_sample = onset_sample + len(rendered)
                if onset_sample >= total_samples:
                    continue
                end_sample = min(end_sample, total_samples)
                chunk = rendered[: end_sample - onset_sample]
                pan = np.clip(track.pan, -1.0, 1.0)
                left_gain = np.cos((pan + 1.0) * np.pi / 4.0)
                right_gain = np.sin((pan + 1.0) * np.pi / 4.0)
                mix_left[onset_sample:end_sample] += left_gain * chunk
                mix_right[onset_sample:end_sample] += right_gain * chunk
        stereo = np.column_stack([mix_left, mix_right]).astype(np.float32)
        peak = np.max(np.abs(stereo))
        if peak > 1.0:
            stereo /= peak
        return stereo

    def _total_duration(self) -> float:
        max_end = 0.0
        for track in self._tracks.values():
            for note in track.notes:
                max_end = max(max_end, note.onset + note.duration)
        return max_end
