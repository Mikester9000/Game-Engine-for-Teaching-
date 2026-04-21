/**
 * @file sdf_text.ps.hlsl
 * @brief Pixel shader for SDF (Signed Distance Field) text rendering.
 *
 * ============================================================================
 * TEACHING NOTE — What SDF Values Mean
 * ============================================================================
 * Each texel in the SDF atlas stores a normalised signed distance:
 *
 *     texel > 0.5  →  pixel is INSIDE the glyph  (solid ink region)
 *     texel = 0.5  →  pixel is ON the glyph edge  (the mathematical boundary)
 *     texel < 0.5  →  pixel is OUTSIDE the glyph  (transparent region)
 *
 * The "signed" part means the distance has a direction: positive = inside,
 * negative = outside.  After normalising to [0, 1] with 0.5 at the edge:
 *
 *     raw_sdf_distance (in pixels) = (texel - 0.5) * search_radius * 2
 *
 * Storing normalised distance means the same atlas works at every resolution.
 *
 * ============================================================================
 * TEACHING NOTE — Why smoothstep Gives Anti-Aliasing
 * ============================================================================
 * A hard threshold at 0.5 would give a fully aliased edge (one texel is 100%
 * opaque, the next is 100% transparent).  smoothstep(edge0, edge1, x) returns:
 *
 *     0        when x <= edge0
 *     1        when x >= edge1
 *     smooth   when edge0 < x < edge1  (cubic Hermite interpolation)
 *
 * Using a small band around 0.5 (e.g. 0.45 to 0.55) creates a soft transition
 * over ~1 pixel's worth of the distance gradient.  This is effectively a 1-pixel
 * box filter on the glyph edge — the cheapest form of screen-space anti-aliasing.
 *
 * ============================================================================
 * TEACHING NOTE — Why This Works at Any Scale
 * ============================================================================
 * When text is rendered small (many SDF texels per screen pixel), the distance
 * gradient across the glyph edge is compressed → the smoothstep band is narrow
 * relative to screen pixels → crisp edge.
 *
 * When text is rendered large (fewer SDF texels per screen pixel), the distance
 * gradient is stretched → the same smoothstep band covers more screen pixels →
 * still smooth (anti-aliased).
 *
 * To further improve quality at large scales you can widen the band:
 *     smoothstep(0.35, 0.65, sdfVal)   →  thicker transition (for large text)
 *     smoothstep(0.48, 0.52, sdfVal)   →  tighter transition (for small text)
 *
 * A real production font renderer (TextMeshPro, slug, msdfgen) computes the
 * correct band width per-pixel based on the screen-space derivative of the UVs
 * (ddx/ddy), but the fixed band is an excellent starting point.
 *
 * ============================================================================
 *
 * Shader Model: SM 4.0  (Direct3D 10+ hardware)
 * Target:       ps_4_0
 */

// ---------------------------------------------------------------------------
// SDF atlas texture and sampler.
// Bound by FontRenderer::RenderText() before each draw call.
// ---------------------------------------------------------------------------

// TEACHING NOTE — register(t0) / register(s0)
// D3D11 uses explicit register slots for resource binding.  t0 = texture slot 0,
// s0 = sampler slot 0.  We must match these in the C++ SetShaderResources /
// SetSamplers calls or the shader will sample an empty/default resource.
Texture2D    atlasTexture  : register(t0);  // R8_UNORM SDF atlas (128×48 px)
SamplerState linearSampler : register(s0);  // linear filtering for sub-texel smoothness

// ---------------------------------------------------------------------------
// Input from the vertex shader.
// ---------------------------------------------------------------------------
struct PSIn
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
    float4 col      : COLOR;
};

// ---------------------------------------------------------------------------
// Main pixel shader entry point.
// ---------------------------------------------------------------------------
float4 main(PSIn p) : SV_Target
{
    // TEACHING NOTE — Sampling the SDF atlas
    // atlasTexture is R8_UNORM (single channel, 8-bit normalised to [0,1]).
    // We read the red channel (.r) — the other channels (g, b, a) are always 0
    // in a single-channel texture.  The linear sampler bilinearly interpolates
    // between the 8×8 glyph texels, which is important for sub-pixel smoothness.
    float sdfVal = atlasTexture.Sample(linearSampler, p.uv).r;

    // TEACHING NOTE — Edge reconstruction with smoothstep
    // smoothstep(0.45, 0.55, sdfVal) maps the [0.45, 0.55] range onto [0, 1]
    // using a cubic Hermite curve.  The 0.1-wide band (centred at 0.5 = edge)
    // produces approximately 1 pixel of anti-aliased softness.
    //
    // The result is the per-pixel "opacity" of the glyph:
    //   0.0 = fully transparent (empty space away from glyph)
    //   1.0 = fully opaque     (solid glyph interior)
    //   0..1 = anti-aliased edge pixels
    float alpha = smoothstep(0.45f, 0.55f, sdfVal);

    // TEACHING NOTE — Alpha-blended text colour
    // We multiply the incoming colour's alpha by the SDF coverage alpha.
    // This allows callers to:
    //   * Change text colour by passing different (r, g, b) values.
    //   * Fade text in/out by reducing the alpha component of the colour.
    //   * Render semi-transparent text (e.g. for UI overlays).
    //
    // The blend state in FontRenderer uses D3D11_BLEND_SRC_ALPHA /
    // D3D11_BLEND_INV_SRC_ALPHA so the GPU blends:
    //   out = src.rgb * src.a + dst.rgb * (1 - src.a)
    //
    // This is standard "straight alpha" (non-premultiplied) blending.
    return float4(p.col.rgb, p.col.a * alpha);
}
