/**
 * @file LuaEngine.cpp
 * @brief Implementation of the LuaEngine singleton and all engine Lua bindings.
 *
 * ============================================================================
 * TEACHING NOTE — Anatomy of a Lua Binding
 * ============================================================================
 *
 * Each C++ function exposed to Lua follows the same pattern:
 *
 *   static int binding_function_name(lua_State* L)
 *   {
 *       // 1. Validate argument count
 *       int nargs = lua_gettop(L);  // number of args on the stack
 *
 *       // 2. Read arguments (stack index 1 = first arg, 2 = second, etc.)
 *       const char* msg = luaL_checkstring(L, 1);   // error if not string
 *       int amount      = luaL_checkinteger(L, 2);  // error if not int
 *
 *       // 3. Do the work
 *       LOG_INFO(msg);
 *
 *       // 4. Push return values
 *       lua_pushinteger(L, 42);
 *
 *       // 5. Return the number of values pushed
 *       return 1;   // 1 return value
 *   }
 *
 * luaL_check* functions:
 *   • luaL_checkstring(L, n)  — requires arg n to be a string; errors otherwise
 *   • luaL_checkinteger(L, n) — requires arg n to be an integer
 *   • luaL_optnumber(L, n, d) — optional number, default d if missing
 *
 * ============================================================================
 * TEACHING NOTE — The Lua Stack and Error Handling
 * ============================================================================
 *
 * The Lua stack state must be "balanced" across function calls.
 * Rule of thumb:
 *   • Every lua_push*() increments the stack top.
 *   • lua_pop(L, n) removes n values.
 *   • lua_pcall(L, nargs, nret, 0) removes nargs and pushes nret (or 1 error).
 *
 * After a FAILED lua_pcall:
 *   Stack = [ error_message_string ]
 *   You must pop this string after reading it, otherwise the stack grows
 *   without bound ("stack overflow" crash after many errors).
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 */

#include "engine/scripting/LuaEngine.hpp"
#include "engine/core/Logger.hpp"
#include "engine/ecs/ECS.hpp"

#include <sstream>      // std::ostringstream
#include <cassert>      // assert()


// ===========================================================================
// Singleton
// ===========================================================================

LuaEngine& LuaEngine::Get()
{
    static LuaEngine instance;
    return instance;
}


// ===========================================================================
// Lifecycle
// ===========================================================================

