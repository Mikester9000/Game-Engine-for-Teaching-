/**
 * @file input_mapper.hpp
 * @brief Win32 keyboard state → ECS component state — M8.2 Player Input.
 *
 * ============================================================================
 * TEACHING NOTE — Input Mapping Architecture
 * ============================================================================
 * "Input mapping" is the layer that sits between raw hardware events and game
 * logic.  Its job is to translate physical device state into logical game
 * actions that live in ECS components:
 *
 *   Hardware layer:   "W key is held down"
 *       ↓ InputMapper
 *   ECS layer:        TransformComponent::velocity.z += moveSpeed
 *
 * Separating these two layers is important because:
 *
 *   1. PORTABILITY — The game logic only knows about ECS components, not about
 *      GetAsyncKeyState or XInput.  Swapping the input backend (keyboard →
 *      gamepad → touch) doesn't touch the gameplay code.
 *
 *   2. REBINDING — The mapping between physical keys and logical actions can
 *      be stored in a data file and changed by the player at runtime without
 *      modifying the gameplay systems.
 *
 *   3. TESTING — In headless CI the InputMapper produces NO input by default.
 *      Tests can inject artificial input via ForceMoveForward() etc.
 *
 * ─── Win32 Input Reading ────────────────────────────────────────────────────
 * We use GetAsyncKeyState() which samples the keyboard state at the moment of
 * the call.  The high bit of the return value is set if the key is currently
 * held down:
 *
 *   bool held = (GetAsyncKeyState(VK_UP) & 0x8000) != 0;
 *
 * GetAsyncKeyState() is thread-safe and does NOT require a window focus check.
 * However it reflects the state of the *entire system*, so if another window
 * has focus it still returns input.  For a game loop running the main window
 * this is fine.
 *
 * TEACHING NOTE — Windows Virtual Key Codes (VK_*)
 * ────────────────────────────────────────────────────
 * Win32 uses integer "virtual key codes" (VKs) rather than physical scan codes:
 *   VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN  — arrow keys
 *   'A', 'D', 'W', 'S'                 — letter keys (must be uppercase)
 *   VK_SPACE                            — space bar
 *   VK_SHIFT                            — shift
 *   VK_LBUTTON                          — left mouse button
 *
 * ============================================================================
 * TEACHING NOTE — Headless / CI Guard
 * ============================================================================
 * GetAsyncKeyState is only available on Windows.  When compiled for Linux or
 * in a build that doesn't define _WIN32, the InputMapper simply does nothing.
 * This lets the same GameRuntime.cpp compile on Linux for CI.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2025
 * C++ Standard: C++17
 * Platform: Windows (engine_sandbox); no-op on Linux (CI safe)
 */

#pragma once

#include "engine/ecs/ECS.hpp"    // World, EntityID, TransformComponent, etc.
#include <cstdint>               // uint32_t

/**
 * @class InputMapper
 * @brief Reads Win32 keyboard/mouse state and writes it into ECS components.
 *
 * Created by GameRuntime and updated once per frame before gameplay systems.
 */
class InputMapper
{
public:
    InputMapper()  = default;
    ~InputMapper() = default;

    // Non-copyable.
    InputMapper(const InputMapper&)            = delete;
    InputMapper& operator=(const InputMapper&) = delete;

    // =========================================================================
    // Per-frame update
    // =========================================================================

    /**
     * @brief Sample current key state and write to ECS components.
     *
     * Must be called once per frame BEFORE any gameplay systems update.
     *
     * On non-Windows platforms this is a no-op — gameplay still runs headless.
     *
     * @param world     ECS World.
     * @param playerID  Player entity whose components receive input.
     * @param dt        Delta time (seconds) — used to scale velocity.
     */
    void Update(World& world, EntityID playerID, float dt);

    // =========================================================================
    // Test injection helpers (used by CI scenes)
    // =========================================================================

    /**
     * @brief Force one frame of "move forward" input, bypassing hardware.
     *
     * Used by headless CI scenes to exercise the movement path without a
     * real keyboard.
     */
    void ForceMoveForward(bool on) { m_forceMoveForward = on; }

    /**
     * @brief Force one frame of "attack" input, bypassing hardware.
     *
     * Used by m8_gameplay CI acceptance test to trigger combat.
     */
    void ForceAttack(bool on)      { m_forceAttack = on; }

private:
    // -------------------------------------------------------------------------
    // Test overrides
    // -------------------------------------------------------------------------
    bool m_forceMoveForward = false;
    bool m_forceAttack      = false;
};
