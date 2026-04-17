"""
AnimAssetPipeline – cook animation source assets to the cooked directory.

TEACHING NOTE — Cook Pipeline
================================
The cook step converts human-readable source assets (JSON animation clips,
JSON skeletons) into runtime-ready cooked assets (.animc, .skelc).

In this teaching implementation, the cooked format is still JSON (for
readability).  A real engine would use a compact binary format here.

Source path:  Content/Animations/*.json  (skeleton + clip files)
Cooked path:  Cooked/Anim/*.skelc, *.animc
"""

from __future__ import annotations

import json
import time
from dataclasses import dataclass, field
from pathlib import Path

from animation_engine.model import Skeleton, AnimClip
from animation_engine.io import Exporter, Importer

__all__ = ["AnimAssetPipeline", "AnimManifest"]


@dataclass
class AnimManifest:
    """Summary of a cook run.

    Attributes
    ----------
    content_dir:
        Source content directory.
    cooked_dir:
        Output cooked directory.
    skeletons:
        List of cooked skeleton entries (dicts with ``source``, ``cooked``,
        ``status`` keys).
    clips:
        List of cooked clip entries.
    errors:
        List of error messages for assets that failed to cook.
    elapsed_seconds:
        Total cook time in seconds.
    """
    content_dir:     str
    cooked_dir:      str
    skeletons:       list = field(default_factory=list)
    clips:           list = field(default_factory=list)
    errors:          list = field(default_factory=list)
    elapsed_seconds: float = 0.0

    def summary(self) -> str:
        """Return a human-readable summary string."""
        n_ok = len(self.skeletons) + len(self.clips)
        n_err = len(self.errors)
        return (
            f"Animation cook complete — {self.cooked_dir}\n"
            f"  Skeletons: {len(self.skeletons)}, Clips: {len(self.clips)}"
            f", Errors: {n_err}, Total: {n_ok} in {self.elapsed_seconds:.1f}s"
        )

    def to_json(self) -> str:
        """Serialise the manifest to a JSON string."""
        return json.dumps({
            "content_dir":     self.content_dir,
            "cooked_dir":      self.cooked_dir,
            "skeletons":       self.skeletons,
            "clips":           self.clips,
            "errors":          self.errors,
            "elapsed_seconds": self.elapsed_seconds,
        }, indent=2)


class AnimAssetPipeline:
    """Cook animation source assets (Content/Animations/) to Cooked/Anim/.

    Parameters
    ----------
    skip_existing:
        If ``True`` (default), skip assets that already have a cooked file.

    TEACHING NOTE — Cook Pipeline Design
    The pipeline follows a simple "scan → transform → write" pattern:
      1. Scan the content directory for JSON source files.
      2. For each file, determine its type (skeleton or clip) from the
         ``$schema`` field or filename convention.
      3. Deserialise, re-serialise to the cooked path.
    """

    _SKELETON_SUFFIX = ".skelc"
    _CLIP_SUFFIX     = ".animc"

    def __init__(self, skip_existing: bool = True) -> None:
        self.skip_existing = skip_existing

    def cook_all(self, content_dir: str | Path, cooked_dir: str | Path) -> AnimManifest:
        """Cook all JSON animation assets from *content_dir* to *cooked_dir*.

        Parameters
        ----------
        content_dir:
            Directory containing source ``.json`` animation files.
        cooked_dir:
            Output directory for cooked ``.skelc`` and ``.animc`` files.

        Returns
        -------
        AnimManifest
            Summary of what was cooked.
        """
        content_dir = Path(content_dir)
        cooked_dir  = Path(cooked_dir)
        cooked_dir.mkdir(parents=True, exist_ok=True)

        manifest = AnimManifest(
            content_dir=str(content_dir),
            cooked_dir=str(cooked_dir),
        )
        t_start = time.monotonic()

        for src_path in sorted(content_dir.glob("*.json")):
            try:
                self._cook_file(src_path, cooked_dir, manifest)
            except Exception as exc:
                manifest.errors.append(f"{src_path.name}: {exc}")

        manifest.elapsed_seconds = time.monotonic() - t_start
        return manifest

    def _cook_file(
        self,
        src_path: Path,
        cooked_dir: Path,
        manifest: AnimManifest,
    ) -> None:
        """Cook one JSON file.  Detects type from ``$schema`` field."""
        raw = json.loads(src_path.read_text(encoding="utf-8"))
        schema_ref: str = raw.get("$schema", "")

        if "skeleton" in schema_ref or src_path.stem.endswith("_skeleton"):
            # --- skeleton ---
            cooked_path = cooked_dir / (src_path.stem + self._SKELETON_SUFFIX)
            if self.skip_existing and cooked_path.exists():
                manifest.skeletons.append({
                    "source": str(src_path), "cooked": str(cooked_path), "status": "skipped"
                })
                return
            skeleton = Importer.import_skeleton(src_path)
            Exporter.export_skeleton(skeleton, cooked_path)
            manifest.skeletons.append({
                "source": str(src_path), "cooked": str(cooked_path), "status": "ok"
            })
        else:
            # --- animation clip (default) ---
            cooked_path = cooked_dir / (src_path.stem + self._CLIP_SUFFIX)
            if self.skip_existing and cooked_path.exists():
                manifest.clips.append({
                    "source": str(src_path), "cooked": str(cooked_path), "status": "skipped"
                })
                return
            clip = Importer.import_clip(src_path)
            Exporter.export_clip_binary(clip, cooked_path)
            manifest.clips.append({
                "source": str(src_path), "cooked": str(cooked_path), "status": "ok"
            })
