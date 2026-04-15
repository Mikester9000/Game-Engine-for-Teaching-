/**
 * @file main.cpp
 * @brief Entry point for the Educational Game Engine.
 *
 * ============================================================================
 * TEACHING NOTE — main() Should Be Minimal
 * ============================================================================
 *
 * A well-designed C++ application's main() function should be very short.
 * Its job is to:
 *   1. Construct the top-level application object.
 *   2. Call Init().
 *   3. Call Run() (blocks until the user quits).
 *   4. Call Shutdown().
 *   5. Return 0 (success) or a non-zero error code.
 *
 * ALL actual logic lives inside the application class and its subsystems.
 * This makes the code testable (you can construct `Game` in a test without
 * running main) and portable (you can replace the entry point for different
 * platforms — e.g., WinMain on Windows).
 *
 * ─── Return Values ────────────────────────────────────────────────────────────
 * Conventionally:
 *   0  = success
 *   1  = generic failure
 *  -1  = fatal exception
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 */

#include <iostream>
#include <exception>

#include "game/Game.hpp"

int main()
{
    try {
        Game& game = Game::Instance();
        game.Init();
        game.Run();
        game.Shutdown();
        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "[FATAL] Uncaught exception: " << ex.what() << "\n";
        return 1;
    }
    catch (...) {
        std::cerr << "[FATAL] Unknown exception in main().\n";
        return -1;
    }
}
