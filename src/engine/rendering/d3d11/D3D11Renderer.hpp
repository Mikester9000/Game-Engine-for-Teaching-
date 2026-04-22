/**
 * @file D3D11Renderer.hpp
 * @brief Direct3D 11 renderer — GT610-compatible Windows baseline.
 *
 * ============================================================================
 * TEACHING NOTE — Why Direct3D 11?
 * ============================================================================
 * D3D11 is the sweet spot for a "runs everywhere on Windows" renderer:
 *
 *   • Ships with Windows 7, 8, 10, 11 — zero end-user setup.
 *   • Supported by every discrete GPU from ~2006 onwards (Feature Level 10_0).
 *   • A GeForce GT 610 (2012 Kepler-rebrand / Fermi) supports FL 11_0.
 *   • D3D11 WARP (Windows Advanced Rasterization Platform) is a CPU software
 *     rasteriser bundled with Windows — it means CI runners and machines
 *     without a physical GPU can still run the renderer for validation.
 *
 * Feature Level Baseline:
 *   We request [11_0, 10_1, 10_0] in order.  The device is created at the
 *   highest level the hardware supports; at minimum 10_0 is required.
 *
 * ============================================================================
 * TEACHING NOTE — D3D11 Device Hierarchy
 * ============================================================================
 * D3D11 has three core objects:
 *
 *   ID3D11Device         — the logical GPU.  Used for resource creation
 *                          (buffers, textures, shaders, states).  Thread-safe
 *                          when the flag D3D11_CREATE_DEVICE_SINGLETHREADED
 *                          is NOT set.
 *
 *   ID3D11DeviceContext  — the immediate drawing context.  Records draw /
 *                          dispatch commands and executes them.  NOT thread-
 *                          safe (you need deferred contexts for MT recording).
 *
 *   IDXGISwapChain       — the flip chain that presents rendered frames to
 *                          the OS compositor.  Belongs to DXGI (DirectX
 *                          Graphics Infrastructure), not D3D11 directly.
 *
 * ============================================================================
 * TEACHING NOTE — WARP (Software Renderer) for Headless CI
 * ============================================================================
 * When --headless is passed (or ENGINE_D3D11_FORCE_WARP is defined) we create
 * the device with D3D_DRIVER_TYPE_WARP instead of D3D_DRIVER_TYPE_HARDWARE.
 * WARP:
 *   • Requires no GPU driver.
 *   • Runs on every GitHub-hosted Windows runner out of the box.
 *   • Supports FL 11_0 in software.
 *   • Is slower than hardware but correct for validation.
 * In headless mode we also skip IDXGISwapChain creation — WARP has no window
 * to present to, and we only need device-creation validation for CI.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Target: Windows (MSVC)
 * Requires: d3d11.lib, dxgi.lib, d3dcompiler.lib (Windows SDK — always present)
 */

#pragma once

#include "engine/rendering/IRenderer.hpp"
#include "engine/rendering/d3d11/d3d11_texture.hpp"
#include "engine/animation/gpu_skinning.hpp"
#include "engine/rendering/sky_renderer.hpp"

// ---------------------------------------------------------------------------
// TEACHING NOTE — D3D11 / DXGI Headers
// ---------------------------------------------------------------------------
// These headers ship with the Windows SDK — no separate download needed.
// d3d11.h     — D3D11 device, context, resource types.
// dxgi.h      — DXGI swap chain, adapter, factory types.
// d3dcompiler.h — runtime HLSL compilation via D3DCompileFromFile.
// ---------------------------------------------------------------------------
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>

#include <cstdint>
#include <string>

namespace engine {
namespace rendering {

// ===========================================================================
// class D3D11Renderer
// ===========================================================================
// Concrete IRenderer implementation using Direct3D 11.
//
// TEACHING NOTE — Object Lifecycle
// All COM objects (ID3D11Device, etc.) are managed via raw COM pointers.
// We call Release() manually in Shutdown() in reverse-creation order.
// An alternative is Microsoft::WRL::ComPtr<T> which auto-releases; we use
// raw pointers here so the release sequence is explicit and teachable.
// ===========================================================================
class D3D11Renderer : public IRenderer
{
public:
    D3D11Renderer();
    ~D3D11Renderer() override;

