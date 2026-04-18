/**
 * @file LuaEngine.hpp
 * @brief Lua 5.4 scripting engine integration for the educational RPG engine.
 *
 * ============================================================================
 * TEACHING NOTE — Why Scripting in a Game Engine?
 * ============================================================================
 *
 * Game logic written directly in C++ has several drawbacks for designers and
 * educators:
 *  • Long compile-edit-test cycles (minutes per change).
 *  • C++ requires knowing memory management, the type system, etc.
 *  • Designers / writers who are not C++ programmers cannot contribute.
 *
 * An embedded scripting language solves all three problems:
 *  • Scripts are loaded and executed at RUNTIME — no recompilation needed.
 *  • Lua's syntax is simpler than C++ (closer to Python or pseudocode).
 *  • Non-programmers can write quest scripts, dialogue trees, or balance
 *    values using Lua without touching the engine code.
 *
 * The pattern is: C++ provides the "engine" (fast, safe, compiled)
 *                 Lua  provides the "game logic" (flexible, hot-reloadable)
 *
 * This architecture is used by:
 *   • World of Warcraft (add-on system)
 *   • Roblox (entire gameplay)
 *   • CryEngine, Unreal (scripting subsystems)
 *   • Factorio, Civilization V/VI, Garry's Mod
 *
 * ============================================================================
 * TEACHING NOTE — How Lua Embeds into C++
 * ============================================================================
 *
 * Lua is designed to be embedded.  The C API is based on a STACK:
 *
 *   C calls   → lua_push*(L, value)   to send data INTO Lua
 *   C reads   → lua_to*(L, index)     to read  data FROM Lua's stack
 *   Lua calls → C functions registered with lua_register()
 *
 * Every Lua-to-C function (lua_CFunction) has this signature:
 *   static int myFunc(lua_State* L) { ... return nResults; }
 *
 * The stack index convention:
 *   Positive indices: 1 = bottom of stack (first pushed)
 *   Negative indices: -1 = top of stack   (last pushed / most recent)
 *
 * Example — calling Lua from C++:
 * @code
 *   lua_getglobal(L, "on_combat_start");   // push Lua function
 *   lua_pushinteger(L, enemyID);           // push argument
 *   lua_pcall(L, 1, 0, 0);                // call with 1 arg, 0 returns
 * @endcode
 *
 * Example — C function callable from Lua:
 * @code
 *   static int game_log(lua_State* L) {
 *     const char* msg = lua_tostring(L, 1);  // get first arg
 *     LOG_INFO(msg);
 *     return 0;  // no return values pushed
 *   }
 *   lua_register(L, "game_log", game_log);
 * @endcode
 *
 * ============================================================================
 * TEACHING NOTE — lua_pcall vs lua_call
 * ============================================================================
 *
 * lua_call(L, nargs, nret)   — calls a Lua function.  If an error occurs,
 *                              it PROPAGATES the error (like an uncaught throw).
 *                              This usually terminates the program.
 *
 * lua_pcall(L, nargs, nret, msgh) — "protected call" analogous to try/catch.
 *                                    On error it returns a non-zero error code
 *                                    and pushes the error message onto the stack.
 *                                    The program continues.
 *
 * ALWAYS use lua_pcall in game code.  A buggy quest script should never
 * crash the entire game.  The LuaEngine wraps all calls in lua_pcall and
 * logs errors via the Logger system.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 */

#pragma once

// ---------------------------------------------------------------------------
// Standard library
// ---------------------------------------------------------------------------
#include <string>           // std::string — function names, error messages
#include <unordered_map>    // std::unordered_map — script cache
#include <functional>       // std::function — optional C++ callback wrappers
#include <vector>           // std::vector — LuaTable field list
#include <stdexcept>        // std::runtime_error — Init failure
#include <type_traits>      // std::is_same_v, std::is_integral_v — type dispatch

