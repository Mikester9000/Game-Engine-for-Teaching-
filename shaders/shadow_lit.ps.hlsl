/**
 * shadow_lit.ps.hlsl
 * Pixel shader for the lit pass of the shadow demo: Lambert + PCF shadows (M17).
 *
 * ============================================================================
 * TEACHING NOTE — Percentage-Closer Filtering (PCF)
 * ============================================================================
 * A naïve shadow test compares one depth sample:
 *
 *   lit = (surfaceDepth <= shadowMap.Sample(uv)) ? 1.0 : 0.0;
 *
 * This produces hard-edged "binary" shadows with severe aliasing at the
 * shadow boundary.  Percentage-Closer Filtering (PCF) blurs the boundary by:
 *
 *   1. Sampling the shadow map at N nearby texels (e.g., a 3×3 grid).
 *   2. Performing a SEPARATE depth comparison at each sample.
 *   3. Averaging the N binary results.
 *
 * The key insight: we blur the COMPARISON RESULTS, not the depth values.
 * Averaging depth values before comparison would give wrong occlusion.
 * Averaging binary comparison results gives a fractional shadow value in
 * [0, 1] that smoothly fades at the shadow boundary.
 *
 * ============================================================================
 * TEACHING NOTE — SamplerComparisonState vs SamplerState
 * ============================================================================
 * D3D11 provides a special sampler type, SamplerComparisonState, that
 * performs a depth comparison as part of the hardware texture fetch:
 *
 *   SamplerComparisonState::SampleCmpLevelZero(shadowMap, uv, referenceDepth)
 *
 * This returns 1.0 if the sampled depth PASSES the comparison (≥ or ≤
 * depending on the ComparisonFunc), 0.0 if it fails, and a bilinear blend
 * of 0/1 at the texel boundary — giving 2×2 free PCF from hardware.
 * Our 3×3 explicit PCF loop combines 9 hardware PCF samples for a smoother
 * result (effectively 18-tap coverage with 9 bilinear blend points).
 *
 * ============================================================================
 * TEACHING NOTE — Shadow Bias (Depth Bias)
 * ============================================================================
 * "Shadow acne" is a self-shadowing artefact caused by floating-point
 * imprecision: a surface casts a shadow on itself because the recorded depth
 * (shadow map) nearly equals the re-computed depth during the lit pass.
 *
 * Two complementary fixes:
 *   1. Rasterizer depth bias (in the shadow pass): offsets depth during
 *      rendering by a constant + slope-scaled amount.  Applied on the GPU
 *      before depth is written to the shadow map.
 *   2. Shader bias (here): subtract a small constant from the reference depth
 *      before the comparison:
 *        refDepth = lightClip.z - 0.005;
 *      This ensures the surface depth is always slightly LESS than what is
 *      stored in the shadow map, preventing self-shadowing.
 *
 * Shader Model: ps_4_0  (D3D_FEATURE_LEVEL_10_0 compatible)
 */

// ---------------------------------------------------------------------------
// Constant buffer b0 — shared with shadow_lit.vs.hlsl
// ---------------------------------------------------------------------------
// TEACHING NOTE — CB Reuse Across Shader Stages
// The same ShadowLitCB is bound to VS slot 0 and PS slot 0.  Only g_lightDir
// and g_lightViewProj are read in the PS; the transform matrices are ignored
// here but declaring the same struct ensures the CB size and layout match.
// ---------------------------------------------------------------------------
cbuffer ShadowLitCB : register(b0)
{
    float4x4 g_world;           ///< (used in VS only)
    float4x4 g_view;            ///< (used in VS only)
    float4x4 g_proj;            ///< (used in VS only)
    float4x4 g_lightViewProj;   ///< Light view-proj — used to reproject worldPos
    float4   g_lightDir;        ///< Directional light direction in world space
};

