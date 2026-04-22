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
