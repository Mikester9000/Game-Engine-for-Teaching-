/**
 * pbr_ibl.ps.hlsl
 * PBR + Image-Based Lighting (IBL) pixel shader (M16).
 *
 * ============================================================================
 * TEACHING NOTE — What is Image-Based Lighting (IBL)?
 * ============================================================================
 * Direct lighting (from a point/directional light) only accounts for a single
 * light source arriving from one direction.  In the real world, surfaces also
 * receive light from EVERY direction — reflected sky, nearby objects, ambient
 * occlusion, etc.  This indirect contribution is called "ambient" light.
 *
 * A naive implementation uses a constant ambient term (0.03 × albedo × ao),
 * which looks flat and incorrect.  Physically, the ambient contribution should
 * come from the actual environment surrounding the object.
 *
 * IBL precomputes the environment's contribution so the pixel shader can
 * evaluate it at real-time rates using just a few texture samples.
 *
 * The approach used here is the "split-sum approximation" from Epic Games'
 * 2013 SIGGRAPH presentation, which separates the reflectance integral into
 * two independent lookups:
 *
 *   1. BRDF Integration LUT (t0) — 2D texture indexed by (NoV, roughness).
 *      Stores the scale and bias of the Fresnel term.  Precomputed offline
 *      using importance sampling so the shader only needs one Sample() call.
 *
 *   2. Irradiance Cubemap (t1) — encodes the diffuse irradiance for every
 *      surface normal direction.  Computed by convolving the environment with
 *      a cosine-weighted hemisphere kernel.  The result is smooth and only
 *      needs one Sample() call per pixel.
 *
 *   3. Prefiltered Environment Cubemap (t2) — encodes the specular radiance
 *      for every reflection direction at every roughness level.  Each mip
 *      level stores the environment filtered for one roughness value.
 *      The shader calls SampleLevel() to pick the correct mip for roughness.
 *
 * ============================================================================
 * TEACHING NOTE — Split-Sum Approximation (Epic 2013)
 * ============================================================================
 * The full reflectance integral is:
 *
 *   L_spec(V) = ∫ f_specular(V,L) · L_env(L) · (N·L) dΩ
 *
 * Epic's insight: split into two independent integrals so each can be
 * precomputed separately:
 *
 *   ≈ prefilteredEnv(R, roughness)   (prefiltered env cubemap — our t2)
 *     × (F0 × scale + bias)          (BRDF LUT — our t0)
 *
 * where scale and bias depend only on (NoV, roughness) and not on F0.
 * Storing them in a 2D LUT lets the PS compute the full Fresnel IBL with
 * two inexpensive texture reads.
 *
 * ============================================================================
 * TEACHING NOTE — How IBL Textures Are Generated for This Demo
 * ============================================================================
 * A production engine loads pre-cooked HDR environment maps from disk.
 * For the teaching engine, all three IBL textures are generated procedurally
 * at scene-load time on the CPU (see LoadPBRIBLScene in D3D11Renderer.cpp):
 *
 *   BRDF LUT       — numerical integration using Hammersley sampling + GGX.
 *                    64×64 RG8_UNORM (scale in R, bias in G).
 *
 *   Irradiance     — hemisphere integration over a procedural sky gradient.
 *                    16×16 per face, 6-face D3D11_RESOURCE_MISC_TEXTURECUBE.
 *
 *   Prefiltered    — GGX importance sampling of the same procedural sky.
 *                    16×16 base, 5 mip levels (roughness 0, 0.25, 0.5, 0.75, 1).
 *
 * This approach is identical in structure to what a production engine does;
 * only the source (procedural vs. HDR file) differs.
 *
 * ============================================================================
 * TEACHING NOTE — Shader Model 4.0 Compatibility
 * ============================================================================
 * Target: ps_4_0  (D3D_FEATURE_LEVEL_10_0 compatible)
 * All intrinsics used (reflect, pow, SampleLevel, TextureCube.Sample) are
 * available in SM 4.0.
 *
 * ============================================================================
 */

