/**
 * @file Renderer.cpp
 * @brief Implementation of TerminalRenderer and MapRenderer (ncurses backend).
 *
 * ============================================================================
 * TEACHING NOTE — Translation Units
 * ============================================================================
 * In C++ a program is built from "translation units" — roughly one .cpp file
 * plus all the headers it includes.  By putting implementation in .cpp files:
 *
 *  1. Compilation is faster: changing Renderer.cpp does not force recompilation
 *     of every file that includes Renderer.hpp.
 *  2. The implementation (ncurses calls) is hidden from callers — information
 *     hiding / encapsulation.
 *  3. Linking is simpler: the linker stitches object files together once.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 */

// ---------------------------------------------------------------------------
// Include our own header first.
// TEACHING NOTE — Include Order Convention
// ──────────────────────────────────────────
// Best practice (Google/LLVM style guides) is to include:
//   1. The header for THIS .cpp (ensures it is self-contained)
//   2. C system headers (stdio.h, etc.)
//   3. C++ standard library headers
//   4. Third-party library headers (ncurses, Lua, …)
//   5. Project headers
//
// By including Renderer.hpp first, the compiler verifies that Renderer.hpp
// can be parsed without relying on any prior includes — making it genuinely
// standalone.
// ---------------------------------------------------------------------------
#include "engine/rendering/Renderer.hpp"

// ---------------------------------------------------------------------------
// Standard library
// ---------------------------------------------------------------------------
#include <algorithm>    // std::clamp, std::min, std::max
#include <cstring>      // std::strlen — used when measuring string width

// ---------------------------------------------------------------------------
// Game data (needed to resolve TileType forward declaration)
// ---------------------------------------------------------------------------
// Renderer.hpp only forward-declares TileType.  Here in the .cpp we need the
// full enum definition so we can write the switch statement in DrawTile.
#include "game/GameData.hpp"

// ---------------------------------------------------------------------------
// Engine utilities
// ---------------------------------------------------------------------------
#include "engine/core/Logger.hpp"   // LOG_INFO, LOG_ERROR


// ===========================================================================
// TerminalRenderer — Lifecycle
// ===========================================================================

TerminalRenderer::~TerminalRenderer()
{
    // TEACHING NOTE — Destructor as Safety Net
    // ─────────────────────────────────────────
    // If the program exits without explicitly calling Shutdown() (e.g., via an
    // exception), this destructor ensures ncurses releases the terminal.
    // Without it the user's shell would be left in raw/noecho mode — broken.
    if (m_initialised)
    {
        Shutdown();
    }
}

