/**
 * @file InputSystem.hpp
 * @brief Keyboard input handling for the game engine (ncurses backend).
 *
 * ============================================================================
 * TEACHING NOTE — Input Handling in Games
 * ============================================================================
 *
 * Input handling is deceptively complex in games.  The naive approach — polling
 * the keyboard inside gameplay code — quickly becomes unmaintainable:
 *
 *   // BAD — input scattered everywhere, hard to remap
 *   if (getch() == 'w') player.MoveUp();
 *   if (getch() == 'a') player.MoveLeft();
 *   …
 *
 * A dedicated InputSystem solves this with TWO layers of abstraction:
 *
 *   Layer 1 — RAW INPUT: physical key codes from the OS / ncurses.
 *     getch() → integer key code (KEY_UP=259, 'w'=119, ESC=27, etc.)
 *
 *   Layer 2 — ACTIONS: game-semantic events, independent of key bindings.
 *     InputAction::MOVE_UP, InputAction::ATTACK, InputAction::OPEN_MENU
 *
 * The InputSystem maps Layer 1 → Layer 2.  Game systems only query Layer 2
 * (actions).  When the player remaps keys, only the mapping table changes,
 * not the game code.  This is the "Input Remapping" or "Input Abstraction"
 * pattern used by Unity (Input.GetAxis), Unreal (Action Mappings), etc.
 *
 * ============================================================================
 * TEACHING NOTE — Polling vs Event-Driven Input
 * ============================================================================
 *
 * This engine uses POLLING: every game-loop iteration we call PollInput()
 * which asks ncurses "was a key pressed this frame?"
 *
 * An alternative is EVENT-DRIVEN input: the OS pushes key events to a queue
 * which game code processes asynchronously.  SDL and Win32 use this model.
 *
 * POLLING advantages: simple, deterministic, easy to debug.
 * EVENT-DRIVEN advantages: no missed keystrokes between frames, natural for
 *                          complex key-combo detection (hold, repeat, release).
 *
 * For an educational engine with a 20fps game loop, polling is perfect.
 *
 * ============================================================================
 * TEACHING NOTE — Non-Blocking Input in Game Loops
 * ============================================================================
 *
 * The game loop runs continuously, updating game state and re-drawing every
 * frame regardless of whether the player pressed a key.  If getch() BLOCKED
 * (waited forever for a keypress) the game would freeze between keystrokes.
 *
 * Solution: configure ncurses with `timeout(ms)` so getch() returns ERR after
 * at most `ms` milliseconds if no key was pressed.  The InputSystem sets a
 * 50ms timeout → 20 polls/second.  ERR maps to InputAction::NONE.
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
#include <unordered_map>    // std::unordered_map — key binding table
#include <functional>       // std::function — action callback support
#include <string>           // std::string — key name lookup

// ---------------------------------------------------------------------------
// ncurses — provides getch() and KEY_* constants
// ---------------------------------------------------------------------------
// TEACHING NOTE — Why include ncurses.h in the header?
// ──────────────────────────────────────────────────────
// The GameKeys namespace below directly uses ncurses KEY_UP, KEY_DOWN, etc.
// These are macros defined in ncurses.h.  We must include it in the header
// so that any file including InputSystem.hpp sees the same macro definitions.
//
// In a production engine you would isolate ncurses behind a platform layer
// so that InputSystem.hpp could be included by non-ncurses code (e.g., unit
// tests on Windows).  For this teaching engine we keep it simple.
#include <ncurses.h>

// ---------------------------------------------------------------------------
// Engine headers
// ---------------------------------------------------------------------------
#include "engine/core/Types.hpp"


// ===========================================================================
// GameKeys — Raw Key Code Constants
// ===========================================================================

/**
 * @namespace GameKeys
 * @brief Maps game-semantic key names to ncurses key codes.
 *
 * TEACHING NOTE — Why a Namespace Instead of an Enum?
 * ──────────────────────────────────────────────────────
 * We use a namespace of `constexpr int` constants rather than an enum because
 * ncurses KEY_* values are plain ints (some > 255, well outside char range).
 * An enum class would require explicit casting everywhere; a namespace gives
 * clean, scoped names without the ceremony.
 *
 * These raw codes are the "physical" layer — they represent actual keys.
 * Game code should prefer the `InputAction` enum (the "logical" layer) and
 * call InputSystem::GetAction() to check the current action.
 *
 * Key code reference:
 *   Arrow keys      : KEY_UP(259), KEY_DOWN(258), KEY_LEFT(260), KEY_RIGHT(261)
 *   ASCII printable : 'a'-'z' = 97-122, '0'-'9' = 48-57
 *   Enter           : '\n' (10) or KEY_ENTER (ncurses)
 *   Escape          : 27 (decimal)
 *   Tab             : '\t' (9)
 */
