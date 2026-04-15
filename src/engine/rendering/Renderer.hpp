/**
 * @file Renderer.hpp
 * @brief ASCII/terminal renderer built on top of ncurses.
 *
 * ============================================================================
 * TEACHING NOTE — Why ncurses for a Game Engine?
 * ============================================================================
 *
 * Most "real" game engines use OpenGL, Vulkan, or DirectX for hardware-
 * accelerated 2D/3D rendering.  Those APIs require hundreds of lines of setup
 * before a single pixel appears.  For an educational engine our goal is to
 * focus on GAME-ENGINE CONCEPTS (ECS, scripting, combat systems, AI) rather
 * than graphics driver boilerplate.
 *
 * ncurses (New Curses) solves this beautifully:
 *  • It draws text characters and coloured blocks to the terminal.
 *  • Setup is a handful of function calls (no GPU required).
 *  • Students can run the engine over SSH on any machine.
 *  • The "rendering pipeline" is simple enough to reason about in a lecture.
 *
 * The conceptual mapping to a real renderer is:
 *
 *   ncurses concept            | Real renderer equivalent
 *   ---------------------------+-----------------------------
 *   WINDOW* (character buffer) | Back-buffer / framebuffer
 *   refresh() / wrefresh()     | Present / SwapBuffers
 *   COLOR_PAIR(n)              | Shader uniform / material
 *   mvaddch(y,x,c)             | Draw call (one texel)
 *   clear()                    | Clear colour pass
 *
 * ============================================================================
 * TEACHING NOTE — ncurses Coordinate System
 * ============================================================================
 *
 * ncurses uses (row, col) = (y, x) ordering, which is the OPPOSITE of the
 * mathematical (x, y) convention.  This trips up almost every new learner.
 *
 * Internally ncurses thinks in "row-major" order (like a printed page):
 *   Row 0 is the TOP of the screen.
 *   Col 0 is the LEFT  of the screen.
 *   Increasing row → moving DOWN.
 *   Increasing col → moving RIGHT.
 *
 * All public API functions in this engine use the conventional (x, y) / (col,
 * row) order.  The renderer is responsible for swapping them when calling into
 * ncurses, keeping the rest of the engine free from this quirk.
 *
 * ============================================================================
 * TEACHING NOTE — ncurses Color Pairs
 * ============================================================================
 *
 * ncurses represents colour as a PAIR: (foreground, background).
 * - Up to 256 pairs are available on a 256-colour terminal.
 * - Pair 0 is the terminal default (usually white on black) and cannot be
 *   changed.
 * - You define pairs 1–255 with: init_pair(pair_id, fg_colour, bg_colour);
 * - You apply a pair with: attron(COLOR_PAIR(pair_id));
 * - You reset it with:     attroff(COLOR_PAIR(pair_id));
 *
 * Colours available without 256-colour mode:
 *   COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_YELLOW,
 *   COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE
 *
 * The CP_* constants defined below map game concepts (player, enemy, UI) to
 * colour-pair IDs that are initialised inside TerminalRenderer::Init().
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
// Standard library headers
// ---------------------------------------------------------------------------
#include <string>       // std::string — used in DrawString / DrawBorderedBox
#include <cstdint>      // uint8_t, int32_t — fixed-width types
#include <stdexcept>    // std::runtime_error — thrown on ncurses init failure

// ---------------------------------------------------------------------------
// ncurses — the terminal rendering library
// ---------------------------------------------------------------------------
// ncurses.h brings in all the ncurses API:
//   initscr(), endwin(), init_pair(), attron() …
//   WINDOW*, KEY_UP, KEY_DOWN, ACS_* box-drawing characters, etc.
//
// On Linux systems install with:   sudo apt-get install libncurses-dev
// Link with:                       -lncurses
#include <ncurses.h>

// ---------------------------------------------------------------------------
// Engine headers
// ---------------------------------------------------------------------------
#include "engine/core/Types.hpp"   // Vec2, ElementType, EntityID …

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
// TileType is defined in GameData.hpp.  We forward-declare the scoped enum
// here so Renderer.hpp can reference it in function signatures WITHOUT pulling
// the entire GameData.hpp (with its large inline data tables) into every
// translation unit that uses the renderer.
//
// TEACHING NOTE — Forward-declaring a scoped enum
// ─────────────────────────────────────────────────
// C++11 allows forward-declaring an enum class if you also specify its
// underlying type.  The declaration must exactly match the definition.
//   enum class TileType : uint8_t;   ← forward declaration (no enumerators)
// Any file that needs the ACTUAL enumerator names (WALL, FLOOR, etc.) must
// still #include "game/GameData.hpp".
enum class TileType : uint8_t;


// ===========================================================================
// Color Pair Constants
// ===========================================================================

/**
 * @defgroup ColorPairs ncurses Color Pair IDs
 * @{
 *
 * TEACHING NOTE — Why constexpr instead of #define?
 * ──────────────────────────────────────────────────
 * Old C code used #define for constants:
 *   #define CP_PLAYER 2
 * The preprocessor blindly replaces the token before compilation, so the
 * compiler never sees the name, which makes debugging painful.
 *
 * Modern C++ uses `constexpr int` (or `constexpr auto`):
 *   constexpr int CP_PLAYER = 2;
 * This is type-safe, scoped, and visible to the debugger.
 *
 * `constexpr` means "evaluate this at compile time."  The value is baked
 * directly into the binary just like #define, but with full type-checking.
 */

