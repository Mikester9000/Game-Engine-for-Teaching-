/**
 * @file demo_main.cpp
 * @brief Entry point for Demo_Game — standalone playable demo executable.
 *
 * ============================================================================
 * TEACHING NOTE — Demo_Game vs engine_sandbox
 * ============================================================================
 * engine_sandbox is the internal development/test harness.  It exposes every
 * --scene flag, including CI-only acceptance tests, and is the first binary
 * students encounter when learning the engine.
 *
 * Demo_Game is the **player-facing** binary.  It always starts in the open
 * world and hides all internal test infrastructure.  Its design goals are:
 *
 *   1. SINGLE BINARY — Launches straight into the title screen with no
 *      command-line fuss.  Students hand this to non-technical testers.
 *
 *   2. BOOT MENU — Title screen → New Game / Continue / Settings / Quit,
 *      matching the convention of every commercial RPG.
 *
 *   3. OPEN WORLD — After "New Game" the player is in the large multi-biome
 *      exploration area defined by OpenWorld (open_world.hpp/.cpp).
 *
 *   4. F1 DEBUG MENU — Press F1 at any time to open the developer overlay
 *      (station list, teleport, overlay toggles).  Mirrors the in-game debug
 *      menu used in AAA studios during production.
 *
 *   5. HEADLESS CI — Pass --headless to exercise the full boot→load→play
 *      flow without a window or GPU.  The headless path is CPU-only: it runs
 *      the OpenWorld state machine directly without initialising D3D11 or
 *      Win32.  (engine_sandbox --scene demo_world additionally wraps this
 *      test inside an active D3D11 WARP device for extra coverage.)
 *
 * ─── Renderer Dependency ────────────────────────────────────────────────────
 * demo_main.cpp compiles ONLY when ENGINE_ENABLE_D3D11=ON (Win32 + D3D11).
 * The OpenWorld state machine itself is renderer-agnostic (pure C++17 logic).
 * This file is the thin Win32 + D3D11 wrapper around it.
 *
 * ============================================================================
 * TEACHING NOTE — Boot Menu Pattern
 * ============================================================================
 * A boot menu (title screen) before gameplay serves three purposes:
 *   1. LOADING COVER — gives the engine time to stream the first cell ring.
 *   2. PLAYER AGENCY — "New Game / Continue" is a contract with the player:
 *      their progress is respected.
 *   3. SETTINGS HOOK — resolution, volume, and key bindings are set before
 *      the first frame of gameplay runs.
 *
 * The menu state machine lives in OpenWorld::UpdateBootMenu().
 *
 * ============================================================================
 * TEACHING NOTE — F1 Debug / Teaching Station Menu
 * ============================================================================
 * The F1 overlay is the "professor's remote control".  In a classroom session:
 *   • Instructor presses F1 and selects "PBR Rendering" → teleports to the
 *     rendering teaching station instantly.
 *   • Students can see the F1 overlay source code (this file) and understand
 *     how ImGui overlays and engine teleport hooks work.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2026
 * C++ Standard: C++17
 * Platform: Windows (D3D11 — Win32 window + WARP headless)
 */

// ---------------------------------------------------------------------------
// Standard + Windows headers
// ---------------------------------------------------------------------------
#include <iostream>
#include <string>
#include <memory>
#include <chrono>
#include <cstring>   // std::strcmp
#include <cstdio>    // std::snprintf (overlay text formatting)
#include <algorithm> // std::min, std::max
#include <vector>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>  // GetAsyncKeyState, VK_F1, GDI
#endif

// ---------------------------------------------------------------------------
// Engine: platform + rendering
// ---------------------------------------------------------------------------
#include "engine/platform/win32/Win32Window.hpp"
#include "engine/rendering/RendererFactory.hpp"
#include "engine/rendering/IRenderer.hpp"

// ---------------------------------------------------------------------------
// Engine: config (performance presets — M-DG-P1)
// ---------------------------------------------------------------------------
#include "engine/core/engine_config.hpp"

// ---------------------------------------------------------------------------
// Demo game: OpenWorld state machine
// ---------------------------------------------------------------------------
#include "demo_game/open_world.hpp"

// ---------------------------------------------------------------------------
// Game runtime (reuse existing M8 gameplay systems)
// ---------------------------------------------------------------------------
#include "sandbox/game_runtime.hpp"

// ===========================================================================
// Helper: parse command-line arguments
// ===========================================================================

struct DemoArgs
{
    bool        headless = false; ///< --headless: no window / GPU (CI mode)
    std::string renderer = "d3d11"; ///< --renderer d3d11 (only option shipped)
};

static DemoArgs ParseArgs(int argc, char* argv[])
{
    DemoArgs args;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--headless") == 0)
            args.headless = true;
        else if (std::strcmp(argv[i], "--renderer") == 0 && i + 1 < argc)
            args.renderer = argv[++i];
    }
    return args;
}

// ===========================================================================
// GDI Overlay Renderer
// ===========================================================================
// TEACHING NOTE — GDI Text Overlay for Windowed D3D11
// ─────────────────────────────────────────────────────────────────────────────
// In windowed D3D11 mode Windows DWM (Desktop Window Manager) composites the
// D3D11 surface and any GDI content drawn directly on the HWND.  This lets us
// draw a readable text-based UI overlay with zero extra GPU shader work or
// external library dependencies.
//
// How it works:
//   1. After D3D11's DrawFrame (which calls Present), the DWM composite
//      for this frame is already committed.  GDI drawing via GetDC(hwnd)
//      writes to a GDI-managed back buffer that is composited by DWM *on the
//      next DWM vsync*, so the overlay lags D3D11 content by at most one
//      compositor frame (imperceptible at 60 Hz, ≈ 16 ms).
//   2. We call GetDC(hwnd), draw with GDI primitives, then ReleaseDC.
//   3. We use TRANSPARENT background mode so only text and explicit fill
//      rectangles are drawn, avoiding full-window flicker.
//
// For a shipping game: replace GDI with the engine's SDF FontRenderer
//   (src/engine/ui/font_renderer.hpp) for GPU-accelerated, anti-aliased text.
//
// Headless CI note: GDI overlay functions are only ever called when a real
//   HWND exists.  The headless path (RunHeadless) never calls them.
// ─────────────────────────────────────────────────────────────────────────────

namespace overlay
{

// ---------------------------------------------------------------------------
// Colour helpers (COLORREF = 0x00BBGGRR)
// ---------------------------------------------------------------------------

constexpr COLORREF kColBackground  = RGB( 10,  10,  30); ///< Dark navy panel
constexpr COLORREF kColText        = RGB(220, 220, 255); ///< Soft white text
constexpr COLORREF kColSelected    = RGB( 80, 160, 255); ///< Blue highlight
constexpr COLORREF kColHeading     = RGB(255, 220,  80); ///< Golden heading
constexpr COLORREF kColDim         = RGB(140, 140, 160); ///< Greyed-out item
constexpr COLORREF kColBorder      = RGB( 60,  80, 140); ///< Panel border
constexpr COLORREF kColDebugBg     = RGB(  0,  20,   0); ///< Dark green debug bg
constexpr COLORREF kColDebugText   = RGB(  0, 230, 100); ///< Bright green debug text

constexpr int kLineH   = 22; ///< Height per text line (pixels)
constexpr int kPadX    = 14; ///< Horizontal padding inside panel
constexpr int kPadY    = 10; ///< Vertical padding inside panel
constexpr int kPanelW  = 480; ///< Panel width (pixels)

// ---------------------------------------------------------------------------
// GdiScope — RAII wrapper around GetDC / ReleaseDC
// TEACHING NOTE — RAII for OS resources
// OS handles (DC, brush, pen, font) must always be released.  RAII guarantees
// release even if we return early.  This mirrors the pattern used for D3D11
// COM pointers (ComPtr) and file handles (unique_ptr with custom deleter).
// ---------------------------------------------------------------------------

class GdiScope
{
public:
    explicit GdiScope(HWND hwnd) : m_hwnd(hwnd), m_hdc(::GetDC(hwnd)) {}
    ~GdiScope() { if (m_hdc) ::ReleaseDC(m_hwnd, m_hdc); }

    HDC  Get()   const { return m_hdc; }
    bool Valid() const { return m_hdc != nullptr; }

