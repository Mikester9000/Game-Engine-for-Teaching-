/**
 * @file menu_stack.hpp
 * @brief MenuStack — push/pop screen navigation for in-game menus.
 *
 * ============================================================================
 * TEACHING NOTE — What is a Menu Stack?
 * ============================================================================
 *
 * Modern RPGs present menus in layers: pressing a button opens the Inventory
 * screen on top of the HUD; pressing another button opens the Item Detail
 * sub-screen on top of the Inventory.  When the player presses Back, the
 * sub-screen is dismissed and the Inventory reappears.
 *
 * This is the classic **stack-based navigation** pattern — the same model
 * used in mobile apps (Android back-stack), web browsers (history API), and
 * AAA games (Uncharted, God of War, FF15's system menu).
 *
 * ─── Stack invariant ────────────────────────────────────────────────────────
 *
 * The TOP of the stack is the ACTIVE screen.  Only the active screen receives
 * input and is rendered.  Screens below it are paused but not destroyed — so
 * when you pop the active screen the previous one resumes immediately without
 * reloading any data.
 *
 *   Stack before:        Stack after Push(INVENTORY):
 *   ┌───────────┐        ┌───────────────┐  ← TOP (active)
 *   │    HUD    │ ← TOP  │  INVENTORY    │
 *   └───────────┘        ├───────────────┤
 *                        │      HUD      │
 *                        └───────────────┘
 *
 * ─── Screen types ─────────────────────────────────────────────────────────
 *
 * TEACHING NOTE — Enum vs Class Hierarchy for Screens
 *
 * Two common designs exist:
 *
 *   1. Enum-based   (this design): each screen is identified by a MenuScreen
 *      value.  The render and input code is in a switch statement elsewhere.
 *      Simple, zero overhead, easy to serialise (save/load last screen).
 *
 *   2. Class hierarchy: each screen is a MenuScreen subclass with its own
 *      Render() / HandleInput() virtual methods.  Flexible, supports custom
 *      per-screen state, but requires heap allocation and vtable dispatch.
 *
 * For a teaching engine the enum design is clearer because all logic stays
 * in one place (the GameRuntime render loop) where students can read the
 * whole picture at once.  A production engine would typically use the class
 * hierarchy to isolate each screen's complexity.
 *
 * ─── Thread safety ────────────────────────────────────────────────────────
 *
 * TEACHING NOTE — Single-Threaded Menu Updates
 * MenuStack is NOT thread-safe.  All calls must occur on the game (main)
 * thread.  This is correct for a single-threaded game loop.  If menu updates
 * need to happen from a background thread (unusual) you would add a mutex or
 * move all mutations into a command queue processed on the main thread.
 *
 * ─── Usage example ────────────────────────────────────────────────────────
 *
 * @code
 *   MenuStack menus;
 *
 *   // Game starts at the HUD.
 *   menus.Push(MenuScreen::HUD);
 *
 *   // Player opens the main menu.
 *   menus.Push(MenuScreen::MAIN_MENU);
 *
 *   // Player opens inventory from the main menu.
 *   menus.Push(MenuScreen::INVENTORY);
 *
 *   // Player presses Back — inventory closes; main menu is top again.
 *   menus.Pop();
 *   assert(menus.Top() == MenuScreen::MAIN_MENU);
 *
 *   // Player presses Back again — main menu closes; HUD is top again.
 *   menus.Pop();
 *   assert(menus.Top() == MenuScreen::HUD);
 * @endcode
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2025
 * C++ Standard: C++17
 * Platform: Windows / Linux (no platform dependencies)
 */

#pragma once

#include <vector>
#include <string>
#include <cassert>
#include <functional>  // std::function (for transition callbacks)

// ===========================================================================
// MenuScreen — identifies each distinct screen in the menu hierarchy
// ===========================================================================

/**
 * @enum MenuScreen
 * @brief All possible menu / HUD screens.
 *
 * TEACHING NOTE — Naming Screens as Enum Values
 * ─────────────────────────────────────────────
 * Each enum value represents one "page" of UI.  The enum is the single
 * source of truth: add a new screen here, handle it in the renderer switch,
 * and the whole system knows about it.  No new classes or files needed.
 *
 * The NONE sentinel is used as a "no screen" / empty-stack indicator so
 * callers can check `Top() == MenuScreen::NONE` instead of `IsEmpty()`.
 */
enum class MenuScreen : uint8_t
{
    NONE       = 0,  ///< Sentinel — used when the stack is empty.
    HUD        = 1,  ///< In-game heads-up display (HP/MP bars, ATB, quest).
    MAIN_MENU  = 2,  ///< Pause / main menu (Items, Equipment, Map, Quest, Save).
    INVENTORY  = 3,  ///< Item bag sub-screen.
    EQUIPMENT  = 4,  ///< Weapon / armour equip sub-screen.
    MAP        = 5,  ///< World map / fast-travel.
    QUEST_LOG  = 6,  ///< Active + completed quest list.
    SAVE_MENU  = 7,  ///< Save / load 15-slot selection screen.
    SHOP       = 8,  ///< NPC shop buy/sell screen.
    DIALOGUE   = 9,  ///< NPC dialogue conversation screen.
};

/**
 * @brief Return a human-readable name for a MenuScreen value.
 *
 * TEACHING NOTE — Debugging with Names
 * Storing only an integer in the stack makes logs unreadable ("screen=3").
 * This helper converts the enum to a string so log lines say
 * "pushed INVENTORY" instead of "pushed 3".  It is marked constexpr so
 * the compiler may optimise it away in release builds.
 */