namespace GameKeys
{
    // -----------------------------------------------------------------------
    // Navigation
    // -----------------------------------------------------------------------

    /// Move up / cursor up (ncurses arrow key constant)
    constexpr int UP     = KEY_UP;

    /// Move down / cursor down
    constexpr int DOWN   = KEY_DOWN;

    /// Move left / cursor left
    constexpr int LEFT   = KEY_LEFT;

    /// Move right / cursor right
    constexpr int RIGHT  = KEY_RIGHT;

    // -----------------------------------------------------------------------
    // UI / Menu
    // -----------------------------------------------------------------------

    /// Confirm selection / interact (Enter key, ASCII 10)
    constexpr int ACTION    = '\n';

    /// Cancel / go back (Escape key, ASCII 27) — also accepts 'q'
    constexpr int BACK      = 27;

    /// Toggle main pause menu
    constexpr int MENU      = 'm';

    /// Open inventory screen
    constexpr int INVENTORY = 'i';

    /// Open camp / rest menu (party management)
    constexpr int CAMP      = 'c';

    /// Toggle world map overlay
    constexpr int MAP_VIEW  = '\t';     // Tab key (ASCII 9)

    // -----------------------------------------------------------------------
    // Combat actions
    // -----------------------------------------------------------------------

    /// Normal attack (melee/ranged)
    constexpr int ATTACK    = 'a';

    /// Dodge / roll / evade
    constexpr int DODGE     = 'd';

    /// Cast magic / tech skill
    constexpr int MAGIC     = 's';

    /// Summon / phantom-arms
    constexpr int SUMMON    = 'z';

    /// Interact with NPC / object in the world
    constexpr int INTERACT  = 'e';

    // -----------------------------------------------------------------------
    // Hotbar skill slots 1–4
    // -----------------------------------------------------------------------

    /// Activate ability slot 1
    constexpr int SKILL1    = '1';

    /// Activate ability slot 2
    constexpr int SKILL2    = '2';

    /// Activate ability slot 3
    constexpr int SKILL3    = '3';

    /// Activate ability slot 4
    constexpr int SKILL4    = '4';

    // -----------------------------------------------------------------------
    // Sentinel
    // -----------------------------------------------------------------------

    /// Returned when no key is pressed (ncurses ERR = -1)
    constexpr int NONE      = ERR;

}   // namespace GameKeys


// ===========================================================================
// InputAction — Logical / Semantic Layer
// ===========================================================================

/**
 * @enum InputAction
 * @brief High-level game actions, independent of which physical key triggers them.
 *
 * TEACHING NOTE — The Action Abstraction
 * ────────────────────────────────────────
 * Game systems check for ACTIONS, not raw keys.  For example:
 *
 *   // System code — portable and remappable:
 *   if (input.GetAction() == InputAction::MOVE_UP) { … }
 *
 *   // NOT this — hard-coded and fragile:
 *   if (rawKey == KEY_UP || rawKey == 'w') { … }
 *
 * This enum uses `uint8_t` as the underlying type because we never need more
 * than 256 actions, and small underlying types pack nicely in arrays/bitsets.
 */
enum class InputAction : uint8_t
{
    NONE = 0,       ///< No input this frame.

    // Movement
    MOVE_UP,        ///< Move player tile north.
    MOVE_DOWN,      ///< Move player tile south.
    MOVE_LEFT,      ///< Move player tile west.
    MOVE_RIGHT,     ///< Move player tile east.

    // UI
    CONFIRM,        ///< Accept / OK / select menu item.
    CANCEL,         ///< Go back / close menu.
    OPEN_MENU,      ///< Toggle pause menu.
    OPEN_INVENTORY, ///< Open item bag.
    OPEN_CAMP,      ///< Open camp/rest menu.
    OPEN_MAP,       ///< Toggle world map.