// ---------------------------------------------------------------------------
// Lua 5.4 C API
// ---------------------------------------------------------------------------
// TEACHING NOTE — extern "C" for C Headers
// ──────────────────────────────────────────
// Lua is written in C, not C++.  C and C++ use different "name mangling" for
// symbols in object files.  C++ encodes overload information into the symbol
// name (e.g., `_ZN3foo3barEi`); C does not (just `bar`).
//
// Without extern "C", the C++ linker would look for C++-mangled symbols that
// don't exist in the Lua library, causing linker errors.
//
// `extern "C" { ... }` tells the C++ compiler to use C-style (unmangled)
// linkage for everything inside the block.
//
// TEACHING NOTE — Multi-version Lua include paths
// ─────────────────────────────────────────────────
// The exact path for Lua headers varies by platform and installation:
//   • Linux (apt): lua5.4/lua.h   (package: liblua5.4-dev)
//   • Windows (bundled): lua.h    (headers placed in Lua/include/ in the repo)
//   • macOS (brew): lua.h         (in HOMEBREW_PREFIX/include/lua5.x/)
//
// We use a cascade of #if checks so the file compiles on every supported
// platform without manual per-platform configuration.
extern "C" {
#if defined(_WIN32) && __has_include(<lua.h>)
// Windows with bundled Lua headers (Lua/include/ added to include path by CMake)
#  include <lua.h>
#  include <lualib.h>
#  include <lauxlib.h>
#elif __has_include(<lua5.5/lua.h>)
// Linux/macOS with Lua 5.5 installed system-wide
#  include <lua5.5/lua.h>
#  include <lua5.5/lualib.h>
#  include <lua5.5/lauxlib.h>
#elif __has_include(<lua5.4/lua.h>)
// Linux/macOS with Lua 5.4 installed system-wide (most common)
#  include <lua5.4/lua.h>
#  include <lua5.4/lualib.h>
#  include <lua5.4/lauxlib.h>
#elif __has_include(<lua.h>)
// Generic fallback — lua.h is directly on the include path
#  include <lua.h>
#  include <lualib.h>
#  include <lauxlib.h>
#else
#  error "Lua headers not found. See LuaEngine.hpp for setup instructions."
#endif
}

// ---------------------------------------------------------------------------
// Engine headers
// ---------------------------------------------------------------------------
#include "engine/core/Types.hpp"
#include "engine/ecs/ECS.hpp"   // World — needed for game_get_player_hp etc.


// ===========================================================================
// LuaTable — Convenience Wrapper
// ===========================================================================

/**
 * @struct LuaTable
 * @brief A lightweight view into a Lua table that is currently on the stack.
 *
 * TEACHING NOTE — Lua Tables
 * ────────────────────────────
 * In Lua, tables are the ONLY data structure.  They function as:
 *   • Arrays:      t = {10, 20, 30}      t[1] == 10
 *   • Dictionaries: t = {name="Noctis"}  t["name"] == "Noctis"
 *   • Objects:      t.attack = 50        (syntactic sugar for t["attack"])
 *   • Modules:      return { foo=bar }
 *
 * The LuaTable struct helps C++ code read Lua tables returned from functions
 * or stored as globals.  It holds the stack index of the table and provides
 * typed getters.
 *
 * Usage:
 * @code
 *   LuaTable tbl = engine.GetTableGlobal("player_stats");
 *   int hp    = tbl.GetInt("hp", 100);    // default 100 if key missing
 *   float spd = tbl.GetFloat("speed", 1.0f);
 *   std::string name = tbl.GetString("name", "Unknown");
 * @endcode
 */
struct LuaTable
{
    lua_State* L;      ///< The Lua interpreter state owning this table.
    int        index;  ///< Stack index of the table (positive = absolute).

    /**
     * @brief Read an integer field from the table.
     * @param key           Field name in the Lua table.
     * @param defaultValue  Value returned if the key is absent or wrong type.
     */
    [[nodiscard]] int GetInt(const std::string& key, int defaultValue = 0) const;

    /**
     * @brief Read a float / number field from the table.
     * @param key           Field name.
     * @param defaultValue  Fallback value.
     */
    [[nodiscard]] float GetFloat(const std::string& key, float defaultValue = 0.0f) const;

    /**
     * @brief Read a string field from the table.
     * @param key           Field name.
     * @param defaultValue  Fallback value.
     */
    [[nodiscard]] std::string GetString(const std::string& key,
                                        const std::string& defaultValue = "") const;

    /**
     * @brief Read a boolean field from the table.
     * @param key           Field name.
     * @param defaultValue  Fallback value.
     */
    [[nodiscard]] bool GetBool(const std::string& key, bool defaultValue = false) const;

