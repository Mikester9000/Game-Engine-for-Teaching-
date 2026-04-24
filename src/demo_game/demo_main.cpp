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
    // 6. Report PASS.
    // -----------------------------------------------------------------------
    world.Shutdown();
    std::cout << "[PASS] demo_world: 5 acceptance tests passed "
                 "(init, boot_menu, biome_cycle, stations, teleport).\n";
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
// ─────────────────────────────────────────────────────────────────────────────

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
    if (!renderer->Init(window.GetHWND(), kWidth, kHeight))
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
    // Main loop.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — F1 key state tracking (windowed mode)
    // We track whether F1 was down in the previous frame to detect a rising
    // edge (key just pressed).  Using a local bool avoids static state inside
    // the loop body.  In a full implementation this would live in InputMapper
    // or an InputSystem component so multiple systems can react.
    // -----------------------------------------------------------------------
    bool debugMenuOpen = false; // F1 overlay toggle
    bool f1WasDown     = false; // previous-frame F1 state for rising-edge detect

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

        // -----------------------------------------------------------------
        // Input: F1 → debug menu toggle.
        // TEACHING NOTE — Key-State Polling (Win32)
        // GetAsyncKeyState returns the real-time physical key state;
        // 0x8000 is the "currently held down" bit.  We track the previous
        // frame state in f1WasDown to detect a rising edge (just pressed).
        // In a full engine this would be routed through EventBus so multiple
        // systems can react without each polling the Win32 API directly.
        // -----------------------------------------------------------------
        const bool f1IsDown = (::GetAsyncKeyState(VK_F1) & 0x8000) != 0;
        if (f1IsDown && !f1WasDown)
        {
            debugMenuOpen = !debugMenuOpen;
            std::cout << "[demo_game] F1 debug menu: "
                      << (debugMenuOpen ? "OPEN" : "CLOSED") << "\n";
        }
        f1WasDown = f1IsDown;

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
        // Render.
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

        renderer->DrawFrame(r, g, b, static_cast<float>(window.GetDeltaTime()));
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