    // Combat
    ATTACK,         ///< Perform a normal attack.
    DODGE,          ///< Perform a dodge/roll.
    CAST_MAGIC,     ///< Cast selected spell.
    SUMMON,         ///< Activate summon / phantom arms.
    INTERACT,       ///< Talk to NPC / examine object / open door.

    // Skill hotbar
    SKILL_1,        ///< Activate ability slot 1.
    SKILL_2,        ///< Activate ability slot 2.
    SKILL_3,        ///< Activate ability slot 3.
    SKILL_4,        ///< Activate ability slot 4.

    COUNT           ///< Sentinel — must stay last; used for array sizing.
};


// ===========================================================================
// InputSystem
// ===========================================================================

/**
 * @class InputSystem
 * @brief Singleton that polls ncurses for keyboard input each game frame.
 *
 * TEACHING NOTE — The Singleton Pattern
 * ───────────────────────────────────────
 * A Singleton ensures there is exactly ONE instance of a class, accessible
 * globally.  It is appropriate for systems that manage a single, global
 * resource — here the terminal keyboard.
 *
 * Implementation via a static local variable (Meyers' Singleton) is the
 * preferred modern C++ approach because:
 *  • The instance is created on first use (lazy initialisation).
 *  • C++11 guarantees static local construction is thread-safe.
 *  • No need for a heap allocation or explicit deletion.
 *
 * CRITICISM: Singletons make unit testing harder (global state is hard to
 * mock).  For a teaching engine the trade-off is acceptable.  In a production
 * engine consider dependency injection (pass an IInputProvider interface).
 *
 * Usage:
 * @code
 *   InputSystem& input = InputSystem::Get();
 *   input.Init();
 *
 *   // In the game loop:
 *   input.PollInput();
 *   if (input.GetAction() == InputAction::MOVE_UP) { … }
 * @endcode
 */
class InputSystem
{
public:
    // -----------------------------------------------------------------------
    // Singleton access
    // -----------------------------------------------------------------------

    /**
     * @brief Return the single global InputSystem instance.
     *
     * TEACHING NOTE — Meyers' Singleton
     * ──────────────────────────────────
     * The `static InputSystem instance;` inside the function body is a
     * "magic static" — guaranteed by the C++11 standard to be:
     *   1. Constructed the first time Get() is called.
     *   2. Destroyed in reverse construction order at program exit.
     *   3. Thread-safe during construction (std::call_once semantics).
     */
    static InputSystem& Get();

    // Prevent copying — only one InputSystem should ever exist.
    InputSystem(const InputSystem&) = delete;
    InputSystem& operator=(const InputSystem&) = delete;

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    /**
     * @brief Initialise the input system (configure ncurses input settings).
     *
     * Must be called AFTER TerminalRenderer::Init() (which calls initscr()).
     * Sets:
     *  • timeout(50)       — 50ms non-blocking getch()
     *  • keypad(stdscr,…)  — enable arrow-key sequences (may already be set)
     *  • nodelay check     — logs if running in blocking mode
     *
     * @return true always (ncurses input is always available post-initscr).
     */
    bool Init();

    /**
     * @brief Shut down the input system.
     *
     * Currently a no-op (ncurses teardown is handled by TerminalRenderer),
     * but provided for symmetry and future use (e.g., game controller cleanup).
     */
    void Shutdown();

    // -----------------------------------------------------------------------
    // Per-frame update
    // -----------------------------------------------------------------------

    /**
     * @brief Read one keypress from ncurses and store it for this frame.
     *
     * Call once per game-loop iteration BEFORE querying any key state.
     *
     * TEACHING NOTE — Per-Frame Input Snapshot
     * ──────────────────────────────────────────
     * By reading input once per frame and caching the result, we ensure all
     * game systems that run during a frame see THE SAME input state.
     * This prevents subtle bugs where System A processes "key pressed"
     * and System B processes "key not pressed" in the same frame because
     * getch() was called at different times.
     *
     * After PollInput(), the raw key and the resolved InputAction are
     * available until the next call to PollInput().
     *
     * @return The raw ncurses key code, or GameKeys::NONE if no key pressed.
     */
    int PollInput();

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------

    /**
     * @brief Return the current frame's raw ncurses key code.
     *
     * Prefer GetAction() for game logic; use GetRawKey() for edge cases
     * (text input fields, debug overlays, custom bindings).
     *
     * @return Key code from the last PollInput(), or GameKeys::NONE.
     */
    [[nodiscard]] int GetRawKey() const { return m_currentKey; }

