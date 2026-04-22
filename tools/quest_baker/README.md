# quest-baker

**M20 quest and dialogue content baking tool** for the educational RPG engine.

## What does it do?

`quest-baker` converts *source* JSON files (human-authored) into *cooked* JSON
files (validated, runtime-ready).  This mirrors the asset pipeline used in
commercial engines (Unreal's DDC, Unity's import pipeline, etc.).

Two bakers are included:

| Baker | Input | Output |
|-------|-------|--------|
| `QuestBaker` | `quest_bank.json` | `quest_bank.cooked.json` |
| `DialogueBaker` | `dialogue_tree.json` | `dialogue_tree.cooked.json` |

## Usage

```bash
# Bake a quest bank
quest-baker quest bake Content/quest_bank.json Cooked/quest_bank.cooked.json

# Validate without writing output
quest-baker quest validate Content/quest_bank.json

# Bake a dialogue tree
quest-baker dialogue bake Content/dialogue_tree.json Cooked/dialogue_tree.cooked.json

# Validate a dialogue tree
quest-baker dialogue validate Content/dialogue_tree.json
```

## Python API

```python
from quest_baker import QuestBaker, DialogueBaker

result = QuestBaker().bake(source, output)
if not result.success:
    for err in result.errors:
        print("ERROR:", err)
```

## Running tests

```bash
pip install -e ".[dev]"
pytest
```