bool TerminalRenderer::Init()
{
    // Guard against double-initialisation.
    if (m_initialised)
    {
        LOG_INFO("TerminalRenderer::Init called when already initialised — ignoring.");
        return true;
    }

    // -----------------------------------------------------------------------
    // Step 1 — Enter curses mode
    // -----------------------------------------------------------------------
    // TEACHING NOTE: initscr() is THE entry point into ncurses.  It:
    //   • Allocates the WINDOW struct for stdscr (the full-screen window).
    //   • Queries the terminal type (via $TERM env var or terminfo database).
    //   • Sets up the internal virtual-screen buffer.
    //
    // It returns NULL on failure (bad terminal, no $TERM set, etc.).
    if (initscr() == nullptr)
    {
        LOG_ERROR("TerminalRenderer: initscr() failed — terminal not supported.");
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 2 — Input mode
    // -----------------------------------------------------------------------
    // TEACHING NOTE: cbreak() puts the terminal in "cbreak" mode:
    //   • Each keystroke is delivered to the program immediately (no Enter).
    //   • Ctrl-C still generates SIGINT (unlike raw() which intercepts it).
    //
    // noecho() stops pressed keys from appearing on screen — essential for a
    // game where we handle rendering ourselves.
    cbreak();
    noecho();

    // -----------------------------------------------------------------------
    // Step 3 — Enable special keys
    // -----------------------------------------------------------------------
    // TEACHING NOTE: keypad(stdscr, TRUE) tells ncurses to translate multi-
    // byte escape sequences (arrow keys, function keys) into single integer
    // constants (KEY_UP, KEY_DOWN, KEY_F(1), etc.).
    //
    // Without this, pressing ↑ would deliver three bytes: ESC, '[', 'A'.
    // With it, you receive the single value KEY_UP (259 on most systems).
    keypad(stdscr, TRUE);

    // -----------------------------------------------------------------------
    // Step 4 — Non-blocking input with timeout
    // -----------------------------------------------------------------------
    // TEACHING NOTE: timeout(N) sets how long getch() waits for a keypress.
    //   timeout(-1) = block indefinitely (bad for game loops).
    //   timeout(0)  = return ERR immediately if no key is ready.
    //   timeout(N)  = wait up to N milliseconds, then return ERR.
    //
    // We set 50ms — one game tick at 20fps.  The InputSystem can adjust this.
    timeout(50);

    // -----------------------------------------------------------------------
    // Step 5 — Hide the hardware cursor
    // -----------------------------------------------------------------------
    // TEACHING NOTE: curs_set(0) hides the blinking cursor.
    //   0 = invisible, 1 = normal, 2 = very visible.
    // A game doesn't want a cursor wandering around the map.
    curs_set(0);

    // -----------------------------------------------------------------------
    // Step 6 — Colour support
    // -----------------------------------------------------------------------
    // TEACHING NOTE: has_colors() returns TRUE if the terminal supports ANSI
    // colour.  Most modern terminals do.  start_color() activates the colour
    // subsystem.  After this call init_pair() can be used.
    //
    // use_default_colors() is a ncurses extension that allows -1 as the
    // "terminal default" colour for transparent backgrounds.
    if (has_colors())
    {
        start_color();
        use_default_colors();
        InitColorPairs();
    }
    else
    {
        LOG_INFO("TerminalRenderer: terminal does not support colours — using mono.");
    }

    m_initialised = true;
    LOG_INFO("TerminalRenderer: ncurses initialised successfully.");
    LOG_INFO("TerminalRenderer: terminal size = " + std::to_string(GetWidth())
             + "x" + std::to_string(GetHeight()));
    return true;
}

void TerminalRenderer::Shutdown()
{
    if (!m_initialised) { return; }

    // -----------------------------------------------------------------------
    // endwin() — exit curses mode
    // -----------------------------------------------------------------------
    // TEACHING NOTE: endwin() restores:
    //   • Echo mode (typed characters appear on screen again).
    //   • Line-buffering (Enter required for input).
    //   • The hardware cursor (visible again).
    //   • The original terminal colour settings.
    //
    // Always call this before program exit, or the shell will be broken.
    endwin();
    m_initialised = false;
    LOG_INFO("TerminalRenderer: ncurses shut down cleanly.");
}

void TerminalRenderer::InitColorPairs()
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — init_pair(id, foreground, bg)
    // ─────────────────────────────────────────────
    // id         : pair number 1–255 (0 is reserved for terminal default).
    // foreground : one of COLOR_BLACK/RED/GREEN/YELLOW/BLUE/MAGENTA/CYAN/WHITE
    //              or a 256-colour index, or -1 for terminal default.
    // background : same as foreground, or -1 for terminal default.
    //
    // A_BOLD can be added with attron() to make colours brighter on terminals
    // that only support 8 colours (each foreground has a "bright" variant).
    //
    // These pairings deliberately echo JRPG color conventions:
    //   • Cyan player — classic Noctis/hero colour
    //   • Red enemies — universal danger colour
    //   • Yellow NPCs — friendly / attention-grabbing
    // -----------------------------------------------------------------------

    // pair 1 — default text (white on terminal background)
    init_pair(CP_DEFAULT,   COLOR_WHITE,   -1);

    // pair 2 — the player character: bright cyan stands out on any map
    init_pair(CP_PLAYER,    COLOR_CYAN,    -1);

    // pair 3 — enemies: red signals hostility/danger
    init_pair(CP_ENEMY,     COLOR_RED,     -1);

    // pair 4 — NPCs: yellow is warm and friendly
    init_pair(CP_NPC,       COLOR_YELLOW,  -1);

    // pair 5 — walls: bright white on black to read clearly
    init_pair(CP_WALL,      COLOR_WHITE,   COLOR_BLACK);

    // pair 6 — floor: dark (dim white) so it doesn't overshadow entities
    init_pair(CP_FLOOR,     COLOR_WHITE,   -1);

    // pair 7 — water: classic blue
    init_pair(CP_WATER,     COLOR_BLUE,    -1);

    // pair 8 — grass/outdoors: green
    init_pair(CP_GRASS,     COLOR_GREEN,   -1);

    // pair 9 — menu background: blue panel (classic FF menu look)
    init_pair(CP_MENU,      COLOR_WHITE,   COLOR_BLUE);

    // pair 10 — highlighted selection: black text on cyan (inverted player)
    init_pair(CP_HIGHLIGHT, COLOR_BLACK,   COLOR_CYAN);

    // pair 11 — damage numbers: bright red
    init_pair(CP_DAMAGE,    COLOR_RED,     -1);

    // pair 12 — healing numbers: bright green
    init_pair(CP_HEAL,      COLOR_GREEN,   -1);

    // pair 13 — quest markers: magenta / purple
    init_pair(CP_QUEST,     COLOR_MAGENTA, -1);

    // pair 14 — gold / currency: bright yellow
    init_pair(CP_GOLD,      COLOR_YELLOW,  -1);

    // pair 15 — danger zone warnings: red on black, signalling high peril
    init_pair(CP_DANGER,    COLOR_RED,     COLOR_BLACK);
}


