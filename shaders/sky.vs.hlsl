/**
 * sky.vs.hlsl
 * Vertex shader for the procedural sky — full-screen triangle trick.
 *
 * ============================================================================
 * TEACHING NOTE — Full-Screen Triangle Trick
 * ============================================================================
 * The sky covers the entire viewport.  A naive approach is to bind a quad
 * (2 triangles) with a vertex buffer.  A more elegant approach (used by
 * Frostbite, Unreal, and many AAA engines) is the "full-screen triangle trick":
 *
 *   1. Bind NO vertex buffer.
 *   2. Call Draw(3, 0) — 3 vertices.
 *   3. Use SV_VertexID (0, 1, 2) to generate clip-space positions in the VS.
 *
 * The generated positions form a single large triangle that fully covers the
 * clip-space rectangle [-1, +1] × [-1, +1]:
 *
 *   ID=0: (-1, -1)  bottom-left
 *   ID=1: (-1, +3)  far above the top edge (outside clip, still rasterised)
 *   ID=2: (+3, -1)  far right of the right edge (outside clip, still rasterised)
 *
 *   The triangle covers all of [-1,+1]² because the clip-space rectangle is
 *   entirely inside the triangle formed by these three extreme vertices.
 *
 * WHY is this better than a quad?
 *   • One fewer triangle = fewer vertex shader invocations.
 *   • No vertex buffer binding = no IA (Input Assembler) stage setup cost.
 *   • Diagonal shared edge of a quad can cause a subtle per-pixel seam on
 *     some GPUs; a single triangle eliminates that artefact.
 *
 * ============================================================================
 * TEACHING NOTE — Sky Depth Placement
 * ============================================================================
 * We set the clip-space depth to 0.9999 (not 1.0 = far plane) so the sky
 * appears BEHIND all 3D geometry in the depth buffer but is NEVER depth-
 * clipped:
 *
 *   float4 pos = float4(ndcXY, 0.9999, 1.0);   // depth ≈ far plane
 *
 * With a standard D3D11 depth range [0, 1] (MinDepth=0, MaxDepth=1) and
 * D3D11_COMPARISON_LESS_EQUAL depth test, 0.9999 loses to any solid geometry
 * that writes a smaller depth value.  Using exactly 1.0 could cause sky pixels
 * to be clipped by the far plane on some hardware; 0.9999 avoids that.
 *
 * ============================================================================
 * TEACHING NOTE — Shader Model 4.0 (Feature Level 10_0 Compatible)
 * ============================================================================
 * Target: vs_4_0
 * Minimum hardware: GeForce GT 610 / D3D_FEATURE_LEVEL_10_0.
 *
 * SV_VertexID is a standard SM4.0 semantic — no SM5 required.
 * ============================================================================
 */

// ---------------------------------------------------------------------------
// Output structure — passed to the pixel shader.
// ---------------------------------------------------------------------------
struct PSInput
{
    float4 pos : SV_POSITION;   ///< Clip-space position (set by VS, read by rasteriser)
    float2 uv  : TEXCOORD0;     ///< Normalised screen UV (0,0 = top-left; 1,1 = bottom-right)
};

// ---------------------------------------------------------------------------
// main — generate a full-screen triangle from SV_VertexID.
// ---------------------------------------------------------------------------
PSInput main(uint vertexID : SV_VertexID)
{
    // TEACHING NOTE — Position Generation from Vertex ID
    // Vertex 0: NDC (-1, -1) — bottom-left
    // Vertex 1: NDC (-1, +3) — above the top (extends the triangle up)
    // Vertex 2: NDC (+3, -1) — right of the right edge (extends right)
    //
    // The three expressions below are branchless equivalents of a switch:
    //   posX = (vertexID == 2) ? +3.0 : -1.0
    //   posY = (vertexID == 1) ? +3.0 : -1.0
    float posX = (vertexID == 2u) ? 3.0f : -1.0f;
    float posY = (vertexID == 1u) ? 3.0f : -1.0f;

    PSInput o;

    // Depth 0.9999: sky is rendered behind all 3D geometry but never clipped.
    o.pos = float4(posX, posY, 0.9999f, 1.0f);

    // TEACHING NOTE — UV Coordinate Convention
    // D3D11 UV: (0,0) = top-left, (1,1) = bottom-right.
    //   u = posX * 0.5 + 0.5    maps [-1, +1] → [0, 1]
    //   v = -posY * 0.5 + 0.5   maps [+1, -1] → [0, 1]  (Y flipped for screen UV)
    //
    // uv.y = 0 at the top of the screen (zenith direction).
    // uv.y = 1 at the bottom of the screen (horizon direction).
    // This convention is used in the pixel shader to drive the zenith→horizon
    // colour gradient.
    o.uv = float2(posX * 0.5f + 0.5f,
                  -posY * 0.5f + 0.5f);

    return o;
}
