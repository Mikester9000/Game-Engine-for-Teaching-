--[[
  @file quests.lua
  @brief Quest callbacks and scripted quest logic.

  ============================================================================
  TEACHING NOTE — Data-Driven Quests with Lua
  ============================================================================

  The C++ QuestSystem handles the MECHANICS of quests (accepting, tracking
  objectives, granting rewards).  Lua handles the CONTENT:
    • Dialogue displayed when a quest starts or ends.
    • Special conditions or branching outcomes.
    • Custom rewards beyond the standard item/XP grants.

  C++ calls into Lua via LuaEngine::CallFunction("on_quest_complete", questID).
  Lua calls back into C++ via registered engine functions.

  This separation is the key benefit of scripted quests:
    → Level designers write Lua without recompiling C++.
    → C++ engineers don't hard-code quest content.
]]

-- ---------------------------------------------------------------------------
-- Quest data table
-- Maps questID → narrative strings and callbacks.
-- ---------------------------------------------------------------------------

QuestCallbacks = {

  -- Quest 1: "The Road to Dawn"
  [1] = {
    onAccept  = function()
      if engine_log then engine_log("[Quest 1] Accepted: The Road to Dawn.") end
    end,
    onComplete = function()
      if engine_log then
        engine_log("[Quest 1] Complete! The road ahead is clear.")
      end
    end,
    onFail = function()
      if engine_log then engine_log("[Quest 1] Failed.") end
    end,
  },

  -- Quest 2: "Trespassers"
  [2] = {
    onAccept  = function()
      if engine_log then engine_log("[Quest 2] Accepted: Trespassers.") end
    end,
    onComplete = function()
      if engine_log then engine_log("[Quest 2] Cleared the Malmalam Thicket.") end
    end,
    onFail = nil,
  },
}

-- ---------------------------------------------------------------------------
-- on_quest_accepted  — called by C++ QuestSystem
-- ---------------------------------------------------------------------------
function on_quest_accepted(questID)
  local cb = QuestCallbacks[questID]
  if cb and cb.onAccept then
    cb.onAccept()
  else
    if engine_log then
      engine_log("[Lua] Quest " .. questID .. " accepted (no custom hook).")
    end
  end
end

-- ---------------------------------------------------------------------------
-- on_quest_complete  — called by C++ QuestSystem
-- ---------------------------------------------------------------------------
function on_quest_complete(questID)
  local cb = QuestCallbacks[questID]
  if cb and cb.onComplete then
    cb.onComplete()
  else
    if engine_log then
      engine_log("[Lua] Quest " .. questID .. " completed (no custom hook).")
    end
  end
end

-- ---------------------------------------------------------------------------
-- on_quest_failed  — called by C++ QuestSystem
-- ---------------------------------------------------------------------------
function on_quest_failed(questID)
  local cb = QuestCallbacks[questID]
  if cb and cb.onFail then
    cb.onFail()
  else
    if engine_log then
      engine_log("[Lua] Quest " .. questID .. " failed.")
    end
  end
end

if engine_log then
  engine_log("[Lua] quests.lua loaded.")
end
