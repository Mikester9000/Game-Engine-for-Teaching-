"""
Core data models for the Animation Engine.

TEACHING NOTE — Data Models
============================
Before writing any algorithm, define the data it operates on.
Good data models:
  1. Are immutable where possible (fewer bugs from aliasing)
  2. Use named fields instead of index-based tuples (self-documenting)
  3. Have a __repr__ for easy debugging

The models here mirror the shared schemas in shared/schemas/:
  Skeleton ↔ skeleton.schema.json
  Clip     ↔ anim_clip.schema.json
"""

from __future__ import annotations

import math
import uuid
from dataclasses import dataclass, field
from typing import List, Optional


# ---------------------------------------------------------------------------
# Math primitives
# ---------------------------------------------------------------------------

@dataclass
class Vec3:
    """3D vector.

    TEACHING NOTE — Why a custom Vec3?
    We could use NumPy arrays, but for small data-model types a named
    dataclass with x/y/z fields is self-documenting and avoids array
    shape errors.  Performance-critical bulk operations use NumPy internally.
    """
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0

    def __add__(self, other: Vec3) -> Vec3:
        return Vec3(self.x + other.x, self.y + other.y, self.z + other.z)

    def __sub__(self, other: Vec3) -> Vec3:
        return Vec3(self.x - other.x, self.y - other.y, self.z - other.z)

    def __mul__(self, scalar: float) -> Vec3:
        return Vec3(self.x * scalar, self.y * scalar, self.z * scalar)

    def length(self) -> float:
        return math.sqrt(self.x**2 + self.y**2 + self.z**2)

    def normalized(self) -> Vec3:
        ln = self.length()
        if ln < 1e-9:
            return Vec3(0, 0, 1)
        return Vec3(self.x / ln, self.y / ln, self.z / ln)

    def dot(self, other: Vec3) -> float:
        return self.x * other.x + self.y * other.y + self.z * other.z

    def to_list(self) -> list:
        return [self.x, self.y, self.z]

    @staticmethod
    def from_list(lst: list) -> Vec3:
        return Vec3(float(lst[0]), float(lst[1]), float(lst[2]))


@dataclass
class Quat:
    """Unit quaternion for rotation.

    TEACHING NOTE — Why Quaternions?
    Euler angles (yaw/pitch/roll) suffer from "gimbal lock" — a singularity
    where two axes align and a degree of freedom is lost.
    Quaternions are a 4D representation that avoids this problem.
    They interpolate smoothly (SLERP), which is critical for animation.
    See: https://en.wikipedia.org/wiki/Quaternions_and_spatial_rotation
    """
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0
    w: float = 1.0  # Identity quaternion: no rotation

    def length(self) -> float:
        return math.sqrt(self.x**2 + self.y**2 + self.z**2 + self.w**2)

    def normalized(self) -> Quat:
        ln = self.length()
        if ln < 1e-9:
            return Quat(0, 0, 0, 1)
        return Quat(self.x/ln, self.y/ln, self.z/ln, self.w/ln)

    def slerp(self, other: Quat, t: float) -> Quat:
        """Spherical Linear Interpolation between two quaternions.

        TEACHING NOTE — SLERP
        SLERP interpolates on the 4D unit sphere, giving constant-speed,
        shortest-path rotation interpolation.  This is why animated bones
        don't "snap" between keyframes.
        """
        # Normalise inputs
        a = self.normalized()
        b = other.normalized()

        dot = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w
        # If dot < 0, negate b to ensure shortest path
        if dot < 0:
            b = Quat(-b.x, -b.y, -b.z, -b.w)
            dot = -dot

        dot = min(1.0, dot)
        theta = math.acos(dot)

        if abs(theta) < 1e-6:
            return a   # Quaternions are very close; return a
        sin_theta = math.sin(theta)

        fa = math.sin((1 - t) * theta) / sin_theta
        fb = math.sin(t * theta)       / sin_theta
        return Quat(fa*a.x + fb*b.x, fa*a.y + fb*b.y,
                    fa*a.z + fb*b.z, fa*a.w + fb*b.w).normalized()

    def to_list(self) -> list:
        return [self.x, self.y, self.z, self.w]

    @staticmethod
    def from_list(lst: list) -> Quat:
        return Quat(float(lst[0]), float(lst[1]), float(lst[2]), float(lst[3]))

    @staticmethod
    def identity() -> Quat:
        return Quat(0, 0, 0, 1)


# ---------------------------------------------------------------------------
# Skeleton (shared/schemas/skeleton.schema.json)
# ---------------------------------------------------------------------------

@dataclass
class Bone:
    """One bone in a skeleton hierarchy.

    TEACHING NOTE — Bone Hierarchy
    A skeleton is a tree of bones.  Each bone has a parent (except the root).
    Animations are stored in LOCAL space (relative to parent), but the engine
    needs WORLD space (relative to the origin) for skinning.
    World transform = Parent.WorldTransform × Local.Transform
    """
    index: int              
    name:  str              
    parent_index: int = -1  

    # Bind-pose (rest-pose) local transform
    bind_translation: Vec3 = field(default_factory=Vec3)
    bind_rotation:    Quat = field(default_factory=Quat.identity)
    bind_scale:       Vec3 = field(default_factory=lambda: Vec3(1, 1, 1))

    def to_dict(self) -> dict:
        """Serialise to shared schema format."""
        d: dict = {
            "index":       self.index,
            "name":        self.name,
            "parentIndex": self.parent_index,
            "bindPose": {
                "translation": self.bind_translation.to_list(),
                "rotation":    self.bind_rotation.to_list(),
                "scale":       self.bind_scale.to_list(),
            }
        }
        return d


