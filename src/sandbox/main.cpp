/**
 * @file main.cpp
 * @brief Entry point for the EngineSandbox — Windows + Vulkan "clear screen" demo.
 *
 * ============================================================================
 * TEACHING NOTE — What is engine_sandbox?
 * ============================================================================
 * engine_sandbox is the *first real rendering milestone* of this educational
 * engine.  Its only job is to:
 *
 *   1. Open a Win32 window.
 *   2. Initialise a Vulkan renderer.
 *   3. Each frame: poll OS events, render a coloured frame, present it.
 *   4. On close: shut everything down cleanly.
 *
 * This is the "Hello Triangle" precursor — before drawing geometry you must
 * have a working window + GPU connection + swapchain.  Many AAA engines
 * started exactly like this.
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
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Target: Windows (MSVC) + Vulkan
 */

#include "engine/platform/win32/Win32Window.hpp"
#include "engine/rendering/vulkan/VulkanRenderer.hpp"

#include <iostream>
#include <exception>
#include <cmath>        // std::sin — used for the animated clear colour

// ---------------------------------------------------------------------------
// TEACHING NOTE — Entry Point
// ---------------------------------------------------------------------------
// We keep main() minimal:
//   1. Create the window.
//   2. Create the renderer.
//   3. Run the frame loop until the user closes the window.
//   4. Shut down and return.
//
// All complexity is encapsulated in Win32Window and VulkanRenderer.
// ---------------------------------------------------------------------------
int main()
{
    try
    {
        // -------------------------------------------------------------------
        // Step 1 — Create and initialise the Win32 window.
        // -------------------------------------------------------------------
        engine::platform::Win32Window window;

        if (!window.Init(L"Engine Sandbox — Vulkan Clear Screen", 1280, 720))
        {
            std::cerr << "[engine_sandbox] Failed to create window.\n";
            return 1;
        }

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

        std::cout << "[engine_sandbox] Vulkan renderer ready.\n";
        std::cout << "[engine_sandbox] Press ESC or close the window to exit.\n";

        // -------------------------------------------------------------------
        // Step 3 — Main loop.
        // -------------------------------------------------------------------
        // TEACHING NOTE — Fixed Timestep vs Variable Timestep
        // For this minimal clear-screen demo we use a simple variable-timestep
        // loop: render as fast as the GPU allows (limited by vsync / mailbox
        // present mode).  A real game loop uses a fixed timestep for
        // deterministic physics — that will be introduced in a future module.
        // -------------------------------------------------------------------

        // Slowly cycle the clear colour so we can visually confirm the
        // renderer is running and is not just frozen on a black screen.
        // TEACHING NOTE — Hue cycling via time
        // We animate R/G/B components as offset sine waves.  The resulting
        // colour sweeps through the rainbow, giving an immediate visual
        // confirmation that frames are being presented and delta time works.
        double totalTime = 0.0;

        while (window.IsRunning())
        {
            // -----------------------------------------------------------------
            // Poll Win32 messages (resize, keyboard, close, etc.)
            // -----------------------------------------------------------------
            window.PollEvents();

            // -----------------------------------------------------------------
            // Handle window resize — tell the renderer to rebuild the swapchain.
            // -----------------------------------------------------------------
            if (window.WasResized())
            {
                renderer.RecreateSwapchain(window.GetWidth(), window.GetHeight());
                window.ClearResizedFlag();
            }

            // -----------------------------------------------------------------
            // Advance total time using delta time from the window timer.
            // -----------------------------------------------------------------
            double dt = window.GetDeltaTime();
            totalTime += dt;

            // -----------------------------------------------------------------
            // Compute an animated clear colour.
            // -----------------------------------------------------------------
            // TEACHING NOTE — std::sin / std::cos for animation
            // sin() and cos() oscillate between -1 and +1.  Remapping to
            // [0, 1] gives a smooth colour wave: (sin(t * speed + phase) + 1) / 2.
            // Each channel has a different phase offset so they don't all
            // peak at the same moment.
            // -----------------------------------------------------------------
            const float speed  = 0.5f;
            const float tF     = static_cast<float>(totalTime);
            float clearR = (std::sin(tF * speed + 0.0f) + 1.0f) * 0.5f;
            float clearG = (std::sin(tF * speed + 2.094f) + 1.0f) * 0.5f;  // 2π/3
            float clearB = (std::sin(tF * speed + 4.189f) + 1.0f) * 0.5f;  // 4π/3

            // -----------------------------------------------------------------
            // Draw the frame (acquire → record → submit → present).
            // -----------------------------------------------------------------
            renderer.DrawFrame(clearR, clearG, clearB);
        }

        // -------------------------------------------------------------------
        // Step 4 — Shutdown in reverse-creation order.
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