// ===========================================================================
// TerminalRenderer — Drawing Primitives
// ===========================================================================

void TerminalRenderer::Clear()
{
    // TEACHING NOTE: clear() in ncurses:
    //   1. Erases the virtual-screen buffer (sets every cell to space).
    //   2. Marks the entire screen as "needs refresh" — the next refresh()
    //      will send a full redraw to the terminal (more reliable than erase()).
    //
    // Use Clear() at the start of each frame before all drawing.
    if (!m_initialised) { return; }
    clear();
}

void TerminalRenderer::DrawChar(int x, int y, char c, int colorPair)
{
    if (!m_initialised) { return; }
    if (!ClampToScreen(x, y)) { return; }   // silently discard out-of-bounds

    // TEACHING NOTE: attron / attroff sandwich
    // ─────────────────────────────────────────
    // attron(ATTRIBUTE)  — activate the attribute for subsequent draw calls.
    // attroff(ATTRIBUTE) — deactivate it.
    //
    // COLOR_PAIR(n) packs the pair index into the curses attribute bitmask.
    // A_BOLD makes the character brighter on 8-colour terminals.
    //
    // mvaddch(y, x, c) moves to (row=y, col=x) and draws character c.
    // NOTE: ncurses uses (row, col) — we pass y first, then x.
    attron(COLOR_PAIR(colorPair));
    mvaddch(y, x, static_cast<chtype>(c));
    attroff(COLOR_PAIR(colorPair));
}

void TerminalRenderer::DrawString(int x, int y, const std::string& s,
                                  int colorPair)
{
    if (!m_initialised || s.empty()) { return; }
    if (!ClampToScreen(x, y)) { return; }

    // Clip the string to avoid writing beyond the right edge.
    const int maxLen = GetWidth() - x;
    if (maxLen <= 0) { return; }

    // Take a substring if the string is too long.
    // TEACHING NOTE: std::string::substr(pos, len) creates a copy of at most
    // `len` characters starting at `pos`.  We only allocate when clipping
    // is needed (common case avoids the allocation).
    const std::string clipped = (static_cast<int>(s.size()) > maxLen)
                                 ? s.substr(0, static_cast<size_t>(maxLen))
                                 : s;

    attron(COLOR_PAIR(colorPair));
    // mvprintw(y, x, fmt, ...) is ncurses' printf equivalent.
    // We use mvaddstr (no format string) to avoid format-string security risks.
    mvaddstr(y, x, clipped.c_str());
    attroff(COLOR_PAIR(colorPair));
}

void TerminalRenderer::DrawBox(int x, int y, int w, int h,
                                int colorPair, char fillChar)
{
    if (!m_initialised || w <= 0 || h <= 0) { return; }

    attron(COLOR_PAIR(colorPair));
    for (int row = 0; row < h; ++row)
    {
        for (int col = 0; col < w; ++col)
        {
            int sx = x + col;
            int sy = y + row;
            if (sx >= 0 && sy >= 0 && sx < GetWidth() && sy < GetHeight())
            {
                mvaddch(sy, sx, static_cast<chtype>(fillChar));
            }
        }
    }
    attroff(COLOR_PAIR(colorPair));
}

