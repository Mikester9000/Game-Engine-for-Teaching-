"""
Animation Authoring Tool — vendored from Mikester9000/Animation-Engine
======================================================================

TEACHING NOTE
=============
This package is vendored from https://github.com/Mikester9000/Animation-Engine
into the monorepo under tools/anim_authoring/.

It provides a Python API for creating and exporting:
  - Hierarchical skeletal animations (50+ bone rigs)
  - Smooth cubic-spline keyframe interpolation
  - Animation state machine graphs
  - Inverse Kinematics (foot placement, hand IK)
  - Blend-shape / morph-target facial animation
  - PBR material authoring

The tool produces cooked artifacts in Cooked/Anim/ that the C++ engine
loads at runtime. Python is the "compiler"; C++ is the runtime.

Shared schemas:
  shared/schemas/skeleton.schema.json
  shared/schemas/anim_clip.schema.json
  shared/schemas/anim_graph.schema.json

Package layout:
  animation_engine.math_utils   – Vector, Quaternion, Matrix, Transform
  animation_engine.model        – Vertex, Mesh, Material, Bone, Skeleton, Model
  animation_engine.animation    – Keyframe, Channel, Clip, BlendTree, IKSolver
  animation_engine.io           – Exporter / Importer (.anim JSON and GLTF 2.0)
  animation_engine.runtime      – Runtime Animator and CPU skinning
  animation_engine.editor       – Timeline / model editor (Tkinter-based)
"""

__version__ = "1.0.0"
__author__  = "Animation Engine Team"
