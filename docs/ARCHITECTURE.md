# ARCHITECTURE — Game Engine for Teaching

> This document explains the end-to-end pipeline: how authoring tools feed data
> into the engine, how the engine's internal systems relate to each other, and
> where each piece of code lives.  Study this before reading individual source files.

---

## Big Picture

```
┌─────────────────────────────────────────────────────────┐
│                  AUTHORING (tools)                       │
│                                                          │
│  Creation Suite Editor (Qt)                              │
│  ├─ Project Browser    ← opens project folder            │
│  ├─ Content Browser    ← shows Content/ raw assets       │
│  └─ Scene Editor       ← places entities, saves JSON     │
│                                                          │
│  tools/audio_authoring/ (Python)                         │
│  ├─ AudioEngine.generate_track()                         │
│  └─ exports  →  Cooked/Audio/*.bank                      │
│                                                          │
│  tools/anim_authoring/ (Python)                          │
│  ├─ animation_engine.io.Exporter                         │
│  └─ exports  →  Cooked/Anim/*.skelc, *.animc             │
└─────────────────────────────────────────────────────────┘
           │                    │
           ▼  cook step         ▼  cook step
┌─────────────────────────────────────────────────────────┐
│                  COOKED DATA                             │
│                                                          │
│  Cooked/                                                 │
│  ├─ Audio/*.bank          (packed audio clips)           │
│  ├─ Anim/*.skelc, *.animc (cooked skeleton + clips)      │
│  ├─ Textures/*.tex        (compressed textures)          │
│  └─ Maps/*.scene.json     (entity data, loaded by Zone)  │
│                                                          │
│  AssetRegistry.json  ← updated by every cook run        │
└─────────────────────────────────────────────────────────┘
           │
           ▼  engine loads from Cooked/
┌─────────────────────────────────────────────────────────┐
│                  RUNTIME ENGINE (C++17)                  │
│                                                          │
│  main()                                                  │
│  └─ Game (singleton)                                     │
│      ├─ World (ECS)                                      │
│      │   ├─ EntityManager                                │
│      │   └─ ComponentPool<T>[N]                          │
│      ├─ Renderer (Vulkan on Windows)                     │
│      ├─ InputSystem                                      │
│      ├─ LuaEngine (Lua 5.4 scripting)                    │
│      ├─ EventBus<T> (Combat/Quest/World/UI)              │
│      └─ GameSystems                                      │
│          ├─ CombatSystem                                 │
│          ├─ AISystem                                     │
│          ├─ CampSystem                                   │
│          ├─ InventorySystem                              │
│          ├─ MagicSystem                                  │
│          ├─ QuestSystem                                  │
│          ├─ ShopSystem                                   │
│          └─ WeatherSystem                                │
└─────────────────────────────────────────────────────────┘
```

---

## Folder Responsibilities

| Folder | Language | Responsibility |
|--------|----------|----------------|
| `src/` | C++17 | Engine runtime + game systems |
| `engine/` | — | Engine README and documentation |
| `editor/` | C++17 / Qt 6 | Creation Suite editor app |
| `tools/audio_authoring/` | Python 3.9+ | Audio synthesis + bank cooking |
| `tools/anim_authoring/` | Python 3.9+ | Skeletal animation authoring + cooking |
| `shared/schemas/` | JSON Schema | Canonical data format definitions |
| `shared/runtime/` | C++17 headers | Cross-project utilities (Guid, VersionedFile) |
| `samples/vertical_slice_project/` | Mixed | End-to-end sample proving the pipeline |
| `scripts/` | Lua 5.4 | Gameplay scripts (hot-reloadable) |
| `assets/` | JSON | Asset manifests and existing schemas |
| `docs/` | Markdown | Architecture, roadmap, feature docs |

---

## Engine Subsystems (C++)

### ECS — Entity Component System (`src/engine/ecs/ECS.hpp`)

```
World
 ├─ EntityManager    — free-list of EntityIDs (uint32_t)
 │                     living-set bitset
 │                     signature bitset per entity
 └─ ComponentPool<T> — one per component type (max 64 types)
     ├─ sparse[] — maps EntityID → dense index
     └─ dense[]  — contiguous component storage (cache-friendly)

SystemBase — Update(float dt) iterates all entities with matching signature
```

**Why ECS?** Data-oriented design: components are packed in arrays, so the CPU
cache is hot when a system iterates them.  Unreal uses UActorComponent/UObject;
Unity uses ECS (DOTS) and MonoBehaviour.  Both are ECS variants.

### EventBus (`src/engine/core/EventBus.hpp`)

Typed publish-subscribe bus.  Four buses:
- `CombatEvent` — hit, kill, status effect
- `QuestEvent` — start, objective, complete
- `WorldEvent` — zone load, weather change
- `UIEvent` — show notification, open menu

**Why EventBus?** Decouples systems.  CombatSystem doesn't need a pointer to
QuestSystem; it just emits a CombatEvent and QuestSystem subscribes.

### Renderer

- **Windows:** Vulkan (from `src/engine/rendering/vulkan/`).  Win32 window
  + Vulkan surface + swapchain + per-frame command buffer.
- **Linux:** ncurses ASCII renderer (from `src/engine/rendering/Renderer.hpp`).

### LuaEngine (`src/engine/scripting/LuaEngine.hpp`)

Embeds Lua 5.4.  C++ functions are registered as Lua globals:
`engine_log`, `game_get_player_hp`, `game_heal_player`, `game_add_item`, …
Scripts in `scripts/` define hook functions called by C++:
`on_combat_start`, `on_camp_rest`, `on_level_up`, `on_explore_update`.

