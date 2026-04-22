/**
 * shadow.vs.hlsl
 * Vertex shader for the directional shadow map depth pass (M17).
 *
 * ============================================================================
 * TEACHING NOTE — What Is a Shadow Map?
 * ============================================================================
 * A shadow map is a depth texture that captures the scene from the point of
 * view of the light source.  Every texel stores how far the nearest visible
 * surface is from the light.  Later, when rendering the scene from the camera,
 * we transform each fragment's world position back into light clip space and
 * compare its depth to the stored value:
 *
 *   depth_fragment > depth_shadowMap + bias  →  in shadow  (occluded)
 *   depth_fragment ≤ depth_shadowMap + bias  →  lit        (visible to light)
 *
 * The result is a per-fragment binary shadow factor (0 = shadow, 1 = lit)
 * which is used to modulate the lighting contribution.
 *
 * ============================================================================
 * TEACHING NOTE — Shadow Pass Vertex Shader
 * ============================================================================
 * The shadow pass only needs to write depth to the depth-stencil buffer.
 * No colour output is required — we bind a depth-stencil view (DSV) but NO
 * render-target view (RTV) during this pass.  The vertex shader only needs to:
 *
 *   1. Read the vertex position.
 *   2. Transform it by the light's combined view-projection matrix.
 *   3. Output the clip-space position via SV_POSITION.
 *
 * The rasteriser fills the depth buffer automatically from SV_POSITION.z/w.
 * Normal and UV attributes are declared in the input struct only to share the
 * same D3D11_INPUT_ELEMENT_DESC (input layout) with the lit-pass VS — reusing
 * one layout object avoids an extra CreateInputLayout call.
 *
 * ============================================================================
 * TEACHING NOTE — Light View-Projection Matrix
 * ============================================================================
 * For a directional light (parallel rays, infinite distance) we use an
 * orthographic projection.  The orthographic matrix maps a box-shaped view
 * volume into NDC without perspective foreshortening.  This gives uniform
 * shadow resolution across the entire frustum — unlike a perspective projection
 * which wastes resolution far from the camera.
 *
 * The light-space transform chain:
 *   lightViewProj = LookAt(lightDir, sceneCenter) × Ortho(width, height, near, far)
 *
 * Shader Model: vs_4_0  (D3D_FEATURE_LEVEL_10_0 compatible)
 */

// ---------------------------------------------------------------------------
// Constant buffer b0 — light view-projection matrix
// ---------------------------------------------------------------------------
// TEACHING NOTE — Why Only One Matrix?
// The shadow pass does not need separate world, view, and projection matrices.
// The light transform is precomputed on the CPU into a single 4×4 matrix
// (lightViewProj = world × view × proj) and uploaded as one CB.  This reduces
// the number of matrix multiplications in the shader from 3 to 1.
// ---------------------------------------------------------------------------
cbuffer ShadowCB : register(b0)
{
    float4x4 g_lightViewProj;   ///< Light view × projection (orthographic, precomputed)
};

// ---------------------------------------------------------------------------
// Vertex input — matches the lit-pass layout (POSITION + NORMAL + TEXCOORD0)
// ---------------------------------------------------------------------------
// TEACHING NOTE — Shared Input Layout
// Both the shadow-pass VS (this file) and the lit-pass VS (shadow_lit.vs.hlsl)
// use the same D3D11_INPUT_ELEMENT_DESC array, so only one ID3D11InputLayout
// object is needed for the whole shadow scene.  Sharing input layouts reduces
// state change overhead on the GPU — changing an input layout flushes part of
// the geometry pipeline.
// ---------------------------------------------------------------------------
struct VSInput
{
    float3 pos    : POSITION;   ///< Model-space vertex position
    float3 normal : NORMAL;     ///< Unused in the depth pass (layout match only)
    float2 uv     : TEXCOORD0;  ///< Unused in the depth pass (layout match only)
};

// ---------------------------------------------------------------------------
// main — transform position to light clip space and output depth
// ---------------------------------------------------------------------------
float4 main(VSInput i) : SV_POSITION
{
    // TEACHING NOTE — Single Matrix Multiply for Shadow Pass
    // The light-view-proj matrix encodes the full light camera transform.
    // Multiplying the model-space position by this matrix in one step gives
    // us the clip-space position from the light's point of view.
    //
    // The hardware rasteriser then interpolates clip.z/clip.w across each
    // triangle and writes the result to the depth-stencil buffer.
    //
    // No normal or UV transformation is required because the shadow pass
    // only writes depth — not colour.
    return mul(float4(i.pos, 1.0f), g_lightViewProj);
}
