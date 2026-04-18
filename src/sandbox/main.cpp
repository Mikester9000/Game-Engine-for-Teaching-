/**
 * @file main.cpp
 * @brief Entry point for EngineSandbox — Windows rendering demo.
 *
 * ============================================================================
 * TEACHING NOTE — What is engine_sandbox?
 * ============================================================================
 * engine_sandbox is the *first real rendering milestone* of this educational
 * engine.  It demonstrates:
 *
 *   M0 — Window + renderer bootstrap: open a Win32 window, initialise the
 *        chosen rendering backend, render an animated clear-colour loop.
 *   M1 — Triangle: vertex shader, fragment shader, PSO, vertex buffer — the
 *        classic "hello triangle" that proves the graphics pipeline works.
 *
 * ============================================================================
 * TEACHING NOTE — Renderer Backends
 * ============================================================================
 * engine_sandbox supports two rendering backends selectable at runtime:
 *
 *   --renderer d3d11   (default) — Direct3D 11; works on any GPU from ~2006+
 *                                  including the GeForce GT 610.  Uses D3D11
 *                                  WARP (CPU software renderer) in headless/CI
 *                                  mode so no GPU driver is needed in CI.
 *
 *   --renderer vulkan  (optional) — Vulkan 1.0+; the modern / high-end path.
 *                                   Requires a Vulkan ICD on the machine.
 *                                   Only available when compiled with
 *                                   ENGINE_ENABLE_VULKAN=ON.
 *
 * D3D11 is the default because:
 *   1. It ships with Windows (no install needed).
 *   2. It runs on older hardware (GT610 class) and CI runners alike.
 *   3. D3D11 WARP lets --headless succeed on GitHub-hosted Windows runners
 *      without any GPU driver installed.
 *
 * ============================================================================
 * TEACHING NOTE — WinMain vs main()
 * ============================================================================
 * A standard Windows console app uses main().  A Windows GUI app (no console
 * window) uses WinMain.  For teaching purposes we use the standard main()
 * entry point and link with the CONSOLE subsystem so log output is visible
 * in the terminal.  When you ship a game you would switch to WinMain and
 * pipe log output to a file instead.
 *
 * ============================================================================
 * TEACHING NOTE — --headless Mode
 * ============================================================================
 * Running with --headless skips the main render loop and exits with code 0
 * after printing "[PASS]".  This allows CI machines (which have no display
 * and may have no GPU driver) to validate the bootstrap path.
 *
 * In D3D11 mode, headless uses WARP — a CPU software rasteriser bundled with
 * every Windows installation — so the binary works on any Windows runner.
 *
 * Usage:
 *   engine_sandbox.exe                              # D3D11 windowed (default)
 *   engine_sandbox.exe --headless                   # D3D11 WARP headless (CI)
 *   engine_sandbox.exe --renderer vulkan --headless # Vulkan headless
 *   engine_sandbox.exe --headless --scene triangle  # M1 validation
 *   engine_sandbox.exe --scene testworld            # M3 full system demo (windowed)
 *   engine_sandbox.exe --headless --scene testworld # M3 full system demo (CI)
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Target: Windows (MSVC)
 */

#include "engine/platform/win32/Win32Window.hpp"
#include "engine/rendering/IRenderer.hpp"
#include "engine/rendering/RendererFactory.hpp"
#include "engine/assets/asset_db.hpp"
#include "engine/assets/asset_loader.hpp"
#include "sandbox/test_world.hpp"

#include <iostream>
#include <exception>
#include <cstring>      // std::strcmp
#include <cmath>        // std::sin — used for the animated clear colour
#include <string>
#include <filesystem>   // std::filesystem::path (C++17)
#include <chrono>       // high_resolution_clock (testworld dt measurement)

// ---------------------------------------------------------------------------
// TEACHING NOTE — Shader Directory Resolution
// ---------------------------------------------------------------------------
// The compiled shader files (.spv for Vulkan, .cso for D3D11) are placed next
// to the executable by CMake.  We derive the executable's directory from
// argv[0] to construct an absolute path, so the binary works from any CWD.
// ---------------------------------------------------------------------------
static std::string GetShaderDir(const char* argv0)
{
    namespace fs = std::filesystem;
    fs::path p(argv0);
    fs::path dir = p.has_parent_path() ? p.parent_path() : fs::path(".");
    return (dir / "shaders" / "").string();   // trailing separator
}

