"""
Animation Exporter / Importer.

TEACHING NOTE — Serialization
================================
Serialization = converting in-memory objects to a storable format (JSON, binary).
Deserialization = the reverse.

We use JSON as our "source" format (human-readable, easy to version-control)
and plan a compact binary ".animc" format as our "cooked" format (fast runtime
loading with no text parsing overhead).

Source:  Content/Animations/*.json  (human-readable, authored here)
Cooked:  Cooked/Anim/*.animc         (compact binary, loaded by engine)

The cook_assets.py script calls Exporter.export_clip() for each source file.
"""

from __future__ import annotations

import json
from pathlib import Path

from animation_engine.model import AnimClip, Skeleton


class Exporter:
    """Export animation data to JSON (source) or stub binary (.animc).

    TEACHING NOTE — Single Responsibility Principle
    The Exporter does ONE thing: write data to files.  It doesn't know how
    to create or edit the data — that's the caller's job.  This separation
    makes the exporter easy to test and easy to swap out.
    """

    @staticmethod
    def export_skeleton(skeleton: Skeleton, path: str | Path) -> Path:
        """Export a skeleton to a JSON file.

        The format matches shared/schemas/skeleton.schema.json.
        """
        output = Path(path)
        output.parent.mkdir(parents=True, exist_ok=True)
        data = json.dumps(skeleton.to_dict(), indent=2)
        output.write_text(data, encoding="utf-8")
        return output

    @staticmethod
    def export_clip(clip: AnimClip, path: str | Path) -> Path:
        """Export an animation clip to a JSON file.

        The format matches shared/schemas/anim_clip.schema.json.
        """
        output = Path(path)
        output.parent.mkdir(parents=True, exist_ok=True)
        data = json.dumps(clip.to_dict(), indent=2)
        output.write_text(data, encoding="utf-8")
        return output

    @staticmethod
    def export_clip_binary(clip: AnimClip, path: str | Path) -> Path:
        """Export an animation clip to a compact .animc binary (STUB).

        TEACHING NOTE — Binary Formats
        JSON is great for authoring (human-readable, diffable) but slow
        to parse at runtime.  A binary format is faster to load because:
          1. No text parsing needed.
          2. Data is already in the correct memory layout.
          3. Smaller file size (floats as 4 bytes, not 10-20 chars).

        This stub just writes JSON with a .animc extension.
        Milestone 2 will add real binary packing.
        """
        output = Path(path).with_suffix(".animc")
        return Exporter.export_clip(clip, output)


class Importer:
    """Import animation data from JSON files.

    TEACHING NOTE — Round-Trip Testing
    A good Exporter+Importer pair should satisfy:
        import(export(obj)) == obj
    This is called a "round-trip" test and is a great first test to write
    for any serialization system.
    """

    @staticmethod
    def import_skeleton(path: str | Path) -> Skeleton:
        """Load a skeleton from a JSON file."""
        from animation_engine.model import Bone, Vec3, Quat

        data = json.loads(Path(path).read_text(encoding="utf-8"))
        skel = Skeleton(
            id=data.get("id", ""),
            name=data.get("name", "Skeleton"),
            source=data.get("source", ""),
        )
        for b in data.get("bones", []):
            bp = b.get("bindPose", {})
            bone = Bone(
                index=b["index"],
                name=b["name"],
                parent_index=b.get("parentIndex", -1),
                bind_translation=Vec3.from_list(bp.get("translation", [0,0,0])),
                bind_rotation=Quat.from_list(bp.get("rotation", [0,0,0,1])),
                bind_scale=Vec3.from_list(bp.get("scale", [1,1,1])),
            )
            skel.bones.append(bone)
        return skel

    @staticmethod
    def import_clip(path: str | Path) -> AnimClip:
        """Load an animation clip from a JSON file."""
        from animation_engine.model import AnimChannel, Keyframe, Vec3, Quat

        data = json.loads(Path(path).read_text(encoding="utf-8"))
        clip = AnimClip(
            id=data.get("id", ""),
            name=data.get("name", "Clip"),
            skeleton_id=data.get("skeletonId", ""),
            duration_seconds=data.get("durationSeconds", 1.0),
            fps=data.get("fps", 30.0),
            loopable=data.get("loopable", False),
            root_motion=data.get("rootMotion", False),
        )
        for ch_data in data.get("channels", []):
            ch = AnimChannel(
                bone_index=ch_data["boneIndex"],
                bone_name=ch_data.get("boneName", ""),
            )
            for kf_data in ch_data.get("keyframes", []):
                kf = Keyframe(
                    time=kf_data["time"],
                    translation=Vec3.from_list(kf_data.get("translation", [0,0,0])),
                    rotation=Quat.from_list(kf_data.get("rotation", [0,0,0,1])),
                    scale=Vec3.from_list(kf_data.get("scale", [1,1,1])),
                )
                ch.keyframes.append(kf)
            clip.channels.append(ch)
        return clip
