# Lua 5.5 — Bundled Source & Windows Runtime Binaries

This directory contains the complete Lua 5.5.0 distribution, enabling the
scripting subsystem (`LuaEngine.hpp/.cpp`) on all platforms.

## Contents

| Path / File              | Purpose                                                   |
|--------------------------|-----------------------------------------------------------|
| `lua-5.5.0/src/*.c/.h`  | Full Lua 5.5.0 source — compiled directly into the engine as a static library (`lua55_static`) |
| `lua-5.5.0/doc/`        | Official Lua 5.5 reference manual (HTML)                  |
| `lua55.dll`             | Lua 5.5 runtime DLL (Windows, copied next to `engine_sandbox.exe` by CMake post-build) |
| `lua55.exe`             | Standalone Lua 5.5 interpreter (REPL / script runner)    |
| `luac55.exe`            | Lua 5.5 byte-code compiler                               |
| `wlua55.exe`            | Windowless Lua 5.5 interpreter (GUI apps, no console)    |

## How Lua is Built

CMake detects `lua-5.5.0/src/lua.h` at configure time and compiles all
library `.c` files (everything except `lua.c` and `luac.c`, which are
standalone tools) into a static library target `lua55_static`.

Both `engine_sandbox` (Windows) and the terminal `game` (Linux) link against
`lua55_static` — **no external Lua installation is required**.

```
Lua/lua-5.5.0/src/
├── lua.h        ← Public API header
├── lualib.h     ← Standard library loader declarations
├── lauxlib.h    ← Auxiliary helper functions
├── luaconf.h    ← Platform configuration macros
├── lapi.c       ┐
├── lbaselib.c   │
├── lcode.c      │  These 32 .c files are compiled into
├── ...          │  lua55_static (static library)
└── lzio.c       ┘

lua.c   — NOT in the library (standalone interpreter entry point)
luac.c  — NOT in the library (byte-code compiler entry point)
```

## CMake Integration Summary

| Platform | Detection                    | ENGINE_ENABLE_LUA | Lua version |
|----------|------------------------------|-------------------|-------------|
| Windows  | `lua-5.5.0/src/lua.h` found  | Defined ✓        | 5.5.0       |
| Linux    | `lua-5.5.0/src/lua.h` found  | Defined ✓        | 5.5.0       |
| Any      | Folder missing (fresh clone) | Undefined ✗       | —           |

## Teaching Note

The Lua scripting integration in this engine demonstrates:

- **C/C++ interop** — `extern "C"` linkage, `lua_State*` VM pointer
- **Safe scripting hooks** — `lua_pcall` catches all Lua errors; a crash in a
  Lua script never takes down the C++ game loop
- **Event-driven scripting** — `on_combat_start`, `on_camp_rest`, `on_zone_loaded`
- **Embedding vs. extending** — we *embed* Lua inside C++ (the engine calls into
  Lua), as opposed to *extending* Lua with C (Lua calls into C modules)

See `src/engine/scripting/LuaEngine.hpp` and `scripts/*.lua` for the full
implementation with teaching-note annotations.