    // No copy — COM pointers are not reference-counted here.
    D3D11Renderer(const D3D11Renderer&)            = delete;
    D3D11Renderer& operator=(const D3D11Renderer&) = delete;

    // -----------------------------------------------------------------------
    // IRenderer interface implementation
    // -----------------------------------------------------------------------

    bool Init(HINSTANCE hinstance, HWND hwnd,
              uint32_t width, uint32_t height,
              bool headless = false) override;

    void Shutdown() override;

    void DrawFrame(float clearR, float clearG, float clearB) override;

    void RecreateSwapchain(uint32_t width, uint32_t height) override;

    bool LoadScene(const std::string& sceneName,
                   const std::string& shaderDir) override;

    bool RecordHeadlessFrame() override;

    const char* BackendName() const override { return "D3D11"; }

    // -----------------------------------------------------------------------
    // D3D11-specific accessors (for advanced use)
    // -----------------------------------------------------------------------

    /** @return The D3D11 device, or nullptr before Init(). */
    ID3D11Device*        GetDevice()        const { return m_device; }

    /** @return The immediate device context, or nullptr before Init(). */
    ID3D11DeviceContext* GetContext()       const { return m_context; }

    /** @return The active feature level that was negotiated at device creation. */
    D3D_FEATURE_LEVEL    GetFeatureLevel()  const { return m_featureLevel; }

private:
    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    /** Create the DXGI swap chain and size-dependent render targets. */
    bool CreateSwapChainResources(HWND hwnd, uint32_t width, uint32_t height);

    /** Release swap-chain-size-dependent objects (before a resize). */
    void ReleaseSwapChainResources();

    /** Draw the textured quad scene to the currently bound render target. */
    void DrawTexturedQuad();

    /** Draw the GPU-skinned mesh scene to the currently bound render target. */
    void DrawSkinnedMesh();

    /** Draw the PBR sphere scene to the currently bound render target (M9). */
    void DrawPBRMesh();

    // -----------------------------------------------------------------------
    // TEACHING NOTE — DrawPBRIBLMesh (M16)
    // -----------------------------------------------------------------------
    // Renders the same UV sphere as DrawPBRMesh() but with a full IBL ambient
    // contribution from three procedurally-generated textures:
    //   • BRDF LUT (64×64 RG8_UNORM) — precomputed split-sum lookup.
    //   • Irradiance cubemap (16×16×6 RGB8) — diffuse environment integral.
    //   • Prefiltered env cubemap (16×16×6 RGB8, 5 mip levels) — specular.
    // Also uses the depth buffer (m_depthStencilView) added in M16.
    // -----------------------------------------------------------------------
    /** Draw the PBR+IBL sphere scene to the currently bound render target (M16). */
    void DrawPBRIBLMesh();

    // -----------------------------------------------------------------------
    // TEACHING NOTE — DrawSky (M10)
    // -----------------------------------------------------------------------
    // DrawSky() renders a full-screen procedural sky using SV_VertexID (no
    // vertex buffer required).  The sky constant buffer is updated from
    // m_skyRenderer every frame.  DrawSky() should be called BEFORE any
    // opaque 3D geometry so the sky fills in only where no geometry covers it.
    // -----------------------------------------------------------------------
    /** Draw the procedural sky to the currently bound render target (M10). */
    void DrawSky();

    // -----------------------------------------------------------------------
    // TEACHING NOTE — DrawShadowScene (M17)
    // -----------------------------------------------------------------------
    // DrawShadowScene() executes both shadow-rendering passes:
    //   Pass 1 (shadow): binds the shadow-map DSV (no colour RTV), renders
    //                    the sphere from the light's view, writes depth.
    //   Pass 2 (lit):    restores the caller's RTV, renders the sphere from
    //                    the camera view with PCF shadow sampling.
    //
    // Using OMGetRenderTargets / OMSetRenderTargets to save and restore the
    // active RTV makes DrawShadowScene independent of context — it can be
    // called from both DrawFrame (windowed) and RecordHeadlessFrame (CI).
    // -----------------------------------------------------------------------
    /** Execute both shadow-map and lit passes for the shadow demo (M17). */
    void DrawShadowScene();

