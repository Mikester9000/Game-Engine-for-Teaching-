/**
 * @file input_mapper.cpp
 * @brief Win32 keyboard state → ECS component state implementation.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2025
 */

#include "game/systems/input_mapper.hpp"
#include "engine/core/Logger.hpp"

// TEACHING NOTE — Platform-gated keyboard sampling
// GetAsyncKeyState lives in <windows.h>.  On Linux / headless CI this
// translation unit still compiles — the #ifdef bodies simply become no-ops,
// leaving m_forceMoveForward / m_forceAttack as the only input channels.
#ifdef _WIN32
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#endif

// ===========================================================================
// Helpers
// ===========================================================================
namespace {

/// Returns true if a Win32 virtual key is currently held down.
/// On non-Windows builds always returns false.
#ifdef _WIN32
inline bool KeyHeld(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}
#else
inline bool KeyHeld(int) { return false; }
#endif

} // namespace anonymous

// ===========================================================================
// InputMapper::Update
// ===========================================================================
void InputMapper::Update(World& world, EntityID playerID, float dt)
{
    // TEACHING NOTE — Guard: dead or invalid player
    // The player may not exist yet (e.g. during the first few frames of Init)
    // or may have died.  A simple IsAlive() check prevents crashes.
    if (playerID == NULL_ENTITY || !world.IsAlive(playerID))
        return;

    if (!world.HasComponent<TransformComponent>(playerID) ||
        !world.HasComponent<MovementComponent>(playerID))
        return;

    auto& tf = world.GetComponent<TransformComponent>(playerID);
    auto& mv = world.GetComponent<MovementComponent>(playerID);

    // -------------------------------------------------------------------------
    // TEACHING NOTE — Determine move speed
    // Sprint (SHIFT held) doubles move speed.  We read the MovementComponent's
    // moveSpeed rather than hardcoding a constant so the component can be
    // tweaked at runtime without recompiling.
    // -------------------------------------------------------------------------
    const bool sprinting = KeyHeld(VK_SHIFT);
    const float speed    = sprinting ? mv.sprintSpeed : mv.moveSpeed;

    // -------------------------------------------------------------------------
    // WASD / Arrow keys → velocity
    //
    // TEACHING NOTE — Velocity vs position
    // We set velocity on the TransformComponent rather than moving position
    // directly.  Velocity is later consumed by a MovementSystem (or
    // integrated here with * dt) so physics and AI can observe/override it.
    //
    // For this implementation we integrate immediately (position += vel * dt)
    // because a dedicated MovementSystem is not yet created (planned for M8.4).
    // -------------------------------------------------------------------------
    const bool moveForward  = KeyHeld('W')      || KeyHeld(VK_UP)    || m_forceMoveForward;
    const bool moveBackward = KeyHeld('S')      || KeyHeld(VK_DOWN);
    const bool moveLeft     = KeyHeld('A')      || KeyHeld(VK_LEFT);
    const bool moveRight    = KeyHeld('D')      || KeyHeld(VK_RIGHT);

    // Build movement direction in the XZ plane (Y is up in our coordinate system).
    float dx = 0.0f;
    float dz = 0.0f;
    if (moveForward)  dz += 1.0f;
    if (moveBackward) dz -= 1.0f;
    if (moveRight)    dx += 1.0f;
    if (moveLeft)     dx -= 1.0f;

    // Normalize diagonal movement so WASD diagonal = same speed as cardinal.
    const float len = std::sqrt(dx*dx + dz*dz);
    if (len > 0.01f) {
        dx /= len;
        dz /= len;
    }

    // Write velocity so other systems (e.g. physics) can observe it.
    tf.velocity.x = dx * speed;
    tf.velocity.z = dz * speed;

    // Integrate position this frame.
    tf.Translate({ dx * speed * dt, 0.0f, dz * speed * dt });

    // -------------------------------------------------------------------------
    // SPACE / J key → melee attack request
    //
    // TEACHING NOTE — Requesting actions via component flags
    // Rather than calling CombatSystem::PlayerAttack() directly we set a flag
    // on the CombatComponent.  The CombatSystem checks this flag each Update()
    // and executes the attack.  This keeps the input mapper decoupled from the
    // combat system entirely.
    // -------------------------------------------------------------------------
    if (world.HasComponent<CombatComponent>(playerID))
    {
        auto& cb  = world.GetComponent<CombatComponent>(playerID);
        const bool attackRequested = KeyHeld(VK_SPACE) || KeyHeld('J') || m_forceAttack;

        // Only set the flag if the cooldown has expired; avoids spamming.
        if (attackRequested && cb.attackCooldown <= 0.0f)
        {
            cb.isInCombat  = true;
            // TEACHING NOTE — attackCooldown is reset here to a base value;
            // CombatSystem will refine it based on stats.
            cb.attackCooldown = 1.0f / std::max(0.1f, cb.attackRate);
        }
    }

    // TEACHING NOTE — Unconditional force-flag reset.
    // m_forceMoveForward and m_forceAttack are test-injection flags set by
    // unit tests or CI harnesses before a single Update() call.  We reset
    // them unconditionally every frame so injected inputs fire exactly once,
    // regardless of whether they were set.  This guarantees deterministic
    // CI behaviour without requiring callers to manually clear the flags.
    m_forceMoveForward = false;
    m_forceAttack      = false;
}
