/**
 * @file sky_renderer.hpp
 * @brief Procedural sky renderer — time-of-day, sun direction, atmospheric colours.
 *
 * ============================================================================
 * TEACHING NOTE — Procedural Sky in Modern Game Engines
 * ============================================================================
 * FF15's Luminous Engine uses a physical sky model derived from Preetham et al.
 * ("A Practical Analytic Model for Daylight", SIGGRAPH 1999).  Our
 * implementation is a simplified version that captures the key visual
 * qualities without the full spectral math:
 *
 *   1. GRADIENT SKY — Zenith-to-horizon colour transition based on time of day.
 *      Day: blue zenith → lighter cyan/white horizon.
 *      Sunset/Sunrise: orange/red horizon, purple zenith.
 *      Night: dark navy throughout.
 *
 *   2. SUN DISC — A bright disc at the sun's screen position.  In the pixel
 *      shader a pow() function sharpens the Gaussian falloff to a crisp disc.
 *
 *   3. ATMOSPHERIC SCATTERING APPROXIMATION — At sunrise/sunset the sun is
 *      low on the horizon and its light travels through more atmosphere.
 *      We approximate this by shifting the horizon colour toward orange/red
 *      when the sun elevation is near 0.
 *
 *   4. WEATHER INTEGRATION — WeatherFx adds fog, rain darkening, and cloud
 *      cover on top of the base sky colour.
 *
 * ============================================================================
 * TEACHING NOTE — SkyShaderConstants Layout
 * ============================================================================
 * This struct is memcpy'd directly into a D3D11 constant buffer so its
 * layout MUST match the HLSL cbuffer declaration in sky.ps.hlsl.
 *
 * D3D11 constant buffer rules:
 *   • Each member is 16-byte aligned (GPU operates on float4 registers).
 *   • We group members into float4 blocks to avoid hidden padding.
 *   • Total size must be a multiple of 16 bytes.
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

#include "engine/rendering/weather_fx.hpp"

namespace engine {
namespace rendering {

// ---------------------------------------------------------------------------
// TEACHING NOTE — SkyShaderConstants
// ---------------------------------------------------------------------------
// Every field maps to a float4 in the HLSL cbuffer.  We pack logically
// related values together to minimise wasted padding:
//
//   b0 (sky.ps.hlsl register b0):
//   float4 g_sunDir       = { sunDirX, sunDirY, sunDirZ, sunIntensity  }
//   float4 g_zenithColor  = { zenithR, zenithG, zenithB, 0.0           }
//   float4 g_horizonColor = { horizonR, horizonG, horizonB, 0.0        }
//   float4 g_fogColor     = { fogR, fogG, fogB, fogDensity             }
//   float4 g_weatherFx    = { rainIntensity, cloudCover, timeOfDay, 0.0 }
//
// sizeof = 5 × 16 = 80 bytes.
// ---------------------------------------------------------------------------
struct SkyShaderConstants
{
    // --- float4 block 0: sun direction + intensity ---
    float sunDirX       = 0.0f;   ///< World-space sun direction X
    float sunDirY       = 1.0f;   ///< World-space sun direction Y (up = noon)
    float sunDirZ       = 0.0f;   ///< World-space sun direction Z
    float sunIntensity  = 1.0f;   ///< 0 = below horizon / night, 1 = noon

    // --- float4 block 1: zenith sky colour ---
    float zenithR       = 0.1f;
    float zenithG       = 0.4f;
    float zenithB       = 0.9f;
    float _pad0         = 0.0f;

    // --- float4 block 2: horizon sky colour ---
    float horizonR      = 0.5f;
    float horizonG      = 0.75f;
    float horizonB      = 0.95f;
    float _pad1         = 0.0f;

    // --- float4 block 3: fog colour + density ---
    float fogR          = 0.5f;
    float fogG          = 0.75f;
    float fogB          = 0.95f;
    float fogDensity    = 0.02f;  ///< 0 = clear, 1 = fog soup

    // --- float4 block 4: weather FX state ---
    float rainIntensity = 0.0f;   ///< 0 = dry, 1 = heavy storm rain
    float cloudCover    = 0.0f;   ///< 0 = clear, 1 = full overcast
    float timeOfDay     = 8.0f;   ///< Current time-of-day 0..24 h (for debug overlay)
    float _pad2         = 0.0f;
};

// Compile-time guard: sizeof must be a multiple of 16 for D3D11 CB alignment.
static_assert(sizeof(SkyShaderConstants) % 16 == 0,
    "SkyShaderConstants must be 16-byte aligned for HLSL constant buffer");

// ---------------------------------------------------------------------------
// SkyRenderer — drives the procedural sky simulation.
// ---------------------------------------------------------------------------
// TEACHING NOTE — Responsibilities
// ─────────────────────────────────────────────────────────────────────────
// SkyRenderer is a PURE CPU object.  It:
//   1. Advances the time-of-day clock each frame (Update).
//   2. Computes the sun's world-space direction from the time.
//   3. Interpolates zenith/horizon colours based on sun elevation.
//   4. Delegates fog/rain/cloud computations to WeatherFx.
//   5. Packs everything into SkyShaderConstants for the GPU.
//
// SkyRenderer knows NOTHING about D3D11, Vulkan, or any GPU resource.
// The D3D11Renderer calls GetShaderConstants() each frame and uploads the
// result to its sky constant buffer — the renderer owns the GPU side.
//
// This design allows SkyRenderer to be tested purely on the CPU (no GPU
// needed) and reused by any future renderer backend.
//
// ============================================================================
// TEACHING NOTE — Sun Direction Model
// ============================================================================
// We use a simplified solar model with no azimuth variation (the sun always
// rises due east and sets due west in our world).
//
//   timeOfDay in [0, 24) hours.
//   elevation = sin(π × (t − 6) / 12)
//
//   t = 0  → elevation = sin(-π/2) = −1.00  midnight, sun deep below horizon
//   t = 6  → elevation = sin(0)    = +0.00  sunrise, sun on horizon
//   t = 12 → elevation = sin(+π/2) = +1.00  noon, sun directly overhead
//   t = 18 → elevation = sin(+π)   = +0.00  sunset, sun on horizon again
//   t = 24 → elevation = sin(+3π/2) = −1.00 midnight
//
// The horizontal component follows the same arc:
//   sunX = cos(π × (t − 6) / 12)  (positive at dawn, negative at dusk)
//
// Both form a unit circle so the sun direction is always normalised.
// ---------------------------------------------------------------------------
class SkyRenderer
{
public:
    SkyRenderer();

    // -----------------------------------------------------------------------
    // Update — advance the time-of-day and weather state by dt seconds.
    //
    // TEACHING NOTE — Time Scale Compression
    // A real day is 86 400 seconds.  The default timeScale of 60 means
    // 1 real second = 60 simulated seconds, giving a 24-minute day/night
    // cycle — the same compression factor used by FF15's field areas.
    // -----------------------------------------------------------------------
    void Update(float dt);

    // -----------------------------------------------------------------------
    // SetTimeOfDay — force the clock to a specific hour (0..24).
    // Wraps around if t ≥ 24.
    // -----------------------------------------------------------------------
    void SetTimeOfDay(float t);

    // -----------------------------------------------------------------------
    // SetWeatherType — override the weather state.
    // The weather type itself is changed immediately; the visual WeatherFx
    // state (fog density, rain intensity, cloud cover) is still smoothly
    // blended toward its target in subsequent Update() calls so transitions
    // remain gradual rather than snapping.
    // -----------------------------------------------------------------------
    void SetWeatherType(WeatherType w);

    // -----------------------------------------------------------------------
    // GetTimeOfDay — current simulation time in hours [0, 24).
    // -----------------------------------------------------------------------
    float GetTimeOfDay() const { return m_timeOfDay; }

    // -----------------------------------------------------------------------
    // GetWeatherType — current weather type.
    // -----------------------------------------------------------------------
    WeatherType GetWeatherType() const { return m_weatherType; }

    // -----------------------------------------------------------------------
    // GetShaderConstants — pack the current sky state into a struct suitable
    // for direct memcpy to a D3D11 constant buffer.
    // -----------------------------------------------------------------------
    SkyShaderConstants GetShaderConstants() const;

private:
    float       m_timeOfDay   = 8.0f;              ///< Simulation time 0..24 h
    float       m_timeScale   = 60.0f;             ///< Real-to-sim compression
    WeatherType m_weatherType = WeatherType::CLEAR;
    WeatherFx   m_weatherFx;

    // Internal helper: compute sun elevation (−1..+1) from time-of-day.
    float ComputeSunElevation() const;
};

} // namespace rendering
} // namespace engine
