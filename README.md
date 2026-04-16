# Game Engine for Teaching — Final Fantasy XV Style Action RPG

> **Goal:** A fully working, exhaustively commented C++17 action RPG engine that
> mirrors the architecture of *Final Fantasy XV*, built so that every line of code
> is a lesson.  Study the finished product, then recreate it yourself.

---

## What You Will Learn

| Topic | Where to look |
|---|---|
| Entity Component System (ECS) | `src/engine/ecs/ECS.hpp` |
| Publish-Subscribe Event Bus | `src/engine/core/EventBus.hpp` |
| Lua 5.4 scripting integration | `src/engine/scripting/LuaEngine.hpp/.cpp` + `scripts/` |
| ATB (Active Time Battle) combat | `src/game/systems/CombatSystem.hpp/.cpp` |
| Party link strikes | `CombatSystem::ProcessLinkStrikes` |
| A\* pathfinding & FSM AI | `src/game/systems/AISystem.hpp/.cpp` |
| Delayed-XP camp system (FF15 style) | `src/game/systems/CampSystem.hpp/.cpp` |
| Item / equipment / crafting | `src/game/systems/InventorySystem.hpp/.cpp` |
| Magic & Elemancy flasks | `src/game/systems/MagicSystem.hpp/.cpp` |
| Shop / economy | `src/game/systems/ShopSystem.hpp/.cpp` |
| Quest tracking | `src/game/systems/QuestSystem.hpp/.cpp` |
| Tile-based world, FOV, dungeon gen | `src/game/world/TileMap.hpp/.cpp` |
| Zone lifecycle & entity spawning | `src/game/world/Zone.hpp/.cpp` |
| ncurses ASCII renderer | `src/engine/rendering/Renderer.hpp/.cpp` |
| Fixed-timestep game loop | `src/game/Game.cpp` — `Game::Run()` |
| Data-driven game database (Flyweight) | `src/game/GameData.hpp` |
| Thread-safe singleton logger | `src/engine/core/Logger.hpp/.cpp` |
| C++17 features in practice | Throughout (fold expressions, `if constexpr`, CTAD…) |

---

## Quick Start

### Linux / macOS — Terminal Game (`game`)

#### 1. Install Dependencies

```bash
# Ubuntu / Debian
sudo apt-get update
sudo apt-get install -y build-essential cmake libncurses-dev liblua5.4-dev

# Fedora / RHEL
sudo dnf install -y gcc-c++ cmake ncurses-devel lua5.4-devel

# macOS (Homebrew)
brew install cmake ncurses lua
```

| Library | Why it's needed |
|---|---|
| `cmake` ≥ 3.16 | Cross-platform build system |
| `libncurses-dev` | Terminal rendering (ASCII game display) |
| `liblua5.4-dev` | Embeds Lua 5.4 scripting interpreter |

#### 2. Build

```bash
git clone https://github.com/Mikester9000/Game-Engine-for-Teaching-.git
cd Game-Engine-for-Teaching-
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

The compiled binary is `build/game`.

#### 3. Run

```bash
# From the build/ directory (Lua scripts are copied here by CMake):
./game
```

Controls are shown in the in-game main menu.

---

### Windows — Vulkan Sandbox (`engine_sandbox`)

`engine_sandbox` is the first Windows rendering milestone: it opens a Win32
window and clears the screen to an animated colour each frame using Vulkan.
This is the foundation on which the full renderer will be built.

#### 1. Install Prerequisites

| Tool | Where to get it | Notes |
|---|---|---|
| Visual Studio 2022 | https://visualstudio.microsoft.com/ | Install **Desktop development with C++** workload |
| CMake ≥ 3.16 | https://cmake.org/download/ | Tick "Add CMake to PATH" during install |
| Vulkan SDK ≥ 1.3 | https://vulkan.lunarg.com/ | The SDK sets the `VULKAN_SDK` env var that CMake's `FindVulkan` uses |

> **Tip:** After installing the Vulkan SDK, open a **new** terminal so that
> the `VULKAN_SDK` environment variable is visible.

#### 2. Build

Open a **Developer Command Prompt for VS 2022** (or any terminal with CMake
on `PATH`) and run:

```bat
git clone https://github.com/Mikester9000/Game-Engine-for-Teaching-.git
cd Game-Engine-for-Teaching-

mkdir build
cd build

:: Configure — CMake detects Windows and enables the Vulkan target automatically
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Debug