    // -----------------------------------------------------------------------
    // TEACHING NOTE — DrawBloomScene (M17)
    // -----------------------------------------------------------------------
    // DrawBloomScene() executes the four-pass bloom pipeline:
    //   1. Fill scene RT with a bright procedural colour (simulated HDR).
    //   2. Bright-pass: extract luminance > threshold → brightRTV.
    //   3. Horizontal Gaussian blur → blurARTV (ping).
    //   4. Vertical   Gaussian blur → blurBRTV (pong = final bloom).
    //   5. Composite: sceneRTV + blurBRTV → caller's current RTV.
    //
    // Step 5 composites back to whatever RTV the caller had bound, so this
    // method works identically in both windowed (back buffer) and headless
    // (64×64 offscreen texture) contexts.
    // -----------------------------------------------------------------------
    /** Execute the full bloom pipeline and composite to the current RTV (M17). */
    void DrawBloomScene();

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Depth Buffer Helpers (M16)
    // -----------------------------------------------------------------------
    // D3D11 does not automatically create a depth buffer when you create a
    // swap chain.  You must explicitly:
    //   1. Create an ID3D11Texture2D with D3D11_BIND_DEPTH_STENCIL.
    //   2. Create an ID3D11DepthStencilView from that texture.
    //   3. Create an ID3D11DepthStencilState that enables depth testing.
    //   4. Bind the DSV alongside the RTV in OMSetRenderTargets().
    //   5. Clear the DSV at the start of each frame.
    //
    // The helpers below are called from CreateSwapChainResources (create) and
    // ReleaseSwapChainResources (release) so the depth buffer is always
    // sized to match the current back buffer.
    // -----------------------------------------------------------------------
    /** Create the depth-stencil buffer for the given back-buffer dimensions. */
    bool CreateDepthStencilBuffer(uint32_t width, uint32_t height);

    /** Release the depth-stencil texture, view, and state. */
    void ReleaseDepthStencilBuffer();

    /** Release all scene resources (called from Shutdown and before LoadScene). */
    void UnloadScene();

    // -----------------------------------------------------------------------
    // D3D11 / DXGI objects
    // -----------------------------------------------------------------------

    // TEACHING NOTE — COM pointer naming convention
    // We prefix all COM interface pointers with m_ (member) and use the
    // interface name as the type hint.  e.g. m_device is an ID3D11Device*.

    ID3D11Device*           m_device        = nullptr;
    ID3D11DeviceContext*    m_context       = nullptr;
    IDXGISwapChain*         m_swapChain     = nullptr;
    ID3D11RenderTargetView* m_renderTarget  = nullptr;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Depth-Stencil Buffer (M16)
    // -----------------------------------------------------------------------
    // Before M16 the renderer had no depth buffer.  All scenes rendered either
    // a 2D quad (no depth needed) or a sphere that only ever occupies the
    // centre of the screen (depth wasn't visible).  Adding a DSV is necessary
    // to correctly composite multiple 3D objects — later milestones will have
    // foreground geometry occlude background geometry correctly.
    //
    // Three objects work together:
    //   m_depthStencilTex   — the raw D3D11 Texture2D.
    //                         Format: DXGI_FORMAT_D24_UNORM_S8_UINT
    //                         (24-bit depth + 8-bit stencil — the most
    //                         compatible depth format across all FL 10_0 GPUs).
    //
    //   m_depthStencilView  — allows D3D11 to use the texture as a depth
    //                         attachment.  Passed to OMSetRenderTargets().
    //
    //   m_depthStencilState — enables depth testing and writing.
    //                         D3D11_COMPARISON_LESS: keep the fragment with
    //                         the SMALLER depth value (the closer fragment).
    //
    // All three are recreated in CreateSwapChainResources() and released in
    // ReleaseSwapChainResources() so they always match the back-buffer size.
    // -----------------------------------------------------------------------
    ID3D11Texture2D*         m_depthStencilTex   = nullptr;
    ID3D11DepthStencilView*  m_depthStencilView  = nullptr;
    ID3D11DepthStencilState* m_depthStencilState = nullptr;

