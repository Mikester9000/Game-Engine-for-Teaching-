/**
 * pbr_mesh.ps.hlsl
 * PBR (Physically Based Rendering) pixel shader.
 *
 * ============================================================================
 * TEACHING NOTE — What is Physically Based Rendering (PBR)?
 * ============================================================================
 * Physically Based Rendering is a shading model that approximates how light
 * physically interacts with surfaces.  The three core principles are:
 *
 *   1. ENERGY CONSERVATION — a surface cannot reflect more energy than it
 *      receives.  The sum of reflected + absorbed energy ≤ incoming energy.
 *      This prevents the common "bright shiny objects glow in the dark" bug
 *      of older ad-hoc lighting models.
 *
 *   2. RECIPROCITY (Helmholtz) — swapping the light and view directions
 *      gives the same result.  Required for physical accuracy and correct
 *      appearance when the camera orbits around an object.
 *
 *   3. MICROFACET THEORY — rough surfaces are modelled as a statistical
 *      distribution of tiny perfect mirrors (microfacets).  The roughness
 *      parameter controls the width of this distribution:
 *        roughness = 0  → microfacets all aligned → mirror surface
 *        roughness = 1  → random microfacet orientations → matte surface
 *
 * The model implemented here (Cook-Torrance BRDF + metallic-roughness
 * workflow) is the same used by Unreal Engine 4, Unity HDRP, Godot 4,
 * Blender Principled BSDF, and glTF 2.0.  Studying this shader means
 * studying production AAA rendering.
 *
 * ============================================================================
 * TEACHING NOTE — Metallic-Roughness Material Workflow
 * ============================================================================
 * Every surface is described by three scalar parameters:
 *
 *   albedo   (float3) — base colour of the surface.
 *                       Dielectrics (plastic, wood, stone): diffuse colour.
 *                       Metals (gold, copper, iron): F0 reflectance tint.
 *
 *   metallic (float, 0..1)
 *                0 = dielectric (plastic, rubber, ceramic)
 *                1 = conductor  (iron, gold, copper)
 *                0.5 = worn metal with surface contamination (rare IRL)
 *
 *     Dielectrics: strong Lambertian diffuse term; colourless specular (F0=0.04)
 *     Conductors:  zero diffuse (all energy becomes specular); coloured F0
 *
 *   roughness (float, 0..1)
 *                0 = perfectly smooth (sharp specular highlight / mirror)
 *                1 = completely rough (diffuse-like broad specular)
 *
 * ============================================================================
 * TEACHING NOTE — Cook-Torrance BRDF
 * ============================================================================
 * The Cook-Torrance specular BRDF for a single light:
 *
 *   f_specular = D(N, H, α) · G(N, V, L, α) · F(V, H, F0)
 *                ───────────────────────────────────────────
 *                          4 · (N·V) · (N·L)
 *
 * where α = roughness², and:
 *
 *   D — GGX Normal Distribution Function (NDF)
 *       Answers: "What fraction of microfacets are oriented so they reflect
 *       light from L toward V?"  D is large only when H ≈ N.
 *
 *   G — Smith-Schlick-GGX Geometry Attenuation
 *       Accounts for self-shadowing (masking + shadowing) between microfacets.
 *       G → 1 at normal incidence, G → 0 at grazing angles.
 *
 *   F — Schlick Fresnel Approximation
 *       Describes how reflectance increases at grazing angles.
 *       Even a matte surface like still water becomes a mirror when viewed
 *       at a shallow angle — this is the Fresnel effect.
 *
 * The full radiance equation for one directional light:
 *
 *   L_out = L_ambient                                    (indirect light)
 *         + (f_diffuse + f_specular) · N·L · lightColor  (direct light)
 *
 * ============================================================================
 * TEACHING NOTE — Absence of IBL in M9 (and how to add it)
 * ============================================================================
 * Full PBR requires Image-Based Lighting (IBL): pre-computed environment maps
 * that supply indirect diffuse (irradiance cubemap) and indirect specular
 * (prefiltered environment map + BRDF split-sum LUT).
 *
 * In this milestone we have no IBL textures, so we use a constant ambient
 * term: 0.03 × albedo × ao.  A factor of 0.03 mimics a dimly lit interior.
 *
 * To add proper IBL in a future milestone:
 *   1. Generate a diffuse irradiance cubemap from an HDRI sky texture.
 *   2. Generate a prefiltered specular cubemap (one mip per roughness level).
 *   3. Pre-integrate the BRDF into a 2D LUT (BRDFIntegrationMap).
 *   4. Replace the ambient term with:
 *        float3 irradiance  = irradianceCube.Sample(sampler, N).rgb;
 *        float2 envBRDF     = brdfLUT.Sample(sampler, float2(NdotV, roughness)).rg;
 *        float3 prefiltEnv  = prefilteredEnvCube.SampleLevel(sampler, R, roughness*MAX_MIP);
 *        float3 ambient = (kD * irradiance * albedo + prefiltEnv*(F*envBRDF.x+envBRDF.y)) * ao;
 *
 * ============================================================================
 *
 * Shader Model: ps_4_0
 * Compile:      D3DCompileFromFile("pbr_mesh.ps.hlsl", "main", "ps_4_0")
 */

