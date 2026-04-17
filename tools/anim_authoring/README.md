# Animation Authoring Tool

Vendored from [Mikester9000/Animation-Engine](https://github.com/Mikester9000/Animation-Engine)
into the monorepo under `tools/anim_authoring/`.

## What this does

Produces cooked animation artifacts (`.skelc`, `.animc`) from source animation
data, using the shared schemas:
- `shared/schemas/skeleton.schema.json`
- `shared/schemas/anim_clip.schema.json`
- `shared/schemas/anim_graph.schema.json`

The engine (C++) loads the cooked files at runtime.

## Install

```bat
cd tools\anim_authoring
pip install -r requirements.txt
```

## Quick start

```python
from animation_engine.model import Skeleton, AnimClip, Quat, Vec3
from animation_engine.io import Exporter, Importer

# Build a simple 2-bone skeleton
skel = Skeleton(name="SimpleSkeleton")
root = skel.add_bone("root")
spine = skel.add_bone("spine_01", parent_index=root.index)

# Export
Exporter.export_skeleton(skel, "Cooked/Anim/SimpleSkeleton.skelc")

# Build an idle animation clip
clip = AnimClip(name="Idle", skeleton_id=skel.id, duration_seconds=2.0, loopable=True)
ch = clip.add_channel(root.index, "root")
ch.add_keyframe(0.0, translation=Vec3(0, 0, 0))
ch.add_keyframe(2.0, translation=Vec3(0, 0, 0))  # loop back to start

# Export
Exporter.export_clip(clip, "Cooked/Anim/Idle.animc")
```

## Cook integration

The `samples/vertical_slice_project/cook_assets.py` script calls this
tool automatically during the cook step.

## Package structure

```
animation_engine/
├── __init__.py      # Package metadata
├── model/           # Data models: Skeleton, Bone, AnimClip, Keyframe, ...
├── io/              # Exporter + Importer (.anim JSON and .animc binary stub)
├── animation/       # BlendTree, IKSolver, MorphTarget (stubs for M2)
├── runtime/         # Runtime Animator — evaluate clip at time T
├── math_utils/      # Vec3, Quat, Matrix, Transform
└── editor/          # Tkinter-based timeline editor
```

## Shared schemas

Located in `shared/schemas/` — the engine and all tools use the same format
definitions.
