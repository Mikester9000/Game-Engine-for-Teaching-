"""
quest_baker — M20 quest and dialogue baking tool for the educational RPG engine.

TEACHING NOTE — Package vs Module
==================================
A Python *package* (directory with __init__.py) lets users write:

    from quest_baker import QuestBaker, DialogueBaker

instead of the longer:

    from quest_baker.baker import QuestBaker, DialogueBaker

We re-export the public API here so the import is clean and discoverable.
"""

from quest_baker.baker import QuestBaker, DialogueBaker, BakeResult

__all__ = ["QuestBaker", "DialogueBaker", "BakeResult"]
