# Audio Authoring Tool

Vendored from [Mikester9000/Audio-Engine](https://github.com/Mikester9000/Audio-Engine)
into the monorepo under `tools/audio_authoring/`.

## What this does

Produces cooked audio artifacts (`.bank` files) from source WAV/OGG files
and AI-generated music/SFX, using the shared `audio_bank.schema.json` format.

The engine (C++) loads the cooked banks at runtime. Python is the compiler
(authoring/cooking); C++ is the runtime (playback).

## Install

```bat
cd tools\audio_authoring
pip install -r requirements.txt

:: Optional: OGG export
pip install soundfile
```

## Quick start

```python
from audio_engine import AudioEngine

engine = AudioEngine()

# Generate a battle music track
engine.generate_track("battle", bars=8, output_path="battle.wav")

# Generate from a text prompt
engine.generate_music("dark ambient dungeon loop 90 BPM",
                      output_path="dungeon.wav", loopable=True)

# Generate SFX
engine.generate_sfx_from_prompt("sword slash", output_path="slash.wav")
```

## Cook integration

The `samples/vertical_slice_project/cook_assets.py` script calls this
tool automatically during the cook step.

## Package structure

```
audio_engine/
├── __init__.py      # Package entry; exports AudioEngine
├── engine.py        # AudioEngine façade (main API)
├── ai/              # AI-powered generators (MusicGen, SFXGen, VoiceGen)
├── composer/        # Sequencer, Note — procedural composition
├── dsp/             # Filters, reverb, compressor
├── export/          # AudioExporter (WAV, OGG)
├── synthesizer/     # InstrumentLibrary (wavetable)
├── render/          # OfflineBounce (mastering)
└── qa/              # LoudnessMeter, ClippingDetector, LoopAnalyzer
```

## Shared schema

`shared/schemas/audio_bank.schema.json` — defines the `.bank` format
consumed by the engine.