void TerminalRenderer::DrawBorderedBox(int x, int y, int w, int h,
                                        const std::string& title,
                                        int colorPair)
{
    // Minimum size: 3 wide × 3 tall so there's room for a 1-char interior.
    if (!m_initialised || w < 3 || h < 3) { return; }

    attron(COLOR_PAIR(colorPair));

    // -----------------------------------------------------------------------
    // Top border row: ┌───[ title ]───┐
    // -----------------------------------------------------------------------
    mvaddch(y, x, ACS_ULCORNER);   // top-left  ┌

    // Build the title portion: ─[ TITLE ]─
    // We centre the title in the available horizontal space (w - 2 border cols).
    const int innerW = w - 2;
    int titleLen = static_cast<int>(title.size());
    // Clamp title length to inner width minus 4 characters for ─[…]─ brackets
    if (titleLen > innerW - 4) { titleLen = innerW - 4; }

    if (!title.empty() && innerW >= 5)
    {
        // Padding on each side of the title
        const int leftPad  = (innerW - titleLen - 2) / 2;  // 2 for [ ]
        const int rightPad = innerW - titleLen - 2 - leftPad;

        for (int i = 0; i < leftPad;  ++i) mvaddch(y, x + 1 + i, ACS_HLINE);
        mvaddch(y, x + 1 + leftPad, '[');
        for (int i = 0; i < titleLen; ++i)
            mvaddch(y, x + 2 + leftPad + i, static_cast<chtype>(title[static_cast<size_t>(i)]));
        mvaddch(y, x + 2 + leftPad + titleLen, ']');
        for (int i = 0; i < rightPad; ++i)
            mvaddch(y, x + 3 + leftPad + titleLen + i, ACS_HLINE);
    }
    else
    {
        // No title — just fill with horizontal line characters
        for (int i = 0; i < innerW; ++i)
            mvaddch(y, x + 1 + i, ACS_HLINE);
    }

    mvaddch(y, x + w - 1, ACS_URCORNER);   // top-right ┐

    // -----------------------------------------------------------------------
    // Middle rows: │                │
    // -----------------------------------------------------------------------
    for (int row = 1; row < h - 1; ++row)
    {
        mvaddch(y + row, x,         ACS_VLINE);       // left  │
        mvaddch(y + row, x + w - 1, ACS_VLINE);       // right │
    }

    // -----------------------------------------------------------------------
    // Bottom border row: └───────────────┘
    // -----------------------------------------------------------------------
    mvaddch(y + h - 1, x,         ACS_LLCORNER);       // bottom-left  └
    for (int i = 0; i < innerW; ++i)
        mvaddch(y + h - 1, x + 1 + i, ACS_HLINE);     // bottom ─
    mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);       // bottom-right ┘

    attroff(COLOR_PAIR(colorPair));
}

void TerminalRenderer::DrawProgressBar(int x, int y, int w,
                                        int current, int max,
                                        int colorPair)
{
    // Safety checks: max must be positive; current is clamped to [0, max].
    if (!m_initialised || w < 3 || max <= 0) { return; }
    current = std::clamp(current, 0, max);

    // -----------------------------------------------------------------------
    // Layout: [ XXXXXXXX     ]
    //          ↑ x+1          w-2 inner cells
    // -----------------------------------------------------------------------
    const int innerW  = w - 2;  // width inside the brackets
    const int filled  = (innerW * current) / max;  // integer division
    const int unfilled = innerW - filled;

    // Outer brackets use default colour
    attron(COLOR_PAIR(CP_DEFAULT));
    mvaddch(y, x,         '[');
    mvaddch(y, x + w - 1, ']');
    attroff(COLOR_PAIR(CP_DEFAULT));

    // Filled portion — use the supplied colour (e.g. green for HP)
    attron(COLOR_PAIR(colorPair));
    for (int i = 0; i < filled; ++i)
    {
        // ACS_BLOCK is a solid ■ block on capable terminals.
        // TEACHING NOTE: the ternary here selects ACS_BLOCK when the terminal
        // supports it (tigetnum("colors") >= 8), otherwise falls back to '#'.
        // For simplicity we always use '#' (reliable on all terminals).
        mvaddch(y, x + 1 + i, '#');
    }
    attroff(COLOR_PAIR(colorPair));

    // Unfilled portion — dim grey (CP_FLOOR is the dimmest default)
    attron(COLOR_PAIR(CP_FLOOR));
    for (int i = 0; i < unfilled; ++i)
    {
        mvaddch(y, x + 1 + filled + i, '.');
    }
    attroff(COLOR_PAIR(CP_FLOOR));
}

