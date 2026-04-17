"""
animation_engine.editor – Timeline editor stub.

TEACHING NOTE — Tkinter vs. Qt
=================================
The full editor is a Qt 6 Widgets application (see tools/editor/).
This module provides a minimal Tkinter-based timeline viewer that works
without Qt installed – useful for quick clip inspection and teaching.

The TimelineEditor is a stub; Milestone 6 will add the full Qt editor panel.
"""

from __future__ import annotations

from animation_engine.model import AnimClip

__all__ = ["TimelineEditor"]


class TimelineEditor:
    """Minimal animation timeline editor (Tkinter stub).

    Provides a simple text summary of a clip's channel/keyframe structure.
    The full graphical editor requires Qt 6 (see editor/ directory).

    Example
    -------
    >>> ed = TimelineEditor(clip)
    >>> print(ed.summary())
    """

    def __init__(self, clip: AnimClip) -> None:
        self.clip = clip

    def summary(self) -> str:
        """Return a human-readable text summary of the clip."""
        lines = [
            f"Clip:     {self.clip.name}",
            f"Duration: {self.clip.duration_seconds:.3f}s  @{self.clip.fps}fps",
            f"Loopable: {self.clip.loopable}",
            f"Channels: {len(self.clip.channels)}",
        ]
        for ch in self.clip.channels:
            lines.append(
                f"  [{ch.bone_index:3d}] {ch.bone_name:<20s}  {len(ch.keyframes)} keyframes"
            )
        return "\n".join(lines)
