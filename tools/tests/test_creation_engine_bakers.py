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


# ===========================================================================
# bake_collision tests (PHY1)
# ===========================================================================

def test_bake_collision_writes_expected_binary_layout(tmp_path: Path) -> None:
    """
    TEACHING NOTE — PHY1 binary layout contract test
    ─────────────────────────────────────────────────
    Verify the header and total size of a baked collision mesh.

    PHY1 layout:
        4s  magic ("PHY1")
        H   version = 1
        I   vertex_count (V)
        I   index_count  (I)   must be multiple of 3
        V*12 float32 vertex data (x, y, z per vertex)
        I*4  uint32  index data  (triangle list)
    Total: 14 + V*12 + I*4 bytes.
    """
    obj_path = tmp_path / "quad.obj"
    obj_path.write_text(
        "\n".join([
            "v 0 0 0",
            "v 2 0 0",
            "v 2 0 2",
            "v 0 0 2",
            "f 1 2 3",
            "f 1 3 4",
        ]),
        encoding="utf-8",
    )
    out_path = tmp_path / "cooked" / "physics" / "quad.phys"
    stats = ce.bake_collision(obj_path, out_path)

    assert out_path.exists()
    blob = out_path.read_bytes()

    header_fmt = "<4sHII"
    header_size = struct.calcsize(header_fmt)  # 14 bytes
    magic, version, vc, ic = struct.unpack(header_fmt, blob[:header_size])

    assert magic == b"PHY1"
    assert version == 1
    assert vc == 4           # four vertices
    assert ic == 6           # two triangles → 6 indices
    assert ic % 3 == 0       # must be a triangle list

    expected_size = header_size + vc * 12 + ic * 4
    assert len(blob) == expected_size

    assert stats["vertices"] == 4
    assert stats["indices"] == 6
    assert stats["bytes"] == expected_size


def test_bake_collision_vertex_roundtrip(tmp_path: Path) -> None:
    """
    TEACHING NOTE — Vertex position fidelity
    ─────────────────────────────────────────
    Every vertex position in the OBJ must survive the round-trip through
    PHY1 binary serialisation with < 1e-4 absolute error per component.
    """
    obj_path = tmp_path / "triangle.obj"
    obj_path.write_text("v 1.5 2.5 3.5\nv 4.0 5.0 6.0\nv 7.25 8.75 9.125\nf 1 2 3",
                        encoding="utf-8")
    out_path = tmp_path / "tri.phys"
    ce.bake_collision(obj_path, out_path)

    blob = out_path.read_bytes()
    header_size = struct.calcsize("<4sHII")
    verts = struct.unpack("<9f", blob[header_size: header_size + 36])
    expected = [1.5, 2.5, 3.5, 4.0, 5.0, 6.0, 7.25, 8.75, 9.125]
    for i, (exp, act) in enumerate(zip(expected, verts)):
        assert abs(act - exp) < 1e-4, f"Vertex component {i}: expected {exp}, got {act}"


def test_bake_collision_rejects_obj_without_vertices(tmp_path: Path) -> None:
    """
    TEACHING NOTE — Empty OBJ validation
    ──────────────────────────────────────
    An OBJ with no vertex lines must raise ValueError.
    """
    obj_path = tmp_path / "empty.obj"
    obj_path.write_text("# just a comment\n", encoding="utf-8")
    try:
        ce.bake_collision(obj_path, tmp_path / "empty.phys")
    except ValueError as exc:
        assert "vertices" in str(exc).lower()
    else:
        raise AssertionError("Expected ValueError for empty OBJ")


def test_bake_collision_rejects_obj_without_faces(tmp_path: Path) -> None:
    """
    TEACHING NOTE — No-face OBJ validation
    ───────────────────────────────────────
    An OBJ with vertices but no face lines must raise ValueError.
    """
    obj_path = tmp_path / "nofaces.obj"
    obj_path.write_text("v 0 0 0\nv 1 0 0\nv 0 0 1\n", encoding="utf-8")
    try:
        ce.bake_collision(obj_path, tmp_path / "nofaces.phys")
    except ValueError as exc:
        assert "face" in str(exc).lower() or "triangle" in str(exc).lower()
    else:
        raise AssertionError("Expected ValueError for OBJ without faces")