    /**
     * @brief Check whether a key exists in the table.
     * @param key  Field name to test.
     * @return     true if the key is present and non-nil.
     */
    [[nodiscard]] bool HasKey(const std::string& key) const;
};


// ===========================================================================
// LuaEngine
// ===========================================================================

/**
 * @class LuaEngine
 * @brief Singleton that manages the Lua 5.4 interpreter and bridges it to C++.
 *
 * TEACHING NOTE — Engine ↔ Script Communication
 * ───────────────────────────────────────────────
 * There are two directions of communication:
 *
 *   C++ → Lua: Call a Lua function from C++.
 *     Use case: trigger an NPC's dialogue callback, fire a quest event.
 *     API: LuaEngine::CallFunction("on_quest_complete", questId)
 *
 *   Lua → C++: Call a C++ function from Lua.
 *     Use case: a Lua script wants to heal the player.
 *     API: In Lua:   game_heal_player(50)
 *          In C++:   LuaEngine::RegisterFunction("game_heal_player", &fn)
 *
 * The LuaEngine::RegisterEngineBindings() method registers all game-relevant
 * C++ functions so Lua scripts can call them freely.
 *
 * TEACHING NOTE — lua_State Lifecycle
 * ─────────────────────────────────────
 * A lua_State (often called "L") represents one Lua interpreter instance.
 * It owns:
 *   • The global variable table
 *   • The call stack
 *   • The garbage collector
 *   • Loaded modules
 *
 * luaL_newstate()  → create a new state (allocates memory)
 * lua_close(L)     → destroy the state and free all memory
 *
 * We create ONE state on Init() and share it for all scripts.
 * Multiple states are possible (e.g., sandboxed scripts), but one is simpler.
 */
class LuaEngine
{
public:
    // -----------------------------------------------------------------------
    // Singleton access
    // -----------------------------------------------------------------------

    /// @brief Return the global LuaEngine instance.
    static LuaEngine& Get();

    LuaEngine(const LuaEngine&)            = delete;
    LuaEngine& operator=(const LuaEngine&) = delete;

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    /**
     * @brief Create the Lua state, open standard libraries, register engine bindings.
     *
     * Steps performed:
     *   1. luaL_newstate()  — allocate a fresh Lua interpreter.
     *   2. luaL_openlibs()  — open the Lua standard libraries (math, string,
     *                         table, io, os…).  In production you might open
     *                         only a safe subset to sandbox scripts.
     *   3. RegisterEngineBindings() — expose C++ game functions to Lua.
     *
     * @param world  Pointer to the ECS World — needed so Lua bindings can
     *               modify entity/component data.  May be nullptr if not yet
     *               available (bindings that need it will log errors).
     * @return true on success.
     */
    bool Init(World* world = nullptr);

    /**
     * @brief Destroy the Lua state and free all associated memory.
     *
     * After Shutdown() all lua_State* pointers held externally are invalid.
     */
    void Shutdown();

    /**
     * @brief Attach a World instance so Lua bindings can access ECS data.
     *
     * Call this after Init() if the World is not ready at Init() time, or
     * whenever the active World changes (e.g. between scenes).  The LuaEngine
     * does NOT own the World; it only stores a non-owning pointer.
     *
     * TEACHING NOTE — Non-Owning Pointers
     * ─────────────────────────────────────
     * A raw pointer T* in C++ is non-owning by convention (the pointee is
     * owned elsewhere and outlives this use).  For ownership semantics use
     * std::unique_ptr (sole owner) or std::shared_ptr (shared ownership).
     * Here the World is owned by the Application, which outlives LuaEngine.
     *
     * TEACHING NOTE — Why we ALSO update the Lua registry here
     * ──────────────────────────────────────────────────────────
     * RegisterEngineBindings() stores the initial World pointer in the Lua
     * registry under the key "engine_world" so that every C binding can
     * retrieve it without a global C++ variable.  If SetWorld() only updated
     * m_world but NOT the registry, binding functions called after a scene
     * change would silently operate on the old (possibly destroyed) World —
     * a classic dangling-pointer bug that is almost impossible to diagnose
     * at runtime.
     *
     * The fix: mirror every assignment to m_world with a matching registry
     * update.  lua_pushlightuserdata / lua_setfield atomically replaces the
     * stored pointer while the Lua state is still valid.
     *
     * @param world  Pointer to the new active ECS World (may be nullptr to
     *               clear the binding before a scene unload).
     */
    void SetWorld(World* world);