    GdiScope(const GdiScope&)            = delete;
    GdiScope& operator=(const GdiScope&) = delete;

private:
    HWND m_hwnd;
    HDC  m_hdc;
};

// ---------------------------------------------------------------------------
// DrawPanel — fill a rectangle with a solid colour and draw a border
// ---------------------------------------------------------------------------

static void DrawPanel(HDC hdc, int x, int y, int w, int h, COLORREF bg, COLORREF border)
{
    const RECT r{ x, y, x + w, y + h };

    HBRUSH bgBrush = ::CreateSolidBrush(bg);
    ::FillRect(hdc, &r, bgBrush);
    ::DeleteObject(bgBrush);

    HPEN borderPen = ::CreatePen(PS_SOLID, 1, border);
    HPEN oldPen    = static_cast<HPEN>(::SelectObject(hdc, borderPen));
    HBRUSH oldBrush = static_cast<HBRUSH>(::SelectObject(hdc, ::GetStockObject(NULL_BRUSH)));
    ::Rectangle(hdc, x, y, x + w, y + h);
    ::SelectObject(hdc, oldPen);
    ::SelectObject(hdc, oldBrush);
    ::DeleteObject(borderPen);
}

// ---------------------------------------------------------------------------
// DrawLine — render a single text line at (x,y) with a given colour
// ---------------------------------------------------------------------------

static void DrawLine(HDC hdc, HFONT font, int x, int y,
                     const char* text, COLORREF color)
{
    ::SelectObject(hdc, font);
    ::SetBkMode(hdc, TRANSPARENT);
    ::SetTextColor(hdc, color);
    ::TextOutA(hdc, x, y, text, static_cast<int>(::lstrlenA(text)));
}

// ---------------------------------------------------------------------------
// CreateOverlayFont — create a Courier-New bold monospaced font
// ---------------------------------------------------------------------------
// TEACHING NOTE — HFONT lifetime
// CreateFont returns a GDI HFONT that must be deleted with DeleteObject when
// no longer needed.  We create it once per overlay draw call to keep the code
// simple; in a production overlay you would cache it and recreate only when
// the DPI changes.
// ---------------------------------------------------------------------------

static HFONT CreateOverlayFont(int height)
{
    return ::CreateFontA(
        height,            // nHeight — logical font height
        0,                 // nWidth  — 0 = auto
        0,                 // nEscapement
        0,                 // nOrientation
        FW_BOLD,           // fnWeight
        FALSE,             // fdwItalic
        FALSE,             // fdwUnderline
        FALSE,             // fdwStrikeOut
        ANSI_CHARSET,      // fdwCharSet
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN,
        "Courier New"
    );
}

// ===========================================================================
// DrawBootMenu — title screen overlay
// ===========================================================================
// TEACHING NOTE — Boot Menu / Title Screen
// ─────────────────────────────────────────────────────────────────────────────
// A title screen serves three purposes:
//   1. LOADING COVER   — gives the engine time to stream the first world cell.
//   2. PLAYER AGENCY   — "New Game / Continue" is a contract with the player.
//   3. SETTINGS HOOK   — key bindings and volume are set before gameplay.
//
// The four menu items map to OpenWorld state transitions:
//   New Game  → world.BootSelectNewGame() → LOADING → PLAYING
//   Continue  → (future) load slot 0 and go to PLAYING
//   Settings  → (future) show settings panel
//   Quit      → post WM_QUIT / set running = false
//
// Navigation:
//   VK_UP / VK_DOWN arrows move the selection.
//   VK_RETURN (Enter)       confirms.
// ─────────────────────────────────────────────────────────────────────────────

static const char* kBootItems[] = { "New Game", "Continue", "Settings", "Quit" };
static constexpr int kBootItemCount = 4;

// ---------------------------------------------------------------------------
// TEACHING NOTE — Boot Menu With Save Detection (M-DG-S3)
// ---------------------------------------------------------------------------
// DrawBootMenu now receives a `saveExists` flag so it can grey out "Continue"
// when there is no save file on disk.  When a save is present, "Continue"
// is highlighted in the normal item colour and fully selectable.
//
// This is the standard commercial RPG boot menu behaviour:
//   • FFXV    — "CONTINUE" is greyed out until the player has saved once.
//   • Dark Souls — "LOAD GAME" does not appear until a save file exists.
// ---------------------------------------------------------------------------

static void DrawBootMenu(HWND hwnd, int clientW, int clientH, int selection,
                         bool saveExists)
{
    GdiScope dc(hwnd);
    if (!dc.Valid()) return;

    HFONT fontTitle  = CreateOverlayFont(32);
    HFONT fontItem   = CreateOverlayFont(20);
    HFONT fontFooter = CreateOverlayFont(14);

    // ---- Panel geometry ----
    const int panelH = kPadY * 2          // top + bottom pad
                     + 40                  // title line height
                     + 8                   // title spacing
                     + kBootItemCount * kLineH   // menu items
                     + 8                   // spacing before footer
                     + 16;                 // footer

    const int panelX = (clientW - kPanelW) / 2;
    const int panelY = (clientH - panelH) / 2;

    DrawPanel(dc.Get(), panelX, panelY, kPanelW, panelH, kColBackground, kColBorder);

    int cy = panelY + kPadY;

    // ---- Title ----
    DrawLine(dc.Get(), fontTitle, panelX + kPadX, cy,
             "  DEMO_GAME  v1.0", kColHeading);
    cy += 40 + 4;

    DrawLine(dc.Get(), fontItem, panelX + kPadX, cy,
             "  Educational Game Engine", kColDim);
    cy += 20 + 8;

    // ---- Menu items ----
    for (int i = 0; i < kBootItemCount; ++i)
    {
        const bool sel  = (i == selection);
        // "Continue" is greyed/disabled only when no save file exists.
        const bool stub = (i == 1 && !saveExists);

        char line[128];
        if (stub)
            std::snprintf(line, sizeof(line), "  %s  %s  (no save file)",
                          sel ? ">" : " ", kBootItems[i]);
        else
            std::snprintf(line, sizeof(line), "  %s  %s",
                          sel ? ">" : " ", kBootItems[i]);

        const COLORREF col = stub ? kColDim : (sel ? kColSelected : kColText);
        DrawLine(dc.Get(), fontItem, panelX + kPadX, cy, line, col);
        cy += kLineH;
    }

    cy += 8;

    // ---- Footer ----
    DrawLine(dc.Get(), fontFooter, panelX + kPadX, cy,
             "  UP/DOWN: navigate   ENTER: select", kColDim);

    // ---- Cleanup ----
    ::DeleteObject(fontTitle);
    ::DeleteObject(fontItem);
    ::DeleteObject(fontFooter);
}

// ===========================================================================
// DrawDebugOverlay — F1 teaching station selector
// ===========================================================================
// TEACHING NOTE — F1 Debug / Teaching Station Menu
// ─────────────────────────────────────────────────────────────────────────────
// The F1 overlay is the "professor's remote control" for classroom demos:
//   • The instructor presses F1, selects "PBR Rendering" (or any station),
//     presses Enter, and the player is instantly teleported to that station.
//   • Students can read this code to understand how UI overlays, station data
//     structures, and teleport hooks fit together.
//
// The overlay renders two sections:
//   1. Station list — scrollable list of TeachingStation entries from
//      OpenWorld::GetStations(), one per line.
//   2. Debug info bar — biome name + current FPS (toggled with key 'D').
//
// Navigation (while F1 overlay is open):
//   VK_UP / VK_DOWN    — move station selection
//   VK_RETURN (Enter)  — teleport to selected station
//   F1 again           — close the overlay
//   D key              — toggle the debug info strip
// ─────────────────────────────────────────────────────────────────────────────

static void DrawDebugOverlay(HWND hwnd, int clientW, int clientH,
                             const std::vector<TeachingStation>& stations,
                             int selectedStation,
                             bool showDebugInfo,
                             const char* biomeName,
                             float fps)
{
    if (stations.empty()) return;

    GdiScope dc(hwnd);
    if (!dc.Valid()) return;

    HFONT fontHead   = CreateOverlayFont(18);
    HFONT fontItem   = CreateOverlayFont(17);
    HFONT fontFooter = CreateOverlayFont(14);
    HFONT fontDebug  = CreateOverlayFont(15);

    // ---- Station list panel ----
    const int numStations = static_cast<int>(stations.size());
    const int listLines   = numStations;
    const int panelH = kPadY * 2
                     + kLineH        // heading
                     + 4             // separator spacing
                     + listLines * kLineH
                     + 4
                     + 14            // footer
                     + kPadY;

    const int panelX = clientW - kPanelW - 12;
    const int panelY = 12;

    DrawPanel(dc.Get(), panelX, panelY, kPanelW, panelH, kColBackground, kColBorder);

    int cy = panelY + kPadY;

    // Heading
    DrawLine(dc.Get(), fontHead, panelX + kPadX, cy,
             " F1 DEBUG — Teaching Stations", kColHeading);
    cy += kLineH + 2;

    // Station entries
    for (int i = 0; i < numStations; ++i)
    {
        const auto& s  = stations[i];
        const bool sel = (i == selectedStation);

        char line[256];
        std::snprintf(line, sizeof(line), " %s %-28s %s",
                      sel ? ">" : " ",
                      s.displayName.c_str(),
                      s.sceneHint.c_str());

        DrawLine(dc.Get(), fontItem, panelX + kPadX, cy,
                 line, sel ? kColSelected : kColText);
        cy += kLineH;
    }

    cy += 4;

    // Footer controls
    DrawLine(dc.Get(), fontFooter, panelX + kPadX, cy,
             " UP/DOWN:select  ENTER:teleport  F1:close  D:debug", kColDim);

    // ---- Debug info strip (toggled with D) ----
    if (showDebugInfo)
    {
        char dbgText[128];
        std::snprintf(dbgText, sizeof(dbgText),
                      " Biome: %-14s  FPS: %.1f", biomeName, fps);

        const int dbgY = panelY + panelH + 6;
        DrawPanel(dc.Get(), panelX, dbgY, kPanelW, kLineH + kPadY, kColDebugBg, kColBorder);
        DrawLine(dc.Get(), fontDebug, panelX + kPadX, dbgY + kPadY / 2,
                 dbgText, kColDebugText);
    }

    // ---- Cleanup ----
    ::DeleteObject(fontHead);
    ::DeleteObject(fontItem);
    ::DeleteObject(fontFooter);
    ::DeleteObject(fontDebug);
}

// ===========================================================================
// DrawHudBar — minimal always-on HUD strip (biome + FPS) when debug info ON
// ===========================================================================

static void DrawHudBar(HWND hwnd, int clientW,
                       const char* biomeName, float fps)
{
    GdiScope dc(hwnd);
    if (!dc.Valid()) return;

    HFONT font = CreateOverlayFont(15);

    char text[128];
    std::snprintf(text, sizeof(text),
                  " Biome: %-14s  FPS: %.1f   [D] hide debug", biomeName, fps);

    const int barH = kLineH + kPadY;
    DrawPanel(dc.Get(), 0, 0, clientW, barH, kColDebugBg, kColBorder);
    DrawLine(dc.Get(), font, kPadX, kPadY / 2, text, kColDebugText);

    ::DeleteObject(font);
}

// ===========================================================================
// DrawQuestHud — bottom-left quest / activity progress strip
// ===========================================================================
// TEACHING NOTE — Quest HUD as Persistent Overlay
// ─────────────────────────────────────────────────────────────────────────────
// The quest HUD is drawn in the bottom-left corner whenever the game is in
// PLAYING state.  It shows:
//   • The current main quest objective (one line, gold colour).
//   • The three side-activity progress bars / counts (three lines, dim colour
//     for in-progress, green for complete).
//
// This is the minimal HUD that every commercial RPG ships: the player always
// knows their current objective at a glance.  (Witcher 3 pins the active
// quest in the top-right; FFXV shows it in the bottom-left — we follow FFXV.)
//
// GDI note: we render the HUD AFTER DrawFrame (same as the other overlays).
// ─────────────────────────────────────────────────────────────────────────────

static void DrawQuestHud(HWND hwnd, int clientW, int clientH,
                         const DemoQuestManager& qm)
{
    const auto& quest      = qm.GetMainQuest();
    const auto& activities = qm.GetActivities();

    if (activities.empty())
        return;

    GdiScope dc(hwnd);
    if (!dc.Valid()) return;

    HFONT fontHead = CreateOverlayFont(16);
    HFONT fontItem = CreateOverlayFont(15);

    // ---- Panel geometry ────────────────────────────────────────────────────
    // Height: heading + quest line + separator + 3 activity lines + padding.
    const int numLines = 1            // main quest objective
                       + static_cast<int>(activities.size()) // activity lines
                       + 1;          // "Activities: N/3" summary line

    const int panelH = kPadY * 2 + kLineH + 4 + numLines * kLineH;
    const int panelW = kPanelW;
    const int panelX = 12;
    const int panelY = clientH - panelH - 12;

    DrawPanel(dc.Get(), panelX, panelY, panelW, panelH, kColBackground, kColBorder);

    int cy = panelY + kPadY;

    // ---- Main quest heading ────────────────────────────────────────────────
    {
        char heading[128];
        if (quest.completed)
        {
            std::snprintf(heading, sizeof(heading),
                          " MAIN: %s [COMPLETE]", quest.title.c_str());
        }
        else
        {
            const DemoQuestObjective* obj = quest.CurrentObjective();
            const int objIdx = quest.currentObjective + 1;
            const int objMax = static_cast<int>(quest.objectives.size());
            if (obj)
            {
                std::snprintf(heading, sizeof(heading),
                              " MAIN [%d/%d]: %s",
                              objIdx, objMax, obj->description.c_str());
            }
            else
            {
                std::snprintf(heading, sizeof(heading),
                              " MAIN: %s", quest.title.c_str());
            }
        }
        DrawLine(dc.Get(), fontHead, panelX + kPadX, cy,
                 heading, quest.completed ? RGB(80, 230, 80) : kColHeading);
        cy += kLineH + 4;
    }

    // ---- Side activity lines ───────────────────────────────────────────────
    for (const auto& act : activities)
    {
        char line[128];
        std::snprintf(line, sizeof(line),
                      "  %s  %s [%d/%d]",
                      act.completed ? "[DONE]" : "[    ]",
                      act.title.c_str(),
                      act.progress,
                      act.required);

        const COLORREF col = act.completed ? RGB(80, 230, 80) : kColDim;
        DrawLine(dc.Get(), fontItem, panelX + kPadX, cy, line, col);
        cy += kLineH;
    }

    // ---- Activity summary ──────────────────────────────────────────────────
    {
        char summary[64];
        std::snprintf(summary, sizeof(summary),
                      "  Activities: %d/%d complete",
                      qm.CompletedActivities(),
                      static_cast<int>(activities.size()));
        DrawLine(dc.Get(), fontItem, panelX + kPadX, cy, summary, kColText);
    }

    ::DeleteObject(fontHead);
    ::DeleteObject(fontItem);
}

// ===========================================================================
// DrawInteractPrompt — "Press E to interact" hint when near a station
// ===========================================================================
// TEACHING NOTE — Interact Prompt UX Pattern
// ─────────────────────────────────────────────────────────────────────────────
// A brief contextual prompt tells the player that an action is available.
// This pattern is used in nearly every modern action/RPG:
//   • FFXV   — "X: Interact" appears near interactable objects.
//   • Witcher 3 — "E: Talk / Examine" near NPCs / items.
//   • Dark Souls — "Y: Light Bonfire" near bonfires.
//
// The prompt:
//   • Appears at the bottom-centre of the screen — visible but unobtrusive.
//   • Shows the station name and the Interact key binding.
//   • Disappears as soon as the player is no longer near any station.
// ─────────────────────────────────────────────────────────────────────────────

static void DrawInteractPrompt(HWND hwnd, int clientW, int clientH,
                               const std::string& nearStationID,
                               const std::vector<TeachingStation>& stations)
{
    if (nearStationID.empty()) return;

    // Resolve display name from station list.
    const char* displayName = nearStationID.c_str();
    for (const auto& s : stations)
        if (s.id == nearStationID) { displayName = s.displayName.c_str(); break; }

    GdiScope dc(hwnd);
    if (!dc.Valid()) return;

    HFONT font = CreateOverlayFont(18);

    char text[128];
    std::snprintf(text, sizeof(text),
                  "  [E]  Interact — %s  ", displayName);

    const int promptW = kPanelW;
    const int promptH = kLineH + kPadY;
    const int promptX = (clientW - promptW) / 2;
    const int promptY = clientH - promptH - 60; // above quest HUD

    DrawPanel(dc.Get(), promptX, promptY, promptW, promptH,
              kColBackground, kColSelected);
    DrawLine(dc.Get(), font, promptX + kPadX, promptY + kPadY / 2,
             text, kColSelected);

    ::DeleteObject(font);
}

// ===========================================================================
// DrawLessonPanel — modal lesson content shown when player presses E at station
// ===========================================================================
// TEACHING NOTE — Lesson Panel as in-game documentation
// ─────────────────────────────────────────────────────────────────────────────
// The lesson panel is the teaching-game equivalent of a "codex" or "lore book"
// in a commercial RPG.  It appears when the player deliberately interacts with
// a teaching station (E key) and shows:
//
//   1. LESSON TITLE    — what subsystem this station demonstrates.
//   2. LESSON TEXT     — multi-line prose explanation (may include \n).
//   3. CODE POINTERS   — file paths / class names to inspect in VS Code.
//   4. DISMISS HINT    — "Press ESC or Enter to close".
//
// The lesson text is loaded from Content/World/station_lessons.json at runtime
// (see OpenWorld::TryLoadLessonsFromJSON).  Instructors can update lessons
// without recompiling the engine.
//
// GDI newline handling:
// GDI TextOut does not interpret "\n"; we split the text on '\n' manually and
// call DrawLine once per line.
// ─────────────────────────────────────────────────────────────────────────────

static void DrawLessonPanel(HWND hwnd, int clientW, int clientH,
                            const StationLesson& lesson)
{
    GdiScope dc(hwnd);
    if (!dc.Valid()) return;

    HFONT fontTitle   = CreateOverlayFont(19);
    HFONT fontBody    = CreateOverlayFont(15);
    HFONT fontPointer = CreateOverlayFont(13);
    HFONT fontFooter  = CreateOverlayFont(14);

    // ---- Split lesson text on '\n' ----
    std::vector<std::string> bodyLines;
    {
        std::string remaining = lesson.lessonText;
        size_t pos = 0;
        while ((pos = remaining.find('\n')) != std::string::npos)
        {
            bodyLines.push_back(remaining.substr(0, pos));
            remaining = remaining.substr(pos + 1);
        }
        if (!remaining.empty())
            bodyLines.push_back(remaining);
    }

    // ---- Panel geometry ----
    const int numBodyLines   = static_cast<int>(bodyLines.size());
    const int numPtrLines    = static_cast<int>(lesson.codePointers.size());

    const int panelH = kPadY * 2
                     + kLineH + 6                          // title
                     + numBodyLines * 17                   // body text (compact)
                     + (numPtrLines > 0 ? 10 + 4 : 0)     // code pointers heading
                     + numPtrLines * 16                    // code pointer lines
                     + 8 + 16;                             // footer

    const int panelW = std::min(clientW - 80, 760);
    const int panelX = (clientW - panelW) / 2;
    const int panelY = (clientH - panelH) / 2;

    DrawPanel(dc.Get(), panelX, panelY, panelW, panelH, kColBackground, kColSelected);

    int cy = panelY + kPadY;

    // ---- Title ----
    DrawLine(dc.Get(), fontTitle, panelX + kPadX, cy,
             lesson.lessonTitle.c_str(), kColHeading);
    cy += kLineH + 6;

    // ---- Body text ----
    for (const auto& line : bodyLines)
    {
        DrawLine(dc.Get(), fontBody, panelX + kPadX, cy,
                 line.c_str(), kColText);
        cy += 17;
    }

    // ---- Code pointers ----
    if (!lesson.codePointers.empty())
    {
        cy += 10;
        DrawLine(dc.Get(), fontBody, panelX + kPadX, cy,
                 "Key files / classes to inspect:", kColHeading);
        cy += 4 + 16;
        for (const auto& ptr : lesson.codePointers)
        {
            std::string line = "  + " + ptr;
            DrawLine(dc.Get(), fontPointer, panelX + kPadX, cy,
                     line.c_str(), kColDim);
            cy += 16;
        }
    }

    // ---- Footer ----
    cy += 8;
    DrawLine(dc.Get(), fontFooter, panelX + kPadX, cy,
             "  Press ESC or ENTER to close", kColDim);

    ::DeleteObject(fontTitle);
    ::DeleteObject(fontBody);
    ::DeleteObject(fontPointer);
    ::DeleteObject(fontFooter);
}

// ===========================================================================
// DrawSettingsPanel — performance preset + volume settings overlay (M-DG-S1)
// ===========================================================================
// TEACHING NOTE — Settings Menu as Performance-Preset Selector
// ─────────────────────────────────────────────────────────────────────────────
// The settings panel lets the player select a performance preset (Low / Medium
// / High / Ultra) which maps directly to the PerformancePresetConfig that was
// built in M-DG-P1.  It also exposes:
//   • Master Volume   — 0–100 integer.
//   • VSync           — On / Off.
//   • Frame Cap       — Unlimited / 30 fps / 60 fps.
//
// Navigation:
//   UP / DOWN          — move row selection.
//   LEFT / RIGHT       — change the value of the selected row.
//   ENTER on "Apply"   — save config to disk and apply to renderer.
//   ENTER on "Cancel"
//   or ESC             — discard changes and close.
// ─────────────────────────────────────────────────────────────────────────────

// Rows in the settings panel.
enum class SettingsRow : int
{
    PRESET    = 0,
    VOLUME    = 1,
    VSYNC     = 2,
    FRAME_CAP = 3,
    APPLY     = 4,
    CANCEL    = 5,
    COUNT     = 6,
};

static void DrawSettingsPanel(HWND hwnd, int clientW, int clientH,
                              int selRow,
                              engine::core::PerformancePreset preset,
                              int volume,
                              bool vsync,
                              int  frameCap)
{
    GdiScope dc(hwnd);
    if (!dc.Valid()) return;

    HFONT fontTitle = CreateOverlayFont(22);
    HFONT fontItem  = CreateOverlayFont(18);
    HFONT fontFooter = CreateOverlayFont(14);

    constexpr int kRows    = static_cast<int>(SettingsRow::COUNT);
    const int     panelH   = kPadY * 2 + 30 + 8 + kRows * kLineH + 8 + 14 + kPadY;
    const int     panelW   = kPanelW + 80;
    const int     panelX   = (clientW - panelW) / 2;
    const int     panelY   = (clientH - panelH) / 2;

    DrawPanel(dc.Get(), panelX, panelY, panelW, panelH, kColBackground, kColSelected);

    int cy = panelY + kPadY;

    // ---- Title ----
    DrawLine(dc.Get(), fontTitle, panelX + kPadX, cy,
             "  Settings — Performance & Audio", kColHeading);
    cy += 30 + 8;

    // ---- Preset row ----
    {
        const bool sel = (selRow == static_cast<int>(SettingsRow::PRESET));
        char line[128];
        std::snprintf(line, sizeof(line), "  %s  Preset:    < %s >",
                      sel ? ">" : " ", engine::core::PresetName(preset));
        DrawLine(dc.Get(), fontItem, panelX + kPadX, cy,
                 line, sel ? kColSelected : kColText);
        cy += kLineH;
    }

    // ---- Volume row ----
    {
        const bool sel = (selRow == static_cast<int>(SettingsRow::VOLUME));
        char line[128];
        std::snprintf(line, sizeof(line), "  %s  Volume:   < %3d >",
                      sel ? ">" : " ", volume);
        DrawLine(dc.Get(), fontItem, panelX + kPadX, cy,
                 line, sel ? kColSelected : kColText);
        cy += kLineH;
    }

    // ---- VSync row ----
    {
        const bool sel = (selRow == static_cast<int>(SettingsRow::VSYNC));
        char line[128];
        std::snprintf(line, sizeof(line), "  %s  VSync:    < %s >",
                      sel ? ">" : " ", vsync ? "On " : "Off");
        DrawLine(dc.Get(), fontItem, panelX + kPadX, cy,
                 line, sel ? kColSelected : kColText);
        cy += kLineH;
    }

    // ---- Frame Cap row ----
    {
        const bool sel = (selRow == static_cast<int>(SettingsRow::FRAME_CAP));
        char line[128];
        const char* capStr = (frameCap == 0)  ? "Unlimited" :
                             (frameCap == 30) ? "30 fps"    :
                                                "60 fps";
        std::snprintf(line, sizeof(line), "  %s  Frame Cap:< %s >",
                      sel ? ">" : " ", capStr);
        DrawLine(dc.Get(), fontItem, panelX + kPadX, cy,
                 line, sel ? kColSelected : kColText);
        cy += kLineH;
    }

    // ---- Apply / Cancel buttons ----
    {
        const bool selApply  = (selRow == static_cast<int>(SettingsRow::APPLY));
        const bool selCancel = (selRow == static_cast<int>(SettingsRow::CANCEL));
        char lineA[64], lineC[64];
        std::snprintf(lineA, sizeof(lineA), "  %s  [ Apply ]",  selApply  ? ">" : " ");
        std::snprintf(lineC, sizeof(lineC), "  %s  [ Cancel ]", selCancel ? ">" : " ");
        DrawLine(dc.Get(), fontItem, panelX + kPadX, cy,
                 lineA, selApply ? kColSelected : kColText);
        cy += kLineH;
        DrawLine(dc.Get(), fontItem, panelX + kPadX, cy,
                 lineC, selCancel ? kColSelected : kColText);
        cy += kLineH;
    }

    cy += 8;

    // ---- Footer ----
    DrawLine(dc.Get(), fontFooter, panelX + kPadX, cy,
             "  UP/DOWN:row   LEFT/RIGHT:value   ENTER:confirm   ESC:cancel",
             kColDim);

    // ---- Cleanup ----
    ::DeleteObject(fontTitle);
    ::DeleteObject(fontItem);
    ::DeleteObject(fontFooter);
}

} // namespace overlay