// ---------------------------------------------------------------------------
// TEACHING NOTE — Entry Point with argc/argv
// ---------------------------------------------------------------------------
// We use int main(int argc, char* argv[]) so the executable can receive
// command-line flags.  This is standard C++ — on Windows the MSVC linker
// routes it to the correct Windows entry point when we use /SUBSYSTEM:CONSOLE.
// ---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    try
    {
        // -------------------------------------------------------------------
        // Step 0 — Parse command-line arguments.
        // -------------------------------------------------------------------
        // TEACHING NOTE — Command-Line Parsing
        // We use a simple linear scan rather than a third-party flag library
        // to keep the dependency count zero and the code readable.
        // -------------------------------------------------------------------
        bool        headless         = false;
        std::string scene;               // empty = no scene; "triangle" = M1
        std::string validateProject;     // M2: path to project dir
        std::string rendererArg;         // "d3d11" or "vulkan"; empty = default

        for (int i = 1; i < argc; ++i)
        {
            if (std::strcmp(argv[i], "--headless") == 0)
            {
                headless = true;
            }
            else if (std::strcmp(argv[i], "--scene") == 0 && i + 1 < argc)
            {
                scene = argv[++i];
            }
            else if (std::strcmp(argv[i], "--validate-project") == 0 && i + 1 < argc)
            {
                // -----------------------------------------------------------
                // TEACHING NOTE — --validate-project flag
                // -----------------------------------------------------------
                // This M2 flag validates that the project's cooked asset
                // database can be loaded and that every registered asset is
                // accessible.  It runs without opening a renderer window and
                // exits 0 on success.
                //
                // Intended CI usage (after cook.exe has run):
                //   engine_sandbox.exe --validate-project samples/vertical_slice_project
                // -----------------------------------------------------------
                validateProject = argv[++i];
            }
            else if (std::strcmp(argv[i], "--renderer") == 0 && i + 1 < argc)
            {
                // -----------------------------------------------------------
                // TEACHING NOTE — --renderer flag
                // -----------------------------------------------------------
                // Selects the graphics backend at runtime.
                //   --renderer d3d11   → Direct3D 11 (default; GT610 compatible)
                //   --renderer vulkan  → Vulkan 1.0+ (requires Vulkan ICD)
                // -----------------------------------------------------------
                rendererArg = argv[++i];
            }
        }

        // -------------------------------------------------------------------
        // Step 0b — --validate-project: M2 AssetDB validation (no renderer).
        // -------------------------------------------------------------------
        // TEACHING NOTE — Validate-Only Mode
        // This path runs cook validation without opening any renderer window.
        // It exercises the AssetDB + AssetLoader pipeline introduced in M2.
        // -------------------------------------------------------------------
        if (!validateProject.empty())
        {
            namespace fs = std::filesystem;

            const fs::path projectPath(validateProject);
            const fs::path assetDbPath = projectPath / "Cooked" / "assetdb.json";

            std::cout << "[validate-project] Project: " << validateProject << "\n";
            std::cout << "[validate-project] Loading: " << assetDbPath.string() << "\n";

            engine::assets::AssetDB db;
            if (!db.Load(assetDbPath.string()))
            {
                std::cout << "[FAIL] AssetDB::Load failed for: "
                          << assetDbPath.string() << "\n";
                return 1;
            }

            std::cout << "[validate-project] AssetDB loaded: "
                      << db.Count() << " asset(s).\n";

            engine::assets::AssetLoader loader(&db);

            int loadErrors = 0;

            // TEACHING NOTE — Validating every asset in the database
            // db.All() returns all GUIDs.  We iterate every GUID and call
            // loader.LoadRaw(), which opens the cooked file.  An empty return
            // vector signals a failure — either the cooked file is missing,
            // corrupted, or the path is wrong.
            for (const std::string& guid : db.All())
            {
                const auto bytes = loader.LoadRaw(guid);
                if (bytes.empty())
                    ++loadErrors;
            }

            if (loadErrors > 0)
            {
                std::cout << "[FAIL] " << loadErrors << " asset(s) failed to load.\n";
                return 1;
            }

            std::cout << "[PASS] AssetDB validated successfully.\n";
            return 0;
        }

        // -------------------------------------------------------------------
        // Step 1 — Resolve the renderer backend.
        // -------------------------------------------------------------------
        // TEACHING NOTE — Default Backend: D3D11
        // If --renderer is not specified we use D3D11 because it works on all
        // Windows machines from Win7 (GT610-compatible) and on CI runners
        // with no GPU driver (via the WARP software renderer).
        // -------------------------------------------------------------------
        const auto backend = engine::rendering::ParseRendererBackend(rendererArg);

        // -------------------------------------------------------------------
        // Step 2 — Create and initialise the Win32 window.
        // -------------------------------------------------------------------
        engine::platform::Win32Window window;

        // Build a wide-string title including the backend name.
        const wchar_t* title =
            (backend == engine::rendering::RendererBackend::Vulkan)
                ? L"Engine Sandbox \u2014 Vulkan"
                : L"Engine Sandbox \u2014 D3D11";

        if (!window.Init(title, 1280, 720, headless))
        {
            std::cerr << "[engine_sandbox] Failed to create window.\n";
            return 1;
        }

        if (!headless)
            std::cout << "[engine_sandbox] Window created: 1280x720\n";

        // -------------------------------------------------------------------
        // Step 3 — Create and initialise the renderer via the factory.
        // -------------------------------------------------------------------
        // TEACHING NOTE — Factory Usage
        // CreateRenderer returns a std::unique_ptr<IRenderer> so ownership
        // is clear: main() owns the renderer, and it is automatically
        // destroyed when the unique_ptr goes out of scope.
        // -------------------------------------------------------------------
        auto renderer = engine::rendering::CreateRenderer(backend);
        if (!renderer)
        {
            std::cerr << "[engine_sandbox] Failed to create renderer.\n";
            window.Shutdown();
            return 1;
        }

        if (!renderer->Init(window.GetHINSTANCE(), window.GetHWND(),
                            window.GetWidth(), window.GetHeight(),
                            headless))
        {
            std::cerr << "[engine_sandbox] Failed to initialise "
                      << renderer->BackendName() << " renderer.\n";
            window.Shutdown();
            return 1;
        }

        if (!headless)
        {
            std::cout << "[engine_sandbox] " << renderer->BackendName()
                      << " renderer ready.\n";
            std::cout << "[engine_sandbox] Press ESC or close the window to exit.\n";
        }

        // -------------------------------------------------------------------
        // Step 4 — Load the requested scene (M1+).
        // -------------------------------------------------------------------
        if (!scene.empty())
        {
            std::string shaderDir = GetShaderDir(argv[0]);

            if (!renderer->LoadScene(scene, shaderDir))
            {
                std::cerr << "[FAIL] Failed to load scene '" << scene << "'.\n";
                renderer->Shutdown();
                window.Shutdown();
                return 1;
            }
        }

        // -------------------------------------------------------------------
        // Step 5 — Headless validation path (no render loop).
        // -------------------------------------------------------------------
        // TEACHING NOTE — Headless Exit Protocol
        // Acceptance tests expect exactly one "[PASS]" line on stdout
        // followed by exit code 0.  Any other output (or non-zero exit) = fail.
        // -------------------------------------------------------------------
        if (headless)
        {
            if (scene == "triangle")
            {
                // Validate: record one frame to confirm the draw call works.
                if (!renderer->RecordHeadlessFrame())
                {
                    std::cout << "[FAIL] Headless frame recording failed.\n";
                    renderer->Shutdown();
                    window.Shutdown();
                    return 1;
                }
                std::cout << "[PASS] Pipeline created. Mesh uploaded. Draw recorded.\n";
            }
            else if (scene == "testworld")
            {
                // -----------------------------------------------------------
                // TEACHING NOTE — Headless TestWorld
                // -----------------------------------------------------------
                // Boots all gameplay systems, runs 600 fixed-dt frames, then
                // exits 0 if every system reported OK.  Ideal for CI: no GPU
                // or audio hardware required.
                // -----------------------------------------------------------
                engine::sandbox::TestWorld tw;
                if (!tw.Init())
                {
                    std::cout << "[FAIL] TestWorld::Init() returned false.\n";
                    renderer->Shutdown();
                    window.Shutdown();
                    return 1;
                }

                constexpr float FIXED_DT = 1.0f / 60.0f;
                constexpr int   FRAMES   = 600;
                for (int f = 0; f < FRAMES; ++f)
                    tw.Update(FIXED_DT);

                if (!tw.AllSystemsOk())
                {
                    std::cout << "[FAIL] TestWorld: one or more systems failed.\n";
                    renderer->Shutdown();
                    window.Shutdown();
                    return 1;
                }

                std::cout << "[PASS] TestWorld: all systems exercised OK.\n";
            }
            else
            {
                // M0 baseline: device init succeeded.
                std::cout << "[PASS] " << renderer->BackendName()
                          << " device initialised. Headless mode: "
                             "skipping present loop.\n";
            }

            renderer->Shutdown();
            window.Shutdown();
            return 0;
        }

        // -------------------------------------------------------------------
        // Step 6 — Main render loop (non-headless).
        // -------------------------------------------------------------------
        // TEACHING NOTE — Fixed Timestep vs Variable Timestep
        // For this minimal demo we use a simple variable-timestep loop:
        // render as fast as the GPU allows (limited by vsync).
        // A real game loop uses a fixed timestep for deterministic physics.
        // -------------------------------------------------------------------
        double totalTime = 0.0;

        // -----------------------------------------------------------------------
        // TEACHING NOTE — TestWorld integration in the render loop
        // -----------------------------------------------------------------------
        // When --scene testworld is specified, we create a TestWorld and call
        // tw.Update(dt) each frame.  The TestWorld returns an RGB clear-colour
        // (GetClearColour) that reflects the current game state:
        //
        //   Combat active → red tint       Night → deep navy
        //   Victory flash → gold pulse     Rain  → grey-blue
        //   Camping       → warm orange    Day   → sky blue
        //
        // This gives a visual confirmation that the game systems are running
        // and changing state.  As more rendering milestones land, replace the
        // clear-colour with actual geometry + lighting draw calls.
        // -----------------------------------------------------------------------
        std::unique_ptr<engine::sandbox::TestWorld> testWorld;
        if (scene == "testworld")
        {
            testWorld = std::make_unique<engine::sandbox::TestWorld>();
            if (!testWorld->Init())
            {
                std::cerr << "[FAIL] TestWorld::Init() returned false.\n";
                renderer->Shutdown();
                window.Shutdown();
                return 1;
            }
        }

        while (window.IsRunning())
        {
            // Poll Win32 messages (resize, keyboard, close, etc.)
            window.PollEvents();

            // Handle window resize — tell the renderer to rebuild resources.
            if (window.WasResized())
            {
                renderer->RecreateSwapchain(window.GetWidth(), window.GetHeight());
                window.ClearResizedFlag();
            }

            // Advance total time using delta time from the window timer.
            double dt = window.GetDeltaTime();
            totalTime += dt;

            float clearR, clearG, clearB;

            if (testWorld)
            {
                // -- TestWorld mode: update all game systems, map state to colour.
                testWorld->Update(static_cast<float>(dt));
                testWorld->GetClearColour(clearR, clearG, clearB);
            }
            else
            {
                // -- Default mode: animated rainbow clear colour.
                // TEACHING NOTE — std::sin / std::cos for animation
                // Each channel has a different phase offset so they don't all
                // peak at the same moment, producing a smooth rainbow sweep.
                const float speed = 0.5f;
                const float tF    = static_cast<float>(totalTime);
                clearR = (std::sin(tF * speed + 0.0f)   + 1.0f) * 0.5f;
                clearG = (std::sin(tF * speed + 2.094f) + 1.0f) * 0.5f;  // 2pi/3
                clearB = (std::sin(tF * speed + 4.189f) + 1.0f) * 0.5f;  // 4pi/3
            }

            renderer->DrawFrame(clearR, clearG, clearB);
        }

        // -------------------------------------------------------------------
        // Step 7 — Shutdown in reverse-creation order.
        // -------------------------------------------------------------------
        // TEACHING NOTE — Shutdown Order
        // The renderer must be shut down BEFORE the window because the
        // swap chain / surface references the HWND.  Destroying the window
        // first would leave the renderer pointing at a destroyed handle.
        // -------------------------------------------------------------------
        renderer->Shutdown();
        window.Shutdown();

        std::cout << "[engine_sandbox] Clean exit.\n";
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[FATAL] Unhandled exception: " << ex.what() << "\n";
        return 1;
    }
    catch (...)
    {
        std::cerr << "[FATAL] Unknown exception in main().\n";
        return -1;
    }
}
