/**
 * @file weather_fx.hpp
 * @brief Weather rendering effect state — fog density, rain intensity, cloud cover.
 *
 * ============================================================================
 * TEACHING NOTE — Weather as a Rendering Layer
 * ============================================================================
 * FF15's weather system has two distinct concerns:
 *
 *   1. GAME LOGIC — WeatherSystem (game/systems/WeatherSystem.hpp) owns the
 *      authoritative FSM: Clear → Cloudy → Rain → Storm.  It broadcasts
 *      WeatherChanged events to any subscriber (enemy spawns, music, etc.).
 *
 *   2. RENDERING STATE — WeatherFx (this file) translates the current
 *      WeatherType into shader-friendly floats: fog density, rain intensity,
 *      and cloud cover.  It knows nothing about the game simulation — only
 *      about what the sky shader needs.
 *
 * This separation respects the engine architecture layer rule:
 *   game/systems/ → engine/rendering/   (allowed: game may call engine)
 *   engine/rendering/ → game/systems/   (FORBIDDEN: engine must not depend on game)
 *
 * The bridge is the shared ::WeatherType enum defined in engine/core/Types.hpp.
 * engine/rendering/ is allowed to include engine/core/ headers — both are in
 * the engine layer.  The game layer's WeatherSystem already stores and
 * broadcasts a ::WeatherType value, so no cast or mapping table is needed.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Target: Windows (MSVC) and Linux (GCC/Clang)
 */

#pragma once

#include "engine/core/Types.hpp"   // ::WeatherType (CLEAR/CLOUDY/RAIN/STORM/FOG)

namespace engine {
namespace rendering {

// ---------------------------------------------------------------------------
// TEACHING NOTE — WeatherFxState
// ---------------------------------------------------------------------------
// Plain-data struct that carries all per-frame weather rendering parameters.
// It is computed once per frame by WeatherFx::Update() and passed to the
// sky shader's constant buffer.
//
// All values are in [0, 1] normalised range so the shader can use them
// directly as lerp weights without any additional scaling.
// ---------------------------------------------------------------------------
struct WeatherFxState
{
    float fogDensity    = 0.02f;  ///< 0 = no fog,  1 = fully fogged (scale: 0..1)
    float rainIntensity = 0.0f;   ///< 0 = no rain, 1 = storm-level precipitation
    float cloudCover    = 0.0f;   ///< 0 = clear sky, 1 = full overcast
};

// ---------------------------------------------------------------------------
// WeatherFx — smooth transitions between weather rendering states.
// ---------------------------------------------------------------------------
// TEACHING NOTE — Smooth State Transitions
// ─────────────────────────────────────────
// Rather than snapping fog density to its target value the moment WeatherType
// changes, WeatherFx uses a simple exponential approach-rate lerp:
//
//   current += (target - current) * alpha
//
// where alpha = dt * k_transitionSpeed.  With k_transitionSpeed = 0.5, the
// state reaches ~63% of the target in 2 seconds (half-life ≈ 1.4 s).
// This produces the gradual weather build-up seen in FF15.
//
// The WeatherFx does NOT own a timer or drive its own transitions — it only
// TRANSLATES a WeatherType to render-state.  Timing is controlled externally
// by the SkyRenderer which calls Update(type, dt) each frame.
// ---------------------------------------------------------------------------
class WeatherFx
{
public:
    WeatherFx() = default;

    // -----------------------------------------------------------------------
    // Update — compute render state for the given weather type and dt.
    // Smoothly lerps current state toward the target implied by `type`.
    // WeatherType::FOG is treated as Rain-level precipitation with maximum
    // fog density, since FOG is a gameplay variant that is visually similar.
    // -----------------------------------------------------------------------
    void Update(::WeatherType type, float dt);

    // -----------------------------------------------------------------------
    // GetState — returns the current (smoothly transitioning) weather state.
    // -----------------------------------------------------------------------
    const WeatherFxState& GetState() const { return m_state; }

    // -----------------------------------------------------------------------
    // SetState — force-set the state (useful for tests and hot-reload).
    // -----------------------------------------------------------------------
    void SetState(const WeatherFxState& s) { m_state = s; }

private:
    WeatherFxState m_state;   ///< Current (lerped) render state
};

} // namespace rendering
} // namespace engine
