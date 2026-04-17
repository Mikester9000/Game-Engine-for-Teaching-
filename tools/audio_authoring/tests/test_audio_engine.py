"""Test suite for the Audio Authoring tool.

18 tests covering:
  - Oscillator waveform generation  (3)
  - ADSREnvelope rendering          (2)
  - SynthFilter processing          (2)
  - Instrument voice render         (2)
  - InstrumentLibrary presets       (1)
  - AudioEngine facade              (2)
  - AudioExporter WAV output        (2)
  - DSP utilities                   (2)
  - Integration: game_state_map     (2)
"""

import tempfile
from pathlib import Path

import numpy as np
import pytest

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

SR = 22050  # lower sample rate keeps tests fast


# ---------------------------------------------------------------------------
# 1-3: Oscillator
# ---------------------------------------------------------------------------

class TestOscillator:
    """Tests for audio_engine.synthesizer.oscillator.Oscillator."""

    def test_sine_shape(self):
        from audio_engine.synthesizer.oscillator import Oscillator
        osc = Oscillator(sample_rate=SR, waveform="sine")
        audio = osc.generate(frequency=440.0, duration=0.1)
        assert audio.dtype == np.float32
        assert audio.ndim == 1
        assert len(audio) == int(0.1 * SR)

    def test_amplitude_clamp(self):
        from audio_engine.synthesizer.oscillator import Oscillator
        osc = Oscillator(sample_rate=SR, waveform="sine")
        audio = osc.generate(440.0, 0.1, amplitude=0.5)
        assert float(np.max(np.abs(audio))) <= 0.5 + 1e-5

    def test_all_waveforms_generate(self):
        from audio_engine.synthesizer.oscillator import Oscillator
        for wf in ("sine", "square", "sawtooth", "triangle", "noise"):
            osc = Oscillator(sample_rate=SR, waveform=wf)
            audio = osc.generate(440.0, 0.05)
            assert audio.dtype == np.float32, f"dtype wrong for {wf}"
            assert len(audio) > 0, f"empty output for {wf}"


# ---------------------------------------------------------------------------
# 4-5: ADSR Envelope
# ---------------------------------------------------------------------------

class TestADSREnvelope:
    """Tests for audio_engine.synthesizer.envelope.ADSREnvelope."""

    def test_envelope_starts_at_zero(self):
        from audio_engine.synthesizer.envelope import ADSREnvelope
        env = ADSREnvelope(attack=0.05, decay=0.05, sustain=0.7, release=0.1, sample_rate=SR)
        curve = env.render(duration=0.5)
        assert curve[0] < 0.05, "envelope should start near zero"

    def test_envelope_ends_at_zero(self):
        from audio_engine.synthesizer.envelope import ADSREnvelope
        env = ADSREnvelope(attack=0.01, decay=0.05, sustain=0.7, release=0.1, sample_rate=SR)
        curve = env.render(duration=0.3)
        assert curve[-1] < 0.05, "envelope should end near zero"


# ---------------------------------------------------------------------------
# 6-7: SynthFilter
# ---------------------------------------------------------------------------

class TestSynthFilter:
    """Tests for audio_engine.synthesizer.filter.SynthFilter."""

    def test_lowpass_attenuates_high_freqs(self):
        from audio_engine.synthesizer.filter import SynthFilter
        # Create a 10 kHz sine – should be attenuated by a 1 kHz low-pass
        t = np.arange(int(0.1 * SR), dtype=np.float64) / SR
        high_freq = np.sin(2.0 * np.pi * 10000.0 * t).astype(np.float32)
        filt = SynthFilter(sample_rate=SR, mode="lowpass", cutoff=1000.0)
        filtered = filt.process(high_freq)
        # Filtered signal should be quieter
        assert float(np.max(np.abs(filtered))) < float(np.max(np.abs(high_freq)))

    def test_output_same_length(self):
        from audio_engine.synthesizer.filter import SynthFilter
        n = int(0.1 * SR)
        signal = np.random.randn(n).astype(np.float32)
        filt = SynthFilter(sample_rate=SR, mode="highpass", cutoff=500.0)
        out = filt.process(signal)
        assert len(out) == n


# ---------------------------------------------------------------------------
# 8-9: Instrument
# ---------------------------------------------------------------------------

