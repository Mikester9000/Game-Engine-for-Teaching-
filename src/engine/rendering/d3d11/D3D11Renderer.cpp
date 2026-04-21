/**
 * @file D3D11Renderer.cpp
 * @brief Direct3D 11 renderer implementation — GT610-compatible Windows baseline.
 *
 * ============================================================================
 * TEACHING NOTE — D3D11 Initialisation Sequence
 * ============================================================================
 * D3D11 device creation is much simpler than Vulkan:
 *
 *   1. Call D3D11CreateDevice (or D3D11CreateDeviceAndSwapChain).
 *   2. The OS driver selects the GPU automatically.
 *   3. The swap chain is created via DXGI.
 *   4. Create a render-target view (RTV) from the back buffer.
 *
 * Compare to Vulkan where you enumerate physical devices, create logical
 * devices, pick queue families, create surfaces, and manage semaphores.
 * D3D11 hides most of that behind the driver — which makes it *easier* to
 * use but *harder* to reason about performance.  Both styles are worth
 * understanding.
 *
 * ============================================================================
 * TEACHING NOTE — WARP Software Renderer
 * ============================================================================
 * For headless / CI mode we pass D3D_DRIVER_TYPE_WARP to
 * D3D11CreateDevice().  WARP (Windows Advanced Rasterization Platform) is a
 * highly optimised CPU-based Direct3D implementation built into Windows.  It
 * supports Feature Level 11_0 in software and requires NO GPU driver, making
 * it perfect for GitHub-hosted CI runners.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Target: Windows (MSVC)
 */

#include "engine/rendering/d3d11/D3D11Renderer.hpp"
#include "engine/math/math_types.hpp"

// ---------------------------------------------------------------------------
// TEACHING NOTE — pragma comment(lib, ...) vs CMake target_link_libraries
// ---------------------------------------------------------------------------
// On MSVC we can tell the linker which .lib to pull in directly from source
// using #pragma comment(lib, ...).  For D3D11 this is convenient because
// d3d11.lib, dxgi.lib, and d3dcompiler.lib ship with the Windows SDK (always
// present on MSVC) and we don't need a separate find_package() in CMake.
//
// We still list them in target_link_libraries in CMakeLists.txt for clarity
// and cross-toolchain compatibility.
// ---------------------------------------------------------------------------
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include <iostream>
#include <cassert>
#include <cstring>    // std::memset
#include <cmath>      // std::sin (used in DrawSkinnedMesh animation)
#include <filesystem> // std::filesystem::path (C++17) — for wide-char path conversion

namespace engine {
namespace rendering {

// ===========================================================================
// Quad geometry constants
// ===========================================================================

// TEACHING NOTE — Quad Vertex Layout
// Each vertex carries a 2-D NDC position (no Z — the VS sets it to 0) and a
// 2-D UV coordinate.  D3D11 convention: UV (0,0) = top-left of the texture.
// The quad is centred at the origin and spans ±0.5 in NDC, giving a quad
// that fills half the screen in each dimension.
#pragma pack(push, 1)
struct QuadVertex
{
    float x, y;  ///< NDC position ([-1,+1] range; z will be 0, w will be 1)
    float u, v;  ///< Texture UV (0,0 = top-left; 1,1 = bottom-right)
};
#pragma pack(pop)

// Vertices: top-left, top-right, bottom-left, bottom-right.
static const QuadVertex kQuadVerts[4] =
{
    { -0.5f,  0.5f,  0.0f, 0.0f },  // top-left
    {  0.5f,  0.5f,  1.0f, 0.0f },  // top-right
    { -0.5f, -0.5f,  0.0f, 1.0f },  // bottom-left
    {  0.5f, -0.5f,  1.0f, 1.0f },  // bottom-right
};

// TEACHING NOTE — Index Buffer
// Two clockwise triangles (D3D11 default front face) sharing the diagonal edge:
//   Triangle 0: top-left (0), top-right (1), bottom-left (2)
//   Triangle 1: top-right (1), bottom-right (3), bottom-left (2)
static const uint16_t kQuadIndices[6] = { 0, 1, 2, 1, 3, 2 };

// ===========================================================================
// Skinned Mesh geometry constants (M4b GPU Skinning Demo)
// ===========================================================================

// TEACHING NOTE — SkinnedVertex Layout
// The skinned mesh vertex format carries:
//   pos       — bind-pose position in NDC space (z=0, w=1)
//   normal    — surface normal for lighting
//   uv        — texture coordinates (V encodes height for the gradient)
//   boneIndex — up to 4 joint indices into the constant buffer array
//   boneWeight — corresponding weights (must sum to 1.0 per vertex)
//
// We use uint32_t for bone indices and float for weights to keep the
// CPU code readable.  In production you would use uint8_t (indices 0-255)
// and UNORM8 weights to halve the vertex size.
#pragma pack(push, 1)
struct SkinnedVertex
{
    float    x, y, z;            ///< NDC position (z=0 for this flat demo)
    float    nx, ny, nz;         ///< Surface normal
    float    u, v;               ///< UV (V = height, 0 = bottom, 1 = top)
    uint32_t boneIndex[4];       ///< Bone indices (only 2 joints used in demo)
    float    boneWeight[4];      ///< Bone weights (sum = 1.0 per vertex)
};
#pragma pack(pop)

// TEACHING NOTE — Skinned Strip Geometry
// 5 rows × 2 vertices = 10 vertices forming a vertical strip in NDC.
//   x = ±0.10 NDC (narrow strip for clarity)
//   y = -0.8 to +0.8 NDC (tall, covers 80% of screen height)
//
// Skeleton (2 joints):
//   Joint 0 (bone 0): root at NDC origin — identity skin matrix — vertices STATIC.
//   Joint 1 (bone 1): child — Rotation(Z, θ) skin matrix — vertices ROTATE.
//
// Skinning weights interpolate from 100% bone 0 at the bottom to 100% bone 1
// in the top half, with a smooth 2-row blend zone in the middle.  This
// demonstrates the blend artefact that linear blend skinning produces at joints.
//
// Normals all point toward the camera (0, 0, -1) for consistent lighting.
// The strip lies in the Z=0 plane.
static const SkinnedVertex kSkinnedVerts[10] =
{
    // Row 0: y = -0.80  — fully weighted to bone 0 (static anchor)
    { -0.10f, -0.80f, 0.0f,   0,0,-1,   0.0f, 0.00f,   {0,0,0,0}, {1.00f,0.00f,0.0f,0.0f} },
    {  0.10f, -0.80f, 0.0f,   0,0,-1,   1.0f, 0.00f,   {0,0,0,0}, {1.00f,0.00f,0.0f,0.0f} },

    // Row 1: y = -0.40  — blend zone: 75% bone 0, 25% bone 1
    { -0.10f, -0.40f, 0.0f,   0,0,-1,   0.0f, 0.25f,   {0,1,0,0}, {0.75f,0.25f,0.0f,0.0f} },
    {  0.10f, -0.40f, 0.0f,   0,0,-1,   1.0f, 0.25f,   {0,1,0,0}, {0.75f,0.25f,0.0f,0.0f} },

    // Row 2: y =  0.00  — blend zone: 25% bone 0, 75% bone 1
    { -0.10f,  0.00f, 0.0f,   0,0,-1,   0.0f, 0.50f,   {0,1,0,0}, {0.25f,0.75f,0.0f,0.0f} },
    {  0.10f,  0.00f, 0.0f,   0,0,-1,   1.0f, 0.50f,   {0,1,0,0}, {0.25f,0.75f,0.0f,0.0f} },

    // Row 3: y = +0.40  — fully weighted to bone 1 (animated)
    { -0.10f,  0.40f, 0.0f,   0,0,-1,   0.0f, 0.75f,   {1,0,0,0}, {1.00f,0.00f,0.0f,0.0f} },
    {  0.10f,  0.40f, 0.0f,   0,0,-1,   1.0f, 0.75f,   {1,0,0,0}, {1.00f,0.00f,0.0f,0.0f} },

    // Row 4: y = +0.80  — fully weighted to bone 1 (tip of animated portion)
    { -0.10f,  0.80f, 0.0f,   0,0,-1,   0.0f, 1.00f,   {1,0,0,0}, {1.00f,0.00f,0.0f,0.0f} },
    {  0.10f,  0.80f, 0.0f,   0,0,-1,   1.0f, 1.00f,   {1,0,0,0}, {1.00f,0.00f,0.0f,0.0f} },
};

// TEACHING NOTE — Index Buffer (4 quads = 8 triangles = 24 indices)
//
// Vertex layout (strip, left = x=-0.1, right = x=+0.1, y increases upward):
//   Row 0 (y=-0.8):  v0 (left)   v1 (right)
//   Row 1 (y=-0.4):  v2 (left)   v3 (right)
//   Row 2 (y= 0.0):  v4 (left)   v5 (right)
//   Row 3 (y=+0.4):  v6 (left)   v7 (right)
//   Row 4 (y=+0.8):  v8 (left)   v9 (right)
//
// D3D11 default: front face = CLOCKWISE when viewed from the camera.
// The camera is conceptually at z<0, looking toward +z.
// Screen-space X is right, Y is up.
//
// For each quad (rows i and i+1), the two triangles are:
//   Triangle A: v[2i], v[2i+1], v[2i+2]  →  (left-bottom, right-bottom, left-top)
//   Triangle B: v[2i+1], v[2i+3], v[2i+2] →  (right-bottom, right-top, left-top)
//
// Screen-space CW verification for Triangle A (using row 0):
//   v0=(-0.1,-0.8), v1=(+0.1,-0.8), v2=(-0.1,-0.4)
//   Cross product (v1-v0) × (v2-v0):
//     (0.2, 0, 0) × (0, 0.4, 0) = (0, 0, 0.08)  — positive z = CW in screen space ✓
static const uint16_t kSkinnedIndices[24] =
{
    0, 1, 2,    1, 3, 2,   // Quad 0: rows 0-1
    2, 3, 4,    3, 5, 4,   // Quad 1: rows 1-2
    4, 5, 6,    5, 7, 6,   // Quad 2: rows 2-3
    6, 7, 8,    7, 9, 8,   // Quad 3: rows 3-4
};

// ===========================================================================
// Constructor / Destructor
// ===========================================================================

D3D11Renderer::D3D11Renderer()  = default;
D3D11Renderer::~D3D11Renderer() { Shutdown(); }

// ===========================================================================
// Init — create D3D11 device (and optionally swap chain)
// ===========================================================================

bool D3D11Renderer::Init(HINSTANCE /*hinstance*/, HWND hwnd,
                         uint32_t width, uint32_t height,
                         bool headless)
{
    if (m_initialised)
        return true;

    m_headless = headless;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Driver Type Selection
    // -----------------------------------------------------------------------
    // D3D_DRIVER_TYPE_HARDWARE — uses the physical GPU (fastest).
    // D3D_DRIVER_TYPE_WARP     — uses the CPU software rasteriser (universal).
    //
    // In headless mode (CI, validation) we force WARP so the binary runs on
    // any Windows machine regardless of GPU driver state.  WARP supports
    // Feature Level 11_0 in software, which is more than enough for CI.
    // -----------------------------------------------------------------------
    const D3D_DRIVER_TYPE driverType =
        headless ? D3D_DRIVER_TYPE_WARP
                 : D3D_DRIVER_TYPE_HARDWARE;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Feature Levels
    // -----------------------------------------------------------------------
    // We request feature levels in descending order.  D3D11CreateDevice picks
    // the highest level the hardware (or WARP) supports and stores the result
    // in m_featureLevel.
    //
    //   11_0 — GeForce GT 610 (Kepler/Fermi rebrand), Radeon HD 7000 series
    //   10_1 — GeForce 9 / 200 / 400 series with updated drivers
    //   10_0 — GeForce 8 series, Radeon HD 2000–3000 (absolute minimum)
    //
    // Feature Level 10_0 is the project's hard minimum for hardware support.
    // -----------------------------------------------------------------------
    const D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    const UINT numFeatureLevels = static_cast<UINT>(
        sizeof(featureLevels) / sizeof(featureLevels[0]));

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Device Creation Flags
    // -----------------------------------------------------------------------
    // D3D11_CREATE_DEVICE_DEBUG enables the D3D11 debug layer (analogous to
    // Vulkan's validation layer).  We only enable it in Debug builds to avoid
    // the performance overhead in Release.
    // -----------------------------------------------------------------------
    UINT createDeviceFlags = 0;
#if defined(_DEBUG) || defined(DEBUG)
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    // -----------------------------------------------------------------------
    // Step 1 — Create the D3D11 device and immediate context.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — D3D11CreateDevice vs D3D11CreateDeviceAndSwapChain
    // We separate device creation from swap chain creation so that headless
    // mode can skip the swap chain entirely (no HWND needed).
    // -----------------------------------------------------------------------
    HRESULT hr = D3D11CreateDevice(
        nullptr,            // Use default adapter (first enumerated GPU)
        driverType,
        nullptr,            // Software module — nullptr unless REFERENCE type
        createDeviceFlags,
        featureLevels,
        numFeatureLevels,
        D3D11_SDK_VERSION,
        &m_device,
        &m_featureLevel,
        &m_context
    );

    if (FAILED(hr))
    {
        // -----------------------------------------------------------------------
        // TEACHING NOTE — Fallback from Debug Layer to No-Debug
        // -----------------------------------------------------------------------
        // On some Windows installations the optional D3D11 debug layer DLL
        // (D3D11_1SDKLayers.dll) is not installed.  When the debug flag is set
        // and the DLL is absent, D3D11CreateDevice returns E_FAIL.  We retry
        // without the debug flag so CI runners still succeed.
        // -----------------------------------------------------------------------
        if (createDeviceFlags & D3D11_CREATE_DEVICE_DEBUG)
        {
            createDeviceFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
            hr = D3D11CreateDevice(
                nullptr, driverType, nullptr,
                createDeviceFlags,
                featureLevels, numFeatureLevels,
                D3D11_SDK_VERSION,
                &m_device, &m_featureLevel, &m_context
            );
        }

        if (FAILED(hr))
        {
            std::cerr << "[D3D11Renderer] D3D11CreateDevice failed. HRESULT=0x"
                      << std::hex << static_cast<unsigned long>(hr) << std::dec << "\n";
            return false;
        }
    }

    // -----------------------------------------------------------------------
    // Step 2 — Log what we got.
    // -----------------------------------------------------------------------
    const char* flName = "Unknown";
    switch (m_featureLevel)
    {
        case D3D_FEATURE_LEVEL_11_0: flName = "11_0"; break;
        case D3D_FEATURE_LEVEL_10_1: flName = "10_1"; break;
        case D3D_FEATURE_LEVEL_10_0: flName = "10_0"; break;
        default: break;
    }

    std::cout << "[D3D11Renderer] Device created."
              << " DriverType=" << (headless ? "WARP" : "Hardware")
              << " FeatureLevel=" << flName << "\n";

    // -----------------------------------------------------------------------
    // Step 3 — Create the swap chain (windowed mode only).
    // -----------------------------------------------------------------------
    if (!headless)
    {
        if (!CreateSwapChainResources(hwnd, width, height))
            return false;
    }

    m_initialised = true;
    return true;
}

// ===========================================================================
// CreateSwapChainResources — (re)create swap chain and render target view
// ===========================================================================

bool D3D11Renderer::CreateSwapChainResources(HWND hwnd,
                                             uint32_t width,
                                             uint32_t height)
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — DXGI Swap Chain Description
    // -----------------------------------------------------------------------
    // IDXGISwapChain is the bridge between D3D11 and the OS window manager.
    // It manages a ring of back buffers; we render to one while the other is
    // being displayed.  Key fields:
    //
    //   BufferCount       — number of back buffers (2 = double-buffering).
    //   BufferDesc.Format — pixel format; BGRA_8888 is the most compatible.
    //   SwapEffect        — DISCARD = fastest, FLIP_SEQUENTIAL = modern.
    //   Windowed          — TRUE for a windowed swap chain.
    // -----------------------------------------------------------------------
    DXGI_SWAP_CHAIN_DESC scDesc = {};
    scDesc.BufferCount                        = 2;
    scDesc.BufferDesc.Width                   = width;
    scDesc.BufferDesc.Height                  = height;
    scDesc.BufferDesc.Format                  = DXGI_FORMAT_B8G8R8A8_UNORM;
    scDesc.BufferDesc.RefreshRate.Numerator   = 60;
    scDesc.BufferDesc.RefreshRate.Denominator = 1;
    scDesc.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.OutputWindow                       = hwnd;
    scDesc.SampleDesc.Count                   = 1;   // No MSAA for baseline
    scDesc.SampleDesc.Quality                 = 0;
    scDesc.Windowed                           = TRUE;
    // TEACHING NOTE — DXGI_SWAP_EFFECT_DISCARD
    // The oldest swap effect; supported on all D3D11 hardware.  The contents
    // of the back buffer are undefined after Present — we always clear so it
    // doesn't matter.  Modern code would use FLIP_DISCARD on Win10+.
    scDesc.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Obtaining the IDXGIFactory via the Device's Adapter
    // -----------------------------------------------------------------------
    // We must create the swap chain through the same DXGI factory that owns
    // the adapter the D3D11 device was created on.  The safest way to get
    // that factory is to query the device's parent adapter via COM's QueryInterface.
    // -----------------------------------------------------------------------
    IDXGIDevice*  dxgiDevice  = nullptr;
    IDXGIAdapter* dxgiAdapter = nullptr;
    IDXGIFactory* dxgiFactory = nullptr;

