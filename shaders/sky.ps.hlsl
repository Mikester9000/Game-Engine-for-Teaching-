/**
 * sky.ps.hlsl
 * Pixel shader for the procedural sky — gradient, sun disc, fog, weather effects.
 *
 * ============================================================================
 * TEACHING NOTE — Sky Rendering Overview
 * ============================================================================
 * This shader implements a simplified but visually convincing procedural sky
 * that captures the same visual classes of effect as FF15's sky system:
 *
 *   1. ZENITH–HORIZON GRADIENT
 *      The sky transitions from a darker zenith colour (top) to a brighter
 *      horizon colour (bottom).  The gradient uses an exponential curve rather
 *      than a linear lerp because the natural atmosphere thins quickly above
 *      the horizon — the blue band near the horizon is denser than at the top.
 *
 *   2. SUN DISC
 *      A bright circle at the sun's screen-space position.  We project the
 *      sun world direction onto 2D screen space and use the distance from
 *      the current pixel to that position.  Two terms:
 *        • sunDisc  — sharp core disc (narrow Gaussian peak)
 *        • sunGlow  — wide soft halo around the disc
 *
 *   3. ATMOSPHERIC SCATTERING APPROXIMATION
 *      At sunrise/sunset the sun is low and its light travels through more
 *      atmosphere.  The CPU-side SkyRenderer shifts the horizon colour toward
 *      orange/red in those phases, so the shader just reads the pre-computed
 *      colour from the constant buffer rather than re-doing the math.
 *
 *   4. FOG OVERLAY
 *      Fog density increases toward the horizon (uv.y → 1).  We lerp toward
 *      the fog colour with an exponential blend that mirrors how real fog
 *      accumulates over distance.
 *
 *   5. WEATHER FX
 *      Rain darkens the sky (light is absorbed by water droplets).  Cloud
 *      cover is already baked into the zenith/horizon colours by the CPU.
 *
 *   6. TONEMAP + GAMMA
 *      The sky can be brighter than 1.0 (especially near the sun).  We apply
 *      Reinhard tonemap (x / (x+1)) to keep everything in [0,1], then
 *      sRGB gamma (^ 1/2.2) to convert from linear to display space.
 *
 * ============================================================================
 * TEACHING NOTE — Constant Buffer Layout (must match SkyShaderConstants)
 * ============================================================================
 * Packed as five float4 blocks (80 bytes total):
 *
 *   b0:
 *   float4 g_sunDir       = { sunDirX, sunDirY, sunDirZ, sunIntensity  }
 *   float4 g_zenithColor  = { r, g, b, 0 }
 *   float4 g_horizonColor = { r, g, b, 0 }
 *   float4 g_fogColor     = { r, g, b, fogDensity }
 *   float4 g_weatherFx    = { rainIntensity, cloudCover, timeOfDay, 0 }
 *
 * ============================================================================
 * TEACHING NOTE — Shader Model 4.0 (Feature Level 10_0 Compatible)
 * ============================================================================
 * Target: ps_4_0
 * Minimum hardware: GeForce GT 610 / D3D_FEATURE_LEVEL_10_0.
 * ============================================================================
 */

// ---------------------------------------------------------------------------
// Constant buffer — uploaded from SkyShaderConstants in sky_renderer.hpp.
// ---------------------------------------------------------------------------
// TEACHING NOTE — cbuffer register(b0)
// The sky constant buffer is the only constant buffer this shader uses.
// It occupies register b0 (the default slot).  The D3D11Renderer calls
// PSSetConstantBuffers(0, 1, &skyConstantsCB) to bind it before Draw().
cbuffer SkyCB : register(b0)
{
    float4 g_sunDir;        ///< xyz = sun world direction, w = intensity (0..1)
    float4 g_zenithColor;   ///< xyz = zenith sky colour, w = unused
    float4 g_horizonColor;  ///< xyz = horizon sky colour, w = unused
    float4 g_fogColor;      ///< xyz = fog colour, w = fog density (0..1)
    float4 g_weatherFx;     ///< x = rainIntensity, y = cloudCover, z = timeOfDay
};