void TerminalRenderer::Refresh()
{
    if (!m_initialised) { return; }

    // TEACHING NOTE: refresh() in ncurses
    // ─────────────────────────────────────
    // refresh() computes the DIFFERENCE between the virtual screen (what we've
    // drawn this frame) and the physical screen (what the terminal currently
    // shows), then sends only the changed cells to the terminal.
    //
    // This differential update is far more efficient than repainting everything
    // — the technique is called "dirty-region tracking" and is used by real
    // game engines, GUIs (GTK, Qt), and even web browsers (virtual DOM).
    refresh();
}

int TerminalRenderer::GetWidth() const
{
    // getmaxx(stdscr) returns the number of columns on the standard screen.
    // COLS is a ncurses global that is equivalent (but may lag behind resizes).
    // Using getmaxx() ensures we always have the current terminal width.
    return getmaxx(stdscr);
}

int TerminalRenderer::GetHeight() const
{
    // TEACHING NOTE: LINES and COLS are ncurses globals for terminal height/
    // width.  getmaxy/getmaxx read directly from the WINDOW struct, which is
    // more reliable after a SIGWINCH (terminal resize signal).
    return getmaxy(stdscr);
}

bool TerminalRenderer::ClampToScreen(int& x, int& y) const
{
    // Return false if the coordinate is entirely outside the screen.
    // We do not clamp — drawing at a negative or over-boundary column would
    // cause ncurses to raise an ERR; we simply skip such draws.
    if (x < 0 || y < 0 || x >= GetWidth() || y >= GetHeight()) { return false; }
    return true;
}


// ===========================================================================
// MapRenderer — Implementation
// ===========================================================================

MapRenderer::MapRenderer(TerminalRenderer& renderer,
                         int screenX, int screenY,
                         int viewportW, int viewportH)
    : m_renderer(renderer)
    , m_screenOffsetX(screenX)
    , m_screenOffsetY(screenY)
    , m_viewportW(viewportW)
    , m_viewportH(viewportH)
    , m_viewOriginX(0)
    , m_viewOriginY(0)
{
    // TEACHING NOTE — Member Initialiser List
    // ─────────────────────────────────────────
    // C++ allows you to initialise class members BEFORE the constructor body
    // runs using the initialiser list (the `: member(value)` syntax).
    //
    // Prefer this over assignment in the constructor body for two reasons:
    //   1. const members and reference members (like m_renderer) MUST be
    //      initialised in the list — they cannot be default-constructed then
    //      assigned.
    //   2. For non-trivial types it avoids double-initialisation (default
    //      construct then assign = two operations; list-init = one).
    LOG_INFO("MapRenderer created: viewport " + std::to_string(viewportW)
             + "x" + std::to_string(viewportH)
             + " at screen (" + std::to_string(screenX)
             + "," + std::to_string(screenY) + ")");
}

void MapRenderer::SetCameraCenter(int centerTileX, int centerTileY)
{
    // The camera origin is the TOP-LEFT tile shown in the viewport.
    // To centre on (centerTileX, centerTileY) we subtract half the viewport:
    //   originX = centerX - viewportW/2
    //   originY = centerY - viewportH/2
    //
    // TEACHING NOTE — Integer Division
    // ──────────────────────────────────
    // m_viewportW / 2 uses integer (truncating) division.  For an odd-width
    // viewport this shifts the camera slightly left-of-centre — imperceptible
    // in practice.  Floating-point division + rounding would be more precise
    // but adds complexity for negligible gain.
    m_viewOriginX = centerTileX - (m_viewportW / 2);
    m_viewOriginY = centerTileY - (m_viewportH / 2);
}