    HRESULT hr = m_device->QueryInterface(__uuidof(IDXGIDevice),
                                          reinterpret_cast<void**>(&dxgiDevice));
    if (FAILED(hr)) {
        std::cerr << "[D3D11Renderer] Failed to get IDXGIDevice.\n";
        return false;
    }

    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    dxgiDevice->Release();
    if (FAILED(hr)) {
        std::cerr << "[D3D11Renderer] Failed to get IDXGIAdapter.\n";
        return false;
    }

    hr = dxgiAdapter->GetParent(__uuidof(IDXGIFactory),
                                reinterpret_cast<void**>(&dxgiFactory));
    dxgiAdapter->Release();
    if (FAILED(hr)) {
        std::cerr << "[D3D11Renderer] Failed to get IDXGIFactory.\n";
        return false;
    }

    hr = dxgiFactory->CreateSwapChain(m_device, &scDesc, &m_swapChain);
    dxgiFactory->Release();
    if (FAILED(hr)) {
        std::cerr << "[D3D11Renderer] CreateSwapChain failed. HRESULT=0x"
                  << std::hex << static_cast<unsigned long>(hr) << std::dec << "\n";
        return false;
    }

    // -----------------------------------------------------------------------
    // Step — Create the render-target view from the back buffer.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Render-Target View (RTV)
    // A RTV is a "view" that tells D3D11 which texture sub-resource to render
    // into.  Here we point it at the swap chain's back buffer.
    // -----------------------------------------------------------------------
    ID3D11Texture2D* backBuffer = nullptr;
    hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                reinterpret_cast<void**>(&backBuffer));
    if (FAILED(hr)) {
        std::cerr << "[D3D11Renderer] GetBuffer failed.\n";
        return false;
    }

    hr = m_device->CreateRenderTargetView(backBuffer, nullptr, &m_renderTarget);
    backBuffer->Release();

    if (FAILED(hr)) {
        std::cerr << "[D3D11Renderer] CreateRenderTargetView failed.\n";
        return false;
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Caching Back-Buffer Dimensions
    // -----------------------------------------------------------------------
    // Store the back-buffer size so DrawFrame can set the viewport and bind
    // the RTV correctly on every frame.  The viewport must match the
    // back-buffer size or the rasteriser will clip rendered geometry.
    // -----------------------------------------------------------------------
    m_width  = width;
    m_height = height;

    return true;
}

// ===========================================================================
// ReleaseSwapChainResources — release size-dependent objects
// ===========================================================================

void D3D11Renderer::ReleaseSwapChainResources()
{
    // Unbind the render target from the context before releasing.
    if (m_context)
        m_context->OMSetRenderTargets(0, nullptr, nullptr);

    if (m_renderTarget) { m_renderTarget->Release(); m_renderTarget = nullptr; }
    if (m_swapChain)    { m_swapChain->Release();    m_swapChain    = nullptr; }
}

// ===========================================================================
// Shutdown
// ===========================================================================

void D3D11Renderer::Shutdown()
{
    if (!m_initialised)
        return;

    // Unload any active scene first to release scene resources cleanly.
    UnloadScene();

    // TEACHING NOTE — Flush and Flush-to-Idle before release
    // Before releasing any D3D11 objects we flush the immediate context so
    // any in-flight GPU commands are drained.  Without this, destroying
    // resources the GPU is still referencing can cause device-removed errors.
    if (m_context)
        m_context->Flush();

    ReleaseSwapChainResources();

    if (m_context) { m_context->Release(); m_context = nullptr; }
    if (m_device)  { m_device->Release();  m_device  = nullptr; }

    m_initialised = false;
}

// ===========================================================================
// DrawFrame — clear the back buffer, optionally draw scene, then present
// ===========================================================================

void D3D11Renderer::DrawFrame(float clearR, float clearG, float clearB)
{
    if (!m_initialised || m_headless)
        return;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — D3D11 Frame Setup: Bind RTV + Viewport
    // -----------------------------------------------------------------------
    // Before issuing any draw or clear commands we must:
    //
    //   1. OMSetRenderTargets — tell the Output Merger (OM) stage which
    //      texture(s) to write into.  The second parameter is the depth-
    //      stencil view (nullptr here because we have no depth buffer yet).
    //
    //   2. RSSetViewports — tell the Rasteriser (RS) stage the region of the
    //      render target to use.  TopLeftX/Y = 0 means "use the full texture".
    //      Without an explicit viewport call, the rasteriser falls back to
    //      implementation-defined behaviour on some drivers.
    // -----------------------------------------------------------------------

    // 1 — Bind render target (no depth buffer yet).
    m_context->OMSetRenderTargets(1, &m_renderTarget, nullptr);

    // 2 — Set the viewport to match the back-buffer dimensions.
    D3D11_VIEWPORT vp      = {};
    vp.TopLeftX            = 0.0f;
    vp.TopLeftY            = 0.0f;
    vp.Width               = static_cast<float>(m_width);
    vp.Height              = static_cast<float>(m_height);
    vp.MinDepth            = 0.0f;
    vp.MaxDepth            = 1.0f;
    m_context->RSSetViewports(1, &vp);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — D3D11 Clear
    // -----------------------------------------------------------------------
    // ClearRenderTargetView fills the back buffer with a solid colour.
    // The clear happens *before* draw calls so geometry is composited on top
    // of the clear colour, not underneath it.
    // -----------------------------------------------------------------------
    const float clearColor[4] = { clearR, clearG, clearB, 1.0f };
    m_context->ClearRenderTargetView(m_renderTarget, clearColor);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Scene Draw Pass (M3+)
    // -----------------------------------------------------------------------
    // If a scene has been loaded via LoadScene(), draw it on top of the clear
    // colour.  The RTV and viewport are already set from the frame setup above,
    // so the draw methods can issue draw calls directly.
    //
    // M3: textured_quad — a UV-mapped full-screen quad.
    // M4b: skinned_mesh — a GPU-skinned animated strip (2-joint skeleton).
    // -----------------------------------------------------------------------
    if (m_quadScene.loaded && m_currentScene == "textured_quad")
        DrawTexturedQuad();

    // TEACHING NOTE — Advancing the demo animation timer.
    // m_sceneTime accumulates real elapsed time (seconds) and is used by
    // DrawSkinnedMesh() to compute a sinusoidal joint rotation angle, and by
    // DrawPBRMesh() to animate the sphere's Y-axis rotation.
    // We advance it unconditionally so LoadScene("skinned_mesh") can start
    // animating immediately.
    m_sceneTime += 1.0f / 60.0f;   // TEACHING NOTE: approx 60fps fixed step

    if (m_skinnedScene.loaded && m_currentScene == "skinned_mesh")
        DrawSkinnedMesh();

    // M9: PBR sphere scene
    if (m_pbrScene.loaded && m_currentScene == "pbr_mesh")
        DrawPBRMesh();

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Present interval
    // -----------------------------------------------------------------------
    // Present(1, 0) — sync to VBlank (v-sync on), 60fps cap on 60Hz monitors.
    // Present(0, 0) — present as fast as possible (no v-sync).
    // We use v-sync for the demo to avoid tearing.
    // -----------------------------------------------------------------------
    m_swapChain->Present(1, 0);
}

// ===========================================================================
// RecreateSwapchain — resize the back buffer
// ===========================================================================

void D3D11Renderer::RecreateSwapchain(uint32_t width, uint32_t height)
{
    if (!m_initialised || m_headless || !m_swapChain)
        return;

    // TEACHING NOTE — Swap Chain Resize Sequence (D3D11)
    // 1. Release the render-target view (it references the old back buffer).
    // 2. Call IDXGISwapChain::ResizeBuffers — the swap chain resizes in place.
    // 3. Re-acquire the back buffer and create a new RTV.
    // Missing step 1 causes E_INVALIDARG because the buffer is still bound.

    if (m_context)
        m_context->OMSetRenderTargets(0, nullptr, nullptr);

    if (m_renderTarget) { m_renderTarget->Release(); m_renderTarget = nullptr; }
    if (m_context)      m_context->Flush();

    HRESULT hr = m_swapChain->ResizeBuffers(
        0,                          // 0 = preserve current buffer count
        width,
        height,
        DXGI_FORMAT_UNKNOWN,        // UNKNOWN = preserve current format
        0
    );

    if (FAILED(hr))
    {
        std::cerr << "[D3D11Renderer] ResizeBuffers failed. HRESULT=0x"
                  << std::hex << static_cast<unsigned long>(hr) << std::dec << "\n";
        return;
    }

    // Re-create the RTV from the resized back buffer.
    ID3D11Texture2D* backBuffer = nullptr;
    m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                           reinterpret_cast<void**>(&backBuffer));
    if (backBuffer)
    {
        m_device->CreateRenderTargetView(backBuffer, nullptr, &m_renderTarget);
        backBuffer->Release();
    }

