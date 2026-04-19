/**
 * skinned_mesh.ps.hlsl
 * Pixel shader for GPU-skinned skeletal meshes.
 *
 * ============================================================================
 * TEACHING NOTE — Simple Directional Lighting (Lambertian)
 * ============================================================================
 * This pixel shader implements the simplest real-time lighting model:
 * Lambertian (diffuse-only) shading.
 *
 * Lambertian shading:
 *   diffuse = max(0, dot(N, L))
 *
 * where:
 *   N = normalised surface normal (from vertex shader, interpolated)
 *   L = normalised light direction (pointing TOWARD the light source)
 *
 * The dot product is clamped to [0, 1] with saturate() because a negative
 * value would mean the surface faces AWAY from the light (self-shadowing) and
 * should contribute 0 to the diffuse term.
 *
 * ============================================================================
 * TEACHING NOTE — Ambient + Diffuse Model
 * ============================================================================
 * A pure Lambertian model makes surfaces facing away from the light completely
 * black, which looks harsh.  We add an "ambient" term — a constant minimum
 * brightness — to simulate the indirect (bounced) light from the environment.
 *
 *   finalColor = baseColor * (ambientFactor + saturate(dot(N, L)) * diffuseFactor)
 *
 * Both factors must sum to ≤ 1 to stay physically plausible.
 *
 * ============================================================================
 * TEACHING NOTE — Color Gradient
 * ============================================================================
 * For visual clarity in the GPU skinning demo, we apply a vertical color
 * gradient using the V texture coordinate:
 *
 *   baseColor = lerp(bottomColor, topColor, uv.y)
 *
 * V = 0 at the bottom of the strip (warm orange, bone 0 = static)
 * V = 1 at the top of the strip    (cool blue,  bone 1 = animated)
 *
 * This color scheme mirrors the FF15 warp-strike particle trail: warm at the
 * origin (feet on ground) and cool blue for the warp-energy tip.
 *
 * ============================================================================
 *
 * Shader Model: ps_4_0
 * Compile:      D3DCompileFromFile("skinned_mesh.ps.hlsl", "main", "ps_4_0")
 */

// ---------------------------------------------------------------------------
// Vertex-to-pixel data (must match VSInput output from skinned_mesh.vs.hlsl)
// ---------------------------------------------------------------------------
struct PSInput
{
    float4 pos    : SV_POSITION;   // Interpolated clip position (NOT accessible)
    float3 normal : NORMAL;        // Interpolated world-space normal
    float2 uv     : TEXCOORD0;     // Interpolated UV for gradient
};

// ---------------------------------------------------------------------------
// Lighting constants (embedded in shader for simplicity)
// ---------------------------------------------------------------------------
// TEACHING NOTE — Embedded constants vs constant buffers.
// For a teaching demo, embedding lighting parameters in the shader is fine.
// In production you would put them in a per-pass constant buffer so artists
// can tweak lighting direction and colour without recompiling shaders.
// ---------------------------------------------------------------------------
static const float3 kLightDir     = float3(0.5f,  1.0f, -1.0f);  // World-space toward light
static const float  kAmbientFactor = 0.30f;
static const float  kDiffuseFactor = 0.70f;

// Color palette: cool blue (top, bone 1 / animated) to warm orange (bottom, bone 0 / static)
static const float3 kTopColor    = float3(0.20f, 0.55f, 1.00f);  // Azure blue
static const float3 kBottomColor = float3(1.00f, 0.42f, 0.08f);  // Deep orange

// ---------------------------------------------------------------------------
// main — pixel shader entry point
// ---------------------------------------------------------------------------
float4 main(PSInput i) : SV_TARGET
{
    // -----------------------------------------------------------------------
    // Step 1 — Compute the base color using the UV-based gradient.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — uv.y (V coordinate) indicates height along the strip.
    // V=0 is the bottom (bone 0 dominant, static), V=1 is the top (bone 1).
    // lerp(a, b, t) = a*(1-t) + b*t, so at V=0 we get kBottomColor and
    // at V=1 we get kTopColor.
    // -----------------------------------------------------------------------
    float3 baseColor = lerp(kBottomColor, kTopColor, saturate(i.uv.y));

    // -----------------------------------------------------------------------
    // Step 2 — Lambertian diffuse lighting.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — The light direction MUST be normalised.
    // Even though kLightDir is a constant, normalise it here because the
    // shader compiler may not fold the normalisation at compile time and it
    // guards against any future changes to kLightDir that forget to normalise.
    // -----------------------------------------------------------------------
    float3 L = normalize(kLightDir);

    // TEACHING NOTE — The normal comes from the vertex shader, interpolated
    // by the rasteriser across the triangle.  Interpolation can de-normalise
    // it slightly, so we re-normalise in the pixel shader.
    float3 N = normalize(i.normal);

    float diffuse = saturate(dot(N, L));

    // -----------------------------------------------------------------------
    // Step 3 — Combine ambient + diffuse.
    // -----------------------------------------------------------------------
    float3 finalColor = baseColor * (kAmbientFactor + diffuse * kDiffuseFactor);

    // Output full opacity.
    return float4(finalColor, 1.0f);
}
