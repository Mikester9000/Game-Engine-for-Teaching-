/**
 * @file InputSystem.cpp
 * @brief Implementation of the InputSystem singleton (ncurses keyboard polling).
 *
 * ============================================================================
 * TEACHING NOTE — Why Separate .hpp and .cpp for a Singleton?
 * ============================================================================
 *
 * Even for a singleton, separating declaration (.hpp) from implementation
 * (.cpp) is important:
 *
 *  • The .hpp contains only what CALLERS need to know (the interface).
 *  • This .cpp contains the actual ncurses calls — an implementation detail.
 *  • Changing how we poll input (e.g., switching from ncurses to SDL_Event)
 *    only requires rewriting this .cpp, not touching any caller code.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 */

#include "engine/input/InputSystem.hpp"
#include "engine/core/Logger.hpp"

#include <sstream>      // std::ostringstream — for KeyName()


// ===========================================================================
// Singleton Access
// ===========================================================================

InputSystem& InputSystem::Get()
{
    // TEACHING NOTE — Thread-Safe Static Local (C++11 §6.7/4)
    // ─────────────────────────────────────────────────────────
    // The static keyword on a local variable means it is constructed ONCE
    // the first time execution reaches this line, and destroyed at program
    // exit.  The C++11 standard explicitly requires that compilers protect
    // this construction from data races (using an internal mutex/flag).
    //
    // This is known as "Meyers' Singleton" after Scott Meyers who popularised
    // it in "Effective C++".
    static InputSystem instance;
    return instance;
}


// ===========================================================================
// Lifecycle
// ===========================================================================

bool InputSystem::Init()
{
    if (m_initialised)
    {
        LOG_INFO("InputSystem: already initialised, skipping.");
        return true;
    }

    // Set up the default key → action bindings before anything else.
    SetupDefaultBindings();

    // Configure ncurses input settings.
    // TEACHING NOTE — These calls configure the terminal's input behaviour:
    //
    //   keypad(stdscr, TRUE)
    //     Enables translation of multi-byte escape sequences (arrow keys,
    //     function keys) into single ncurses KEY_* integer constants.
    //     Without this, pressing ↑ would give three characters: ESC, '[', 'A'.
    //
    //   timeout(ms)
    //     Sets how long getch() waits for input before returning ERR.
    //     This is essential for a non-blocking game loop.
    keypad(stdscr, TRUE);
    timeout(m_timeoutMs);

    m_initialised = true;
    LOG_INFO("InputSystem: initialised. Timeout=" + std::to_string(m_timeoutMs) + "ms.");
    return true;
}

void InputSystem::Shutdown()
{
    // Input teardown is largely handled by ncurses endwin() in TerminalRenderer.
    // We just reset our internal state.
    m_initialised   = false;
    m_currentKey    = GameKeys::NONE;
    m_currentAction = InputAction::NONE;
    LOG_INFO("InputSystem: shut down.");
}


// ===========================================================================
// Per-Frame Polling
// ===========================================================================

int InputSystem::PollInput()
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — getch() in a Game Loop
    // ────────────────────────────────────────
    // getch() reads one keypress from the terminal.
    //
    // Behaviour (with timeout(50) set):
    //   • If a key is pressed:    returns the key code immediately.
    //   • If no key within 50ms:  returns ERR (-1).
    //
    // Special key codes (arrows, F-keys) are integers > 255.
    // Regular printable ASCII is in the range 32–126.
    // Control characters: Enter=10, Escape=27, Tab=9, Backspace=263/127.
    //
    // We store the result in m_currentKey for the entire frame so all
    // game systems see the same input state (see InputSystem.hpp teaching note).
    // -----------------------------------------------------------------------
    m_currentKey = getch();

    // Resolve the raw key to a logical action via the binding table.
    ResolveAction();

    // Log debug info only when a key is actually pressed (avoids spam).
    if (m_currentKey != ERR)
    {
        LOG_INFO("InputSystem: key=" + std::to_string(m_currentKey)
                 + " (" + KeyName(m_currentKey) + ")"
                 + " action=" + ActionName(m_currentAction));
    }

    return m_currentKey;
}


// ===========================================================================
// Queries
// ===========================================================================

