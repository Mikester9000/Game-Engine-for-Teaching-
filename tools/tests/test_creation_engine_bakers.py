#!/usr/bin/env python3
"""
test_creation_engine_bakers.py — Tests for M21 baker commands in creation_engine.py.

TEACHING NOTE — Baker test strategy
-----------------------------------
For file-output bakers we verify binary contracts directly:
  1) output file exists,
  2) header fields are correct,
  3) payload size matches expected fixed-size layouts.
This keeps tests stable while still enforcing the runtime-facing format.
"""

from __future__ import annotations

import json
import struct
import sys
from pathlib import Path


_TESTS_DIR = Path(__file__).resolve().parent
_TOOLS_DIR = _TESTS_DIR.parent
if str(_TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(_TOOLS_DIR))

import creation_engine as ce  # noqa: E402


def test_bake_navmesh_writes_expected_binary_layout(tmp_path: Path) -> None:
    """Baking a simple plane OBJ writes NVM1 header + grid payload."""
    obj_path = tmp_path / "plane.obj"
    obj_path.write_text(
        "\n".join(
            [
                "v 0 0 0",
                "v 1 0 0",
                "v 1 0 1",
                "v 0 0 1",
                "f 1 2 3",
                "f 1 3 4",
            ]
        ),
        encoding="utf-8",
    )

    out_path = tmp_path / "cooked" / "ai" / "plane.navmesh"
    stats = ce.bake_navmesh(obj_path, out_path, grid_width=8, grid_height=4)

    blob = out_path.read_bytes()
    header_size = struct.calcsize("<4sHHHffff")
    assert len(blob) == header_size + (8 * 4)

    magic, version, width, height, min_x, max_x, min_z, max_z = struct.unpack(
        "<4sHHHffff", blob[:header_size]
    )
    assert magic == b"NVM1"
    assert version == 1
    assert (width, height) == (8, 4)
    assert min_x == 0.0
    assert max_x == 1.0
    assert min_z == 0.0
    assert max_z == 1.0
    assert stats["totalCells"] == 32
    assert stats["walkableCells"] > 0
    assert any(v != 0 for v in blob[header_size:])


def test_bake_navmesh_rejects_obj_without_faces(tmp_path: Path) -> None:
    """An OBJ with no faces should fail validation."""
    obj_path = tmp_path / "invalid.obj"
    obj_path.write_text("v 0 0 0\nv 1 0 0\n", encoding="utf-8")
    out_path = tmp_path / "bad.navmesh"

    try:
        ce.bake_navmesh(obj_path, out_path)
    except ValueError as exc:
        assert "no faces" in str(exc).lower()
    else:
        raise AssertionError("Expected bake_navmesh to raise ValueError")


def test_bake_tod_writes_expected_binary_layout(tmp_path: Path) -> None:
    """Baking ToD keys writes TDL1 header + RGBA payload with interpolation."""
    tod_path = tmp_path / "tod.json"
    tod_path.write_text(
        json.dumps(
            {
                "version": "1.0.0",
                "keys": [
                    {"time": 0.0, "color": [0.0, 0.0, 0.0, 1.0]},
                    {"time": 1.0, "color": [1.0, 0.5, 0.0, 1.0]},
                ],
            },
            indent=2,
        ),
        encoding="utf-8",
    )

    out_path = tmp_path / "cooked" / "environment" / "tod.lut"
    stats = ce.bake_tod(tod_path, out_path, sample_count=8)

    blob = out_path.read_bytes()
    header_size = struct.calcsize("<4sH")
    assert len(blob) == header_size + (8 * 4)
    magic, samples = struct.unpack("<4sH", blob[:header_size])
    assert magic == b"TDL1"
    assert samples == 8
    assert stats["samples"] == 8
    assert stats["bytes"] == 32

    first = blob[header_size:header_size + 4]
    last = blob[-4:]
    assert first == bytes([0, 0, 0, 255])
    assert last == bytes([255, 128, 0, 255])


def test_bake_tod_rejects_missing_keys(tmp_path: Path) -> None:
    """A ToD file without a keys array should fail."""
    bad = tmp_path / "bad_tod.json"
    bad.write_text(json.dumps({"version": "1.0.0"}, indent=2), encoding="utf-8")
    out_path = tmp_path / "bad.lut"

    try:
        ce.bake_tod(bad, out_path)
    except ValueError as exc:
        assert "keys" in str(exc).lower()
    else:
        raise AssertionError("Expected bake_tod to raise ValueError")


