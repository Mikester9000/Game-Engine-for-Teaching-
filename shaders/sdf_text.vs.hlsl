/**
 * @file sdf_text.vs.hlsl
 * @brief Vertex shader for SDF (Signed Distance Field) text rendering.
 *
 * ============================================================================
 * TEACHING NOTE — What is SDF Text Rendering?
 * ============================================================================
 * Traditional bitmap fonts store a pre-rasterised glyph image at one fixed
 * resolution (e.g. 12px, 24px).  When you scale them up the pixels become
 * visible and the glyphs look blurry or blocky.
 *
 * Signed Distance Field (SDF) fonts store *distance to the nearest edge*
 * instead of a hard on/off pixel.  Each texel value encodes:
 *
 *     > 0.5  →  inside the glyph  (solid ink)
 *     = 0.5  →  exactly on the edge
 *     < 0.5  →  outside the glyph (empty paper)
 *
 * The pixel shader reconstructs the hard edge by applying a smooth threshold
 * (smoothstep) to the SDF value.  Because the threshold is just arithmetic on
 * the stored distance, it works at ANY scale:
 *
 *     Small text  → narrow smoothstep band → crisp edge
 *     Large text  → wider smoothstep band  → still crisp (anti-aliased)
 *
 * This is the same technique used by Valve (Half-Life 2 font system, 2007),
 * Unity's TextMeshPro, and most modern UI middleware.
 *
 * ============================================================================
 * TEACHING NOTE — Screen-Space → NDC Coordinate Transformation
 * ============================================================================
 * The CPU passes vertex positions in *screen-space pixel coordinates*:
 *     (0, 0) = top-left corner of the render target
 *     (W, H) = bottom-right corner (W = screenWidth, H = screenHeight)
 *
 * Direct3D 11 NDC (Normalised Device Coordinates) space uses:
 *     (-1, -1) = bottom-left
 *     ( 1,  1) = top-right
 *
 * The conversion formulas are:
 *     ndcX =  2 * (px / W) - 1         range: -1 .. +1
 *     ndcY = -(2 * (py / H) - 1)       range: +1 .. -1  (Y is flipped!)
 *
 * ============================================================================
 * TEACHING NOTE — Why Is the Y Axis Flipped?
 * ============================================================================
 * Screen-space has its Y origin at the TOP-LEFT and increases downward (the
 * typical convention for 2D UI and texture coordinates).
 *
 * D3D11 NDC has its Y origin at the BOTTOM-LEFT and increases upward (the
 * convention inherited from OpenGL-style clip-space math).
 *
 * So pixel (0, 0) maps to NDC (-1, +1), and pixel (W, H) maps to NDC (+1, -1).
 * Failing to flip Y would render text upside-down.
 *
 * ============================================================================
 *
 * Shader Model: SM 4.0  (Direct3D 10+ hardware; GT610 / every DX10 card)
 * Target:       vs_4_0
 */

// ---------------------------------------------------------------------------
// Per-frame constant buffer (bound to slot b0 by D3D11Renderer).
// Updated once per RenderText() call to communicate the render target size.
// ---------------------------------------------------------------------------
cbuffer ScreenCB : register(b0)
{
    // TEACHING NOTE — Why float2?
    // We only need width and height for the NDC transform.  Packing them into
    // a float2 + float2 pad keeps the constant buffer at 16 bytes (one D3D11
    // register), which is the minimum size.  Constant buffers must be a
    // multiple of 16 bytes.
    float2 screenSize;   // (renderTargetWidth, renderTargetHeight) in pixels
    float2 _pad;         // padding to reach 16-byte alignment
};

// ---------------------------------------------------------------------------
// Input vertex layout — must match D3D11_INPUT_ELEMENT_DESC in FontRenderer.
// ---------------------------------------------------------------------------
struct VSIn
{
    float2 pos  : POSITION;   // screen-space pixel coordinates (x, y)
    float2 uv   : TEXCOORD0;  // atlas texture coordinates (0..1 each)
    float4 col  : COLOR;      // RGBA text colour (premultiplied or straight alpha)
};

// ---------------------------------------------------------------------------
// Interpolated output passed to the pixel shader.
// ---------------------------------------------------------------------------
struct VSOut
{
    float4 position : SV_POSITION;  // clip-space position (w=1 for 2D)
    float2 uv       : TEXCOORD0;    // passed through unchanged
    float4 col      : COLOR;        // passed through unchanged
};

// ---------------------------------------------------------------------------
// Main vertex shader entry point.
// ---------------------------------------------------------------------------
VSOut main(VSIn v)
{
    VSOut o;

    // TEACHING NOTE — Screen-to-NDC transform step by step:
    //
    //   1. Divide pixel coordinate by screen size → [0, 1] range.
    //   2. Multiply by 2 → [0, 2] range.
    //   3. Subtract 1    → [-1, 1] range  (NDC X, or flipped NDC Y).
    //   4. Negate Y      → correct orientation (screen +Y down → NDC +Y up).
    //
    // All in one expression:
    //   ndcX =  2 * (px / W) - 1
    //   ndcY = -(2 * (py / H) - 1)  =  1 - 2 * (py / H)

    float ndcX =  2.0f * (v.pos.x / screenSize.x) - 1.0f;
    float ndcY =  1.0f - 2.0f * (v.pos.y / screenSize.y);

    // TEACHING NOTE — SV_POSITION and the w component
    // For 2D text we use an orthographic (w=1) projection — no perspective
    // divide is needed.  Setting w=1 means clip-space == NDC-space, which
    // is correct for screen-aligned quads.
    o.position = float4(ndcX, ndcY, 0.0f, 1.0f);

    // Pass UV and colour straight through to the pixel shader.
    o.uv  = v.uv;
    o.col = v.col;

    return o;
}