// ---------------------------------------------------------------------------
// Constant buffer b1 — per-frame lighting parameters (PS slot 1)
// ---------------------------------------------------------------------------
// TEACHING NOTE — Why a separate light CB?
// The light direction and camera position change rarely (once per frame at
// most).  Keeping them in a separate cbuffer from the per-draw material
// lets the driver optimise: it can skip uploading the light CB when only
// the material changes (e.g. rendering different objects with the same light).
// ---------------------------------------------------------------------------
cbuffer LightCB : register(b1)
{
    float3 g_cameraWorldPos;  // World-space eye position (for V = cam - worldPos)
    float  g_lightIntensity;  // Scalar multiplier for the light (HDR: can be > 1)
    float3 g_lightDir;        // World-space light direction pointing TOWARD the light
    float  g_padL;            // Explicit padding — float3 is 12 bytes; needs 16-byte slot
    float3 g_lightColor;      // Linear-space light colour (e.g. warm white = (1, 1, 0.9))
    float  g_padL2;           // Explicit padding
};

// ---------------------------------------------------------------------------
// Constant buffer b2 — per-draw material parameters (PS slot 2)
// ---------------------------------------------------------------------------
// TEACHING NOTE — cbuffer Alignment
// HLSL packs cbuffer members into 16-byte slots but will NOT split a member
// across a slot boundary.  A float2 after two floats is fine (they fit in
// the same 16-byte slot: 4+4+8 = 16).  A float4 after a float3 is also fine.
// Explicit padding avoids surprises on different compiler versions.
// ---------------------------------------------------------------------------
cbuffer MaterialCB : register(b2)
{
    float3 g_albedo;     // Base colour (linear space; sRGB gamma applied at output)
    float  g_metallic;   // 0 = dielectric (plastic/stone), 1 = conductor (metal)
    float  g_roughness;  // 0 = mirror-smooth, 1 = fully rough (diffuse-like specular)
    float  g_ao;         // Ambient occlusion in [0,1] (0 = fully occluded cavity)
    float2 g_matPad;     // Pad to 16-byte boundary (4+4+8 = 16 bytes in slot 1)
};

// ---------------------------------------------------------------------------
// Vertex-to-pixel input (interpolated by rasteriser from VS output)
// ---------------------------------------------------------------------------
struct PSInput
{
    float4 clipPos  : SV_POSITION;
    float3 worldPos : TEXCOORD1;
    float3 worldNrm : NORMAL;
    float2 uv       : TEXCOORD0;
};

// ===========================================================================
// BRDF Helper Functions
// ===========================================================================