def test_bake_cinematic_writes_expected_binary_layout(tmp_path: Path) -> None:
    """Baking cinematic JSON writes CIN1 header and encoded shot payload."""
    src = tmp_path / "intro.cinematic.json"
    src.write_text(
        json.dumps(
            {
                "version": "1.0.0",
                "id": "intro_cutscene",
                "shots": [
                    {
                        "label": "intro_pan",
                        "duration": 3.0,
                        "keyframes": [
                            {
                                "time": 0.0,
                                "position": [0.0, 5.0, -10.0],
                                "lookAt": [0.0, 1.5, 0.0],
                                "fov": 55.0,
                            },
                            {
                                "time": 3.0,
                                "position": [4.0, 4.0, -6.0],
                                "lookAt": [0.0, 1.2, 1.0],
                                "fov": 50.0,
                            },
                        ],
                        "audioEvents": [
                            {"time": 0.5, "event": "play_sfx", "clipID": "whoosh_01"}
                        ],
                    }
                ],
            },
            indent=2,
        ),
        encoding="utf-8",
    )

    out_path = tmp_path / "cooked" / "cinematics" / "intro.cinematic"
    stats = ce.bake_cinematic(src, out_path)

    blob = out_path.read_bytes()
    header_size = struct.calcsize("<4sHHHH")
    assert len(blob) > header_size
    magic, version, shots, total_keyframes, total_events = struct.unpack(
        "<4sHHHH", blob[:header_size]
    )
    assert magic == b"CIN1"
    assert version == 1
    assert shots == 1
    assert total_keyframes == 2
    assert total_events == 1
    assert stats["shots"] == 1
    assert stats["keyframes"] == 2
    assert stats["audioEvents"] == 1
    assert stats["bytes"] == len(blob) - header_size


def test_bake_cinematic_rejects_invalid_keyframe_time(tmp_path: Path) -> None:
    """A keyframe time beyond shot duration should fail validation."""
    bad = tmp_path / "bad.cinematic.json"
    bad.write_text(
        json.dumps(
            {
                "version": "1.0.0",
                "id": "bad_cutscene",
                "shots": [
                    {
                        "label": "bad_shot",
                        "duration": 1.0,
                        "keyframes": [
                            {
                                "time": 2.0,
                                "position": [0.0, 0.0, 0.0],
                                "lookAt": [0.0, 0.0, 1.0],
                                "fov": 60.0,
                            }
                        ],
                    }
                ],
            },
            indent=2,
        ),
        encoding="utf-8",
    )

    out_path = tmp_path / "bad.cinematic"
    try:
        ce.bake_cinematic(bad, out_path)
    except ValueError as exc:
        assert "duration" in str(exc).lower()
    else:
        raise AssertionError("Expected bake_cinematic to raise ValueError")


# ---------------------------------------------------------------------------
# M25 — bake_terrain tests
# ---------------------------------------------------------------------------


def _make_terrain_json(tmp_path: Path, *, width: int = 4, height: int = 4,
                       cell_size: float = 2.0,
                       heights: list | None = None) -> Path:
    """Helper: write a minimal terrain JSON and return its path."""
    if heights is None:
        heights = [float(r + c) * 0.5 for r in range(height) for c in range(width)]
    data = {
        "width": width,
        "height": height,
        "cellSize": cell_size,
        "heights": heights,
    }
    p = tmp_path / "test.terrain.json"
    p.write_text(json.dumps(data, indent=2), encoding="utf-8")
    return p


def test_bake_terrain_writes_expected_binary_layout(tmp_path: Path) -> None:
    """
    TEACHING NOTE — TRN1 binary layout contract test
    ─────────────────────────────────────────────────
    Verify the header fields and total size of a baked terrain asset.

    Layout:
        4s  magic ("TRN1")
        H   version  = 1
        H   width
        H   height
        f   cellSize
        N*4 height floats (N = width * height)
    Total: 14 + N*4 bytes.
    """
    src = _make_terrain_json(tmp_path, width=4, height=4, cell_size=2.0)
    out = tmp_path / "cooked" / "terrain" / "test.terrain"

    stats = ce.bake_terrain(src, out)

    blob = out.read_bytes()
    header_size = struct.calcsize("<4sHHHf")
    expected_total = header_size + 4 * 4 * 4  # 14 + 64 = 78 bytes

    assert len(blob) == expected_total, f"Expected {expected_total} bytes, got {len(blob)}"

    magic, version, w, h, cs = struct.unpack("<4sHHHf", blob[:header_size])
    assert magic == b"TRN1", f"Magic mismatch: {magic!r}"
    assert version == 1
    assert w == 4
    assert h == 4
    assert abs(cs - 2.0) < 1e-5

    assert stats["width"] == 4
    assert stats["height"] == 4
    assert abs(stats["cellSize"] - 2.0) < 1e-5
    assert stats["bytes"] == expected_total


def test_bake_terrain_height_samples_roundtrip(tmp_path: Path) -> None:
    """
    TEACHING NOTE — Height sample fidelity test
    ────────────────────────────────────────────
    Every height value stored in the JSON must be retrievable from the binary
    as a float32 with < 1e-4 absolute error.  This confirms that:
      1. The heights array is read in row-major order.
      2. No value is discarded or reordered during serialisation.
      3. float32 precision is sufficient for terrain heights (true for any
         value that fits in a 32-bit float, which all reasonable heights do).
    """
    known_heights = [0.0, 1.0, 2.0, 3.0,
                     0.5, 1.5, 2.5, 3.5,
                     1.0, 2.0, 3.0, 4.0,
                     1.5, 2.5, 3.5, 4.5]
    src = _make_terrain_json(tmp_path, width=4, height=4,
                             cell_size=1.0, heights=known_heights)
    out = tmp_path / "roundtrip.terrain"
    ce.bake_terrain(src, out)

    blob = out.read_bytes()
    header_size = struct.calcsize("<4sHHHf")
    payload = blob[header_size:]

    num_samples = 4 * 4
    unpacked = struct.unpack(f"<{num_samples}f", payload)
    for i, (expected, actual) in enumerate(zip(known_heights, unpacked)):
        assert abs(actual - expected) < 1e-4, (
            f"Height mismatch at index {i}: expected {expected}, got {actual}"
        )