    // -----------------------------------------------------------------------
    // Script loading
    // -----------------------------------------------------------------------

    /**
     * @brief Load and execute a Lua script file.
     *
     * TEACHING NOTE — luaL_loadfile vs luaL_dofile
     * ──────────────────────────────────────────────
     * luaL_dofile(L, path)  — loads AND runs the file.  Simple but provides
     *                          less control over error handling.
     *
     * luaL_loadfile(L, path) — compiles the file into a Lua "chunk" (a closure)
     *                           and pushes it onto the stack.  Does NOT run it.
     *
     * lua_pcall(L, 0, 0, 0) — pops and executes the chunk with error protection.
     *
     * We use loadfile + pcall so we can:
     *   1. Get a detailed error message on failure.
     *   2. Have one centralised error-handling path.
     *
     * @param filename  Path to the .lua file (relative to working directory).
     * @return true if the script loaded and ran without errors.
     */
    bool LoadScript(const std::string& filename);

    /**
     * @brief Execute a Lua string directly (useful for testing and REPL).
     *
     * @param luaCode  A string of Lua source code to execute.
     * @return true on success.
     */
    bool ExecuteString(const std::string& luaCode);

    // -----------------------------------------------------------------------
    // Calling Lua functions from C++
    // -----------------------------------------------------------------------

    /**
     * @brief Call a Lua function by name with no arguments, no return value.
     *
     * @param funcName  Name of a global Lua function.
     * @return true if the function was found and executed without errors.
     */
    bool CallFunction(const std::string& funcName);

    /**
     * @brief Call a Lua function with one integer argument, no return value.
     *
     * TEACHING NOTE — Overloads vs Templates for Lua calls
     * ──────────────────────────────────────────────────────
     * We provide typed overloads rather than a single variadic template
     * because the Lua push API is typed:
     *   lua_pushinteger(L, val)   for integers
     *   lua_pushnumber(L, val)    for floats/doubles
     *   lua_pushstring(L, str)    for strings
     *
     * A variadic template (C++17 fold expressions) would look like:
     *   template<typename... Args>
     *   bool CallFunction(const std::string& name, Args&&... args);
     * This is more elegant but harder to read for students learning templates.
     * We provide both (the variadic version is the primary, overloads exist
     * as convenient wrappers for the common cases).
     *
     * @param funcName  Lua global function name.
     * @param arg       The integer argument to pass.
     * @return true on success.
     */
    bool CallFunction(const std::string& funcName, int arg);

    /**
     * @brief Call a Lua function with one string argument, no return value.
     * @param funcName  Lua global function name.
     * @param arg       The string argument to pass.
     * @return true on success.
     */
    bool CallFunction(const std::string& funcName, const std::string& arg);

    /**
     * @brief Call a Lua function with an int and a string argument.
     * @param funcName  Lua global function name.
     * @param arg1      First argument (integer).
     * @param arg2      Second argument (string).
     * @return true on success.
     */
    bool CallFunction(const std::string& funcName, int arg1, const std::string& arg2);

    /**
     * @brief Call a Lua function with two integer arguments, return an int.
     *
     * @param funcName   Lua function name.
     * @param arg1       First integer argument.
     * @param arg2       Second integer argument.
     * @param outResult  The integer return value from Lua.  Only valid if
     *                   this function returns true.
     * @return true on success.
     */
    bool CallFunctionRet(const std::string& funcName,
                         int arg1, int arg2,
                         int& outResult);

    // -----------------------------------------------------------------------
    // Registering C++ functions for Lua to call
    // -----------------------------------------------------------------------

    /**
     * @brief Register a C function so Lua scripts can call it by name.
     *
     * TEACHING NOTE — lua_CFunction Type
     * ────────────────────────────────────
     * Every function exposed to Lua must have this exact signature:
     *
     *   int myFunction(lua_State* L);
     *
     * It receives arguments from the Lua call via the stack (lua_tostring,
     * lua_tointeger, etc.) and pushes return values onto the stack.
     * The return value of the C function is the COUNT of values pushed.
     *
     * lua_register(L, "name", fn) is a macro for:
     *   lua_pushcfunction(L, fn);
     *   lua_setglobal(L, "name");
     *
     * @param name  Name the function will have in Lua global scope.
     * @param fn    A C function pointer matching int(lua_State*).
     */
    void RegisterFunction(const std::string& name, lua_CFunction fn);