---

## Qt Editor Architecture

```
QApplication
└─ MainWindow (QMainWindow)
    ├─ Menu bar (File | Build | Help)
    ├─ Toolbar
    ├─ ContentBrowser (QDockWidget → QTreeView + QFileSystemModel)
    │   Signals: fileSelected(path)
    ├─ SceneEditor (QWidget — central, custom paintEvent)
    │   Draws: grid, entity boxes
    │   Handles: left-click = place entity, Delete = remove
    │   saveScene() → JSON via QJsonDocument
    │   loadScene() ← JSON via QJsonDocument
    └─ Status bar
```

**How to add a new editor panel:**
1. Create `editor/src/MyPanel.hpp` / `.cpp` deriving from `QWidget` or `QDockWidget`.
2. Add a `Q_OBJECT` macro and any signals/slots.
3. In `MainWindow::setupDockWidgets()`, create and dock it.
4. Add the `.cpp` to `EDITOR_SOURCES` in `editor/CMakeLists.txt`.

---

## Audio Pipeline

```
tools/audio_authoring/audio_engine/
├─ engine.py     — AudioEngine façade
├─ ai/           — MusicGen, SFXGen, VoiceGen (prompt → audio)
├─ composer/     — Sequencer + Note (procedural)
├─ synthesizer/  — InstrumentLibrary (wavetable synth)
├─ dsp/          — filters, reverb, compressor
├─ export/       — AudioExporter (WAV, OGG)
└─ qa/           — LoudnessMeter, ClippingDetector

Cook step (stub, see samples/vertical_slice_project/cook_assets.py):
  AudioEngine.generate_track("battle") → "Cooked/Audio/battle.bank"

Engine loads:  Cooked/Audio/*.bank → AudioSystem (C++, to be implemented in M2)
```

**Schema:** `shared/schemas/audio_bank.schema.json`

---

## Animation Pipeline

```
tools/anim_authoring/animation_engine/
├─ model/       — Vertex, Mesh, Material, Bone, Skeleton, Model
├─ animation/   — Keyframe, Channel, Clip, BlendTree, IKSolver
├─ io/          — Exporter / Importer (.anim JSON and GLTF 2.0)
├─ runtime/     — Animator (evaluates clip at time t), CPU skinning
├─ math_utils/  — Vector, Quaternion, Matrix, Transform
└─ editor/      — Tkinter timeline editor (standalone tool)

Cook step:
  animation_engine.io.Exporter.export_clip(clip, "Cooked/Anim/Walk.animc")
  animation_engine.io.Exporter.export_skeleton(skel, "Cooked/Anim/Human.skelc")

Engine loads:  Cooked/Anim/*.skelc + *.animc → AnimationSystem (C++, M2)
```

**Schemas:** `shared/schemas/skeleton.schema.json`, `anim_clip.schema.json`,
             `anim_graph.schema.json`

---

## Data Format Versioning

Every shared file has:
```json
{
  "$schema": "../../shared/schemas/scene.schema.json",
  "version": "1.0.0",
  ...
}
```

When you change a format:
- **PATCH** bump: bug fix, no consumer changes needed.
- **MINOR** bump: added optional field, old readers ignore it safely.
- **MAJOR** bump: breaking change; add a migration path in `shared/runtime/VersionedFile.hpp`.

---

## Vertical Slice Pipeline (end-to-end)

```
samples/vertical_slice_project/
├─ Project.json             ← project descriptor
├─ AssetRegistry.json       ← updated by cook step
├─ Content/                 ← raw source assets (committed)
│   ├─ Textures/            ← .png placeholder textures
│   ├─ Audio/               ← .wav source audio
│   ├─ Maps/                ← .scene.json maps
│   └─ Animations/          ← .json source animation data
├─ Cooked/                  ← generated at cook time (gitignored)
└─ cook_assets.py           ← one-command cook script

Run: cd samples/vertical_slice_project && python cook_assets.py
```

The cook script:
1. Reads `Project.json` → knows source + cooked paths.
2. Runs audio authoring → writes `Cooked/Audio/*.bank`.
3. Runs animation authoring → writes `Cooked/Anim/*.skelc`, `*.animc`.
4. Updates `AssetRegistry.json`.
5. Prints a summary of cooked files.

---

## Learning Path

Study files in this order:

1. `shared/schemas/*.schema.json` — understand the data model
2. `src/engine/core/Types.hpp` — vocabulary types
3. `src/engine/core/Logger.hpp` — singleton, thread safety, macros
4. `src/engine/ecs/ECS.hpp §1-4` — ECS basics (ComponentPool, sparse-set)
5. `src/engine/ecs/ECS.hpp §5-7` — World, SystemBase
6. `src/engine/core/EventBus.hpp` — Observer pattern, pub/sub
7. `src/game/GameData.hpp` — Flyweight pattern, static databases
8. `src/game/systems/CombatSystem.*` — ATB combat
9. `src/game/systems/AISystem.*` — FSM + A*
10. `src/engine/scripting/LuaEngine.*` — scripting integration
11. `editor/src/MainWindow.*` — Qt Widgets, signals/slots, dock widgets
12. `editor/src/ContentBrowser.*` — Qt Model/View (QFileSystemModel)
13. `editor/src/SceneEditor.*` — Custom painting, JSON I/O
14. `tools/audio_authoring/audio_engine/engine.py` — Python façade pattern
15. `tools/anim_authoring/animation_engine/` — animation data model
16. `samples/vertical_slice_project/cook_assets.py` — scripted pipeline