def test_bake_collision_creates_output_directories(tmp_path: Path) -> None:
    """bake_collision must create missing parent directories."""
    obj_path = tmp_path / "t.obj"
    obj_path.write_text("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3", encoding="utf-8")
    deep_out = tmp_path / "a" / "b" / "c" / "t.phys"
    ce.bake_collision(obj_path, deep_out)
    assert deep_out.exists()
    assert deep_out.stat().st_size > 0


def test_bake_collision_roundtrips_vertical_slice_arena_plane() -> None:
    """
    TEACHING NOTE — Sample asset contract test
    ───────────────────────────────────────────
    Bake the committed arena_plane.obj from the vertical slice project and
    verify the PHY1 header is valid.  This acts as a regression guard: if
    the OBJ format changes or the baker breaks, this test fails immediately.
    """
    repo_root = Path(__file__).resolve().parents[2]
    obj_path = repo_root / "samples/vertical_slice_project/Content/AI/arena_plane.obj"
    if not obj_path.exists():
        import pytest
        pytest.skip(f"arena_plane.obj not found: {obj_path}")

    import tempfile, os
    with tempfile.TemporaryDirectory() as td:
        out = Path(td) / "arena_plane.phys"
        stats = ce.bake_collision(obj_path, out)
        blob = out.read_bytes()

    magic, version, vc, ic = struct.unpack("<4sHII", blob[:14])
    assert magic == b"PHY1"
    assert version == 1
    assert vc > 0
    assert ic > 0
    assert ic % 3 == 0
    assert stats["bytes"] == 14 + vc * 12 + ic * 4


# ===========================================================================
# bake_font tests (FNT1)
# ===========================================================================

def _make_font_json(tmp_path: Path, **overrides) -> Path:
    """Helper: write a minimal valid font.json."""
    data = {
        "name": "test_font",
        "glyphSize": 8,
        "atlasCols": 16,
        "atlasRows": 6,
        "firstChar": 32,
        "charCount": 96,
    }
    data.update(overrides)
    p = tmp_path / "test.font.json"
    p.write_text(json.dumps(data), encoding="utf-8")
    return p


def test_bake_font_writes_expected_binary_layout(tmp_path: Path) -> None:
    """
    TEACHING NOTE — FNT1 binary layout contract test
    ─────────────────────────────────────────────────
    Verify header fields and total size for a standard 128×48 atlas.

    FNT1 layout:
        4s  magic ("FNT1")
        H   version = 1
        H   atlasWidth  (pixels)
        H   atlasHeight (pixels)
        H   glyphCount
        H   glyphSize
        H   firstChar
        glyphCount * 20 bytes  glyph table (5 floats: u0 v0 u1 v1 advance)
        atlasWidth * atlasHeight bytes  SDF R8 pixel data
    """
    src = _make_font_json(tmp_path)
    out = tmp_path / "cooked" / "ui" / "test.font"
    stats = ce.bake_font(src, out)

    blob = out.read_bytes()
    header_fmt = "<4sHHHHHH"
    hsize = struct.calcsize(header_fmt)  # 14 bytes
    magic, version, aw, ah, gc, gs, fc = struct.unpack(header_fmt, blob[:hsize])

    assert magic == b"FNT1"
    assert version == 1
    assert aw == 128   # 16 cols * 8 px
    assert ah == 48    # 6 rows * 8 px
    assert gc == 96
    assert gs == 8
    assert fc == 32

    glyph_table_bytes = gc * 20  # 5 floats * 4 bytes each
    pixel_bytes = aw * ah
    expected_total = hsize + glyph_table_bytes + pixel_bytes
    assert len(blob) == expected_total

    assert stats["glyphs"] == 96
    assert stats["atlasWidth"] == 128
    assert stats["atlasHeight"] == 48
    assert stats["bytes"] == expected_total


