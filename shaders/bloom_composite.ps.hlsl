/**
 * bloom_composite.ps.hlsl
 * Pixel shader — additive bloom composite + Reinhard tonemap (M17).
 *
 * ============================================================================
 * TEACHING NOTE — Additive Compositing
 * ============================================================================
 * The final bloom step additively blends the blurred bright image (g_bloom)
 * on top of the original scene (g_scene):
 *
 *   result = scene + bloom × bloomStrength
 *
 * This additive blend models how light physically accumulates — each photon
 * adds to, rather than replaces, the ones already present.  Multiplication by
 * bloomStrength gives the artist a linear control over how intense the glow
 * appears, from 0.0 (no bloom) to >1.0 (very intense).
 *
 * ============================================================================
 * TEACHING NOTE — Reinhard Tonemapping
 * ============================================================================
 * After adding bloom, the combined RGB values may exceed 1.0 — this is
 * expected when working with HDR (High Dynamic Range) scene content.
 * Tonemapping compresses the HDR range into the displayable LDR [0, 1].
 *
 * Reinhard tonemap: out = in / (in + 1)
 *
 * Properties:
 *   • At in = 0:   out = 0           (black stays black)
 *   • At in = 1:   out = 0.5         (middle grey becomes ~50% brightness)
 *   • As in → ∞:   out → 1           (very bright values approach white)
 *   • Monotonically increasing — no colour inversions
 *
 * Applied per channel, Reinhard naturally rolls off bright colours into
 * saturation without clipping — preventing the harsh "burning" artefact
 * of a simple saturate() call.
 *
 * ============================================================================
 * TEACHING NOTE — Where To Tonemap
 * ============================================================================
 * Tonemapping belongs in the LAST rendering pass, AFTER all additive effects
 * (bloom, lens flares, atmospheric fog) have been composited.  If you tonemap
 * the scene BEFORE bloom, the bright bloom values are already compressed to
 * near-white and the additive blend produces incorrect over-bright results.
 *
 * Execution order in the bloom pipeline:
 *   1. Render scene → scene RT         (linear HDR values)
 *   2. Bright-pass                     (still linear HDR)
 *   3. Blur X + Y                      (still linear HDR)
 *   4. Composite: scene + bloom        (HDR sum)
 *   5. Tonemap (THIS SHADER)           (compress to [0,1] for display)
 *
 * Shader Model: ps_4_0  (D3D_FEATURE_LEVEL_10_0 compatible)
 */

// ---------------------------------------------------------------------------
// Input textures
// ---------------------------------------------------------------------------
// TEACHING NOTE — Two Inputs: Scene and Bloom
// We bind two SRVs to the composite pass:
//   t0 — the original scene render target (before bloom was applied)
//   t1 — the vertically-blurred bright-pass result (the "bloom layer")
//
// Using the ORIGINAL scene (not the bright-pass) as the base ensures that
// dim regions (below threshold) still appear with their correct colour —
// the bright-pass image is black in those regions.
// ---------------------------------------------------------------------------
Texture2D    g_scene : register(t0);  ///< Original scene render target
Texture2D    g_bloom : register(t1);  ///< Blurred bloom texture (final blur pass output)
SamplerState g_smp   : register(s0);  ///< Linear-clamp sampler (shared)

// ---------------------------------------------------------------------------
// Constant buffer b0 — composite parameters
// ---------------------------------------------------------------------------
cbuffer CompCB : register(b0)
{
    float  g_bloomStrength;  ///< Artist-controlled bloom intensity (typical: 0.5–1.5)
    float3 g_pad;            ///< Padding to 16-byte CB alignment
};

// ---------------------------------------------------------------------------
// main — additive composite + Reinhard tonemap
// ---------------------------------------------------------------------------
float4 main(float4 pos : SV_POSITION,
            float2 uv  : TEXCOORD0) : SV_TARGET
{
    // Sample scene and bloom textures at the current screen UV.
    float3 scene = g_scene.Sample(g_smp, uv).rgb;
    float3 bloom = g_bloom.Sample(g_smp, uv).rgb;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Additive Blend
    // -----------------------------------------------------------------------
    // Add the scaled bloom on top of the scene.  The bloom texture is already
    // low-frequency (blurred), so bright edges spread naturally without
    // hard outlines.  g_bloomStrength > 1.0 amplifies the effect; < 1.0 tones
    // it down.  A value of 0.0 disables bloom entirely.
    // -----------------------------------------------------------------------
    float3 hdr = scene + bloom * g_bloomStrength;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Per-Channel Reinhard Tonemap
    // -----------------------------------------------------------------------
    // Apply Reinhard independently per channel.  This can slightly desaturate
    // very bright colours (e.g., a bright red becomes slightly more orange)
    // because R compresses to 1 faster than G and B.  Luminance-based Reinhard
    // avoids desaturation but is slightly more complex; per-channel is the
    // teachable baseline.
    // -----------------------------------------------------------------------
    float3 ldr = hdr / (hdr + 1.0f);

    return float4(ldr, 1.0f);
}
