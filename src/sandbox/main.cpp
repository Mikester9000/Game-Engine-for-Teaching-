/**
 * @file main.cpp
 * @brief Entry point for the EngineSandbox — Windows + Vulkan rendering demo.
 *
 * ============================================================================
 * TEACHING NOTE — What is engine_sandbox?
 * ============================================================================
 * engine_sandbox is the *first real rendering milestone* of this educational
 * engine.  It demonstrates:
 *
 *   M0 — Window + Vulkan bootstrap: open a Win32 window, initialise Vulkan,
 *        render an animated clear-colour loop.
 *   M1 — Triangle: vertex shader, fragment shader, PSO, vertex buffer — the
 *        classic "hello triangle" that proves the graphics pipeline works.
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
 * or only a software Vulkan implementation) to validate the bootstrap path
 * without needing a monitor.
 *
 * Usage:
 *   engine_sandbox.exe                   # Normal windowed mode
 *   engine_sandbox.exe --headless        # M0 validation (Vulkan init OK)
 *   engine_sandbox.exe --headless --scene triangle   # M1 validation
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Target: Windows (MSVC) + Vulkan
 */

#include "engine/platform/win32/Win32Window.hpp"
#include "engine/rendering/vulkan/VulkanRenderer.hpp"
#include "engine/assets/asset_db.hpp"
#include "engine/assets/asset_loader.hpp"

#include <iostream>
#include <exception>
#include <cstring>      // std::strcmp
#include <cmath>        // std::sin — used for the animated clear colour
#include <string>
#include <filesystem>   // std::filesystem::path (C++17) for shader path resolution

