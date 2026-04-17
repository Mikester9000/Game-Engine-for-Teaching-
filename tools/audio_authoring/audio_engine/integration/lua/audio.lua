-- audio.lua – Lua integration layer for the C++ AudioSystem.
--
-- TEACHING NOTE — Lua/C++ Bridge
-- ==================================
-- The C++ engine registers several global C functions that Lua scripts can call:
--   audio_play_sfx(event_key)       – play a registered sound effect
--   audio_play_music(filename)      – start a music track
--   audio_set_volume(level)         – set master volume (0.0 – 1.0)
--   audio_play_voice(key)           – play a voice line
--
-- This Lua file provides a higher-level wrapper module so game scripts can
-- write readable code like:
--   Audio.on_combat_start()
-- instead of raw C calls.
--
-- To install:
--   cp tools/audio_authoring/audio_engine/integration/lua/audio.lua scripts/
--
-- Then in the engine's main Lua file (scripts/main.lua), add:
--   require("audio")
--
-- The Lua hooks called by C++ (on_combat_start, on_camp_rest, etc.) are
-- registered automatically when this file is loaded.
--
-- TEACHING NOTE — Module Pattern in Lua
-- Lua does not have a built-in module/namespace system.  The idiomatic way
-- to create a namespace is to create a table and assign functions to it:
--   Audio = {}
--   Audio.play_sfx = function(key) ... end
-- This avoids polluting the global namespace with bare function names.

-- ---------------------------------------------------------------------------
-- Module table
-- ---------------------------------------------------------------------------

Audio = Audio or {}

-- ---------------------------------------------------------------------------
-- Guard: silently no-op if C++ audio functions are not registered.
-- This keeps the script working in unit-test environments that don't wire
-- up the C++ bindings.
-- ---------------------------------------------------------------------------

local function _sfx(key)
    if audio_play_sfx then
        audio_play_sfx(key)
    end
end

local function _music(filename)
    if audio_play_music then
        audio_play_music(filename)
    end
end

local function _voice(key)
    if audio_play_voice then
        audio_play_voice(key)
    end
end

local function _volume(level)
    if audio_set_volume then
        audio_set_volume(level)
    end
end

-- ---------------------------------------------------------------------------
-- Public Audio API
-- ---------------------------------------------------------------------------

--- Play a registered sound effect.
-- @param key  SFX event key (matches SFX_MANIFEST event field).
function Audio.play_sfx(key)
    _sfx(key)
end

--- Start a music track by filename.
-- @param filename  Music file name (relative to assets/audio/music/).
function Audio.play_music(filename)
    _music(filename)
end

--- Play a voice line by key.
-- @param key  Voice event key (matches VOICE_MANIFEST key field).
function Audio.play_voice(key)
    _voice(key)
end

--- Set the master volume.
-- @param level  Volume level between 0.0 (silent) and 1.0 (full).
function Audio.set_volume(level)
    _volume(level)
end

-- ---------------------------------------------------------------------------
-- Game-state hooks
-- These are called by the C++ engine at the relevant game moments.
-- Register them as globals so C++ can find them by name.
-- ---------------------------------------------------------------------------

--- Called when the player enters combat.
function on_combat_start()
    _music("music_combat.wav")
end

--- Called when combat ends (victory or retreat).
function on_combat_end()
    _music("music_exploring.wav")
    _sfx("sword_hit")
end

--- Called when a boss fight begins.
function on_boss_start()
    _music("music_boss.wav")
end

--- Called when the player sets up camp.
function on_camp_rest()
    _music("music_camp.wav")
    _sfx("camp_fire")
end

--- Called when the player levels up.
function on_level_up()
    _sfx("level_up")
    _voice("level_up")
end

--- Called when a quest is completed.
function on_quest_complete()
    _sfx("quest_complete")
    _voice("quest_complete")
end

--- Called when the player picks up an item.
function on_item_pickup()
    _sfx("item_pickup")
end

--- Called when the main menu is opened.
function on_menu_open()
    _music("music_menu.wav")
end

--- Called when a UI menu item is selected.
function on_menu_select()
    _sfx("menu_select")
end

--- Called when a UI menu action is cancelled.
function on_menu_cancel()
    _sfx("menu_cancel")
end

--- Called when the player character dies.
function on_game_over()
    _music("music_game_over.wav")
    _voice("game_over")
end

--- Called when the player HP drops below a critical threshold.
-- @param hp     Current HP.
-- @param max_hp Maximum HP.
function on_low_hp(hp, max_hp)
    if max_hp and max_hp > 0 and (hp / max_hp) < 0.2 then
        _voice("party_low_hp")
    end
end

--- Called when a cutscene begins.
function on_cutscene_start()
    _music("music_cutscene.wav")
end

--- Called when the player enters the open world (after load/cutscene).
function on_explore_update()
    -- Continuously called; only transition to explore music if not in combat.
    -- C++ AudioSystem handles preventing re-trigger of the same track.
end
