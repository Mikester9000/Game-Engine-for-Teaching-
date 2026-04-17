"""
tests/test_animation_engine.py — Tests for the animation_engine package.

Covers the five core scenarios from the anim_authoring integration:
  1. Model types: Vec3, Quat, Bone, Skeleton, AnimClip
  2. Skeleton construction and bone hierarchy
  3. AnimClip channel sampling (interpolation)
  4. Exporter / Importer round-trip (JSON)
  5. Exporter binary stub (.animc)

TEACHING NOTE — Why test round-trips?
A serialization round-trip test (export → import → compare) is one of the
most valuable tests you can write. It exercises both the exporter and
importer in one shot, and catches any field-name mismatches between the two.
"""

import json
import math
import tempfile
from pathlib import Path

import pytest

from animation_engine.model import (
    Vec3,
    Quat,
    Bone,
    Skeleton,
    AnimClip,
    AnimChannel,
    Keyframe,
)
from animation_engine.io import Exporter, Importer


# ===========================================================================
# 1. Model primitives
# ===========================================================================

def test_vec3_arithmetic():
    """Vec3 supports +, -, * and length / normalize."""
    a = Vec3(1.0, 0.0, 0.0)
    b = Vec3(0.0, 1.0, 0.0)
    assert (a + b) == Vec3(1.0, 1.0, 0.0)
    assert (a - b) == Vec3(1.0, -1.0, 0.0)
    assert (a * 2.0) == Vec3(2.0, 0.0, 0.0)
    assert abs(a.length() - 1.0) < 1e-6
    n = Vec3(3.0, 4.0, 0.0).normalized()
    assert abs(n.length() - 1.0) < 1e-6


def test_quat_slerp_identity():
    """Slerping two identical quaternions at any t returns the same quaternion."""
    q = Quat.identity()
    result = q.slerp(q, 0.5)
    assert abs(result.w - 1.0) < 1e-5


def test_quat_slerp_halfway():
    """Slerp at t=0.5 between identity and a 90° rotation yields a unit quaternion."""
    q0 = Quat.identity()
    # 90° rotation around Y axis: Quat(x=0, y=sin(45°), z=0, w=cos(45°))
    half = math.sin(math.radians(45))
    q1 = Quat(0.0, half, 0.0, half)
    mid = q0.slerp(q1, 0.5)
    # Length should be 1 (unit quaternion)
    assert abs(mid.length() - 1.0) < 1e-5


# ===========================================================================
# 2. Skeleton construction
# ===========================================================================

def test_skeleton_add_bone():
    """add_bone appends a bone and assigns indices in order."""
    skel = Skeleton(name="TestSkeleton")
    root = skel.add_bone("root")
    spine = skel.add_bone("spine_01", parent_index=root.index)

    assert root.index == 0
    assert spine.index == 1
    assert spine.parent_index == root.index
    assert len(skel.bones) == 2


def test_skeleton_find_bone():
    """find_bone returns the correct bone by name, None for unknown."""
    skel = Skeleton(name="TestSkeleton")
    skel.add_bone("root")
    skel.add_bone("hip")

    assert skel.find_bone("hip") is not None
    assert skel.find_bone("hip").name == "hip"
    assert skel.find_bone("nonexistent") is None


def test_skeleton_to_dict_has_required_fields():
    """Skeleton.to_dict() includes the shared schema fields."""
    skel = Skeleton(name="S")
    skel.add_bone("root")
    d = skel.to_dict()
    assert "version" in d
    assert "id" in d
    assert "name" in d
    assert "bones" in d
    assert len(d["bones"]) == 1


# ===========================================================================
# 3. AnimClip channel sampling
# ===========================================================================

def test_anim_clip_sample_clamp_start():
    """Sampling before first keyframe returns the first keyframe."""
    skel = Skeleton(name="S")
    root = skel.add_bone("root")
    clip = AnimClip(name="Idle", skeleton_id=skel.id, duration_seconds=2.0)
    ch = clip.add_channel(root.index, "root")
    ch.add_keyframe(0.5, translation=Vec3(0, 0, 0))
    ch.add_keyframe(1.5, translation=Vec3(0, 1, 0))

    sampled = ch.sample(0.0)  # before first keyframe → clamp to first
    assert sampled.translation == Vec3(0, 0, 0)


def test_anim_clip_sample_midpoint():
    """Sampling halfway between two keyframes returns an interpolated value."""
    skel = Skeleton(name="S")
    root = skel.add_bone("root")
    clip = AnimClip(name="Move", skeleton_id=skel.id, duration_seconds=2.0)
    ch = clip.add_channel(root.index, "root")
    ch.add_keyframe(0.0, translation=Vec3(0, 0, 0))
    ch.add_keyframe(1.0, translation=Vec3(2, 0, 0))

    sampled = ch.sample(0.5)
    assert abs(sampled.translation.x - 1.0) < 1e-5


# ===========================================================================
# 4. Exporter / Importer round-trip
# ===========================================================================

def test_skeleton_export_import_round_trip(tmp_path):
    """Exporting and importing a Skeleton preserves all fields."""
    skel = Skeleton(name="RoundTrip")
    root = skel.add_bone("root")
    hip = skel.add_bone("hip", parent_index=root.index)
    hip.bind_translation = Vec3(0, 1, 0)

    out = tmp_path / "skeleton.json"
    Exporter.export_skeleton(skel, out)
    assert out.exists()

    loaded = Importer.import_skeleton(out)
    assert loaded.name == skel.name
    assert loaded.id == skel.id
    assert len(loaded.bones) == len(skel.bones)
    assert loaded.find_bone("hip").bind_translation == Vec3(0, 1, 0)


def test_clip_export_import_round_trip(tmp_path):
    """Exporting and importing an AnimClip preserves channels and keyframes."""
    skel = Skeleton(name="S")
    root = skel.add_bone("root")
    clip = AnimClip(name="Idle", skeleton_id=skel.id, duration_seconds=2.0, loopable=True)
    ch = clip.add_channel(root.index, "root")
    ch.add_keyframe(0.0, translation=Vec3(0, 0, 0))
    ch.add_keyframe(2.0, translation=Vec3(0, 0, 0))

    out = tmp_path / "idle.animc"
    Exporter.export_clip(clip, out)
    assert out.exists()

    loaded = Importer.import_clip(out)
    assert loaded.name == clip.name
    assert loaded.loopable == clip.loopable
    assert loaded.duration_seconds == clip.duration_seconds
    assert len(loaded.channels) == 1
    assert len(loaded.channels[0].keyframes) == 2


# ===========================================================================
# 5. Binary stub exporter
# ===========================================================================

def test_export_clip_binary_creates_animc(tmp_path):
    """export_clip_binary() creates a .animc file."""
    clip = AnimClip(name="BinaryTest", duration_seconds=1.0)
    out = tmp_path / "test.animc"
    result = Exporter.export_clip_binary(clip, out)
    assert result.suffix == ".animc"
    assert result.exists()
    assert result.stat().st_size > 0