    D3D_FEATURE_LEVEL       m_featureLevel  = D3D_FEATURE_LEVEL_10_0;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Storing Back-Buffer Dimensions
    // -----------------------------------------------------------------------
    // We cache the current back-buffer size so that DrawFrame can set the
    // viewport correctly on every frame.  Without an explicit viewport the
    // rasteriser uses a full-surface default on some drivers, but it is
    // better practice to set it explicitly so the behaviour is predictable
    // across hardware and WARP.
    // -----------------------------------------------------------------------
    uint32_t                m_width         = 0;
    uint32_t                m_height        = 0;

    bool                    m_headless      = false;
    bool                    m_initialised   = false;

public:
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Public Scene-Resource Structs
    // -----------------------------------------------------------------------
    // These inner structs are public to allow static helper functions in the
    // .cpp file to create and populate them without requiring friendship.
    // They contain only D3D11 COM pointers and are plain aggregates —
    // making them public does not expose any hidden invariants.
    //
    // TexturedQuadScene (M3): resources for the UV-mapped quad.
    // SkinnedMeshScene  (M4b): resources for the GPU-skinned strip.
    // -----------------------------------------------------------------------

    struct TexturedQuadScene
    {
        ID3D11VertexShader*       vs             = nullptr;
        ID3D11PixelShader*        ps             = nullptr;
        ID3D11InputLayout*        inputLayout    = nullptr;
        ID3D11Buffer*             vertexBuf      = nullptr;
        ID3D11Buffer*             indexBuf       = nullptr;

        D3D11Texture              texture;

        ID3D11ShaderResourceView* fallbackSRV     = nullptr;
        ID3D11SamplerState*       fallbackSampler  = nullptr;

        bool                      useFallbackTex   = false;
        bool                      loaded           = false;
    };

    struct SkinnedMeshScene
    {
        ID3D11VertexShader*       vs          = nullptr;
        ID3D11PixelShader*        ps          = nullptr;
        ID3D11InputLayout*        inputLayout = nullptr;
        ID3D11Buffer*             vertexBuf   = nullptr;
        ID3D11Buffer*             indexBuf    = nullptr;
        ID3D11RasterizerState*    rastState   = nullptr;

        engine::animation::GpuSkinningBuffer skinningCB;

        int  indexCount = 0;
        bool loaded     = false;
    };

    // -----------------------------------------------------------------------
    // TEACHING NOTE — PBRScene (M9: Physically Based Rendering)
    // -----------------------------------------------------------------------
    // PBRScene holds every Direct3D 11 resource required to render a
    // physically-based (Cook-Torrance BRDF) sphere under a directional light.
    //
    // Resources:
    //   vs / ps          — HLSL vertex + pixel shaders (SM 4.0).
    //   inputLayout      — describes each vertex attribute to the IA stage.
    //   vertexBuf        — UV sphere geometry (pos + normal + uv per vertex).
    //   indexBuf         — triangle list indices for the sphere.
    //   perFrameCB (b0)  — per-frame transform matrices (world, view, proj).
    //   lightCB    (b1)  — directional light and camera world position.
    //   materialCB (b2)  — albedo, metallic, roughness, ambient-occlusion.
    //   rastState        — cull-none so both faces of the sphere are visible
    //                      from any camera angle without winding-order issues.
    //
    // Constant buffer update frequency:
    //   perFrameCB  — every frame  (world matrix rotates with m_sceneTime).
    //   lightCB     — once on load  (light direction does not change).
    //   materialCB  — once on load  (single material sphere demo).
    // -----------------------------------------------------------------------
    struct PBRScene
    {
        ID3D11VertexShader*    vs          = nullptr;
        ID3D11PixelShader*     ps          = nullptr;
        ID3D11InputLayout*     inputLayout = nullptr;
        ID3D11Buffer*          vertexBuf   = nullptr;
        ID3D11Buffer*          indexBuf    = nullptr;
        ID3D11Buffer*          perFrameCB  = nullptr;   ///< b0 (VS): world, worldInvTrans, view, proj
        ID3D11Buffer*          lightCB     = nullptr;   ///< b1 (PS): camera pos, light dir/color/intensity
        ID3D11Buffer*          materialCB  = nullptr;   ///< b2 (PS): albedo, metallic, roughness, ao
        ID3D11RasterizerState* rastState   = nullptr;
        int                    indexCount  = 0;
        bool                   loaded      = false;
    };