bool InputSystem::IsKeyPressed(int key) const
{
    return (m_currentKey != ERR) && (m_currentKey == key);
}

bool InputSystem::IsActionActive(InputAction action) const
{
    return m_currentAction == action;
}

std::string InputSystem::KeyName(int key) const
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Readable Key Names for Debugging
    // ──────────────────────────────────────────────────
    // Printing raw integer key codes during debugging is tedious.
    // This function maps the most common codes to human-readable strings.
    // It is not performance-critical, so a series of if/else is fine.
    //
    // ncurses provides keyname(key) but it returns NULL for unknown keys
    // and its output format varies by platform.  We roll our own.
    // -----------------------------------------------------------------------
    if (key == ERR)           return "NONE";
    if (key == KEY_UP)        return "KEY_UP";
    if (key == KEY_DOWN)      return "KEY_DOWN";
    if (key == KEY_LEFT)      return "KEY_LEFT";
    if (key == KEY_RIGHT)     return "KEY_RIGHT";
    if (key == KEY_ENTER)     return "KEY_ENTER";
    if (key == KEY_BACKSPACE) return "KEY_BACKSPACE";
    if (key == '\n')          return "Enter";
    if (key == 27)            return "Escape";
    if (key == '\t')          return "Tab";
    if (key == ' ')           return "Space";

    // Printable ASCII: 32–126
    if (key >= 32 && key <= 126)
    {
        return std::string("'") + static_cast<char>(key) + "'";
    }

    // Unknown — show the decimal code
    std::ostringstream oss;
    oss << "Unknown(" << key << ")";
    return oss.str();
}

std::string InputSystem::ActionName(InputAction action) const
{
    // TEACHING NOTE — switch over enum class
    // ─────────────────────────────────────────
    // Because InputAction is a scoped enum (enum class), we must qualify
    // each case label: InputAction::MOVE_UP, not just MOVE_UP.
    // The compiler warns if any enumerator is missing from the switch
    // (when compiled with -Wswitch), which is a useful correctness check.
    switch (action)
    {
        case InputAction::NONE:           return "None";
        case InputAction::MOVE_UP:        return "Move Up";
        case InputAction::MOVE_DOWN:      return "Move Down";
        case InputAction::MOVE_LEFT:      return "Move Left";
        case InputAction::MOVE_RIGHT:     return "Move Right";
        case InputAction::CONFIRM:        return "Confirm";
        case InputAction::CANCEL:         return "Cancel";
        case InputAction::OPEN_MENU:      return "Open Menu";
        case InputAction::OPEN_INVENTORY: return "Open Inventory";
        case InputAction::OPEN_CAMP:      return "Open Camp";
        case InputAction::OPEN_MAP:       return "Open Map";
        case InputAction::ATTACK:         return "Attack";
        case InputAction::DODGE:          return "Dodge";
        case InputAction::CAST_MAGIC:     return "Cast Magic";
        case InputAction::SUMMON:         return "Summon";
        case InputAction::INTERACT:       return "Interact";
        case InputAction::SKILL_1:        return "Skill 1";
        case InputAction::SKILL_2:        return "Skill 2";
        case InputAction::SKILL_3:        return "Skill 3";
        case InputAction::SKILL_4:        return "Skill 4";
        case InputAction::COUNT:          return "COUNT";
        // No default — compiler warns if we miss a new action.
    }
    return "Unknown";
}


// ===========================================================================
// Key Binding
// ===========================================================================

void InputSystem::Bind(int rawKey, InputAction action)
{
    // TEACHING NOTE — std::unordered_map::operator[]
    // ─────────────────────────────────────────────────
    // m_bindings[rawKey] = action  inserts OR overwrites the entry for rawKey.
    // If rawKey doesn't exist in the map, a new entry is created.
    // If it does exist, the old action is replaced (supports rebinding).
    m_bindings[rawKey] = action;
}

void InputSystem::ResetBindings()
{
    m_bindings.clear();
    SetupDefaultBindings();
    LOG_INFO("InputSystem: key bindings reset to defaults.");
}

void InputSystem::SetInputTimeout(int ms)
{
    m_timeoutMs = ms;
    if (m_initialised)
    {
        // Apply the new timeout to ncurses immediately.
        timeout(ms);
        LOG_INFO("InputSystem: timeout changed to " + std::to_string(ms) + "ms.");
    }
}