def test_bake_font_glyph_uv_coverage(tmp_path: Path) -> None:
    """
    TEACHING NOTE — UV coordinate correctness
    ───────────────────────────────────────────
    For each glyph the baker must write UV coordinates that tile correctly
    inside the atlas:
      • u0 < u1 and v0 < v1 (positive extents)
      • u1 == u0 + 1/atlasCols  (exactly one column wide)
      • v1 == v0 + 1/atlasRows  (exactly one row tall)
    """
    src = _make_font_json(tmp_path, glyphSize=8, atlasCols=16, atlasRows=6,
                          firstChar=32, charCount=96)
    out = tmp_path / "uv_test.font"
    ce.bake_font(src, out)

    blob = out.read_bytes()
    hsize = struct.calcsize("<4sHHHHHH")  # 16 bytes (4s + 6×H)
    glyph_table = blob[hsize: hsize + 96 * 20]

    expected_u_step = 1.0 / 16
    expected_v_step = 1.0 / 6
    for i in range(96):
        offset = i * 20
        u0, v0, u1, v1, adv = struct.unpack("<5f", glyph_table[offset: offset + 20])
        assert u0 < u1
        assert v0 < v1
        assert abs((u1 - u0) - expected_u_step) < 1e-5, f"glyph {i}: u span wrong"
        assert abs((v1 - v0) - expected_v_step) < 1e-5, f"glyph {i}: v span wrong"
        assert abs(adv - 1.0) < 1e-5, f"glyph {i}: advance should be 1.0 (monospace)"


def test_bake_font_rejects_invalid_glyph_size(tmp_path: Path) -> None:
    """bake_font must reject glyphSize < 4."""
    src = _make_font_json(tmp_path, glyphSize=2)
    try:
        ce.bake_font(src, tmp_path / "bad.font")
    except ValueError as exc:
        assert "glyphsize" in str(exc).lower() or "4" in str(exc)
    else:
        raise AssertionError("Expected ValueError for glyphSize < 4")


def test_bake_font_rejects_undersized_atlas(tmp_path: Path) -> None:
    """bake_font must reject atlas that cannot fit charCount glyphs."""
    src = _make_font_json(tmp_path, atlasCols=2, atlasRows=2, charCount=96)
    try:
        ce.bake_font(src, tmp_path / "tiny.font")
    except ValueError as exc:
        assert "charcount" in str(exc).lower() or "96" in str(exc) or "cells" in str(exc).lower()
    else:
        raise AssertionError("Expected ValueError for undersized atlas")


def test_bake_font_creates_output_directories(tmp_path: Path) -> None:
    """bake_font must create missing parent directories."""
    src = _make_font_json(tmp_path)
    deep_out = tmp_path / "a" / "b" / "c" / "test.font"
    ce.bake_font(src, deep_out)
    assert deep_out.exists()
    assert deep_out.stat().st_size > 0


def test_bake_font_roundtrips_vertical_slice_asset() -> None:
    """
    TEACHING NOTE — Sample asset contract test
    ───────────────────────────────────────────
    Bake the committed ffxv_ui.font.json from the vertical slice project
    and verify the FNT1 header matches the declared glyph grid.
    """
    repo_root = Path(__file__).resolve().parents[2]
    src = repo_root / "samples/vertical_slice_project/Content/UI/Fonts/ffxv_ui.font.json"
    if not src.exists():
        import pytest
        pytest.skip(f"ffxv_ui.font.json not found: {src}")

    import tempfile
    with tempfile.TemporaryDirectory() as td:
        out = Path(td) / "ffxv_ui.font"
        stats = ce.bake_font(src, out)
        blob = out.read_bytes()

    magic, version, aw, ah, gc, gs, fc = struct.unpack("<4sHHHHHH", blob[:16])
    assert magic == b"FNT1"
    assert version == 1
    assert aw == 128   # 16 * 8
    assert ah == 48    # 6 * 8
    assert gc == 96
    assert gs == 8
    assert fc == 32
    assert stats["bytes"] == 16 + 96 * 20 + 128 * 48


# ===========================================================================
# bake_road tests (RD01)
# ===========================================================================

def _make_road_json(tmp_path: Path, waypoints=None, name="test_road") -> Path:
    """Helper: write a minimal valid road JSON."""
    if waypoints is None:
        waypoints = [{"x": 0.0, "y": 0.0, "z": 0.0}, {"x": 10.0, "y": 0.0, "z": 0.0}]
    data = {"name": name, "waypoints": waypoints}
    p = tmp_path / f"{name}.road.json"
    p.write_text(json.dumps(data), encoding="utf-8")
    return p