    // -----------------------------------------------------------------------
    // TEACHING NOTE — PBRIBLScene (M16: Image-Based Lighting)
    // -----------------------------------------------------------------------
    // PBRIBLScene extends PBRScene with three IBL textures that implement
    // the split-sum ambient lighting model (Epic 2013):
    //
    //   brdfLutSRV      (t0) — 2D lookup table for BRDF scale + bias.
    //                          Indexed by (NoV, roughness); precomputed on CPU.
    //
    //   irradianceSRV   (t1) — Diffuse irradiance cubemap.
    //                          Encodes ∫ L_env(L) × (N·L) dΩ per direction.
    //                          Computed from a procedural sky gradient.
    //
    //   prefilteredSRV  (t2) — Prefiltered specular cubemap, 5 mip levels.
    //                          Mip k ↔ roughness = k/4.  GGX importance sampled.
    //
    //   linearSampler        — Linear clamp sampler bound to s0 for all IBL
    //                          textures.
    //
    // The raw ID3D11Texture2D* members are kept separately so UnloadScene()
    // can Release() them (the SRVs do NOT release the underlying textures when
    // their ref-count goes to zero if the device also holds a reference).
    //
    // This struct stores the same geometry + CB resources as PBRScene (it
    // renders the same UV sphere), plus the IBL additions above.
    // -----------------------------------------------------------------------
    struct PBRIBLScene
    {
        ID3D11VertexShader*       vs             = nullptr;
        ID3D11PixelShader*        ps             = nullptr;
        ID3D11InputLayout*        inputLayout    = nullptr;
        ID3D11Buffer*             vertexBuf      = nullptr;
        ID3D11Buffer*             indexBuf       = nullptr;
        ID3D11Buffer*             perFrameCB     = nullptr;  ///< b0 (VS): transform matrices
        ID3D11Buffer*             lightCB        = nullptr;  ///< b1 (PS): camera pos + dir light
        ID3D11Buffer*             materialCB     = nullptr;  ///< b2 (PS): material parameters

        // IBL textures
        ID3D11ShaderResourceView* brdfLutSRV     = nullptr;  ///< t0: BRDF LUT 2D
        ID3D11ShaderResourceView* irradianceSRV  = nullptr;  ///< t1: irradiance cubemap
        ID3D11ShaderResourceView* prefilteredSRV = nullptr;  ///< t2: prefiltered env cubemap

        // Raw textures (kept for Release() in UnloadScene)
        ID3D11Texture2D*          brdfLutTex     = nullptr;
        ID3D11Texture2D*          irradianceTex  = nullptr;
        ID3D11Texture2D*          prefilteredTex = nullptr;

        ID3D11SamplerState*       linearSampler  = nullptr;  ///< s0: linear clamp
        ID3D11RasterizerState*    rastState      = nullptr;

        int   indexCount = 0;
        bool  loaded     = false;
    };

    // -----------------------------------------------------------------------
    // TEACHING NOTE — SkyScene (M10: Dynamic Sky + Weather VFX)
    // -----------------------------------------------------------------------
    // SkyScene is the simplest scene struct: it only needs a VS, PS, and a
    // single constant buffer.
    //
    //   vs             — sky.vs.hlsl: SV_VertexID full-screen triangle
    //   ps             — sky.ps.hlsl: gradient + sun disc + fog + weather
    //   skyConstantsCB — b0 (PS): SkyShaderConstants packed struct
    //                    updated every frame from m_skyRenderer.GetShaderConstants()
    //
    // No vertex buffer is required because the sky VS generates all three
    // vertices procedurally from SV_VertexID (see sky.vs.hlsl).
    //
    // No input layout is required because there is no vertex buffer to
    // describe.  D3D11 accepts a null input layout when IA::SetInputLayout(null)
    // is called — the shader does not read from the IA stage at all.
    //
    // No rasterizer state override is needed: the default cull-back state is
    // fine because the full-screen triangle never faces away from the camera.
    // -----------------------------------------------------------------------
    struct SkyScene
    {
        ID3D11VertexShader* vs             = nullptr;
        ID3D11PixelShader*  ps             = nullptr;
        ID3D11Buffer*       skyConstantsCB = nullptr;  ///< b0 (PS): SkyShaderConstants
        bool                loaded         = false;
    };

