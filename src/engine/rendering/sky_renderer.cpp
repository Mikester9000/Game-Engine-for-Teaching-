/**
 * @file sky_renderer.cpp
 * @brief SkyRenderer implementation — time-of-day, sun direction, sky colours.
 *
 * ============================================================================
 * TEACHING NOTE — Colour Science for Procedural Skies
 * ============================================================================
 * Real sky colour comes from Rayleigh scattering of sunlight:
 *   • Short wavelengths (blue) scatter more than long wavelengths (red).
 *   • At noon, the sun path through the atmosphere is short → blue sky.
 *   • At sunrise/sunset, the path is long → blue scattered out → orange/red.
 *   • At twilight, scattered blue light illuminates the zenith → purple.
 *
 * We approximate all of this with hand-tuned colour curves that capture the
 * visual appearance without full spectral simulation:
 *
 *   Phase          Zenith colour     Horizon colour
 *   Night          navy (0.02,0.03,0.12)  very dark (0.03,0.05,0.15)
 *   Sunrise/Sunset mauve blend            orange (0.95,0.55,0.20)
 *   Day            sky blue (0.10,0.40,0.90)  pale blue (0.50,0.75,0.95)
 *
 * Production note: FF15 and most modern engines store these curves in a
 * time-of-day LUT (look-up texture) authored in the editor so artists can
 * control the sky without recompiling.  Our implementation hardcodes the
 * curves here; a future M10+ extension would load a tod.lut cooked asset.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 */

#include "engine/rendering/sky_renderer.hpp"
#include <cmath>      // std::sin, std::cos, std::fmod, std::max, std::min
#include <algorithm>  // std::max, std::min (MSVC headers)

namespace engine {
namespace rendering {

// ---------------------------------------------------------------------------
// TEACHING NOTE — Mathematical Constants
// ---------------------------------------------------------------------------
// We define kPi locally rather than using M_PI (which is a POSIX extension
// not guaranteed by the C++ standard on all compilers / with /W4 on MSVC).
// ---------------------------------------------------------------------------
static constexpr float kPi = 3.14159265358979323846f;

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

SkyRenderer::SkyRenderer() = default;

// ---------------------------------------------------------------------------
// Update — advance simulation time and weather FX
// ---------------------------------------------------------------------------

void SkyRenderer::Update(float dt)
{
    // TEACHING NOTE — Time-of-Day Advancement
    // dt is in seconds.  timeScale converts to simulated hours:
    //   simulatedHours = dt × timeScale / 3600
    // With timeScale = 60:  1 s real → 60 s simulated → 60/3600 h = 1/60 h.
    // A full 24-hour cycle completes in 24 × 60 = 1440 real seconds (24 min).
    m_timeOfDay += dt * m_timeScale / 3600.0f;

    // Wrap [0, 24)
    if (m_timeOfDay >= 24.0f)
        m_timeOfDay -= 24.0f;

    // Delegate weather FX update to WeatherFx (smooth lerp toward target).
    m_weatherFx.Update(m_weatherType, dt);
}

// ---------------------------------------------------------------------------
// SetTimeOfDay
// ---------------------------------------------------------------------------

void SkyRenderer::SetTimeOfDay(float t)
{
    m_timeOfDay = std::fmod(t, 24.0f);
    if (m_timeOfDay < 0.0f)
        m_timeOfDay += 24.0f;
}

// ---------------------------------------------------------------------------
// SetWeatherType
// ---------------------------------------------------------------------------

void SkyRenderer::SetWeatherType(WeatherType w)
{
    m_weatherType = w;
}

// ---------------------------------------------------------------------------
// ComputeSunElevation
// ---------------------------------------------------------------------------

float SkyRenderer::ComputeSunElevation() const
{
    // TEACHING NOTE — Sun Elevation Formula
    // elevation = sin(π × (t − 6) / 12)
    //   t = 6  → 0  (horizon, sunrise)
    //   t = 12 → 1  (zenith, noon)
    //   t = 18 → 0  (horizon, sunset)
    //   t = 0  → −1 (deepest night)
    return std::sin(kPi * (m_timeOfDay - 6.0f) / 12.0f);
}

// ---------------------------------------------------------------------------
// GetShaderConstants — compute sky colour + sun + fog for the pixel shader
// ---------------------------------------------------------------------------

SkyShaderConstants SkyRenderer::GetShaderConstants() const
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Sun Direction Computation
    // -----------------------------------------------------------------------
    // We model the sun moving on a great circle through the zenith:
    //   elevation component  → sin(angle)  → sunY
    //   horizontal component → cos(angle)  → sunX  (east in the morning)
    //   depth component      → 0           (sun stays in XY plane)
    //
    // This is a simplified 2D arc — the sun always rises due east and sets
    // due west.  Real sun position calculations also account for latitude,
    // season, and time zone.  For a teaching project the simplification is
    // acceptable and produces good-looking results.
    // -----------------------------------------------------------------------
    const float angle      = kPi * (m_timeOfDay - 6.0f) / 12.0f;
    const float elevation  = std::sin(angle);
    const float sunX       = std::cos(angle);   // east at dawn, west at dusk

