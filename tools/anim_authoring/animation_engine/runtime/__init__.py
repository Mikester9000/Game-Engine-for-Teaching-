"""
animation_engine.runtime – Runtime animation evaluator.

TEACHING NOTE — Runtime vs. Authoring
========================================
The authoring layer (model, io) is Python-only and runs at cook time.
The runtime layer mirrors what the C++ engine does at play time:
  - Load a skeleton + clip from disk.
  - For each frame, evaluate the clip at the current time t.
  - Return per-bone local transforms ready for skinning.

This Python runtime is useful for:
  1. Unit-testing the clip data before the C++ runtime is available.
  2. Offline rendering / preview tools.
  3. Teaching: students can study the algorithm in readable Python.

TEACHING NOTE — Animator Design
The Animator follows the same pattern as the C++ AnimationSystem:
  animator.load(skeleton, clip)
  transforms = animator.evaluate(time_seconds)
"""

from __future__ import annotations

from animation_engine.model import Skeleton, AnimClip, Keyframe, Vec3, Quat

__all__ = ["Animator"]


class Animator:
    """CPU animation evaluator.

    Evaluates an :class:`~animation_engine.model.AnimClip` at a given time
    and returns the resulting per-bone local transforms.

    Parameters
    ----------
    skeleton:
        The skeleton the clip was authored for.
    clip:
        The animation clip to evaluate.

    Example
    -------
    >>> animator = Animator(skeleton, walk_clip)
    >>> transforms = animator.evaluate(0.5)  # at t=0.5 seconds
    >>> bone0_translation = transforms[0]["translation"]
    """

    def __init__(self, skeleton: Skeleton, clip: AnimClip) -> None:
        self.skeleton = skeleton
        self.clip = clip
        self._channel_map: dict[int, int] = {
            ch.bone_index: i for i, ch in enumerate(clip.channels)
        }

    def evaluate(self, time: float) -> list[dict]:
        """Evaluate the clip at *time* seconds.

        Parameters
        ----------
        time:
            Playback time in seconds.  Clamped to [0, clip.duration_seconds].

        Returns
        -------
        list[dict]
            One dict per bone (in skeleton order) with keys
            ``"translation"``, ``"rotation"``, ``"scale"`` – each a list of
            floats in the shared schema format.
        """
        t = float(max(0.0, min(time, self.clip.duration_seconds)))

        results: list[dict] = []
        for bone in self.skeleton.bones:
            ch_idx = self._channel_map.get(bone.index)
            if ch_idx is not None:
                kf = self.clip.channels[ch_idx].sample(t)
            else:
                # No channel for this bone → use bind pose
                kf = Keyframe(
                    time=t,
                    translation=bone.bind_translation,
                    rotation=bone.bind_rotation,
                    scale=bone.bind_scale,
                )
            results.append({
                "boneName":    bone.name,
                "boneIndex":   bone.index,
                "translation": kf.translation.to_list(),
                "rotation":    kf.rotation.to_list(),
                "scale":       kf.scale.to_list(),
            })
        return results

    def evaluate_looping(self, time: float) -> list[dict]:
        """Evaluate with automatic looping (wraps *time* modulo clip duration).

        Parameters
        ----------
        time:
            Absolute playback time (may exceed clip duration).
        """
        dur = self.clip.duration_seconds
        looped_t = time % dur if dur > 1e-9 else 0.0
        return self.evaluate(looped_t)
