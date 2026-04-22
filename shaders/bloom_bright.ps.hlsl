/**
 * bloom_bright.ps.hlsl
 * Pixel shader — bright-pass extraction for the bloom effect (M17).
 *
 * ============================================================================
 * TEACHING NOTE — What Is Bloom?
 * ============================================================================
 * Bloom simulates the way bright light bleeds into surrounding areas due to
 * lens scattering and optical aberration in a camera (or in the human eye).
 * It makes light sources and reflections appear to glow.
 *
 * The bloom pipeline consists of four passes:
 *
 *   1. Bright-pass (THIS FILE): extract pixels brighter than a threshold.
 *      Pixels below the threshold become black.  The result is an image
 *      containing only the "emissive" parts of the scene.
 *
 *   2. Horizontal Gaussian blur (bloom_blur.ps.hlsl):
 *      Spread each bright pixel horizontally.
 *
 *   3. Vertical Gaussian blur (bloom_blur.ps.hlsl):
 *      Spread the result vertically, creating a 2D Gaussian kernel.
 *
 *   4. Composite (bloom_composite.ps.hlsl):
 *      Add the blurred bright image on top of the original scene.
 *
 * ============================================================================
 * TEACHING NOTE — Luminance Threshold
 * ============================================================================
 * The "brightness" of a pixel is measured by its PERCEPTUAL LUMINANCE, not
 * its simple average.  Human vision is most sensitive to green and least to
 * blue, so the ITU-R BT.709 luminance coefficients are:
 *
 *   L = 0.2126 × R + 0.7152 × G + 0.0722 × B
 *
 * Using perceptual luminance gives a more natural bloom than using average
 * brightness, which would over-bloom warm colours (heavy in red).
 *
 * Typical threshold: 0.7 – 0.9 (linear brightness scale).
 * Values below the threshold → black (no bloom contribution).
 * Values above the threshold → passed through to the blur pass.
 *
 * ============================================================================
 * TEACHING NOTE — Full-Screen Triangle Trick (VS Not In This File)
 * ============================================================================
 * The VS for all bloom passes is sky.vs.hlsl (the SV_VertexID full-screen
 * triangle, see that file for details).  sky.vs.hlsl outputs:
 *   pos  : SV_POSITION  — clip-space position of the full-screen triangle
 *   uv   : TEXCOORD0    — screen UV (0,0 = top-left; 1,1 = bottom-right)
 *
 * Reusing the same VS avoids duplicating the full-screen triangle logic
 * and demonstrates the general-purpose nature of the SV_VertexID trick.
 *
 * Shader Model: ps_4_0  (D3D_FEATURE_LEVEL_10_0 compatible)
 */

// ---------------------------------------------------------------------------
// Inputs from sky.vs.hlsl
// ---------------------------------------------------------------------------
Texture2D    g_scene : register(t0);  ///< Scene render target (HDR-range colour)
SamplerState g_smp   : register(s0);  ///< Linear-clamp sampler

// ---------------------------------------------------------------------------
// Constant buffer b0 — bright-pass parameters
// ---------------------------------------------------------------------------
// TEACHING NOTE — Luminance Threshold in a Constant Buffer
// Placing the threshold in a CB makes it trivial to adjust at runtime without
// recompiling the shader.  In a production engine this allows the artist to
// tune bloom intensity per-scene or per-cutscene.
// ---------------------------------------------------------------------------
cbuffer BloomCB : register(b0)
{
    float  g_threshold;  ///< Luminance threshold: pixels below this become black
    float3 g_pad;        ///< Padding to 16-byte CB alignment (required by D3D11)
};

// ---------------------------------------------------------------------------
// main — luminance-based bright-pass extraction
// ---------------------------------------------------------------------------
float4 main(float4 pos : SV_POSITION,
            float2 uv  : TEXCOORD0) : SV_TARGET
{
    // Sample the scene colour at the current screen UV.
    float4 c = g_scene.Sample(g_smp, uv);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Perceptual Luminance (ITU-R BT.709)
    // -----------------------------------------------------------------------
    // Convert the RGB colour to a single perceptual brightness value.
    // We use the BT.709 standard coefficients which model the sensitivity of
    // cone cells in the human retina.  Green dominates (~72%) because our
    // eyes are most sensitive to green light.
    // -----------------------------------------------------------------------
    float lum = dot(c.rgb, float3(0.2126f, 0.7152f, 0.0722f));

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Threshold vs. Knee Curve
    // -----------------------------------------------------------------------
    // A hard threshold (lum > threshold → pass, else → black) works but can
    // cause flickering when a pixel's luminance oscillates around the threshold.
    // A production engine uses a "soft knee" (smooth curve) to avoid this.
    // For the teaching demo a hard threshold is clearer and sufficient.
    // -----------------------------------------------------------------------
    if (lum > g_threshold)
        return float4(c.rgb, 1.0f);  // pass through bright pixel
    else
        return float4(0.0f, 0.0f, 0.0f, 1.0f);  // black out dim pixel
}