    // Intensity: 0 when the sun is below the horizon, elevation when above.
    const float sunIntensity = std::max(0.0f, elevation);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Sky Colour Phases
    // -----------------------------------------------------------------------
    // We define three phases and blend between them:
    //
    //   Night  (elevation < −0.10): dark navy
    //   Day    (elevation > +0.30): bright blue
    //   Transition (elevation in [−0.10, +0.30]): orange/red sunrise-sunset
    //
    // The blend factor sunsetFactor peaks at elevation = 0 (sun on horizon)
    // and fades toward 0 as the sun rises or sets further from the horizon.
    // A width of ±0.30 gives a believable hour-long transition.
    // -----------------------------------------------------------------------

    // Sunset blend: strongest at elevation=0 (sun on horizon)
    const float sunsetWidth  = 0.30f;
    const float rawSunset    = 1.0f - std::min(1.0f, std::abs(elevation) / sunsetWidth);
    // Only show sunset colouring when the sun is not below the night threshold
    const float sunsetFactor = (elevation > -0.10f) ? rawSunset : 0.0f;

    // Night blend: 0 during day, ramps in as the sun goes below horizon
    const float nightFactor = std::max(0.0f, std::min(1.0f, -elevation / 0.20f));

    // -----------------------------------------------------------------------
    // Target colours for each phase
    // -----------------------------------------------------------------------
    // Day sky
    static constexpr float kDayZenR  = 0.10f, kDayZenG  = 0.40f, kDayZenB  = 0.90f;
    static constexpr float kDayHorR  = 0.50f, kDayHorG  = 0.75f, kDayHorB  = 0.95f;
    // Night sky
    static constexpr float kNightZenR= 0.02f, kNightZenG= 0.03f, kNightZenB= 0.12f;
    static constexpr float kNightHorR= 0.03f, kNightHorG= 0.05f, kNightHorB= 0.15f;
    // Sunset/sunrise colouring (zenith tint + horizon orange)
    static constexpr float kSunsetZenR = 0.40f, kSunsetZenG = 0.15f, kSunsetZenB = 0.30f;
    static constexpr float kSunsetHorR = 0.95f, kSunsetHorG = 0.55f, kSunsetHorB = 0.20f;

    // Helper lambda — linear interpolation
    auto lerp = [](float a, float b, float t) -> float
    {
        return a + (b - a) * t;
    };

    // Start with day colours, then blend toward night.
    float zenR = lerp(kDayZenR, kNightZenR, nightFactor);
    float zenG = lerp(kDayZenG, kNightZenG, nightFactor);
    float zenB = lerp(kDayZenB, kNightZenB, nightFactor);

    float horR = lerp(kDayHorR, kNightHorR, nightFactor);
    float horG = lerp(kDayHorG, kNightHorG, nightFactor);
    float horB = lerp(kDayHorB, kNightHorB, nightFactor);