// ---------------------------------------------------------------------------
// TEACHING NOTE — Constant Buffer b1 (Light + Camera)
// ---------------------------------------------------------------------------
// We need the camera world position to build the view vector V = norm(cam-pos).
// The directional light defines the single direct-light contribution.
//
// Alignment: float3 occupies 12 bytes, followed by 4 bytes of implicit padding
// to keep the next float3 on a 16-byte boundary as required by HLSL.
// ---------------------------------------------------------------------------
cbuffer LightCB : register(b1)
{
    float3 g_cameraPos;         // World-space camera position (for V vector)
    float  g_lightPad0;         // Explicit padding (HLSL packs float3 + float)
    float3 g_lightDir;          // World-space direction TO the light source
    float  g_lightPad1;
    float3 g_lightColor;        // Directional light RGB colour (linear)
    float  g_lightIntensity;    // Multiplier for the direct-light contribution
};

// ---------------------------------------------------------------------------
// TEACHING NOTE — Constant Buffer b2 (Material)
// ---------------------------------------------------------------------------
// The metallic-roughness workflow is the glTF 2.0 / Unreal / Unity HDRP
// standard.  All parameters are stored as constant values in M16 (no
// per-pixel texture maps yet — those arrive in M17 with albedo/metallic/
// roughness/AO map sampling added to this shader).
// ---------------------------------------------------------------------------
cbuffer MaterialCB : register(b2)
{
    float3 g_albedo;            // Base colour (linear sRGB)
    float  g_metallic;          // 0 = dielectric, 1 = metal
    float  g_roughness;         // 0 = mirror, 1 = fully rough
    float  g_ao;                // Ambient occlusion factor (0..1)
    float2 g_matPad;            // Padding to reach 32-byte alignment
};

// ---------------------------------------------------------------------------
// TEACHING NOTE — IBL Textures
// ---------------------------------------------------------------------------
// These three textures implement the split-sum IBL pipeline:
//
//   t0  BRDF LUT (Texture2D)
//       A 64×64 precomputed lookup table.
//       U axis = NoV (dot(N,V) clamped to [0,1]).
//       V axis = roughness (0..1).
//       R channel = integral scale (depends on F0 via 1-Fc term).
//       G channel = integral bias  (depends on Fc = pow(1-VdotH,5) term).
//       Together they let us write:
//         F_specular_IBL = F0 * brdfLut.r + brdfLut.g
//
//   t1  Irradiance Cubemap (TextureCube)
//       Encodes ∫ L_env(L) * (N·L) dΩ for every normal direction.
//       Sampled with the surface normal N (smooth result — no mip needed).
//
//   t2  Prefiltered Environment Cubemap (TextureCube, 5 mip levels)
//       Mip 0 = roughness 0.00  (mirror environment)
//       Mip 1 = roughness 0.25
//       Mip 2 = roughness 0.50
//       Mip 3 = roughness 0.75
//       Mip 4 = roughness 1.00  (fully diffuse-like environment)
//       Sampled with the reflection vector R and SampleLevel(roughness*4).
//
//   s0  Linear sampler for all IBL textures.
// ---------------------------------------------------------------------------
Texture2D   g_brdfLut          : register(t0);
TextureCube g_irradianceCube   : register(t1);
TextureCube g_prefilteredEnv   : register(t2);
SamplerState g_linearSampler   : register(s0);

// ---------------------------------------------------------------------------
// Interpolated input from the vertex shader
// ---------------------------------------------------------------------------
struct PSInput
{
    float4 clipPos  : SV_POSITION;
    float3 worldPos : TEXCOORD1;
    float3 worldNrm : NORMAL;
    float2 uv       : TEXCOORD0;
};

// ---------------------------------------------------------------------------
// TEACHING NOTE — Constants
// ---------------------------------------------------------------------------
// kPi is used in the Lambert diffuse term (albedo/π) and in the Fresnel
// formula.  Static constants in HLSL are inlined at compile time — no CB
// needed, no register consumed.
// ---------------------------------------------------------------------------
static const float kPi         = 3.14159265f;
static const float kMaxMipLevel = 4.0f;  // Mip levels 0..4 → roughness 0..1