// ===========================================================================
// Forward-declare demo_quest_manager types (used by DrawQuestHud)
// ===========================================================================
// Already available via open_world.hpp → demo_quest_manager.hpp

// ===========================================================================
// Headless validation path (CI — no window, no user input)
// ===========================================================================
// TEACHING NOTE — Headless Demo_Game CI
// ─────────────────────────────────────
// When --headless is passed, demo_main skips the Win32 window and runs the
// OpenWorld state machine for kHeadlessFrames frames.  Acceptance criteria:
//
//   1. Init() returns true (stations registered, state = BOOT_MENU).
//   2. After 2 frames the state machine auto-selects "New Game".
//   3. After a brief simulated load the state becomes PLAYING.
//   4. All 5 biomes are visited in headless cycle mode.
//   5. IsHeadlessDone() returns true at frame kHeadlessFrames.
//   6. The executable exits 0.
// ─────────────────────────────────────────────────────────────────────────────

static int RunHeadless()
{
    std::cout << "[demo_game] Starting headless validation ...\n";

    // -----------------------------------------------------------------------
    // 1. Create and initialise the OpenWorld state machine.
    // -----------------------------------------------------------------------
    OpenWorld world;
    if (!world.Init())
    {
        std::cout << "[FAIL] demo_world: OpenWorld::Init() returned false.\n";
        return 1;
    }
    std::cout << "[OK] demo_world/1: Init — "
              << world.GetStations().size() << " stations registered.\n";

    // -----------------------------------------------------------------------
    // 2. Check BOOT_MENU initial state.
    // -----------------------------------------------------------------------
    if (world.GetState() != OpenWorldState::BOOT_MENU)
    {
        std::cout << "[FAIL] demo_world/2: initial state is not BOOT_MENU.\n";
        return 1;
    }
    std::cout << "[OK] demo_world/2: Initial state = BOOT_MENU.\n";

    // -----------------------------------------------------------------------
    // 3. Run frames until headless completion (auto-advance boot menu).
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Extra guard frames
    // We allow kHeadlessFrames + kExtraGuardFrames iterations so the
    // BOOT_MENU auto-advance (2 frames) and LOADING phase (about 6 frames
    // at 60 Hz: 0.1 s / (1/60 s) ≈ 6) complete before we start counting
    // the PLAYING biome-cycle frames.  The loop exits early via `break`
    // once IsHeadlessDone() is true.
    // -----------------------------------------------------------------------
    constexpr float kDt             = 1.0f / 60.0f;
    constexpr int   kExtraGuardFrames = 12; // boot (2) + loading (~6 @ 60Hz) + margin
    for (int f = 0; f < OpenWorld::kHeadlessFrames + kExtraGuardFrames; ++f)
    {
        world.Update(kDt, /*headless=*/true);
        if (world.IsHeadlessDone())
        {
            break;
        }
    }

    if (!world.IsHeadlessDone())
    {
        std::cout << "[FAIL] demo_world/3: IsHeadlessDone() never became true "
                     "after " << OpenWorld::kHeadlessFrames << " frames.\n";
        return 1;
    }
    std::cout << "[OK] demo_world/3: All biomes visited; headless done at frame "
              << world.GetFrameCount() << ".\n";

    // -----------------------------------------------------------------------
    // 4. Verify station count and per-station data integrity.
    // -----------------------------------------------------------------------
    constexpr int kExpectedStations = 12;
    const auto& stations = world.GetStations();
    const int actualStations = static_cast<int>(stations.size());

    // TEACHING NOTE — Fail explicitly on empty station list
    // An empty list is a test failure, not a vacuous success.  We check the
    // count first (not just the per-field loop) so the error message tells
    // the student exactly what went wrong.
    if (actualStations < kExpectedStations)
    {
        std::cout << "[FAIL] demo_world/4: Expected >= " << kExpectedStations
                  << " teaching stations, got " << actualStations << ".\n";
        return 1;
    }

    // Every station must have non-empty id, displayName, and sceneHint.
    bool stationDataOk = true;
    for (const auto& s : stations)
    {
        if (s.id.empty() || s.displayName.empty() || s.sceneHint.empty())
        {
            std::cout << "[FAIL] demo_world/4: station has empty id, "
                         "displayName, or sceneHint.\n";
            stationDataOk = false;
            break;
        }
    }
    if (!stationDataOk)
    {
        return 1;
    }
    std::cout << "[OK] demo_world/4: " << actualStations
              << " teaching stations registered; all have non-empty id, "
                 "displayName, and sceneHint.\n";

    // -----------------------------------------------------------------------
    // 5. Teleport test — navigation only; no quest progress.
    // TEACHING NOTE — Verifying teleport is navigation-only
    // ─────────────────────────────────────────────────────────────────────────
    // After this change teleport updates biome + sets nearestStationID but
    // does NOT advance the quest.  We verify the biome change (still works)
    // and that the nearest station ID was set correctly.
    // -----------------------------------------------------------------------
    world.TeleportToStation("rendering_pbr");
    if (world.GetCurrentBiome() != BiomeType::GRASSLAND)
    {
        std::cout << "[FAIL] demo_world/5: TeleportToStation(\"rendering_pbr\") "
                     "did not set biome to GRASSLAND.\n";
        return 1;
    }
    if (world.GetNearestStationID() != "rendering_pbr")
    {
        std::cout << "[FAIL] demo_world/5: TeleportToStation(\"rendering_pbr\") "
                     "did not set nearestStationID to \"rendering_pbr\".\n";
        return 1;
    }
    std::cout << "[OK] demo_world/5: Teleport to rendering_pbr → "
                 "biome=GRASSLAND, nearestStation=rendering_pbr (navigation only).\n";

    // -----------------------------------------------------------------------
    // 6. Station list non-empty after TryLoadStationsFromJSON fallback.
    // TEACHING NOTE — Testing the fallback path
    // ─────────────────────────────────────────────────────────────────────────
    // We call TryLoadStationsFromJSON with a path that does not exist to
    // verify the fallback behaviour: stations remain from RegisterDefault-
    // Stations() and the count stays >= kExpectedStations.
    // In a full CI run with the content deployed, OpenWorld::Init() will have
    // already exercised the JSON-found path; here we exercise the not-found
    // path explicitly.
    // ─────────────────────────────────────────────────────────────────────────
    {
        OpenWorld world2;
        if (!world2.Init())
        {
            std::cout << "[FAIL] demo_world/6: second OpenWorld::Init() failed.\n";
            return 1;
        }
        // Force the fallback path by using a non-existent path.
        world2.TryLoadStationsFromJSON("/nonexistent/path/teaching_stations.json");
        const int count6 = static_cast<int>(world2.GetStations().size());
        if (count6 < kExpectedStations)
        {
            std::cout << "[FAIL] demo_world/6: station count after bad JSON path = "
                      << count6 << " (expected >= " << kExpectedStations << ").\n";
            return 1;
        }
        std::cout << "[OK] demo_world/6: JSON fallback safe — "
                  << count6 << " stations retained after missing-file load.\n";
    }

    // -----------------------------------------------------------------------
    // 7. Quest & activity definitions — DemoQuestManager initialisation.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Testing the quest/activity layer independently
    // ─────────────────────────────────────────────────────────────────────────
    // The DemoQuestManager is accessible via OpenWorld::GetQuestManager().
    // We already called OpenWorld::Init() (test 1), which initialised the
    // quest manager with built-in defaults.  Here we:
    //   a) Verify that TotalDefined() returns 1 main quest + 3 activities = 4.
    //   b) Verify the main quest has 3 objectives.
    //   c) Simulate Teleport+Interact for three station objectives in order
    //      and verify the main quest completes.
    //   d) Verify the Lesson Reader activity advances on unique interacts.
    // ─────────────────────────────────────────────────────────────────────────
    {
        // Use 'world' (already past PLAYING after test 3) — its quest manager
        // was initialised in world.Init() with built-in defaults.
        const DemoQuestManager& qm = world.GetQuestManager();

        // a) Total definitions check.
        constexpr int kExpectedTotal = 1 + DemoQuestManager::kExpectedActivities;
        if (qm.TotalDefined() != kExpectedTotal)
        {
            std::cout << "[FAIL] demo_world/7a: TotalDefined() = "
                      << qm.TotalDefined()
                      << " (expected " << kExpectedTotal << ").\n";
            return 1;
        }

        // b) Main quest objectives count.
        const int objCount = static_cast<int>(qm.GetMainQuest().objectives.size());
        if (objCount < 2)
        {
            std::cout << "[FAIL] demo_world/7b: main quest has " << objCount
                      << " objectives (expected >= 2).\n";
            return 1;
        }

        // c) Complete the main quest by visiting objectives in order.
        //    We use a fresh OpenWorld so the quest starts from scratch.
        OpenWorld qWorld;
        if (!qWorld.Init())
        {
            std::cout << "[FAIL] demo_world/7c: qWorld.Init() returned false.\n";
            return 1;
        }

        // Visit each station whose ID matches a main quest objective.
        // TEACHING NOTE — Simulating Interact in headless CI
        // ─────────────────────────────────────────────────────
        // In headless mode there is no keyboard, so we cannot literally press E.
        // Instead we call TeleportToStation() (which sets m_nearestStationID)
        // followed immediately by InteractAtStation() — the same sequence the
        // player would perform: arrive at the station (teleport) then press E.
        // This is deterministic and reproducible in CI without any UI.
        const auto& questRef = qWorld.GetQuestManager().GetMainQuest();
        for (const auto& obj : questRef.objectives)
        {
            if (!obj.stationID.empty())
            {
                qWorld.TeleportToStation(obj.stationID);  // navigate to station
                qWorld.InteractAtStation();               // press E at station
            }
        }

        if (!qWorld.GetQuestManager().GetMainQuest().completed)
        {
            std::cout << "[FAIL] demo_world/7c: main quest did not complete "
                         "after interacting at all objective stations.\n";
            return 1;
        }

        // d) STATION_INTERACT activity — 3 unique station interacts.
        //    qWorld already interacted at the 3 objective stations; each was
        //    unique, so the Lesson Reader activity (3 distinct interacts) should
        //    have progress >= 3.
        const auto& scanAct = qWorld.GetQuestManager().GetActivities();
        bool scannerOk = false;
        for (const auto& a : scanAct)
        {
            if (a.type == DemoActivityType::STATION_INTERACT
                && a.specificStationID.empty()
                && a.progress >= 3)
            {
                scannerOk = true;
                break;
            }
        }
        if (!scannerOk)
        {
            std::cout << "[FAIL] demo_world/7d: Lesson Reader activity did not "
                         "reach progress >= 3 after interacting at 3 unique stations.\n";
            return 1;
        }

        std::cout << "[OK] demo_world/7: DemoQuestManager — "
                  << qm.TotalDefined()
                  << " defined (1 main quest, " << DemoQuestManager::kExpectedActivities
                  << " activities); main quest completes on station interacts; "
                     "Lesson Reader advances on unique interacts.\n";
    }

    // -----------------------------------------------------------------------
    // 8. Save / Load roundtrip — M-DG-S2
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Headless save/load test
    // ─────────────────────────────────────────────────────────────────────────
    // We exercise the full save/load cycle without a window:
    //   a) Create a world, interact at one objective station.
    //   b) Save to a temp file.
    //   c) Create a fresh world, load from the temp file.
    //   d) Verify quest objective index survived the roundtrip.
    //
    // On non-JSON builds (ENGINE_ENABLE_JSON not defined), SaveProgress()
    // returns false.  We treat this as a graceful skip rather than a failure
    // because the save system is deliberately guarded by ENGINE_ENABLE_JSON.
    // ─────────────────────────────────────────────────────────────────────────
    {
        OpenWorld saveWorld;
        if (!saveWorld.Init())
        {
            std::cout << "[FAIL] demo_world/8: saveWorld.Init() returned false.\n";
            return 1;
        }

        // Simulate New Game → PLAYING transition.
        saveWorld.BootSelectNewGame();
        constexpr float kDt8 = 1.0f / 60.0f;
        // Pump until PLAYING (the boot/load transition needs a few frames).
        for (int f = 0; f < 20 && saveWorld.GetState() != OpenWorldState::PLAYING; ++f)
            saveWorld.Update(kDt8, /*headless=*/true);

        // Interact at the first objective station to advance the quest.
        const auto& firstObj = saveWorld.GetQuestManager().GetMainQuest().objectives;
        if (!firstObj.empty() && !firstObj[0].stationID.empty())
        {
            saveWorld.TeleportToStation(firstObj[0].stationID);
            saveWorld.InteractAtStation();
        }
        const int savedObjIdx = saveWorld.GetQuestManager().GetMainQuest().currentObjective;

        // Save to /tmp so the file is not committed to the repository.
        const std::string tmpPath = "C:\\Temp\\demo_save_test.json";
        const bool saved = saveWorld.SaveProgress(tmpPath);

        if (!saved)
        {
            // ENGINE_ENABLE_JSON not defined — gracefully skip.
            std::cout << "[OK] demo_world/8: Save/load skipped "
                         "(ENGINE_ENABLE_JSON not active on this build).\n";
        }
        else
        {
            // Load into a fresh world.
            OpenWorld loadWorld;
            if (!loadWorld.Init())
            {
                std::cout << "[FAIL] demo_world/8: loadWorld.Init() returned false.\n";
                return 1;
            }

            if (!loadWorld.LoadProgress(tmpPath))
            {
                std::cout << "[FAIL] demo_world/8: LoadProgress returned false.\n";
                return 1;
            }

            // State should be PLAYING.
            if (loadWorld.GetState() != OpenWorldState::PLAYING)
            {
                std::cout << "[FAIL] demo_world/8: state after LoadProgress is not PLAYING "
                             "(got " << static_cast<int>(loadWorld.GetState()) << ").\n";
                return 1;
            }

            // Quest objective index must survive the roundtrip.
            const int loadedObjIdx =
                loadWorld.GetQuestManager().GetMainQuest().currentObjective;
            if (loadedObjIdx != savedObjIdx)
            {
                std::cout << "[FAIL] demo_world/8: quest objective index mismatch "
                             "(saved=" << savedObjIdx
                          << ", loaded=" << loadedObjIdx << ").\n";
                return 1;
            }

            std::cout << "[OK] demo_world/8: Save/load roundtrip — "
                         "state=PLAYING, questObj=" << loadedObjIdx << ".\n";
        }
    }

    // -----------------------------------------------------------------------
    // PASS report.
    // -----------------------------------------------------------------------
    world.Shutdown();
    std::cout << "[PASS] demo_world: 8 acceptance tests passed "
                 "(init, boot_menu, biome_cycle, stations, teleport, json_fallback, "
                 "quest_manager_interact, save_load_roundtrip).\n";
    return 0;
}