void MapRenderer::SetViewOrigin(int tileX, int tileY)
{
    m_viewOriginX = tileX;
    m_viewOriginY = tileY;
}

bool MapRenderer::WorldToScreen(int worldX, int worldY,
                                 int& screenX, int& screenY) const
{
    // Translate world tile coords → viewport-relative → screen-absolute.
    const int relX = worldX - m_viewOriginX;
    const int relY = worldY - m_viewOriginY;

    // If the relative position is outside the viewport, it is not visible.
    if (relX < 0 || relY < 0 || relX >= m_viewportW || relY >= m_viewportH)
    {
        return false;
    }

    screenX = m_screenOffsetX + relX;
    screenY = m_screenOffsetY + relY;
    return true;
}

void MapRenderer::ClearViewport()
{
    // Fill the entire viewport region with spaces (black background).
    m_renderer.DrawBox(m_screenOffsetX, m_screenOffsetY,
                       m_viewportW, m_viewportH, CP_DEFAULT, ' ');
}

void MapRenderer::GetTileVisuals(TileType tile, char& outChar, int& outColor) const
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Data-Driven Tile Rendering
    // ─────────────────────────────────────────────
    // Each TileType maps to a display character and a colour.
    // This switch statement is the "render dictionary" — it bridges
    // the abstract game-data TileType enum with concrete visual choices.
    //
    // An alternative approach is a lookup table (array of structs indexed by
    // the enum value).  That would be faster (O(1) vs O(n) for switch on some
    // compilers) but less readable for students.  Modern compilers compile
    // switch statements on dense enums as jump tables anyway.
    // -----------------------------------------------------------------------
    switch (tile)
    {
        // FLOOR: a middle dot (·) represents empty walkable space.
        // Using ACS_BULLET (·) is ideal but we use '.' for max portability.
        case TileType::FLOOR:
            outChar  = '.';
            outColor = CP_FLOOR;
            break;

        // WALL: a solid '#' block. In many roguelikes '#' = wall.
        case TileType::WALL:
            outChar  = '#';
            outColor = CP_WALL;
            break;

        // WATER: tilde '~' is universally recognised as water.
        case TileType::WATER:
            outChar  = '~';
            outColor = CP_WATER;
            break;

        // GRASS: comma ',' suggests low-lying vegetation.
        case TileType::GRASS:
            outChar  = ',';
            outColor = CP_GRASS;
            break;

        // SAND: period '.' with gold colour — subtle difference from floor.
        case TileType::SAND:
            outChar  = '.';
            outColor = CP_GOLD;
            break;

        // FOREST: caret '^' represents tree canopy (classic JRPG overworld).
        case TileType::FOREST:
            outChar  = '^';
            outColor = CP_GRASS;
            break;

        // MOUNTAIN: caret '^' with white — snow-capped mountains.
        case TileType::MOUNTAIN:
            outChar  = '^';
            outColor = CP_WALL;
            break;

        // ROAD: equals '=' suggests a worn path.
        case TileType::ROAD:
            outChar  = '=';
            outColor = CP_GOLD;
            break;

        // DOOR: plus '+' is the classic roguelike door glyph.
        case TileType::DOOR:
            outChar  = '+';
            outColor = CP_NPC;
            break;

        // STAIRS_UP: '<' pointing left-up (classic convention).
        case TileType::STAIRS_UP:
            outChar  = '<';
            outColor = CP_HIGHLIGHT;
            break;

        // STAIRS_DOWN: '>' pointing right-down.
        case TileType::STAIRS_DOWN:
            outChar  = '>';
            outColor = CP_HIGHLIGHT;
            break;

        // CHEST: '$' or 'c' — here we use '$' for treasure hint.
        case TileType::CHEST:
            outChar  = '$';
            outColor = CP_GOLD;
            break;

        // SAVE_POINT: 'S' with quest colour — clearly important.
        case TileType::SAVE_POINT:
            outChar  = 'S';
            outColor = CP_QUEST;
            break;

        // SHOP: '$' sign for commerce — gold colour.
        case TileType::SHOP:
            outChar  = 'B';     // 'B' for Bazaar / store
            outColor = CP_NPC;
            break;

        // INN: 'I' for Inn.
        case TileType::INN:
            outChar  = 'I';
            outColor = CP_HEAL;
            break;

        default:
            outChar  = '?';
            outColor = CP_DANGER;
            break;
    }
}