// TEACHING NOTE — kInvGamma constant
// The gamma correction exponent 1/2.2 is applied at the very end of the PS.
// Defining it as a named constant avoids repeating the magic literal three
// times (once per channel) in the pow() call and makes the intent clear.
static const float kInvGamma   = 1.0f / 2.2f;

// ===========================================================================
// PBR Helper Functions
// ===========================================================================

// ---------------------------------------------------------------------------
// TEACHING NOTE — Fresnel-Schlick Approximation
// ---------------------------------------------------------------------------
// The Fresnel equation describes how much light is reflected vs. transmitted
// at a surface boundary as a function of the angle of incidence.
//
// Schlick's approximation (accurate to ~0.5% for most materials):
//   F(V,H) = F0 + (1 - F0) × (1 - V·H)^5
//
// At V·H = 1 (light hits head-on): F = F0             (minimum reflectance)
// At V·H = 0 (grazing incidence): F → 1               (total reflection)
//
// F0 is the reflectance at normal incidence:
//   Dielectrics (plastic, glass): F0 ≈ 0.04 (4%)
//   Metals:                        F0 = albedo (coloured reflectance)
// ---------------------------------------------------------------------------
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    float x  = 1.0f - clamp(cosTheta, 0.0f, 1.0f);
    float x5 = x * x * x * x * x;   // (1-cosTheta)^5
    return F0 + (1.0f - F0) * x5;
}

// ---------------------------------------------------------------------------
// TEACHING NOTE — Fresnel-Schlick with Roughness (for IBL ambient)
// ---------------------------------------------------------------------------
// The standard Schlick approximation assumes a smooth mirror surface.
// For rough surfaces the Fresnel contribution at grazing angles is reduced.
// Sebastien Lagarde (2012) proposed clamping the max term with roughness:
//
//   F_roughness(V,N) = F0 + (max(1-α, F0) - F0) × (1-N·V)^5
//
// where α = roughness.  This prevents very rough metals from having
// unrealistically high specular contributions at grazing angles.
// ---------------------------------------------------------------------------
float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    float  x   = 1.0f - clamp(cosTheta, 0.0f, 1.0f);
    float  x5  = x * x * x * x * x;
    float3 maxF0 = max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), F0);
    return F0 + (maxF0 - F0) * x5;
}

// ---------------------------------------------------------------------------
// TEACHING NOTE — GGX Normal Distribution Function (NDF)
// ---------------------------------------------------------------------------
// The NDF answers: "What fraction of microfacets are oriented with their
// half-vector H?"  A larger fraction means more light reflected toward V.
//
// GGX (Trowbridge-Reitz) NDF:
//
//   D(N, H, α) = α² / (π × ((N·H)² × (α²-1) + 1)²)
//
// where α = roughness².  GGX has a longer "tail" than Blinn-Phong which
// produces realistic soft highlights at medium roughness (not as square-edged
// as specular exponent models).
//
// NdotH clamped to avoid D becoming infinite at NdotH=0 and α=0 (mirror).
// ---------------------------------------------------------------------------
float DistributionGGX(float NdotH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / (kPi * d * d + 1e-7f);
}

// ---------------------------------------------------------------------------
// TEACHING NOTE — Smith-Schlick-GGX Geometry Attenuation
// ---------------------------------------------------------------------------
// The geometry function accounts for self-shadowing (masking) and self-
// shadowing from the viewer (shadowing) between microfacets.
//
// Schlick-GGX approximation (used by Unreal Engine, Unity HDRP):
//   G1(N, X, α_direct) = (N·X) / ((N·X) × (1 - k) + k)
//   where k = (roughness + 1)² / 8   (direct lighting)
//
// Smith's method multiplies shadowing and masking independently:
//   G(N, V, L) = G1(N, V) × G1(N, L)
//
// TEACHING NOTE — Why k differs for IBL vs. Direct?
// For direct lighting: k = (r+1)²/8.
// For IBL (used in the BRDF LUT precomputation): k = r²/2.
// The difference arises because IBL integrates over all incoming directions
// while direct lighting uses a single direction.  Using the direct-light k
// here (for the realtime direct-light pass) gives the correct energy.
// ---------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;   // Direct lighting variant
    return NdotV / (NdotV * (1.0f - k) + k + 1e-7f);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    return GeometrySchlickGGX(NdotV, roughness) *
           GeometrySchlickGGX(NdotL, roughness);
}