class TestInstrument:
    """Tests for audio_engine.synthesizer.instrument.Instrument."""

    def test_render_returns_float32(self):
        from audio_engine.synthesizer.instrument import Instrument
        inst = Instrument(waveform="sine", sample_rate=SR)
        audio = inst.render(frequency=440.0, duration=0.2)
        assert audio.dtype == np.float32

    def test_render_length(self):
        from audio_engine.synthesizer.instrument import Instrument
        inst = Instrument(sample_rate=SR)
        duration = 0.3
        audio = inst.render(frequency=440.0, duration=duration)
        expected = int(duration * SR)
        # Allow ±1 sample due to float rounding
        assert abs(len(audio) - expected) <= 1


# ---------------------------------------------------------------------------
# 10: InstrumentLibrary
# ---------------------------------------------------------------------------

class TestInstrumentLibrary:
    """Tests for audio_engine.synthesizer.instrument.InstrumentLibrary."""

    def test_presets_available(self):
        from audio_engine.synthesizer.instrument import InstrumentLibrary
        presets = InstrumentLibrary.available()
        assert "piano" in presets
        assert "strings" in presets
        assert "bass" in presets

    # ← counts as test 10; we use one method per test-class rule


# ---------------------------------------------------------------------------
# 11-12: AudioEngine façade
# ---------------------------------------------------------------------------

class TestAudioEngine:
    """Tests for audio_engine.engine.AudioEngine."""

    def test_generate_track_returns_array(self):
        from audio_engine import AudioEngine
        engine = AudioEngine(sample_rate=SR)
        audio = engine.generate_track("battle", bars=1)
        assert isinstance(audio, np.ndarray)
        assert audio.ndim == 2  # stereo

    def test_available_styles(self):
        from audio_engine import AudioEngine
        styles = AudioEngine.available_styles()
        assert "battle" in styles
        assert "exploration" in styles


# ---------------------------------------------------------------------------
# 13-14: AudioExporter
# ---------------------------------------------------------------------------

class TestAudioExporter:
    """Tests for audio_engine.export.audio_exporter.AudioExporter."""

    def test_export_wav_creates_file(self):
        from audio_engine.export.audio_exporter import AudioExporter
        exporter = AudioExporter(sample_rate=SR, bit_depth=16)
        audio = np.zeros((SR, 2), dtype=np.float32)
        with tempfile.TemporaryDirectory() as tmp:
            out = exporter.export(audio, Path(tmp) / "test.wav", fmt="wav")
            assert out.exists()
            assert out.stat().st_size > 44  # at least WAV header

    def test_export_mono_wav(self):
        from audio_engine.export.audio_exporter import AudioExporter
        exporter = AudioExporter(sample_rate=SR, bit_depth=16)
        audio = np.zeros(SR, dtype=np.float32)
        with tempfile.TemporaryDirectory() as tmp:
            out = exporter.export(audio, Path(tmp) / "mono.wav")
            assert out.exists()


# ---------------------------------------------------------------------------
# 15-16: DSP utilities
# ---------------------------------------------------------------------------

class TestDSP:
    """Tests for audio_engine.dsp modules."""

    def test_compressor_output_shape(self):
        from audio_engine.dsp.compressor import Compressor
        comp = Compressor(sample_rate=SR)
        signal = np.random.randn(int(0.2 * SR)).astype(np.float32)
        out = comp.process(signal)
        assert out.shape == signal.shape

    def test_reverb_output_length(self):
        from audio_engine.dsp.reverb import ConvolutionReverb
        reverb = ConvolutionReverb(sample_rate=SR, rt60=0.3, seed=0)
        signal = np.random.randn(int(0.2 * SR)).astype(np.float32)
        out = reverb.process(signal)
        assert len(out) == len(signal)


# ---------------------------------------------------------------------------
# 17-18: Integration – game_state_map
# ---------------------------------------------------------------------------

class TestGameStateMap:
    """Tests for audio_engine.integration.game_state_map."""

    def test_music_manifest_has_required_states(self):
        from audio_engine.integration.game_state_map import MUSIC_MANIFEST
        states = {a.game_state for a in MUSIC_MANIFEST}
        for required in ("exploring", "combat", "boss", "menu"):
            assert required in states, f"Missing music state: {required}"

    def test_sfx_manifest_filenames_are_wav(self):
        from audio_engine.integration.game_state_map import SFX_MANIFEST
        for asset in SFX_MANIFEST:
            assert asset.filename.endswith(".wav"), (
                f"SFX filename should end with .wav: {asset.filename}"
            )