// ---------------------------------------------------------------------------
// Shadow map + comparison sampler
// ---------------------------------------------------------------------------
// TEACHING NOTE — Texture2D<float> for Depth Maps
// A depth shadow map is stored as a single-channel floating-point texture
// (DXGI_FORMAT_D32_FLOAT).  When bound as an SRV to the shader, D3D11 uses
// the R32_FLOAT view format, so we read it as Texture2D<float> — not
// Texture2D<float4>.  Each texel stores one depth value in [0, 1].
//
// TEACHING NOTE — SamplerComparisonState (PCF sampler)
// D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT enables bilinear PCF:
//   • Bilinear filtering of comparison results across neighbouring texels.
//   • D3D11_COMPARISON_LESS: sample PASSES if stored depth > reference.
//     (The shadow map stores LIGHT-SPACE depth; we test whether the surface
//      is closer to the light than the stored occluder.)
// ---------------------------------------------------------------------------
Texture2D<float>      g_shadowMap  : register(t0);  ///< 512×512 D32_FLOAT depth map
SamplerComparisonState g_cmpSampler : register(s0);  ///< PCF comparison sampler

// ---------------------------------------------------------------------------
// main — Lambert shading with 3×3 PCF shadow factor
// ---------------------------------------------------------------------------
float4 main(float4 clipPos  : SV_POSITION,
            float3 worldPos : TEXCOORD1,
            float3 worldNrm : NORMAL) : SV_TARGET
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Reprojecting World Position to Light Clip Space
    // -----------------------------------------------------------------------
    // We use the same g_lightViewProj matrix used in the shadow pass to map
    // the fragment's world-space position into the shadow map's UV space.
    //
    //   lightClip.xy / lightClip.w  → Normalised Device Coordinates [-1, +1]
    //   → shadowUV.x = ndcX * 0.5 + 0.5       (maps [-1,+1] to [0,1])
    //   → shadowUV.y = ndcY * (-0.5) + 0.5    (flip Y: D3D11 Y-down in texture)
    //   → refDepth   = ndcZ (already in [0,1] for D3D11 projection)
    //
    // We subtract a small constant (0.005) to avoid shadow acne.
    // -----------------------------------------------------------------------
    float4 lClip    = mul(float4(worldPos, 1.0f), g_lightViewProj);
    float  invW     = 1.0f / lClip.w;
    float2 shadowUV = lClip.xy * invW * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
    float  refDepth = lClip.z * invW - 0.005f;  // bias to prevent self-shadowing

    // -----------------------------------------------------------------------
    // TEACHING NOTE — 3×3 PCF Kernel
    // -----------------------------------------------------------------------
    // We sample 9 neighbouring texels in a 3×3 grid centred on shadowUV.
    // Each SampleCmpLevelZero call returns 1.0 (lit) or 0.0 (shadow), with
    // bilinear blending at texel boundaries thanks to the comparison sampler.
    // Averaging 9 samples smooths the shadow edge into a soft gradient.
    //
    // kTexelSize = 1 / 512: the size of one shadow-map texel in UV space.
    // Sampling at offsets ±1 texel covers one-pixel step in the shadow map.
    // -----------------------------------------------------------------------
    static const float kTexelSize = 1.0f / 512.0f;
    float shadow = 0.0f;
    [unroll] for (int dy = -1; dy <= 1; ++dy)
    {
        [unroll] for (int dx = -1; dx <= 1; ++dx)
        {
            float2 off = float2(dx, dy) * kTexelSize;
            shadow += g_cmpSampler.SampleCmpLevelZero(
                g_shadowMap,
                saturate(shadowUV + off),   // clamp to [0,1] to avoid border wrap
                refDepth);
        }
    }
    shadow /= 9.0f;  // average the 9 comparison results

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Lambert Diffuse Shading
    // -----------------------------------------------------------------------
    // NdotL = max(0, N·L) gives the cosine falloff of Lambert's law:
    //   irradiance ∝ cos(angle between normal and light direction)
    //
    // We add a 0.15 ambient term so shadowed regions are never completely
    // black (simulates indirect light bouncing off the environment).
    //
    // The final colour modulates a warm stone-like base (0.7, 0.65, 0.55)
    // by the combined diffuse × shadow factor.
    // -----------------------------------------------------------------------
    float3 N     = normalize(worldNrm);
    float3 L     = normalize(-g_lightDir.xyz);   // towards the light
    float  NdotL = saturate(dot(N, L));
    float3 col   = float3(0.7f, 0.65f, 0.55f) * (0.15f + 0.85f * NdotL * shadow);

    return float4(col, 1.0f);
}