// ---------------------------------------------------------------------------
// TEACHING NOTE — GGX Normal Distribution Function (Trowbridge-Reitz NDF)
// ---------------------------------------------------------------------------
// D(N, H, roughness) answers: "What fraction of the surface's microfacets
// are oriented exactly toward H (the half-vector between V and L)?"
//
// Only microfacets aligned with H can reflect light from direction L toward
// the viewer V.  The GGX distribution:
//
//                     α²
//   D(N,H,α) = ─────────────────────────────
//              π · ((N·H)² · (α²-1) + 1)²
//
// where α = roughness².  Disney research (Burley 2012) showed that squaring
// roughness before passing it to the NDF makes the "roughness" slider feel
// more perceptually linear to artists.
//
// Key properties:
//   • D ≥ 0 for all inputs (physically valid).
//   • ∫ D(N,H) dω_h = 1 over the hemisphere (energy normalised).
//   • At roughness=0 (α=0): D is a Dirac delta spike at H=N (mirror).
//   • At roughness=1 (α=1): D = 1/π everywhere (uniform roughness).
// ---------------------------------------------------------------------------
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a      = roughness * roughness;   // α  = roughness²
    float a2     = a * a;                   // α²
    float NdotH  = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;

    // Denominator: π · ((N·H)² · (α²-1) + 1)²
    // Note: when N·H = 1 and α = 0, denom = 0.  The max() prevents
    // division-by-zero at grazing specular on a perfectly smooth surface.
    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom       = 3.14159265f * denom * denom;

    return a2 / max(denom, 0.0001f);
}

// ---------------------------------------------------------------------------
// TEACHING NOTE — Schlick-GGX Geometry Sub-function G₁
// ---------------------------------------------------------------------------
// G₁(N·X, roughness) accounts for ONE direction of self-shadowing:
//   • When X = V: MASKING  — does the surface block the view direction?
//   • When X = L: SHADOWING — does the surface block the incoming light?
//
// The Schlick-GGX approximation:
//
//   G₁(N·X, k) = (N·X) / ((N·X)·(1-k) + k)
//
// where k = (roughness+1)² / 8  (for direct lighting, Burley / Epic variant)
//
// This evaluates to 1.0 (no masking) when N·X = 1 (perfectly face-on) and
// decreases toward 0 at grazing angles (N·X → 0).
// ---------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotX, float roughness)
{
    // TEACHING NOTE — Different k for direct vs. IBL lighting.
    // Direct lighting: k = (roughness+1)²/8
    //   The +1 reduces the over-darkening that roughness²/2 produces at low
    //   roughness values for direct point/directional lights.
    // IBL (pre-integrated) lighting: k = roughness²/2
    //   Without the +1 bias; the IBL integration already accounts for it.
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;

    // Protect against division-by-zero at grazing angles (NdotX → 0).
    return NdotX / max(NdotX * (1.0f - k) + k, 0.0001f);
}

// ---------------------------------------------------------------------------
// TEACHING NOTE — Smith Geometry Function (combined masking + shadowing)
// ---------------------------------------------------------------------------
// Smith's method applies G₁ independently for the view direction (masking)
// and the light direction (shadowing), then multiplies:
//
//   G(N, V, L, roughness) = G₁(N·V, roughness) × G₁(N·L, roughness)
//
// This is an approximation of the correlated masking-shadowing function
// (Heitz 2014 gives the exact correlated form), but it is inexpensive,
// physically reasonable, and used in Epic's UE4 and most production shaders.
// ---------------------------------------------------------------------------
float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float ggxV = GeometrySchlickGGX(NdotV, roughness);  // masking  (view side)
    float ggxL = GeometrySchlickGGX(NdotL, roughness);  // shadowing (light side)
    return ggxV * ggxL;
}

// ---------------------------------------------------------------------------
// TEACHING NOTE — Schlick Fresnel Approximation
// ---------------------------------------------------------------------------
// The Fresnel effect: surfaces reflect more light at grazing angles.
// Look at a still pond from above — you can see through it.
// Look at it from the horizon — it becomes a perfect mirror.
// This happens for every material, from glass to rough stone.
//
// The exact Fresnel equations involve complex numbers and are expensive.
// The Schlick (1994) approximation is accurate and inexpensive:
//
//   F(V·H, F0) = F0 + (1 - F0) · (1 - V·H)^5
//
// where F0 = reflectance at 0° incidence (light perpendicular to surface).
//   • Non-metals (dielectrics): F0 ≈ float3(0.04) — about 4% reflectance.
//   • Metals (conductors): F0 = albedo — the "tint" of the metal's reflection.
//
// (1 - V·H)^5 is the "power 5" term that rapidly increases reflection
// toward 1.0 as the viewing angle becomes more grazing (V·H → 0).
// ---------------------------------------------------------------------------
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    // Clamp cosTheta to [0,1] to avoid pow() on a negative base (undefined).
    float ct = clamp(cosTheta, 0.0f, 1.0f);

    // TEACHING NOTE — (1-ct)^5 via repeated multiplication.
    // pow(x, 5) works in SM 4.0, but explicit multiplies are slightly faster
    // and make the intent (Schlick power-5 exponent) obvious to the reader.
    float f  = 1.0f - ct;
    float f5 = f * f * f * f * f;

    return F0 + (float3(1.0f, 1.0f, 1.0f) - F0) * f5;
}