// ---------------------------------------------------------------------------
// Input from the vertex shader.
// ---------------------------------------------------------------------------
struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;   ///< (0,0) = top-left, (1,1) = bottom-right
};

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
float4 main(PSInput i) : SV_TARGET
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Vertical Gradient Factor
    // -----------------------------------------------------------------------
    // uv.y = 0 at the top of the screen (zenith) and 1 at the bottom (horizon).
    //
    // A linear lerp produces an even gradient; an exponential one models the
    // fact that the colour change is rapid near the horizon (dense lower
    // atmosphere) and slow near the zenith (thin upper atmosphere):
    //
    //   gradFactor = 1 - exp(-t * k)
    //
    // With k = 3.0, gradFactor reaches 0.95 at t = 1.0 (full horizon),
    // and 0.26 at t = 0.1 (just below zenith) — a steep bottom curve.
    // -----------------------------------------------------------------------
    float t = i.uv.y;  // 0 = zenith, 1 = horizon
    float gradFactor = 1.0f - exp(-t * 3.0f);

    float3 skyColor = lerp(g_zenithColor.xyz, g_horizonColor.xyz, gradFactor);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Sun Disc Projection
    // -----------------------------------------------------------------------
    // The sun direction (g_sunDir.xyz) is a world-space unit vector.  We need
    // to project it onto the 2D screen to find the sun's pixel position.
    //
    // SIMPLIFIED PROJECTION for a standalone sky scene (no camera matrix):
    //   sunX maps east-west (world X) to screen X (negative because D3D11
    //     NDC X increases right while our sky "east" is positive X).
    //   sunY maps the elevation (world Y) to screen Y (negative because
    //     D3D11 NDC Y increases up but UV Y increases down).
    //
    // In a full game implementation the sun direction would be projected
    // through the camera's view-projection matrix.  For the standalone
    // dynamic_sky demo scene this simplified 2D projection is sufficient.
    // -----------------------------------------------------------------------

    // Convert uv [0,1] to centred NDC [-1,+1].
    float2 centeredUV = i.uv * 2.0f - 1.0f;   // (0,0) to (-1,-1); (1,1) to (+1,+1)

    // Project sun direction to 2D screen-space position.
    // sunDirX: east-west (positive = east = screen right = positive centeredUV.x).
    // sunDirY: elevation (positive = up = screen top = negative centeredUV.y because
    //          centeredUV.y is POSITIVE downward in our UV convention).
    float2 sunScreenPos = float2(g_sunDir.x, -g_sunDir.y);

    // Distance from this pixel to the sun centre.
    float sunDist = length(centeredUV - sunScreenPos);

    // TEACHING NOTE — Two-Term Sun Glow
    // Term 1 — sunDisc: a tight bright disc (narrow falloff).
    //   saturate(1 - dist * 15) clips to 0 outside a small radius and
    //   ramps steeply from 0→1 toward the centre.
    //   We multiply by sunIntensity so the disc disappears at night.
    float sunDisc = saturate(1.0f - sunDist * 15.0f) * g_sunDir.w;

    // Term 2 — sunGlow: a wider soft halo (corona effect).
    //   The halo extends ~3× farther than the disc core.
    float sunGlow = saturate(1.0f - sunDist * 4.0f) * g_sunDir.w * 0.35f;

    // Sun colour: warm orange near the horizon (low intensity), white at noon.
    // lerp(orange, white, sunIntensity) gives a physically plausible shift.
    float3 sunColor = lerp(float3(1.0f, 0.55f, 0.15f),
                           float3(1.3f, 1.2f,  1.0f),
                           g_sunDir.w);

    skyColor += sunColor * (sunDisc + sunGlow);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Fog Overlay (Horizon Haze)
    // -----------------------------------------------------------------------
    // Fog accumulates toward the horizon (uv.y → 1).  We use an exponential
    // ramp driven by fogDensity:
    //
    //   fogFactor = saturate(uv.y * fogDensity * fogScaleK)
    //
    // fogScaleK = 5 means full fog is reached at uv.y = 1 / (fogDensity × 5).
    // With fogDensity = 0.02 (clear day): full fog at uv.y = 10 — never reached.
    // With fogDensity = 0.55 (storm):     full fog at uv.y ≈ 0.36 (mid-sky).
    // -----------------------------------------------------------------------
    float fogFactor = saturate(t * g_fogColor.w * 5.0f);
    skyColor = lerp(skyColor, g_fogColor.xyz, fogFactor);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Rain Darkening
    // -----------------------------------------------------------------------
    // Rain reduces the overall sky brightness: water droplets absorb and
    // scatter light in all directions, lowering the luminance reaching the
    // viewer.  A linear 30% reduction at maximum rain intensity is enough
    // to create a moody storm atmosphere without making the sky pitch-black.
    // -----------------------------------------------------------------------
    float rainDarken = 1.0f - g_weatherFx.x * 0.30f;
    skyColor *= rainDarken;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Reinhard Tonemap
    // -----------------------------------------------------------------------
    // The sun disc and glow can push channel values above 1.0.  Without
    // tonemapping this causes colour channels to clip (blow out), losing all
    // detail near the sun.  Reinhard's formula:
    //
    //   out = in / (in + 1)
    //
    // maps [0, ∞) to [0, 1) smoothly:  low values are barely changed,
    // high values are compressed toward 1.  Simple, cheap, and good-looking.
    // A production engine would use ACES or GT Tonemap here.
    // -----------------------------------------------------------------------
    skyColor = skyColor / (skyColor + 1.0f);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — sRGB Gamma Correction
    // -----------------------------------------------------------------------
    // Monitors display in sRGB (gamma ≈ 2.2).  Our lighting math is done in
    // linear space.  We apply the inverse gamma (^(1/2.2)) to convert from
    // linear to the non-linear sRGB space the monitor expects.
    //
    // Without gamma correction colours appear too dark in the mid-tones.
    // pow() is applied component-wise; max(0) avoids negative inputs.
    // -----------------------------------------------------------------------
    skyColor = pow(max(skyColor, float3(0.0f, 0.0f, 0.0f)), 1.0f / 2.2f);

    return float4(skyColor, 1.0f);
}
