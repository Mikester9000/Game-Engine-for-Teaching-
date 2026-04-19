// textured_quad.vs.hlsl
// Shader Model 4.0 vertex shader — pass NDC position + UV to the pixel stage.
//
// ============================================================================
// TEACHING NOTE — HLSL Vertex Shaders
// ============================================================================
// A vertex shader runs once per vertex.  Its job is to transform the vertex
// from model/world space into clip space (NDC), and to forward any data the
// pixel shader will need (here: UV coordinates).
//
// For this introductory quad we skip a projection matrix entirely — the
// vertex positions are already specified in NDC (Normalised Device Coordinates)
// where (-1,-1) is the bottom-left corner and (+1,+1) is the top-right.
// That simplification lets us focus on the texture-sampling part of the
// pipeline before adding constant buffers / matrix maths in later milestones.
//
// TEACHING NOTE — Shader Model 4.0 (vs_4_0)
// SM 4.0 maps to D3D_FEATURE_LEVEL_10_0, the minimum level we request.
// This means the shader runs on every GPU supported by this engine, including
// the GeForce GT 610.  Newer SM targets would give us compute shaders,
// geometry shaders, hull/domain shaders — all added in later milestones.
//
// TEACHING NOTE — Semantics
// HLSL uses *semantics* (SV_POSITION, TEXCOORD0, etc.) to bind struct members
// to specific pipeline slots.  The IA stage feeds POSITION into 'pos' and
// TEXCOORD0 into 'uv' based on the InputLayout descriptor created in C++
// (see D3D11Renderer::LoadScene).
// ============================================================================

struct VSInput
{
    float2 pos : POSITION;   // NDC xy (z will be set to 0, w to 1 below)
    float2 uv  : TEXCOORD0;  // Texture UV (0,0 = top-left in D3D11 convention)
};

struct PSInput
{
    float4 pos : SV_POSITION; // Required: clip-space position for rasterizer
    float2 uv  : TEXCOORD0;   // Passed through to the pixel shader
};

PSInput main(VSInput input)
{
    PSInput output;
    // Expand the 2-D NDC position to homogeneous clip-space.
    // z = 0 places the quad on the near plane (depth = 0 in reversed-Z is not
    // yet used here; depth testing will be added in a later milestone).
    // w = 1 means no perspective divide — the quad keeps its exact NDC size.
    output.pos = float4(input.pos, 0.0f, 1.0f);
    output.uv  = input.uv;
    return output;
}