// ===========================================================================
// Pixel shader entry point
// ===========================================================================
float4 main(PSInput i) : SV_TARGET
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Re-Normalise Interpolated Vectors
    // -----------------------------------------------------------------------
    // The rasteriser interpolates vertex attributes (including normals) using
    // bilinear interpolation across each triangle.  The interpolated result of
    // two unit vectors is generally NOT a unit vector (the midpoint of two
    // points on a unit sphere lies inside the sphere, not on it).
    //
    // Re-normalising in the pixel shader ensures N, V, L, H are all unit
    // vectors for the BRDF math, which requires them to be normalised.
    // -----------------------------------------------------------------------
    float3 N = normalize(i.worldNrm);
    float3 V = normalize(g_cameraWorldPos - i.worldPos);  // view direction (surface → camera)
    float3 L = normalize(g_lightDir);                     // light direction (surface → light)
    float3 H = normalize(V + L);                          // half-vector (bisects V and L)

    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Metallic-Roughness F0 Setup
    // -----------------------------------------------------------------------
    // F0 = base reflectance at 0° incidence.
    //
    // Dielectrics (metallic = 0):
    //   F0 = 0.04 (4% reflectance) — empirically correct for most non-metals.
    //   Exceptions: diamond (F0≈0.17), water (F0≈0.02).  In practice 0.04
    //   works for 90% of non-metal materials.
    //
    // Conductors (metallic = 1):
    //   F0 = albedo — the specular colour IS the metal's characteristic colour.
    //   Gold: F0 ≈ (1.00, 0.71, 0.29).   Silver: F0 ≈ (0.95, 0.93, 0.88).
    //   Iron: F0 ≈ (0.56, 0.57, 0.58).
    //
    // We lerp between 0.04 and albedo based on the metallic parameter.
    // In-between values (e.g. metallic=0.5) represent mixed surfaces like
    // worn metal with dust/corrosion — rare in reality but useful in games.
    // -----------------------------------------------------------------------
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), g_albedo, g_metallic);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Cook-Torrance BRDF Evaluation
    // -----------------------------------------------------------------------
    // We evaluate each BRDF term separately to make the math explicit.
    // In a production shader these would be in-lined and the compiler would
    // optimise common sub-expressions automatically.
    // -----------------------------------------------------------------------

    float  D = DistributionGGX(N, H, g_roughness);         // microfacet alignment
    float  G = GeometrySmith(NdotV, NdotL, g_roughness);   // self-shadowing
    float3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0);  // Fresnel reflectance

    // Specular BRDF:  (D · G · F) / (4 · NdotV · NdotL)
    //
    // TEACHING NOTE — The denominator 4·NdotV·NdotL
    // The Cook-Torrance denominator normalises the BRDF so it integrates
    // correctly over the hemisphere (energy conservation).  The factor of 4
    // comes from the Jacobian of the half-vector transformation.
    // We clamp to a small ε to prevent division-by-zero at grazing angles.
    float3 numerator   = D * G * F;
    float  denominator = max(4.0f * NdotV * NdotL, 0.0001f);
    float3 specular    = numerator / denominator;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Energy Conservation: the kS / kD Split
    // -----------------------------------------------------------------------
    // kS = Fresnel term = fraction of incoming light that is specularly
    //      reflected (the rest is refracted into the surface).
    // kD = (1 - kS) = fraction available for diffuse scattering.
    //
    // For pure metals (metallic=1): ALL refracted light is absorbed by the
    // conductor lattice; none undergoes subsurface scattering back out.
    // Therefore kD = 0 for metals.  The (1 - metallic) factor enforces this.
    //
    // Lambertian diffuse = albedo / π
    //   The π normalisation ensures ∫ diffuse dω_out = albedo (energy budget
    //   for the diffuse term integrates to 1 × albedo over the hemisphere).
    // -----------------------------------------------------------------------
    float3 kS = F;
    float3 kD = (float3(1.0f, 1.0f, 1.0f) - kS) * (1.0f - g_metallic);
    float3 diffuse = kD * g_albedo / 3.14159265f;

    // -----------------------------------------------------------------------
    // Direct-light contribution:
    //   Lo += (diffuse + specular) × lightRadiance × NdotL
    //
    // TEACHING NOTE — NdotL (Lambert's Cosine Law)
    // The dot product of the surface normal with the light direction is the
    // classic "diffuse factor" (Lambert's Cosine Law).  It arises in the
    // rendering equation from the projected area term: a surface tilted away
    // from the light receives less energy per unit area.
    //
    // NdotL is clamped to [0,1] so back-lit surfaces don't subtract light.
    // -----------------------------------------------------------------------
    float3 radiance = g_lightColor * g_lightIntensity;
    float3 Lo       = (diffuse + specular) * radiance * NdotL;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Ambient / IBL Approximation (M9 stub)
    // -----------------------------------------------------------------------
    // Full PBR requires Image-Based Lighting (two pre-computed cubemaps:
    // diffuse irradiance + prefiltered specular, plus a BRDF split-sum LUT).
    // These are generated offline from an HDRI sky panorama.
    //
    // For M9 we approximate indirect lighting with a constant:
    //   ambient = 0.03 × albedo × ao
    //
    // 0.03 represents roughly "a dimly lit studio" (3% of white).
    // Multiplying by ao allows mesh cavities to be darker (AO texture maps).
    //
    // When IBL textures are added in a future milestone this line becomes
    // a proper irradiance sample + prefiltered specular integration.
    // -----------------------------------------------------------------------
    float3 ambient = 0.03f * g_albedo * g_ao;

    float3 color = ambient + Lo;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Reinhard Tone Mapping
    // -----------------------------------------------------------------------
    // PBR lighting operates in HDR (High Dynamic Range): values can exceed
    // 1.0, especially for bright specular highlights or sun-like light
    // intensities.  A standard monitor can only display [0, 1].
    //
    // Tone mapping compresses the HDR range into [0, 1] while preserving
    // relative contrast.  The Reinhard operator:
    //
    //   tone(x) = x / (1 + x)
    //
    // asymptotically approaches 1 as x → ∞, so no highlight "blows out"
    // to pure white but very bright highlights do approach white gradually.
    //
    // More sophisticated operators (ACES filmic, Hable/Uncharted 2) give
    // more contrast and a warmer "filmic" look.  They are used in production
    // but have more parameters to tune.  Reinhard is shown here because it
    // demonstrates the concept with a single division.
    // -----------------------------------------------------------------------
    color = color / (color + float3(1.0f, 1.0f, 1.0f));

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Gamma Correction (Linear → sRGB)
    // -----------------------------------------------------------------------
    // ALL PBR arithmetic is done in LINEAR colour space (light intensities
    // are physical, additions model photon superposition).
    //
    // Monitors apply a gamma curve to their input signal before emitting
    // light: brightness ≈ signal^2.2.  If we output a linear value of 0.5,
    // the monitor displays 0.5^2.2 ≈ 0.22 (much darker than intended).
    //
    // We must pre-apply the INVERSE gamma before sending values to the
    // display: output = linear^(1/2.2) ≈ linear^0.4545.
    //
    // In a production pipeline this step is often handled automatically by
    // using a DXGI_FORMAT_R8G8B8A8_UNORM_SRGB render target (the hardware
    // applies gamma on Present).  We apply it in the shader here so the
    // math is visible and students can experiment with removing it to see
    // the under-darkening artefact.
    // -----------------------------------------------------------------------
    color = pow(max(color, float3(0.0f, 0.0f, 0.0f)),
                float3(1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f));

    return float4(color, 1.0f);
}