/// Terminal default colours (white text on black).  Pair 0 is reserved.
constexpr int CP_DEFAULT   = 1;

/// The player character — bright cyan so they stand out on the map.
constexpr int CP_PLAYER    = 2;

/// Hostile enemies — red to signal danger.
constexpr int CP_ENEMY     = 3;

/// Non-player characters (shopkeepers, quest givers) — yellow / gold.
constexpr int CP_NPC       = 4;

/// Walls and impassable terrain — dark grey / white on black.
constexpr int CP_WALL      = 5;

/// Walkable floor tiles — dim white.
constexpr int CP_FLOOR     = 6;

/// Water tiles — bright blue.
constexpr int CP_WATER     = 7;

/// Grass and outdoor terrain — green.
constexpr int CP_GRASS     = 8;

/// UI menu background — blue on white (classic RPG menu).
constexpr int CP_MENU      = 9;

/// Highlighted selection in menus — black on yellow (inverse of NPC).
constexpr int CP_HIGHLIGHT = 10;

/// Damage numbers / animations — bright red.
constexpr int CP_DAMAGE    = 11;

/// Healing numbers / animations — bright green.
constexpr int CP_HEAL      = 12;

/// Quest-related markers and text — magenta / purple.
constexpr int CP_QUEST     = 13;

/// Gold / currency display — bright yellow.
constexpr int CP_GOLD      = 14;

/// Danger warnings (poison zones, boss rooms) — red on black, bold.
constexpr int CP_DANGER    = 15;

/// Total number of colour pairs registered.  Must be ≤ 256.
constexpr int CP_COUNT     = 16;

/** @} */  // end ColorPairs


// ===========================================================================
// TerminalRenderer
// ===========================================================================

/**
 * @class TerminalRenderer
 * @brief Manages ncurses initialisation and provides primitive drawing calls.
 *
 * TEACHING NOTE — The Renderer Layer
 * ────────────────────────────────────
 * In a layered engine architecture the renderer sits between the OS/hardware
 * and higher-level systems (map renderer, UI system, combat display):
 *
 *   [ Game Logic ]   ← calls →   [ MapRenderer / UIRenderer ]
 *                                         ↓ calls
 *                               [ TerminalRenderer ]   ← this class
 *                                         ↓ calls
 *                               [ ncurses C library ]
 *                                         ↓
 *                               [ Terminal / OS ]
 *
 * Responsibilities of this class:
 *  1. ncurses lifecycle (Init / Shutdown).
 *  2. Primitive drawing operations (characters, strings, boxes, progress bars).
 *  3. Flushing the back-buffer to the screen (Refresh).
 *  4. Querying terminal dimensions (GetWidth / GetHeight).
 *
 * TEACHING NOTE — Why a Class Instead of Global Functions?
 * ─────────────────────────────────────────────────────────
 * Encapsulating ncurses in a class means:
 *  • All ncurses state lives in one place.
 *  • We can mock/replace the renderer in tests.
 *  • The destructor provides a safety net (calling Shutdown if forgotten).
 *  • It is easy to extend — swap ncurses for SDL_ttf without touching callers.
 */
class TerminalRenderer {
public:
    // -----------------------------------------------------------------------
    // Construction / Lifecycle
    // -----------------------------------------------------------------------