void MapRenderer::DrawTile(int worldX, int worldY,
                            TileType tile, bool visible, bool explored)
{
    // Convert world coordinates to screen coordinates.
    int sx, sy;
    if (!WorldToScreen(worldX, worldY, sx, sy)) { return; }   // off-screen

    char  tileChar  = '.';
    int   tileColor = CP_FLOOR;
    GetTileVisuals(tile, tileChar, tileColor);

    if (visible)
    {
        // Fully lit — draw at full colour brightness.
        m_renderer.DrawChar(sx, sy, tileChar, tileColor);
    }
    else if (explored)
    {
        // -----------------------------------------------------------------------
        // TEACHING NOTE — "Explored but not visible" rendering
        // ─────────────────────────────────────────────────────
        // Tiles the player has visited but cannot currently see are drawn in
        // CP_FLOOR (dim white) regardless of their actual type.  This creates
        // the classic "memory map" effect: you know the layout but not the
        // current state (enemies/chests may have changed while out of view).
        // -----------------------------------------------------------------------
        m_renderer.DrawChar(sx, sy, tileChar, CP_FLOOR);
    }
    else
    {
        // Completely unexplored — draw solid black (space).
        m_renderer.DrawChar(sx, sy, ' ', CP_DEFAULT);
    }
}

void MapRenderer::DrawEntity(int worldX, int worldY, char symbol, int colorPair)
{
    int sx, sy;
    if (!WorldToScreen(worldX, worldY, sx, sy)) { return; }

    // Entities are drawn with A_BOLD so they stand out above tiles.
    // TEACHING NOTE: A_BOLD is a ncurses attribute modifier.  Combining it
    // with a COLOR_PAIR creates a bright foreground on the pair's background.
    attron(COLOR_PAIR(colorPair) | A_BOLD);
    mvaddch(sy, sx, static_cast<chtype>(symbol));
    attroff(COLOR_PAIR(colorPair) | A_BOLD);
}

void MapRenderer::DrawFOV(int viewerWorldX, int viewerWorldY, int radius)
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Field of View (FOV) overlay
    // ──────────────────────────────────────────────
    // This draws a darkening overlay OUTSIDE the player's vision radius.
    // Tiles within `radius` world-tile units are left at full brightness;
    // tiles beyond are overdrawn with a dim '#' to represent darkness.
    //
    // Algorithm:
    //   For every screen cell in the viewport:
    //     1. Convert screen → world coordinates.
    //     2. Compute Euclidean distance from the viewer.
    //     3. If distance > radius, draw a dark overlay character.
    //
    // A production engine would use a proper shadowcasting algorithm (e.g.
    // "Precise Permissive FOV" or the "recursive shadowcasting" algorithm)
    // to handle walls blocking sight lines.  This simple circle check is an
    // intentional simplification to keep the code readable for learners.
    // -----------------------------------------------------------------------

    const float r2 = static_cast<float>(radius * radius);

    for (int relY = 0; relY < m_viewportH; ++relY)
    {
        for (int relX = 0; relX < m_viewportW; ++relX)
        {
            // World position of this viewport cell
            const int wX = m_viewOriginX + relX;
            const int wY = m_viewOriginY + relY;

            // Distance squared from viewer (avoids sqrt for comparison)
            const float dX = static_cast<float>(wX - viewerWorldX);
            const float dY = static_cast<float>(wY - viewerWorldY);
            const float dist2 = dX * dX + dY * dY;

            if (dist2 > r2)
            {
                // Outside FOV — draw dark overlay cell
                const int sx = m_screenOffsetX + relX;
                const int sy = m_screenOffsetY + relY;

                // A_DIM makes the character visually darker on 256-colour terms.
                // On 8-colour terminals it renders as normal dim white.
                attron(COLOR_PAIR(CP_DEFAULT) | A_DIM);
                mvaddch(sy, sx, ' ');
                attroff(COLOR_PAIR(CP_DEFAULT) | A_DIM);
            }
        }
    }
}