@dataclass
class Skeleton:
    """Hierarchical bone rig.  Matches shared/schemas/skeleton.schema.json."""
    id:     str = field(default_factory=lambda: str(uuid.uuid4()))
    name:   str = "Skeleton"
    source: str = ""
    bones:  List[Bone] = field(default_factory=list)

    def add_bone(self, name: str, parent_index: int = -1) -> Bone:
        """Append a bone and return it."""
        bone = Bone(index=len(self.bones), name=name, parent_index=parent_index)
        self.bones.append(bone)
        return bone

    def find_bone(self, name: str) -> Optional[Bone]:
        """Find a bone by name (linear search — fine for <100 bones)."""
        for b in self.bones:
            if b.name == name:
                return b
        return None

    def to_dict(self) -> dict:
        """Serialise to shared schema format."""
        return {
            "$schema": "../../../shared/schemas/skeleton.schema.json",
            "version": "1.0.0",
            "id":      self.id,
            "name":    self.name,
            "source":  self.source,
            "bones":   [b.to_dict() for b in self.bones],
        }


# ---------------------------------------------------------------------------
# Animation Clip (shared/schemas/anim_clip.schema.json)
# ---------------------------------------------------------------------------

@dataclass
class Keyframe:
    """One keyframe for a single bone channel."""
    time:        float        
    translation: Vec3  = field(default_factory=Vec3)
    rotation:    Quat  = field(default_factory=Quat.identity)
    scale:       Vec3  = field(default_factory=lambda: Vec3(1, 1, 1))

    def to_dict(self) -> dict:
        return {
            "time":        self.time,
            "translation": self.translation.to_list(),
            "rotation":    self.rotation.to_list(),
            "scale":       self.scale.to_list(),
        }


@dataclass
class AnimChannel:
    """Animation data for one bone."""
    bone_index: int
    bone_name:  str
    keyframes:  List[Keyframe] = field(default_factory=list)

    def add_keyframe(self, time: float, translation: Vec3 = None,
                     rotation: Quat = None, scale: Vec3 = None) -> Keyframe:
        kf = Keyframe(
            time=time,
            translation=translation or Vec3(),
            rotation=rotation    or Quat.identity(),
            scale=scale          or Vec3(1, 1, 1),
        )
        self.keyframes.append(kf)
        self.keyframes.sort(key=lambda k: k.time)   # keep sorted
        return kf

    def sample(self, time: float) -> Keyframe:
        """Sample the channel at a given time using linear interpolation.

        TEACHING NOTE — Linear Interpolation vs. SLERP
        We lerp translations (straight-line blend) but slerp rotations
        (arc blend on unit quaternion sphere). Mixing these gives smooth,
        natural-looking motion.
        """
        if not self.keyframes:
            return Keyframe(time)

        # Clamp to first/last keyframe
        if time <= self.keyframes[0].time:
            return self.keyframes[0]
        if time >= self.keyframes[-1].time:
            return self.keyframes[-1]

        # Binary search for the surrounding keyframes
        lo, hi = 0, len(self.keyframes) - 1
        while lo < hi - 1:
            mid = (lo + hi) // 2
            if self.keyframes[mid].time <= time:
                lo = mid
            else:
                hi = mid

        kf_a = self.keyframes[lo]
        kf_b = self.keyframes[hi]
        dt   = kf_b.time - kf_a.time
        t    = (time - kf_a.time) / dt if dt > 1e-9 else 0.0

        # Lerp translation and scale; slerp rotation
        trans = kf_a.translation * (1 - t) + kf_b.translation * t
        rot   = kf_a.rotation.slerp(kf_b.rotation, t)
        sc    = kf_a.scale * (1 - t) + kf_b.scale * t

        return Keyframe(time=time, translation=trans, rotation=rot, scale=sc)

    def to_dict(self) -> dict:
        return {
            "boneIndex": self.bone_index,
            "boneName":  self.bone_name,
            "keyframes": [kf.to_dict() for kf in self.keyframes],
        }


@dataclass
class AnimClip:
    """An animation clip.  Matches shared/schemas/anim_clip.schema.json."""
    id:               str   = field(default_factory=lambda: str(uuid.uuid4()))
    name:             str   = "Clip"
    skeleton_id:      str   = ""
    duration_seconds: float = 1.0
    fps:              float = 30.0
    loopable:         bool  = False
    root_motion:      bool  = False
    channels:         List[AnimChannel] = field(default_factory=list)

    def add_channel(self, bone_index: int, bone_name: str) -> AnimChannel:
        ch = AnimChannel(bone_index=bone_index, bone_name=bone_name)
        self.channels.append(ch)
        return ch

    def to_dict(self) -> dict:
        return {
            "$schema":         "../../../shared/schemas/anim_clip.schema.json",
            "version":         "1.0.0",
            "id":              self.id,
            "name":            self.name,
            "skeletonId":      self.skeleton_id,
            "durationSeconds": self.duration_seconds,
            "fps":             self.fps,
            "loopable":        self.loopable,
            "rootMotion":      self.root_motion,
            "channels":        [ch.to_dict() for ch in self.channels],
        }
