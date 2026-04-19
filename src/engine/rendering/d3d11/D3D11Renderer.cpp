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
    // DrawSkinnedMesh() to compute a sinusoidal joint rotation angle.
    // We advance it unconditionally so LoadScene("skinned_mesh") can start
    // animating immediately.
    m_sceneTime += 1.0f / 60.0f;   // TEACHING NOTE: approx 60fps fixed step

    if (m_skinnedScene.loaded && m_currentScene == "skinned_mesh")
        DrawSkinnedMesh();

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

    // Step 4 — Flush to ensure the GPU (or WARP) processes the work.
    m_context->Flush();

    // Step 5 — Release temporary resources.
    offscreenRTV->Release();

    return true;
}

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
    // higher-level systems.  Only "textured_quad" and "skinned_mesh" have
    // D3D11 implementations.
    if (sceneName != "textured_quad" && sceneName != "skinned_mesh")
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

    m_currentScene.clear();
}

} // namespace rendering
} // namespace engine
