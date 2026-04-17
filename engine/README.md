# engine/

This directory is the **engine subdirectory** of the monorepo. It serves as
the CMake anchor and documentation root for the C++ engine code.

## Source layout

The C++ engine source lives in the repository root's `src/` directory:

```
src/
├── engine/          # Platform-independent engine kernel
│   ├── core/        # Logger, EventBus, Types (vocabulary types)
│   ├── ecs/         # Entity-Component-System (ECS.hpp — ~2 000 lines)
│   ├── input/       # Input system (ncurses on Linux)
│   ├── platform/    # Win32 window + message pump + QPC timer
│   ├── rendering/   # ncurses ASCII renderer + Vulkan bootstrap (Windows)
│   └── scripting/   # Lua 5.4 embedding + C++ bindings
├── game/            # FFXV-style gameplay systems
│   ├── systems/     # Combat, AI, Camp, Inventory, Magic, Quest, Shop, Weather
│   └── world/       # TileMap (FOV+dungeon gen), Zone, WorldMap
├── sandbox/         # Windows Vulkan clear-screen demo (Phase-1 renderer)
└── main.cpp         # Entry point for the terminal (ncurses) game build
```

## Building

See the repository root `README.md` and `CMakePresets.json` for full build
instructions.

**Quick start (Windows — engine sandbox only):**
```bat
cmake --preset windows-debug-engine-only
cmake --build --preset windows-debug-engine-only
```

**Quick start (Linux — terminal game):**
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
./game
```

## Key subsystems

| Subsystem | File(s) | What you learn |
|-----------|---------|----------------|
| ECS | `src/engine/ecs/ECS.hpp` | Sparse-set component storage, archetypes |
| EventBus | `src/engine/core/EventBus.hpp` | Pub/sub decoupling, token-based unsubscription |
| LuaEngine | `src/engine/scripting/LuaEngine.hpp/.cpp` | Embedding a scripting language |
| VulkanRenderer | `src/engine/rendering/vulkan/VulkanRenderer.hpp/.cpp` | Vulkan bootstrap |
| Win32Window | `src/engine/platform/win32/Win32Window.hpp/.cpp` | Win32 API window loop |
| CombatSystem | `src/game/systems/CombatSystem.hpp/.cpp` | ATB combat, damage formula |
| AISystem | `src/game/systems/AISystem.hpp/.cpp` | FSM + A* pathfinding |

## Architecture diagram

```
main()
└── Game (singleton)
    ├── World (ECS)
    │   ├── EntityManager       free-list + living set + signature bitset
    │   └── ComponentPool<T>[N] one sparse-set dense array per component type
    ├── Renderer                ncurses (Linux) or Vulkan (Windows)
    ├── InputSystem             raw ncurses getch() or Win32 WM_KEYDOWN
    ├── LuaEngine               lua_State*, C++ bindings, hook dispatch
    ├── EventBus<T>             4 typed buses: Combat / Quest / World / UI
    └── GameSystems (8+)        each takes World* and runs Update(float dt)
```

## Next steps (Milestone 2)

- Replace Vulkan clear-screen with a proper render graph
- Add a Mesh/Texture loader that reads cooked assets from `Cooked/`
- Wire up the animation runtime from `tools/anim_authoring/`
- Wire up the audio runtime from `tools/audio_authoring/`