    // -----------------------------------------------------------------------
    // Global variable access
    // -----------------------------------------------------------------------

    /**
     * @brief Read a global Lua variable into a C++ value.
     *
     * TEACHING NOTE — Template Specialisation
     * ─────────────────────────────────────────
     * GetGlobal<T> is a function template.  The implementation uses
     * `if constexpr` (C++17) to select the right lua_to* call at compile time
     * based on T.  This is a form of COMPILE-TIME DISPATCH.
     *
     * For example:
     *   GetGlobal<int>("level")       → calls lua_tointeger()
     *   GetGlobal<float>("speed")     → calls lua_tonumber()
     *   GetGlobal<std::string>("name")→ calls lua_tostring()
     *
     * `if constexpr` is evaluated at compile time; the unused branches are
     * completely removed from the binary (unlike a runtime `if`).
     *
     * @tparam T  The C++ type to read.  Supported: int, float, double,
     *            std::string, bool.
     * @param name          Name of the Lua global variable.
     * @param defaultValue  Returned if the variable doesn't exist or is nil.
     * @return The value as type T, or defaultValue on failure.
     */
    template<typename T>
    T GetGlobal(const std::string& name, T defaultValue = T{}) const;

    /**
     * @brief Set a Lua global variable from a C++ value.
     *
     * TEACHING NOTE — SetGlobal and Type Dispatch
     * ─────────────────────────────────────────────
     * Like GetGlobal, this template uses if constexpr to select the right
     * lua_push* variant.  After pushing the value, lua_setglobal(L, name)
     * pops it and stores it in Lua's global table.
     *
     * This is useful for:
     *   • Passing game state to scripts ("level", "difficulty", "player_name")
     *   • Hot-patching balance values without recompiling
     *   • Configuring scripts from C++ before running them
     *
     * @tparam T    The C++ type.  Supported: int, float, double, std::string, bool.
     * @param name  The Lua global variable name to set.
     * @param val   The value to assign.
     */
    template<typename T>
    void SetGlobal(const std::string& name, T val);

    /**
     * @brief Read a Lua global table and return a LuaTable view.
     *
     * @param name  Name of the Lua global table.
     * @return A LuaTable struct pointing at the table on the stack.
     *         The table remains on the stack until PopTable() or until
     *         the lua_State is modified.  Call lua_pop(L, 1) when done.
     */
    LuaTable GetTableGlobal(const std::string& name);

    // -----------------------------------------------------------------------
    // Utilities
    // -----------------------------------------------------------------------

    /**
     * @brief Return true if the Lua state has been successfully initialised.
     */
    [[nodiscard]] bool IsInitialised() const { return m_L != nullptr; }

    /**
     * @brief Return the raw lua_State pointer (for advanced use / extensions).
     *
     * TEACHING NOTE — When to Use the Raw Pointer
     * ─────────────────────────────────────────────
     * Most code should use the LuaEngine API.  Use the raw state only when:
     *   • Integrating a third-party Lua binding library (sol2, LuaBridge).
     *   • Writing complex binding code that goes beyond the LuaEngine API.
     *   • Debugging stack contents with lua_gettop / lua_typename.
     */
    [[nodiscard]] lua_State* GetState() const { return m_L; }

    /**
     * @brief Register all built-in game → Lua bindings.
     *
     * Exposed Lua functions after this call:
     *   game_log(msg)                    — log a message via Logger
     *   game_get_player_hp()             — returns current player HP
     *   game_heal_player(amount)         — restore player HP
     *   game_get_gold()                  — return current gold amount
     *   game_add_item(item_name, qty)    — add item to inventory
     *   game_start_combat(enemy_id)      — trigger a combat encounter
     *   game_complete_quest(quest_id)    — mark quest as done
     *   game_show_message(msg)           — display in-game message box
     */
    void RegisterEngineBindings();

private:
    // -----------------------------------------------------------------------
    // Private constructor — Singleton
    // -----------------------------------------------------------------------
    LuaEngine()  = default;
    ~LuaEngine() = default;