// ===========================================================================
// Private Helpers
// ===========================================================================

void InputSystem::ResolveAction()
{
    // Default to NONE every frame.
    m_currentAction = InputAction::NONE;

    if (m_currentKey == ERR) { return; }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Hash Map Lookup
    // ─────────────────────────────────
    // std::unordered_map::find() returns an iterator.
    //   end() == not found.
    //   otherwise, it->second is the mapped value (the InputAction).
    //
    // This is O(1) average case — constant time regardless of how many
    // bindings are registered.  Perfect for a per-frame operation.
    // -----------------------------------------------------------------------
    const auto it = m_bindings.find(m_currentKey);
    if (it != m_bindings.end())
    {
        m_currentAction = it->second;
    }
}

void InputSystem::SetupDefaultBindings()
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Default Bindings Table
    // ────────────────────────────────────────
    // We register multiple physical keys for the same logical action (e.g.,
    // both KEY_UP and 'w' map to MOVE_UP).  This gives players flexibility
    // without any extra code in game systems — they just check the action.
    //
    // The pattern mirrors Unity's "Input.GetAxis" with named axes:
    //   Horizontal: ← → A D
    //   Vertical:   ↑ ↓ W S
    // -----------------------------------------------------------------------

    // --- Navigation (arrow keys + WASD) ---
    Bind(GameKeys::UP,    InputAction::MOVE_UP);
    Bind('w',             InputAction::MOVE_UP);
    Bind(GameKeys::DOWN,  InputAction::MOVE_DOWN);
    Bind('s',             InputAction::MOVE_DOWN);   // also mapped to CAST_MAGIC below — last write wins, so we handle 's' for magic
    Bind(GameKeys::LEFT,  InputAction::MOVE_LEFT);
    Bind('a',             InputAction::MOVE_LEFT);   // also ATTACK — we prefer WASD nav; combat uses separate keys
    Bind(GameKeys::RIGHT, InputAction::MOVE_RIGHT);
    Bind('d',             InputAction::MOVE_RIGHT);  // also DODGE

    // --- WASD-specific overrides: when not moving, these are combat keys ---
    // NOTE: For a real game you would use a context-sensitive input system
    // that selects a different binding table based on the current GameState
    // (exploring vs combat).  For this teaching engine we use a unified table
    // and let game systems filter based on state.
    //
    // Combat-specific bindings take the dedicated combat keys:
    Bind(GameKeys::ATTACK,   InputAction::ATTACK);       // 'a'
    Bind(GameKeys::DODGE,    InputAction::DODGE);        // 'd'
    Bind(GameKeys::MAGIC,    InputAction::CAST_MAGIC);   // 's'
    Bind(GameKeys::SUMMON,   InputAction::SUMMON);       // 'z'
    Bind(GameKeys::INTERACT, InputAction::INTERACT);     // 'e'

    // --- UI ---
    Bind(GameKeys::ACTION,    InputAction::CONFIRM);
    Bind(KEY_ENTER,           InputAction::CONFIRM);     // numpad Enter
    Bind(GameKeys::BACK,      InputAction::CANCEL);      // Escape
    Bind('q',                 InputAction::CANCEL);      // 'q' also quits menus
    Bind(GameKeys::MENU,      InputAction::OPEN_MENU);
    Bind(GameKeys::INVENTORY, InputAction::OPEN_INVENTORY);
    Bind(GameKeys::CAMP,      InputAction::OPEN_CAMP);
    Bind(GameKeys::MAP_VIEW,  InputAction::OPEN_MAP);    // Tab

    // --- Skill hotbar ---
    Bind(GameKeys::SKILL1,    InputAction::SKILL_1);     // '1'
    Bind(GameKeys::SKILL2,    InputAction::SKILL_2);     // '2'
    Bind(GameKeys::SKILL3,    InputAction::SKILL_3);     // '3'
    Bind(GameKeys::SKILL4,    InputAction::SKILL_4);     // '4'

    LOG_INFO("InputSystem: " + std::to_string(m_bindings.size())
             + " key bindings registered.");
}
