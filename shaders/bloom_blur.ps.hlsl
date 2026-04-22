/**
 * bloom_blur.ps.hlsl
 * Pixel shader — separable 5-tap Gaussian blur for the bloom effect (M17).
 *
 * ============================================================================
 * TEACHING NOTE — Separable Gaussian Blur
 * ============================================================================
 * A 2D Gaussian blur kernel (N×N) requires N² texture samples per pixel.
 * For a 9×9 kernel that is 81 samples — expensive on large images.
 *
 * The SEPARABLE property of the Gaussian function means:
 *
 *   G2D(x, y) = G1D(x) × G1D(y)
 *
 * A 2D Gaussian can be decomposed into two INDEPENDENT 1D passes:
 *   Pass 1: sample along the X axis (horizontal blur).
 *   Pass 2: sample along the Y axis (vertical blur) of Pass 1's output.
 *
 * This reduces the cost from N² to 2N samples per pixel.  For our 5-tap
 * kernel (N=5): 25 → 10 samples.  For a 9-tap kernel: 81 → 18 samples.
 *
 * Both passes use THIS SAME shader — the direction (horizontal or vertical)
 * is controlled by the g_direction constant buffer variable:
 *   Horizontal pass: g_direction = (1, 0)
 *   Vertical pass:   g_direction = (0, 1)
 *
 * ============================================================================
 * TEACHING NOTE — Gaussian Weights
 * ============================================================================
 * The weights approximate the 1D Gaussian G(x) = e^(-x²/2σ²) evaluated at
 * integer offsets x = 0, ±1, ±2 (5 taps).  The weights sum to 1.0:
 *
 *   w[0] = 0.22703  (centre)
 *   w[1] = 0.19459  (±1 step)
 *   w[2] = 0.12163  (±2 steps)
 *   w[3] = 0.05405  (±3 steps)
 *   w[4] = 0.01622  (±4 steps)
 *
 *   Sum = 0.22703 + 2×(0.19459 + 0.12163 + 0.05405 + 0.01622)
 *       = 0.22703 + 2×0.38649
 *       = 0.22703 + 0.77298 ≈ 1.0  ✓
 *
 * A larger σ produces a wider, softer bloom; a smaller σ produces a tighter,
 * crisper bloom.  Changing the number of taps or the weights is how artists
 * tune the "bokeh" appearance of the bloom.
 *
 * Shader Model: ps_4_0  (D3D_FEATURE_LEVEL_10_0 compatible)
 */

// ---------------------------------------------------------------------------
// Input texture — bright-pass result (horizontal) or horizontal-blur result (vertical)
// ---------------------------------------------------------------------------
Texture2D    g_input : register(t0);  ///< Input texture for this blur pass
SamplerState g_smp   : register(s0);  ///< Linear-clamp sampler

// ---------------------------------------------------------------------------
// Constant buffer b0 — blur direction and texel size
// ---------------------------------------------------------------------------
// TEACHING NOTE — Encoding Direction in a Constant Buffer
// Instead of two separate shaders (one horizontal, one vertical), we pass
// the blur direction as a float2.  This halves the number of pipeline state
// objects (PSOs) required and keeps the shader code in one file.
//
// g_texelSize = (1/width, 1/height) of the blur render target (e.g. 1/256).
// Multiplying the integer offset (0..4) by g_texelSize converts it to UV
// space, which is always in [0, 1] regardless of render target resolution.
// ---------------------------------------------------------------------------
cbuffer BlurCB : register(b0)
{
    float2 g_direction;  ///< (1,0) = horizontal pass; (0,1) = vertical pass
    float2 g_texelSize;  ///< (1/width, 1/height) of the blur render target
};

// ---------------------------------------------------------------------------
// 5-tap Gaussian weights (symmetric, centred at index 0)
// ---------------------------------------------------------------------------
// TEACHING NOTE — Static Constants in HLSL
// "static const" creates compile-time constants in HLSL — they are inlined
// into the shader bytecode and don't cost a constant buffer slot.
// ---------------------------------------------------------------------------
static const float kWeights[5] = {
    0.22703f,   // centre weight (index 0)
    0.19459f,   // ±1 tap
    0.12163f,   // ±2 taps
    0.05405f,   // ±3 taps
    0.01622f    // ±4 taps
};

// ---------------------------------------------------------------------------
// main — separable 1D Gaussian blur
// ---------------------------------------------------------------------------
float4 main(float4 pos : SV_POSITION,
            float2 uv  : TEXCOORD0) : SV_TARGET
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Accumulation Pattern for Separable Blur
    // -----------------------------------------------------------------------
    // 1. Start with the centre tap weighted by kWeights[0].
    // 2. Loop from i=1 to 4, sampling symmetrically on both sides of centre.
    //    The symmetric property of the Gaussian means the weight at +i equals
    //    the weight at -i, so we sample both sides and add them together.
    // 3. The total accumulation is a weighted sum of 9 samples (centre + 4
    //    symmetric pairs), approximating the 1D Gaussian convolution.
    //
    // The direction vector turns the 1D step into a 2D UV offset:
    //   horizontal: offset = (i, 0) × texelSize  → (i/width,  0)
    //   vertical:   offset = (0, i) × texelSize  → (0,  i/height)
    // -----------------------------------------------------------------------
    float4 col = g_input.Sample(g_smp, uv) * kWeights[0];

    [unroll]
    for (int i = 1; i <= 4; ++i)
    {
        float2 off = g_direction * g_texelSize * float(i);
        col += g_input.Sample(g_smp, uv + off) * kWeights[i];
        col += g_input.Sample(g_smp, uv - off) * kWeights[i];
    }

    return col;
}
