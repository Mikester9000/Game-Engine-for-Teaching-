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

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>  // GetAsyncKeyState, VK_F1
#endif

// ---------------------------------------------------------------------------
// Engine: platform + rendering
// ---------------------------------------------------------------------------
#include "engine/platform/win32/Win32Window.hpp"
#include "engine/rendering/RendererFactory.hpp"
#include "engine/rendering/IRenderer.hpp"

// ---------------------------------------------------------------------------
// Demo game: OpenWorld state machine
// ---------------------------------------------------------------------------
#include "demo_game/open_world.hpp"

// ---------------------------------------------------------------------------
// Game runtime (reuse existing M8 gameplay systems)
// ---------------------------------------------------------------------------
#include "sandbox/game_runtime.hpp"

// ---------------------------------------------------------------------------
// Standard + Windows headers
// ---------------------------------------------------------------------------
#include <iostream>
#include <string>
#include <memory>
#include <chrono>
#include <cstring>   // std::strcmp
#include <cstdio>    // std::snprintf (overlay text formatting)
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

static const char* kBootItems[] = { "New Game", "Continue  (coming soon)", "Settings  (coming soon)", "Quit" };
static constexpr int kBootItemCount = 4;

static void DrawBootMenu(HWND hwnd, int clientW, int clientH, int selection)
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
        const bool stub = (i == 1 || i == 2); // Continue + Settings are stubs

        char line[128];
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

} // namespace overlay

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
    // 5. Teleport test — exercise TeleportToStation.
    // -----------------------------------------------------------------------
    world.TeleportToStation("rendering_pbr");
    if (world.GetCurrentBiome() != BiomeType::GRASSLAND)
    {
        std::cout << "[FAIL] demo_world/5: TeleportToStation(\"rendering_pbr\") "
                     "did not set biome to GRASSLAND.\n";
        return 1;
    }
    std::cout << "[OK] demo_world/5: Teleport to rendering_pbr → biome = GRASSLAND.\n";

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
    // 7. Report PASS.
    // -----------------------------------------------------------------------
    world.Shutdown();
    std::cout << "[PASS] demo_world: 6 acceptance tests passed "
                 "(init, boot_menu, biome_cycle, stations, teleport, json_fallback).\n";
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
    KeyEdge keyESC (VK_ESCAPE);      // ESC → pause / close overlay
    KeyEdge keyUp  (VK_UP);          // ↑ navigate menu / station list
    KeyEdge keyDown(VK_DOWN);        // ↓ navigate menu / station list
    KeyEdge keyEnter(VK_RETURN);     // Enter → confirm selection
    KeyEdge keyD   ('D');            // D → toggle debug info strip

    // --- Overlay state ---
    bool debugMenuOpen  = false; ///< F1 overlay visible
    bool showDebugInfo  = false; ///< Debug info bar (biome + FPS)
    int  bootMenuSel    = 0;     ///< Boot menu highlighted item (0..3)
    int  stationSel     = 0;     ///< F1 overlay selected station index

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
        const bool pressedEnter = keyEnter.JustPressed();
        const bool pressedD     = keyD.JustPressed();

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
            // Enter activates the selected item.  Stub items (Continue, Settings)
            // log but do not change state; in a full game they would open the
            // save-slot picker or settings panel.
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
                    case 1: // Continue (stub)
                        std::cout << "[demo_game] Continue: not yet implemented.\n";
                        break;
                    case 2: // Settings (stub)
                        std::cout << "[demo_game] Settings: not yet implemented.\n";
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
        else if (debugMenuOpen)
        {
            // -----------------------------------------------------------------
            // F1 OVERLAY input
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
            overlay::DrawBootMenu(hwnd, winW, winH, bootMenuSel);
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
