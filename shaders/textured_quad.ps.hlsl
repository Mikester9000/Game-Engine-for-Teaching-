// textured_quad.ps.hlsl
// Shader Model 4.0 pixel shader — sample a 2D texture at the interpolated UV.
//
// ============================================================================
// TEACHING NOTE — HLSL Pixel Shaders
// ============================================================================
// A pixel shader (also called a fragment shader in OpenGL/Vulkan) runs once
// per rasterised pixel.  It receives interpolated values from the vertex
// shader — here, the UV coordinates — and outputs one or more colours that
// are written into the render target.
//
// TEACHING NOTE — Texture + Sampler binding in D3D11
// D3D11 separates the *texture data* (ID3D11ShaderResourceView → register t0)
// from the *sampling parameters* (ID3D11SamplerState → register s0).
// This separation lets you bind one texture with multiple samplers (e.g. one
// for normal rendering with bilinear filtering and another for shadow maps
// with point sampling).
//
// In HLSL:
//   Texture2D    g_texture : register(t0)  — bound by PSSetShaderResources
//   SamplerState g_sampler : register(s0)  — bound by PSSetSamplers
//
// TEACHING NOTE — SV_TARGET semantic
// SV_TARGET tells the output-merger stage that this value should be written
// to render target 0 (the back buffer in our case).
// Multiple render targets (MRT) are needed for deferred shading — not required
// at this milestone.
//
// TEACHING NOTE — Texture.Sample vs Texture.SampleLevel
// Sample(sampler, uv) uses the hardware mip-map selection heuristic based on
// how fast the UV changes across screen space (the "derivative").
// SampleLevel(sampler, uv, mipLevel) forces a specific mip — useful for
// shadow map lookups where automatic derivatives would give wrong results.
// For this quad we want normal mip selection, so we use Sample.
// ============================================================================

// Texture and sampler registered in slots 0 — must match
// PSSetShaderResources(0, ...) and PSSetSamplers(0, ...) in C++.
Texture2D    g_texture : register(t0);
SamplerState g_sampler : register(s0);

struct PSInput
{
    float4 pos : SV_POSITION; // Pixel position (screen-space); not used here
    float2 uv  : TEXCOORD0;   // Interpolated UV from the vertex shader
};

float4 main(PSInput input) : SV_TARGET
{
    // Sample the texture at the interpolated UV and output as the pixel colour.
    // The sampler state (created in D3D11Renderer::LoadScene) handles filtering
    // and wrap mode — bilinear for this quad.
    return g_texture.Sample(g_sampler, input.uv);
}
