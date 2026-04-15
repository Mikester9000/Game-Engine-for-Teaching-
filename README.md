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

### 1. Install Dependencies

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

### 2. Build

```bash
git clone https://github.com/Mikester9000/Game-Engine-for-Teaching-.git
cd Game-Engine-for-Teaching-
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

The compiled binary is `build/game`.

### 3. Run

```bash
# From the build/ directory (Lua scripts are copied here by CMake):
./game
```

Controls are shown in the in-game main menu.

---

## Repository Structure

```
Game-Engine-for-Teaching-/
├── CMakeLists.txt              # Build configuration
├── README.md                   # This file
├── scripts/                    # Lua 5.4 game scripts (hot-reloadable)
│   ├── main.lua                # Global hooks: on_explore_update, on_camp_rest …
│   ├── quests.lua              # Quest event callbacks
│   └── enemies.lua             # Enemy/spell effect callbacks
└── src/
    ├── main.cpp                # Entry point
    ├── engine/
    │   ├── core/
    │   │   ├── Types.hpp       # Engine-wide type definitions (Vec3, EntityID …)
    │   │   ├── Logger.hpp/.cpp # Thread-safe LOG_INFO / LOG_WARN / LOG_ERROR macros
    │   │   └── EventBus.hpp    # Publish-subscribe event bus (CombatEvent, UIEvent …)
    │   ├── ecs/
    │   │   └── ECS.hpp         # Full ECS: ComponentPool, World, SystemBase (2 000 lines)
    │   ├── input/
    │   │   └── InputSystem.hpp/.cpp  # ncurses keyboard polling
    │   ├── rendering/
    │   │   └── Renderer.hpp/.cpp     # ncurses ASCII renderer
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

