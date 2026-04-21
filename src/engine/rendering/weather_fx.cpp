/**
 * @file weather_fx.cpp
 * @brief WeatherFx implementation — fog, rain, and cloud cover transitions.
 *
 * ============================================================================
 * TEACHING NOTE — Approach-Rate Lerp for Weather Transitions
 * ============================================================================
 * A fixed percentage approach-rate lerp is one of the simplest and most
 * readable ways to smooth a state transition:
 *
 *   current += (target - current) * alpha
 *
 * Properties:
 *   • Never overshoots the target (assuming alpha ∈ [0, 1]).
 *   • Frame-rate independent when alpha = 1 - exp(-k * dt).
 *
 * For brevity we use alpha = min(1, dt * 0.5) which is close enough to the
 * exact exponential for educational purposes and avoids the exp() call.
 * In a production engine you would use the exact form to be truly
 * frame-rate independent.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 */

#include "engine/rendering/weather_fx.hpp"
#include <algorithm>  // std::min

namespace engine {
namespace rendering {

// ---------------------------------------------------------------------------
// Target fog/rain/cloud values for each WeatherType.
// ---------------------------------------------------------------------------
// TEACHING NOTE — Data-Driven Weather Parameters
// ─────────────────────────────────────────────────
// Defining target values as local constants (rather than switch literals)
// makes it easy to tune them without digging into logic code.  A production
// engine would load these from a JSON "weather profile" asset baked by the
// tools pipeline, so designers can tune weather without recompiling.
// ---------------------------------------------------------------------------
static constexpr float kClearFog    = 0.02f;   // Barely perceptible horizon haze
static constexpr float kCloudyFog   = 0.08f;   // Light mist in distance
static constexpr float kRainFog     = 0.30f;   // Noticeable fog curtain
static constexpr float kStormFog    = 0.55f;   // Heavy fog, 50% visibility

static constexpr float kClearRain   = 0.00f;
static constexpr float kCloudyRain  = 0.00f;
static constexpr float kRainRain    = 0.60f;   // Moderate rainfall
static constexpr float kStormRain   = 1.00f;   // Maximum rainfall

static constexpr float kClearCloud  = 0.00f;   // Fully clear sky
static constexpr float kCloudyCloud = 0.65f;   // Scattered-to-broken overcast
static constexpr float kRainCloud   = 0.90f;   // Mostly overcast
static constexpr float kStormCloud  = 1.00f;   // Complete overcast

// ---------------------------------------------------------------------------
// WeatherFx::Update
// ---------------------------------------------------------------------------

void WeatherFx::Update(::WeatherType type, float dt)
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Target State Selection
    // -----------------------------------------------------------------------
    // We pick the target fog/rain/cloud values for the current WeatherType
    // and then lerp the current state toward them each frame.
    // WeatherType::FOG (gameplay visibility reduction) is treated as
    // Rain-level precipitation with maximum fog density for rendering
    // purposes — dense fog without heavy rain matches FF15's misty dungeon look.
    // -----------------------------------------------------------------------
    float targetFog   = kClearFog;
    float targetRain  = kClearRain;
    float targetCloud = kClearCloud;

    switch (type)
    {
    case WeatherType::CLEAR:
        targetFog   = kClearFog;
        targetRain  = kClearRain;
        targetCloud = kClearCloud;
        break;

    case WeatherType::CLOUDY:
        targetFog   = kCloudyFog;
        targetRain  = kCloudyRain;
        targetCloud = kCloudyCloud;
        break;

    case WeatherType::RAIN:
        targetFog   = kRainFog;
        targetRain  = kRainRain;
        targetCloud = kRainCloud;
        break;

    case WeatherType::STORM:
        targetFog   = kStormFog;
        targetRain  = kStormRain;
        targetCloud = kStormCloud;
        break;

    case WeatherType::FOG:
        // TEACHING NOTE — FOG as a rendering state
        // FOG is a gameplay type (halves enemy detection range).
        // Visually we render it as maximum fog density + light rain,
        // similar to heavy cloud cover without the full storm.
        targetFog   = kStormFog;
        targetRain  = kCloudyRain;
        targetCloud = kRainCloud;
        break;
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Approach-Rate Lerp
    // -----------------------------------------------------------------------
    // alpha controls how fast we transition.  alpha = dt * 0.5 means the
    // state moves ~50% of the remaining distance per second — reaching
    // ~75% of the target in 2 s, and visually "fully transitioned" in ~6 s.
    //
    // std::min(1.0f, ...) clamps to [0,1] so a large dt (e.g. the first
    // frame after a long load) does not overshoot the target.
    // -----------------------------------------------------------------------
    const float alpha = std::min(1.0f, dt * 0.5f);

    m_state.fogDensity    += (targetFog   - m_state.fogDensity)    * alpha;
    m_state.rainIntensity += (targetRain  - m_state.rainIntensity) * alpha;
    m_state.cloudCover    += (targetCloud - m_state.cloudCover)    * alpha;
}

} // namespace rendering
} // namespace engine