def test_bake_terrain_rejects_mismatched_heights_array(tmp_path: Path) -> None:
    """
    TEACHING NOTE — Validation: heights array length mismatch
    ──────────────────────────────────────────────────────────
    The baker must raise ValueError if len(heights) != width * height.
    This prevents silent data corruption (partial height data would produce
    incorrect mesh geometry that is hard to debug visually).
    """
    bad_json = tmp_path / "bad.terrain.json"
    bad_json.write_text(
        json.dumps({
            "width": 4,
            "height": 4,
            "cellSize": 2.0,
            "heights": [0.0, 1.0, 2.0],  # only 3 values instead of 16
        }),
        encoding="utf-8",
    )
    out = tmp_path / "bad.terrain"
    try:
        ce.bake_terrain(bad_json, out)
    except ValueError as exc:
        assert "16" in str(exc) or "3" in str(exc)
    else:
        raise AssertionError("Expected bake_terrain to raise ValueError")


def test_bake_terrain_rejects_too_small_grid(tmp_path: Path) -> None:
    """
    TEACHING NOTE — Validation: minimum grid size
    ──────────────────────────────────────────────
    A 1×4 or 1×1 grid cannot form a single triangle quad; the baker must
    reject width or height < 2 with a clear error.
    """
    bad_json = tmp_path / "tiny.terrain.json"
    bad_json.write_text(
        json.dumps({
            "width": 1,
            "height": 4,
            "cellSize": 1.0,
            "heights": [0.0, 1.0, 2.0, 3.0],
        }),
        encoding="utf-8",
    )
    out = tmp_path / "tiny.terrain"
    try:
        ce.bake_terrain(bad_json, out)
    except ValueError as exc:
        assert "width" in str(exc).lower() or "height" in str(exc).lower() or "2" in str(exc)
    else:
        raise AssertionError("Expected bake_terrain to raise ValueError for width=1")


def test_bake_terrain_creates_output_directories(tmp_path: Path) -> None:
    """
    TEACHING NOTE — Output directory auto-creation
    ───────────────────────────────────────────────
    The baker must create missing parent directories so callers do not need to
    run mkdir before calling bake_terrain.  This matches the convention used by
    all other bakers (bake_navmesh, bake_tod, bake_cinematic).
    """
    src = _make_terrain_json(tmp_path)
    deep_out = tmp_path / "a" / "b" / "c" / "sample.terrain"

    ce.bake_terrain(src, deep_out)

    assert deep_out.exists(), "bake_terrain must create intermediate directories"
    assert deep_out.stat().st_size > 0


def test_bake_terrain_roundtrips_sample_vertical_slice_asset() -> None:
    """
    TEACHING NOTE — Sample vertical slice asset contract test
    ──────────────────────────────────────────────────────────
    Verify that the committed highland.terrain.json in the vertical slice
    project bakes to the correct TRN1 format.  This acts as a golden-file
    sanity check: if the JSON or the baker change in an incompatible way
    this test will fail, alerting reviewers to the regression.

    The test loads the ACTUAL cooked binary from the repo and verifies:
      1. Magic == "TRN1"
      2. Version == 1
      3. Grid dimensions and cell size match the authored JSON source
      4. Sample count in payload equals width * height
      5. All height values are non-negative (terrain stays above the XZ plane)
    """
    repo_root = Path(__file__).resolve().parents[2]
    cooked_path = (
        repo_root
        / "samples" / "vertical_slice_project"
        / "Cooked" / "Terrain" / "highland.terrain"
    )
    source_path = (
        repo_root
        / "samples" / "vertical_slice_project"
        / "Content" / "Terrain" / "highland.terrain.json"
    )
    if not cooked_path.exists():
        import pytest
        pytest.skip(f"Cooked terrain asset not found: {cooked_path}")
    if not source_path.exists():
        import pytest
        pytest.skip(f"Source terrain asset not found: {source_path}")

    source = json.loads(source_path.read_text(encoding="utf-8"))
    expected_w = int(source["width"])
    expected_h = int(source["height"])
    expected_cs = float(source["cellSize"])

    blob = cooked_path.read_bytes()
    header_size = struct.calcsize("<4sHHHf")
    magic, version, w, h, cs = struct.unpack("<4sHHHf", blob[:header_size])

    assert magic == b"TRN1"
    assert version == 1
    assert w == expected_w
    assert h == expected_h
    assert abs(cs - expected_cs) < 1e-5

    num_samples = w * h
    heights = struct.unpack(f"<{num_samples}f", blob[header_size:])
    assert len(heights) == expected_w * expected_h
    assert all(v >= 0.0 for v in heights), "All height samples must be >= 0"
