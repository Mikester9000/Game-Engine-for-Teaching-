/**
 * terrain.vs.hlsl
 * Vertex shader for heightmap-driven terrain grid (M25).
 *
 * ============================================================================
 * TEACHING NOTE — Heightmap Terrain Vertex Stage
 * ============================================================================
 * Terrain rendering differs from static mesh rendering in one key way:
 * the vertex Y coordinate is NOT authored by a 3-D modeller — it is derived
 * at bake time from a *heightmap*: a grid of floating-point height samples.
 *
 * The geometry is generated on the CPU (see terrain_renderer.cpp) as a regular
 * NxM grid, with each vertex's Y component set to the corresponding height
 * sample value.  The vertex shader then applies the standard world→view→clip
 * transform so the terrain appears at the correct position in the scene.
 *
 * Normal vectors are also computed on the CPU using finite differences of the
 * surrounding height samples (see terrain_renderer.cpp :: GenerateMesh).
 * This is the same technique used in AAA terrain renderers — computing normals
 * per-vertex on the GPU via a compute shader is an optimisation added later.
 *
 * ============================================================================
 * TEACHING NOTE — Constant Buffer Layout (TerrainCB)
 * ============================================================================
 * A single 208-byte constant buffer carries both VS and PS data so the CPU
 * only needs one SetConstantBuffers call per draw.  Both shader stages bind
 * it to register b0.
 *
 * The buffer layout is:
 *   float4x4  g_world     (64 B)  — object→world transform
 *   float4x4  g_view      (64 B)  — world→view  transform
 *   float4x4  g_proj      (64 B)  — view→clip   transform (perspective)
 *   float3    g_lightDir  (12 B)  — directional light direction (world space)
 *   float     g_pad0      ( 4 B)  — explicit 16-byte alignment pad
 *
 * Total: 208 bytes.  D3D11 requires constant buffer sizes to be multiples of
 * 16 bytes — 208 / 16 = 13, so this is valid.
 *
 * ============================================================================
 * TEACHING NOTE — Shader Model 4.0 (Feature Level 10_0 Compatible)
 * ============================================================================
 * Target: vs_4_0
 * Minimum hardware: GeForce GT 610 / D3D_FEATURE_LEVEL_10_0.
 * All intrinsics used here (mul, normalize) are available in SM 4.0.
 *
 * ============================================================================
 *
 * Shader Model: vs_4_0
 * Compile:      D3DCompileFromFile("terrain.vs.hlsl", "main", "vs_4_0")
 */

// ---------------------------------------------------------------------------
// Constant buffer b0 — terrain transform + lighting (shared with PS)
// ---------------------------------------------------------------------------
// TEACHING NOTE — Shared VS/PS Constant Buffer
// D3D11 allows the same constant buffer to be bound to both the vertex and
// pixel shader stages simultaneously:
//   context->VSSetConstantBuffers(0, 1, &terrainCB);
//   context->PSSetConstantBuffers(0, 1, &terrainCB);
// Both shaders then declare the same cbuffer block with the same register(b0).
// This avoids duplicating data and reduces the number of GPU round-trips.
// ---------------------------------------------------------------------------
cbuffer TerrainCB : register(b0)
{
    float4x4 g_world;      // Model → World transform
    float4x4 g_view;       // World → View  transform
    float4x4 g_proj;       // View  → Clip  transform
    float3   g_lightDir;   // Directional light direction (world space, unnormalised)
    float    g_pad0;       // Padding to reach 16-byte alignment
};

// ---------------------------------------------------------------------------
// Vertex input — matches D3D11_INPUT_ELEMENT_DESC in terrain_renderer.cpp
// ---------------------------------------------------------------------------
// TEACHING NOTE — Terrain Vertex Layout
// Position, normal and UV are all needed for basic diffuse terrain shading.
// The layout is identical to pbr_mesh.vs.hlsl so we can reuse the same
// input assembler setup code (copy-paste reduction).
// ---------------------------------------------------------------------------
struct VSInput
{
    float3 pos    : POSITION;   // Heightmap-displaced grid position (model space)
    float3 normal : NORMAL;     // Per-vertex normal (finite-difference from heights)
    float2 uv     : TEXCOORD0;  // Normalised [0,1]² UV over the terrain patch
};

// ---------------------------------------------------------------------------
// VS output — interpolated to each fragment by the rasteriser
// ---------------------------------------------------------------------------
struct PSInput
{
    float4 clipPos  : SV_POSITION;  // Required clip-space position for the rasteriser
    float3 worldPos : TEXCOORD1;    // World-space position  (for PS lighting)
    float3 worldNrm : NORMAL;       // World-space normal    (for N·L diffuse)
    float2 uv       : TEXCOORD0;    // Passthrough UV        (for future texture maps)
};

// ---------------------------------------------------------------------------
// main — vertex shader entry point
// ---------------------------------------------------------------------------
PSInput main(VSInput i)
{
    PSInput o;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Model → World → View → Clip transform chain
    // -----------------------------------------------------------------------
    // Step 1: Model space → World space.
    //   We retain worldPos because the pixel shader needs it to compute
    //   the view direction V = normalize(cameraPos - worldPos).
    //
    // Step 2: World space → Clip space.
    //   Two mul calls chained: world * view * proj.
    //   Row-vector convention (D3D11): position is on the LEFT of mul().
    // -----------------------------------------------------------------------

    float4 worldPos4 = mul(float4(i.pos, 1.0f), g_world);
    o.worldPos       = worldPos4.xyz;
    o.clipPos        = mul(mul(worldPos4, g_view), g_proj);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Normal Transform (upper 3×3 of world matrix)
    // -----------------------------------------------------------------------
    // For terrain the world matrix is typically identity (or a rigid-body TRS
    // with uniform scale).  In that case, the inverse-transpose of the upper
    // 3×3 equals the original upper 3×3 — so we can transform normals with
    // the same matrix as positions without a separate worldInvTrans upload.
    //
    // If non-uniform scale is ever applied to terrain (rare), a dedicated
    // inverse-transpose uniform would be needed — see pbr_mesh.vs.hlsl.
    // -----------------------------------------------------------------------
    o.worldNrm = normalize(mul(i.normal, (float3x3)g_world));

    o.uv = i.uv;
    return o;
}
