--[[
  audio.lua — Lua integration for the Game Engine for Teaching AudioSystem
  =========================================================================

  TEACHING NOTE — Lua ↔ C++ Audio Bridge
  ========================================
  The C++ AudioSystem registers four C functions with the Lua state:

      audio_play_sfx(event_key)          – play a one-shot sound effect
      audio_play_music(filename)         – start/crossfade a music track
      audio_set_volume(volume)           – set master music+sfx volume (0–1)
      audio_play_voice(voice_key)        – play a voiced narrator line

  These bindings are registered in LuaEngine::RegisterEngineBindings():

      AudioSystem* audio = &Game::Instance().GetAudio();
      lua_pushlightuserdata(L, audio);
      lua_setglobal(L, "__audio_ptr");
      lua_register(L, "audio_play_sfx",   AudioSystem::Lua_PlaySFX);
      lua_register(L, "audio_play_music", AudioSystem::Lua_PlayMusic);
      lua_register(L, "audio_set_volume", AudioSystem::Lua_SetVolume);
      lua_register(L, "audio_play_voice", AudioSystem::Lua_PlayVoice);

  This Lua module wraps those raw C bindings with a friendly Lua API and
  auto-wires into the existing engine hooks
  (on_combat_start, on_camp_rest, on_level_up, …).

  Usage
  -----
  -- In main.lua (or any script):
  local Audio = require("audio")
  Audio.play_sfx("combat_hit")
  Audio.play_music("music_combat.wav")
  Audio.set_volume(0.8)
  Audio.play_voice("level_up")

  Installation
  ------------
  Copy this file into the game engine's scripts/ directory:

      cp <audio_engine_package>/integration/lua/audio.lua scripts/audio.lua

  Then require it from scripts/main.lua:

      local Audio = require("audio")

--]]

-- TEACHING NOTE — Module pattern
-- We return a table of functions instead of polluting the global namespace.
-- This is idiomatic Lua (similar to Python's "import module" pattern).
local Audio = {}


-- ---------------------------------------------------------------------------
-- Low-level wrappers
-- (The real work is done by the C++ AudioSystem via lua_register)
-- ---------------------------------------------------------------------------

--- Play a one-shot sound effect by its event key.
-- @param event_key string  e.g. "combat_hit", "spell_cast", "level_up"
function Audio.play_sfx(event_key)
    -- TEACHING NOTE — Defensive nil check
    -- If the C++ side never registered the binding (headless server, stub
    -- build), we silently skip rather than crashing the script.
    if audio_play_sfx then
        audio_play_sfx(event_key)
    end
end

--- Start or crossfade to a music track.
-- @param filename string  WAV filename, e.g. "music_combat.wav"
function Audio.play_music(filename)
    if audio_play_music then
        audio_play_music(filename)
    end
end

--- Set the master volume for both music and SFX.
-- @param volume number  0.0 (silent) to 1.0 (full volume)
function Audio.set_volume(volume)
    if audio_set_volume then
        audio_set_volume(volume)
    end
end

--- Play a voiced narrator or character line.
-- @param voice_key string  e.g. "welcome", "level_up", "boss_intro"
function Audio.play_voice(voice_key)
    if audio_play_voice then
        audio_play_voice(voice_key)
    end
end


-- ---------------------------------------------------------------------------
-- Named music helpers  (match the music manifest in game_state_map.py)
-- ---------------------------------------------------------------------------

--- Start the main menu music.
function Audio.on_main_menu()
    Audio.play_music("music_main_menu.wav")
end

--- Start the exploration music.
function Audio.on_exploring()
    Audio.play_music("music_exploring.wav")
end

--- Start the combat music.
function Audio.on_combat_start()
    Audio.play_music("music_combat.wav")
    Audio.play_voice("welcome")   -- optional narrator intro on first combat
end

--- Start the boss battle music.
function Audio.on_boss_combat()
    Audio.play_music("music_boss_combat.wav")
    Audio.play_voice("boss_intro")
end

--- Play the victory music and voice line.
function Audio.on_combat_victory()
    Audio.play_music("music_victory.wav")
end

--- Start the camping music and rest voice line.
function Audio.on_camp_rest()
    Audio.play_music("music_camping.wav")
    Audio.play_voice("camp_rest")
end

--- Play the level-up SFX and voice line.
-- @param new_level number  (unused by audio, reserved for future use)
function Audio.on_level_up(new_level)
    Audio.play_sfx("level_up")
    Audio.play_voice("level_up")
end

--- Play the quest-complete SFX and voice line.
function Audio.on_quest_complete()
    Audio.play_sfx("quest_complete")
    Audio.play_voice("quest_complete")
end

--- Play the game-over voice line.
function Audio.on_game_over()
    Audio.play_voice("game_over")
end


-- ---------------------------------------------------------------------------
-- Low-level SFX helpers  (direct event key bindings)
-- ---------------------------------------------------------------------------

function Audio.on_combat_hit()   Audio.play_sfx("combat_hit")   end
function Audio.on_combat_miss()  Audio.play_sfx("combat_miss")  end
function Audio.on_spell_cast()   Audio.play_sfx("spell_cast")   end
function Audio.on_spell_hit()    Audio.play_sfx("spell_hit")    end
function Audio.on_item_pickup()  Audio.play_sfx("item_pickup")  end
function Audio.on_item_equip()   Audio.play_sfx("item_equip")   end
function Audio.on_ui_confirm()   Audio.play_sfx("ui_confirm")   end
function Audio.on_ui_cancel()    Audio.play_sfx("ui_cancel")    end
function Audio.on_door_open()    Audio.play_sfx("door_open")    end
function Audio.on_enemy_death()  Audio.play_sfx("enemy_death")  end
function Audio.on_player_death()
    Audio.play_sfx("player_death")
    Audio.play_voice("game_over")
end


-- ---------------------------------------------------------------------------
-- Module return
-- ---------------------------------------------------------------------------

-- TEACHING NOTE — Always return the module table at the end of the file.
-- Lua's require() system caches this table and returns it to every caller,
-- so all scripts share the same Audio instance.
return Audio