    // Update cached dimensions so DrawFrame uses the correct viewport.
    m_width  = width;
    m_height = height;
}

// ===========================================================================
// RecordHeadlessFrame — validate device creation for CI
// ===========================================================================

bool D3D11Renderer::RecordHeadlessFrame()
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Off-Screen Validation for Headless CI
    // -----------------------------------------------------------------------
    // In headless mode the swap chain does not exist (no HWND surface).
    // To validate that the D3D11 device can actually render — not just that
    // it was created — we:
    //
    //   1. Create a small off-screen D3D11_TEXTURE2D (64×64, RGBA8).
    //   2. Create a Render-Target View (RTV) for it.
    //   3. Clear the RTV to a known colour.
    //   4. Flush the command queue.
    //   5. Release the temporary resources.
    //
    // This round-trip exercises the full D3D11 resource creation + clear
    // path on the WARP software renderer, confirming the device is functional
    // even without a physical GPU or display.
    //
    // A future iteration could read back the pixel data via a staging texture
    // and assert the exact clear colour to catch subtle driver bugs.
    // -----------------------------------------------------------------------
    if (!m_initialised)
        return false;

    // Step 1 — Create a small off-screen render target texture.
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width              = 64;
    texDesc.Height             = 64;
    texDesc.MipLevels          = 1;
    texDesc.ArraySize          = 1;
    texDesc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count   = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage              = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags          = D3D11_BIND_RENDER_TARGET;
    texDesc.CPUAccessFlags     = 0;
    texDesc.MiscFlags          = 0;

    ID3D11Texture2D* offscreenTex = nullptr;
    HRESULT hr = m_device->CreateTexture2D(&texDesc, nullptr, &offscreenTex);
    if (FAILED(hr))
    {
        std::cerr << "[D3D11Renderer] RecordHeadlessFrame: CreateTexture2D failed. "
                     "HRESULT=0x" << std::hex << static_cast<unsigned long>(hr)
                  << std::dec << "\n";
        return false;
    }

    // Step 2 — Create the RTV.
    ID3D11RenderTargetView* offscreenRTV = nullptr;
    hr = m_device->CreateRenderTargetView(offscreenTex, nullptr, &offscreenRTV);
    // TEACHING NOTE — COM Reference Counting
    // COM objects are reference-counted.  CreateRenderTargetView internally
    // calls AddRef on the texture, so the texture stays alive even after we
    // Release() our own handle (offscreenTex).  We release early to keep
    // resource lifetimes tight and avoid leaks if the next check returns early.
    offscreenTex->Release();
    if (FAILED(hr))
    {
        std::cerr << "[D3D11Renderer] RecordHeadlessFrame: CreateRenderTargetView failed.\n";
        return false;
    }

    // Step 3 — Clear the off-screen RTV to cornflower blue (the classic D3D test colour).
    const float clearColor[4] = { 0.392f, 0.584f, 0.929f, 1.0f };
    m_context->ClearRenderTargetView(offscreenRTV, clearColor);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Validating the Scene Pipeline in Headless Mode (M3+)
    // -----------------------------------------------------------------------
    // If a scene has been loaded (e.g. "textured_quad"), we bind the offscreen
    // RTV and run one draw call to confirm the full shader + geometry +
    // texture pipeline is functional under WARP.
    // We set a small viewport matching the 64×64 offscreen texture so the
    // rasteriser clips correctly.
    // -----------------------------------------------------------------------
    if (m_quadScene.loaded && m_currentScene == "textured_quad")
    {
        D3D11_VIEWPORT vp = {};
        vp.Width    = 64.0f;
        vp.Height   = 64.0f;
        vp.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &vp);

        m_context->OMSetRenderTargets(1, &offscreenRTV, nullptr);
        DrawTexturedQuad();
    }

    // TEACHING NOTE — Headless validation for the GPU skinning scene (M4b).
    // We bind the off-screen RTV, set a matching 64×64 viewport, and call
    // DrawSkinnedMesh() once.  This validates that the skinned mesh pipeline
    // (VS, PS, input layout, joint constant buffer, geometry buffers)
    // compiles and executes correctly under WARP without a physical GPU.
    if (m_skinnedScene.loaded && m_currentScene == "skinned_mesh")
    {
        D3D11_VIEWPORT vp = {};
        vp.Width    = 64.0f;
        vp.Height   = 64.0f;
        vp.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &vp);

        m_context->OMSetRenderTargets(1, &offscreenRTV, nullptr);
        DrawSkinnedMesh();
    }

    // TEACHING NOTE — Headless validation for the PBR scene (M9).
    // Same pattern as skinned_mesh: bind the 64×64 off-screen RTV, set the
    // matching viewport, and call DrawPBRMesh() once.  This validates that
    // the PBR pipeline (Cook-Torrance shaders, sphere VB/IB, all three
    // constant buffers, rasterizer state) compiles and executes correctly
    // under the WARP software renderer, confirming the full PBR path works
    // without a physical GPU or display.
    if (m_pbrScene.loaded && m_currentScene == "pbr_mesh")
    {
        D3D11_VIEWPORT vp = {};
        vp.Width    = 64.0f;
        vp.Height   = 64.0f;
        vp.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &vp);

        m_context->OMSetRenderTargets(1, &offscreenRTV, nullptr);
        DrawPBRMesh();
    }

    // Step 4 — Flush to ensure the GPU (or WARP) processes the work.
    m_context->Flush();

    // Step 5 — Release temporary resources.
    offscreenRTV->Release();

    return true;
}

// Forward declaration — defined after LoadScene (below line 1000).
// TEACHING NOTE — C++ requires functions to be declared before use.
// LoadSkinnedMeshScene is a file-scope static helper defined later in this
// translation unit.  Rather than move the entire 300-line function above
// LoadScene (which would hurt reading order), we use a forward declaration.
static bool LoadSkinnedMeshScene(
    ID3D11Device*                    device,
    const std::string&               shaderDir,
    D3D11Renderer::SkinnedMeshScene& scene);

// Forward declaration for the PBR scene builder (M9).
// Same rationale: defined after LoadScene to keep reading order logical.
static bool LoadPBRMeshScene(
    ID3D11Device*              device,
    const std::string&         shaderDir,
    D3D11Renderer::PBRScene&   scene);

// ===========================================================================
// LoadScene — load scene resources (M3+)
// ===========================================================================