    /**
     * @brief Default constructor.  Does NOT initialise ncurses automatically.
     *
     * TEACHING NOTE — Separating Construction from Initialisation
     * ────────────────────────────────────────────────────────────
     * Many engine objects separate the constructor (allocates memory, sets
     * defaults) from an explicit Init() call (acquires OS resources).
     * This is the "two-phase initialisation" pattern.
     *
     * Reason: constructors cannot return error codes.  Init() can (it returns
     * bool here and logs failures).  This also makes the object safe to store
     * as a member variable before the terminal is ready.
     */
    TerminalRenderer()  = default;

    /**
     * @brief Destructor — automatically shuts down ncurses if still active.
     *
     * TEACHING NOTE — RAII (Resource Acquisition Is Initialisation)
     * ──────────────────────────────────────────────────────────────
     * RAII is one of C++'s most important idioms.  Resources (memory, file
     * handles, OS handles) are acquired in a constructor or Init() and
     * released in the destructor.  This guarantees cleanup even if an
     * exception is thrown, eliminating resource leaks.
     *
     * Here the "resource" is the raw terminal: ncurses takes over the
     * terminal (disabling echo, hiding the cursor, etc.).  If we crash or
     * throw without calling endwin(), the terminal is left in a broken state.
     * The destructor prevents that.
     */
    ~TerminalRenderer();

    // -----------------------------------------------------------------------
    // Lifecycle methods
    // -----------------------------------------------------------------------

    /**
     * @brief Initialise ncurses and configure the terminal for game use.
     *
     * Call this once at application startup before any drawing.
     *
     * Performs:
     *  1. initscr()         — enters curses mode (raw terminal).
     *  2. cbreak()          — disables line-buffering (keys arrive instantly).
     *  3. noecho()          — typed keys are NOT echoed to the screen.
     *  4. keypad(stdscr,…)  — enables function / arrow-key decoding.
     *  5. curs_set(0)       — hides the blinking cursor.
     *  6. Color pair setup  — registers all CP_* pairs via init_pair().
     *
     * @return true on success, false if ncurses could not start.
     */
    bool Init();

    /**
     * @brief Tear down ncurses and restore the terminal to normal mode.
     *
     * Calls endwin() to undo all the changes made by Init().
     * Safe to call multiple times (guarded by m_initialised flag).
     */
    void Shutdown();

    // -----------------------------------------------------------------------
    // Drawing primitives
    // -----------------------------------------------------------------------

    /**
     * @brief Erase every character in the ncurses back-buffer.
     *
     * TEACHING NOTE — Back-Buffer / Double Buffering
     * ──────────────────────────────────────────────
     * ncurses maintains an internal "virtual screen" (the back-buffer).
     * Drawing calls write to this buffer.  The screen is only updated when
     * Refresh() is called.  This is conceptually identical to double-
     * buffering in OpenGL (glClear → draw → SwapBuffers).
     *
     * Without double-buffering you would see partial frames — flickering.
     * With it, the player only ever sees complete, consistent frames.
     *
     * clear() marks every cell of the virtual screen as blank.
     * erase() does the same but does not move the cursor; we use clear().
     */
    void Clear();

    /**
     * @brief Draw a single character at screen position (x, y).
     *
     * TEACHING NOTE — The Character Cell Model
     * ─────────────────────────────────────────
     * An ASCII terminal divides the screen into a grid of "cells".
     * Each cell holds exactly ONE character plus colour attributes.
     * This maps perfectly to tile-based 2D games:
     *   cell (x, y) = tile at column x, row y.
     *
     * @param x         Column (0 = left edge).
     * @param y         Row    (0 = top  edge).
     * @param c         ASCII character to draw.
     * @param colorPair CP_* constant selecting foreground/background colours.
     */
    void DrawChar(int x, int y, char c, int colorPair = CP_DEFAULT);

    /**
     * @brief Draw a null-terminated string starting at (x, y).
     *
     * Strings are drawn left-to-right.  Characters beyond GetWidth() are
     * clipped silently (ncurses wraps at the edge; we truncate earlier).
     *
     * @param x         Column of the first character.
     * @param y         Row of the first character.
     * @param s         The string to draw.
     * @param colorPair CP_* colour pair for the entire string.
     */
    void DrawString(int x, int y, const std::string& s,
                    int colorPair = CP_DEFAULT);

