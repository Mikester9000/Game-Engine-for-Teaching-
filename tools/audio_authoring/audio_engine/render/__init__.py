"""
Render – offline bounce and stem rendering utilities.

Provides:
- :class:`OfflineBounce` – render a complete mix through an optional DSP
  chain and export it to disk with loudness normalisation.
- :class:`StemRenderer` – render individual tracks (stems) from a
  :class:`~audio_engine.composer.Sequencer` to separate files.
"""

from audio_engine.render.offline_bounce import OfflineBounce
from audio_engine.render.stem_renderer import StemRenderer

__all__ = ["OfflineBounce", "StemRenderer"]