bool D3D11Renderer::LoadScene(const std::string& sceneName,
                               const std::string& shaderDir)
{
    if (!m_initialised)
    {
        std::cerr << "[D3D11Renderer] LoadScene called before Init().\n";
        return false;
    }

    // Unknown scene names are accepted silently — they may be handled by
    // higher-level systems.  Only "textured_quad", "skinned_mesh", and
    // "pbr_mesh" have D3D11 implementations.
    if (sceneName != "textured_quad" && sceneName != "skinned_mesh" &&
        sceneName != "pbr_mesh")
    {
        std::cout << "[D3D11Renderer] LoadScene('" << sceneName
                  << "') — no D3D11 scene handler; accepted as no-op.\n";
        return true;
    }

    // Release any previously loaded scene.
    UnloadScene();
    m_sceneTime = 0.0f;

    // -----------------------------------------------------------------------
    // Dispatch to the correct scene builder.
    // -----------------------------------------------------------------------
    if (sceneName == "skinned_mesh")
    {
        if (!LoadSkinnedMeshScene(m_device, shaderDir, m_skinnedScene))
        {
            std::cerr << "[D3D11Renderer] LoadScene('skinned_mesh') failed.\n";
            return false;
        }
        m_currentScene = "skinned_mesh";
        return true;
    }

    if (sceneName == "pbr_mesh")
    {
        if (!LoadPBRMeshScene(m_device, shaderDir, m_pbrScene))
        {
            std::cerr << "[D3D11Renderer] LoadScene('pbr_mesh') failed.\n";
            return false;
        }
        m_currentScene = "pbr_mesh";
        return true;
    }

    // Fall through to textured_quad scene builder below.

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Runtime HLSL Compilation with D3DCompileFromFile
    // -----------------------------------------------------------------------
    // D3D11 shaders are written in HLSL and can be compiled either:
    //   a) At build time with fxc.exe or dxc.exe → .cso (compiled shader object)
    //   b) At runtime with D3DCompileFromFile() → ID3DBlob of bytecode
    //
    // We use runtime compilation here for two reasons:
    //   1. Students can edit the .hlsl files and immediately see the effect
    //      by restarting the engine — no separate build step.
    //   2. It avoids adding a fxc.exe pre-build step to CMake which would
    //      require a special Windows SDK tool search.
    //
    // In a production engine you would always use pre-compiled .cso files
    // for shipping because runtime compilation is slower and exposes shader
    // source to end-users.
    //
    // D3DCompileFromFile takes a WIDE character path (LPCWSTR) because the
    // Windows filesystem API uses UTF-16 internally.
    // -----------------------------------------------------------------------

    // Construct paths to the HLSL files.
    namespace fs = std::filesystem;
    const fs::path vsPath = fs::path(shaderDir) / "textured_quad.vs.hlsl";
    const fs::path psPath = fs::path(shaderDir) / "textured_quad.ps.hlsl";

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Embedded Fallback HLSL
    // -----------------------------------------------------------------------
    // If the .hlsl files are not present on disk (e.g. a minimal CI run that
    // didn't copy shaders) we compile from inline string literals.  This
    // guarantees LoadScene always succeeds in any environment.
    //
    // The fallback shaders are identical to the .hlsl files; keeping them in
    // sync is the developer's responsibility.  An alternative design would use
    // a resource file (.rc) to embed the HLSL at link time.
    // -----------------------------------------------------------------------
    static const char* kVsFallback =
        "struct VSInput { float2 pos:POSITION; float2 uv:TEXCOORD0; };\n"
        "struct PSInput { float4 pos:SV_POSITION; float2 uv:TEXCOORD0; };\n"
        "PSInput main(VSInput i) { PSInput o; o.pos=float4(i.pos,0,1); o.uv=i.uv; return o; }\n";

    static const char* kPsFallback =
        "Texture2D g_tex:register(t0); SamplerState g_smp:register(s0);\n"
        "struct PSInput { float4 pos:SV_POSITION; float2 uv:TEXCOORD0; };\n"
        "float4 main(PSInput i):SV_TARGET { return g_tex.Sample(g_smp,i.uv); }\n";

    // -----------------------------------------------------------------------
    // Helper lambda: compile a shader from file, or fall back to string.
    // -----------------------------------------------------------------------
    auto compileShader = [&](const fs::path& path,
                              const char*     fallbackSrc,
                              const char*     entryPoint,
                              const char*     target) -> ID3DBlob*
    {
        ID3DBlob* code    = nullptr;
        ID3DBlob* errors  = nullptr;
        HRESULT   hr      = E_FAIL;

        if (fs::exists(path))
        {
            // TEACHING NOTE — std::wstring for Win32 wide-char path
            // D3DCompileFromFile requires a LPCWSTR (wide string) path.
            // std::filesystem::path::wstring() gives us that on MSVC.
            std::wstring wpath = path.wstring();
            hr = D3DCompileFromFile(
                wpath.c_str(),
                nullptr,          // no macro definitions
                nullptr,          // no include handler
                entryPoint,
                target,
                D3DCOMPILE_ENABLE_STRICTNESS,   // catch undeclared variables
                0,                              // no effect compile flags
                &code,
                &errors
            );
        }

        if (FAILED(hr))
        {
            if (errors)
            {
                // Print any HLSL compilation diagnostics so the developer can fix them.
                std::cerr << "[D3D11Renderer] HLSL compile error ("
                          << path.filename().string() << "):\n"
                          << static_cast<const char*>(errors->GetBufferPointer()) << "\n";
                errors->Release();
                errors = nullptr;
            }

            // Fall back to inline HLSL.
            std::cout << "[D3D11Renderer] Using embedded fallback for "
                      << path.filename().string() << ".\n";
            hr = D3DCompile(
                fallbackSrc,
                std::strlen(fallbackSrc),
                nullptr,          // no source name
                nullptr,          // no macros
                nullptr,          // no includes
                entryPoint,
                target,
                D3DCOMPILE_ENABLE_STRICTNESS,
                0,
                &code,
                &errors
            );
        }

        if (FAILED(hr))
        {
            if (errors)
            {
                std::cerr << "[D3D11Renderer] Fallback HLSL error:\n"
                          << static_cast<const char*>(errors->GetBufferPointer()) << "\n";
                errors->Release();
            }
            return nullptr;
        }

        if (errors) errors->Release();
        return code;
    };

    // Compile the vertex shader (SM4.0 — compatible with Feature Level 10_0).
    ID3DBlob* vsBlob = compileShader(vsPath, kVsFallback, "main", "vs_4_0");
    if (!vsBlob)
    {
        std::cerr << "[D3D11Renderer] LoadScene: vertex shader compilation failed.\n";
        return false;
    }

    // Compile the pixel shader.
    ID3DBlob* psBlob = compileShader(psPath, kPsFallback, "main", "ps_4_0");
    if (!psBlob)
    {
        vsBlob->Release();
        std::cerr << "[D3D11Renderer] LoadScene: pixel shader compilation failed.\n";
        return false;
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Creating Shader Objects
    // -----------------------------------------------------------------------
    // D3D11 separates shader compilation (→ bytecode blob) from shader object
    // creation (→ ID3D11VertexShader / ID3D11PixelShader).  The bytecode is
    // needed once for object creation + input-layout creation, then can be
    // discarded.  We Release() the blobs after we are done with them.
    // -----------------------------------------------------------------------
    HRESULT hr = m_device->CreateVertexShader(
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        nullptr, &m_quadScene.vs);

    if (FAILED(hr))
    {
        vsBlob->Release(); psBlob->Release();
        std::cerr << "[D3D11Renderer] CreateVertexShader failed.\n";
        return false;
    }

    hr = m_device->CreatePixelShader(
        psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
        nullptr, &m_quadScene.ps);
    psBlob->Release();   // pixel shader blob is no longer needed.

    if (FAILED(hr))
    {
        vsBlob->Release();
        std::cerr << "[D3D11Renderer] CreatePixelShader failed.\n";
        UnloadScene();
        return false;
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Input Layout
    // -----------------------------------------------------------------------
    // The Input Assembler (IA) stage needs to know how the raw bytes in the
    // vertex buffer map to shader input semantics.  D3D11_INPUT_ELEMENT_DESC
    // describes each field:
    //
    //   SemanticName   — matches the HLSL attribute name ("POSITION", "TEXCOORD")
    //   SemanticIndex  — for arrays (e.g. TEXCOORD0 vs TEXCOORD1)
    //   Format         — DXGI_FORMAT_R32G32_FLOAT = two 32-bit floats
    //   InputSlot      — which vertex buffer slot (we only have slot 0)
    //   AlignedByteOffset — byte offset within the vertex struct
    // -----------------------------------------------------------------------
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8,
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = m_device->CreateInputLayout(
        layoutDesc,
        static_cast<UINT>(std::size(layoutDesc)),
        vsBlob->GetBufferPointer(),   // must match the VS bytecode semantics
        vsBlob->GetBufferSize(),
        &m_quadScene.inputLayout);
    vsBlob->Release();   // vertex shader blob is no longer needed.

    if (FAILED(hr))
    {
        std::cerr << "[D3D11Renderer] CreateInputLayout failed.\n";
        UnloadScene();
        return false;
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Vertex and Index Buffers
    // -----------------------------------------------------------------------
    // D3D11_BUFFER_DESC describes the buffer's purpose and access pattern:
    //
    //   Usage = DEFAULT  — GPU reads and writes; CPU cannot map.
    //                      Fastest for geometry that never changes.
    //   BindFlags = VERTEX_BUFFER / INDEX_BUFFER — usage in the IA stage.
    //
    // D3D11_SUBRESOURCE_DATA carries the initial CPU data that is uploaded
    // to VRAM when CreateBuffer() is called.  After the call the CPU buffer
    // is no longer referenced by D3D11.
    // -----------------------------------------------------------------------
    {
        D3D11_BUFFER_DESC bd  = {};
        bd.ByteWidth          = static_cast<UINT>(sizeof(kQuadVerts));
        bd.Usage              = D3D11_USAGE_DEFAULT;
        bd.BindFlags          = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA sd = {};
        sd.pSysMem             = kQuadVerts;

        hr = m_device->CreateBuffer(&bd, &sd, &m_quadScene.vertexBuf);
        if (FAILED(hr))
        {
            std::cerr << "[D3D11Renderer] CreateBuffer (vertex) failed.\n";
            UnloadScene();
            return false;
        }
    }

    {
        D3D11_BUFFER_DESC bd  = {};
        bd.ByteWidth          = static_cast<UINT>(sizeof(kQuadIndices));
        bd.Usage              = D3D11_USAGE_DEFAULT;
        bd.BindFlags          = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA sd = {};
        sd.pSysMem             = kQuadIndices;

        hr = m_device->CreateBuffer(&bd, &sd, &m_quadScene.indexBuf);
        if (FAILED(hr))
        {
            std::cerr << "[D3D11Renderer] CreateBuffer (index) failed.\n";
            UnloadScene();
            return false;
        }
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Texture Loading vs Fallback
    // -----------------------------------------------------------------------
    // We look for a test DDS texture in the shaderDir's parent (project root)
    // under "samples/vertical_slice_project/Cooked/textures/".  If not found
    // we create a 1×1 white RGBA8 texture inline so LoadScene never fails due
    // to a missing asset.
    //
    // This pattern — "try to load asset, fall back to procedural placeholder" —
    // is common in AAA engines.  It lets the pipeline validate even when
    // content hasn't been cooked yet.
    // -----------------------------------------------------------------------
    fs::path ddsPath = fs::path(shaderDir) / ".." / ".." / ".." / ".."
                       / "samples" / "vertical_slice_project"
                       / "Cooked" / "textures" / "test_texture.dds";
    ddsPath = ddsPath.lexically_normal();

    bool texLoaded = m_quadScene.texture.LoadFromFile(m_device, m_context,
                                                      ddsPath.string());
    if (texLoaded)
    {
        m_quadScene.useFallbackTex = false;
        std::cout << "[D3D11Renderer] Loaded texture: " << ddsPath.string() << "\n";
    }
    else
    {
        // -----------------------------------------------------------------------
        // TEACHING NOTE — Procedural 1×1 White Fallback Texture
        // -----------------------------------------------------------------------
        // When no DDS file is present we create a 1×1 RGBA8 white texture
        // directly without going through the DDS loader.  This exercises the
        // same texture-binding code path so the quad renders in white.
        // -----------------------------------------------------------------------
        std::cout << "[D3D11Renderer] No DDS found; using 1×1 white fallback texture.\n";

        uint8_t white[4] = { 255, 255, 255, 255 };
        D3D11_TEXTURE2D_DESC td = {};
        td.Width            = 1;
        td.Height           = 1;
        td.MipLevels        = 1;
        td.ArraySize        = 1;
        td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage            = D3D11_USAGE_IMMUTABLE;
        td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA sd = {};
        sd.pSysMem    = white;
        sd.SysMemPitch = 4;

        ID3D11Texture2D* fallbackTex = nullptr;
        hr = m_device->CreateTexture2D(&td, &sd, &fallbackTex);
        if (FAILED(hr))
        {
            std::cerr << "[D3D11Renderer] CreateTexture2D (fallback) failed.\n";
            UnloadScene();
            return false;
        }

        hr = m_device->CreateShaderResourceView(fallbackTex, nullptr,
                                                 &m_quadScene.fallbackSRV);
        fallbackTex->Release();   // SRV holds a ref; our raw ptr is no longer needed.
        if (FAILED(hr))
        {
            std::cerr << "[D3D11Renderer] CreateShaderResourceView (fallback) failed.\n";
            UnloadScene();
            return false;
        }

        // Create a simple bilinear sampler for the fallback texture.
        D3D11_SAMPLER_DESC smpDesc = {};
        smpDesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        smpDesc.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
        smpDesc.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
        smpDesc.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
        smpDesc.MaxAnisotropy  = 1;
        smpDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        smpDesc.MaxLOD         = D3D11_FLOAT32_MAX;

        hr = m_device->CreateSamplerState(&smpDesc, &m_quadScene.fallbackSampler);
        if (FAILED(hr))
        {
            std::cerr << "[D3D11Renderer] CreateSamplerState (fallback) failed.\n";
            UnloadScene();
            return false;
        }

        m_quadScene.useFallbackTex = true;
    }

    m_quadScene.loaded = true;
    m_currentScene     = "textured_quad";
    std::cout << "[D3D11Renderer] LoadScene('textured_quad') — OK.\n";
    return true;
}

// ===========================================================================
// LoadScene_SkinnedMesh — build GPU skinning demo pipeline (M4b)
// ===========================================================================
// Called from LoadScene() when sceneName == "skinned_mesh".
// Implements the skinned strip demo:
//   • HLSL shaders: skinned_mesh.vs.hlsl (SM4.0 GPU skinning) + skinned_mesh.ps.hlsl
//   • 10-vertex strip with per-vertex bone indices + weights
//   • GpuSkinningBuffer (64 × Mat4 constant buffer)
//   • Rasterizer state: cull-none for double-sided visibility
// ===========================================================================

// TEACHING NOTE — LoadScene_SkinnedMesh (private helper — inlined in LoadScene)
// We use a local lambda at file scope to keep the main LoadScene() readable.
// All resource creation follows the same pattern as the textured quad:
//   compile HLSL → create shaders → create input layout → create buffers.

static bool LoadSkinnedMeshScene(
    ID3D11Device*         device,
    const std::string&    shaderDir,
    D3D11Renderer::SkinnedMeshScene& scene)
{
    namespace fs = std::filesystem;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Fallback HLSL for the skinned mesh vertex shader.
    // -----------------------------------------------------------------------
    // This is a minimal version of skinned_mesh.vs.hlsl that performs linear
    // blend skinning for up to 4 bone influences.  It matches the full HLSL
    // file that ships in the shaders/ directory; the inline copy guarantees
    // LoadScene never fails even if the .hlsl files were not copied to the
    // output directory (e.g. on a first clean build before POST_BUILD runs).
    // -----------------------------------------------------------------------
    static const char* kSkinnedVsFallback =
        "cbuffer JointCB:register(b0){float4x4 g_joints[64];};\n"
        "struct VSIn{float3 p:POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;"
        "uint4 bi:BLENDINDICES;float4 bw:BLENDWEIGHT;};\n"
        "struct PSIn{float4 p:SV_POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;};\n"
        "PSIn main(VSIn i){\n"
        "  float4 sp=float4(0,0,0,0);float3 sn=float3(0,0,0);\n"
        "  [unroll]for(int b=0;b<4;++b){\n"
        "    float w=i.bw[b];uint idx=i.bi[b];\n"
        "    sp+=w*mul(float4(i.p,1),g_joints[idx]);\n"
        "    sn+=w*mul(i.n,(float3x3)g_joints[idx]);}\n"
        "  PSIn o;float wi=(abs(sp.w)>0.0001f)?(1.0f/sp.w):1.0f;\n"
        "  o.p=float4(sp.xyz*wi,1);o.n=normalize(sn);o.uv=i.uv;return o;}\n";

    static const char* kSkinnedPsFallback =
        "struct PSIn{float4 p:SV_POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;};\n"
        "float4 main(PSIn i):SV_TARGET{\n"
        "  float3 L=normalize(float3(0.5,1,-1));\n"
        "  float d=saturate(dot(normalize(i.n),L));\n"
        "  float3 c=lerp(float3(1,0.42,0.08),float3(0.2,0.55,1),saturate(i.uv.y));\n"
        "  return float4(c*(0.3+d*0.7),1);}\n";

    // Paths to HLSL files.
    const fs::path vsPath = fs::path(shaderDir) / "skinned_mesh.vs.hlsl";
    const fs::path psPath = fs::path(shaderDir) / "skinned_mesh.ps.hlsl";

    // -----------------------------------------------------------------------
    // Compile helper (same pattern used by textured_quad).
    // -----------------------------------------------------------------------
    auto compile = [&](const fs::path& path, const char* fallback,
                       const char* entry, const char* target) -> ID3DBlob*
    {
        ID3DBlob* code   = nullptr;
        ID3DBlob* errors = nullptr;
        HRESULT   hr     = E_FAIL;

        if (fs::exists(path))
        {
            std::wstring wp = path.wstring();
            hr = D3DCompileFromFile(wp.c_str(), nullptr, nullptr,
                                    entry, target,
                                    D3DCOMPILE_ENABLE_STRICTNESS, 0,
                                    &code, &errors);
        }
        if (FAILED(hr))
        {
            if (errors) {
                std::cerr << "[D3D11Renderer] HLSL error ("
                          << path.filename().string() << "):\n"
                          << static_cast<const char*>(errors->GetBufferPointer()) << "\n";
                errors->Release(); errors = nullptr;
            }
            std::cout << "[D3D11Renderer] Using embedded fallback for "
                      << path.filename().string() << ".\n";
            hr = D3DCompile(fallback, std::strlen(fallback), nullptr, nullptr, nullptr,
                            entry, target, D3DCOMPILE_ENABLE_STRICTNESS, 0,
                            &code, &errors);
        }
        if (FAILED(hr)) {
            if (errors) {
                std::cerr << "[D3D11Renderer] Fallback HLSL error:\n"
                          << static_cast<const char*>(errors->GetBufferPointer()) << "\n";
                errors->Release();
            }
            return nullptr;
        }
        if (errors) errors->Release();
        return code;
    };

    // -----------------------------------------------------------------------
    // Step 1 — Compile and create shaders.
    // -----------------------------------------------------------------------
    ID3DBlob* vsBlob = compile(vsPath, kSkinnedVsFallback, "main", "vs_4_0");
    if (!vsBlob) return false;

    ID3DBlob* psBlob = compile(psPath, kSkinnedPsFallback, "main", "ps_4_0");
    if (!psBlob) { vsBlob->Release(); return false; }

    HRESULT hr = device->CreateVertexShader(
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &scene.vs);
    if (FAILED(hr)) {
        vsBlob->Release(); psBlob->Release();
        std::cerr << "[D3D11Renderer] CreateVertexShader (skinned) failed.\n";
        return false;
    }

    hr = device->CreatePixelShader(
        psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &scene.ps);
    psBlob->Release();
    if (FAILED(hr)) {
        vsBlob->Release();
        std::cerr << "[D3D11Renderer] CreatePixelShader (skinned) failed.\n";
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 2 — Create the input layout for SkinnedVertex.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — SkinnedVertex Input Layout
    // The D3D11_INPUT_ELEMENT_DESC array must exactly match the SkinnedVertex
    // struct defined at the top of this file (field order and byte offsets).
    //
    // BLENDINDICES uses DXGI_FORMAT_R32G32B32A32_UINT  (4 × uint32 = 16 bytes).
    // BLENDWEIGHT  uses DXGI_FORMAT_R32G32B32A32_FLOAT (4 × float  = 16 bytes).
    //
    // In production you would use R8G8B8A8_UINT + R8G8B8A8_UNORM (4+4 = 8 bytes
    // per vertex instead of 32 bytes) to reduce vertex bandwidth.  We use 32-bit
    // types here for readability.
    // -----------------------------------------------------------------------
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",        0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",      0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BLENDINDICES",  0, DXGI_FORMAT_R32G32B32A32_UINT,  0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = device->CreateInputLayout(layout, static_cast<UINT>(std::size(layout)),
                                   vsBlob->GetBufferPointer(),
                                   vsBlob->GetBufferSize(),
                                   &scene.inputLayout);
    vsBlob->Release();
    if (FAILED(hr)) {
        std::cerr << "[D3D11Renderer] CreateInputLayout (skinned) failed.\n";
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 3 — Create vertex and index buffers.
    // -----------------------------------------------------------------------
    {
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth  = static_cast<UINT>(sizeof(kSkinnedVerts));
        bd.Usage      = D3D11_USAGE_DEFAULT;
        bd.BindFlags  = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA sd = {};
        sd.pSysMem = kSkinnedVerts;
        hr = device->CreateBuffer(&bd, &sd, &scene.vertexBuf);
        if (FAILED(hr)) {
            std::cerr << "[D3D11Renderer] CreateBuffer (skinned VB) failed.\n";
            return false;
        }
    }
    {
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth  = static_cast<UINT>(sizeof(kSkinnedIndices));
        bd.Usage      = D3D11_USAGE_DEFAULT;
        bd.BindFlags  = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA sd = {};
        sd.pSysMem = kSkinnedIndices;
        hr = device->CreateBuffer(&bd, &sd, &scene.indexBuf);
        if (FAILED(hr)) {
            std::cerr << "[D3D11Renderer] CreateBuffer (skinned IB) failed.\n";
            return false;
        }
    }
    scene.indexCount = static_cast<int>(std::size(kSkinnedIndices));

    // -----------------------------------------------------------------------
    // Step 4 — Initialise the GPU skinning constant buffer.
    // -----------------------------------------------------------------------
    if (!scene.skinningCB.Init(device, engine::animation::kMaxGpuJoints))
    {
        std::cerr << "[D3D11Renderer] GpuSkinningBuffer::Init failed.\n";
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 5 — Rasterizer state: cull none (double-sided).
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Why cull-none for the skinning demo?
    // The strip starts facing the camera but rotates 360° as bone 1 oscillates.
    // With the default back-face culling the strip disappears every 180°.
    // D3D11_CULL_NONE makes both faces visible — useful for flat 2-sided meshes
    // like cloth, leaves, and demo strips.  For opaque characters you typically
    // keep cull-back and ensure mesh normals are consistent.
    // -----------------------------------------------------------------------
    {
        D3D11_RASTERIZER_DESC rd = {};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        rd.FrontCounterClockwise = FALSE;
        rd.DepthClipEnable = TRUE;
        hr = device->CreateRasterizerState(&rd, &scene.rastState);
        if (FAILED(hr)) {
            std::cerr << "[D3D11Renderer] CreateRasterizerState (skinned) failed.\n";
            return false;
        }
    }

    scene.loaded = true;
    std::cout << "[D3D11Renderer] LoadScene('skinned_mesh') — OK.\n";
    return true;
}

// ===========================================================================
// DrawTexturedQuad — bind quad pipeline state and issue one indexed draw call
// ===========================================================================

void D3D11Renderer::DrawTexturedQuad()
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — The D3D11 Draw Call Sequence
    // -----------------------------------------------------------------------
    // Every draw call in D3D11 requires the full pipeline state to be set:
    //
    //   IA — Input Assembler: topology, vertex buffer, index buffer, layout.
    //   VS — Vertex Shader: program + constant buffers.
    //   PS — Pixel Shader: program + textures + samplers.
    //   OM — Output Merger: render targets + blend state.
    //
    // The OM is already configured by the caller (DrawFrame or RecordHeadlessFrame).
    // We set IA, VS, and PS here for the quad draw call.
    //
    // TEACHING NOTE — PSSetShaderResources / PSSetSamplers
    // These calls bind texture resources and sampler states to HLSL registers.
    // register(t0) in HLSL ↔ slot 0 of PSSetShaderResources.
    // register(s0) in HLSL ↔ slot 0 of PSSetSamplers.
    // -----------------------------------------------------------------------

    // IA stage.
    m_context->IASetInputLayout(m_quadScene.inputLayout);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    UINT stride = sizeof(QuadVertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, &m_quadScene.vertexBuf, &stride, &offset);
    m_context->IASetIndexBuffer(m_quadScene.indexBuf, DXGI_FORMAT_R16_UINT, 0);

    // VS stage.
    m_context->VSSetShader(m_quadScene.vs, nullptr, 0);

    // PS stage — select texture SRV and sampler.
    ID3D11ShaderResourceView* srv     = nullptr;
    ID3D11SamplerState*       sampler = nullptr;

    if (m_quadScene.useFallbackTex)
    {
        srv     = m_quadScene.fallbackSRV;
        sampler = m_quadScene.fallbackSampler;
    }
    else
    {
        srv     = m_quadScene.texture.GetSRV();
        sampler = m_quadScene.texture.GetSampler();
    }

    m_context->PSSetShader(m_quadScene.ps, nullptr, 0);
    m_context->PSSetShaderResources(0, 1, &srv);
    m_context->PSSetSamplers(0, 1, &sampler);

    // Draw 6 indices = 2 triangles = 1 quad.
    m_context->DrawIndexed(static_cast<UINT>(std::size(kQuadIndices)), 0, 0);

    // Unbind the SRV to avoid "resource still bound" debug warnings on the
    // next frame when the SRV is used as a render target (not an issue for
    // our basic quad, but good practice to always clean up after a draw).
    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_context->PSSetShaderResources(0, 1, &nullSRV);
}

// ===========================================================================
// DrawSkinnedMesh — bind GPU skinning pipeline and draw the animated strip
// ===========================================================================

void D3D11Renderer::DrawSkinnedMesh()
{
    using namespace engine::math;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Constructing the Skin Matrices for the 2-Joint Demo
    // -----------------------------------------------------------------------
    // The demo skeleton has two joints:
    //
    //   Joint 0 (bone 0 — root):
    //     bind-pose world matrix = Identity.
    //     invBind[0]             = Identity.
    //     worldAnim[0]           = Identity (root never moves).
    //     skinMatrix[0]          = Identity * Identity = Identity.
    //     → Vertices weighted to bone 0 are NOT deformed (static anchor).
    //
    //   Joint 1 (bone 1 — child):
    //     bind-pose world matrix = Identity (joint 1 is co-located at origin).
    //     invBind[1]             = Identity.
    //     worldAnim[1]           = Rotation(Z, θ(t)).
    //     skinMatrix[1]          = Identity * Rotation(Z, θ) = Rotation(Z, θ).
    //     → Vertices weighted to bone 1 rotate around the world origin.
    //
    // The result: the bottom half of the strip (bone 0) is fixed; the top
    // half (bone 1) sweeps an arc; the blend zone smoothly transitions.
    // -----------------------------------------------------------------------
    const float angle = std::sin(m_sceneTime * 1.5f) * (kPi * 0.25f);  // ±45°

    // Build skin matrices: joint 0 = identity, joint 1 = Rotation(Z, angle).
    Mat4 jointMats[2];
    jointMats[0] = Mat4::Identity();
    jointMats[1] = Mat4::Rotation(Quat::FromAxisAngle(Vec3::Fwd(), angle));

    // -----------------------------------------------------------------------
    // Upload joint matrices to the GPU constant buffer and bind to b0.
    // -----------------------------------------------------------------------
    m_skinnedScene.skinningCB.Upload(m_context, jointMats, 2);
    m_skinnedScene.skinningCB.Bind(m_context, 0);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Input Assembler (IA) Stage Setup
    // -----------------------------------------------------------------------
    // We set the same four IA parameters as any other draw call:
    //   IASetInputLayout      — tells D3D11 how to decode vertex bytes.
    //   IASetPrimitiveTopology — triangles for a solid mesh.
    //   IASetVertexBuffers    — our SkinnedVertex buffer.
    //   IASetIndexBuffer      — 16-bit index buffer.
    // -----------------------------------------------------------------------
    m_context->IASetInputLayout(m_skinnedScene.inputLayout);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    UINT stride = static_cast<UINT>(sizeof(SkinnedVertex));
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, &m_skinnedScene.vertexBuf, &stride, &offset);
    m_context->IASetIndexBuffer(m_skinnedScene.indexBuf, DXGI_FORMAT_R16_UINT, 0);

    // VS and PS stages.
    m_context->VSSetShader(m_skinnedScene.vs, nullptr, 0);
    m_context->PSSetShader(m_skinnedScene.ps, nullptr, 0);

    // Rasterizer state (cull-none so back face is visible during rotation).
    m_context->RSSetState(m_skinnedScene.rastState);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — DrawIndexed
    // -----------------------------------------------------------------------
    // DrawIndexed(indexCount, startIndex, baseVertex):
    //   • Reads `indexCount` indices starting at `startIndex`.
    //   • Each index is added to `baseVertex` to get the actual vertex index.
    // For our strip: 24 indices, 0 start, 0 base offset.
    // -----------------------------------------------------------------------
    m_context->DrawIndexed(static_cast<UINT>(m_skinnedScene.indexCount), 0, 0);

    // Unbind constant buffer to avoid debug-layer "resource still bound" warnings.
    m_skinnedScene.skinningCB.Unbind(m_context, 0);

    // Restore default rasterizer state (cull back-face).
    m_context->RSSetState(nullptr);
}

// ===========================================================================
// UnloadScene — release all scene resources
// ===========================================================================

void D3D11Renderer::UnloadScene()
{
    // TEACHING NOTE — Release Order (LIFO vs Creation Order)
    // COM objects must be released in reverse-creation order when one object
    // holds a reference to another.  For independent scene objects (shaders,
    // buffers, textures) the order doesn't strictly matter, but releasing in
    // reverse makes intent clear.

    // --- Textured quad scene ---
    if (m_quadScene.fallbackSampler)
    {
        m_quadScene.fallbackSampler->Release();
        m_quadScene.fallbackSampler = nullptr;
    }
    if (m_quadScene.fallbackSRV)
    {
        m_quadScene.fallbackSRV->Release();
        m_quadScene.fallbackSRV = nullptr;
    }

    m_quadScene.texture.Release();

    if (m_quadScene.indexBuf)    { m_quadScene.indexBuf->Release();    m_quadScene.indexBuf    = nullptr; }
    if (m_quadScene.vertexBuf)   { m_quadScene.vertexBuf->Release();   m_quadScene.vertexBuf   = nullptr; }
    if (m_quadScene.inputLayout) { m_quadScene.inputLayout->Release(); m_quadScene.inputLayout = nullptr; }
    if (m_quadScene.ps)          { m_quadScene.ps->Release();          m_quadScene.ps          = nullptr; }
    if (m_quadScene.vs)          { m_quadScene.vs->Release();          m_quadScene.vs          = nullptr; }

    m_quadScene.loaded        = false;
    m_quadScene.useFallbackTex = false;

    // --- Skinned mesh scene (M4b) ---
    // TEACHING NOTE — Release order: state objects first (they don't depend on
    // shaders), then shaders, then geometry buffers, then the CB.
    if (m_skinnedScene.rastState)   { m_skinnedScene.rastState->Release();   m_skinnedScene.rastState   = nullptr; }
    if (m_skinnedScene.ps)          { m_skinnedScene.ps->Release();          m_skinnedScene.ps          = nullptr; }
    if (m_skinnedScene.vs)          { m_skinnedScene.vs->Release();          m_skinnedScene.vs          = nullptr; }
    if (m_skinnedScene.inputLayout) { m_skinnedScene.inputLayout->Release(); m_skinnedScene.inputLayout = nullptr; }
    if (m_skinnedScene.indexBuf)    { m_skinnedScene.indexBuf->Release();    m_skinnedScene.indexBuf    = nullptr; }
    if (m_skinnedScene.vertexBuf)   { m_skinnedScene.vertexBuf->Release();   m_skinnedScene.vertexBuf   = nullptr; }

    m_skinnedScene.skinningCB.Shutdown();
    m_skinnedScene.indexCount = 0;
    m_skinnedScene.loaded     = false;

    // --- PBR sphere scene (M9) ---
    // TEACHING NOTE — Release in reverse creation order (LIFO):
    // state objects first (no dependents), then shaders, then buffers.
    if (m_pbrScene.rastState)   { m_pbrScene.rastState->Release();   m_pbrScene.rastState   = nullptr; }
    if (m_pbrScene.materialCB)  { m_pbrScene.materialCB->Release();  m_pbrScene.materialCB  = nullptr; }
    if (m_pbrScene.lightCB)     { m_pbrScene.lightCB->Release();     m_pbrScene.lightCB     = nullptr; }
    if (m_pbrScene.perFrameCB)  { m_pbrScene.perFrameCB->Release();  m_pbrScene.perFrameCB  = nullptr; }
    if (m_pbrScene.ps)          { m_pbrScene.ps->Release();          m_pbrScene.ps          = nullptr; }
    if (m_pbrScene.vs)          { m_pbrScene.vs->Release();          m_pbrScene.vs          = nullptr; }
    if (m_pbrScene.inputLayout) { m_pbrScene.inputLayout->Release(); m_pbrScene.inputLayout = nullptr; }
    if (m_pbrScene.indexBuf)    { m_pbrScene.indexBuf->Release();    m_pbrScene.indexBuf    = nullptr; }
    if (m_pbrScene.vertexBuf)   { m_pbrScene.vertexBuf->Release();   m_pbrScene.vertexBuf   = nullptr; }
    m_pbrScene.indexCount = 0;
    m_pbrScene.loaded     = false;

    m_currentScene.clear();
}

// ===========================================================================
// DrawPBRMesh — render the Cook-Torrance PBR sphere (M9)
// ===========================================================================

void D3D11Renderer::DrawPBRMesh()
{
    if (!m_pbrScene.loaded || !m_context)
        return;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — PBR Per-Frame Constant Buffer Update
    // -----------------------------------------------------------------------
    // The world matrix changes every frame (the sphere rotates slowly around
    // the Y axis).  We therefore update the perFrameCB each call using
    // Map/Unmap on a D3D11_USAGE_DYNAMIC buffer.
    //
    // For the worldInvTrans: our world matrix is a pure rotation (orthogonal),
    // so its inverse-transpose equals itself.  We upload the same matrix for
    // both slots.  The pixel shader still benefits from the explicit slot
    // because if the object were non-uniformly scaled in a future milestone,
    // only worldInvTrans would need to change — no shader recompile needed.
    //
    // View matrix: camera at (0, 0, 4) looking at the origin (RH convention).
    // Proj matrix: FovY=60°, aspect from current back-buffer, near=0.1, far=100.
    // -----------------------------------------------------------------------

    using namespace engine::math;

    // Slow Y-axis rotation: 0.5 radians per second.
    float angle = m_sceneTime * 0.5f;
    Mat4 worldMat = Mat4::Rotation(Quat::FromAxisAngle(Vec3::Up(), angle));

    // -----------------------------------------------------------------------
    // TEACHING NOTE — LookAt matrix (Right-Handed, row-major D3D11)
    // -----------------------------------------------------------------------
    // We build the view matrix manually to show the derivation:
    //
    //   zAxis = normalize(eye - target)      // camera "back" direction (RH)
    //   xAxis = normalize(up × zAxis)        // camera "right"
    //   yAxis = zAxis × xAxis                // camera "up" (re-orthogonalised)
    //
    // For row-vector × matrix multiplication (D3D11 convention):
    //   View[row][col]:
    //     rows 0..2 encode the x, y, z camera axes (transposed from column-major)
    //     row 3     encodes -dot(axis, eye) (translation to camera origin)
    // -----------------------------------------------------------------------
    Vec3 eye    = { 0.0f, 0.5f, 4.0f };  // slightly above centre for a natural look
    Vec3 target = { 0.0f, 0.0f, 0.0f };
    Vec3 up     = { 0.0f, 1.0f, 0.0f };

    Vec3 zAxis = (eye - target).Normalized();                  // camera back (RH)
    Vec3 xAxis = up.Cross(zAxis).Normalized();                 // camera right
    Vec3 yAxis = zAxis.Cross(xAxis);                           // camera up (derived)

    Mat4 viewMat;
    viewMat.m[0][0] = xAxis.x;         viewMat.m[0][1] = yAxis.x;         viewMat.m[0][2] = zAxis.x;         viewMat.m[0][3] = 0;
    viewMat.m[1][0] = xAxis.y;         viewMat.m[1][1] = yAxis.y;         viewMat.m[1][2] = zAxis.y;         viewMat.m[1][3] = 0;
    viewMat.m[2][0] = xAxis.z;         viewMat.m[2][1] = yAxis.z;         viewMat.m[2][2] = zAxis.z;         viewMat.m[2][3] = 0;
    viewMat.m[3][0] = -xAxis.Dot(eye); viewMat.m[3][1] = -yAxis.Dot(eye); viewMat.m[3][2] = -zAxis.Dot(eye); viewMat.m[3][3] = 1;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Perspective Projection (Right-Handed, D3D11 Z=[0,1])
    // -----------------------------------------------------------------------
    // D3D11 maps view-space z ∈ [-near, -far] to NDC z ∈ [0, 1].
    //
    // For row-vector × matrix multiplication, the projection matrix is:
    //   proj[row][col]:
    //     [0][0] = f/aspect,  [1][1] = f
    //     [2][2] = -far/(far-near),    [2][3] = -1    (w_clip = -z_view)
    //     [3][2] = -near*far/(far-near)                (z_clip offset)
    //
    // where f = cot(FovY/2) = 1/tan(FovY/2).
    //
    // After perspective divide (NDC = clip/w):
    //   NDC_z = 0 at z_view = -near   (near plane)
    //   NDC_z = 1 at z_view = -far    (far plane)
    // -----------------------------------------------------------------------
    const float kFovY   = 3.14159265f / 3.0f;   // 60 degrees
    const float kNear   = 0.1f;
    const float kFar    = 100.0f;
    float aspect = (m_height > 0) ? (static_cast<float>(m_width) / static_cast<float>(m_height)) : 1.0f;
    float f      = 1.0f / std::tan(kFovY * 0.5f);   // cot(FovY/2)

    Mat4 projMat;
    projMat.m[0][0] = f / aspect;
    projMat.m[1][1] = f;
    projMat.m[2][2] = -kFar / (kFar - kNear);
    projMat.m[2][3] = -1.0f;                              // w_clip = -z_view
    projMat.m[3][2] = -(kNear * kFar) / (kFar - kNear);
    // all other elements remain 0.0f (Mat4 default-initialises to zero)

    // -----------------------------------------------------------------------
    // Upload the per-frame CB.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Map / Unmap for DYNAMIC buffers.
    // D3D11_MAP_WRITE_DISCARD tells the driver "discard the old contents and
    // give me a new pointer to write into".  This avoids GPU/CPU stalls: the
    // driver allocates a new backing page for this frame and the GPU keeps
    // reading from the old one.  The alternative, UpdateSubresource, is
    // simpler but can cause pipeline stalls on some drivers.
    struct alignas(16) PerFrameCBData
    {
        float world[4][4];
        float worldInvTrans[4][4];
        float view[4][4];
        float proj[4][4];
    } pfData;
    std::memcpy(pfData.world,        worldMat.Data(), 64);
    std::memcpy(pfData.worldInvTrans, worldMat.Data(), 64);  // rotation-only: invT == M
    std::memcpy(pfData.view,         viewMat.Data(),  64);
    std::memcpy(pfData.proj,         projMat.Data(),  64);

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(m_context->Map(m_pbrScene.perFrameCB, 0,
                                  D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        std::memcpy(mapped.pData, &pfData, sizeof(pfData));
        m_context->Unmap(m_pbrScene.perFrameCB, 0);
    }

    // -----------------------------------------------------------------------
    // Bind pipeline state.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Input Assembler (IA) stage setup.
    // We must set:
    //   1. The primitive topology (triangles, lines, etc.).
    //   2. The vertex buffer (stride = bytes per vertex).
    //   3. The index buffer (UINT16 indices here).
    //   4. The input layout (describes how to decode each vertex element).
    // Without all four, the draw call reads garbage or produces no output.
    // -----------------------------------------------------------------------
    UINT stride = sizeof(float) * 8;   // pos(3) + normal(3) + uv(2) = 8 floats
    UINT offset = 0;
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->IASetVertexBuffers(0, 1, &m_pbrScene.vertexBuf, &stride, &offset);
    m_context->IASetIndexBuffer(m_pbrScene.indexBuf, DXGI_FORMAT_R16_UINT, 0);
    m_context->IASetInputLayout(m_pbrScene.inputLayout);

    // Vertex shader + its constant buffer (b0 = per-frame transforms).
    m_context->VSSetShader(m_pbrScene.vs, nullptr, 0);
    m_context->VSSetConstantBuffers(0, 1, &m_pbrScene.perFrameCB);

    // Pixel shader + its constant buffers (b1 = light, b2 = material).
    m_context->PSSetShader(m_pbrScene.ps, nullptr, 0);
    m_context->PSSetConstantBuffers(1, 1, &m_pbrScene.lightCB);
    m_context->PSSetConstantBuffers(2, 1, &m_pbrScene.materialCB);

    // Rasterizer state: cull-none so the sphere is visible from any angle.
    m_context->RSSetState(m_pbrScene.rastState);

    // Draw the sphere.
    m_context->DrawIndexed(static_cast<UINT>(m_pbrScene.indexCount), 0, 0);

    // Restore default rasterizer state.
    m_context->RSSetState(nullptr);
}

// ===========================================================================
// LoadPBRMeshScene — build the Cook-Torrance PBR sphere scene (M9)
// ===========================================================================
// Called from D3D11Renderer::LoadScene() when sceneName == "pbr_mesh".
//
// Steps:
//   1. Compile HLSL shaders from file (or embedded fallback).
//   2. Create the D3D11 input layout.
//   3. Generate UV sphere geometry (vertices + indices).
//   4. Upload VB + IB to the GPU.
//   5. Create three constant buffers (per-frame, light, material).
//   6. Upload the static CBs (light, material).
//   7. Create cull-none rasterizer state.
// ===========================================================================

// -----------------------------------------------------------------------
// TEACHING NOTE — Embedded PBR Shader Fallbacks
// -----------------------------------------------------------------------
// As with the textured_quad and skinned_mesh scenes, we include minimal
// inline HLSL strings as fallbacks in case the .hlsl files are absent
// (e.g. a clean build before POST_BUILD copies shaders to the output dir).
//
// The fallbacks are deliberately short — they omit the full Cook-Torrance
// BRDF to stay below the constant-string-literal size limit on some
// compilers.  The on-disk .hlsl files contain the full annotated versions.
// -----------------------------------------------------------------------
static const char* kPBRVsFallback =
    "cbuffer PerFrameCB:register(b0){"
    "float4x4 g_world;float4x4 g_worldInvTrans;float4x4 g_view;float4x4 g_proj;};"
    "struct VSIn{float3 pos:POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;};"
    "struct PSIn{float4 cp:SV_POSITION;float3 wp:TEXCOORD1;float3 wn:NORMAL;float2 uv:TEXCOORD0;};"
    "PSIn main(VSIn i){"
    "PSIn o;"
    "float4 wp=mul(float4(i.pos,1),g_world);o.wp=wp.xyz;"
    "o.cp=mul(mul(wp,g_view),g_proj);"
    "o.wn=normalize(mul(i.n,(float3x3)g_worldInvTrans));"
    "o.uv=i.uv;return o;}";

static const char* kPBRPsFallback =
    "cbuffer LightCB:register(b1){float3 g_cam;float g_li;float3 g_ld;float g_pl;float3 g_lc;float g_pl2;};"
    "cbuffer MatCB:register(b2){float3 g_alb;float g_met;float g_rgh;float g_ao;float2 g_mp;};"
    "struct PSIn{float4 cp:SV_POSITION;float3 wp:TEXCOORD1;float3 wn:NORMAL;float2 uv:TEXCOORD0;};"
    "float4 main(PSIn i):SV_TARGET{"
    "float3 N=normalize(i.wn);float3 L=normalize(g_ld);"
    "float ndl=max(dot(N,L),0);"
    "float3 c=g_alb*(0.03*g_ao+ndl*g_lc*g_li);"
    "c=c/(c+1);c=pow(max(c,0),0.4545);"
    "return float4(c,1);}";

// -----------------------------------------------------------------------
// TEACHING NOTE — kPi for the UV sphere generation inside LoadPBRMeshScene.
// math_types.hpp defines engine::math::kPi, but that name requires the
// engine::math namespace which is not open at file scope here.  We declare a
// local constant so the geometry generation code reads cleanly without a
// using-namespace directive that would pollute the global scope.
// -----------------------------------------------------------------------
static constexpr float kPi = 3.14159265358979323846f;

static bool LoadPBRMeshScene(
    ID3D11Device*            device,
    const std::string&       shaderDir,
    D3D11Renderer::PBRScene& scene)
{
    namespace fs = std::filesystem;

    // -----------------------------------------------------------------------
    // Step 1 — Compile HLSL shaders.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Same compile helper pattern as the skinned mesh scene.
    // We attempt to compile from the .hlsl file on disk; if that fails (file
    // missing, syntax error) we fall back to the embedded string.  This
    // guarantees the scene always loads even in a minimal environment.
    // -----------------------------------------------------------------------
    auto compile = [&](const fs::path& path, const char* fallback,
                       const char* entry, const char* target) -> ID3DBlob*
    {
        ID3DBlob* code   = nullptr;
        ID3DBlob* errors = nullptr;
        HRESULT   hr     = E_FAIL;

        if (fs::exists(path))
        {
            std::wstring wp = path.wstring();
            hr = D3DCompileFromFile(wp.c_str(), nullptr, nullptr,
                                    entry, target,
                                    D3DCOMPILE_ENABLE_STRICTNESS, 0,
                                    &code, &errors);
        }
        if (FAILED(hr))
        {
            if (errors) {
                std::cerr << "[D3D11Renderer] HLSL error ("
                          << path.filename().string() << "):\n"
                          << static_cast<const char*>(errors->GetBufferPointer()) << "\n";
                errors->Release(); errors = nullptr;
            }
            std::cout << "[D3D11Renderer] Using embedded fallback for "
                      << path.filename().string() << ".\n";
            hr = D3DCompile(fallback, std::strlen(fallback), nullptr, nullptr, nullptr,
                            entry, target, D3DCOMPILE_ENABLE_STRICTNESS, 0,
                            &code, &errors);
        }
        if (FAILED(hr)) {
            if (errors) {
                std::cerr << "[D3D11Renderer] Fallback HLSL error:\n"
                          << static_cast<const char*>(errors->GetBufferPointer()) << "\n";
                errors->Release();
            }
            return nullptr;
        }
        if (errors) errors->Release();
        return code;
    };

    const fs::path vsPath = fs::path(shaderDir) / "pbr_mesh.vs.hlsl";
    const fs::path psPath = fs::path(shaderDir) / "pbr_mesh.ps.hlsl";

    ID3DBlob* vsBlob = compile(vsPath, kPBRVsFallback, "main", "vs_4_0");
    if (!vsBlob) { std::cerr << "[D3D11Renderer] PBR VS compile failed.\n"; return false; }

    ID3DBlob* psBlob = compile(psPath, kPBRPsFallback, "main", "ps_4_0");
    if (!psBlob) { vsBlob->Release(); std::cerr << "[D3D11Renderer] PBR PS compile failed.\n"; return false; }

    HRESULT hr = device->CreateVertexShader(vsBlob->GetBufferPointer(),
                                            vsBlob->GetBufferSize(),
                                            nullptr, &scene.vs);
    if (FAILED(hr)) { vsBlob->Release(); psBlob->Release(); return false; }

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(),
                                   psBlob->GetBufferSize(),
                                   nullptr, &scene.ps);
    if (FAILED(hr)) { vsBlob->Release(); psBlob->Release(); scene.vs->Release(); return false; }

    // -----------------------------------------------------------------------
    // Step 2 — Create the input layout.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — D3D11 Input Layout
    // The input layout maps each field of the C++ vertex struct to a
    // semantic name in the HLSL VSInput struct.  We have three fields:
    //   POSITION  — float3 model-space position
    //   NORMAL    — float3 model-space normal
    //   TEXCOORD0 — float2 UV coordinates
    //
    // The byte offsets (AlignedByteOffset) accumulate: 0, 12, 24.
    // DXGI_FORMAT_R32G32B32_FLOAT = three 32-bit floats (12 bytes).
    // DXGI_FORMAT_R32G32_FLOAT    = two   32-bit floats  (8 bytes).
    // -----------------------------------------------------------------------
    const D3D11_INPUT_ELEMENT_DESC kPBRLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = device->CreateInputLayout(kPBRLayout, 3,
                                   vsBlob->GetBufferPointer(),
                                   vsBlob->GetBufferSize(),
                                   &scene.inputLayout);
    vsBlob->Release();
    psBlob->Release();
    if (FAILED(hr)) {
        scene.vs->Release(); scene.ps->Release();
        std::cerr << "[D3D11Renderer] PBR input layout creation failed.\n";
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 3 — Generate UV sphere geometry.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — UV Sphere Parametric Generation
    // A UV sphere is generated by sweeping a circle (latitude) around the
    // Y axis (longitude).  Parameters:
    //
    //   phi   (φ) = latitude  ∈ [0, π]       0 = north pole, π = south pole
    //   theta (θ) = longitude ∈ [0, 2π]
    //
    // Vertex position on unit sphere:
    //   x = sin(φ) · cos(θ)
    //   y = cos(φ)
    //   z = sin(φ) · sin(θ)
    //
    // For a unit sphere the normal is equal to the position (outward-facing).
    //
    // UV mapping:
    //   U = θ / (2π)    → 0 at θ=0, 1 at θ=2π  (wraps around the equator)
    //   V = φ / π       → 0 at north pole, 1 at south pole
    //
    // We duplicate the vertices at the seam (θ=0 and θ=2π) so that UV
    // coordinates remain continuous — necessary for correct texture mapping
    // in a future milestone.
    //
    // Winding: CW when viewed from outside (D3D11 default front-face), but
    // we use a cull-none rasterizer state so winding order doesn't matter
    // for the sphere demo (it lets students orbit without culling artefacts).
    // -----------------------------------------------------------------------
    constexpr int N_STACKS = 16;
    constexpr int N_SLICES = 16;

    struct PBRVertex { float pos[3]; float normal[3]; float uv[2]; };

    const int nVerts   = (N_STACKS + 1) * (N_SLICES + 1);   // 17 × 17 = 289
    const int nTris    = N_STACKS * N_SLICES * 2;             // 16 × 16 × 2 = 512
    const int nIndices = nTris * 3;                           // 1536

    std::vector<PBRVertex>  verts(static_cast<size_t>(nVerts));
    std::vector<uint16_t>   indices(static_cast<size_t>(nIndices));

    // Build vertices.
    for (int stack = 0; stack <= N_STACKS; ++stack)
    {
        float phi = kPi * static_cast<float>(stack) / static_cast<float>(N_STACKS);
        float y   = std::cos(phi);
        float r   = std::sin(phi);   // radius of the latitude circle

        for (int slice = 0; slice <= N_SLICES; ++slice)
        {
            float theta = 2.0f * kPi * static_cast<float>(slice) / static_cast<float>(N_SLICES);
            float x     = r * std::cos(theta);
            float z     = r * std::sin(theta);

            int idx          = stack * (N_SLICES + 1) + slice;
            verts[idx].pos[0]    = x;
            verts[idx].pos[1]    = y;
            verts[idx].pos[2]    = z;
            verts[idx].normal[0] = x;   // unit sphere: normal == position
            verts[idx].normal[1] = y;
            verts[idx].normal[2] = z;
            verts[idx].uv[0]     = static_cast<float>(slice) / static_cast<float>(N_SLICES);
            verts[idx].uv[1]     = static_cast<float>(stack) / static_cast<float>(N_STACKS);
        }
    }

    // Build indices (two triangles per quad, CW winding from outside).
    int iIdx = 0;
    for (int stack = 0; stack < N_STACKS; ++stack)
    {
        for (int slice = 0; slice < N_SLICES; ++slice)
        {
            uint16_t v0 = static_cast<uint16_t>( stack      * (N_SLICES + 1) + slice);
            uint16_t v1 = static_cast<uint16_t>( stack      * (N_SLICES + 1) + (slice + 1));
            uint16_t v2 = static_cast<uint16_t>((stack + 1) * (N_SLICES + 1) + slice);
            uint16_t v3 = static_cast<uint16_t>((stack + 1) * (N_SLICES + 1) + (slice + 1));

            // TEACHING NOTE — Triangle winding (clockwise from outside of sphere).
            // Triangle 1: top-left, bottom-left, top-right
            // Triangle 2: top-right, bottom-left, bottom-right
            // (We use cull-none so this choice is cosmetic for the demo.)
            indices[iIdx++] = v0;
            indices[iIdx++] = v2;
            indices[iIdx++] = v1;

            indices[iIdx++] = v1;
            indices[iIdx++] = v2;
            indices[iIdx++] = v3;
        }
    }
    scene.indexCount = nIndices;

    // -----------------------------------------------------------------------
    // Step 4 — Upload VB and IB to the GPU (IMMUTABLE buffers).
    // -----------------------------------------------------------------------
    // TEACHING NOTE — IMMUTABLE vs DYNAMIC buffers for geometry.
    // The sphere geometry never changes, so we use D3D11_USAGE_IMMUTABLE:
    //   • GPU-only access (no CPU write after creation).
    //   • Optimal for static meshes — the driver can place the data in
    //     the fastest GPU memory without reserving a CPU-accessible mapping.
    // Compare to the perFrameCB which must be DYNAMIC (CPU writes each frame).
    // -----------------------------------------------------------------------
    {
        D3D11_BUFFER_DESC vbd = {};
        vbd.ByteWidth      = static_cast<UINT>(nVerts * sizeof(PBRVertex));
        vbd.Usage          = D3D11_USAGE_IMMUTABLE;
        vbd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA vsd = {};
        vsd.pSysMem = verts.data();
        hr = device->CreateBuffer(&vbd, &vsd, &scene.vertexBuf);
    }
    if (FAILED(hr)) {
        scene.vs->Release(); scene.ps->Release(); scene.inputLayout->Release();
        std::cerr << "[D3D11Renderer] PBR VB creation failed.\n";
        return false;
    }

    {
        D3D11_BUFFER_DESC ibd = {};
        ibd.ByteWidth  = static_cast<UINT>(nIndices * sizeof(uint16_t));
        ibd.Usage      = D3D11_USAGE_IMMUTABLE;
        ibd.BindFlags  = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA isd = {};
        isd.pSysMem = indices.data();
        hr = device->CreateBuffer(&ibd, &isd, &scene.indexBuf);
    }
    if (FAILED(hr)) {
        scene.vs->Release(); scene.ps->Release(); scene.inputLayout->Release();
        scene.vertexBuf->Release();
        std::cerr << "[D3D11Renderer] PBR IB creation failed.\n";
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 5 — Create constant buffers.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — D3D11 Constant Buffer Size Rules.
    // A D3D11 constant buffer must be a MULTIPLE of 16 bytes.
    // The perFrameCB holds four 4×4 float matrices = 4 × 64 = 256 bytes. ✓
    // The lightCB holds three float3 (+ padding) = 48 bytes. ✓
    // The materialCB holds float3+float+float+float+float2 = 32 bytes. ✓
    //
    // We use D3D11_USAGE_DYNAMIC + D3D11_CPU_ACCESS_WRITE for all three so
    // the CPU can update them via Map/Unmap at runtime:
    //   perFrameCB — updated every frame (rotating world matrix).
    //   lightCB    — only written once here (constant light direction).
    //   materialCB — only written once here (single-material demo).
    // -----------------------------------------------------------------------
    auto makeDynCB = [&](UINT byteWidth) -> ID3D11Buffer*
    {
        D3D11_BUFFER_DESC cbd = {};
        cbd.ByteWidth      = byteWidth;
        cbd.Usage          = D3D11_USAGE_DYNAMIC;
        cbd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        ID3D11Buffer* buf  = nullptr;
        device->CreateBuffer(&cbd, nullptr, &buf);
        return buf;
    };

    scene.perFrameCB = makeDynCB(256);   // 4 × mat4 = 256 bytes
    scene.lightCB    = makeDynCB(48);    // 3 × float4 = 48 bytes
    scene.materialCB = makeDynCB(32);    // 2 × float4 = 32 bytes

    if (!scene.perFrameCB || !scene.lightCB || !scene.materialCB) {
        std::cerr << "[D3D11Renderer] PBR constant buffer creation failed.\n";
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 6 — Upload static constant buffer data (light + material).
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Uploading data to a DYNAMIC constant buffer at init.
    // For the very first upload we use D3D11_MAP_WRITE_DISCARD (same as the
    // per-frame update).  The resource has never been used by the GPU, so
    // "discarding" its previous contents is safe (there are none).
    // -----------------------------------------------------------------------

    // Light parameters: warm directional sun from upper-right-front.
    // TEACHING NOTE — Light direction points TOWARD the light (toward the source),
    // so the dot product N·L is positive for surfaces facing the light.
    struct alignas(16) LightData {
        float cameraWorldPos[3]; float lightIntensity;
        float lightDir[3];       float padL;
        float lightColor[3];     float padL2;
    } lightData;
    lightData.cameraWorldPos[0] = 0.0f;
    lightData.cameraWorldPos[1] = 0.5f;
    lightData.cameraWorldPos[2] = 4.0f;
    lightData.lightIntensity    = 3.0f;
    // Light direction: from lower-left-back toward upper-right-front (normalised).
    {
        float lx = 0.5f, ly = 1.0f, lz = -0.5f;
        float len = std::sqrt(lx*lx + ly*ly + lz*lz);
        lightData.lightDir[0] = lx / len;
        lightData.lightDir[1] = ly / len;
        lightData.lightDir[2] = lz / len;
    }
    lightData.padL            = 0.0f;
    lightData.lightColor[0]   = 1.0f;   // warm-white sunlight
    lightData.lightColor[1]   = 0.98f;
    lightData.lightColor[2]   = 0.90f;
    lightData.padL2           = 0.0f;

    {
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        // Note: we need a temporary device context here; but LoadPBRMeshScene
        // is a static function that only has the device.  For one-time uploads
        // we use D3D11_USAGE_DEFAULT + UpdateSubresource, or we accept that
        // the per-frame update (DrawPBRMesh) must also upload lightCB once.
        // TEACHING NOTE — Workaround: use device->GetImmediateContext to get
        // a context pointer for the one-time init upload.  In a production
        // engine the context would be passed as a parameter.
        ID3D11DeviceContext* ctx = nullptr;
        device->GetImmediateContext(&ctx);
        if (ctx)
        {
            if (SUCCEEDED(ctx->Map(scene.lightCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            {
                std::memcpy(mapped.pData, &lightData, sizeof(lightData));
                ctx->Unmap(scene.lightCB, 0);
            }
            ctx->Release();
        }
    }

    // Material: shiny gold-like sphere (high metallic, moderate roughness).
    // TEACHING NOTE — Material Parameters for a Gold-like Surface:
    //   albedo   = warm orange-gold (reflected tint for metals = albedo)
    //   metallic = 0.9  (mostly metallic; a small dielectric contribution
    //                    simulates a light layer of surface oxidation)
    //   roughness = 0.25 (relatively smooth — visible sharp-ish specular)
    //   ao       = 1.0   (no occlusion on a standalone sphere)
    struct alignas(16) MaterialData {
        float albedo[3]; float metallic;
        float roughness; float ao;      float matPad[2];
    } matData;
    matData.albedo[0] = 1.00f;   // gold-orange
    matData.albedo[1] = 0.71f;
    matData.albedo[2] = 0.29f;
    matData.metallic  = 0.9f;
    matData.roughness = 0.25f;
    matData.ao        = 1.0f;
    matData.matPad[0] = matData.matPad[1] = 0.0f;

    {
        ID3D11DeviceContext* ctx = nullptr;
        device->GetImmediateContext(&ctx);
        if (ctx)
        {
            D3D11_MAPPED_SUBRESOURCE mapped = {};
            if (SUCCEEDED(ctx->Map(scene.materialCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            {
                std::memcpy(mapped.pData, &matData, sizeof(matData));
                ctx->Unmap(scene.materialCB, 0);
            }
            ctx->Release();
        }
    }

    // -----------------------------------------------------------------------
    // Step 7 — Create cull-none rasterizer state.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Cull-None for the PBR Demo Sphere.
    // The default D3D11 rasterizer state back-face culls (removes triangles
    // whose vertices wind clockwise from the camera's perspective).  For a
    // closed sphere mesh viewed from outside, back-face culling is correct
    // and efficient.
    //
    // We disable culling here for two pedagogical reasons:
    //   1. Students orbiting the camera inside the sphere can still see the
    //      inner surface, which helps visualise the sphere geometry.
    //   2. It means the winding order of the generated sphere is not critical,
    //      making the geometry generation code easier to understand.
    // -----------------------------------------------------------------------
    {
        D3D11_RASTERIZER_DESC rd = {};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        rd.FrontCounterClockwise = FALSE;
        rd.DepthClipEnable       = TRUE;
        device->CreateRasterizerState(&rd, &scene.rastState);
    }

    scene.loaded = true;
    std::cout << "[D3D11Renderer] LoadScene('pbr_mesh') — OK. "
              << nVerts << " verts, " << nTris << " tris.\n";
    return true;
}

} // namespace rendering
} // namespace engine
