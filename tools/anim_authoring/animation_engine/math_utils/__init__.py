"""
animation_engine.math_utils – Math primitives for animation.

TEACHING NOTE — Why separate math_utils?
==========================================
Game math (vectors, quaternions, matrices) is used across many sub-systems.
Putting it in a dedicated module makes it importable without pulling in the
full model layer. This follows the dependency inversion principle.

Re-exports the core math types from animation_engine.model for convenience:
    from animation_engine.math_utils import Vec3, Quat
"""

from animation_engine.model import Vec3, Quat

__all__ = ["Vec3", "Quat"]