    /**
     * @brief Draw a filled rectangular region using a single character.
     *
     * Useful for clearing UI panels before redrawing them.
     *
     * @param x         Left column  of the rectangle.
     * @param y         Top  row     of the rectangle.
     * @param w         Width  in columns (≥ 1).
     * @param h         Height in rows    (≥ 1).
     * @param colorPair CP_* colour pair for the fill character.
     * @param fillChar  Character to fill with (default: space = clear).
     */
    void DrawBox(int x, int y, int w, int h,
                 int colorPair = CP_DEFAULT, char fillChar = ' ');

    /**
     * @brief Draw a rectangle with a single-line ACS border and a title bar.
     *
     * TEACHING NOTE — ACS Box-Drawing Characters
     * ────────────────────────────────────────────
     * ncurses provides the ACS_* macros for portable box-drawing characters
     * (the ┌ ─ ┐ │ └ ┘ glyphs).  These are rendered as real Unicode box-
     * drawing characters on modern terminals, but fall back gracefully to
     * ASCII (+, -, |) on older systems.
     *
     * ACS_ULCORNER = ┌,  ACS_URCORNER = ┐
     * ACS_LLCORNER = └,  ACS_LRCORNER = ┘
     * ACS_HLINE    = ─,  ACS_VLINE    = │
     *
     * @param x         Left column of the outer border.
     * @param y         Top  row    of the outer border.
     * @param w         Total width  including border (≥ 3).
     * @param h         Total height including border (≥ 3).
     * @param title     Text shown in the top border bar (centred).
     * @param colorPair CP_* colour pair for border and title.
     */
    void DrawBorderedBox(int x, int y, int w, int h,
                         const std::string& title,
                         int colorPair = CP_MENU);

    /**
     * @brief Draw a horizontal progress bar (e.g. HP, MP, XP bars).
     *
     * TEACHING NOTE — Visual Feedback in Game UI
     * ────────────────────────────────────────────
     * Progress bars communicate numerical quantities intuitively.
     * A full bar is drawn as a row of filled block characters (█ = ACS_BLOCK
     * or a '#' fallback).  The unfilled portion uses a lighter character.
     *
     * Example (50% full, width=10):  [#####     ]
     *
     * @param x         Left column of the progress bar (does NOT include '[').
     * @param y         Row of the bar.
     * @param w         Total width in columns including brackets.
     * @param current   Current value (clamped to [0, max]).
     * @param max       Maximum value (must be > 0).
     * @param colorPair CP_* colour pair for the filled portion.
     */
    void DrawProgressBar(int x, int y, int w,
                         int current, int max,
                         int colorPair = CP_HEAL);

    // -----------------------------------------------------------------------
    // Frame management
    // -----------------------------------------------------------------------

    /**
     * @brief Flush the back-buffer to the physical terminal.
     *
     * TEACHING NOTE — The Game Loop and Refresh
     * ───────────────────────────────────────────
     * A typical game loop looks like:
     *   while (running) {
     *     ProcessInput();
     *     Update(deltaTime);
     *     Clear();         ← erase back-buffer
     *     Draw();          ← write to back-buffer
     *     Refresh();       ← blit back-buffer → screen
     *   }
     *
     * Only calling Refresh() at the END of all drawing minimises flicker
     * because the player never sees an intermediate frame.
     */
    void Refresh();

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------

    /**
     * @brief Return the current terminal width in columns.
     *
     * TEACHING NOTE — Dynamic Terminal Sizes
     * ────────────────────────────────────────
     * Terminals can be resized at runtime (SIGWINCH on Unix).  A robust
     * renderer polls getmaxx/getmaxy on every frame so the UI adapts.
     * For this engine we read dimensions fresh each time from ncurses.
     *
     * @return Number of columns available (usually 80–220 on modern systems).
     */
    [[nodiscard]] int GetWidth()  const;

    /**
     * @brief Return the current terminal height in rows.
     * @return Number of rows available (usually 24–60 on modern systems).
     */
    [[nodiscard]] int GetHeight() const;

    /**
     * @brief Returns true if ncurses has been successfully initialised.
     */
    [[nodiscard]] bool IsInitialised() const { return m_initialised; }

private:
    // -----------------------------------------------------------------------
    // Private helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Register all CP_* colour pairs with ncurses via init_pair().
     *
     * Called once from Init() after start_color().  Extracted to a separate
     * method for readability (Init() is already doing several things).
     */
    void InitColorPairs();