    // -----------------------------------------------------------------------
    // TEACHING NOTE — ShadowScene (M17: Directional Shadow Maps)
    // -----------------------------------------------------------------------
    // ShadowScene implements a classic two-pass shadow algorithm:
    //
    //   Pass 1 — Shadow Pass (shadow.vs.hlsl, no PS, no colour RTV):
    //     Renders the scene from the LIGHT'S viewpoint into a 512×512 depth
    //     texture (shadowTex / shadowDSV).  The depth values record how far
    //     each surface is from the light — the "shadow map".
    //
    //   Pass 2 — Lit Pass (shadow_lit.vs/ps.hlsl, camera RTV):
    //     Renders the scene from the CAMERA'S viewpoint.  Each fragment re-
    //     projects its world position into light space and samples the shadow
    //     map via a 3×3 PCF kernel (SamplerComparisonState in the PS).
    //     Fragments deeper than the stored value are "in shadow".
    //
    // Key resources:
    //   shadowTex / shadowDSV — the 512×512 D32_FLOAT depth render target.
    //   shadowSRV             — read-only SRV bound to the lit PS (t0).
    //   cmpSampler            — D3D11_COMPARISON_LESS_EQUAL hardware PCF sampler.
    //   shadowCB (b0, shadow VS) — 64-byte lightViewProj matrix (ortho).
    //   litCB    (b0, lit VS+PS) — 272-byte world/view/proj + lightViewProj + lightDir.
    //   shadowRast             — cull-front rasterizer with depth bias.
    //   shadowDSS              — depth test + write enabled (no stencil).
    // -----------------------------------------------------------------------
    struct ShadowScene
    {
        // Shadow map texture + views.
        ID3D11Texture2D*          shadowTex    = nullptr;  ///< 512×512 D32_FLOAT depth texture
        ID3D11DepthStencilView*   shadowDSV    = nullptr;  ///< DSV: shadow pass renders here
        ID3D11ShaderResourceView* shadowSRV    = nullptr;  ///< SRV: lit PS samples this (t0)

        // Shadow pass (depth-only, no colour output).
        ID3D11VertexShader*       shadowVS     = nullptr;  ///< shadow.vs.hlsl
        ID3D11InputLayout*        shadowLayout = nullptr;  ///< pos+normal+uv (shared with lit)
        ID3D11Buffer*             shadowCB     = nullptr;  ///< b0: lightViewProj (64 B)
        ID3D11DepthStencilState*  shadowDSS    = nullptr;  ///< depth test + write
        ID3D11RasterizerState*    shadowRast   = nullptr;  ///< depth bias, cull-back

        // Lit pass (camera view, PCF shadow lookup).
        ID3D11VertexShader*       litVS        = nullptr;  ///< shadow_lit.vs.hlsl
        ID3D11PixelShader*        litPS        = nullptr;  ///< shadow_lit.ps.hlsl
        ID3D11Buffer*             litCB        = nullptr;  ///< b0: ShadowLitCB (272 B)
        ID3D11SamplerState*       cmpSampler   = nullptr;  ///< s0: comparison sampler (PCF)

        // Shared sphere geometry (UV sphere, 16×16 stacks/slices).
        ID3D11Buffer*             vertexBuf    = nullptr;
        ID3D11Buffer*             indexBuf     = nullptr;
        int                       indexCount   = 0;

        bool loaded = false;
    };

