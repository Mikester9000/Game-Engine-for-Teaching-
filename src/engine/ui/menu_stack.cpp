/**
 * @file menu_stack.cpp
 * @brief MenuStack — push/pop screen navigation implementation.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2025
 */

#include "engine/ui/menu_stack.hpp"

#include <algorithm>  // std::find

// ===========================================================================
// MenuScreenName
// ===========================================================================

// TEACHING NOTE — Lookup Table vs Switch for String Conversion
// ─────────────────────────────────────────────────────────────────────────
// Both patterns are valid.  A switch statement is checked at compile time
// (the compiler warns on missing enum cases with -Wswitch).  A lookup table
// is marginally faster (O(1) array index) but loses that compiler safety.
// We use switch here so that adding a new MenuScreen without updating this
// function produces a compiler warning — a useful teaching safety net.

const char* MenuScreenName(MenuScreen screen) noexcept
{
    switch (screen)
    {
    case MenuScreen::NONE:       return "NONE";
    case MenuScreen::HUD:        return "HUD";
    case MenuScreen::MAIN_MENU:  return "MAIN_MENU";
    case MenuScreen::INVENTORY:  return "INVENTORY";
    case MenuScreen::EQUIPMENT:  return "EQUIPMENT";
    case MenuScreen::MAP:        return "MAP";
    case MenuScreen::QUEST_LOG:  return "QUEST_LOG";
    case MenuScreen::SAVE_MENU:  return "SAVE_MENU";
    case MenuScreen::SHOP:       return "SHOP";
    case MenuScreen::DIALOGUE:   return "DIALOGUE";
    default:                     return "<unknown>";
    }
}

// ===========================================================================
// MenuStack
// ===========================================================================

MenuStack::MenuStack()
{
    // TEACHING NOTE — Reserve to avoid reallocations in the render hot-path
    // ─────────────────────────────────────────────────────────────────────
    // std::vector grows by doubling when capacity is exceeded.  In a tight
    // game loop a surprise reallocation (malloc + memcpy + free) on a Push()
    // call could cause a one-frame hitch.  Reserving 8 slots covers virtually
    // all real-world nesting depths (the deepest FF15 menu chain is ~5 levels)
    // and keeps the stack entirely in a single small heap allocation.
    m_stack.reserve(8);
}

// ---------------------------------------------------------------------------
// Push
// ---------------------------------------------------------------------------

void MenuStack::Push(MenuScreen screen)
{
    // TEACHING NOTE — Guard against duplicate top-of-stack push
    // ─────────────────────────────────────────────────────────
    // If the same screen is already on top, a second Push() is a no-op.
    // This prevents the player from accidentally nesting two copies of the
    // same screen (e.g., double-tapping the Inventory button).
    if (!m_stack.empty() && m_stack.back() == screen)
        return;

    m_stack.push_back(screen);
    NotifyChanged();
}

// ---------------------------------------------------------------------------
// Pop
// ---------------------------------------------------------------------------

void MenuStack::Pop()
{
    // TEACHING NOTE — Floor guard: never pop below 1 screen
    // ──────────────────────────────────────────────────────
    // The HUD (or whatever the caller pushed first) is the "floor".
    // Popping below 1 would leave the game with no active screen — a
    // logic error.  We silently ignore the call so gameplay code can
    // always call Pop() on Back without a guard condition.
    if (m_stack.size() <= 1)
        return;

    m_stack.pop_back();
    NotifyChanged();
}

// ---------------------------------------------------------------------------
// PopToBase
// ---------------------------------------------------------------------------

void MenuStack::PopToBase()
{
    // TEACHING NOTE — Batch pop with a single notification
    // ──────────────────────────────────────────────────────────────────
    // Resizing to 1 rather than calling Pop() in a loop avoids N separate
    // OnScreenChanged fires.  The renderer only needs to know the *final*
    // active screen, not every intermediate state.  One notification keeps
    // the sound/animation system from playing a "menu close" effect N times.
    if (m_stack.size() <= 1)
        return;

    m_stack.resize(1);
    NotifyChanged();
}

// ---------------------------------------------------------------------------
// Top
// ---------------------------------------------------------------------------

MenuScreen MenuStack::Top() const noexcept
{
    // TEACHING NOTE — noexcept on a trivial query
    // ────────────────────────────────────────────
    // Marking this noexcept tells the compiler (and callers) that this
    // method will never throw.  That allows the compiler to omit exception
    // unwind tables in the call site, and allows containers to use move
    // semantics on objects that contain a MenuStack.
    if (m_stack.empty())
        return MenuScreen::NONE;
    return m_stack.back();
}

// ---------------------------------------------------------------------------
// Size / IsEmpty
// ---------------------------------------------------------------------------

int MenuStack::Size() const noexcept
{
    return static_cast<int>(m_stack.size());
}

bool MenuStack::IsEmpty() const noexcept
{
    return m_stack.empty();
}

// ---------------------------------------------------------------------------
// Contains
// ---------------------------------------------------------------------------

bool MenuStack::Contains(MenuScreen screen) const noexcept
{
    // TEACHING NOTE — std::find on a small vector
    // ────────────────────────────────────────────
    // For a stack that is almost always ≤ 8 elements, a linear scan with
    // std::find is optimal: cache-friendly sequential access, no hash overhead.
    // A std::unordered_set would be faster for large collections but wasteful
    // here because the extra bookkeeping outweighs the O(n) linear scan cost.
    return std::find(m_stack.begin(), m_stack.end(), screen) != m_stack.end();
}

// ---------------------------------------------------------------------------
// Screens
// ---------------------------------------------------------------------------

const std::vector<MenuScreen>& MenuStack::Screens() const noexcept
{
    return m_stack;
}

// ---------------------------------------------------------------------------
// SetOnScreenChanged
// ---------------------------------------------------------------------------

void MenuStack::SetOnScreenChanged(std::function<void(MenuScreen)> cb)
{
    // TEACHING NOTE — std::function move semantics
    // ─────────────────────────────────────────────
    // Taking the callback by value and std::move-ing it into the member
    // avoids an extra copy of the callable's internal state.  For a plain
    // function pointer the difference is negligible, but for a lambda that
    // captures by value (e.g., [this]{...}) it avoids copying captured state.
    m_onChange = std::move(cb);
}

// ---------------------------------------------------------------------------
// NotifyChanged (private)
// ---------------------------------------------------------------------------

void MenuStack::NotifyChanged() const
{
    // TEACHING NOTE — operator bool on std::function
    // ────────────────────────────────────────────────
    // std::function converts to true when it holds a callable target (a
    // function pointer, functor, or lambda) and to false when it is empty
    // (default-constructed or assigned nullptr).  This is more idiomatic
    // than comparing to nullptr directly.
    if (m_onChange)
        m_onChange(Top());
}