// ===========================================================================
// main — pixel shader entry point
// ===========================================================================
float4 main(PSInput i) : SV_TARGET
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Basis Vectors for the BRDF
    // -----------------------------------------------------------------------
    // N — surface normal in world space (normalised interpolated WorldNrm).
    //     The rasteriser interpolates WorldNrm across the triangle; we must
    //     renormalize because linear interpolation of unit vectors does not
    //     stay unit length.
    //
    // V — view direction FROM the surface TO the camera.
    //     V = normalize(cameraPos - worldPos)
    //
    // R — reflection direction of V around N.
    //     R = reflect(-V, N) = 2*(N·V)*N - V
    //     Used to sample the prefiltered specular cubemap.
    // -----------------------------------------------------------------------
    float3 N      = normalize(i.worldNrm);
    float3 V      = normalize(g_cameraPos - i.worldPos);
    float3 R      = reflect(-V, N);
    float  NdotV  = max(dot(N, V), 0.0f);

    // Material properties (constant for the entire sphere in M16).
    float3 albedo    = g_albedo;
    float  metallic  = g_metallic;
    float  roughness = g_roughness;
    float  ao        = g_ao;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — F0: Reflectance at Normal Incidence
    // -----------------------------------------------------------------------
    // Dielectrics have a fixed F0 ≈ 0.04 (4%).  Metals use the albedo as F0
    // (coloured specular).  lerp() interpolates between these extremes using
    // the metallic parameter.
    // -----------------------------------------------------------------------
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

    // =======================================================================
    // SECTION 1 — Direct Lighting (Cook-Torrance BRDF)
    // =======================================================================
    // TEACHING NOTE — Cook-Torrance Specular BRDF
    // The full formula for one directional light:
    //
    //   L_direct = (f_diffuse + f_specular) × lightColor × lightIntensity × (N·L)
    //
    // where:
    //   f_diffuse  = kD × albedo / π      (Lambertian diffuse, energy-conserving)
    //   f_specular = D × G × F / (4 × NdotV × NdotL)  (Cook-Torrance)
    //
    // kD = (1 - F) × (1 - metallic)       (metals have no diffuse component)
    // kS = F                              (specular fraction = Fresnel term)
    // kD + kS ≤ 1 ensures energy conservation.
    // =======================================================================
    float3 Lo = float3(0.0f, 0.0f, 0.0f);
    {
        float3 L     = normalize(g_lightDir);           // direction TO light
        float3 H     = normalize(V + L);               // half-vector
        float  NdotL = max(dot(N, L), 0.0f);
        float  NdotH = max(dot(N, H), 0.0f);
        float  VdotH = max(dot(V, H), 0.0f);

        // Specular BRDF terms.
        float  D = DistributionGGX(NdotH, roughness);
        float3 F = FresnelSchlick(VdotH, F0);
        float  G = GeometrySmith(NdotV, NdotL, roughness);

        // Cook-Torrance denominator: 4 × NdotV × NdotL.
        // Clamp denominator to 0.001 to avoid division by zero at grazing angles.
        float3 specularBRDF = (D * G * F) / max(4.0f * NdotV * NdotL, 0.001f);

        // Diffuse term: kD × albedo / π.
        // kD = (1 - F) × (1 - metallic) — metals contribute only specular.
        float3 kS_direct = F;
        float3 kD_direct = (1.0f - kS_direct) * (1.0f - metallic);
        float3 diffuseBRDF = kD_direct * albedo / kPi;

        // Add direct-light contribution (NdotL = cosine attenuation).
        Lo += (diffuseBRDF + specularBRDF) * g_lightColor * g_lightIntensity * NdotL;
    }

    // =======================================================================
    // SECTION 2 — Indirect / Ambient Lighting (IBL split-sum)
    // =======================================================================
    // TEACHING NOTE — IBL Ambient with the Split-Sum Approximation
    // -----------------------------------------------------------------------
    // We split the ambient contribution into diffuse and specular parts,
    // matching the roughness (energy distribution) of the direct pass:
    //
    //   Diffuse IBL:
    //     kD_ibl × irradiance(N) × albedo × ao
    //     where irradiance(N) = ∫ L_env(L) × (N·L) dΩ  (precomputed in t1)
    //
    //   Specular IBL:
    //     prefilteredEnv(R, roughness) × (F0 × brdfScale + brdfBias) × ao
    //     where brdfScale, brdfBias = g_brdfLut(NoV, roughness)
    //
    // The BRDF LUT stores the two constants that depend on (NoV, roughness)
    // but NOT on F0, so F0 can be applied at runtime without another sample.
    //
    // TEACHING NOTE — FresnelSchlickRoughness for IBL kS
    // We use the roughness-adjusted Fresnel for the ambient kS term so that
    // very rough metal surfaces don't have an unrealistically high ambient
    // specular contribution (which would "glow" even in full darkness).
    // =======================================================================

    // IBL ambient Fresnel (roughness-aware).
    float3 kS_ibl = FresnelSchlickRoughness(NdotV, F0, roughness);
    float3 kD_ibl = (1.0f - kS_ibl) * (1.0f - metallic);

    // --- Diffuse IBL: sample irradiance cubemap with N. ---
    // TEACHING NOTE — Why sample with N and not R?
    // The irradiance cubemap stores the integrated cosine-weighted hemisphere
    // from every DIRECTION (i.e. for a surface with normal = that direction).
    // Sampling with N gives us the total diffuse ambient for our surface.
    // Sampling with R would give specular ambient (handled separately below).
    float3 irradiance  = g_irradianceCube.Sample(g_linearSampler, N).rgb;
    float3 diffuseIBL  = kD_ibl * irradiance * albedo;

    // --- Specular IBL: split-sum lookup. ---
    // TEACHING NOTE — Mip level from roughness
    // The prefiltered env cubemap has kMaxMipLevel+1 = 5 mip levels.
    // roughness=0 → mip 0 (sharp reflections, smooth surface)
    // roughness=1 → mip 4 (blurry, diffuse-like reflections)
    float  mip             = roughness * kMaxMipLevel;
    float3 prefilteredColor = g_prefilteredEnv.SampleLevel(g_linearSampler, R, mip).rgb;

    // TEACHING NOTE — BRDF LUT lookup
    // UV = (NdotV, roughness).
    // .r = scale factor applied to F0 (accounts for specular lobe shape).
    // .g = bias  factor (accounts for Fresnel contribution at grazing angles).
    float2 brdfScale = g_brdfLut.Sample(g_linearSampler, float2(NdotV, roughness)).rg;
    float3 specularIBL = prefilteredColor * (kS_ibl * brdfScale.r + brdfScale.g);

    // Combine diffuse + specular IBL, modulated by ambient occlusion.
    float3 ambient = (diffuseIBL + specularIBL) * ao;

    // =======================================================================
    // SECTION 3 — Combine + Tonemap + Gamma
    // =======================================================================
    // TEACHING NOTE — Reinhard Tonemap + sRGB Gamma Correction
    // All lighting is computed in linear (scene-referred) colour space.
    // Before display we must:
    //   1. Tonemap: compress HDR values into [0,1] (Reinhard: c / (c+1)).
    //   2. Gamma-correct: convert linear → sRGB (approximate: pow(c, 1/2.2)).
    // Without tonemapping, bright areas (IBL + direct specular) would clip to
    // 1 (pure white) and lose detail.  Without gamma, the image looks washed
    // out because monitors expect sRGB input, not linear output.
    // =======================================================================
    float3 color = ambient + Lo;

    // Reinhard tonemap (simple but effective for teaching purposes).
    color = color / (color + float3(1.0f, 1.0f, 1.0f));

    // Approximate sRGB gamma (pow(c, 1/2.2)).
    color = pow(max(color, float3(0.0f, 0.0f, 0.0f)), float3(kInvGamma, kInvGamma, kInvGamma));

    return float4(color, 1.0f);
}