bool LuaEngine::Init(World* world)
{
    if (m_L)
    {
        LOG_INFO("LuaEngine: already initialised, skipping.");
        return true;
    }

    m_world = world;

    // -----------------------------------------------------------------------
    // Step 1 — Create a new Lua state
    // -----------------------------------------------------------------------
    // TEACHING NOTE: luaL_newstate() is equivalent to:
    //   lua_newstate(luaL_alloc, NULL)
    // where luaL_alloc is the default allocator (wraps realloc/free).
    //
    // You can provide a CUSTOM allocator to track Lua's memory usage:
    //   lua_newstate(myAllocFunc, myUserData);
    // Useful for profiling, or in embedded systems with fixed-size pools.
    m_L = luaL_newstate();
    if (!m_L)
    {
        LOG_ERROR("LuaEngine: luaL_newstate() failed — out of memory?");
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 2 — Open standard libraries
    // -----------------------------------------------------------------------
    // TEACHING NOTE: luaL_openlibs() loads all standard Lua 5.4 libraries:
    //   • base    : print, type, error, assert, pairs, ipairs, etc.
    //   • math    : math.sin, math.random, math.floor, etc.
    //   • string  : string.format, string.find, string.sub, etc.
    //   • table   : table.insert, table.remove, table.sort, etc.
    //   • io      : io.open, io.read, io.write — file I/O
    //   • os      : os.time, os.clock, os.exit, etc.
    //   • package : require() — module loading
    //   • coroutine: coroutine.create, coroutine.resume, etc.
    //   • utf8    : UTF-8 aware string operations
    //   • debug   : debug.traceback, etc.
    //
    // SECURITY NOTE: In a sandboxed mod system you would NOT open io/os/package
    // to prevent scripts from reading files or executing shell commands.
    // For this engine all libraries are available (no untrusted scripts).
    luaL_openlibs(m_L);

    // -----------------------------------------------------------------------
    // Step 3 — Register game engine bindings
    // -----------------------------------------------------------------------
    RegisterEngineBindings();

    LOG_INFO("LuaEngine: Lua 5.4 interpreter initialised successfully.");
    LOG_INFO("LuaEngine: " + std::string(LUA_VERSION) + " embedded.");
    return true;
}

void LuaEngine::Shutdown()
{
    if (!m_L) { return; }

    // lua_close() performs a full garbage-collection cycle, then frees all
    // memory allocated by Lua (tables, strings, closures, userdata).
    lua_close(m_L);
    m_L = nullptr;
    m_world = nullptr;
    LOG_INFO("LuaEngine: Lua state closed and memory freed.");
}


// ===========================================================================
// Script Loading
// ===========================================================================

bool LuaEngine::LoadScript(const std::string& filename)
{
    if (!m_L)
    {
        LOG_ERROR("LuaEngine::LoadScript: not initialised. Call Init() first.");
        return false;
    }

    LOG_INFO("LuaEngine: loading script '" + filename + "'");

    // -----------------------------------------------------------------------
    // TEACHING NOTE — luaL_loadfile internals
    // ─────────────────────────────────────────
    // luaL_loadfile(L, filename):
    //   1. Opens the file.
    //   2. Lexes and parses the Lua source into bytecode.
    //   3. Wraps the bytecode in a Lua closure.
    //   4. Pushes the closure onto the stack.
    //   5. Returns LUA_OK (0) on success, or an error code.
    //
    // The file is NOT executed yet — just compiled.  This separation lets
    // us check for syntax errors before running anything.
    // -----------------------------------------------------------------------
    const int loadResult = luaL_loadfile(m_L, filename.c_str());
    if (loadResult != LUA_OK)
    {
        HandleLuaError("LoadScript('" + filename + "') — compile error");
        return false;
    }

    // -----------------------------------------------------------------------
    // Now execute the compiled chunk.
    // lua_pcall(L, nargs, nret, errfunc):
    //   • nargs=0   : the closure takes no arguments
    //   • nret=0    : we don't expect any return values from the top-level chunk
    //   • errfunc=0 : no error handler function (0 = use default)
    //
    // A non-zero return means an error was raised (runtime error, etc.).
    // The error message is at the top of the stack.
    // -----------------------------------------------------------------------
    const int callResult = lua_pcall(m_L, 0, 0, 0);
    if (callResult != LUA_OK)
    {
        HandleLuaError("LoadScript('" + filename + "') — runtime error");
        return false;
    }

    LOG_INFO("LuaEngine: script '" + filename + "' loaded and executed OK.");
    return true;
}

bool LuaEngine::ExecuteString(const std::string& luaCode)
{
    if (!m_L) { return false; }

    // luaL_loadstring compiles a C string as a Lua chunk.
    // luaL_dostring is a macro shorthand but doesn't give us the error text.
    if (luaL_loadstring(m_L, luaCode.c_str()) != LUA_OK)
    {
        HandleLuaError("ExecuteString — compile error");
        return false;
    }
    if (lua_pcall(m_L, 0, 0, 0) != LUA_OK)
    {
        HandleLuaError("ExecuteString — runtime error");
        return false;
    }
    return true;
}


// ===========================================================================
// Calling Lua Functions from C++
// ===========================================================================

bool LuaEngine::CallFunction(const std::string& funcName)
{
    if (!m_L) { return false; }

    // Push the Lua global with this name onto the stack.
    // lua_getglobal returns the TYPE of the pushed value.
    const int luaType = lua_getglobal(m_L, funcName.c_str());
    if (luaType != LUA_TFUNCTION)
    {
        // Not a function — pop whatever was pushed, return false.
        lua_pop(m_L, 1);
        LOG_ERROR("LuaEngine::CallFunction: '" + funcName + "' is not a function.");
        return false;
    }

    // lua_pcall(L, nargs=0, nret=0, errfunc=0)
    if (lua_pcall(m_L, 0, 0, 0) != LUA_OK)
    {
        HandleLuaError("CallFunction('" + funcName + "')");
        return false;
    }
    return true;
}

bool LuaEngine::CallFunction(const std::string& funcName, int arg)
{
    if (!m_L) { return false; }

    if (lua_getglobal(m_L, funcName.c_str()) != LUA_TFUNCTION)
    {
        lua_pop(m_L, 1);
        LOG_ERROR("LuaEngine::CallFunction: '" + funcName + "' is not a function.");
        return false;
    }

    // Push the single integer argument.
    lua_pushinteger(m_L, static_cast<lua_Integer>(arg));

    if (lua_pcall(m_L, 1, 0, 0) != LUA_OK)
    {
        HandleLuaError("CallFunction('" + funcName + "', int)");
        return false;
    }
    return true;
}

bool LuaEngine::CallFunction(const std::string& funcName, const std::string& arg)
{
    if (!m_L) { return false; }

    if (lua_getglobal(m_L, funcName.c_str()) != LUA_TFUNCTION)
    {
        lua_pop(m_L, 1);
        LOG_ERROR("LuaEngine::CallFunction: '" + funcName + "' is not a function.");
        return false;
    }

    // lua_pushstring copies the string data — safe even if arg is a temporary.
    lua_pushstring(m_L, arg.c_str());

    if (lua_pcall(m_L, 1, 0, 0) != LUA_OK)
    {
        HandleLuaError("CallFunction('" + funcName + "', string)");
        return false;
    }
    return true;
}

bool LuaEngine::CallFunction(const std::string& funcName,
                              int arg1, const std::string& arg2)
{
    if (!m_L) { return false; }

    if (lua_getglobal(m_L, funcName.c_str()) != LUA_TFUNCTION)
    {
        lua_pop(m_L, 1);
        return false;
    }

    lua_pushinteger(m_L, static_cast<lua_Integer>(arg1));
    lua_pushstring(m_L, arg2.c_str());

    if (lua_pcall(m_L, 2, 0, 0) != LUA_OK)
    {
        HandleLuaError("CallFunction('" + funcName + "', int, string)");
        return false;
    }
    return true;
}

bool LuaEngine::CallFunctionRet(const std::string& funcName,
                                 int arg1, int arg2,
                                 int& outResult)
{
    if (!m_L) { return false; }

    if (lua_getglobal(m_L, funcName.c_str()) != LUA_TFUNCTION)
    {
        lua_pop(m_L, 1);
        return false;
    }

    lua_pushinteger(m_L, static_cast<lua_Integer>(arg1));
    lua_pushinteger(m_L, static_cast<lua_Integer>(arg2));

    // nret=1 — we expect one integer return value.
    if (lua_pcall(m_L, 2, 1, 0) != LUA_OK)
    {
        HandleLuaError("CallFunctionRet('" + funcName + "')");
        return false;
    }

    // Read the return value from the top of the stack (-1 = top).
    outResult = static_cast<int>(lua_tointeger(m_L, -1));
    lua_pop(m_L, 1);   // pop the return value
    return true;
}


// ===========================================================================
// Registering C++ Functions for Lua
// ===========================================================================

void LuaEngine::RegisterFunction(const std::string& name, lua_CFunction fn)
{
    if (!m_L) { return; }

    // TEACHING NOTE: lua_register is a convenience macro:
    //   lua_pushcfunction(L, fn);     // push a C closure onto the stack
    //   lua_setglobal(L, name);       // pop it and store as global 'name'
    lua_register(m_L, name.c_str(), fn);
    LOG_INFO("LuaEngine: registered C function '" + name + "' → Lua.");
}


// ===========================================================================
// Global Variable Access
// ===========================================================================

LuaTable LuaEngine::GetTableGlobal(const std::string& name)
{
    // Push the global (which we expect to be a table) onto the stack.
    const int luaType = lua_getglobal(m_L, name.c_str());

    LuaTable tbl;
    tbl.L     = m_L;
    tbl.index = lua_gettop(m_L);   // absolute index of the just-pushed value

    if (luaType != LUA_TTABLE)
    {
        LOG_ERROR("LuaEngine::GetTableGlobal: '" + name + "' is not a table.");
        // Leave nil on the stack — caller must pop it.  tbl.index is valid
        // but accessing it will return defaults (HasKey → false).
    }
    return tbl;
}


// ===========================================================================
// Error Handling
// ===========================================================================

void LuaEngine::HandleLuaError(const std::string& context)
{
    if (!m_L) { return; }

    // After a failed pcall, the error message (a string) is on top of the stack.
    // lua_tostring(-1) reads it without popping.
    const char* errMsg = lua_tostring(m_L, -1);
    if (errMsg)
    {
        LOG_ERROR("LuaEngine [" + context + "]: " + std::string(errMsg));
    }
    else
    {
        LOG_ERROR("LuaEngine [" + context + "]: unknown error (non-string error object).");
    }

    // Pop the error message.
    lua_pop(m_L, 1);
}


// ===========================================================================
// LuaTable Implementation
// ===========================================================================

int LuaTable::GetInt(const std::string& key, int defaultValue) const
{
    if (!L || !lua_istable(L, index)) { return defaultValue; }

    // lua_getfield(L, table_index, key) pushes t[key] onto the stack.
    lua_getfield(L, index, key.c_str());
    int result = defaultValue;

    if (lua_isnumber(L, -1))
    {
        result = static_cast<int>(lua_tointeger(L, -1));
    }
    lua_pop(L, 1);   // pop t[key]
    return result;
}

float LuaTable::GetFloat(const std::string& key, float defaultValue) const
{
    if (!L || !lua_istable(L, index)) { return defaultValue; }

    lua_getfield(L, index, key.c_str());
    float result = defaultValue;

    if (lua_isnumber(L, -1))
    {
        result = static_cast<float>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);
    return result;
}

std::string LuaTable::GetString(const std::string& key,
                                 const std::string& defaultValue) const
{
    if (!L || !lua_istable(L, index)) { return defaultValue; }

    lua_getfield(L, index, key.c_str());
    std::string result = defaultValue;

    if (lua_isstring(L, -1))
    {
        const char* s = lua_tostring(L, -1);
        if (s) { result = s; }
    }
    lua_pop(L, 1);
    return result;
}

bool LuaTable::GetBool(const std::string& key, bool defaultValue) const
{
    if (!L || !lua_istable(L, index)) { return defaultValue; }

    lua_getfield(L, index, key.c_str());
    bool result = defaultValue;

    if (lua_isboolean(L, -1))
    {
        result = (lua_toboolean(L, -1) != 0);
    }
    lua_pop(L, 1);
    return result;
}

bool LuaTable::HasKey(const std::string& key) const
{
    if (!L || !lua_istable(L, index)) { return false; }

    lua_getfield(L, index, key.c_str());
    // lua_isnil returns true if the value is nil (key absent).
    const bool exists = !lua_isnil(L, -1);
    lua_pop(L, 1);
    return exists;
}


// ===========================================================================
// Engine Bindings — C functions callable from Lua
// ===========================================================================
//
// TEACHING NOTE — Anonymous Namespace for Binding Functions
// ──────────────────────────────────────────────────────────
// We put the binding functions in an anonymous namespace so they have
// INTERNAL LINKAGE — they are not visible outside this translation unit.
// This prevents name collisions with other modules and is good hygiene.
//
// Alternative: declare them as static functions (also gives internal linkage).
// The anonymous namespace is preferred in modern C++ as static has a
// different (and confusing) meaning for class members.
// ──────────────────────────────────────────────────────────────────────────

namespace {

// ---------------------------------------------------------------------------
// Helper: retrieve the World* stored in the Lua registry.
// -----------------------------------------------------------------------
// TEACHING NOTE — Lua Registry
// ──────────────────────────────
// The Lua registry is a special table accessible only from C (not from Lua
// scripts).  It is located at pseudo-index LUA_REGISTRYINDEX.
//
// We store the World pointer as a light userdata in the registry so that all
// binding functions can find it without needing a global C++ pointer.
//
// Light userdata: a raw void* stored in Lua's value system without GC tracking.
// -----------------------------------------------------------------------
static World* GetWorldFromRegistry(lua_State* L)
{
    // Retrieve the World pointer stored under the key "engine_world".
    lua_getfield(L, LUA_REGISTRYINDEX, "engine_world");
    World* w = static_cast<World*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return w;   // may be nullptr if SetWorld() was never called
}

// ---------------------------------------------------------------------------
// game_log(message: string) → void
// ---------------------------------------------------------------------------
// Lua usage:  game_log("Hello from a quest script!")
// ---------------------------------------------------------------------------
static int binding_game_log(lua_State* L)
{
    // TEACHING NOTE: luaL_checkstring
    // ──────────────────────────────────
    // luaL_checkstring raises a Lua error if argument 1 is not a string.
    // The error message is descriptive:
    //   "bad argument #1 to 'game_log' (string expected, got number)"
    // This is the recommended way to validate binding arguments — it gives
    // useful feedback to script authors.
    const char* msg = luaL_checkstring(L, 1);
    LOG_INFO(std::string("[Lua] ") + msg);
    return 0;   // 0 return values
}

// ---------------------------------------------------------------------------
// game_get_player_hp() → integer
// ---------------------------------------------------------------------------
// Lua usage:  local hp = game_get_player_hp()
// ---------------------------------------------------------------------------
static int binding_game_get_player_hp(lua_State* L)
{
    World* world = GetWorldFromRegistry(L);
    if (!world)
    {
        lua_pushinteger(L, 0);
        return 1;
    }

    // Find the player entity — by convention the player is the entity
    // with a NameComponent whose name is "Player".
    int hp = 0;
    world->ForEach<NameComponent, HealthComponent>(
        [&](EntityID /*eid*/, NameComponent& name, HealthComponent& health)
        {
            if (name.displayName == "Player" || name.displayName == "Noctis")
            {
                hp = health.current;
            }
        });

    // Push the result and return 1 (one return value).
    lua_pushinteger(L, static_cast<lua_Integer>(hp));
    return 1;
}

// ---------------------------------------------------------------------------
// game_heal_player(amount: integer) → void
// ---------------------------------------------------------------------------
// Lua usage:  game_heal_player(50)
// ---------------------------------------------------------------------------
static int binding_game_heal_player(lua_State* L)
{
    const int amount = static_cast<int>(luaL_checkinteger(L, 1));

    World* world = GetWorldFromRegistry(L);
    if (!world) { return 0; }

    world->ForEach<NameComponent, HealthComponent>(
        [&](EntityID /*eid*/, NameComponent& name, HealthComponent& health)
        {
            if (name.displayName == "Player" || name.displayName == "Noctis")
            {
                health.current = std::min(health.current + amount, health.maximum);
                LOG_INFO("[Lua] Healed player for " + std::to_string(amount) + " HP.");
            }
        });
    return 0;
}

// ---------------------------------------------------------------------------
// game_get_gold() → integer
// ---------------------------------------------------------------------------
// Lua usage:  local coins = game_get_gold()
// ---------------------------------------------------------------------------
static int binding_game_get_gold(lua_State* L)
{
    World* world = GetWorldFromRegistry(L);
    if (!world)
    {
        lua_pushinteger(L, 0);
        return 1;
    }

    int gold = 0;
    world->ForEach<NameComponent, InventoryComponent>(
        [&](EntityID /*eid*/, NameComponent& name, InventoryComponent& inv)
        {
            if (name.displayName == "Player" || name.displayName == "Noctis")
            {
                gold = inv.gil;
            }
        });

    lua_pushinteger(L, static_cast<lua_Integer>(gold));
    return 1;
}

// ---------------------------------------------------------------------------
// game_add_item(item_name: string, quantity: integer) → void
// ---------------------------------------------------------------------------
// Lua usage:  game_add_item("Potion", 3)
// ---------------------------------------------------------------------------
static int binding_game_add_item(lua_State* L)
{
    const char* itemName = luaL_checkstring(L, 1);
    const int   quantity = static_cast<int>(luaL_optinteger(L, 2, 1));

    World* world = GetWorldFromRegistry(L);
    if (!world) { return 0; }

    // TEACHING NOTE — Finding the player and modifying their inventory.
    // The item ID is looked up by name in a simple linear search.
    // A production engine would have an ItemDatabase with O(1) name lookup.
    world->ForEach<NameComponent, InventoryComponent>(
        [&](EntityID /*eid*/, NameComponent& name, InventoryComponent& inv)
        {
            if (name.displayName == "Player" || name.displayName == "Noctis")
            {
                // Add to existing stack if item already present.
                bool found = false;
                for (auto& stack : inv.items)
                {
                    if (stack.itemId == 0 && stack.quantity == 0) { continue; }
                    // Use itemId 0 as a placeholder; real code would look up by name.
                    // For now, create a new stack with itemId = hash of name.
                    // (A real engine would use ItemDatabase::FindByName())
                    (void)stack;
                }

                if (!found && inv.items.size() < static_cast<size_t>(MAX_INVENTORY_SLOTS))
                {
                    ItemStack newStack;
                    // Simple hash of item name as a temporary ID.
                    uint32_t id = 0;
                    for (char c : std::string(itemName)) { id = id * 31 + static_cast<uint32_t>(c); }
                    newStack.itemId   = id;
                    newStack.quantity = quantity;
                    inv.items.push_back(newStack);
                    LOG_INFO("[Lua] Added " + std::to_string(quantity)
                             + "x " + std::string(itemName) + " to inventory.");
                }
            }
        });
    return 0;
}

// ---------------------------------------------------------------------------
// game_start_combat(enemy_id: integer) → void
// ---------------------------------------------------------------------------
// Lua usage:  game_start_combat(101)   -- start fight with enemy ID 101
// ---------------------------------------------------------------------------
static int binding_game_start_combat(lua_State* L)
{
    const int enemyId = static_cast<int>(luaL_checkinteger(L, 1));

    // In a full implementation this would fire a CombatStartEvent via EventBus.
    // For the binding layer we log it; the game logic hooks into this via event.
    LOG_INFO("[Lua] game_start_combat: initiating combat with enemy ID "
             + std::to_string(enemyId));

    // Store the requested enemy ID as a Lua global so the CombatSystem can
    // read it on the next update.
    lua_pushinteger(L, static_cast<lua_Integer>(enemyId));
    lua_setglobal(L, "_pending_combat_enemy_id");

    return 0;
}

// ---------------------------------------------------------------------------
// game_complete_quest(quest_id: integer) → void
// ---------------------------------------------------------------------------
// Lua usage:  game_complete_quest(5)
// ---------------------------------------------------------------------------
static int binding_game_complete_quest(lua_State* L)
{
    const int questId = static_cast<int>(luaL_checkinteger(L, 1));

    World* world = GetWorldFromRegistry(L);
    if (!world) { return 0; }

    world->ForEach<NameComponent, QuestComponent>(
        [&](EntityID /*eid*/, NameComponent& name, QuestComponent& quests)
        {
            if (name.displayName == "Player" || name.displayName == "Noctis")
            {
                for (auto& entry : quests.entries)
                {
                    if (entry.questId == static_cast<uint32_t>(questId))
                    {
                        entry.isCompleted = true;
                        LOG_INFO("[Lua] Quest " + std::to_string(questId)
                                 + " marked complete.");
                    }
                }
            }
        });
    return 0;
}

// ---------------------------------------------------------------------------
// game_show_message(text: string) → void
// ---------------------------------------------------------------------------
// Lua usage:  game_show_message("A mysterious stranger approaches...")
// ---------------------------------------------------------------------------
static int binding_game_show_message(lua_State* L)
{
    const char* text = luaL_checkstring(L, 1);

    // Store the message in a Lua global for the UI system to display.
    lua_pushstring(L, text);
    lua_setglobal(L, "_pending_ui_message");

    LOG_INFO("[Lua] game_show_message: \"" + std::string(text) + "\"");
    return 0;
}

}   // anonymous namespace


// ===========================================================================
// RegisterEngineBindings
// ===========================================================================

void LuaEngine::RegisterEngineBindings()
{
    if (!m_L) { return; }

    // -----------------------------------------------------------------------
    // Store the World pointer in the Lua registry so binding functions can
    // retrieve it without capturing a C++ global.
    //
    // TEACHING NOTE — Lua Registry Storage
    // ──────────────────────────────────────
    // lua_pushlightuserdata(L, ptr) pushes a raw pointer as a "light userdata"
    // value.  It is NOT garbage-collected (the GC doesn't know about it).
    // It is solely the programmer's responsibility to ensure the pointer
    // remains valid for as long as the Lua state exists.
    //
    // Here m_world is owned by the Application which outlives the LuaEngine,
    // so the pointer remains valid.
    // -----------------------------------------------------------------------
    lua_pushlightuserdata(m_L, static_cast<void*>(m_world));
    lua_setfield(m_L, LUA_REGISTRYINDEX, "engine_world");

    // Register all game functions so Lua scripts can call them.
    lua_register(m_L, "game_log",             binding_game_log);
    lua_register(m_L, "game_get_player_hp",   binding_game_get_player_hp);
    lua_register(m_L, "game_heal_player",     binding_game_heal_player);
    lua_register(m_L, "game_get_gold",        binding_game_get_gold);
    lua_register(m_L, "game_add_item",        binding_game_add_item);
    lua_register(m_L, "game_start_combat",    binding_game_start_combat);
    lua_register(m_L, "game_complete_quest",  binding_game_complete_quest);
    lua_register(m_L, "game_show_message",    binding_game_show_message);

    // -----------------------------------------------------------------------
    // Expose useful constants to Lua so scripts don't need magic numbers.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Exposing C++ Constants to Lua
    // ──────────────────────────────────────────────
    // We push each constant as a Lua global.  In Lua, uppercase globals by
    // convention (matching C++ ALL_CAPS) signal "do not modify these."
    // -----------------------------------------------------------------------
    lua_pushinteger(m_L, MAX_INVENTORY_SLOTS);
    lua_setglobal(m_L, "MAX_INVENTORY_SLOTS");

    lua_pushinteger(m_L, MAX_PARTY_SIZE);
    lua_setglobal(m_L, "MAX_PARTY_SIZE");

    lua_pushnumber(m_L, static_cast<lua_Number>(COMBAT_FLEE_CHANCE));
    lua_setglobal(m_L, "COMBAT_FLEE_CHANCE");

    lua_pushinteger(m_L, BASE_XP_PER_LEVEL);
    lua_setglobal(m_L, "BASE_XP_PER_LEVEL");

    LOG_INFO("LuaEngine: all engine bindings registered ("
             + std::to_string(8) + " functions, 4 constants).");
}