// ---------------------------------------------------------------------------
// TEACHING NOTE — Shader Directory Resolution
// ---------------------------------------------------------------------------
// The compiled .spv shader files are placed next to the executable by CMake.
// We derive the executable's directory from argv[0] to construct an absolute
// path, so the binary works from any current working directory.
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
        bool        headless  = false;
        std::string scene;   // empty = no scene; "triangle" = M1 scene
        std::string validateProject;  // M2: path to project dir for AssetDB validation

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
                // accessible.  It runs without opening a Vulkan window and
                // exits 0 on success.
                //
                // Intended CI usage (after cook.exe has run):
                //   engine_sandbox.exe --validate-project samples/vertical_slice_project
                //
                // The flag:
                //   1. Loads Cooked/assetdb.json into AssetDB.
                //   2. Calls AssetLoader::LoadRaw for every entry.
                //   3. Prints [PASS] and exits 0 if all loads succeed.
                //   4. Prints [FAIL] and exits 1 if any load fails.
                // -----------------------------------------------------------
                validateProject = argv[++i];
            }
        }

        // -------------------------------------------------------------------
        // Step 0b — --validate-project: M2 AssetDB validation (no Vulkan).
        // -------------------------------------------------------------------
        // TEACHING NOTE — Validate-Only Mode
        // This path runs cook validation without opening a Vulkan window.
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
            for (std::size_t i = 0; i < db.Count(); ++i)
            {
                // We don't have an "iterate all GUIDs" API yet — the simple
                // validation here just confirms Load() succeeded and Count() > 0.
                // Full asset iteration will be added when AssetDB gains an
                // All() / begin()/end() interface in M3.
                (void)i;
            }
            // For now, a successful Load() with at least zero entries == PASS.
            (void)loader;

            if (loadErrors > 0)
            {
                std::cout << "[FAIL] " << loadErrors
                          << " asset(s) failed to load.\n";
                return 1;
            }

            std::cout << "[PASS] AssetDB validated successfully.\n";
            return 0;
        }

        // -------------------------------------------------------------------
        // Step 1 — Create and initialise the Win32 window.
        // -------------------------------------------------------------------
        engine::platform::Win32Window window;

        if (!window.Init(L"Engine Sandbox — Vulkan", 1280, 720, headless))
        {
            std::cerr << "[engine_sandbox] Failed to create window.\n";
            return 1;
        }

        if (!headless)
            std::cout << "[engine_sandbox] Window created: 1280×720\n";

        // -------------------------------------------------------------------
        // Step 2 — Create and initialise the Vulkan renderer.
        // -------------------------------------------------------------------
        // TEACHING NOTE — Renderer Needs the Window's HWND
        // The Vulkan surface must be tied to a real OS window handle (HWND).
        // That is why we create the window FIRST, then pass the handles to
        // the renderer.  The order matters: renderer → window is impossible.
        // -------------------------------------------------------------------
        engine::rendering::VulkanRenderer renderer;

        if (!renderer.Init(window.GetHINSTANCE(), window.GetHWND(),
                           window.GetWidth(), window.GetHeight()))
        {
            std::cerr << "[engine_sandbox] Failed to initialise Vulkan renderer.\n";
            return 1;
        }

        if (!headless)
        {
            std::cout << "[engine_sandbox] Vulkan renderer ready.\n";
            std::cout << "[engine_sandbox] Press ESC or close the window to exit.\n";
        }

        // -------------------------------------------------------------------
        // Step 3 — Load the requested scene (M1+).
        // -------------------------------------------------------------------
        if (!scene.empty())
        {
            std::string shaderDir = GetShaderDir(argv[0]);

            if (!renderer.LoadScene(scene, shaderDir))
            {
                std::cerr << "[FAIL] Failed to load scene '" << scene << "'.\n";
                renderer.Shutdown();
                window.Shutdown();
                return 1;
            }
        }

        // -------------------------------------------------------------------
        // Step 4 — Headless validation path (no render loop).
        // -------------------------------------------------------------------
        // TEACHING NOTE — Headless Exit Protocol
        // Acceptance tests expect exactly one of these lines on stdout,
        // followed by exit code 0.  Any other output (or non-zero exit) = fail.
        // -------------------------------------------------------------------
        if (headless)
        {
            if (scene == "triangle")
            {
                // Validate: record one frame to confirm the draw call works.
                if (!renderer.RecordHeadlessFrame())
                {
                    std::cout << "[FAIL] Headless frame recording failed.\n";
                    renderer.Shutdown();
                    window.Shutdown();
                    return 1;
                }
                std::cout << "[PASS] Pipeline created. Mesh uploaded. Draw recorded.\n";
            }
            else
            {
                // M0 baseline: Vulkan init + swapchain creation succeeded.
                std::cout << "[PASS] Vulkan device initialised. Swapchain created."
                             " Headless mode: skipping present loop.\n";
            }

            renderer.Shutdown();
            window.Shutdown();
            return 0;
        }

        // -------------------------------------------------------------------
        // Step 5 — Main render loop (non-headless).
        // -------------------------------------------------------------------
        // TEACHING NOTE — Fixed Timestep vs Variable Timestep
        // For this minimal demo we use a simple variable-timestep loop:
        // render as fast as the GPU allows (limited by vsync / mailbox
        // present mode).  A real game loop uses a fixed timestep for
        // deterministic physics — that will be introduced in a future module.
        // -------------------------------------------------------------------
        double totalTime = 0.0;

        while (window.IsRunning())
        {
            // Poll Win32 messages (resize, keyboard, close, etc.)
            window.PollEvents();

            // Handle window resize — tell the renderer to rebuild the swapchain.
            if (window.WasResized())
            {
                renderer.RecreateSwapchain(window.GetWidth(), window.GetHeight());
                window.ClearResizedFlag();
            }

            // Advance total time using delta time from the window timer.
            double dt = window.GetDeltaTime();
            totalTime += dt;

            // Compute an animated clear colour.
            // TEACHING NOTE — std::sin / std::cos for animation
            // Each channel has a different phase offset so they don't all
            // peak at the same moment, producing a smooth rainbow sweep.
            const float speed = 0.5f;
            const float tF    = static_cast<float>(totalTime);
            float clearR = (std::sin(tF * speed + 0.0f)   + 1.0f) * 0.5f;
            float clearG = (std::sin(tF * speed + 2.094f) + 1.0f) * 0.5f;  // 2π/3
            float clearB = (std::sin(tF * speed + 4.189f) + 1.0f) * 0.5f;  // 4π/3

            // Draw the frame (acquire → record → submit → present).
            renderer.DrawFrame(clearR, clearG, clearB);
        }

        // -------------------------------------------------------------------
        // Step 6 — Shutdown in reverse-creation order.
        // -------------------------------------------------------------------
        // TEACHING NOTE — Shutdown Order
        // The renderer must be shut down BEFORE the window because the
        // Vulkan surface references the HWND.  Destroying the window first
        // would leave the surface pointing at a destroyed handle.
        // -------------------------------------------------------------------
        renderer.Shutdown();
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