    // TEACHING NOTE — Sunset Tint Layering
    // We apply the sunset/sunrise tint AFTER the night blend.  The sunset
    // zenith gets a subtle mauve/purple tint (sky darkens before turning
    // orange) while the horizon gets a strong orange.
    // sunsetFactor already accounts for the sun being above −0.1 elevation.
    zenR = lerp(zenR, kSunsetZenR, sunsetFactor * 0.35f);
    zenG = lerp(zenG, kSunsetZenG, sunsetFactor * 0.35f);
    zenB = lerp(zenB, kSunsetZenB, sunsetFactor * 0.35f);

    horR = lerp(horR, kSunsetHorR, sunsetFactor * 0.75f);
    horG = lerp(horG, kSunsetHorG, sunsetFactor * 0.75f);
    horB = lerp(horB, kSunsetHorB, sunsetFactor * 0.75f);

    // TEACHING NOTE — Cloud Cover Darkening
    // A fully overcast sky suppresses sun-related colouring and reduces
    // overall brightness (like a thin white sheet over the sun).
    // We reduce saturation as well by lerping toward a grey midpoint.
    const WeatherFxState& wfx = m_weatherFx.GetState();
    const float cloudDarken   = 1.0f - wfx.cloudCover * 0.45f;
    const float greyMidR = 0.55f, greyMidG = 0.58f, greyMidB = 0.62f;

    zenR = lerp(zenR * cloudDarken, greyMidR, wfx.cloudCover * 0.5f);
    zenG = lerp(zenG * cloudDarken, greyMidG, wfx.cloudCover * 0.5f);
    zenB = lerp(zenB * cloudDarken, greyMidB, wfx.cloudCover * 0.5f);

    horR = lerp(horR * cloudDarken, greyMidR, wfx.cloudCover * 0.6f);
    horG = lerp(horG * cloudDarken, greyMidG, wfx.cloudCover * 0.6f);
    horB = lerp(horB * cloudDarken, greyMidB, wfx.cloudCover * 0.6f);

    // -----------------------------------------------------------------------
    // Fog colour and density
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Fog Colour
    // In clear weather fog picks up the horizon colour (natural atmospheric
    // haze).  In rain the fog shifts toward a neutral cool grey.
    float fogR, fogG, fogB, fogDensity;

    if (wfx.rainIntensity > 0.1f)
    {
        // TEACHING NOTE — Rain fog is a cool, desaturated grey.
        // Real rain scatters all wavelengths equally → neutral grey.
        fogR = lerp(horR, 0.68f, wfx.rainIntensity);
        fogG = lerp(horG, 0.70f, wfx.rainIntensity);
        fogB = lerp(horB, 0.73f, wfx.rainIntensity);
        fogDensity = 0.10f + wfx.fogDensity * 0.90f;
    }
    else
    {
        // Clear/cloudy: fog matches the horizon colour for seamless blending.
        fogR       = horR;
        fogG       = horG;
        fogB       = horB;
        fogDensity = wfx.fogDensity;
    }

    // -----------------------------------------------------------------------
    // Pack into SkyShaderConstants
    // -----------------------------------------------------------------------
    SkyShaderConstants c{};

    c.sunDirX      = sunX;
    c.sunDirY      = elevation;
    c.sunDirZ      = 0.0f;
    c.sunIntensity = sunIntensity;

    c.zenithR  = zenR;
    c.zenithG  = zenG;
    c.zenithB  = zenB;
    c._pad0    = 0.0f;

    c.horizonR = horR;
    c.horizonG = horG;
    c.horizonB = horB;
    c._pad1    = 0.0f;

    c.fogR       = fogR;
    c.fogG       = fogG;
    c.fogB       = fogB;
    c.fogDensity = fogDensity;

    c.rainIntensity = wfx.rainIntensity;
    c.cloudCover    = wfx.cloudCover;
    c.timeOfDay     = m_timeOfDay;
    c._pad2         = 0.0f;

    return c;
}

} // namespace rendering
} // namespace engine