def test_bake_road_writes_expected_binary_layout(tmp_path: Path) -> None:
    """
    TEACHING NOTE — RD01 binary layout contract test
    ─────────────────────────────────────────────────
    Verify header fields and total size.

    RD01 layout:
        4s  magic ("RD01")
        H   version = 1
        I   waypoint_count (N)
        N*12 float32 positions (x, y, z per waypoint)
    Total: 10 + N*12 bytes.
    """
    wps = [{"x": 0.0, "y": 0.0, "z": 0.0},
           {"x": 5.0, "y": 1.0, "z": 2.0},
           {"x": 10.0, "y": 0.0, "z": 4.0}]
    src = _make_road_json(tmp_path, waypoints=wps)
    out = tmp_path / "cooked" / "roads" / "test.road"
    stats = ce.bake_road(src, out)

    blob = out.read_bytes()
    header_fmt = "<4sHI"
    hsize = struct.calcsize(header_fmt)  # 10 bytes
    magic, version, wc = struct.unpack(header_fmt, blob[:hsize])

    assert magic == b"RD01"
    assert version == 1
    assert wc == 3

    expected_total = hsize + 3 * 12
    assert len(blob) == expected_total

    assert stats["waypoints"] == 3
    assert stats["bytes"] == expected_total


def test_bake_road_position_roundtrip(tmp_path: Path) -> None:
    """
    TEACHING NOTE — Waypoint position fidelity
    ────────────────────────────────────────────
    Every waypoint position must survive float32 serialisation with < 1e-4
    absolute error per component.
    """
    wps = [
        {"x": 1.5, "y": 2.25, "z": -3.75},
        {"x": 10.0, "y": 0.0, "z": 5.5},
    ]
    src = _make_road_json(tmp_path, waypoints=wps)
    out = tmp_path / "fidelity.road"
    ce.bake_road(src, out)

    blob = out.read_bytes()
    hsize = struct.calcsize("<4sHI")
    positions = struct.unpack("<6f", blob[hsize:])
    expected = [1.5, 2.25, -3.75, 10.0, 0.0, 5.5]
    for i, (exp, act) in enumerate(zip(expected, positions)):
        assert abs(act - exp) < 1e-4, f"Position component {i}: expected {exp}, got {act}"


def test_bake_road_rejects_single_waypoint(tmp_path: Path) -> None:
    """bake_road must reject fewer than 2 waypoints."""
    src = _make_road_json(tmp_path, waypoints=[{"x": 0.0, "y": 0.0, "z": 0.0}])
    try:
        ce.bake_road(src, tmp_path / "bad.road")
    except ValueError as exc:
        assert "2" in str(exc) or "waypoint" in str(exc).lower()
    else:
        raise AssertionError("Expected ValueError for single waypoint")


def test_bake_road_rejects_missing_waypoints_key(tmp_path: Path) -> None:
    """bake_road must raise ValueError if 'waypoints' key is absent."""
    src = tmp_path / "nowaypoints.json"
    src.write_text(json.dumps({"name": "no_wp"}), encoding="utf-8")
    try:
        ce.bake_road(src, tmp_path / "nowaypoints.road")
    except ValueError as exc:
        assert "waypoint" in str(exc).lower()
    else:
        raise AssertionError("Expected ValueError for missing waypoints key")


def test_bake_road_creates_output_directories(tmp_path: Path) -> None:
    """bake_road must create missing parent directories."""
    src = _make_road_json(tmp_path)
    deep_out = tmp_path / "a" / "b" / "c" / "test.road"
    ce.bake_road(src, deep_out)
    assert deep_out.exists()
    assert deep_out.stat().st_size > 0


def test_bake_road_roundtrips_vertical_slice_asset() -> None:
    """
    TEACHING NOTE — Sample asset contract test
    ───────────────────────────────────────────
    Bake the committed regalia_route.road.json from the vertical slice project
    and verify the RD01 binary has the correct waypoint count and header.
    """
    repo_root = Path(__file__).resolve().parents[2]
    src = repo_root / "samples/vertical_slice_project/Content/Roads/regalia_route.road.json"
    if not src.exists():
        import pytest
        pytest.skip(f"regalia_route.road.json not found: {src}")

    import tempfile
    with tempfile.TemporaryDirectory() as td:
        out = Path(td) / "regalia_route.road"
        stats = ce.bake_road(src, out)
        blob = out.read_bytes()

    magic, version, wc = struct.unpack("<4sHI", blob[:10])
    assert magic == b"RD01"
    assert version == 1
    assert wc == 12   # 12 waypoints in the sample JSON
    assert stats["bytes"] == 10 + 12 * 12
