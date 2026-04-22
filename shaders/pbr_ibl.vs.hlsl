/**
 * pbr_ibl.vs.hlsl
 * Vertex shader for PBR + Image-Based Lighting (IBL) sphere scene (M16).
 *
 * ============================================================================
 * TEACHING NOTE — Why a Separate VS for IBL?
 * ============================================================================
 * The vertex stage for the IBL scene is identical to pbr_mesh.vs.hlsl.
 * We keep it as a separate file so that:
 *   1. Students can trace the exact shader pipeline for each milestone
 *      without "what changed?" confusion.
 *   2. Future milestones can extend the IBL VS independently (e.g. adding
 *      tangent vectors for normal-map support in M17).
 *
 * For now, pbr_ibl.vs.hlsl and pbr_mesh.vs.hlsl produce identical output.
 * The real IBL work happens in the pixel shader (pbr_ibl.ps.hlsl).
 *
 * ============================================================================
 * TEACHING NOTE — Outputs Required by the IBL Pixel Shader
 * ============================================================================
 * The IBL PS needs three interpolated inputs from the VS:
 *
 *   worldPos  — used to compute V = normalize(cameraPos - worldPos).
 *               The reflection vector R = reflect(-V, N) is then used to
 *               sample the prefiltered environment cubemap.
 *
 *   worldNrm  — used for all N·L, N·V, N·H dot products in the BRDF,
 *               AND to sample the irradiance cubemap (diffuse IBL).
 *
 *   uv        — for future texture maps (albedo, metallic, roughness, AO).
 *               In M16 the material is still driven by constant buffer data.
 *
 * ============================================================================
 * TEACHING NOTE — Row-Major Convention (D3D11)
 * ============================================================================
 * Our C++ Mat4 type uses row-major storage (m[row][col]).  D3D11 HLSL also
 * defaults to row-major storage, so we can memcpy() Mat4 data directly into
 * a constant buffer without transposing.  The transform chain therefore reads
 * left-to-right:
 *
 *   clipPos = mul(mul(mul(float4(pos,1), g_world), g_view), g_proj)
 *
 * Shader Model: vs_4_0  (D3D_FEATURE_LEVEL_10_0 compatible)
 */

// ---------------------------------------------------------------------------
// Constant buffer b0 — per-frame transform matrices (VS slot 0)
// ---------------------------------------------------------------------------
// TEACHING NOTE — Shared Constant Buffer Layout
// b0 is bound to both the VS (for transforms) and—in theory—could be
// accessed by the PS, but we only bind it to VS for this pass.  The PS
// gets its own CBs at b1 (light) and b2 (material).
// ---------------------------------------------------------------------------
cbuffer PerFrameCB : register(b0)
{
    float4x4 g_world;          // Model  → World  (sphere world matrix, rotates each frame)
    float4x4 g_worldInvTrans;  // Inverse-transpose of World  (for correct normal transform)
    float4x4 g_view;           // World  → View   (camera look-at)
    float4x4 g_proj;           // View   → Clip   (perspective projection)
};

// ---------------------------------------------------------------------------
// Vertex input — must match D3D11_INPUT_ELEMENT_DESC in LoadPBRIBLScene()
// ---------------------------------------------------------------------------
struct VSInput
{
    float3 pos    : POSITION;   // Model-space vertex position
    float3 normal : NORMAL;     // Model-space surface normal (unit sphere: == pos)
    float2 uv     : TEXCOORD0;  // Texture UV (U = longitude, V = latitude)
};

// ---------------------------------------------------------------------------
// Per-pixel interpolated data passed to the pixel shader
// ---------------------------------------------------------------------------
struct PSInput
{
    float4 clipPos  : SV_POSITION;  // Clip-space position (required by rasteriser)
    float3 worldPos : TEXCOORD1;    // World-space position (for V vector in PS)
    float3 worldNrm : NORMAL;       // World-space normal   (for BRDF + IBL sampling)
    float2 uv       : TEXCOORD0;    // Texture UV           (passthrough)
};

// ---------------------------------------------------------------------------
// main — vertex entry point
// ---------------------------------------------------------------------------
PSInput main(VSInput i)
{
    PSInput o;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Transform Chain: Model → World → View → Clip
    // -----------------------------------------------------------------------
    // Step 1: Model → World.
    //   We retain worldPos4 so the pixel shader can compute the view vector.
    //   The W component is 1 because positions are affine points.
    //
    // Step 2: World → View → Clip.
    //   Two mul() calls chain the view and projection transforms.
    //   D3D11 row-vector convention: result = vec × matrix (left-multiply).
    // -----------------------------------------------------------------------
    float4 worldPos4 = mul(float4(i.pos, 1.0f), g_world);
    o.worldPos       = worldPos4.xyz;
    o.clipPos        = mul(mul(worldPos4, g_view), g_proj);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Normal Transform via Inverse-Transpose
    // -----------------------------------------------------------------------
    // Normals are co-vectors: they must be transformed by the inverse-
    // transpose of the World matrix so they remain perpendicular to the
    // surface under non-uniform scale.  For pure rotations (our sphere),
    // the inverse-transpose equals the rotation itself, but we keep the
    // dedicated slot for correctness when scale is later introduced.
    //
    // normalize() corrects any length change introduced by the transform.
    // -----------------------------------------------------------------------
    o.worldNrm = normalize(mul(i.normal, (float3x3)g_worldInvTrans));

    o.uv = i.uv;
    return o;
}