:: Build only the Vulkan sandbox (fastest way to see results)
cmake --build . --config Debug --target engine_sandbox
```

Alternatively open the generated `EducationalGameEngine.sln` in Visual Studio,
set `engine_sandbox` as the **Startup Project**, and press **F5**.

#### 3. Run

```bat
:: From the build\Debug\ directory:
engine_sandbox.exe
```

A window titled **"Engine Sandbox — Vulkan Clear Screen"** opens and the
background colour slowly cycles through the rainbow.  Press **ESC** or click
the × button to exit.

#### CMake options

| Option | Default (Windows) | Description |
|---|---|---|
| `ENGINE_ENABLE_TERMINAL` | `OFF` | Build the ncurses `game` target (Linux/macOS only) |
| `ENGINE_ENABLE_VULKAN`   | `ON`  | Build the `engine_sandbox` Vulkan target |

To force-enable both on Linux (Vulkan must also be installed):

```bash
cmake .. -DENGINE_ENABLE_VULKAN=ON -DENGINE_ENABLE_TERMINAL=ON
```

---

## Repository Structure

```
Game-Engine-for-Teaching-/
├── CMakeLists.txt              # Build configuration (platform-gated targets)
├── README.md                   # This file
├── assets/
│   ├── schema/
│   │   └── asset-manifest.schema.json  # Canonical JSON Schema (draft-07)
│   └── examples/
│       ├── audio-manifest.json         # Example audio asset manifest
│       ├── texture-manifest.json       # Example texture asset manifest
│       ├── tilemap-manifest.json       # Example tilemap asset manifest
│       └── model-manifest.json         # Example model asset manifest
├── docs/
│   └── asset-manifest.md       # Schema reference & integration guide
├── tools/
│   ├── validate-assets.py  # Asset manifest validator CLI
│   ├── audio_engine.py     # Audio Engine: register clips, emit/consume manifests
│   └── creation_engine.py  # Creation Engine: all asset types, emit/consume manifests
├── scripts/                    # Lua 5.4 game scripts (hot-reloadable)
│   ├── main.lua                # Global hooks: on_explore_update, on_camp_rest …
│   ├── quests.lua              # Quest event callbacks
│   └── enemies.lua             # Enemy/spell effect callbacks
└── src/
    ├── main.cpp                # Entry point (terminal game)
    ├── sandbox/
    │   └── main.cpp            # Entry point (Windows Vulkan sandbox)
    ├── engine/
    │   ├── core/
    │   │   ├── Types.hpp       # Engine-wide type definitions (Vec3, EntityID …)
    │   │   ├── Logger.hpp/.cpp # Thread-safe LOG_INFO / LOG_WARN / LOG_ERROR macros
    │   │   └── EventBus.hpp    # Publish-subscribe event bus (CombatEvent, UIEvent …)
    │   ├── ecs/
    │   │   └── ECS.hpp         # Full ECS: ComponentPool, World, SystemBase (2 000 lines)
    │   ├── input/
    │   │   └── InputSystem.hpp/.cpp  # ncurses keyboard polling (Linux/macOS)
    │   ├── platform/
    │   │   └── win32/
    │   │       └── Win32Window.hpp/.cpp  # Win32 window, message pump, QPC timer
    │   ├── rendering/
    │   │   ├── Renderer.hpp/.cpp         # ncurses ASCII renderer (Linux/macOS)
    │   │   └── vulkan/
    │   │       └── VulkanRenderer.hpp/.cpp  # Vulkan bootstrap (Windows)
    │   └── scripting/
    │       └── LuaEngine.hpp/.cpp    # Lua 5.4 embedding + C++ bindings
    └── game/
        ├── Game.hpp/.cpp        # Top-level game class, state machine, main loop
        ├── GameData.hpp         # Static game database: items, spells, enemies, quests
        ├── systems/
        │   ├── CombatSystem     # ATB combat, damage formula, warp strike, link strikes
        │   ├── AISystem         # FSM + A* pathfinding
        │   ├── CampSystem       # Rest, cook, delayed XP (FF15 camp mechanic)
        │   ├── InventorySystem  # Item add/remove/use/equip
        │   ├── MagicSystem      # Spell casting, AOE, Elemancy flasks
        │   ├── QuestSystem      # Objective tracking, rewards
        │   ├── ShopSystem       # Buy / sell
        │   └── WeatherSystem    # Time-of-day, weather, EventBus publisher
        └── world/
            ├── TileMap.hpp/.cpp  # Tile grid, FOV ray-casting, procedural dungeon gen
            ├── Zone.hpp/.cpp     # Zone lifecycle, enemy / NPC spawning
            └── WorldMap.hpp/.cpp # Regional overworld map