const char* MenuScreenName(MenuScreen screen) noexcept;


// ===========================================================================
// MenuStack
// ===========================================================================

/**
 * @class MenuStack
 * @brief A push/pop navigation stack for in-game menu screens.
 *
 * TEACHING NOTE — Stack vs Queue for UI Navigation
 * ─────────────────────────────────────────────────
 * A stack (LIFO) is the right data structure for nested menus because:
 *
 *   • Opening a sub-screen is always a "deeper" layer → Push.
 *   • Pressing Back always dismisses the current layer → Pop.
 *   • The game never needs to "jump" to an arbitrary layer by index.
 *
 * A queue (FIFO) would be the wrong choice here — it models ordered
 * processing (tasks, events) not layered navigation.
 *
 * std::vector<MenuScreen> gives O(1) push_back / pop_back — exactly what
 * we need.  A std::stack<MenuScreen> adaptor would also work but hides the
 * underlying container from students; the raw vector is more transparent.
 *
 * TEACHING NOTE — Capacity Reserve
 * The maximum useful nesting depth for game menus is around 8 levels
 * (HUD → Main → Items → Item Detail → Confirm → …).  Reserving 8 slots
 * at construction avoids any heap reallocation during gameplay, which
 * could cause micro-stutters on a frame boundary.
 */
class MenuStack
{
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct an empty MenuStack.
     *
     * Reserves 8 slots to avoid reallocation during normal gameplay.
     */
    MenuStack();
    ~MenuStack() = default;

    // Copyable and movable — all members are value types.
    MenuStack(const MenuStack&)            = default;
    MenuStack& operator=(const MenuStack&) = default;
    MenuStack(MenuStack&&)                 = default;
    MenuStack& operator=(MenuStack&&)      = default;

    // =========================================================================
    // Navigation
    // =========================================================================

    /**
     * @brief Push a new screen onto the top of the stack.
     *
     * The pushed screen becomes the active screen immediately.
     * Fires OnScreenChanged if a callback is registered.
     *
     * @param screen  Screen to activate.
     *
     * TEACHING NOTE — Preventing Duplicate Pushes
     * If the caller pushes the same screen that is already on top,
     * Push() silently ignores the request.  This avoids the common bug
     * where a player presses the Inventory button twice and gets two
     * INVENTORY entries on the stack (requiring two Back presses to exit).
     */
    void Push(MenuScreen screen);

    /**
     * @brief Pop the top screen and return to the one below.
     *
     * If the stack has only one screen (typically HUD), Pop() does nothing —
     * the HUD is always the "floor" of the stack.
     *
     * Fires OnScreenChanged if a callback is registered.
     *
     * TEACHING NOTE — Why not pop the HUD?
     * The HUD is the base layer — it is always visible beneath all menus.
     * Popping the last screen would leave the stack empty and the game with
     * no active screen.  The convention is: the caller pushes HUD first at
     * startup and the stack always retains at least one element.
     */
    void Pop();

    /**
     * @brief Pop all screens above the HUD (or clear to 1 element).
     *
     * Useful when transitioning back to gameplay from any nested menu depth.
     * Equivalent to calling Pop() until Size() == 1.
     *
     * TEACHING NOTE — PopAll vs Multiple Pop calls
     * PopAll fires OnScreenChanged once (after the batch pop) rather than
     * firing it N times.  This prevents intermediate screen transitions
     * from triggering unintended side-effects (sound effects, animations,
     * save-game queries) on each intermediate state.
     */
    void PopToBase();

    // =========================================================================
    // Query
    // =========================================================================

    /**
     * @brief Return the active (topmost) screen.
     *
     * @return The top screen, or MenuScreen::NONE if the stack is empty.
     */
    [[nodiscard]] MenuScreen Top() const noexcept;

    /**
     * @brief Return the number of screens currently on the stack.
     */
    [[nodiscard]] int Size() const noexcept;

    /**
     * @brief Return true if the stack has no screens.
     */
    [[nodiscard]] bool IsEmpty() const noexcept;

    /**
     * @brief Return true if the given screen is anywhere in the stack.
     *
     * TEACHING NOTE — Contains() vs Top() == screen
     * Top() only checks the active layer.  Contains() searches the whole
     * stack — useful when you need to know "is the player currently in any
     * kind of menu" (e.g., to pause AI updates while any menu is open).
     *
     * @param screen  Screen to search for.
     */
    [[nodiscard]] bool Contains(MenuScreen screen) const noexcept;

    /**
     * @brief Return a read-only view of the entire stack (bottom to top).
     *
     * Index 0 = bottom of stack (typically HUD).
     * Index Size()-1 = top of stack (active screen).
     */
    [[nodiscard]] const std::vector<MenuScreen>& Screens() const noexcept;

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Register a callback fired whenever the active screen changes.
     *
     * @param cb  Called with the new top screen (MenuScreen::NONE if empty).
     *
     * TEACHING NOTE — Optional Callbacks via std::function
     * We store the callback as a std::function<void(MenuScreen)>.  If no
     * callback is registered (or it is explicitly reset to nullptr), every
     * Push/Pop is a no-op for the callback path — no null-check in the
     * hot path needed because std::function's operator bool handles it.
     */
    void SetOnScreenChanged(std::function<void(MenuScreen)> cb);

private:
    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    std::vector<MenuScreen> m_stack;            ///< Bottom-to-top ordering.
    std::function<void(MenuScreen)> m_onChange; ///< Optional change callback.

    // Internal helper — fires m_onChange if set.
    void NotifyChanged() const;
};
