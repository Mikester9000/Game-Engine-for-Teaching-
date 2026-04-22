/**
 * shadow_lit.vs.hlsl
 * Vertex shader for the lit (camera-view) pass of the shadow demo (M17).
 *
 * ============================================================================
 * TEACHING NOTE — Two-Pass Shadow Rendering
 * ============================================================================
 * Shadow rendering requires two separate draw passes over the same geometry:
 *
 *   Pass 1 — Shadow Pass (shadow.vs.hlsl)
 *     • Render scene from the LIGHT's perspective.
 *     • Write only depth to a 512×512 shadow map texture.
 *     • Output: depth-stencil buffer (the "shadow map").
 *
 *   Pass 2 — Lit Pass (this file + shadow_lit.ps.hlsl)
 *     • Render scene from the CAMERA's perspective (normal view).
 *     • In the pixel shader, look up the shadow map to determine whether
 *       each fragment is in shadow.
 *     • Output: shaded colour to the back buffer (or offscreen RT in CI).
 *
 * This vertex shader handles Pass 2.  It outputs world-space position and
 * normal so the pixel shader can compute lighting AND perform the shadow map
 * comparison without needing an additional texture coordinate interpolant.
 *
 * ============================================================================
 * TEACHING NOTE — Why Pass World-Space Data?
 * ============================================================================
 * The pixel shader needs:
 *
 *   1. worldPos — to transform back into LIGHT clip space for the shadow lookup:
 *         lightClip = mul(float4(worldPos, 1), g_lightViewProj)
 *
 *   2. worldNrm — to compute the diffuse term:
 *         NdotL = dot(normalize(worldNrm), normalize(-g_lightDir))
 *
 * Passing world-space data as interpolated outputs (TEXCOORD1 + NORMAL) is the
 * standard pattern in deferred-friendly pipelines.  It also generalises to IBL,
 * point lights, and other effects that require world-space coordinates.
 *
 * Shader Model: vs_4_0  (D3D_FEATURE_LEVEL_10_0 compatible)
 */

// ---------------------------------------------------------------------------
// Constant buffer b0 — camera + light transforms (shared with shadow_lit.ps)
// ---------------------------------------------------------------------------
// TEACHING NOTE — Shared Constant Buffer Across Stages
// The same CB is bound to both the VS (b0) and the PS (b0).  This is safe
// because D3D11 allows each stage to bind its own set of CBs independently.
// Sharing the same buffer object avoids a redundant upload of light data.
//
// Layout (272 bytes total, aligned to 16 bytes):
//   g_world          — model → world transform   (64 B)
//   g_view           — world → view  transform   (64 B)
//   g_proj           — view  → clip  transform   (64 B)
//   g_lightViewProj  — light view-projection      (64 B)
//   g_lightDir       — directional light vector   (16 B)
// ---------------------------------------------------------------------------
cbuffer ShadowLitCB : register(b0)
{
    float4x4 g_world;           ///< Model  → World  (object placement)
    float4x4 g_view;            ///< World  → View   (camera look-at)
    float4x4 g_proj;            ///< View   → Clip   (perspective projection)
    float4x4 g_lightViewProj;   ///< Light view-projection (for shadow map lookup in PS)
    float4   g_lightDir;        ///< Directional light direction in world space (w = unused)
};

// ---------------------------------------------------------------------------
// Vertex input — model-space position, normal, and UV
// ---------------------------------------------------------------------------
struct VSInput
{
    float3 pos    : POSITION;   ///< Model-space vertex position
    float3 normal : NORMAL;     ///< Model-space surface normal (unit sphere: == pos)
    float2 uv     : TEXCOORD0;  ///< Texture UV (passthrough, unused in M17 lit pass)
};

// ---------------------------------------------------------------------------
// Per-pixel interpolated data passed to the pixel shader
// ---------------------------------------------------------------------------
struct PSInput
{
    float4 clipPos  : SV_POSITION;  ///< Clip-space position (required by rasteriser)
    float3 worldPos : TEXCOORD1;    ///< World-space position for shadow map transform
    float3 worldNrm : NORMAL;       ///< World-space normal for diffuse lighting
};

// ---------------------------------------------------------------------------
// main — camera-view transform with world-space output
// ---------------------------------------------------------------------------
PSInput main(VSInput i)
{
    PSInput o;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Model → World → View → Clip Transform Chain
    // -----------------------------------------------------------------------
    // Same four-step chain as the PBR vertex shaders (M9, M16):
    //
    //   Step 1: pos_model  → pos_world  (g_world multiply)
    //   Step 2: pos_world  → pos_view   (g_view multiply)
    //   Step 3: pos_view   → pos_clip   (g_proj multiply)
    //
    // We store worldPos as an output so the PS can transform it into light
    // clip space for the shadow map lookup.
    // -----------------------------------------------------------------------
    float4 wPos = mul(float4(i.pos, 1.0f), g_world);
    o.worldPos  = wPos.xyz;
    o.clipPos   = mul(mul(wPos, g_view), g_proj);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Normal Transform for a Pure-Rotation World Matrix
    // -----------------------------------------------------------------------
    // For a world matrix that contains only rotation (no scale or shear), the
    // inverse-transpose equals the rotation part, so we can safely use the
    // upper-left 3×3 of g_world to transform the normal.
    //
    // In a production shader you would use a dedicated worldInvTrans matrix
    // (as in pbr_mesh.vs.hlsl) to handle non-uniform scale correctly.
    // For the shadow demo, the sphere is only rotated so the simplified
    // form is exact.
    // -----------------------------------------------------------------------
    o.worldNrm = mul(i.normal, (float3x3)g_world);

    return o;
}
