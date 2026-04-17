"""
animation_engine.integration – Game Engine for Teaching compatibility layer.

TEACHING NOTE — Integration Layer Pattern
==========================================
The integration sub-package bridges the Python Animation Engine with the
C++ Game Engine for Teaching monorepo.

It provides:
  - :class:`AnimAssetPipeline` – cook all animation assets from ``Content/``
    to ``Cooked/Anim/`` in one call.
  - :class:`AnimManifest` – summary of generated assets (mirrors
    ``AssetRegistry.json`` entries).

Shared schemas used:
  - ``shared/schemas/skeleton.schema.json``
  - ``shared/schemas/anim_clip.schema.json``

Usage
-----
From ``cook_assets.py``:

    from animation_engine.integration import AnimAssetPipeline
    pipeline = AnimAssetPipeline()
    manifest = pipeline.cook_all(
        content_dir="Content/Animations",
        cooked_dir="Cooked/Anim",
    )
    print(manifest.summary())
"""

from animation_engine.integration.anim_pipeline import AnimAssetPipeline, AnimManifest

__all__ = ["AnimAssetPipeline", "AnimManifest"]
