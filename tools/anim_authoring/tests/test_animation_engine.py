"""Test suite for the Animation Authoring tool.

5 tests covering:
  - Model: Skeleton and Bone construction
  - Model: AnimClip and channel sampling (interpolation)
  - IO: round-trip export + import for skeleton and clip
  - Runtime: Animator.evaluate
  - Integration: AnimAssetPipeline cook_all
"""

import json
import math
import tempfile
from pathlib import Path

import pytest


# ---------------------------------------------------------------------------
# 1: Skeleton model
# ---------------------------------------------------------------------------

class TestSkeletonModel:
    """Tests for animation_engine.model.Skeleton / Bone."""

    def test_add_and_find_bone(self):
        from animation_engine.model import Skeleton
        skel = Skeleton(name="TestSkeleton")
        root = skel.add_bone("root")
        spine = skel.add_bone("spine", parent_index=root.index)
        assert skel.find_bone("root") is root
        assert skel.find_bone("spine") is spine
        assert skel.find_bone("missing") is None

    def test_skeleton_serialises_to_dict(self):
        from animation_engine.model import Skeleton
        skel = Skeleton(name="Human")
        skel.add_bone("root")
        data = skel.to_dict()
        assert data["name"] == "Human"
        assert len(data["bones"]) == 1
        assert data["bones"][0]["name"] == "root"


# ---------------------------------------------------------------------------
# 2: AnimClip sampling / interpolation
# ---------------------------------------------------------------------------

class TestAnimClipSampling:
    """Tests for AnimChannel.sample (LERP / SLERP interpolation)."""

    def test_channel_sample_clamps_to_endpoints(self):
        from animation_engine.model import AnimClip, Vec3, Quat

        clip = AnimClip(name="Walk", duration_seconds=1.0)
        ch = clip.add_channel(0, "root")
        ch.add_keyframe(0.0, translation=Vec3(0, 0, 0))
        ch.add_keyframe(1.0, translation=Vec3(10, 0, 0))

        kf_start = ch.sample(-1.0)
        kf_end   = ch.sample(2.0)
        assert kf_start.translation.x == pytest.approx(0.0)
        assert kf_end.translation.x   == pytest.approx(10.0)

    def test_channel_midpoint_interpolation(self):
        from animation_engine.model import AnimClip, Vec3

        clip = AnimClip(name="Test", duration_seconds=1.0)
        ch = clip.add_channel(0, "root")
        ch.add_keyframe(0.0, translation=Vec3(0, 0, 0))
        ch.add_keyframe(1.0, translation=Vec3(10, 0, 0))

        kf_mid = ch.sample(0.5)
        assert kf_mid.translation.x == pytest.approx(5.0, abs=0.01)


# ---------------------------------------------------------------------------
# 3: IO round-trip
# ---------------------------------------------------------------------------

class TestIOExporterImporter:
    """Tests for animation_engine.io.Exporter and Importer round-trips."""

    def test_skeleton_round_trip(self):
        from animation_engine.model import Skeleton
        from animation_engine.io import Exporter, Importer

        skel = Skeleton(name="Round-trip Skeleton")
        skel.add_bone("root")
        skel.add_bone("spine", parent_index=0)

        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "test.skelc"
            Exporter.export_skeleton(skel, path)
            loaded = Importer.import_skeleton(path)

        assert loaded.name == skel.name
        assert len(loaded.bones) == 2
        assert loaded.bones[0].name == "root"
        assert loaded.bones[1].parent_index == 0

    def test_clip_round_trip(self):
        from animation_engine.model import AnimClip, Vec3
        from animation_engine.io import Exporter, Importer

        clip = AnimClip(name="Walk", duration_seconds=2.0, fps=30.0)
        ch = clip.add_channel(0, "root")
        ch.add_keyframe(0.0, translation=Vec3(0, 0, 0))
        ch.add_keyframe(2.0, translation=Vec3(5, 0, 0))

        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "walk.animc"
            Exporter.export_clip(clip, path)
            loaded = Importer.import_clip(path)

        assert loaded.name == "Walk"
        assert loaded.duration_seconds == pytest.approx(2.0)
        assert len(loaded.channels) == 1
        assert len(loaded.channels[0].keyframes) == 2


# ---------------------------------------------------------------------------
# 4: Runtime Animator
# ---------------------------------------------------------------------------

class TestAnimator:
    """Tests for animation_engine.runtime.Animator."""

    def _make_skeleton_and_clip(self):
        from animation_engine.model import Skeleton, AnimClip, Vec3

        skel = Skeleton(name="TestSkel")
        root = skel.add_bone("root")

        clip = AnimClip(name="TestClip", duration_seconds=1.0)
        ch = clip.add_channel(root.index, "root")
        ch.add_keyframe(0.0, translation=Vec3(0, 0, 0))
        ch.add_keyframe(1.0, translation=Vec3(10, 0, 0))
        return skel, clip

    def test_evaluate_returns_one_entry_per_bone(self):
        from animation_engine.runtime import Animator

        skel, clip = self._make_skeleton_and_clip()
        animator = Animator(skel, clip)
        result = animator.evaluate(0.5)
        assert len(result) == len(skel.bones)

    def test_evaluate_looping_wraps(self):
        from animation_engine.runtime import Animator

        skel, clip = self._make_skeleton_and_clip()
        animator = Animator(skel, clip)
        # t=0.5 and t=1.5 should give same result (clip duration=1.0)
        r1 = animator.evaluate(0.5)
        r2 = animator.evaluate_looping(1.5)
        assert r1[0]["translation"] == pytest.approx(r2[0]["translation"], abs=1e-4)


# ---------------------------------------------------------------------------
# 5: Integration pipeline
# ---------------------------------------------------------------------------

class TestAnimAssetPipeline:
    """Tests for animation_engine.integration.AnimAssetPipeline."""

    def _write_clip_json(self, path: Path) -> None:
        data = {
            "$schema": "../../../shared/schemas/anim_clip.schema.json",
            "version": "1.0.0",
            "id": "test-clip",
            "name": "TestClip",
            "skeletonId": "",
            "durationSeconds": 1.0,
            "fps": 30.0,
            "loopable": False,
            "rootMotion": False,
            "channels": [],
        }
        path.write_text(json.dumps(data), encoding="utf-8")

    def test_cook_all_produces_cooked_files(self):
        from animation_engine.integration import AnimAssetPipeline

        with tempfile.TemporaryDirectory() as tmp:
            content = Path(tmp) / "Content"
            cooked  = Path(tmp) / "Cooked"
            content.mkdir()

            self._write_clip_json(content / "test_clip.json")

            pipeline = AnimAssetPipeline(skip_existing=False)
            manifest = pipeline.cook_all(content, cooked)

            assert len(manifest.errors) == 0, f"Cook errors: {manifest.errors}"
            assert len(manifest.clips) == 1
            cooked_file = Path(manifest.clips[0]["cooked"])
            assert cooked_file.exists()