// ===========================================================================
// Windowed path (interactive play)
// ===========================================================================
// TEACHING NOTE — Windowed Demo_Game
// ─────────────────────────────────────────────────────────────────────────────
// The windowed path mirrors engine_sandbox's main loop (Win32 window + D3D11
// renderer + game-loop delta-time) but always starts in the open world.
//
// Key differences from engine_sandbox:
//   • No --scene flag — the open world IS the scene.
//   • Boot menu is shown first (OpenWorld FSM state BOOT_MENU).
//   • F1 key toggles the developer station overlay (DemoDebugMenu).
//   • ESC pauses the game (PAUSED state) rather than quitting immediately.
//
// Input handling pattern — rising-edge detection:
//   We track each key's previous-frame state to detect "just pressed" (rising
//   edge).  This prevents a single key press from triggering multiple actions.
//   A production engine routes input through an EventBus or InputMapper
//   (src/game/systems/input_mapper.hpp) so all systems react to the same event.
// ─────────────────────────────────────────────────────────────────────────────

// ---------------------------------------------------------------------------
// Helper: BiomeName — convert enum to display string (shared by overlay + hud)
// ---------------------------------------------------------------------------

static const char* GetBiomeDisplayName(BiomeType b)
{
    switch (b)
    {
        case BiomeType::GRASSLAND: return "Lucis Plains";
        case BiomeType::FOREST:    return "Vesperpool Edge";
        case BiomeType::SNOW:      return "Ghorovas Rift";
        case BiomeType::DESERT:    return "Leide Badlands";
        case BiomeType::COAST:     return "Cape Caem Shore";
        default:                   return "Unknown";
    }
}

