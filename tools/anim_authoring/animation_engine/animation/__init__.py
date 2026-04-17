"""
animation_engine.animation – Animation clip and channel types.

TEACHING NOTE — Package Layering
==================================
The ``animation`` sub-package contains the pure data types for animation:
  - Keyframe   – a snapshot of a bone's transform at one point in time
  - AnimChannel – all keyframes for one bone
  - AnimClip   – a complete named animation (collection of channels)

These are re-exported from animation_engine.model for convenience:
    from animation_engine.animation import AnimClip, AnimChannel, Keyframe
"""

from animation_engine.model import Keyframe, AnimChannel, AnimClip

__all__ = ["Keyframe", "AnimChannel", "AnimClip"]
