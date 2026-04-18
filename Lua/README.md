# Lua 5.5 — Windows Runtime Binaries

This directory contains the Lua 5.5 runtime for Windows, enabling the scripting
subsystem (`LuaEngine.hpp/.cpp`) to run on Windows alongside the D3D11 renderer.

## Contents

| File           | Purpose                                          |
|----------------|--------------------------------------------------|
| `lua55.dll`    | Lua 5.5 runtime DLL — copied next to `engine_sandbox.exe` by CMake post-build |
| `lua55.exe`    | Standalone Lua 5.5 interpreter (REPL / script runner) |
| `luac55.exe`   | Lua 5.5 byte-code compiler (`luac55 script.lua → script.luac`) |
| `wlua55.exe`   | Windowless Lua 5.5 interpreter (no console window — useful for GUI apps) |

## Enabling Lua Scripting in engine_sandbox (Windows)

The DLL is present, but to **compile** the scripting system on Windows you also
need the C headers and an import library.  CMake detects these automatically if
you place them in the correct subdirectories:

```
Lua/
├── include/          ← Put Lua 5.5 headers here
│   ├── lua.h
│   ├── lualib.h
│   ├── lauxlib.h
│   └── luaconf.h
├── lib/              ← Put the import library here
│   └── lua55.lib     ← Generated from lua55.dll (see below)
├── lua55.dll         ← Already present ✓
├── lua55.exe         ← Already present ✓
└── luac55.exe        ← Already present ✓
```

### Step 1 — Get the Lua 5.5 source headers

Download the Lua 5.5 work release source from https://www.lua.org/work/ and
extract the `.h` files (`lua.h`, `lualib.h`, `lauxlib.h`, `luaconf.h`) into
`Lua/include/`.

### Step 2 — Generate lua55.lib from lua55.dll

Open a **Developer Command Prompt** (with MSVC tools on PATH):

```bat
cd /path/to/repo/Lua
dumpbin /exports lua55.dll > lua55.def
:: Manually edit lua55.def to match the lib /DEF format (remove header lines,
:: keep only EXPORTS and the symbol list)
lib /DEF:lua55.def /MACHINE:X64 /OUT:lib\lua55.lib
```

Alternatively, the MinGW `dlltool` produces a compatible `.lib`:

```bash
gendef lua55.dll
dlltool -d lua55.def -l lib/lua55.lib
```

### Step 3 — CMake auto-detects everything

Once both `Lua/include/lua.h` and `Lua/lib/lua55.lib` exist, CMake will:

1. Set `LUA_BUNDLED=ON`.
2. Add `Lua/include/` to the include path.
3. Link `engine_sandbox` against `lua55.lib`.
4. Copy `lua55.dll` next to `engine_sandbox.exe` as a post-build step.
5. Define `ENGINE_ENABLE_LUA` so all scripting hooks compile in.

Reconfigure with:

```bat
cmake --preset windows-debug-engine-only
cmake --build --preset windows-debug-engine-only
```

## Teaching Note

The Lua scripting integration in this engine demonstrates:

- **C/C++ interop** (`extern "C"` linkage, `lua_State*`)
- **Safe scripting hooks** (`lua_pcall` catches all Lua errors — a crash in a
  Lua script never takes down the C++ game loop)
- **Event-driven scripting** (`on_combat_start`, `on_camp_rest`, etc.)

See `src/engine/scripting/LuaEngine.hpp` and `scripts/*.lua` for the full
implementation and annotated teaching notes.