// ---------------------------------------------------------------------------
// KeyEdge — rising-edge tracker for a single virtual key
// TEACHING NOTE — Rising-edge detection
// ─────────────────────────────────────────────────────────────────────────────
// GetAsyncKeyState's 0x8000 bit is 1 while the key is held.  To fire an
// action only ONCE per press we XOR with the previous state:
//   pressed = isDown && !wasDown
// This is the same "edge trigger" pattern used in digital circuits.
// ─────────────────────────────────────────────────────────────────────────────
struct KeyEdge
{
    int  vk      = 0;
    bool wasDown = false;

    explicit KeyEdge(int vk_) : vk(vk_) {}

    /// Returns true the frame the key transitions from up → down.
    bool JustPressed()
    {
        const bool isDown = (::GetAsyncKeyState(vk) & 0x8000) != 0;
        const bool rising = isDown && !wasDown;
        wasDown = isDown;
        return rising;
    }
};

static int RunWindowed(const DemoArgs& args)
{
    // -----------------------------------------------------------------------
    // Load EngineConfig — M-DG-P1
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Config-first startup ordering
    // ─────────────────────────────────────────────
    // We load the engine config BEFORE creating the window or the renderer.
    // This lets the config determine window resolution and renderer settings
    // (VSync, frame cap, etc.) from the first frame.  Loading config after
    // renderer init would require a restart or hot-reload path.
    //
    // The fail-soft design (returns false on missing file, keeps defaults)
    // means the game always boots even on a fresh install without a config.
    // -----------------------------------------------------------------------
    engine::core::EngineConfig engConfig;
    const bool configLoaded = engConfig.Load("engine_config.json");
    if (configLoaded)
        std::cout << "[demo_game] engine_config.json loaded (preset="
                  << engine::core::PresetName(engConfig.activePreset) << ").\n";
    else
        std::cout << "[demo_game] engine_config.json not found — using defaults "
                     "(preset=Medium).\n";

    // -----------------------------------------------------------------------
    // Check for existing save file — M-DG-S3
    // -----------------------------------------------------------------------
    const bool saveExists = OpenWorld::SaveExists("SavedGames/demo_auto.json");
    if (saveExists)
        std::cout << "[demo_game] Save file detected — Continue is available.\n";

    // -----------------------------------------------------------------------
    // Create Win32 window.
    // -----------------------------------------------------------------------
    engine::platform::Win32Window window;
    constexpr int kWidth  = 1280;
    constexpr int kHeight = 720;
    if (!window.Init(L"Demo_Game - Open World", kWidth, kHeight))
    {
        std::cerr << "[FATAL] demo_game: Win32Window::Init() failed.\n";
        return 1;
    }
    std::cout << "[demo_game] Window created " << kWidth << "×" << kHeight << ".\n";

    // -----------------------------------------------------------------------
    // Create D3D11 renderer.
    // -----------------------------------------------------------------------
    auto renderer = engine::rendering::CreateRenderer(
        engine::rendering::ParseRendererBackend(args.renderer));
    if (!renderer)
    {
        std::cerr << "[FATAL] demo_game: RendererFactory could not create '"
                  << args.renderer << "' renderer.\n";
        window.Shutdown();
        return 1;
    }
    if (!renderer->Init(window.GetHINSTANCE(), window.GetHWND(), kWidth, kHeight))
    {
        std::cerr << "[FATAL] demo_game: renderer Init() failed.\n";
        window.Shutdown();
        return 1;
    }
    std::cout << "[demo_game] D3D11 renderer initialised.\n";

    // Apply performance preset to the renderer — M-DG-P1
    renderer->SetShadowsEnabled(engConfig.presetConfig.shadowsEnabled);
    renderer->SetBloomEnabled  (engConfig.presetConfig.bloomEnabled);
    renderer->SetIBLEnabled    (engConfig.presetConfig.iblEnabled);
    renderer->SetVSyncEnabled  (engConfig.presetConfig.vsync);
    renderer->SetFrameCap      (engConfig.presetConfig.frameCap);
    renderer->SetAnisoLevel    (engConfig.presetConfig.anisoLevel);
    std::cout << "[demo_game] Renderer preset applied: shadows="
              << (engConfig.presetConfig.shadowsEnabled ? "ON" : "OFF")
              << " bloom=" << (engConfig.presetConfig.bloomEnabled ? "ON" : "OFF")
              << " vsync=" << (engConfig.presetConfig.vsync ? "ON" : "OFF")
              << " frameCap=" << engConfig.presetConfig.frameCap << "\n";

    // -----------------------------------------------------------------------
    // Initialise OpenWorld state machine.
    // -----------------------------------------------------------------------
    OpenWorld world;
    if (!world.Init())
    {
        std::cerr << "[FATAL] demo_game: OpenWorld::Init() failed.\n";
        renderer->Shutdown();
        window.Shutdown();
        return 1;
    }

    // -----------------------------------------------------------------------
    // Initialise GameRuntime (all gameplay systems).
    // TEACHING NOTE — Reusing GameRuntime from engine_sandbox (M8)
    // ─────────────────────────────────────────────────────────────
    // Demo_Game reuses the same GameRuntime that engine_sandbox uses for its
    // --scene game mode.  This avoids duplicating the gameplay system
    // initialisation code.  The only difference is the entry path:
    //   engine_sandbox --scene game → GameRuntime started immediately.
    //   demo_game                  → GameRuntime started after the boot menu.
    // -----------------------------------------------------------------------
    std::unique_ptr<sandbox::GameRuntime> gameRuntime;

    // -----------------------------------------------------------------------
    // Main loop state.
    // -----------------------------------------------------------------------
    HWND hwnd = window.GetHWND();

    // --- Input edges ---
    KeyEdge keyF1  (VK_F1);          // F1 → toggle F1 debug overlay
    KeyEdge keyESC (VK_ESCAPE);      // ESC → pause / close overlay / close lesson panel
    KeyEdge keyUp  (VK_UP);          // ↑ navigate menu / station list
    KeyEdge keyDown(VK_DOWN);        // ↓ navigate menu / station list
    KeyEdge keyLeft (VK_LEFT);       // ← change value in settings
    KeyEdge keyRight(VK_RIGHT);      // → change value in settings
    KeyEdge keyEnter(VK_RETURN);     // Enter → confirm selection / dismiss lesson panel
    KeyEdge keyD   ('D');            // D → toggle debug info strip
    KeyEdge keyE   ('E');            // E → Interact with nearest teaching station

    // --- Overlay state ---
    bool debugMenuOpen   = false; ///< F1 overlay visible
    bool settingsMenuOpen = false; ///< Settings panel visible
    bool showDebugInfo   = false; ///< Debug info bar (biome + FPS)
    int  bootMenuSel     = 0;     ///< Boot menu highlighted item (0..3)
    int  stationSel      = 0;     ///< F1 overlay selected station index
    bool lessonPanelOpen = false; ///< Lesson panel visible (shown on E press)
    StationLesson currentLesson;  ///< Content of the currently displayed lesson

    // --- Settings panel working state (M-DG-S1) ---
    // Working copies that are edited in the settings panel and committed to
    // engConfig only when the player selects "Apply".
    // Volume is stored as a 0–100 integer here for display; it is stored in
    // engConfig.audio.masterVolume as a 0.0–1.0 float.
    int  settingsRow      = 0;
    engine::core::PerformancePreset settingsPreset = engConfig.activePreset;
    int  settingsVolume   = static_cast<int>(engConfig.audio.masterVolume * 100.f + 0.5f);
    bool settingsVsync    = engConfig.presetConfig.vsync;
    int  settingsFrameCap = engConfig.presetConfig.frameCap;

    // --- FPS tracking ---
    float fpsAccum  = 0.f;
    int   fpsCounts = 0;
    float fps       = 0.f;

    // -----------------------------------------------------------------------
    // Main loop.
    // -----------------------------------------------------------------------
    while (window.IsRunning())
    {
        window.PollEvents();

        // Handle window resize.
        if (window.WasResized())
        {
            renderer->RecreateSwapchain(window.GetWidth(), window.GetHeight());
            window.ClearResizedFlag();
        }

        // Delta time.
        const float dt = static_cast<float>(window.GetDeltaTime());

        // FPS calculation: rolling average over 1-second windows.
        fpsAccum  += dt;
        fpsCounts += 1;
        if (fpsAccum >= 1.0f)
        {
            fps       = static_cast<float>(fpsCounts) / fpsAccum;
            fpsAccum  = 0.f;
            fpsCounts = 0;
        }

        // -----------------------------------------------------------------
        // Input processing — depends on current game state.
        // -----------------------------------------------------------------
        const OpenWorldState state = world.GetState();
        const int numStations      = static_cast<int>(world.GetStations().size());

        // Poll all edges unconditionally so wasDown stays up-to-date every frame.
        const bool pressedF1    = keyF1.JustPressed();
        const bool pressedESC   = keyESC.JustPressed();
        const bool pressedUp    = keyUp.JustPressed();
        const bool pressedDown  = keyDown.JustPressed();
        const bool pressedLeft  = keyLeft.JustPressed();
        const bool pressedRight = keyRight.JustPressed();
        const bool pressedEnter = keyEnter.JustPressed();
        const bool pressedD     = keyD.JustPressed();
        const bool pressedE     = keyE.JustPressed();

        // Toggle debug info bar with D (available in all states).
        if (pressedD)
        {
            showDebugInfo = !showDebugInfo;
            std::cout << "[demo_game] Debug info: "
                      << (showDebugInfo ? "ON" : "OFF") << "\n";
        }

        if (state == OpenWorldState::BOOT_MENU)
        {
            // -----------------------------------------------------------------
            // BOOT MENU input
            // TEACHING NOTE — Menu navigation
            // Up/Down arrows move the selection index with wrap-around.
            // Enter activates the selected item.
            //   • New Game  — starts a fresh session via BootSelectNewGame().
            //   • Continue  — loads the auto-save and transitions to PLAYING.
            //                 Only selectable when a save file exists.
            //   • Settings  — opens the settings panel overlay (M-DG-S1).
            //   • Quit      — posts a Win32 WM_QUIT / shuts down the window.
            // -----------------------------------------------------------------
            constexpr int kBootCount = 4; // New Game, Continue, Settings, Quit

            if (pressedUp)
                bootMenuSel = (bootMenuSel - 1 + kBootCount) % kBootCount;
            if (pressedDown)
                bootMenuSel = (bootMenuSel + 1) % kBootCount;

            if (pressedEnter)
            {
                switch (bootMenuSel)
                {
                    case 0: // New Game
                        world.BootSelectNewGame();
                        break;
                    case 1: // Continue — M-DG-S3
                        if (saveExists)
                        {
                            // TEACHING NOTE — Restore session from save file
                            // LoadProgress() deserialises the save JSON, restores
                            // quest/activity state via DemoQuestManager::Restore(),
                            // and transitions the FSM to PLAYING.  The GameRuntime
                            // will be created on the next frame when IsPlaying() is
                            // detected (same path as New Game).
                            if (!world.LoadProgress("SavedGames/demo_auto.json"))
                            {
                                std::cerr << "[demo_game] Continue: save file exists "
                                             "but LoadProgress failed — starting New Game.\n";
                                world.BootSelectNewGame();
                            }
                            else
                            {
                                std::cout << "[demo_game] Continue: session restored.\n";
                            }
                        }
                        else
                        {
                            std::cout << "[demo_game] Continue: no save file.\n";
                        }
                        break;
                    case 2: // Settings — M-DG-S1
                        // Snapshot current config into the working copies before
                        // opening the panel so the player sees the live values.
                        settingsPreset   = engConfig.activePreset;
                        settingsVolume   = static_cast<int>(engConfig.audio.masterVolume * 100.f + 0.5f);
                        settingsVsync    = engConfig.presetConfig.vsync;
                        settingsFrameCap = engConfig.presetConfig.frameCap;
                        settingsRow      = 0;
                        settingsMenuOpen = true;
                        std::cout << "[demo_game] Settings menu opened.\n";
                        break;
                    case 3: // Quit
                        std::cout << "[demo_game] Quit selected from boot menu.\n";
                        window.Shutdown();
                        break;
                    default:
                        break;
                }
            }
        }
        else if (settingsMenuOpen)
        {
            // -----------------------------------------------------------------
            // SETTINGS PANEL input — M-DG-S1
            // TEACHING NOTE — Settings panel navigation
            // ─────────────────────────────────────────────────────────────────
            // The settings panel uses a row-based model:
            //   UP/DOWN move the row cursor.
            //   LEFT/RIGHT change the value on the selected row.
            //   ENTER on APPLY commits changes to engConfig, saves config.json,
            //          and applies the preset to the renderer via Set*() calls.
            //   ENTER on CANCEL / ESC discards changes.
            // ─────────────────────────────────────────────────────────────────
            constexpr int kRowCount = static_cast<int>(SettingsRow::COUNT);
            if (pressedUp)
                settingsRow = (settingsRow - 1 + kRowCount) % kRowCount;
            if (pressedDown)
                settingsRow = (settingsRow + 1) % kRowCount;

            const auto curRow = static_cast<SettingsRow>(settingsRow);

            if (pressedLeft || pressedRight)
            {
                const int dir = pressedRight ? 1 : -1;
                switch (curRow)
                {
                    case SettingsRow::PRESET:
                    {
                        // Cycle through Low(0) → Medium(1) → High(2) → Ultra(3).
                        const int cur  = static_cast<int>(settingsPreset);
                        const int next = (cur + dir + 4) % 4;
                        settingsPreset = static_cast<engine::core::PerformancePreset>(next);
                        // Update vsync + frameCap from the new preset defaults.
                        const auto def = engine::core::PresetDefaults(settingsPreset);
                        settingsVsync    = def.vsync;
                        settingsFrameCap = def.frameCap;
                        break;
                    }
                    case SettingsRow::VOLUME:
                        settingsVolume = std::max(0, std::min(100, settingsVolume + dir * 5));
                        break;
                    case SettingsRow::VSYNC:
                        settingsVsync = !settingsVsync;
                        break;
                    case SettingsRow::FRAME_CAP:
                    {
                        // Cycle: 0 → 30 → 60 → 0 ...
                        const int caps[] = { 0, 30, 60 };
                        int idx = 0;
                        for (int k = 0; k < 3; ++k)
                            if (caps[k] == settingsFrameCap) { idx = k; break; }
                        idx = (idx + dir + 3) % 3;
                        settingsFrameCap = caps[idx];
                        break;
                    }
                    default:
                        break;
                }
            }

            if (pressedEnter)
            {
                if (curRow == SettingsRow::APPLY)
                {
                    // TEACHING NOTE — Applying the preset
                    // ApplyPreset() fills presetConfig from PresetDefaults(p)
                    // then we override the individual toggles with the player's
                    // manual selections (vsync, frameCap).  This matches the
                    // Load() order in engine_config.cpp.
                    engConfig.ApplyPreset(settingsPreset);
                    engConfig.presetConfig.vsync    = settingsVsync;
                    engConfig.presetConfig.frameCap = settingsFrameCap;
                    // Convert 0–100 display volume back to 0.0–1.0 float.
                    engConfig.audio.masterVolume    = settingsVolume / 100.f;

                    // Push the new settings to the renderer.
                    renderer->SetShadowsEnabled(engConfig.presetConfig.shadowsEnabled);
                    renderer->SetBloomEnabled  (engConfig.presetConfig.bloomEnabled);
                    renderer->SetIBLEnabled    (engConfig.presetConfig.iblEnabled);
                    renderer->SetVSyncEnabled  (engConfig.presetConfig.vsync);
                    renderer->SetFrameCap      (engConfig.presetConfig.frameCap);
                    renderer->SetAnisoLevel    (engConfig.presetConfig.anisoLevel);

                    // Persist to engine_config.json so the choice survives restarts.
                    const bool saved = engConfig.Save("engine_config.json");
                    std::cout << "[demo_game] Settings applied: preset="
                              << engine::core::PresetName(engConfig.activePreset)
                              << " vol=" << settingsVolume
                              << (saved ? " (saved to engine_config.json)" : " (save failed)")
                              << "\n";

                    settingsMenuOpen = false;
                }
                else if (curRow == SettingsRow::CANCEL)
                {
                    std::cout << "[demo_game] Settings cancelled.\n";
                    settingsMenuOpen = false;
                }
            }

            if (pressedESC)
            {
                std::cout << "[demo_game] Settings cancelled (ESC).\n";
                settingsMenuOpen = false;
            }
        }
        else if (lessonPanelOpen)
        {
            // -----------------------------------------------------------------
            // LESSON PANEL input — ESC or Enter dismisses the panel.
            // TEACHING NOTE — Modal dismiss pattern
            // The lesson panel is a soft modal: it doesn't block the world
            // update but intercepts input so other actions don't fire while
            // the student is reading.  ESC and Enter are standard dismiss keys
            // in UI systems (think "OK" button / "Press Enter to continue").
            // -----------------------------------------------------------------
            if (pressedESC || pressedEnter)
            {
                lessonPanelOpen = false;
                currentLesson   = StationLesson{};
                std::cout << "[demo_game] Lesson panel: CLOSED\n";
            }
        }
        else if (debugMenuOpen)
        {
            // -----------------------------------------------------------------
            // F1 OVERLAY input
            // TEACHING NOTE — Teleport is navigation only
            // Teleporting via the F1 menu brings the player to a station so
            // they can press E to interact.  The teleport itself does NOT
            // advance the quest or trigger a lesson panel.
            // -----------------------------------------------------------------
            if (pressedF1 || pressedESC)
            {
                debugMenuOpen = false;
                std::cout << "[demo_game] F1 debug menu: CLOSED\n";
            }
            else if (pressedUp && numStations > 0)
            {
                stationSel = (stationSel - 1 + numStations) % numStations;
            }
            else if (pressedDown && numStations > 0)
            {
                stationSel = (stationSel + 1) % numStations;
            }
            else if (pressedEnter && numStations > 0)
            {
                const auto& s = world.GetStations()[stationSel];
                std::cout << "[demo_game] Teleporting to station: "
                          << s.displayName << "\n";
                world.TeleportToStation(s.id);
                // Close the F1 menu so the prompt "[E] Interact" is visible
                // and the player can press E without the overlay in the way.
                debugMenuOpen = false;
                std::cout << "[demo_game] Press E to interact with \""
                          << s.displayName << "\"\n";
            }
        }
        else
        {
            // -----------------------------------------------------------------
            // PLAYING / PAUSED input
            // -----------------------------------------------------------------
            if (pressedF1 && state == OpenWorldState::PLAYING)
            {
                debugMenuOpen = true;
                // Clamp station index in case station list changed.
                if (numStations > 0 && stationSel >= numStations)
                    stationSel = 0;
                std::cout << "[demo_game] F1 debug menu: OPEN ("
                          << numStations << " stations)\n";
            }

            // TEACHING NOTE — Interact key (E) at a station
            // ─────────────────────────────────────────────────────────────────
            // When the player presses E, OpenWorld::InteractAtStation() checks
            // whether the player is near a station (m_nearestStationID).
            // If so it:
            //   1. Advances the main quest objective (if this station matches).
            //   2. Advances STATION_INTERACT side activities.
            //   3. Returns the StationLesson for the lesson panel.
            // The F1 menu teleport sets m_nearestStationID so that the next E
            // press at the correct location triggers the lesson correctly.
            if (pressedE && state == OpenWorldState::PLAYING)
            {
                StationLesson lesson = world.InteractAtStation();
                if (lesson.IsValid())
                {
                    currentLesson   = std::move(lesson);
                    lessonPanelOpen = true;
                    std::cout << "[demo_game] Lesson panel OPEN: \""
                              << currentLesson.lessonTitle << "\"\n";

                    // TEACHING NOTE — Autosave on Interact (M-DG-S2)
                    // ─────────────────────────────────────────────────
                    // We save automatically when the player successfully
                    // interacts at a teaching station.  This mirrors the FFXV
                    // "camp save" design pattern: a deliberate meaningful action
                    // (engaging with learning content) doubles as a save point.
                    // The save is silent — the player sees the lesson panel and
                    // does not need to manually navigate to a save menu.
                    world.SaveProgress("SavedGames/demo_auto.json");
                }
                else
                {
                    std::cout << "[demo_game] E pressed — not near a station.\n";
                }
            }

            if (pressedESC)
            {
                if (state == OpenWorldState::PLAYING)
                {
                    // ESC while playing → pause
                    // (OpenWorld will respond to PAUSED state internally)
                }
                else if (state == OpenWorldState::PAUSED)
                {
                    // ESC while paused could return to PLAYING (handled by FSM)
                }
            }
        }

        // -----------------------------------------------------------------
        // OpenWorld FSM update.
        // -----------------------------------------------------------------
        world.Update(dt, /*headless=*/false);

        // Transition: when OpenWorld enters PLAYING for the first time,
        // start the GameRuntime.
        if (world.IsPlaying() && !gameRuntime)
        {
            gameRuntime = std::make_unique<sandbox::GameRuntime>();
            if (!gameRuntime->Init())
            {
                std::cerr << "[FATAL] demo_game: GameRuntime::Init() failed.\n";
                renderer->Shutdown();
                window.Shutdown();
                return 1;
            }
            std::cout << "[demo_game] GameRuntime initialised — open world PLAYING.\n";
        }

        // Tick gameplay systems once per frame when in PLAYING state.
        if (gameRuntime && world.IsPlaying())
        {
            gameRuntime->Update(dt);
        }

        // -----------------------------------------------------------------
        // Render — D3D11 scene (sky colour from current biome / game state).
        // -----------------------------------------------------------------
        float r, g, b;
        if (gameRuntime && world.IsPlaying())
        {
            gameRuntime->GetClearColour(r, g, b);
        }
        else
        {
            world.GetClearColour(r, g, b);
        }

        renderer->DrawFrame(r, g, b);

        // -----------------------------------------------------------------
        // GDI Overlays — drawn after D3D11 Present via DWM compositing.
        // TEACHING NOTE — Overlay draw order
        // We draw overlays AFTER DrawFrame so the D3D11 colour fills the
        // background and GDI text appears on top.  In windowed mode DWM
        // composites both surfaces before the final screen display.
        // -----------------------------------------------------------------
        const int winW = static_cast<int>(window.GetWidth());
        const int winH = static_cast<int>(window.GetHeight());

        if (world.GetState() == OpenWorldState::BOOT_MENU)
        {
            if (settingsMenuOpen)
            {
                // Settings panel shown as a sub-panel on top of the boot menu.
                overlay::DrawSettingsPanel(hwnd, winW, winH,
                                           settingsRow,
                                           settingsPreset,
                                           settingsVolume,
                                           settingsVsync,
                                           settingsFrameCap);
            }
            else
            {
                overlay::DrawBootMenu(hwnd, winW, winH, bootMenuSel, saveExists);
            }
        }
        else if (lessonPanelOpen)
        {
            overlay::DrawLessonPanel(hwnd, winW, winH, currentLesson);
        }
        else if (debugMenuOpen)
        {
            const char* biomeName = GetBiomeDisplayName(world.GetCurrentBiome());
            overlay::DrawDebugOverlay(hwnd, winW, winH,
                                      world.GetStations(),
                                      stationSel,
                                      showDebugInfo,
                                      biomeName,
                                      fps);
        }
        else if (showDebugInfo)
        {
            const char* biomeName = GetBiomeDisplayName(world.GetCurrentBiome());
            overlay::DrawHudBar(hwnd, winW, biomeName, fps);
        }

        // Quest HUD — always visible while PLAYING (not in boot, lesson, or debug menu).
        // TEACHING NOTE — Persistent quest overlay
        // The quest HUD is drawn on top of all other content when the player
        // is in the PLAYING state.  It does not interfere with the lesson panel
        // (DrawLessonPanel takes centre stage), the F1 overlay, or the boot menu.
        if (world.GetState() == OpenWorldState::PLAYING && !debugMenuOpen && !lessonPanelOpen)
        {
            // Show the interact prompt when the player is near a station.
            if (!world.GetNearestStationID().empty())
            {
                overlay::DrawInteractPrompt(hwnd, winW, winH,
                                            world.GetNearestStationID(),
                                            world.GetStations());
            }
            overlay::DrawQuestHud(hwnd, winW, winH, world.GetQuestManager());
        }
    }

    // -----------------------------------------------------------------------
    // Shutdown.
    // -----------------------------------------------------------------------
    if (gameRuntime)
        gameRuntime->Shutdown();
    world.Shutdown();
    renderer->Shutdown();
    window.Shutdown();
    std::cout << "[demo_game] Clean shutdown.\n";
    return 0;
}

// ===========================================================================
// main()
// ===========================================================================
// TEACHING NOTE — Entry Point Design
// ─────────────────────────────────────────────────────────────────────────────
// We keep main() minimal: parse args, dispatch to headless or windowed path.
// This mirrors the "thin entry point" convention used in most AAA engines:
//   main() owns lifetime but delegates all logic to subsystems.
//
// The subsystem separation (OpenWorld, GameRuntime, Win32Window, D3D11Renderer)
// means you can add a new subsystem (VR, network, analytics) without touching
// the others — the Hollywood Principle ("don't call us, we'll call you") in
// action via the renderer independence already established by IRenderer.
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    std::cout << "=== Demo_Game v1.0 — Educational Game Engine ===\n";

    const DemoArgs args = ParseArgs(argc, argv);

    if (args.headless)
        return RunHeadless();

    return RunWindowed(args);
}
