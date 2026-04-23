--[[
  @file enemies.lua
  @brief Enemy spell-cast callbacks and special abilities defined in Lua.

  ============================================================================
  TEACHING NOTE — Script-Defined Abilities
  ============================================================================

  In MagicSystem.cpp, after a spell is cast, the engine calls:
    LuaEngine::CallFunction(spell.luaCastCallback, casterID, targetID)

  This means special effects (status applications, summons, multi-hit chains)
  can be defined here in Lua without touching C++.

  The pattern is:
    SpellData::luaCastCallback = "spell_fire_callback"
    → After dealing damage, C++ calls spell_fire_callback(casterID, targetID)
    → This Lua function can apply BURN status, spawn particles, etc.
]]

-- ---------------------------------------------------------------------------
-- Spell callbacks (luaCastCallback field in SpellData)
-- ---------------------------------------------------------------------------

-- Fire spell: apply Burn status for 3 turns.
function spell_fire_callback(casterID, targetID)
  if engine_log then
    engine_log("[Lua] Fire hit entity " .. targetID .. " — burn applied!")
  end
  -- In a full implementation: engine_apply_status(targetID, "BURN", 3)
end

-- Blizzard spell: apply Frozen status (skip one turn).
function spell_blizzard_callback(casterID, targetID)
  if engine_log then
    engine_log("[Lua] Blizzard hit entity " .. targetID .. " — frozen!")
  end
end

-- Thunder spell: chance to paralyse.
function spell_thunder_callback(casterID, targetID)
  -- 30% chance to paralyse.
  local roll = math.random(1, 10)
  if roll <= 3 then
    if engine_log then
      engine_log("[Lua] Thunder paralysed entity " .. targetID .. "!")
    end
  end
end

-- Cure spell: additional regeneration ticks.
function spell_cure_callback(casterID, targetID)
  if engine_log then
    engine_log("[Lua] Cure cast on entity " .. targetID .. " — regen started.")
  end
end

-- ---------------------------------------------------------------------------
-- Enemy defeat hooks
-- ---------------------------------------------------------------------------

-- Called when a specific enemy type dies.
-- enemyDataID matches EnemyData::id in GameData.hpp.
EnemyDeathCallbacks = {

  -- ID 1: Goblin — drops a message.
  [1] = function(entityID)
    if engine_log then engine_log("[Lua] Goblin " .. entityID .. " slain!") end
  end,

  -- ID 4: Cockatrice — rare drop hint.
  [4] = function(entityID)
    if engine_log then
      engine_log("[Lua] Cockatrice defeated — rare Talon may have dropped!")
    end
  end,

  -- ID 11: Iron Giant (boss).
  [11] = function(entityID)
    if engine_log then
      engine_log("[Lua] Iron Giant falls! The plains are safe again.")
    end
  end,
}

function on_enemy_killed(enemyDataID, entityID)
  local cb = EnemyDeathCallbacks[enemyDataID]
  if cb then
    cb(entityID)
  end
end

if engine_log then
  engine_log("[Lua] enemies.lua loaded.")
end
