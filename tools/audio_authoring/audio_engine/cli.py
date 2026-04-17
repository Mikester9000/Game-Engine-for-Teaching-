"""
Command-line interface for the Audio Engine.

Usage examples
--------------
Generate a battle track (WAV):
    audio-engine generate --style battle --bars 8 --output battle.wav

Generate music from a prompt:
    audio-engine generate-music --prompt "dark ambient dungeon 90 BPM" --duration 60 --loop --output dungeon.wav

Generate a sound effect from a prompt:
    audio-engine generate-sfx --prompt "explosion impact" --duration 1.5 --output boom.wav

Generate a voice line:
    audio-engine generate-voice --text "Welcome, hero." --voice narrator --output hero_greeting.wav

Run quality assurance checks on a WAV file:
    audio-engine qa --input track.wav

Generate ALL assets for the Game Engine for Teaching:
    audio-engine generate-game-assets --output-dir ./assets/audio

Verify that all game assets are present:
    audio-engine verify-game-assets --assets-dir ./assets/audio

List available styles and instruments:
    audio-engine list-styles
    audio-engine list-instruments

Render a quick SFX chord:
    audio-engine sfx --instrument piano --notes 261.63 329.63 392.0 --output chord.wav
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


def _cmd_generate(args: argparse.Namespace) -> None:
    from audio_engine import AudioEngine

    engine = AudioEngine(sample_rate=args.sample_rate, seed=args.seed)
    print(f"Generating '{args.style}' track ({args.bars} bars) → {args.output} …")
    audio = engine.generate_track(
        style=args.style,
        bars=args.bars,
        output_path=args.output,
        fmt=args.format,
    )
    duration = len(audio) / args.sample_rate
    print(f"Done. Duration: {duration:.2f}s  |  Samples: {len(audio):,}")


def _cmd_sfx(args: argparse.Namespace) -> None:
    from audio_engine import AudioEngine

    engine = AudioEngine(sample_rate=args.sample_rate)
    print(
        f"Rendering SFX: instrument='{args.instrument}' "
        f"notes={args.notes} duration={args.duration}s → {args.output}"
    )
    audio = engine.render_sfx(
        instrument_name=args.instrument,
        frequencies=[float(n) for n in args.notes],
        duration=args.duration,
        overlap=args.overlap,
    )
    engine.export(audio, args.output, fmt="wav")
    print(f"Done.  {len(audio)} samples written.")


def _cmd_generate_music(args: argparse.Namespace) -> None:
    """Generate music from a text prompt using the AI pipeline."""
    from audio_engine.ai import MusicGen

    gen = MusicGen(sample_rate=args.sample_rate, seed=args.seed)
    print(f"Generating music: '{args.prompt}' → {args.output} …")
    path = gen.generate_to_file(
        prompt=args.prompt,
        output_path=args.output,
        duration=args.duration,
        loopable=args.loop,
        fmt=args.format,
    )
    print(f"Done. Saved to: {path}")


def _cmd_generate_sfx(args: argparse.Namespace) -> None:
    """Generate a sound effect from a text prompt."""
    from audio_engine.ai import SFXGen

    gen = SFXGen(sample_rate=args.sample_rate, seed=args.seed)
    print(f"Generating SFX: '{args.prompt}' → {args.output} …")
    pitch = float(args.pitch) if args.pitch is not None else None
    path = gen.generate_to_file(
        prompt=args.prompt,
        output_path=args.output,
        duration=args.duration,
        pitch_hz=pitch,
    )
    print(f"Done. Saved to: {path}")


def _cmd_generate_voice(args: argparse.Namespace) -> None:
    """Generate voice/TTS audio from text."""
    from audio_engine.ai import VoiceGen

    gen = VoiceGen(sample_rate=args.sample_rate)
    print(f"Synthesising voice: '{args.text}' (voice={args.voice}) → {args.output} …")
    path = gen.generate_to_file(
        text=args.text,
        output_path=args.output,
        voice=args.voice,
        speed=args.speed,
    )
    print(f"Done. Saved to: {path}")


def _cmd_qa(args: argparse.Namespace) -> None:
    """Run quality-assurance checks on a WAV file."""
    import wave

    import numpy as np

    from audio_engine.qa import LoudnessMeter, ClippingDetector, LoopAnalyzer

    input_path = Path(args.input)
    if not input_path.exists():
        print(f"Error: file not found: {input_path}", file=sys.stderr)
        raise FileNotFoundError(f"file not found: {input_path}")

    # Load audio
    with wave.open(str(input_path), "r") as wf:
        n_channels = wf.getnchannels()
        sampwidth = wf.getsampwidth()
        sr = wf.getframerate()
        n_frames = wf.getnframes()
        raw = wf.readframes(n_frames)

    dtype = {"1": "int8", "2": "int16", "3": "int32", "4": "int32"}.get(str(sampwidth), "int16")
    scale = {1: 128.0, 2: 32768.0, 3: 8388608.0, 4: 2147483648.0}.get(sampwidth, 32768.0)
    samples = np.frombuffer(raw, dtype=np.dtype(dtype)).astype(np.float32) / scale

    if n_channels > 1:
        audio = samples.reshape(-1, n_channels)
    else:
        audio = samples

    print(f"\nQA Report: {input_path.name}")
    print(f"  Sample rate : {sr} Hz")
    print(f"  Channels    : {n_channels}")
    print(f"  Duration    : {n_frames / sr:.2f}s  ({n_frames:,} frames)")
    print()

    # Loudness
    meter = LoudnessMeter(sample_rate=sr)
    result = meter.measure(audio)
    print(f"  Integrated loudness : {result.integrated_lufs:.1f} LUFS")
    print(f"  True peak           : {result.true_peak_dbfs:.1f} dBFS")
    print(f"  Loudness range      : {result.loudness_range_lu:.1f} LU")

    # Clipping
    detector = ClippingDetector()
    clip_report = detector.detect(audio)
    print(f"  Clipping            : {clip_report.summary()}")

    # Loop analysis
    if args.check_loop:
        analyzer = LoopAnalyzer(sample_rate=sr)
        loop_report = analyzer.analyze(audio)
        print(f"  Loop boundary       : {loop_report.summary()}")

    print()
    issues = []
    if result.integrated_lufs > -9.0:
        issues.append(f"Loudness too high ({result.integrated_lufs:.1f} LUFS; target ≤ -9 LUFS)")
    if result.integrated_lufs < -30.0:
        issues.append(f"Loudness too low ({result.integrated_lufs:.1f} LUFS; target ≥ -30 LUFS)")
    if result.true_peak_dbfs > -0.1:
        issues.append(f"True peak too high ({result.true_peak_dbfs:.1f} dBFS; should be ≤ -0.1 dBFS)")
    if clip_report.has_clipping:
        issues.append(clip_report.summary())

    if issues:
        print("  ⚠  Issues found:")
        for issue in issues:
            print(f"     • {issue}")
    else:
        print("  ✓  All checks passed.")


def _cmd_list_styles(_args: argparse.Namespace) -> None:
    from audio_engine import AudioEngine

    styles = AudioEngine.available_styles()
    print("Available styles:")
    for s in styles:
        print(f"  {s}")


def _cmd_list_instruments(_args: argparse.Namespace) -> None:
    from audio_engine import AudioEngine

    instruments = AudioEngine.available_instruments()
    print("Available instruments:")
    for i in instruments:
        print(f"  {i}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="audio-engine",
        description="Audio Engine – produce AI-assisted music, SFX, and voice.",
    )
    sub = parser.add_subparsers(dest="command", required=True)

    # --- generate (legacy style-based) ---
    gen = sub.add_parser("generate", help="Generate a music track using the AI composer.")
    gen.add_argument(
        "--style",
        default="battle",
        help="Music style: battle, exploration, ambient, boss, victory, menu. (default: battle)",
    )
    gen.add_argument("--bars", type=int, default=None, help="Number of bars (default: style default)")
    gen.add_argument("--output", "-o", default="output.wav", help="Output file path.")
    gen.add_argument(
        "--format",
        choices=["wav", "ogg"],
        default="wav",
        help="Output format (default: wav).",
    )
    gen.add_argument(
        "--sample-rate", type=int, default=44100, help="Sample rate in Hz (default: 44100)."
    )
    gen.add_argument("--seed", type=int, default=None, help="Random seed for reproducibility.")

    # --- generate-music (prompt-driven) ---
    gm = sub.add_parser(
        "generate-music",
        help="Generate music from a natural-language prompt.",
    )
    gm.add_argument(
        "--prompt", "-p", required=True,
        help='Natural-language prompt, e.g. "epic battle theme 140 BPM loopable".',
    )
    gm.add_argument("--duration", type=float, default=30.0, help="Duration in seconds (default: 30).")
    gm.add_argument("--loop", action="store_true", help="Generate a seamless loop.")
    gm.add_argument("--output", "-o", default="music.wav", help="Output file path.")
    gm.add_argument(
        "--format",
        choices=["wav", "ogg"],
        default="wav",
        help="Output format (default: wav).",
    )
    gm.add_argument("--sample-rate", type=int, default=44100, help="Sample rate in Hz.")
    gm.add_argument("--seed", type=int, default=None, help="Random seed.")

    # --- generate-sfx ---
    gs = sub.add_parser(
        "generate-sfx",
        help="Generate a sound effect from a natural-language prompt.",
    )
    gs.add_argument(
        "--prompt", "-p", required=True,
        help='Description, e.g. "explosion impact 1.5 seconds".',
    )
    gs.add_argument("--duration", type=float, default=1.0, help="Duration in seconds (default: 1).")
    gs.add_argument("--pitch", type=float, default=None, help="Base pitch in Hz (optional).")
    gs.add_argument("--output", "-o", default="sfx.wav", help="Output WAV file.")
    gs.add_argument("--sample-rate", type=int, default=44100, help="Sample rate in Hz.")
    gs.add_argument("--seed", type=int, default=None, help="Random seed.")

    # --- generate-voice ---
    gv = sub.add_parser(
        "generate-voice",
        help="Synthesise speech from text using local TTS.",
    )
    gv.add_argument("--text", "-t", required=True, help="Text to speak.")
    gv.add_argument(
        "--voice",
        default="narrator",
        choices=["narrator", "hero", "villain", "announcer", "npc"],
        help="Voice preset (default: narrator).",
    )
    gv.add_argument("--speed", type=float, default=1.0, help="Speech rate multiplier (default: 1.0).")
    gv.add_argument("--output", "-o", default="voice.wav", help="Output WAV file.")
    gv.add_argument("--sample-rate", type=int, default=22050, help="Sample rate in Hz (default: 22050).")

    # --- qa ---
    qa = sub.add_parser(
        "qa",
        help="Run quality-assurance checks on a WAV file.",
    )
    qa.add_argument("--input", "-i", required=True, help="Path to the WAV file to check.")
    qa.add_argument(
        "--check-loop",
        action="store_true",
        help="Also check for loop boundary click artefacts.",
    )

    # --- sfx (instrument-based) ---
    sfx = sub.add_parser("sfx", help="Render a quick sound effect using an instrument.")
    sfx.add_argument("--instrument", default="piano", help="Instrument name.")
    sfx.add_argument(
        "--notes",
        nargs="+",
        default=["440.0"],
        metavar="FREQ",
        help="One or more frequencies in Hz.",
    )
    sfx.add_argument(
        "--duration", type=float, default=0.5, help="Duration per note in seconds (default: 0.5)."
    )
    sfx.add_argument(
        "--overlap",
        action="store_true",
        help="Play all notes simultaneously (chord) instead of in sequence.",
    )
    sfx.add_argument("--output", "-o", default="sfx.wav", help="Output WAV file.")
    sfx.add_argument("--sample-rate", type=int, default=44100, help="Sample rate in Hz.")

    # --- list-styles ---
    sub.add_parser("list-styles", help="List available music generation styles.")

    # --- list-instruments ---
    sub.add_parser("list-instruments", help="List available synthesised instruments.")

    # --- generate-game-assets ---
    gga = sub.add_parser(
        "generate-game-assets",
        help="Pre-generate ALL audio assets for the Game Engine for Teaching.",
    )
    gga.add_argument(
        "--output-dir", "-o", default="assets/audio",
        help="Root output directory (default: assets/audio).",
    )
    gga.add_argument(
        "--only",
        choices=["music", "sfx", "voice", "all"],
        default="all",
        help="Generate only a subset of assets (default: all).",
    )
    gga.add_argument("--sample-rate", type=int, default=44100, help="Sample rate in Hz.")
    gga.add_argument("--seed", type=int, default=None, help="Random seed.")
    gga.add_argument(
        "--force", action="store_true",
        help="Regenerate assets even if they already exist.",
    )
    gga.add_argument(
        "--quiet", action="store_true",
        help="Suppress per-asset progress messages.",
    )

    # --- verify-game-assets ---
    vga = sub.add_parser(
        "verify-game-assets",
        help="Check that all expected game audio assets are present on disk.",
    )
    vga.add_argument(
        "--assets-dir", "-d", default="assets/audio",
        help="Root audio assets directory to verify (default: assets/audio).",
    )

    return parser


def _cmd_generate_game_assets(args: argparse.Namespace) -> None:
    """Generate the complete audio asset library for the Game Engine for Teaching."""
    from audio_engine.integration.asset_pipeline import AssetPipeline

    def _progress(msg: str) -> None:
        if not args.quiet:
            print(msg)

    pipeline = AssetPipeline(
        sample_rate=args.sample_rate,
        seed=args.seed,
        progress_callback=_progress,
        skip_existing=not args.force,
    )

    print(f"Generating game audio assets → {args.output_dir}")

    if args.only == "music":
        manifest = pipeline.generate_music_only(args.output_dir)
    elif args.only == "sfx":
        manifest = pipeline.generate_sfx_only(args.output_dir)
    elif args.only == "voice":
        manifest = pipeline.generate_voice_only(args.output_dir)
    else:
        manifest = pipeline.generate_all(args.output_dir)

    print()
    print(manifest.summary())
    if manifest.errors:
        return  # errors already printed in summary


def _cmd_verify_game_assets(args: argparse.Namespace) -> None:
    """Verify that all required game audio assets are present."""
    from audio_engine.integration.asset_pipeline import AssetPipeline
    from pathlib import Path

    assets_dir = Path(args.assets_dir)
    if not assets_dir.exists():
        raise FileNotFoundError(f"assets directory not found: {assets_dir}")

    pipeline = AssetPipeline()
    report = pipeline.verify(assets_dir)

    n_present = len(report["present"])
    n_missing = len(report["missing"])
    n_total = n_present + n_missing

    print(f"Verifying game assets: {assets_dir}")
    print(f"  Present : {n_present}/{n_total}")
    print(f"  Missing : {n_missing}/{n_total}")

    if report["missing"]:
        print("\n  Missing files:")
        for f in report["missing"]:
            print(f"    • {f}")
        print(
            "\n  Run:  audio-engine generate-game-assets"
            f" --output-dir {args.assets_dir}"
        )
        raise FileNotFoundError(f"{n_missing} asset(s) missing")
    else:
        print("  ✓  All assets present.")


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    dispatch = {
        "generate": _cmd_generate,
        "generate-music": _cmd_generate_music,
        "generate-sfx": _cmd_generate_sfx,
        "generate-voice": _cmd_generate_voice,
        "qa": _cmd_qa,
        "sfx": _cmd_sfx,
        "list-styles": _cmd_list_styles,
        "list-instruments": _cmd_list_instruments,
        "generate-game-assets": _cmd_generate_game_assets,
        "verify-game-assets": _cmd_verify_game_assets,
    }

    handler = dispatch.get(args.command)
    if handler is None:  # pragma: no cover
        parser.print_help()
        return 1

    try:
        handler(args)
        return 0
    except (KeyError, ValueError, FileNotFoundError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1
    except Exception as exc:  # pragma: no cover – surface unexpected errors
        print(f"Unexpected error: {exc}", file=sys.stderr)
        raise


if __name__ == "__main__":
    sys.exit(main())