    /**
     * @brief Clamp a coordinate so it stays inside the terminal buffer.
     *
     * ncurses silently wraps or crashes when you draw outside the window.
     * This helper lets the rest of the code be slightly sloppy without
     * causing undefined behaviour.
     *
     * @param x  Column to clamp.
     * @param y  Row to clamp.
     * @return   true if (x,y) is inside the terminal after clamping.
     */
    bool ClampToScreen(int& x, int& y) const;

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    /// Guards against double-Init or drawing before Init.
    bool m_initialised = false;
};


// ===========================================================================
// MapRenderer
// ===========================================================================

/**
 * @class MapRenderer
 * @brief Renders tile maps and entities onto a sub-region of the terminal.
 *
 * TEACHING NOTE — Separation of Concerns
 * ────────────────────────────────────────
 * TerminalRenderer knows nothing about GAME concepts — it draws chars.
 * MapRenderer knows game concepts (tiles, fog-of-war, entities) but delegates
 * all actual drawing to a TerminalRenderer reference it holds.
 *
 * This is the "Dependency Injection" pattern:
 *   MapRenderer depends on TerminalRenderer (an abstraction) rather than
 *   calling ncurses directly.  If we swap ncurses for SDL we only need to
 *   provide a new TerminalRenderer implementation, not rewrite MapRenderer.
 *
 * TEACHING NOTE — The Viewport
 * ─────────────────────────────
 * A game map is often LARGER than the terminal window.  The viewport is a
 * window that tracks the player and shows only the visible portion of the map.
 *
 *   Full map (e.g. 200×200 tiles)
 *   ┌────────────────────────────┐
 *   │                            │
 *   │     ┌──────────┐           │
 *   │     │ VIEWPORT │  camera   │
 *   │     │  (shown) │ ──────►  │
 *   │     └──────────┘           │
 *   │                            │
 *   └────────────────────────────┘
 *
 * m_viewOriginX/Y is the top-left tile of what is currently visible.
 * To convert a world tile (wx, wy) to a screen position:
 *   screenX = (wx - m_viewOriginX) * m_tileWidth  + m_screenOffsetX
 *   screenY = (wy - m_viewOriginY) * m_tileHeight + m_screenOffsetY
 *
 * For ASCII rendering each tile is one character, so m_tileWidth = 1.
 */
class MapRenderer {
public:
    /**
     * @brief Construct a MapRenderer backed by the given TerminalRenderer.
     *
     * @param renderer     The terminal renderer to draw into.
     * @param screenX      Left column of the map viewport on screen.
     * @param screenY      Top  row    of the map viewport on screen.
     * @param viewportW    Width  of the viewport in columns.
     * @param viewportH    Height of the viewport in rows.
     */
    MapRenderer(TerminalRenderer& renderer,
                int screenX, int screenY,
                int viewportW, int viewportH);

    // -----------------------------------------------------------------------
    // Camera / Viewport control
    // -----------------------------------------------------------------------

    /**
     * @brief Centre the viewport on the given world-space tile coordinate.
     *
     * Call this every frame with the player's tile position so the map
     * scrolls to keep them centred.
     *
     * @param centerTileX  World-space tile column to centre on.
     * @param centerTileY  World-space tile row    to centre on.
     */
    void SetCameraCenter(int centerTileX, int centerTileY);

    /**
     * @brief Directly set the viewport's top-left world-space tile.
     *
     * Useful when you want fine-grained camera control (e.g. cinematic pan).
     */
    void SetViewOrigin(int tileX, int tileY);

    // -----------------------------------------------------------------------
    // Map drawing
    // -----------------------------------------------------------------------

    /**
     * @brief Draw a single tile at a given WORLD-SPACE position.
     *
     * The method converts the world position to screen space and draws
     * the appropriate character and colour for that TileType.
     *
     * TEACHING NOTE — Visibility States
     * ───────────────────────────────────
     * A typical tile can be in one of three states:
     *
     *   1. VISIBLE   — Inside the player's field of view (FOV).
     *                  Drawn at full brightness with correct colour.
     *   2. EXPLORED  — The player visited here before but it is outside FOV.
     *                  Drawn in dim grey so the map stays legible.
     *   3. UNEXPLORED — Never seen.
     *                  Drawn as solid black or completely omitted.
     *
     * @param worldX   Tile column in world space.
     * @param worldY   Tile row    in world space.
     * @param tile     The type of tile to render.
     * @param visible  True if the tile is currently in the player's FOV.
     * @param explored True if the player has visited this tile before.
     */
    void DrawTile(int worldX, int worldY,
                  TileType tile, bool visible, bool explored);