```

---

## Architecture Overview

```
main()
└── Game (Meyers singleton)
    ├── World (ECS)
    │   ├── EntityManager   — free-list + living set + signature bitset per entity
    │   └── ComponentPool<T>[64]  — one sparse-set dense array per component type
    ├── TerminalRenderer    — ncurses back-buffer, color pairs
    ├── InputSystem         — raw ncurses getch()
    ├── LuaEngine           — lua_State*, registry-stored World ptr, 9 bindings
    ├── EventBus<T>         — 4 typed buses: Combat / Quest / World / UI
    ├── CombatSystem        — ATB turn engine, damage, link strikes, loot
    ├── AISystem            — FSM (IDLE/PATROL/ALERT/CHASE/ATTACK/FLEE) + A*
    ├── CampSystem          — rest → level-up (pending XP), meal buffs, day advance
    ├── InventorySystem     — items only (Gil is in CurrencyComponent)
    ├── MagicSystem         — spell casting, MP, Lua spell callbacks
    ├── QuestSystem         — objective counters, quest rewards
    ├── ShopSystem          — buy / sell with CurrencyComponent
    └── WeatherSystem       — time / weather cycle, EventBus publisher
    
    Zone (current)
    ├── TileMap             — FOV + dungeon gen
    └── WorldMap            — regional map

    GameData.hpp            — header-only, inline static Flyweight database
    scripts/*.lua           — Lua hooks called by C++ at predefined moments
```

**Game state machine:**
`MAIN_MENU` → `EXPLORING` → `COMBAT` → `DIALOGUE` / `INVENTORY` / `SHOPPING` / `CAMPING` / `VEHICLE`

---

## Lua Scripting Hooks

Scripts in `scripts/` can define the following functions.  C++ calls them
automatically at the right moments:

| Hook function | Called from | Arguments |
|---|---|---|
| `on_explore_update()` | Every exploration frame | none |
| `on_combat_start(enemyName)` | `CombatSystem::StartCombat` | enemy name string |
| `on_camp_rest()` | `CampSystem::Rest` (after XP applied) | none |
| `on_level_up(newLevel)` | `CampSystem::Rest` (when level changed) | new level integer |

C++ functions available to Lua scripts:

```lua
engine_log("message")          -- Write to the engine log
game_get_player_hp()           -- Returns player HP as integer
game_heal_player(amount)       -- Restore HP
game_get_gold()                -- Returns Gil balance
game_add_item("Potion", 3)     -- Add items to player inventory
game_start_combat(enemyID)     -- Trigger a combat encounter
game_complete_quest(questID)   -- Mark a quest complete
game_show_message("text")      -- Display a UI notification
```

---

## Asset Manifest & Validator

All game assets (audio, textures, tilemaps, models) are described by **manifest
files** that share a canonical JSON schema.  Three CLI tools interact with this
format:

| Tool | Purpose |
|---|---|
| `tools/validate-assets.py` | Validate manifest files against the schema |
| `tools/audio_engine.py` | Audio Engine — register clips, emit/consume manifests |
| `tools/creation_engine.py` | Creation Engine — all asset types, emit/consume manifests |

### Quick start

```bash
# Validate all example manifests (no extra dependencies needed)
python3 tools/validate-assets.py

# Audio Engine: consume an existing manifest and list loaded clips
python3 tools/audio_engine.py consume \
    --manifest assets/examples/audio-manifest.json --list

# Creation Engine: emit a demo manifest covering all asset types
python3 tools/creation_engine.py emit --manifest /tmp/demo.json

# Install optional jsonschema for richer validation error messages
pip install jsonschema
python3 tools/validate-assets.py assets/examples/ --verbose
```

### What the manifest covers

| Field | Purpose |
|---|---|
| `id` | Stable GUID / slug — referenced by the engine and other assets |
| `version` | Asset revision (SemVer) |
| `type` | `audio` / `texture` / `tilemap` / `model` / `script` / `material` |
| `source` | Relative path to the source file |
| `hash` | SHA-256 of the file — detects accidental overwrites |
| `dependencies` | IDs of assets this one depends on |
| `tags` | Free-form labels (`["combat", "boss"]`) for pipeline filtering |

Per-type extension blocks (`"audio": {...}`, `"texture": {...}`, …) carry
format-specific metadata (sample rate, dimensions, triangle count, etc.).

See **`docs/asset-manifest.md`** for the full schema reference and integration
guide.

---

## Educational Curriculum Path

Study the files in this order for the smoothest learning curve:

1. **`Types.hpp`** — vocabulary types, why `EntityID` and not raw `int`, `Vec3` math
2. **`Logger.hpp`** — Meyers singleton, thread safety, `do {} while(0)` macros
3. **`ECS.hpp` §1–4** — what is an ECS, sparse-set `ComponentPool`, swap-and-pop
4. **`ECS.hpp` §5–7** — `SystemBase`, `World` facade, `CreateCharacter` factory
5. **`EventBus.hpp`** — Observer pattern, pub/sub decoupling, token unsubscription
6. **`GameData.hpp`** — Flyweight pattern, header-only libraries, static databases
7. **`CombatSystem`** — ATB clock, damage formula, status DoT, link strikes
8. **`AISystem`** — FSM states, A\* pathfinding on a tile grid
9. **`LuaEngine`** — why scripting, Lua stack model, `pcall` error handling, bindings
10. **`Game.cpp`** — fixed-timestep loop, dependency injection, singleton trade-offs
11. **`TileMap`** — FOV ray-casting, procedural dungeon generation
12. **Next step** — add an animation system (sprite-frame state machine)
13. **Next step** — replace ncurses with SDL2 for a real draw pipeline
14. **Next step** — streaming world / async zone loading with `std::thread`
15. **Next step** — behaviour-tree AI to express complex boss patterns

---

## Contributing / Extending

1. Fork the repository.
2. Add a new system in `src/game/systems/` following the existing pattern
   (`MySystem.hpp` + `MySystem.cpp`, constructor takes `World*`, method
   `Update(float dt)`).
3. Register and wire it in `Game::Init()`.
4. Add Lua bindings if designers need to script against the new system.
5. Document every non-obvious decision with a `// TEACHING NOTE —` comment.

