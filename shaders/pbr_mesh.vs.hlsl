/**
 * pbr_mesh.vs.hlsl
 * Vertex shader for Physically Based Rendering (PBR) mesh.
 *
 * ============================================================================
 * TEACHING NOTE — PBR Vertex Stage
 * ============================================================================
 * In a PBR pipeline the vertex shader's job is straightforward: transform
 * vertex data from model space into the coordinate spaces the pixel shader
 * needs.
 *
 * Outputs required by the PBR pixel shader:
 *   SV_POSITION (clip space)  — rasteriser uses this to determine which
 *                               pixels the triangle covers. Required.
 *   WorldPos    (world space) — PS uses this to compute the view direction
 *                               V = normalize(cameraPos - worldPos).
 *   WorldNorm   (world space) — used for N·L, N·V, and N·H dot products in
 *                               the Cook-Torrance BRDF.
 *   UV          (passthrough) — texture coordinates for M9 (albedo/metallic/
 *                               roughness/normal maps added in a future pass).
 *
 * ============================================================================
 * TEACHING NOTE — Two Different Transforms for Positions vs. Normals
 * ============================================================================
 * Positions are transformed by the full World matrix (4×4):
 *   worldPos = mul(float4(modelPos, 1), g_world)
 *
 * Normals use the INVERSE-TRANSPOSE of the upper 3×3 of the World matrix
 * (g_worldInvTrans):
 *   worldNorm = normalize(mul(modelNorm, (float3x3)g_worldInvTrans))
 *
 * WHY the inverse-transpose?
 *   Imagine a sphere scaled 2× along X (non-uniform scale).  Its side
 *   normals, which in model space are parallel to X, should still point
 *   outward after the stretch.  Simply applying the World matrix stretches
 *   them along X too, making them no longer perpendicular to the surface.
 *
 *   The inverse-transpose of a matrix transforms normals (co-vectors)
 *   correctly regardless of scale.  For pure-rotation matrices (orthogonal)
 *   the inverse-transpose equals the original, so it is always safe to use.
 *
 * ============================================================================
 * TEACHING NOTE — Row-Major Matrices and D3D11 Constant Buffers
 * ============================================================================
 * D3D11 HLSL uses ROW-MAJOR matrix storage by default (the opposite of
 * OpenGL / GLSL which uses column-major).
 *
 * Our C++ Mat4 type also uses row-major storage (m[row][col]), so we can
 * memcpy it directly into a constant buffer without transposing.
 *
 * Vertex transform:
 *   clipPos = mul(mul(mul(modelPos, g_world), g_view), g_proj)
 *
 * Note the left-to-right application order (row-vector on the left).
 *
 * ============================================================================
 * TEACHING NOTE — Shader Model 4.0 (Feature Level 10_0 Compatible)
 * ============================================================================
 * Target: vs_4_0
 * Minimum hardware: GeForce GT 610 / D3D_FEATURE_LEVEL_10_0.
 * All float intrinsics used here (mul, normalize) are available in SM 4.0.
 *
 * ============================================================================
 *
 * Shader Model: vs_4_0
 * Compile:      D3DCompileFromFile("pbr_mesh.vs.hlsl", "main", "vs_4_0")
 */

// ---------------------------------------------------------------------------
// Constant buffer b0 — per-frame transform matrices (bound to VS slot 0)
// ---------------------------------------------------------------------------
// TEACHING NOTE — cbuffer Packing in HLSL
// Members in a cbuffer are packed into 16-byte "registers".  A float4x4
// takes exactly 4 registers (64 bytes).  A float3 fits in one register
// (12 bytes used, 4 bytes of implicit padding follow).
//
// We use separate cbuffers for VS (b0) and PS (b1 / b2) so that the CPU
// only needs to update the affected stage's CB when that data changes:
//   b0 updates every frame (world matrix changes when the object rotates).
//   b1 updates rarely    (light direction rarely changes).
//   b2 updates per draw  (material changes between objects).
// ---------------------------------------------------------------------------
cbuffer PerFrameCB : register(b0)
{
    float4x4 g_world;          // Model  → World transform
    float4x4 g_worldInvTrans;  // Inverse-transpose of upper 3×3 (for normals)
    float4x4 g_view;           // World  → View transform
    float4x4 g_proj;           // View   → Clip  (perspective) transform
};

// ---------------------------------------------------------------------------
// Vertex input — matches D3D11_INPUT_ELEMENT_DESC in LoadPBRMeshScene()
// ---------------------------------------------------------------------------
struct VSInput
{
    float3 pos    : POSITION;   // Model-space vertex position
    float3 normal : NORMAL;     // Model-space normal  (for unit sphere: == pos)
    float2 uv     : TEXCOORD0;  // Texture UV (U along longitude, V along latitude)
};

// ---------------------------------------------------------------------------
// Output interpolated to each pixel by the rasteriser
// ---------------------------------------------------------------------------
struct PSInput
{
    float4 clipPos  : SV_POSITION;  // Clip-space position — required by rasteriser
    float3 worldPos : TEXCOORD1;    // World-space position — for V vector in PS
    float3 worldNrm : NORMAL;       // World-space normal   — for BRDF dot products
    float2 uv       : TEXCOORD0;    // Texture UV           — for future texture maps
};

// ---------------------------------------------------------------------------
// main — vertex shader entry point
// ---------------------------------------------------------------------------
PSInput main(VSInput i)
{
    PSInput o;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Transform Chain: Model → World → View → Clip
    // -----------------------------------------------------------------------
    // We compute the world-space position first because the pixel shader needs
    // it directly (for the view-direction vector V = camera - worldPos).
    //
    // Then we continue the chain: world → view → clip.
    // mul(a, b) for float4×float4x4 is the row-vector × matrix product,
    // i.e. the standard "transform a point" operation in D3D11.
    // -----------------------------------------------------------------------

    // Step 1: Model → World (retain worldPos for the pixel shader)
    float4 worldPos4 = mul(float4(i.pos, 1.0f), g_world);
    o.worldPos       = worldPos4.xyz;

    // Step 2: World → View → Clip (two mul calls chained)
    o.clipPos = mul(mul(worldPos4, g_view), g_proj);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Normal Transform via Inverse-Transpose
    // -----------------------------------------------------------------------
    // Cast g_worldInvTrans to float3x3 to extract the upper 3×3 block.
    // mul(float3_normal, float3x3_M) is the row-vector × matrix form.
    //
    // For pure-rotation matrices (no non-uniform scale), the inverse-transpose
    // equals the original, so this reduces to the same as g_world.  We keep
    // the dedicated slot so non-uniform scale works correctly when artists
    // squish or stretch objects.
    //
    // normalize() corrects any length change introduced by the matrix.
    // -----------------------------------------------------------------------
    o.worldNrm = normalize(mul(i.normal, (float3x3)g_worldInvTrans));

    o.uv = i.uv;
    return o;
}