    /**
     * @brief Draw an entity (player, enemy, NPC) at a world-space position.
     *
     * Entities are drawn on top of tiles.  The caller supplies the symbol
     * character and colour pair so that DrawEntity is not coupled to any
     * specific entity type.
     *
     * @param worldX    Tile column in world space.
     * @param worldY    Tile row    in world space.
     * @param symbol    ASCII character representing this entity (@, !, G, etc.)
     * @param colorPair CP_* colour pair (use CP_PLAYER, CP_ENEMY, etc.)
     */
    void DrawEntity(int worldX, int worldY, char symbol, int colorPair);

    /**
     * @brief Draw a circular field-of-view overlay around a world position.
     *
     * TEACHING NOTE — FOV / Fog of War
     * ──────────────────────────────────
     * Fog of War (FOW) is a classic game mechanic that limits how much of
     * the map the player can see.  A simple implementation:
     *
     *   For every cell in the viewport:
     *     dist = distance from viewer to cell
     *     if dist ≤ radius → visible (draw bright)
     *     else             → not visible (draw dim or skip)
     *
     * More sophisticated engines use ray-casting or the "shadowcasting"
     * algorithm (Björn Bergström, 2001) for better corner behaviour.
     * This implementation uses a simple circle check as a teaching example.
     *
     * @param viewerWorldX  World-space column of the FOV origin (the player).
     * @param viewerWorldY  World-space row    of the FOV origin.
     * @param radius        How many tiles away the player can see.
     */
    void DrawFOV(int viewerWorldX, int viewerWorldY, int radius);

    /**
     * @brief Clear the entire viewport region to solid black.
     *
     * Call before drawing any tiles on a new frame.
     */
    void ClearViewport();

    // -----------------------------------------------------------------------
    // Coordinate utilities
    // -----------------------------------------------------------------------

    /**
     * @brief Convert a world-space tile coordinate to screen-space column/row.
     *
     * Returns false and leaves out parameters unchanged if the tile is
     * outside the current viewport (i.e. not visible on screen).
     *
     * @param worldX    Input: world-space column.
     * @param worldY    Input: world-space row.
     * @param screenX   Output: terminal column.
     * @param screenY   Output: terminal row.
     * @return          true if the position falls inside the viewport.
     */
    bool WorldToScreen(int worldX, int worldY,
                       int& screenX, int& screenY) const;

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------

    /// @return Viewport width in columns.
    [[nodiscard]] int GetViewportWidth()  const { return m_viewportW; }
    /// @return Viewport height in rows.
    [[nodiscard]] int GetViewportHeight() const { return m_viewportH; }
    /// @return Current camera origin X (world-space).
    [[nodiscard]] int GetViewOriginX()    const { return m_viewOriginX; }
    /// @return Current camera origin Y (world-space).
    [[nodiscard]] int GetViewOriginY()    const { return m_viewOriginY; }

private:
    // -----------------------------------------------------------------------
    // Private helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Return the display character and colour pair for a given TileType.
     *
     * Centralising tile visuals here means map data (TileType) is completely
     * separate from rendering decisions — the "Model–View" separation.
     *
     * @param tile     The tile type to look up.
     * @param outChar  Output: the character to display.
     * @param outColor Output: the CP_* colour pair to use.
     */
    void GetTileVisuals(TileType tile, char& outChar, int& outColor) const;

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    TerminalRenderer& m_renderer;   ///< Reference to the underlying renderer.
    int m_screenOffsetX;            ///< Terminal column of the viewport's left edge.
    int m_screenOffsetY;            ///< Terminal row    of the viewport's top  edge.
    int m_viewportW;                ///< Viewport width  in columns.
    int m_viewportH;                ///< Viewport height in rows.
    int m_viewOriginX = 0;          ///< World-space tile at the viewport's left edge.
    int m_viewOriginY = 0;          ///< World-space tile at the viewport's top  edge.
};