    /**
     * @brief Return the current frame's logical InputAction.
     *
     * This is the primary query interface for game systems.
     * Returns InputAction::NONE if no key was pressed or the pressed key
     * has no binding.
     *
     * @return Resolved InputAction from the current raw key.
     */
    [[nodiscard]] InputAction GetAction() const { return m_currentAction; }

    /**
     * @brief Return true if a specific raw key code was pressed this frame.
     *
     * @param key  A GameKeys:: constant or any ncurses KEY_ value.
     * @return     true if that exact key code was returned by getch().
     *
     * @code
     *   if (input.IsKeyPressed(GameKeys::ATTACK)) { StartAttack(); }
     * @endcode
     */
    [[nodiscard]] bool IsKeyPressed(int key) const;

    /**
     * @brief Return true if a specific InputAction was triggered this frame.
     *
     * @param action  The logical action to test.
     * @return        true if the current action matches.
     */
    [[nodiscard]] bool IsActionActive(InputAction action) const;

    /**
     * @brief Return a human-readable name for a raw key code (for debugging).
     *
     * @param key  ncurses key code or ASCII code.
     * @return     std::string like "KEY_UP", "Enter", "'a'", "Unknown(305)".
     */
    [[nodiscard]] std::string KeyName(int key) const;

    /**
     * @brief Return a human-readable name for an InputAction (for UI hints).
     *
     * @param action  The action to name.
     * @return        std::string like "Move Up", "Attack", "Open Inventory".
     */
    [[nodiscard]] std::string ActionName(InputAction action) const;

    // -----------------------------------------------------------------------
    // Key binding
    // -----------------------------------------------------------------------

    /**
     * @brief Bind a raw key code to a logical InputAction.
     *
     * TEACHING NOTE — Rebinding at Runtime
     * ──────────────────────────────────────
     * The binding table is a simple hash map: raw key → action.
     * Calling Bind(KEY_UP, InputAction::MOVE_UP) and later
     *         Bind('w',    InputAction::MOVE_UP)
     * supports both arrow-key and WASD navigation simultaneously.
     *
     * To load custom bindings from a config file, read each line and call
     * Bind() with the parsed values.
     *
     * @param rawKey  ncurses key code or ASCII value.
     * @param action  The InputAction to trigger when rawKey is pressed.
     */
    void Bind(int rawKey, InputAction action);

    /**
     * @brief Restore default key bindings (as documented in GameKeys::).
     *
     * Clears all custom bindings and resets to the defaults configured
     * in InputSystem::Init().
     */
    void ResetBindings();

    // -----------------------------------------------------------------------
    // Settings
    // -----------------------------------------------------------------------

    /**
     * @brief Set the ncurses getch() timeout in milliseconds.
     *
     * Default: 50ms (20fps polling rate).
     * Increase for less CPU usage on slow systems;
     * decrease for more responsive input (but higher CPU usage).
     *
     * @param ms  Timeout in milliseconds. 0 = non-blocking, -1 = blocking.
     */
    void SetInputTimeout(int ms);

private:
    // -----------------------------------------------------------------------
    // Private constructor — Singleton, not publicly constructable.
    // -----------------------------------------------------------------------
    InputSystem() = default;
    ~InputSystem() = default;

    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Resolve m_currentKey → m_currentAction using the binding table.
     *
     * Called internally at the end of PollInput().
     */
    void ResolveAction();

    /**
     * @brief Set up all default key → action bindings.
     */
    void SetupDefaultBindings();

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    /// Raw ncurses key code from the most recent PollInput() call.
    int          m_currentKey    = GameKeys::NONE;

    /// Resolved logical action from the most recent PollInput() call.
    InputAction  m_currentAction = InputAction::NONE;

    /// Mapping from raw ncurses key codes → logical InputActions.
    /// TEACHING NOTE: std::unordered_map gives O(1) average-case lookup
    ///   (hash table) vs O(log n) for std::map (red-black tree).
    ///   For input lookup (called every frame) the constant factor matters.
    std::unordered_map<int, InputAction> m_bindings;

    /// Whether Init() has been called successfully.
    bool m_initialised = false;

    /// Current getch() timeout in milliseconds.
    int  m_timeoutMs   = 50;
};