    // -----------------------------------------------------------------------
    // Error handling helper
    // -----------------------------------------------------------------------

    /**
     * @brief Pop the error message off the Lua stack and log it.
     *
     * After a failed lua_pcall, Lua pushes an error string.  This helper
     * pops it, formats a useful log message, and clears the stack.
     *
     * @param context  Descriptive string for the log (e.g., script filename).
     */
    void HandleLuaError(const std::string& context);

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    lua_State* m_L     = nullptr;   ///< The Lua interpreter state.  nullptr = not initialised.
    World*     m_world = nullptr;   ///< Non-owning pointer to the ECS World.
};


// ===========================================================================
// Template Implementations (must be in the header — templates are compiled
// into every translation unit that instantiates them)
// ===========================================================================

/**
 * TEACHING NOTE — Templates in Headers
 * ──────────────────────────────────────
 * Function templates must have their FULL implementation visible at the point
 * of instantiation (where GetGlobal<int>(...) is written in user code).
 * This means template implementations typically live in the .hpp file, not
 * the .cpp file.
 *
 * Alternatives:
 *   1. Put the implementation in the .hpp (done here — simplest for learners).
 *   2. Use explicit instantiations: declare the template in .cpp and add
 *      `template int GetGlobal<int>(...);` etc.  Faster compile, less flexible.
 *   3. Use a .inl file included at the bottom of the .hpp for organisation.
 */

template<typename T>
T LuaEngine::GetGlobal(const std::string& name, T defaultValue) const
{
    if (!m_L) { return defaultValue; }

    // Push the named global onto the Lua stack.
    // lua_getglobal returns the type of the value pushed:
    //   LUA_TNIL, LUA_TNUMBER, LUA_TSTRING, LUA_TBOOLEAN, etc.
    const int luaType = lua_getglobal(m_L, name.c_str());

    if (luaType == LUA_TNIL)
    {
        // Variable not defined in Lua — pop the nil and return default.
        lua_pop(m_L, 1);
        return defaultValue;
    }

    T result = defaultValue;

    // TEACHING NOTE — if constexpr (C++17)
    // ──────────────────────────────────────
    // `if constexpr` evaluates its condition at COMPILE TIME.
    // Only the branch matching T is compiled; the others are discarded.
    // This is zero-overhead compared to a runtime `if`.
    //
    // std::is_same_v<T, int> is true only when T == int exactly.
    // std::is_integral_v<T> is true for int, long, short, char, bool, etc.

    if constexpr (std::is_same_v<T, bool>)
    {
        result = static_cast<T>(lua_toboolean(m_L, -1) != 0);
    }
    else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>)
    {
        result = static_cast<T>(lua_tointeger(m_L, -1));
    }
    else if constexpr (std::is_floating_point_v<T>)
    {
        result = static_cast<T>(lua_tonumber(m_L, -1));
    }
    else if constexpr (std::is_same_v<T, std::string>)
    {
        const char* str = lua_tostring(m_L, -1);
        if (str) { result = str; }
    }

    // Always pop the value we pushed with getglobal.
    // TEACHING NOTE: forgetting to pop = "stack leak".  The Lua GC will not
    // free values sitting on the stack.  Always maintain a balanced stack.
    lua_pop(m_L, 1);
    return result;
}

template<typename T>
void LuaEngine::SetGlobal(const std::string& name, T val)
{
    if (!m_L) { return; }

    // Push the value, then lua_setglobal pops it and stores it.
    if constexpr (std::is_same_v<T, bool>)
    {
        lua_pushboolean(m_L, val ? 1 : 0);
    }
    else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>)
    {
        lua_pushinteger(m_L, static_cast<lua_Integer>(val));
    }
    else if constexpr (std::is_floating_point_v<T>)
    {
        lua_pushnumber(m_L, static_cast<lua_Number>(val));
    }
    else if constexpr (std::is_same_v<T, std::string>)
    {
        lua_pushstring(m_L, val.c_str());
    }
    else if constexpr (std::is_same_v<T, const char*>)
    {
        lua_pushstring(m_L, val);
    }

    lua_setglobal(m_L, name.c_str());
}
