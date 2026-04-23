/**
 * terrain.ps.hlsl
 * Pixel shader for heightmap-driven terrain grid (M25).
 *
 * ============================================================================
 * TEACHING NOTE — Terrain Lighting and Colour Model
 * ============================================================================
 * Real AAA terrain renderers blend many texture layers (grass, rock, snow,
 * sand) based on slope and altitude, and use a full PBR pipeline with
 * normal maps and roughness maps.  That complexity is introduced in later
 * milestones.  Here we demonstrate the foundational pattern with the
 * simplest correct implementation:
 *
 *   1. Height-based colour blending
 *      Low terrain (y ≈ 0) → grass green.
 *      High terrain (y ≈ maxHeight) → rock grey.
 *      lerp between them linearly.  This is the same trick used for
 *      quick "programmer art" terrain in many indie and teaching projects.
 *
 *   2. Lambert (N·L) diffuse
 *      Lights the terrain with a single directional sun.  The light
 *      direction comes from the shared TerrainCB constant buffer.
 *      NdotL = max(0, dot(N, L)) — negative values mean the surface
 *      faces away from the sun and receives only ambient light.
 *
 *   3. Ambient term
 *      A small constant ambient ensures shadowed faces are not pure black.
 *      Value: 0.25 × base colour.
 *
 *   4. Reinhard tonemap + gamma correction
 *      Same post-processing as pbr_mesh.ps.hlsl so the terrain integrates
 *      correctly with other passes that also apply Reinhard tonemap.
 *
 * ============================================================================
 * TEACHING NOTE — Shared Constant Buffer (TerrainCB)
 * ============================================================================
 * The same cbuffer from terrain.vs.hlsl is declared here so the PS can
 * access g_lightDir.  In D3D11, both shaders bind the SAME buffer object
 * to their respective stages (VSSetConstantBuffers + PSSetConstantBuffers),
 * so there is no data duplication — this declaration just tells the HLSL
 * compiler what the layout is.
 *
 * ============================================================================
 * TEACHING NOTE — Shader Model 4.0 (Feature Level 10_0 Compatible)
 * ============================================================================
 * Target: ps_4_0.
 * All intrinsics used (normalize, dot, saturate, lerp, pow) are SM 4.0.
 *
 * ============================================================================
 *
 * Shader Model: ps_4_0
 * Compile:      D3DCompileFromFile("terrain.ps.hlsl", "main", "ps_4_0")
 */

// ---------------------------------------------------------------------------
// Constant buffer b0 — shared with terrain.vs.hlsl
// ---------------------------------------------------------------------------
cbuffer TerrainCB : register(b0)
{
    float4x4 g_world;      // (not used in PS, but must match VS layout)
    float4x4 g_view;       // (not used in PS)
    float4x4 g_proj;       // (not used in PS)
    float3   g_lightDir;   // Directional light direction (world space)
    float    g_pad0;       // Alignment pad
};

// ---------------------------------------------------------------------------
// PS input — matches PSInput from terrain.vs.hlsl
// ---------------------------------------------------------------------------
struct PSInput
{
    float4 clipPos  : SV_POSITION;
    float3 worldPos : TEXCOORD1;
    float3 worldNrm : NORMAL;
    float2 uv       : TEXCOORD0;
};

// ---------------------------------------------------------------------------
// main — pixel shader entry point
// ---------------------------------------------------------------------------
float4 main(PSInput i) : SV_Target
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Normal reconstruction
    // -----------------------------------------------------------------------
    // Even though the vertex shader normalises worldNrm, interpolation across
    // a triangle can shrink it below unit length.  Re-normalise in the PS
    // to ensure N·L is computed with a true unit vector.
    // -----------------------------------------------------------------------
    float3 N = normalize(i.worldNrm);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Light direction convention
    // -----------------------------------------------------------------------
    // g_lightDir points FROM the surface TOWARD the sun (not from sun toward
    // surface).  Convention: L = -direction_from_sun_to_scene.
    // We normalise here to allow the CPU to pass a non-unit vector for convenience.
    // -----------------------------------------------------------------------
    float3 L = normalize(g_lightDir);

    // Lambert diffuse term
    float NdotL = saturate(dot(N, L));

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Height-based colour blending
    // -----------------------------------------------------------------------
    // Grass covers low-altitude terrain; rock/stone appears at higher altitudes.
    // We map world Y into [0,1] using a designer-tunable maxHeight value (8.0 m).
    // saturate() clamps the result to [0,1] so out-of-range heights don't
    // produce artefacts.
    //
    // Future extension: add a slope-based blend (steep slopes → rock regardless
    // of height) by using dot(N, float3(0,1,0)) as a second lerp weight.
    // -----------------------------------------------------------------------
    float heightT    = saturate(i.worldPos.y / 8.0f);
    float3 grassColor = float3(0.15f, 0.50f, 0.12f);   // Lush green
    float3 rockColor  = float3(0.45f, 0.42f, 0.38f);   // Grey stone
    float3 baseColor  = lerp(grassColor, rockColor, heightT);

    // Ambient + diffuse combination
    float3 ambient = 0.25f * baseColor;
    float3 diffuse = NdotL  * baseColor;
    float3 finalLinear = ambient + diffuse;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Reinhard Tonemap + Gamma Correction
    // -----------------------------------------------------------------------
    // Reinhard tonemap: maps any HDR value to [0,1]:
    //   L_display = L_hdr / (1 + L_hdr)
    //
    // Gamma correction converts from linear radiance to the sRGB display curve
    // that monitors expect (gamma = 2.2, approximated by the 1/2.2 power).
    //
    // Both operations are identical to pbr_mesh.ps.hlsl so all scenes in the
    // engine share a consistent look.
    // -----------------------------------------------------------------------
    finalLinear = finalLinear / (finalLinear + float3(1.0f, 1.0f, 1.0f));
    float3 finalGamma = pow(abs(finalLinear), 1.0f / 2.2f);

    return float4(finalGamma, 1.0f);
}