    // -----------------------------------------------------------------------
    // TEACHING NOTE — BloomScene (M17: HDR Bloom Post-Processing)
    // -----------------------------------------------------------------------
    // BloomScene implements a standard four-pass HDR bloom pipeline:
    //
    //   Scene RT  → bright-pass → blur-X → blur-Y → composite → back buffer
    //
    //   1. sceneRTV  — RGBA8 render target for the original scene colour.
    //   2. brightRTV — bright-pass: extract pixels with luminance > threshold.
    //   3. blurARTV  — horizontal Gaussian blur of the bright-pass result.
    //   4. blurBRTV  — vertical Gaussian blur = final bloom texture.
    //   5. Composite — blurBSRV + sceneSRV → back buffer (Reinhard tonemap).
    //
    // All four passes use the same full-screen triangle VS (sky.vs.hlsl,
    // SV_VertexID trick) — no vertex buffers are needed.
    //
    // Constant buffers:
    //   bloomCB (b0, bright PS) — luminance threshold (16 B).
    //   blurCB  (b0, blur  PS) — direction float2 + texelSize float2 (16 B).
    //   compCB  (b0, comp  PS) — bloom strength scalar (16 B).
    //
    // All bloom RTs are 256×256 RGBA8_UNORM — large enough to show the
    // effect but small enough for WARP headless validation to complete quickly.
    // -----------------------------------------------------------------------
    struct BloomScene
    {
        static constexpr uint32_t kRTSize = 256;  ///< Bloom RT dimensions (256×256)

        // Scene render target (source for bright-pass).
        ID3D11Texture2D*          sceneTex  = nullptr;
        ID3D11RenderTargetView*   sceneRTV  = nullptr;
        ID3D11ShaderResourceView* sceneSRV  = nullptr;

        // Bright-pass render target.
        ID3D11Texture2D*          brightTex = nullptr;
        ID3D11RenderTargetView*   brightRTV = nullptr;
        ID3D11ShaderResourceView* brightSRV = nullptr;

        // Blur ping RT (horizontal-blur output).
        ID3D11Texture2D*          blurATex  = nullptr;
        ID3D11RenderTargetView*   blurARTV  = nullptr;
        ID3D11ShaderResourceView* blurASRV  = nullptr;

        // Blur pong RT (vertical-blur output = final bloom).
        ID3D11Texture2D*          blurBTex  = nullptr;
        ID3D11RenderTargetView*   blurBRTV  = nullptr;
        ID3D11ShaderResourceView* blurBSRV  = nullptr;

        // Full-screen triangle pipeline (sky.vs.hlsl reused as VS).
        ID3D11VertexShader*       fullscreenVS = nullptr;  ///< sky.vs.hlsl
        ID3D11PixelShader*        brightPS     = nullptr;  ///< bloom_bright.ps.hlsl
        ID3D11PixelShader*        blurPS       = nullptr;  ///< bloom_blur.ps.hlsl
        ID3D11PixelShader*        compositePS  = nullptr;  ///< bloom_composite.ps.hlsl

        // Constant buffers.
        ID3D11Buffer*             bloomCB      = nullptr;  ///< b0 (bright PS): threshold
        ID3D11Buffer*             blurCB       = nullptr;  ///< b0 (blur  PS): direction+texelSize
        ID3D11Buffer*             compCB       = nullptr;  ///< b0 (comp  PS): bloomStrength

        ID3D11SamplerState*       linearSampler = nullptr;  ///< s0: linear clamp (all passes)

        bool loaded = false;
    };

private:
    TexturedQuadScene   m_quadScene;
    std::string         m_currentScene;   ///< Name of the active scene, or "".

    SkinnedMeshScene    m_skinnedScene;
    PBRScene            m_pbrScene;
    PBRIBLScene         m_pbrIblScene;    ///< M16: PBR + IBL sphere scene
    SkyScene            m_skyScene;
    ShadowScene         m_shadowScene;    ///< M17: directional shadow maps
    BloomScene          m_bloomScene;     ///< M17: HDR bloom post-processing

    // TEACHING NOTE — SkyRenderer member
    // m_skyRenderer owns the CPU-side procedural sky simulation (time-of-day,
    // weather state, colour math).  It is updated each frame in DrawSky()
    // and queried for SkyShaderConstants which are then uploaded to
    // m_skyScene.skyConstantsCB.
    engine::rendering::SkyRenderer m_skyRenderer;

    float               m_sceneTime = 0.0f;  ///< Seconds since last LoadScene call.
};

} // namespace rendering
} // namespace engine
