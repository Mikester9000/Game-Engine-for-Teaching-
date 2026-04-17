--[[
  @file main.lua
  @brief Main Lua entry point — global event hooks and startup logic.

  ============================================================================
  TEACHING NOTE — Why Lua for Scripting?
  ============================================================================

  Lua is the most popular scripting language for game engines because:
    1. Tiny runtime (~200 KB) — fits in embedded devices.
    2. Clean C API — calling Lua from C++ (and vice-versa) is simple.
    3. Fast — LuaJIT can approach C speed.
    4. Forgiving syntax — easy for non-programmers (designers, QA).

  In our engine, Lua scripts are called from C++ at specific points
  (hooks) such as:
    • on_explore_update()   — called every exploring frame.
    • on_combat_start()     — called when combat begins.
    • on_camp_rest()        — called when the party rests.
    • on_spell_cast(id, target) — called per spell.

  This lets designers add custom logic without recompiling C++.

  ============================================================================
  TEACHING NOTE — Tables as Modules
  ============================================================================

  Lua has no built-in module system.  The convention is to use tables as
  namespaces:
    Game = {}          -- "module"
    Game.state = "idle"
    function Game.doSomething() end

  This prevents name collisions with other scripts.
]]

-- Global game state visible to all Lua scripts.
GameState = {
    day        = 1,
    totalSteps = 0,
}

-- ---------------------------------------------------------------------------
-- on_explore_update
-- Called by C++ every exploring frame (see Game.cpp UpdateExploring).
-- ---------------------------------------------------------------------------
function on_explore_update()
    GameState.totalSteps = GameState.totalSteps + 1

    -- Every 500 steps, log a reminder about camping.
    if GameState.totalSteps % 500 == 0 then
        -- engine_log is registered by LuaEngine::RegisterEngineBindings()
        if engine_log then
            engine_log("[Lua] " .. GameState.totalSteps .. " steps taken. Time to camp?")
        end
    end
end

-- ---------------------------------------------------------------------------
-- on_combat_start
-- Called when the C++ CombatSystem starts a new combat encounter.
-- ---------------------------------------------------------------------------
function on_combat_start(enemyName)
    if engine_log then
        engine_log("[Lua] Combat started! Enemy: " .. tostring(enemyName))
    end
end

-- ---------------------------------------------------------------------------
-- on_camp_rest
-- Called when the party rests at camp.
-- ---------------------------------------------------------------------------
function on_camp_rest()
    GameState.day = GameState.day + 1
    if engine_log then
        engine_log("[Lua] Rested at camp. Day " .. GameState.day)
    end
end

-- ---------------------------------------------------------------------------
-- on_level_up
-- Called when the player gains a level.
-- ---------------------------------------------------------------------------
function on_level_up(newLevel)
    if engine_log then
        engine_log("[Lua] Level up! New level: " .. tostring(newLevel))
    end
end

-- ---------------------------------------------------------------------------
-- Startup message
-- ---------------------------------------------------------------------------
if engine_log then
    engine_log("[Lua] main.lua loaded successfully.")
end
