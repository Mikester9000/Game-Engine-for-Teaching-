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
#include <algorithm>  // std::min/std::max
#include <cassert>
#include <cctype>     // std::isspace
#include <cstdlib>    // std::strtof
#include <cstring>    // std::memset
#include <cmath>      // std::sin (used in DrawSkinnedMesh animation)
#include <filesystem> // std::filesystem::path (C++17) — for wide-char path conversion
#include <fstream>    // std::ifstream for authored material ingestion
#include <sstream>    // std::istringstream for authored mesh parsing

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

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Creating the Depth Buffer (M16)
    // -----------------------------------------------------------------------
    // Now that we have the swap-chain dimensions, create the depth-stencil
    // buffer to match.  This call also creates the DepthStencilState that
    // enables depth testing for all 3D draws.
    // -----------------------------------------------------------------------
    if (!CreateDepthStencilBuffer(width, height))
    {
        std::cerr << "[D3D11Renderer] CreateDepthStencilBuffer failed.\n";
        return false;
    }

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

    // Release depth buffer objects first (they are bound to the same OM slot).
    ReleaseDepthStencilBuffer();

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

    // TEACHING NOTE — m_depthStencilState is device-level (not swap-chain-sized),
    // so we release it in Shutdown rather than in ReleaseSwapChainResources.
    if (m_depthStencilState) { m_depthStencilState->Release(); m_depthStencilState = nullptr; }

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
    // TEACHING NOTE — D3D11 Frame Setup: Bind RTV + DSV + Viewport
    // -----------------------------------------------------------------------
    // Before issuing any draw or clear commands we must:
    //
    //   1. OMSetRenderTargets — tell the Output Merger (OM) stage which
    //      texture(s) to write into.  The second parameter is the depth-
    //      stencil view (m_depthStencilView, added in M16).  Passing the DSV
    //      enables hardware depth testing: fragments with a larger depth value
    //      than what is stored in the DSV are discarded.
    //
    //   2. OMSetDepthStencilState — activate the depth-test state created in
    //      CreateDepthStencilBuffer().  D3D11_COMPARISON_LESS keeps the front-
    //      most (closest) fragment.
    //
    //   3. RSSetViewports — tell the Rasteriser (RS) stage the region of the
    //      render target to use.  TopLeftX/Y = 0 means "use the full texture".
    //      Without an explicit viewport call, the rasteriser falls back to
    //      implementation-defined behaviour on some drivers.
    // -----------------------------------------------------------------------

    // 1 — Bind render target and depth buffer together.
    m_context->OMSetRenderTargets(1, &m_renderTarget, m_depthStencilView);

    // 2 — Activate depth testing.
    if (m_depthStencilState)
        m_context->OMSetDepthStencilState(m_depthStencilState, 0);

    // 3 — Set the viewport to match the back-buffer dimensions.
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
    // ClearDepthStencilView resets all depth values to 1.0 (the far plane) so
    // that the first fragment drawn at any pixel passes the LESS depth test.
    // Both clears must happen before any draw calls so geometry is composited
    // on top of clean state, not residual data from the previous frame.
    // -----------------------------------------------------------------------
    const float clearColor[4] = { clearR, clearG, clearB, 1.0f };
    m_context->ClearRenderTargetView(m_renderTarget, clearColor);
    if (m_depthStencilView)
        m_context->ClearDepthStencilView(m_depthStencilView,
                                         D3D11_CLEAR_DEPTH, 1.0f, 0);

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

    // M16: PBR + IBL sphere scene
    if (m_pbrIblScene.loaded && m_currentScene == "pbr_ibl")
        DrawPBRIBLMesh();

    // M10: Dynamic sky scene
    // TEACHING NOTE — Sky Draw Order
    // The sky is drawn AFTER clearing the back buffer but could optionally be
    // drawn first since it uses depth 0.9999 (behind everything).  In a full
    // game with 3D geometry, draw the sky first (or draw it with depth test
    // disabled) so the GPU can early-Z reject pixels covered by solid geometry.
    // For this standalone sky demo there is no other geometry so order doesn't
    // matter.
    //
    // m_skyRenderer.Update(1/60) advances the time-of-day each frame.
    if (m_skyScene.loaded && m_currentScene == "dynamic_sky")
    {
        m_skyRenderer.Update(1.0f / 60.0f);
        DrawSky();
    }

    // M17: Shadow map demo — two-pass (depth pass + lit PCF pass).
    if (m_shadowScene.loaded && m_currentScene == "shadow_test")
        DrawShadowScene();

    // M17: Bloom post-processing demo — bright-pass + blur + composite.
    if (m_bloomScene.loaded && m_currentScene == "bloom_test")
        DrawBloomScene();

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

    // TEACHING NOTE — Recreate the depth buffer to match the new back-buffer size.
    // The depth buffer must always be the same width×height as the back buffer.
    // Release the old DSV + DST and create new ones at the new dimensions.
    ReleaseDepthStencilBuffer();
    CreateDepthStencilBuffer(width, height);
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

    // TEACHING NOTE — Headless validation for the PBR + IBL scene (M16).
    // Same 64×64 offscreen RTV pattern.  DrawPBRIBLMesh() validates that:
    //   • pbr_ibl.vs.hlsl + pbr_ibl.ps.hlsl compile under WARP.
    //   • All three IBL textures (BRDF LUT, irradiance cube, prefiltered env)
    //     were created successfully from the CPU-generated data.
    //   • The SRV bindings (t0, t1, t2) and the linear sampler (s0) reach
    //     the pixel shader without D3D11 debug validation errors.
    // A successful WARP render (no HRESULT failures) is sufficient CI validation
    // because WARP uses the same shader compilation path as real hardware.
    if (m_pbrIblScene.loaded && m_currentScene == "pbr_ibl")
    {
        D3D11_VIEWPORT vp = {};
        vp.Width    = 64.0f;
        vp.Height   = 64.0f;
        vp.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &vp);

        m_context->OMSetRenderTargets(1, &offscreenRTV, nullptr);
        DrawPBRIBLMesh();
    }

    // TEACHING NOTE — Headless validation for the dynamic sky scene (M10).
    // Bind the 64×64 off-screen RTV and call DrawSky() once.  This validates
    // that the sky shaders (sky.vs.hlsl + sky.ps.hlsl), the sky constant
    // buffer, and the SV_VertexID full-screen triangle draw all work correctly
    // under the WARP software renderer without a physical GPU.
    if (m_skyScene.loaded && m_currentScene == "dynamic_sky")
    {
        D3D11_VIEWPORT vp = {};
        vp.Width    = 64.0f;
        vp.Height   = 64.0f;
        vp.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &vp);

        m_context->OMSetRenderTargets(1, &offscreenRTV, nullptr);
        DrawSky();
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Headless validation for the shadow map scene (M17).
    // -----------------------------------------------------------------------
    // DrawShadowScene() executes two passes:
    //   Pass 1 (shadow): renders the sphere to the 512×512 shadow map DSV.
    //                    We set the offscreen RTV as the current target so
    //                    DrawShadowScene can restore it after the shadow pass.
    //   Pass 2 (lit):    renders the sphere from the camera view, sampling
    //                    the shadow map via PCF.  Output goes to the 64×64
    //                    offscreen RTV (restored by DrawShadowScene).
    //
    // A successful WARP execution (no device removal, no HRESULT failures)
    // confirms: shadow map creation, depth rendering, SRV binding,
    // SamplerComparisonState, and PCF sampling all work correctly.
    // -----------------------------------------------------------------------
    if (m_shadowScene.loaded && m_currentScene == "shadow_test")
    {
        D3D11_VIEWPORT vp = {};
        vp.Width    = 64.0f;
        vp.Height   = 64.0f;
        vp.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &vp);

        // Bind the 64×64 offscreen RTV so DrawShadowScene can save + restore it.
        m_context->OMSetRenderTargets(1, &offscreenRTV, nullptr);
        DrawShadowScene();
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Headless validation for the bloom scene (M17).
    // -----------------------------------------------------------------------
    // DrawBloomScene() executes four full-screen triangle passes:
    //   1. ClearRenderTargetView on sceneRTV (bright HDR colour).
    //   2. bright-pass: sceneRTV → brightRTV.
    //   3. blur-X:      brightRTV → blurARTV.
    //   4. blur-Y:      blurARTV  → blurBRTV.
    //   5. composite:   blurBRTV + sceneRTV → 64×64 offscreenRTV.
    //
    // This validates all bloom shaders (bloom_bright.ps.hlsl,
    // bloom_blur.ps.hlsl, bloom_composite.ps.hlsl) plus sky.vs.hlsl (reused
    // as the full-screen VS), all CB uploads, and the 4×(RTV+SRV) RT set.
    // -----------------------------------------------------------------------
    if (m_bloomScene.loaded && m_currentScene == "bloom_test")
    {
        D3D11_VIEWPORT vp = {};
        vp.Width    = 64.0f;
        vp.Height   = 64.0f;
        vp.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &vp);

        // Bind the 64×64 offscreen RTV so DrawBloomScene can restore it for
        // the final composite pass.
        m_context->OMSetRenderTargets(1, &offscreenRTV, nullptr);
        DrawBloomScene();
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

// Forward declaration for the sky scene builder (M10).
static bool LoadSkyScene(
    ID3D11Device*             device,
    const std::string&        shaderDir,
    D3D11Renderer::SkyScene&  scene);

// Forward declaration for the PBR + IBL scene builder (M16).
static bool LoadPBRIBLScene(
    ID3D11Device*                device,
    const std::string&           shaderDir,
    D3D11Renderer::PBRIBLScene&  scene);

// Forward declaration for the directional shadow map demo (M17).
// LoadShadowScene sets up both the depth-only shadow pass and the PCF lit pass.
static bool LoadShadowScene(
    ID3D11Device*                device,
    const std::string&           shaderDir,
    D3D11Renderer::ShadowScene&  scene);

// Forward declaration for the HDR bloom post-processing demo (M17).
// LoadBloomScene creates four offscreen RTs plus bright-pass/blur/composite shaders.
static bool LoadBloomScene(
    ID3D11Device*               device,
    const std::string&          shaderDir,
    D3D11Renderer::BloomScene&  scene);

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
    // higher-level systems.  Only "textured_quad", "skinned_mesh",
    // "pbr_mesh", "pbr_ibl", "dynamic_sky", "shadow_test", and "bloom_test"
    // have D3D11 implementations.
    if (sceneName != "textured_quad" && sceneName != "skinned_mesh" &&
        sceneName != "pbr_mesh"      && sceneName != "pbr_ibl" &&
        sceneName != "dynamic_sky"   && sceneName != "shadow_test" &&
        sceneName != "bloom_test")
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

    if (sceneName == "dynamic_sky")
    {
        if (!LoadSkyScene(m_device, shaderDir, m_skyScene))
        {
            std::cerr << "[D3D11Renderer] LoadScene('dynamic_sky') failed.\n";
            return false;
        }
        m_currentScene = "dynamic_sky";
        return true;
    }

    if (sceneName == "pbr_ibl")
    {
        if (!LoadPBRIBLScene(m_device, shaderDir, m_pbrIblScene))
        {
            std::cerr << "[D3D11Renderer] LoadScene('pbr_ibl') failed.\n";
            return false;
        }
        m_currentScene = "pbr_ibl";
        return true;
    }

    if (sceneName == "shadow_test")
    {
        if (!LoadShadowScene(m_device, shaderDir, m_shadowScene))
        {
            std::cerr << "[D3D11Renderer] LoadScene('shadow_test') failed.\n";
            return false;
        }
        m_currentScene = "shadow_test";
        return true;
    }

    if (sceneName == "bloom_test")
    {
        if (!LoadBloomScene(m_device, shaderDir, m_bloomScene))
        {
            std::cerr << "[D3D11Renderer] LoadScene('bloom_test') failed.\n";
            return false;
        }
        m_currentScene = "bloom_test";
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

    // --- Dynamic sky scene (M10) ---
    // TEACHING NOTE — The sky scene only has three objects: VS, PS, and the
    // sky constant buffer.  No vertex buffer or input layout to release (the
    // full-screen triangle uses SV_VertexID — no IA stage resources needed).
    if (m_skyScene.skyConstantsCB) { m_skyScene.skyConstantsCB->Release(); m_skyScene.skyConstantsCB = nullptr; }
    if (m_skyScene.ps)             { m_skyScene.ps->Release();             m_skyScene.ps             = nullptr; }
    if (m_skyScene.vs)             { m_skyScene.vs->Release();             m_skyScene.vs             = nullptr; }
    m_skyScene.loaded = false;

    // --- PBR + IBL sphere scene (M16) ---
    // TEACHING NOTE — Release IBL textures.
    // The raw ID3D11Texture2D* objects must be released separately from the
    // SRVs.  When CreateShaderResourceView() was called, the SRV got its own
    // COM reference to the texture (AddRef).  Releasing the SRV reduces the
    // ref count by 1, but the texture stays alive because the device also
    // holds a reference.  We must release our own texture handle explicitly.
    if (m_pbrIblScene.prefilteredSRV) { m_pbrIblScene.prefilteredSRV->Release(); m_pbrIblScene.prefilteredSRV = nullptr; }
    if (m_pbrIblScene.irradianceSRV)  { m_pbrIblScene.irradianceSRV->Release();  m_pbrIblScene.irradianceSRV  = nullptr; }
    if (m_pbrIblScene.brdfLutSRV)     { m_pbrIblScene.brdfLutSRV->Release();     m_pbrIblScene.brdfLutSRV     = nullptr; }
    if (m_pbrIblScene.albedoFallbackSRV) { m_pbrIblScene.albedoFallbackSRV->Release(); m_pbrIblScene.albedoFallbackSRV = nullptr; }
    if (m_pbrIblScene.normalFallbackSRV) { m_pbrIblScene.normalFallbackSRV->Release(); m_pbrIblScene.normalFallbackSRV = nullptr; }
    if (m_pbrIblScene.metallicRoughnessFallbackSRV) { m_pbrIblScene.metallicRoughnessFallbackSRV->Release(); m_pbrIblScene.metallicRoughnessFallbackSRV = nullptr; }
    if (m_pbrIblScene.aoFallbackSRV) { m_pbrIblScene.aoFallbackSRV->Release(); m_pbrIblScene.aoFallbackSRV = nullptr; }
    if (m_pbrIblScene.prefilteredTex) { m_pbrIblScene.prefilteredTex->Release();  m_pbrIblScene.prefilteredTex = nullptr; }
    if (m_pbrIblScene.irradianceTex)  { m_pbrIblScene.irradianceTex->Release();   m_pbrIblScene.irradianceTex  = nullptr; }
    if (m_pbrIblScene.brdfLutTex)     { m_pbrIblScene.brdfLutTex->Release();      m_pbrIblScene.brdfLutTex     = nullptr; }
    if (m_pbrIblScene.albedoFallbackTex) { m_pbrIblScene.albedoFallbackTex->Release(); m_pbrIblScene.albedoFallbackTex = nullptr; }
    if (m_pbrIblScene.normalFallbackTex) { m_pbrIblScene.normalFallbackTex->Release(); m_pbrIblScene.normalFallbackTex = nullptr; }
    if (m_pbrIblScene.metallicRoughnessFallbackTex) { m_pbrIblScene.metallicRoughnessFallbackTex->Release(); m_pbrIblScene.metallicRoughnessFallbackTex = nullptr; }
    if (m_pbrIblScene.aoFallbackTex) { m_pbrIblScene.aoFallbackTex->Release(); m_pbrIblScene.aoFallbackTex = nullptr; }
    m_pbrIblScene.albedoMap.Release();
    m_pbrIblScene.normalMap.Release();
    m_pbrIblScene.metallicRoughnessMap.Release();
    m_pbrIblScene.aoMap.Release();
    if (m_pbrIblScene.linearSampler)  { m_pbrIblScene.linearSampler->Release();   m_pbrIblScene.linearSampler  = nullptr; }
    if (m_pbrIblScene.rastState)      { m_pbrIblScene.rastState->Release();       m_pbrIblScene.rastState      = nullptr; }
    if (m_pbrIblScene.materialCB)     { m_pbrIblScene.materialCB->Release();      m_pbrIblScene.materialCB     = nullptr; }
    if (m_pbrIblScene.lightCB)        { m_pbrIblScene.lightCB->Release();         m_pbrIblScene.lightCB        = nullptr; }
    if (m_pbrIblScene.perFrameCB)     { m_pbrIblScene.perFrameCB->Release();      m_pbrIblScene.perFrameCB     = nullptr; }
    if (m_pbrIblScene.ps)             { m_pbrIblScene.ps->Release();              m_pbrIblScene.ps             = nullptr; }
    if (m_pbrIblScene.vs)             { m_pbrIblScene.vs->Release();              m_pbrIblScene.vs             = nullptr; }
    if (m_pbrIblScene.inputLayout)    { m_pbrIblScene.inputLayout->Release();     m_pbrIblScene.inputLayout    = nullptr; }
    if (m_pbrIblScene.indexBuf)       { m_pbrIblScene.indexBuf->Release();        m_pbrIblScene.indexBuf       = nullptr; }
    if (m_pbrIblScene.vertexBuf)      { m_pbrIblScene.vertexBuf->Release();       m_pbrIblScene.vertexBuf      = nullptr; }
    m_pbrIblScene.indexCount = 0;
    m_pbrIblScene.loaded     = false;

    // --- Shadow map scene (M17) ---
    // TEACHING NOTE — Release Order for Shadow Resources
    // The shadow map SRV and DSV both reference the same underlying texture
    // (shadowTex).  The SRV and DSV each add a COM reference when created, so
    // we must release the SRV and DSV BEFORE releasing the texture pointer —
    // otherwise the texture is released while views still hold references to it.
    // Releasing the SRV/DSV does NOT destroy the texture; it only decrements
    // the ref count.  Our explicit Release() of shadowTex decrements our ref
    // to zero (assuming only we hold it) and frees the GPU memory.
    if (m_shadowScene.cmpSampler)   { m_shadowScene.cmpSampler->Release();   m_shadowScene.cmpSampler   = nullptr; }
    if (m_shadowScene.litCB)        { m_shadowScene.litCB->Release();        m_shadowScene.litCB        = nullptr; }
    if (m_shadowScene.litPS)        { m_shadowScene.litPS->Release();        m_shadowScene.litPS        = nullptr; }
    if (m_shadowScene.litVS)        { m_shadowScene.litVS->Release();        m_shadowScene.litVS        = nullptr; }
    if (m_shadowScene.shadowRast)   { m_shadowScene.shadowRast->Release();   m_shadowScene.shadowRast   = nullptr; }
    if (m_shadowScene.shadowDSS)    { m_shadowScene.shadowDSS->Release();    m_shadowScene.shadowDSS    = nullptr; }
    if (m_shadowScene.shadowCB)     { m_shadowScene.shadowCB->Release();     m_shadowScene.shadowCB     = nullptr; }
    if (m_shadowScene.shadowLayout) { m_shadowScene.shadowLayout->Release(); m_shadowScene.shadowLayout = nullptr; }
    if (m_shadowScene.shadowVS)     { m_shadowScene.shadowVS->Release();     m_shadowScene.shadowVS     = nullptr; }
    if (m_shadowScene.shadowSRV)    { m_shadowScene.shadowSRV->Release();    m_shadowScene.shadowSRV    = nullptr; }
    if (m_shadowScene.shadowDSV)    { m_shadowScene.shadowDSV->Release();    m_shadowScene.shadowDSV    = nullptr; }
    if (m_shadowScene.shadowTex)    { m_shadowScene.shadowTex->Release();    m_shadowScene.shadowTex    = nullptr; }
    if (m_shadowScene.indexBuf)     { m_shadowScene.indexBuf->Release();     m_shadowScene.indexBuf     = nullptr; }
    if (m_shadowScene.vertexBuf)    { m_shadowScene.vertexBuf->Release();    m_shadowScene.vertexBuf    = nullptr; }
    m_shadowScene.indexCount = 0;
    m_shadowScene.loaded     = false;

    // --- Bloom post-processing scene (M17) ---
    // TEACHING NOTE — Releasing Render Targets
    // Each offscreen RT consists of three objects: the ID3D11Texture2D (raw
    // GPU memory), an ID3D11RenderTargetView (write access), and an
    // ID3D11ShaderResourceView (read access).  All three hold independent COM
    // references to the underlying resource.  We must release all three, and
    // we release the views before the texture to keep the release order clear:
    //   SRV → RTV → Texture2D
    // (The D3D11 runtime handles the actual memory free when all ref counts
    // reach zero, so strict ordering is not required — but LIFO is good style.)
    if (m_bloomScene.linearSampler) { m_bloomScene.linearSampler->Release(); m_bloomScene.linearSampler = nullptr; }
    if (m_bloomScene.compCB)        { m_bloomScene.compCB->Release();        m_bloomScene.compCB        = nullptr; }
    if (m_bloomScene.blurCB)        { m_bloomScene.blurCB->Release();        m_bloomScene.blurCB        = nullptr; }
    if (m_bloomScene.bloomCB)       { m_bloomScene.bloomCB->Release();       m_bloomScene.bloomCB       = nullptr; }
    if (m_bloomScene.compositePS)   { m_bloomScene.compositePS->Release();   m_bloomScene.compositePS   = nullptr; }
    if (m_bloomScene.blurPS)        { m_bloomScene.blurPS->Release();        m_bloomScene.blurPS        = nullptr; }
    if (m_bloomScene.brightPS)      { m_bloomScene.brightPS->Release();      m_bloomScene.brightPS      = nullptr; }
    if (m_bloomScene.fullscreenVS)  { m_bloomScene.fullscreenVS->Release();  m_bloomScene.fullscreenVS  = nullptr; }
    // Blur-B RT (pong)
    if (m_bloomScene.blurBSRV)  { m_bloomScene.blurBSRV->Release();  m_bloomScene.blurBSRV  = nullptr; }
    if (m_bloomScene.blurBRTV)  { m_bloomScene.blurBRTV->Release();  m_bloomScene.blurBRTV  = nullptr; }
    if (m_bloomScene.blurBTex)  { m_bloomScene.blurBTex->Release();  m_bloomScene.blurBTex  = nullptr; }
    // Blur-A RT (ping)
    if (m_bloomScene.blurASRV)  { m_bloomScene.blurASRV->Release();  m_bloomScene.blurASRV  = nullptr; }
    if (m_bloomScene.blurARTV)  { m_bloomScene.blurARTV->Release();  m_bloomScene.blurARTV  = nullptr; }
    if (m_bloomScene.blurATex)  { m_bloomScene.blurATex->Release();  m_bloomScene.blurATex  = nullptr; }
    // Bright-pass RT
    if (m_bloomScene.brightSRV) { m_bloomScene.brightSRV->Release(); m_bloomScene.brightSRV = nullptr; }
    if (m_bloomScene.brightRTV) { m_bloomScene.brightRTV->Release(); m_bloomScene.brightRTV = nullptr; }
    if (m_bloomScene.brightTex) { m_bloomScene.brightTex->Release(); m_bloomScene.brightTex = nullptr; }
    // Scene RT
    if (m_bloomScene.sceneSRV)  { m_bloomScene.sceneSRV->Release();  m_bloomScene.sceneSRV  = nullptr; }
    if (m_bloomScene.sceneRTV)  { m_bloomScene.sceneRTV->Release();  m_bloomScene.sceneRTV  = nullptr; }
    if (m_bloomScene.sceneTex)  { m_bloomScene.sceneTex->Release();  m_bloomScene.sceneTex  = nullptr; }
    m_bloomScene.loaded = false;

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
// DrawSky — render the full-screen procedural sky (M10)
// ===========================================================================

void D3D11Renderer::DrawSky()
{
    if (!m_skyScene.loaded || !m_context)
        return;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Updating the Sky Constant Buffer
    // -----------------------------------------------------------------------
    // m_skyRenderer.Update(dt) is called by DrawFrame each frame to advance
    // the time-of-day and weather simulation.  Here we read the current state
    // and upload it to the GPU before issuing the draw call.
    //
    // We use Map/Unmap with D3D11_MAP_WRITE_DISCARD on a DYNAMIC buffer.
    // This is the lowest-overhead CPU→GPU path for frequently-updated CBs:
    //   • The driver allocates a fresh memory page each frame (no stall waiting
    //     for the GPU to finish reading the previous frame's data).
    //   • We copy 80 bytes of SkyShaderConstants into that page.
    //   • The GPU reads from the page during the subsequent Draw(3,0) call.
    // -----------------------------------------------------------------------
    engine::rendering::SkyShaderConstants constants = m_skyRenderer.GetShaderConstants();

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(m_context->Map(m_skyScene.skyConstantsCB, 0,
                                  D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        std::memcpy(mapped.pData, &constants,
                    sizeof(engine::rendering::SkyShaderConstants));
        m_context->Unmap(m_skyScene.skyConstantsCB, 0);
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Sky Pipeline State
    // -----------------------------------------------------------------------
    // The sky draw uses the absolute minimum pipeline state:
    //
    //   IA: No vertex buffer, no index buffer, no input layout.
    //       The VS reads SV_VertexID to generate positions procedurally.
    //       Setting IASetInputLayout(nullptr) is required on some D3D11
    //       implementations to clear a previously bound layout; without it,
    //       debug validation layers warn about mismatched input layout.
    //
    //   VS: sky.vs.hlsl — generates 3 clip-space vertices.
    //       Draw(3, 0) produces vertex IDs 0, 1, 2.
    //
    //   PS: sky.ps.hlsl — reads SkyCB (b0) for all colours.
    //       PSSetConstantBuffers(0, 1, &skyConstantsCB) binds it.
    //
    //   OM: Render target is already bound by the caller (DrawFrame or
    //       RecordHeadlessFrame).  We do not change blend state or depth
    //       stencil state — the sky quad depth (0.9999) ensures it sits
    //       behind all 3D geometry in the depth buffer.
    // -----------------------------------------------------------------------

    // IA — no geometry resources.
    m_context->IASetInputLayout(nullptr);
    m_context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    m_context->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // VS stage.
    m_context->VSSetShader(m_skyScene.vs, nullptr, 0);
    // TEACHING NOTE — The sky VS does not use any constant buffers.
    // Explicitly clear b0 of the VS stage so no stale matrix CB from a
    // previous draw call bleeds into the sky VS's register space.
    ID3D11Buffer* nullCB = nullptr;
    m_context->VSSetConstantBuffers(0, 1, &nullCB);

    // PS stage — sky constants at b0.
    m_context->PSSetShader(m_skyScene.ps, nullptr, 0);
    m_context->PSSetConstantBuffers(0, 1, &m_skyScene.skyConstantsCB);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Draw(3, 0): Full-Screen Triangle, No Vertex Buffer
    // -----------------------------------------------------------------------
    // Draw(vertexCount, startVertexLocation):
    //   vertexCount          = 3  (one triangle, three vertices)
    //   startVertexLocation  = 0  (start at SV_VertexID = 0)
    //
    // Because no vertex buffer is bound, the IA stage produces no vertex data.
    // The VS reads SV_VertexID (0, 1, 2) and generates NDC positions internally.
    // This is a standard "bindless" trick used in post-process passes in all
    // modern rendering engines.
    // -----------------------------------------------------------------------
    m_context->Draw(3, 0);

    // Unbind the sky CB to avoid debug-layer "resource still bound" warnings.
    m_context->PSSetConstantBuffers(0, 1, &nullCB);
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

// -----------------------------------------------------------------------
// TEACHING NOTE — M23 authored material ingestion (JSON-lite parser)
// -----------------------------------------------------------------------
// The first M23 runtime step is reading authored PBR material parameters from
// cooked material files so rendering is no longer hard-coded to demo values.
//
// We intentionally use a tiny parser here instead of pulling JSON library
// headers into the renderer translation unit.  The format is machine-authored
// and stable (material.schema.json), so key-based extraction is sufficient.
// If parsing fails, we safely fall back to existing demo defaults.
// -----------------------------------------------------------------------
struct AuthoredMaterialParams
{
    float       albedo[3] = { 1.0f, 1.0f, 1.0f };
    float       metallic  = 0.0f;
    float       roughness = 1.0f;
    float       ao        = 1.0f;
    std::string albedoTextureRelPath;
    std::string normalTextureRelPath;
    std::string metallicRoughnessTextureRelPath;
    std::string aoTextureRelPath;
    std::string loadedFromPath;
};

static void SkipJsonWhitespace(const char*& p)
{
    if (!p) return;
    while (*p && std::isspace(static_cast<unsigned char>(*p))) { ++p; }
}

static bool ExtractJsonNumber(const std::string& json, const char* key, float& outValue)
{
    const std::string token = std::string("\"") + key + "\"";
    std::size_t pos = json.find(token);
    if (pos == std::string::npos) {
        std::cerr << "[D3D11Renderer] Authored material parse: missing key '" << key << "'.\n";
        return false;
    }
    pos = json.find(':', pos + token.size());
    if (pos == std::string::npos) {
        std::cerr << "[D3D11Renderer] Authored material parse: missing ':' for key '" << key << "'.\n";
        return false;
    }

    const char* p = json.c_str() + pos + 1;
    SkipJsonWhitespace(p);

    char* end = nullptr;
    float v = std::strtof(p, &end);
    if (end == p) {
        std::cerr << "[D3D11Renderer] Authored material parse: non-numeric value for key '" << key << "'.\n";
        return false;
    }
    outValue = v;
    return true;
}

static bool ExtractJsonFloat3(const std::string& json, const char* key, float out3[3])
{
    const std::string token = std::string("\"") + key + "\"";
    std::size_t pos = json.find(token);
    if (pos == std::string::npos) {
        std::cerr << "[D3D11Renderer] Authored material parse: missing key '" << key << "'.\n";
        return false;
    }
    pos = json.find('[', pos + token.size());
    if (pos == std::string::npos) {
        std::cerr << "[D3D11Renderer] Authored material parse: key '" << key << "' is not an array.\n";
        return false;
    }

    const char* p = json.c_str() + pos + 1;
    for (int i = 0; i < 3; ++i)
    {
        SkipJsonWhitespace(p);
        char* end = nullptr;
        float v = std::strtof(p, &end);
        if (end == p) {
            std::cerr << "[D3D11Renderer] Authored material parse: failed float at index "
                      << i << " for key '" << key << "'.\n";
            return false;
        }
        out3[i] = v;
        p = end;
        SkipJsonWhitespace(p);
        if (i < 2)
        {
            if (*p != ',') {
                std::cerr << "[D3D11Renderer] Authored material parse: expected comma at index "
                          << i << " for key '" << key << "'.\n";
                return false;
            }
            ++p;
        }
    }
    return true;
}

static bool ExtractJsonString(const std::string& json, const char* key, std::string& outValue)
{
    const std::string token = std::string("\"") + key + "\"";
    std::size_t pos = json.find(token);
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos + token.size());
    if (pos == std::string::npos) return false;

    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return false;
    const std::size_t start = pos + 1;

    const std::size_t end = json.find('"', start);
    if (end == std::string::npos) return false;

    outValue = json.substr(start, end - start);
    return true;
}

static bool ExtractJsonTextureString(const std::string& json, const char* key, std::string& outValue)
{
    const std::size_t texturesPos = json.find("\"textures\"");
    if (texturesPos == std::string::npos) return false;

    const std::size_t openBrace = json.find('{', texturesPos);
    if (openBrace == std::string::npos) return false;
    const std::size_t closeBrace = json.find('}', openBrace + 1);
    if (closeBrace == std::string::npos || closeBrace <= openBrace) return false;

    const std::string texturesBlock = json.substr(openBrace, closeBrace - openBrace + 1);
    return ExtractJsonString(texturesBlock, key, outValue);
}

static std::filesystem::path ReplaceTexExtensionWithDDS(const std::filesystem::path& p)
{
    if (p.extension() == ".tex")
    {
        std::filesystem::path out = p;
        out.replace_extension(".dds");
        return out;
    }
    return p;
}

static std::filesystem::path TryResolveAuthoredTexturePath(const AuthoredMaterialParams& mat,
                                                           const std::string& relPath)
{
    namespace fs = std::filesystem;
    if (relPath.empty() || mat.loadedFromPath.empty()) return {};

    const fs::path materialPath = fs::path(mat.loadedFromPath);
    const fs::path projectRoot  = materialPath.parent_path().parent_path().parent_path();
    fs::path texRel = fs::path(relPath);

    const fs::path candidates[] = {
        projectRoot / "Cooked"  / texRel,
        projectRoot / "Content" / texRel,
        fs::absolute(projectRoot / "Cooked"  / ReplaceTexExtensionWithDDS(texRel)),
        fs::absolute(projectRoot / "Content" / ReplaceTexExtensionWithDDS(texRel)),
        fs::absolute(texRel),
        fs::absolute(ReplaceTexExtensionWithDDS(texRel))
    };

    for (const fs::path& p : candidates)
    {
        const fs::path abs = fs::absolute(p);
        if (fs::exists(abs)) return abs;
    }
    return {};
}

static bool TryLoadAuthoredMaterial(AuthoredMaterialParams& outMat)
{
    namespace fs = std::filesystem;

    const fs::path candidates[] = {
        fs::path("samples/vertical_slice_project/Cooked/Materials/default_armor.material"),
        fs::path("samples/vertical_slice_project/Content/Materials/default_armor.material.json"),
        fs::path("../../samples/vertical_slice_project/Cooked/Materials/default_armor.material"),
        fs::path("../../samples/vertical_slice_project/Content/Materials/default_armor.material.json")
    };

    for (const fs::path& relPath : candidates)
    {
        const fs::path absPath = fs::absolute(relPath);
        if (!fs::exists(absPath))
            continue;

        std::ifstream f(absPath, std::ios::in);
        if (!f.is_open())
            continue;

        std::string json((std::istreambuf_iterator<char>(f)),
                          std::istreambuf_iterator<char>());
        if (json.empty())
            continue;

        AuthoredMaterialParams parsed;
        if (!ExtractJsonFloat3(json, "albedoColor", parsed.albedo)) {
            std::cerr << "[D3D11Renderer] Authored material parse failed in " << absPath.string()
                      << " (albedoColor).\n";
            continue;
        }
        if (!ExtractJsonNumber(json, "metallic", parsed.metallic)) {
            std::cerr << "[D3D11Renderer] Authored material parse failed in " << absPath.string()
                      << " (metallic).\n";
            continue;
        }
        if (!ExtractJsonNumber(json, "roughness", parsed.roughness)) {
            std::cerr << "[D3D11Renderer] Authored material parse failed in " << absPath.string()
                      << " (roughness).\n";
            continue;
        }
        if (!ExtractJsonNumber(json, "ao", parsed.ao)) {
            std::cerr << "[D3D11Renderer] Authored material parse failed in " << absPath.string()
                      << " (ao).\n";
            continue;
        }
        // Texture slots are optional; if absent we bind fallback textures.
        ExtractJsonTextureString(json, "albedo", parsed.albedoTextureRelPath);
        ExtractJsonTextureString(json, "normal", parsed.normalTextureRelPath);
        ExtractJsonTextureString(json, "metallicRoughness", parsed.metallicRoughnessTextureRelPath);
        ExtractJsonTextureString(json, "ao", parsed.aoTextureRelPath);

        parsed.loadedFromPath = absPath.string();
        outMat = parsed;
        return true;
    }

    return false;
}

static bool CreateSolidTextureSRV(ID3D11Device* device,
                                  const uint8_t rgba[4],
                                  ID3D11Texture2D** outTex,
                                  ID3D11ShaderResourceView** outSRV)
{
    if (!device || !outTex || !outSRV) return false;
    *outTex = nullptr;
    *outSRV = nullptr;

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = 1;
    td.Height = 1;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc = {1, 0};
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = rgba;
    sd.SysMemPitch = 4;

    HRESULT hr = device->CreateTexture2D(&td, &sd, outTex);
    if (FAILED(hr)) return false;

    hr = device->CreateShaderResourceView(*outTex, nullptr, outSRV);
    if (FAILED(hr))
    {
        (*outTex)->Release(); *outTex = nullptr;
        return false;
    }
    return true;
}

static void LoadAuthoredTextureWithFallback(ID3D11Device* device,
                                            ID3D11DeviceContext* context,
                                            const AuthoredMaterialParams& mat,
                                            const std::string& relPath,
                                            const uint8_t fallbackRGBA[4],
                                            D3D11Texture& outTexture,
                                            ID3D11Texture2D*& outFallbackTex,
                                            ID3D11ShaderResourceView*& outFallbackSRV)
{
    namespace fs = std::filesystem;
    const fs::path resolved = TryResolveAuthoredTexturePath(mat, relPath);
    if (!resolved.empty() &&
        outTexture.LoadFromFile(device, context, resolved.string()))
    {
        std::cout << "[D3D11Renderer] Loaded authored texture: " << resolved.string() << "\n";
    }
    else
    {
        CreateSolidTextureSRV(device, fallbackRGBA, &outFallbackTex, &outFallbackSRV);
    }
}

static bool TryLoadAuthoredMesh(std::vector<float>& outVerts,
                                std::vector<uint16_t>& outIndices,
                                std::string& outPath)
{
    namespace fs = std::filesystem;
    const fs::path candidates[] = {
        fs::path("samples/vertical_slice_project/Cooked/Meshes/default_armor.mesh"),
        fs::path("samples/vertical_slice_project/Content/Meshes/default_armor.mesh"),
        fs::path("../../samples/vertical_slice_project/Cooked/Meshes/default_armor.mesh"),
        fs::path("../../samples/vertical_slice_project/Content/Meshes/default_armor.mesh")
    };

    for (const fs::path& relPath : candidates)
    {
        const fs::path absPath = fs::absolute(relPath);
        if (!fs::exists(absPath)) continue;
        std::ifstream in(absPath);
        if (!in.is_open()) continue;

        std::vector<float> verts;
        std::vector<uint16_t> idx;

        std::string line;
        while (std::getline(in, line))
        {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream iss(line);
            char type = '\0';
            iss >> type;
            if (type == 'v')
            {
                float x, y, z, nx, ny, nz, u, v;
                if (iss >> x >> y >> z >> nx >> ny >> nz >> u >> v)
                {
                    verts.insert(verts.end(), {x, y, z, nx, ny, nz, u, v});
                }
            }
            else if (type == 'i')
            {
                int a, b, c;
                if (iss >> a >> b >> c &&
                    a >= 0 && b >= 0 && c >= 0 &&
                    a <= 65535 && b <= 65535 && c <= 65535)
                {
                    idx.push_back(static_cast<uint16_t>(a));
                    idx.push_back(static_cast<uint16_t>(b));
                    idx.push_back(static_cast<uint16_t>(c));
                }
            }
        }

        if (!verts.empty() && !idx.empty())
        {
            outVerts = std::move(verts);
            outIndices = std::move(idx);
            outPath = absPath.string();
            return true;
        }
    }

    return false;
}

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

    AuthoredMaterialParams authoredMat;
    if (TryLoadAuthoredMaterial(authoredMat))
    {
        matData.albedo[0] = authoredMat.albedo[0];
        matData.albedo[1] = authoredMat.albedo[1];
        matData.albedo[2] = authoredMat.albedo[2];
        matData.metallic  = authoredMat.metallic;
        matData.roughness = authoredMat.roughness;
        matData.ao        = authoredMat.ao;
        std::cout << "[D3D11Renderer] Loaded authored material params from: "
                  << authoredMat.loadedFromPath << "\n";
    }

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

// ===========================================================================
// LoadSkyScene — build the dynamic sky pipeline (M10)
// ===========================================================================
// Called from D3D11Renderer::LoadScene() when sceneName == "dynamic_sky".
//
// Steps:
//   1. Compile HLSL shaders: sky.vs.hlsl (SV_VertexID full-screen triangle)
//                            sky.ps.hlsl (gradient + sun disc + fog + weather)
//   2. Create the sky constant buffer (80 bytes — SkyShaderConstants).
//   3. No vertex buffer, no input layout (SV_VertexID approach).
//
// TEACHING NOTE — Minimal Pipeline for a Sky Scene
// ──────────────────────────────────────────────────
// The sky is the simplest possible D3D11 pipeline:
//   • VS    — generates 3 clip-space vertices from SV_VertexID (no VB needed)
//   • PS    — samples the sky colour from a constant buffer (no texture needed)
//   • No IA stage resources (no vertex buffer, no index buffer, no input layout)
//   • No rasterizer override (default cull-back is fine; the triangle always
//     faces the camera since it's generated in clip space)
//   • One constant buffer (b0) carrying all sky parameters
//
// This simplicity makes the sky scene a perfect study example for D3D11 basics:
//   "How little does a GPU draw call need?" — just VS + PS + a constant.
// ===========================================================================

// -----------------------------------------------------------------------
// TEACHING NOTE — Embedded Sky Shader Fallbacks
// -----------------------------------------------------------------------
// As with all other scenes, we include minimal inline HLSL strings as
// fallbacks in case the .hlsl files are absent (e.g. a clean build before
// the POST_BUILD step copies shaders to the output directory).
//
// The sky VS fallback generates the same three SV_VertexID positions.
// The sky PS fallback does a simple zenith-horizon lerp without the full
// sun disc or weather effects — enough to confirm the pipeline works.
// -----------------------------------------------------------------------
static const char* kSkyVsFallback =
    "struct PSInput{float4 pos:SV_POSITION;float2 uv:TEXCOORD0;};\n"
    "PSInput main(uint id:SV_VertexID){\n"
    "PSInput o;\n"
    "float px=(id==2u)?3.0f:-1.0f;\n"
    "float py=(id==1u)?3.0f:-1.0f;\n"
    "o.pos=float4(px,py,0.9999f,1.0f);\n"
    "o.uv=float2(px*0.5f+0.5f,-py*0.5f+0.5f);\n"
    "return o;}\n";

static const char* kSkyPsFallback =
    "cbuffer SkyCB:register(b0){"
    "float4 g_sunDir;float4 g_zenithColor;float4 g_horizonColor;"
    "float4 g_fogColor;float4 g_weatherFx;};\n"
    "struct PSInput{float4 pos:SV_POSITION;float2 uv:TEXCOORD0;};\n"
    "float4 main(PSInput i):SV_TARGET{\n"
    "float t=i.uv.y;\n"
    "float gf=1.0f-exp(-t*3.0f);\n"
    "float3 c=lerp(g_zenithColor.xyz,g_horizonColor.xyz,gf);\n"
    "c=c/(c+1.0f);\n"
    "c=pow(max(c,float3(0,0,0)),1.0f/2.2f);\n"
    "return float4(c,1.0f);}\n";

static bool LoadSkyScene(
    ID3D11Device*             device,
    const std::string&        shaderDir,
    D3D11Renderer::SkyScene&  scene)
{
    namespace fs = std::filesystem;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — compile helper (same pattern as all other scene loaders)
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
            std::cout << "[D3D11Renderer] Using embedded sky fallback for "
                      << path.filename().string() << ".\n";
            hr = D3DCompile(fallback, std::strlen(fallback), nullptr, nullptr, nullptr,
                            entry, target, D3DCOMPILE_ENABLE_STRICTNESS, 0,
                            &code, &errors);
        }
        if (FAILED(hr)) {
            if (errors) {
                std::cerr << "[D3D11Renderer] Sky fallback HLSL error:\n"
                          << static_cast<const char*>(errors->GetBufferPointer()) << "\n";
                errors->Release();
            }
            return nullptr;
        }
        if (errors) errors->Release();
        return code;
    };

    // -----------------------------------------------------------------------
    // Step 1 — Compile vertex and pixel shaders.
    // -----------------------------------------------------------------------
    const fs::path vsPath = fs::path(shaderDir) / "sky.vs.hlsl";
    const fs::path psPath = fs::path(shaderDir) / "sky.ps.hlsl";

    ID3DBlob* vsBlob = compile(vsPath, kSkyVsFallback, "main", "vs_4_0");
    if (!vsBlob) {
        std::cerr << "[D3D11Renderer] Sky VS compile failed.\n";
        return false;
    }

    ID3DBlob* psBlob = compile(psPath, kSkyPsFallback, "main", "ps_4_0");
    if (!psBlob) {
        vsBlob->Release();
        std::cerr << "[D3D11Renderer] Sky PS compile failed.\n";
        return false;
    }

    HRESULT hr = device->CreateVertexShader(vsBlob->GetBufferPointer(),
                                            vsBlob->GetBufferSize(),
                                            nullptr, &scene.vs);
    vsBlob->Release();
    if (FAILED(hr)) {
        psBlob->Release();
        std::cerr << "[D3D11Renderer] CreateVertexShader (sky) failed.\n";
        return false;
    }

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(),
                                   psBlob->GetBufferSize(),
                                   nullptr, &scene.ps);
    psBlob->Release();
    if (FAILED(hr)) {
        scene.vs->Release(); scene.vs = nullptr;
        std::cerr << "[D3D11Renderer] CreatePixelShader (sky) failed.\n";
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 2 — Create the sky constant buffer.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Sky Constant Buffer Size
    // SkyShaderConstants is 80 bytes (5 × float4 = 5 × 16 bytes).
    // D3D11 requires constant buffers to be multiples of 16 bytes.
    // The static_assert in sky_renderer.hpp verifies this at compile time.
    //
    // We use DYNAMIC + CPU_ACCESS_WRITE because the sky constants change
    // every frame (time-of-day advances, weather interpolates).  Map/Unmap
    // with D3D11_MAP_WRITE_DISCARD is the fastest way to update a dynamic CB.
    // -----------------------------------------------------------------------
    static_assert(sizeof(engine::rendering::SkyShaderConstants) % 16 == 0,
        "SkyShaderConstants must be 16-byte aligned");

    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth      = static_cast<UINT>(sizeof(engine::rendering::SkyShaderConstants));
    cbd.Usage          = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = device->CreateBuffer(&cbd, nullptr, &scene.skyConstantsCB);
    if (FAILED(hr)) {
        scene.vs->Release(); scene.vs = nullptr;
        scene.ps->Release(); scene.ps = nullptr;
        std::cerr << "[D3D11Renderer] CreateBuffer (sky CB) failed.\n";
        return false;
    }

    scene.loaded = true;
    std::cout << "[D3D11Renderer] LoadScene('dynamic_sky') — OK.\n";
    return true;
}

// ===========================================================================
// CreateDepthStencilBuffer — create DSV + DepthStencilState (M16)
// ===========================================================================
// TEACHING NOTE — Depth Buffer Creation in D3D11
// ===========================================================================
// Creating a D3D11 depth buffer requires three steps:
//
//   Step 1: ID3D11Texture2D — a GPU texture to hold depth values.
//           Format: DXGI_FORMAT_D24_UNORM_S8_UINT.
//             • D24 = 24-bit float depth, mapping view-space far → 1.0 and
//               near → 0.0 (D3D11 NDC Z range is [0, 1]).
//             • S8  = 8-bit integer stencil (reserved for future milestones:
//               shadow stencil volumes, screen-space outlines, etc.).
//           BindFlags: D3D11_BIND_DEPTH_STENCIL (not RENDER_TARGET).
//           Usage: D3D11_USAGE_DEFAULT — the GPU writes it; the CPU never
//             reads it back in the depth-test path.
//
//   Step 2: ID3D11DepthStencilView (DSV) — a "view" over the texture that
//           tells the OM stage which texture resource is the depth attachment.
//           The view also selects which array slice / mip level to use (we
//           use the whole texture: mip 0, array index 0).
//
//   Step 3: ID3D11DepthStencilState — GPU state object that configures the
//           OM depth test.  Key fields:
//             DepthEnable = TRUE              — enable depth testing.
//             DepthWriteMask = ALL            — fragments that pass write depth.
//             DepthFunc = COMPARISON_LESS     — keep the CLOSER fragment
//               (the one with the smaller NDC depth value).
//           Stencil is disabled (StencilEnable = FALSE) because we don't
//           use it yet.
//
// The state object is device-level (not swap-chain-sized), so we create it
// once and release it only in Shutdown().  The texture + view are swap-chain-
// sized and are recreated on every resize.
// ===========================================================================

bool D3D11Renderer::CreateDepthStencilBuffer(uint32_t width, uint32_t height)
{
    if (!m_device)
        return false;

    // -----------------------------------------------------------------------
    // Step 1 — Create the depth-stencil texture.
    // -----------------------------------------------------------------------
    D3D11_TEXTURE2D_DESC dsDesc = {};
    dsDesc.Width              = width;
    dsDesc.Height             = height;
    dsDesc.MipLevels          = 1;      // Only need mip 0 for depth
    dsDesc.ArraySize          = 1;
    dsDesc.Format             = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsDesc.SampleDesc.Count   = 1;      // No MSAA (baseline)
    dsDesc.SampleDesc.Quality = 0;
    dsDesc.Usage              = D3D11_USAGE_DEFAULT;
    dsDesc.BindFlags          = D3D11_BIND_DEPTH_STENCIL;
    dsDesc.CPUAccessFlags     = 0;
    dsDesc.MiscFlags          = 0;

    HRESULT hr = m_device->CreateTexture2D(&dsDesc, nullptr, &m_depthStencilTex);
    if (FAILED(hr))
    {
        std::cerr << "[D3D11Renderer] CreateTexture2D (depth) failed.\n";
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 2 — Create the depth-stencil view.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — D3D11_DEPTH_STENCIL_VIEW_DESC
    // ViewDimension = TEXTURE2D means we bind the entire 2D texture as depth.
    // MipSlice = 0 selects the single mip level we created above.
    // -----------------------------------------------------------------------
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format             = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension      = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    hr = m_device->CreateDepthStencilView(m_depthStencilTex, &dsvDesc,
                                          &m_depthStencilView);
    if (FAILED(hr))
    {
        m_depthStencilTex->Release();
        m_depthStencilTex = nullptr;
        std::cerr << "[D3D11Renderer] CreateDepthStencilView failed.\n";
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 3 — Create the depth-stencil state (only once — device-level).
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Why create the state here and not in Init()?
    // The state is logically part of the depth buffer setup.  Keeping all
    // three objects together makes the initialisation sequence obvious and
    // self-contained.  The if(!m_depthStencilState) guard prevents
    // re-creation on every resize while still creating it on the first call.
    // -----------------------------------------------------------------------
    if (!m_depthStencilState)
    {
        D3D11_DEPTH_STENCIL_DESC dsStateDesc = {};
        dsStateDesc.DepthEnable    = TRUE;
        dsStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dsStateDesc.DepthFunc      = D3D11_COMPARISON_LESS;
        dsStateDesc.StencilEnable  = FALSE;   // Not used yet

        hr = m_device->CreateDepthStencilState(&dsStateDesc, &m_depthStencilState);
        if (FAILED(hr))
        {
            m_depthStencilView->Release();  m_depthStencilView = nullptr;
            m_depthStencilTex->Release();   m_depthStencilTex  = nullptr;
            std::cerr << "[D3D11Renderer] CreateDepthStencilState failed.\n";
            return false;
        }
    }

    return true;
}

// ===========================================================================
// ReleaseDepthStencilBuffer — release DST + DSV (NOT the state)
// ===========================================================================

void D3D11Renderer::ReleaseDepthStencilBuffer()
{
    // TEACHING NOTE — We do NOT release m_depthStencilState here because it is
    // device-level and not swap-chain-sized.  Releasing it on resize would
    // force an unnecessary CreateDepthStencilState() round-trip.  It is
    // released once in Shutdown().
    if (m_depthStencilView) { m_depthStencilView->Release(); m_depthStencilView = nullptr; }
    if (m_depthStencilTex)  { m_depthStencilTex->Release();  m_depthStencilTex  = nullptr; }
}

// ===========================================================================
// LoadPBRIBLScene — build PBR + IBL sphere scene (M16)
// ===========================================================================
// TEACHING NOTE — Image-Based Lighting (IBL) Procedural Generation
// ===========================================================================
// A production engine loads IBL textures from pre-cooked HDR cubemap files.
// For the teaching engine we generate all three IBL textures on the CPU at
// scene-load time using well-known mathematical algorithms:
//
//   BRDF LUT — 64×64 RG8_UNORM.
//     For each texel (NoV=u, roughness=v):
//       Integrate the GGX specular BRDF over the hemisphere using a
//       Hammersley quasi-random sequence (128 samples).  Store the Fresnel
//       scale in R and the Fresnel bias in G.
//
//   Irradiance cubemap — 16×16 × 6 faces, RGB8.
//     For each face direction:
//       Average the procedural sky colour over a cosine-weighted hemisphere
//       (64 samples).  The result is the pre-convolved diffuse irradiance.
//
//   Prefiltered env cubemap — 16×16 × 6 faces × 5 mip levels, RGB8.
//     For each (face, mip level):
//       roughness = mip / 4.  Importance-sample the procedural sky with the
//       GGX NDF (64 samples per texel).  Lower mips → sharper reflections.
//
// All three textures are created as D3D11_USAGE_IMMUTABLE after the CPU
// fills the data — no further updates are needed.
// ===========================================================================

namespace {

// ---------------------------------------------------------------------------
// TEACHING NOTE — Hammersley Quasi-Random Sequence
// ---------------------------------------------------------------------------
// The Hammersley sequence gives a well-distributed set of 2D sample points
// in [0,1)^2 for numerical integration.  It is based on Van der Corput's
// radical inverse function which reverses the bits of the integer index.
// Compared to pseudo-random (uniform) sampling, the Hammersley sequence
// achieves the same visual quality with far fewer samples (less variance).
//
// This is the same sampling strategy used in Unreal Engine's IBL baker.
// ---------------------------------------------------------------------------
static float RadicalInverse_VdC(uint32_t bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u)  | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u)  | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u)  | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u)  | ((bits & 0xFF00FF00u) >> 8u);
    return static_cast<float>(bits) * 2.3283064365386963e-10f; // 1/2^32
}

static std::pair<float,float> Hammersley(uint32_t i, uint32_t N)
{
    return { static_cast<float>(i) / static_cast<float>(N),
             RadicalInverse_VdC(i) };
}

// ---------------------------------------------------------------------------
// TEACHING NOTE — GGX Importance Sampling (Tangent Space)
// ---------------------------------------------------------------------------
// Given two uniform random numbers (xi1, xi2) and a roughness α, this
// function returns the half-vector H in tangent space sampled according to
// the GGX Normal Distribution Function.
//
// The derivation:
//   cosTheta = sqrt((1 - xi2) / (1 + (α² - 1) × xi2))   [inversion method]
//   phi      = 2π × xi1                                    [azimuth uniform]
//   sinTheta = sqrt(1 - cosTheta²)
//
// H is expressed as (Hx, Hy, Hz) in tangent space (Z = surface normal).
// ---------------------------------------------------------------------------
static std::tuple<float,float,float> ImportanceSampleGGX(float xi1, float xi2,
                                                          float roughness)
{
    float a        = roughness * roughness;
    float phi      = 2.0f * kPi * xi1;
    // TEACHING NOTE — epsilon placement: add to the full denominator expression
    // so the protection is clearly outside the main formula term.
    float cosTheta = std::sqrt((1.0f - xi2) /
                               ((1.0f + (a * a - 1.0f) * xi2) + 1e-8f));
    float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
    return { sinTheta * std::cos(phi),
             sinTheta * std::sin(phi),
             cosTheta };
}

// ---------------------------------------------------------------------------
// TEACHING NOTE — Smith-Schlick-GGX Geometry (IBL variant)
// ---------------------------------------------------------------------------
// For the BRDF LUT precomputation we use the IBL variant of k:
//   k_IBL = roughness² / 2
// (Direct lighting uses k_direct = (roughness+1)² / 8.)
// The IBL variant is used here because we integrate over all incoming
// directions (not just a single directional light).
// ---------------------------------------------------------------------------
static float G_IBL(float NdotV, float roughness)
{
    float a = roughness * roughness;
    float k = a / 2.0f;
    return NdotV / (NdotV * (1.0f - k) + k + 1e-7f);
}

static float GeometrySmith_IBL(float NdotV, float NdotL, float roughness)
{
    return G_IBL(NdotV, roughness) * G_IBL(NdotL, roughness);
}

// ---------------------------------------------------------------------------
// TEACHING NOTE — Integrate BRDF for one (NoV, roughness) sample
// ---------------------------------------------------------------------------
// This computes the two components of the split-sum BRDF LUT for a single
// texel.  The integral is:
//
//   scale = ∫ G(N,V,L,α) × (1-Fc) × VdotH / (NdotV × NdotH) dΩ
//   bias  = ∫ G(N,V,L,α) × Fc    × VdotH / (NdotV × NdotH) dΩ
//
// where Fc = (1 - VdotH)^5 (Schlick Fresnel factor — the F0-independent part).
//
// The integral is solved numerically using Hammersley importance sampling
// over the GGX NDF.  The view vector V is reconstructed from NdotV (assuming
// V lies in the N-T plane, so Vy=0 in tangent space).
// ---------------------------------------------------------------------------
static std::pair<float,float> IntegrateBRDF(float NdotV, float roughness,
                                             uint32_t numSamples)
{
    // Reconstruct V in tangent space (N = +Z axis, T = +X axis).
    float sinTheta = std::sqrt(std::max(0.0f, 1.0f - NdotV * NdotV));
    float Vx = sinTheta;   // V = (sinθ, 0, cosθ) = (sinθ, 0, NdotV)
    float Vz = NdotV;

    float scale = 0.0f, bias = 0.0f;

    for (uint32_t i = 0; i < numSamples; ++i)
    {
        auto [xi1, xi2] = Hammersley(i, numSamples);
        auto [Hx, Hy, Hz] = ImportanceSampleGGX(xi1, xi2, roughness);

        // Reflect V around H to get L (in tangent space).
        float VdotH = Vx * Hx + Vz * Hz;
        float Lx    = 2.0f * VdotH * Hx - Vx;
        float Ly    = 2.0f * VdotH * Hy;
        float Lz    = 2.0f * VdotH * Hz - Vz;

        float NdotL = std::max(0.0f, Lz);
        float NdotH = std::max(0.0f, Hz);

        if (NdotL > 0.0f && VdotH > 0.0f)
        {
            float G    = GeometrySmith_IBL(NdotV, NdotL, roughness);
            float Gvis = (G * VdotH) /
                         (std::max(NdotH, 1e-7f) * std::max(NdotV, 1e-7f));
            float tmp  = 1.0f - std::max(0.0f, VdotH);
            float Fc   = tmp * tmp * tmp * tmp * tmp;   // (1-VdotH)^5
            scale += (1.0f - Fc) * Gvis;
            bias  += Fc           * Gvis;
        }
    }

    scale /= static_cast<float>(numSamples);
    bias  /= static_cast<float>(numSamples);
    return { std::min(1.0f, std::max(0.0f, scale)),
             std::min(1.0f, std::max(0.0f, bias)) };
}

// ---------------------------------------------------------------------------
// TEACHING NOTE — Procedural Sky Environment Colour
// ---------------------------------------------------------------------------
// Returns a physically plausible sky colour for a given world-space direction.
// This is a gradient from ground (warm grey-brown) through horizon (light
// blue) to zenith (deep blue).
//
// In a production engine this function would instead sample an HDR cubemap
// loaded from a .hdr file (e.g. PolyHaven skies).  The procedural version
// here has the same structure — the only difference is the colour source.
// ---------------------------------------------------------------------------
static void SkyColour(float dx, float dy, float dz,
                      float& r, float& g, float& b)
{
    float len = std::sqrt(dx*dx + dy*dy + dz*dz);
    if (len < 1e-6f) { r = g = b = 0.0f; return; }
    float ny = dy / len;   // normalised elevation component

    // Sky hemisphere: horizon → zenith colour gradient.
    float t = std::max(0.0f, ny);   // 0 at horizon, 1 at zenith
    r = 0.55f + (0.10f - 0.55f) * t;  // horizon 0.55 → zenith 0.10
    g = 0.75f + (0.40f - 0.75f) * t;  // horizon 0.75 → zenith 0.40
    b = 0.90f + (0.85f - 0.90f) * t;  // horizon 0.90 → zenith 0.85

    // Ground hemisphere: blend toward warm grey-brown below horizon.
    if (ny < 0.0f)
    {
        float bt = std::min(1.0f, -ny * 3.0f);
        r = r * (1.0f - bt) + 0.22f * bt;
        g = g * (1.0f - bt) + 0.18f * bt;
        b = b * (1.0f - bt) + 0.14f * bt;
    }
}

// ---------------------------------------------------------------------------
// TEACHING NOTE — Cubemap Face Direction (D3D11 convention)
// ---------------------------------------------------------------------------
// D3D11 cubemaps use a left-handed coordinate system per face:
//
//   Face 0 (+X): U = -Z, V = -Y  →  dir = (+1, -v,  -u)
//   Face 1 (-X): U = +Z, V = -Y  →  dir = (-1, -v,  +u)
//   Face 2 (+Y): U = +X, V = +Z  →  dir = (+u, +1,  +v)
//   Face 3 (-Y): U = +X, V = -Z  →  dir = (+u, -1,  -v)
//   Face 4 (+Z): U = +X, V = -Y  →  dir = (+u, -v,  +1)
//   Face 5 (-Z): U = -X, V = -Y  →  dir = (-u, -v,  -1)
//
// u, v ∈ [-1, +1] (centre of texel, remapped from [0, size]).
// ---------------------------------------------------------------------------
static void CubeFaceDir(int face, float u, float v,
                         float& dx, float& dy, float& dz)
{
    switch (face)
    {
        case 0: dx =  1;  dy = -v;  dz = -u;  break;   // +X
        case 1: dx = -1;  dy = -v;  dz =  u;  break;   // -X
        case 2: dx =  u;  dy =  1;  dz =  v;  break;   // +Y
        case 3: dx =  u;  dy = -1;  dz = -v;  break;   // -Y
        case 4: dx =  u;  dy = -v;  dz =  1;  break;   // +Z
        default: dx = -u; dy = -v;  dz = -1;  break;   // -Z (face 5)
    }
}

// ---------------------------------------------------------------------------
// TEACHING NOTE — Orthonormal Basis from a Normal Vector
// ---------------------------------------------------------------------------
// To integrate over a hemisphere aligned with a surface normal N, we need
// tangent (T) and bitangent (B) vectors perpendicular to N.
// We use the "frisvad" method (avoids degenerate cases at N=(0,0,1)):
//   if |Nz| < 0.999:  T = normalize(cross(N, (0,1,0)))
//   else:             T = normalize(cross(N, (0,0,1)))
// B = cross(N, T).
// ---------------------------------------------------------------------------
static void BuildBasis(float nx, float ny, float nz,
                        float& tx, float& ty, float& tz,
                        float& bx, float& by, float& bz)
{
    // Choose a stable up-vector not aligned with N.
    float ux = 0.0f, uy = 1.0f, uz = 0.0f;
    if (std::abs(ny) > 0.999f) { ux = 0.0f; uy = 0.0f; uz = 1.0f; }

    // T = cross(N, U)
    tx = ny * uz - nz * uy;
    ty = nz * ux - nx * uz;
    tz = nx * uy - ny * ux;
    float tlen = std::sqrt(tx*tx + ty*ty + tz*tz);
    if (tlen < 1e-8f) { tx = 1; ty = tz = 0; }
    else { tx /= tlen; ty /= tlen; tz /= tlen; }

    // B = cross(N, T)
    bx = ny * tz - nz * ty;
    by = nz * tx - nx * tz;
    bz = nx * ty - ny * tx;
}

// ---------------------------------------------------------------------------
// Generate the 64×64 BRDF LUT pixel data (128 samples per texel).
// Output: pixels array of size 64×64×2 bytes (RG8_UNORM: scale, bias).
// ---------------------------------------------------------------------------
static void GenerateBRDFLUT(std::vector<uint8_t>& pixels,
                              uint32_t size = 64,
                              uint32_t numSamples = 128)
{
    // TEACHING NOTE — BRDF LUT Layout
    // The LUT is a 2D texture indexed by (NoV = U, roughness = V).
    // Row 0 = roughness 0, row (size-1) = roughness 1.
    // Column 0 = NoV ≈ 0 (grazing), column (size-1) = NoV = 1 (normal incidence).
    pixels.resize(size * size * 2);   // 2 bytes per texel (RG8)

    for (uint32_t row = 0; row < size; ++row)
    {
        float roughness = (static_cast<float>(row) + 0.5f) / static_cast<float>(size);
        roughness = std::max(roughness, 0.04f);   // avoid pure-mirror singularity

        for (uint32_t col = 0; col < size; ++col)
        {
            float NoV = (static_cast<float>(col) + 0.5f) / static_cast<float>(size);
            NoV = std::max(NoV, 0.001f);

            auto [sc, bi] = IntegrateBRDF(NoV, roughness, numSamples);

            uint32_t idx = (row * size + col) * 2;
            pixels[idx + 0] = static_cast<uint8_t>(sc * 255.0f + 0.5f);   // R = scale
            pixels[idx + 1] = static_cast<uint8_t>(bi * 255.0f + 0.5f);   // G = bias
        }
    }
}

// ---------------------------------------------------------------------------
// Generate one face of the irradiance cubemap (cosine-weighted hemisphere).
// face: 0..5, size: texels per edge, numSamples: hemisphere sample count.
// Output: 'out' array appended with size*size*3 bytes (RGB8).
// ---------------------------------------------------------------------------
static void GenerateIrradianceFace(int face, uint32_t size,
                                    uint32_t numSamples,
                                    std::vector<uint8_t>& out)
{
    // TEACHING NOTE — Hemisphere Integration for Irradiance
    // For each texel on the cubemap face, we find the world-space direction
    // (the surface normal N), build a tangent basis, and sum cosine-weighted
    // sky samples over the upper hemisphere.  The result is the irradiance
    // at that normal direction — used as the diffuse ambient term in the PS.
    for (uint32_t row = 0; row < size; ++row)
    {
        float fv = (static_cast<float>(row) + 0.5f) / static_cast<float>(size)
                   * 2.0f - 1.0f;   // [-1, +1]
        for (uint32_t col = 0; col < size; ++col)
        {
            float fu = (static_cast<float>(col) + 0.5f) / static_cast<float>(size)
                       * 2.0f - 1.0f;

            float nx, ny, nz;
            CubeFaceDir(face, fu, fv, nx, ny, nz);
            float nlen = std::sqrt(nx*nx + ny*ny + nz*nz);
            nx /= nlen; ny /= nlen; nz /= nlen;

            float tx, ty, tz, bx, by, bz;
            BuildBasis(nx, ny, nz, tx, ty, tz, bx, by, bz);

            float accR = 0, accG = 0, accB = 0, weight = 0;
            for (uint32_t i = 0; i < numSamples; ++i)
            {
                auto [xi1, xi2] = Hammersley(i, numSamples);
                // Cosine-weighted hemisphere sampling:
                float phi = 2.0f * kPi * xi1;
                float cosT = std::sqrt(xi2);
                float sinT = std::sqrt(1.0f - xi2);
                // Sample direction in world space.
                float lx = sinT * std::cos(phi) * tx + sinT * std::sin(phi) * bx + cosT * nx;
                float ly = sinT * std::cos(phi) * ty + sinT * std::sin(phi) * by + cosT * ny;
                float lz = sinT * std::cos(phi) * tz + sinT * std::sin(phi) * bz + cosT * nz;
                float NdotL = std::max(0.0f, lx*nx + ly*ny + lz*nz);
                float sr, sg, sb;
                SkyColour(lx, ly, lz, sr, sg, sb);
                // Cosine-weighted: PDF = NdotL/π, weight = NdotL * (1/PDF) = π
                // Simplified: just accumulate and divide — equivalent to π factor.
                accR += sr * NdotL;
                accG += sg * NdotL;
                accB += sb * NdotL;
                weight += NdotL;
            }
            if (weight > 1e-6f) { accR /= weight; accG /= weight; accB /= weight; }
            out.push_back(static_cast<uint8_t>(std::min(1.0f, accR) * 255.0f + 0.5f));
            out.push_back(static_cast<uint8_t>(std::min(1.0f, accG) * 255.0f + 0.5f));
            out.push_back(static_cast<uint8_t>(std::min(1.0f, accB) * 255.0f + 0.5f));
        }
    }
}

// ---------------------------------------------------------------------------
// Generate one mip level + face of the prefiltered env cubemap.
// roughness: 0..1 for this mip level.
// Output: 'out' array appended with size*size*3 bytes (RGB8).
// ---------------------------------------------------------------------------
static void GeneratePrefilteredFace(int face, uint32_t size,
                                     float roughness, uint32_t numSamples,
                                     std::vector<uint8_t>& out)
{
    // TEACHING NOTE — Prefiltered Environment Map Generation
    // For each texel (= reflection direction R), we importance-sample the
    // GGX NDF to generate a set of half-vectors H.  We reflect R around each
    // H to get incoming light directions L, then look up the sky colour at L.
    // The weighted average gives the prefiltered environment for this roughness.
    //
    // At roughness=0 (mip 0) we sample near-mirror directions → sharp env.
    // At roughness=1 (mip 4) we sample a broad hemisphere → blurry env.
    //
    // TEACHING NOTE — Roughness clamping
    // roughness=0 (perfect mirror) causes ImportanceSampleGGX to produce all
    // samples concentrated at the mirror direction.  For the very first mip we
    // clamp to 0.04 so there is slight spread (avoids aliasing in practice).
    float eff_roughness = std::max(roughness, 0.04f);

    for (uint32_t row = 0; row < size; ++row)
    {
        float fv = (static_cast<float>(row) + 0.5f) / static_cast<float>(size)
                   * 2.0f - 1.0f;
        for (uint32_t col = 0; col < size; ++col)
        {
            float fu = (static_cast<float>(col) + 0.5f) / static_cast<float>(size)
                       * 2.0f - 1.0f;

            // R = the reflection direction for this texel.
            float rx, ry, rz;
            CubeFaceDir(face, fu, fv, rx, ry, rz);
            float rlen = std::sqrt(rx*rx + ry*ry + rz*rz);
            rx /= rlen; ry /= rlen; rz /= rlen;

            // For very low roughness, treat R as the view direction V ≈ N = R.
            float tx, ty, tz, bx, by, bz;
            BuildBasis(rx, ry, rz, tx, ty, tz, bx, by, bz);

            float accR = 0, accG = 0, accB = 0, wTotal = 0;
            for (uint32_t i = 0; i < numSamples; ++i)
            {
                auto [xi1, xi2] = Hammersley(i, numSamples);
                auto [Hx, Hy, Hz] = ImportanceSampleGGX(xi1, xi2, eff_roughness);

                // Transform H from tangent space to world space.
                float Hwx = Hx * tx + Hy * bx + Hz * rx;
                float Hwy = Hx * ty + Hy * by + Hz * ry;
                float Hwz = Hx * tz + Hy * bz + Hz * rz;

                // L = reflect(R, H) = 2*(R·H)*H - R
                float RdotH = rx * Hwx + ry * Hwy + rz * Hwz;
                float Lx = 2.0f * RdotH * Hwx - rx;
                float Ly = 2.0f * RdotH * Hwy - ry;
                float Lz = 2.0f * RdotH * Hwz - rz;

                // NdotL = dot(R, L) — only contribute if L is in upper hemi.
                float NdotL = std::max(0.0f, rx * Lx + ry * Ly + rz * Lz);
                if (NdotL > 0.0f)
                {
                    float sr, sg, sb;
                    SkyColour(Lx, Ly, Lz, sr, sg, sb);
                    accR += sr * NdotL;
                    accG += sg * NdotL;
                    accB += sb * NdotL;
                    wTotal += NdotL;
                }
            }
            if (wTotal > 1e-6f) { accR /= wTotal; accG /= wTotal; accB /= wTotal; }
            out.push_back(static_cast<uint8_t>(std::min(1.0f, accR) * 255.0f + 0.5f));
            out.push_back(static_cast<uint8_t>(std::min(1.0f, accG) * 255.0f + 0.5f));
            out.push_back(static_cast<uint8_t>(std::min(1.0f, accB) * 255.0f + 0.5f));
        }
    }
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Helper: compile a shader from file or fallback string.
// (Declared static to avoid ODR conflicts with the same helper in other
// translation units that have the same pattern.)
// ---------------------------------------------------------------------------
static ID3DBlob* CompileIBLShader(const std::filesystem::path& path,
                                   const char* fallback,
                                   const char* entry,
                                   const char* target)
{
    namespace fs = std::filesystem;
    ID3DBlob* blob = nullptr;
    ID3DBlob* errBlob = nullptr;

    if (fs::exists(path))
    {
        std::wstring wpath = path.wstring();
        HRESULT hr = D3DCompileFromFile(wpath.c_str(), nullptr, nullptr,
                                        entry, target,
                                        D3DCOMPILE_ENABLE_STRICTNESS, 0,
                                        &blob, &errBlob);
        if (FAILED(hr))
        {
            if (errBlob) {
                std::cerr << "[IBL] Shader compile error: "
                          << static_cast<const char*>(errBlob->GetBufferPointer()) << "\n";
                errBlob->Release();
            }
            blob = nullptr;
        }
        if (errBlob) errBlob->Release();
        if (blob) return blob;
    }

    // Fallback: compile from inline string.
    HRESULT hr = D3DCompile(fallback, std::strlen(fallback),
                             nullptr, nullptr, nullptr,
                             entry, target,
                             D3DCOMPILE_ENABLE_STRICTNESS, 0,
                             &blob, &errBlob);
    if (FAILED(hr))
    {
        if (errBlob) {
            std::cerr << "[IBL] Fallback compile error: "
                      << static_cast<const char*>(errBlob->GetBufferPointer()) << "\n";
            errBlob->Release();
        }
        return nullptr;
    }
    if (errBlob) errBlob->Release();
    return blob;
}

// ---------------------------------------------------------------------------
// Inline HLSL fallbacks for pbr_ibl.vs.hlsl and pbr_ibl.ps.hlsl.
// Minimal but correct — used when .hlsl files have not been copied yet.
// ---------------------------------------------------------------------------
static const char* kIBLVsFallback =
    "cbuffer PerFrameCB:register(b0){"
    "float4x4 g_world;float4x4 g_worldInvTrans;float4x4 g_view;float4x4 g_proj;};"
    "struct VSIn{float3 pos:POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;};"
    "struct PSIn{float4 cp:SV_POSITION;float3 wp:TEXCOORD1;float3 wn:NORMAL;float2 uv:TEXCOORD0;};"
    "PSIn main(VSIn i){"
    "PSIn o;"
    "float4 wp4=mul(float4(i.pos,1),g_world);o.wp=wp4.xyz;"
    "o.cp=mul(mul(wp4,g_view),g_proj);"
    "o.wn=normalize(mul(i.n,(float3x3)g_worldInvTrans));"
    "o.uv=i.uv;return o;}";

static const char* kIBLPsFallback =
    "cbuffer LightCB:register(b1){float3 g_cameraPos;float g_p0;"
    "float3 g_lightDir;float g_p1;float3 g_lightColor;float g_lightIntensity;};"
    "cbuffer MatCB:register(b2){float3 g_albedo;float g_metallic;float g_roughness;float g_ao;float2 gp;};"
    "Texture2D g_brdfLut:register(t0);"
    "TextureCube g_irradianceCube:register(t1);"
    "TextureCube g_prefilteredEnv:register(t2);"
    "Texture2D g_albedoMap:register(t3);"
    "Texture2D g_normalMap:register(t4);"
    "Texture2D g_metallicRoughnessMap:register(t5);"
    "Texture2D g_aoMap:register(t6);"
    "SamplerState g_linearSampler:register(s0);"
    "struct PSIn{float4 cp:SV_POSITION;float3 wp:TEXCOORD1;float3 wn:NORMAL;float2 uv:TEXCOORD0;};"
    "float4 main(PSIn i):SV_TARGET{"
    "float3 nTex=g_normalMap.Sample(g_linearSampler,i.uv).xyz*2-1;"
    "float3 N=normalize(i.wn+nTex*0.25);float3 V=normalize(g_cameraPos-i.wp);"
    "float NdotV=max(dot(N,V),0);"
    "float3 albTex=g_albedoMap.Sample(g_linearSampler,i.uv).rgb;"
    "float2 mrTex=g_metallicRoughnessMap.Sample(g_linearSampler,i.uv).gb;"
    "float aoTex=g_aoMap.Sample(g_linearSampler,i.uv).r;"
    "float3 albedo=saturate(g_albedo*albTex);"
    "float metallic=saturate(g_metallic*mrTex.y);"
    "float roughness=saturate(g_roughness*mrTex.x);"
    "float ao=saturate(g_ao*aoTex);"
    "float3 F0=lerp(float3(0.04,0.04,0.04),albedo,metallic);"
    "float3 kS=F0+(max(float3(1-roughness,1-roughness,1-roughness),F0)-F0)*pow(1-NdotV,5);"
    "float3 kD=(1-kS)*(1-metallic);"
    "float3 irr=g_irradianceCube.Sample(g_linearSampler,N).rgb;"
    "float3 R=reflect(-V,N);"
    "float3 pre=g_prefilteredEnv.SampleLevel(g_linearSampler,R,roughness*4).rgb;"
    "float2 brdf=g_brdfLut.Sample(g_linearSampler,float2(NdotV,roughness)).rg;"
    "float3 ambient=(kD*irr*albedo+pre*(kS*brdf.r+brdf.g))*ao;"
    "float3 L=normalize(g_lightDir);float NdotL=max(dot(N,L),0);"
    "float3 direct=albedo/3.14159*g_lightColor*g_lightIntensity*NdotL;"
    "float3 c=ambient+direct;c=c/(c+1);c=pow(max(c,0),0.4545);"
    "return float4(c,1);}";

static bool LoadPBRIBLScene(ID3D11Device*              device,
                             const std::string&         shaderDir,
                             D3D11Renderer::PBRIBLScene& scene)
{
    namespace fs = std::filesystem;
    AuthoredMaterialParams authoredMat;
    const bool hasAuthoredMaterial = TryLoadAuthoredMaterial(authoredMat);

    // -----------------------------------------------------------------------
    // Step 1 — Compile shaders.
    // -----------------------------------------------------------------------
    const fs::path vsPath = fs::path(shaderDir) / "pbr_ibl.vs.hlsl";
    const fs::path psPath = fs::path(shaderDir) / "pbr_ibl.ps.hlsl";

    ID3DBlob* vsBlob = CompileIBLShader(vsPath, kIBLVsFallback, "main", "vs_4_0");
    if (!vsBlob) { std::cerr << "[IBL] VS compile failed.\n"; return false; }

    ID3DBlob* psBlob = CompileIBLShader(psPath, kIBLPsFallback, "main", "ps_4_0");
    if (!psBlob) {
        vsBlob->Release();
        std::cerr << "[IBL] PS compile failed.\n"; return false;
    }

    HRESULT hr = device->CreateVertexShader(vsBlob->GetBufferPointer(),
                                             vsBlob->GetBufferSize(),
                                             nullptr, &scene.vs);
    if (FAILED(hr)) {
        vsBlob->Release(); psBlob->Release();
        std::cerr << "[IBL] CreateVertexShader failed.\n"; return false;
    }

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(),
                                   psBlob->GetBufferSize(),
                                   nullptr, &scene.ps);
    psBlob->Release();
    if (FAILED(hr)) {
        vsBlob->Release();
        std::cerr << "[IBL] CreatePixelShader failed.\n"; return false;
    }

    // -----------------------------------------------------------------------
    // Step 2 — Create input layout.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Input Layout for PBR+IBL
    // Same layout as pbr_mesh: POSITION (float3) + NORMAL (float3) + TEXCOORD0 (float2).
    // Each vertex is 32 bytes.  The layout must match the vertex shader's
    // VSInput struct and the vertex buffer data generated in Step 3.
    // -----------------------------------------------------------------------
    D3D11_INPUT_ELEMENT_DESC inputElems[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = device->CreateInputLayout(inputElems, 3,
                                   vsBlob->GetBufferPointer(),
                                   vsBlob->GetBufferSize(),
                                   &scene.inputLayout);
    vsBlob->Release();
    if (FAILED(hr)) {
        std::cerr << "[IBL] CreateInputLayout failed.\n";
        scene.vs->Release(); scene.vs = nullptr;
        scene.ps->Release(); scene.ps = nullptr;
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 3 — Generate UV sphere geometry (same algorithm as pbr_mesh).
    // -----------------------------------------------------------------------
    // TEACHING NOTE — UV Sphere vs Icosphere
    // A UV sphere divides the sphere into stacks (latitudinal rings) and
    // slices (longitudinal strips).  It is simple to generate and gives clean
    // UV coordinates (U = longitude/2π, V = latitude/π) suitable for
    // texture mapping.  The poles have degenerate triangles (zero-area wedges)
    // which is why production engines often prefer icospheres, but for
    // teaching the UV sphere's regular structure is easier to understand.
    // -----------------------------------------------------------------------
    const int kStacks = 16, kSlices = 16;
    std::vector<float>    verts;
    std::vector<uint16_t> indices;
    std::string authoredMeshPath;

    if (TryLoadAuthoredMesh(verts, indices, authoredMeshPath))
    {
        std::cout << "[D3D11Renderer] Loaded authored cooked mesh: "
                  << authoredMeshPath << "\n";
    }
    else
    {
        for (int s = 0; s <= kStacks; ++s)
        {
            float phi = kPi * static_cast<float>(s) / static_cast<float>(kStacks);
            float sinPhi = std::sin(phi), cosPhi = std::cos(phi);

            for (int sl = 0; sl <= kSlices; ++sl)
            {
                float theta  = 2.0f * kPi * static_cast<float>(sl) / static_cast<float>(kSlices);
                float x = sinPhi * std::cos(theta);
                float y = cosPhi;
                float z = sinPhi * std::sin(theta);
                float u = static_cast<float>(sl) / static_cast<float>(kSlices);
                float v = static_cast<float>(s)  / static_cast<float>(kStacks);
                verts.insert(verts.end(), { x, y, z, x, y, z, u, v }); // pos+normal+uv
            }
        }

        for (int s = 0; s < kStacks; ++s)
        {
            for (int sl = 0; sl < kSlices; ++sl)
            {
                uint16_t a = static_cast<uint16_t>( s      * (kSlices + 1) + sl);
                uint16_t b = static_cast<uint16_t>((s + 1) * (kSlices + 1) + sl);
                uint16_t c = static_cast<uint16_t>( s      * (kSlices + 1) + sl + 1);
                uint16_t d = static_cast<uint16_t>((s + 1) * (kSlices + 1) + sl + 1);
                indices.insert(indices.end(), { a, b, c, b, d, c });
            }
        }
    }
    scene.indexCount = static_cast<int>(indices.size());

    // -----------------------------------------------------------------------
    // Upload vertex buffer.
    // -----------------------------------------------------------------------
    D3D11_BUFFER_DESC vbd = {};
    vbd.ByteWidth  = static_cast<UINT>(verts.size() * sizeof(float));
    vbd.Usage      = D3D11_USAGE_IMMUTABLE;
    vbd.BindFlags  = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vbd_data = {};
    vbd_data.pSysMem = verts.data();
    hr = device->CreateBuffer(&vbd, &vbd_data, &scene.vertexBuf);
    if (FAILED(hr)) { std::cerr << "[IBL] CreateBuffer(VB) failed.\n"; return false; }

    // Upload index buffer.
    D3D11_BUFFER_DESC ibd = {};
    ibd.ByteWidth = static_cast<UINT>(indices.size() * sizeof(uint16_t));
    ibd.Usage     = D3D11_USAGE_IMMUTABLE;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA ibd_data = {};
    ibd_data.pSysMem = indices.data();
    hr = device->CreateBuffer(&ibd, &ibd_data, &scene.indexBuf);
    if (FAILED(hr)) { std::cerr << "[IBL] CreateBuffer(IB) failed.\n"; return false; }

    // -----------------------------------------------------------------------
    // Step 4 — Create constant buffers.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Constant Buffer Sizes must be multiples of 16 bytes.
    // PerFrameCB: 4 × float4x4 = 256 bytes.
    // LightCB:    2 × float4 + float3 + float = 48 bytes → pad to 48.
    // MaterialCB: float3 + 4 floats = 28 bytes → pad to 32.
    // -----------------------------------------------------------------------
    auto makeCB = [&](UINT byteWidth) -> ID3D11Buffer*
    {
        D3D11_BUFFER_DESC cbd = {};
        cbd.ByteWidth      = byteWidth;
        cbd.Usage          = D3D11_USAGE_DYNAMIC;
        cbd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        ID3D11Buffer* buf = nullptr;
        device->CreateBuffer(&cbd, nullptr, &buf);
        return buf;
    };

    scene.perFrameCB = makeCB(256);   // 4 × Mat4
    scene.lightCB    = makeCB(48);    // float3+pad + float3+pad + float3+float
    scene.materialCB = makeCB(32);    // float3+float+float+float+float2

    if (!scene.perFrameCB || !scene.lightCB || !scene.materialCB)
    {
        std::cerr << "[IBL] CreateBuffer(CB) failed.\n";
        return false;
    }

    // -----------------------------------------------------------------------
    // Upload static CBs (light + material — set once at load time).
    // -----------------------------------------------------------------------
    // Light CB: camera pos, light dir (toward sun), light colour, intensity.
    // TEACHING NOTE — We use an immediate DeviceContext write via Map/Unmap
    // instead of UpdateSubresource because the CB is DYNAMIC.  UpdateSubresource
    // on a DYNAMIC buffer is slower than Map/Unmap on some drivers.
    // -----------------------------------------------------------------------
    {
        struct alignas(16) LightData {
            float cameraPos[3]; float p0;
            float lightDir[3];  float p1;
            float lightColor[3]; float lightIntensity;
        };
        ID3D11DeviceContext* ctx = nullptr;
        device->GetImmediateContext(&ctx);

        D3D11_MAPPED_SUBRESOURCE msr = {};
        if (ctx && SUCCEEDED(ctx->Map(scene.lightCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr)))
        {
            auto* d = static_cast<LightData*>(msr.pData);
            d->cameraPos[0] = 0.0f;
            d->cameraPos[1] = 0.5f;
            d->cameraPos[2] = 4.0f;
            d->p0           = 0.0f;
            // Light direction: normalized (0.5, 0.8, 0.3) — slightly elevated.
            float len        = std::sqrt(0.5f*0.5f + 0.8f*0.8f + 0.3f*0.3f);
            d->lightDir[0]  = 0.5f / len;
            d->lightDir[1]  = 0.8f / len;
            d->lightDir[2]  = 0.3f / len;
            d->p1           = 0.0f;
            d->lightColor[0]   = 1.0f;
            d->lightColor[1]   = 0.96f;
            d->lightColor[2]   = 0.9f;
            d->lightIntensity  = 3.0f;
            ctx->Unmap(scene.lightCB, 0);
        }

        // Material CB: gold-coloured sphere (metallic 1, roughness 0.3).
        struct alignas(16) MatData {
            float albedo[3];  float metallic;
            float roughness;  float ao;   float pad[2];
        };
        if (ctx && SUCCEEDED(ctx->Map(scene.materialCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr)))
        {
            auto* m = static_cast<MatData*>(msr.pData);
            m->albedo[0]  = 1.00f;   // gold base colour
            m->albedo[1]  = 0.76f;
            m->albedo[2]  = 0.33f;
            m->metallic   = 1.0f;
            m->roughness  = 0.3f;
            m->ao         = 1.0f;
            m->pad[0]     = 0.0f;
            m->pad[1]     = 0.0f;

            if (hasAuthoredMaterial)
            {
                m->albedo[0]  = authoredMat.albedo[0];
                m->albedo[1]  = authoredMat.albedo[1];
                m->albedo[2]  = authoredMat.albedo[2];
                m->metallic   = authoredMat.metallic;
                m->roughness  = authoredMat.roughness;
                m->ao         = authoredMat.ao;
                std::cout << "[D3D11Renderer] Loaded authored IBL material params from: "
                          << authoredMat.loadedFromPath << "\n";
            }
            ctx->Unmap(scene.materialCB, 0);
        }

        if (ctx) ctx->Release();
    }

    // -----------------------------------------------------------------------
    // Step 5 — Load authored material maps (M23) with fallback 1×1 textures.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Fallback map conventions
    //   Albedo fallback:              white (1,1,1)
    //   Normal fallback:              +Z tangent normal (0.5,0.5,1.0)
    //   Metallic-Roughness fallback:  G=roughness, B=metallic
    //   AO fallback:                  white (1.0)
    // This keeps shading stable when M24 authored texture content is sparse.
    // -----------------------------------------------------------------------
    {
        ID3D11DeviceContext* ctx = nullptr;
        device->GetImmediateContext(&ctx);
        if (ctx)
        {
            const uint8_t kWhite[4]          = {255, 255, 255, 255};
            const uint8_t kFlatNormal[4]     = {128, 128, 255, 255};
            const float fallbackRoughness = hasAuthoredMaterial ? authoredMat.roughness : 0.3f;
            const float fallbackMetallic  = hasAuthoredMaterial ? authoredMat.metallic  : 1.0f;
            const uint8_t kDefaultMR[4]      = {
                0,
                static_cast<uint8_t>(std::max(0.0f, std::min(1.0f, fallbackRoughness)) * 255.0f),
                static_cast<uint8_t>(std::max(0.0f, std::min(1.0f, fallbackMetallic)) * 255.0f),
                255
            };
            const uint8_t kDefaultAO[4]      = {255, 255, 255, 255};

            LoadAuthoredTextureWithFallback(device, ctx, authoredMat,
                                            authoredMat.albedoTextureRelPath,
                                            kWhite,
                                            scene.albedoMap,
                                            scene.albedoFallbackTex,
                                            scene.albedoFallbackSRV);
            LoadAuthoredTextureWithFallback(device, ctx, authoredMat,
                                            authoredMat.normalTextureRelPath,
                                            kFlatNormal,
                                            scene.normalMap,
                                            scene.normalFallbackTex,
                                            scene.normalFallbackSRV);
            LoadAuthoredTextureWithFallback(device, ctx, authoredMat,
                                            authoredMat.metallicRoughnessTextureRelPath,
                                            kDefaultMR,
                                            scene.metallicRoughnessMap,
                                            scene.metallicRoughnessFallbackTex,
                                            scene.metallicRoughnessFallbackSRV);
            LoadAuthoredTextureWithFallback(device, ctx, authoredMat,
                                            authoredMat.aoTextureRelPath,
                                            kDefaultAO,
                                            scene.aoMap,
                                            scene.aoFallbackTex,
                                            scene.aoFallbackSRV);
            ctx->Release();
        }
    }

    // -----------------------------------------------------------------------
    // Step 6 — Generate and upload IBL textures.
    // -----------------------------------------------------------------------

    // ---- 5a: BRDF LUT (64×64 RG8_UNORM) ----
    // TEACHING NOTE — DXGI_FORMAT_R8G8_UNORM
    // Two 8-bit channels (R=scale, G=bias).  UNORM means values are
    // interpreted as [0.0, 1.0] in the shader.  The LUT encodes values in
    // [0, 1] so 8 bits per channel gives ~0.4% precision — sufficient for
    // diffuse IBL, though 16F is used in production for sharper specular.
    {
        const uint32_t kLUTSize = 64;
        std::vector<uint8_t> lutPixels;
        std::cout << "[IBL] Generating BRDF LUT (" << kLUTSize << "x" << kLUTSize << ") ...\n";
        GenerateBRDFLUT(lutPixels, kLUTSize, 128);

        D3D11_TEXTURE2D_DESC td = {};
        td.Width          = kLUTSize;
        td.Height         = kLUTSize;
        td.MipLevels      = 1;
        td.ArraySize      = 1;
        td.Format         = DXGI_FORMAT_R8G8_UNORM;
        td.SampleDesc     = {1, 0};
        td.Usage          = D3D11_USAGE_IMMUTABLE;
        td.BindFlags      = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA sd = {};
        sd.pSysMem     = lutPixels.data();
        sd.SysMemPitch = kLUTSize * 2;   // 2 bytes per texel (RG8)

        hr = device->CreateTexture2D(&td, &sd, &scene.brdfLutTex);
        if (FAILED(hr)) { std::cerr << "[IBL] CreateTexture2D(BRDF LUT) failed.\n"; return false; }

        hr = device->CreateShaderResourceView(scene.brdfLutTex, nullptr, &scene.brdfLutSRV);
        if (FAILED(hr)) { std::cerr << "[IBL] CreateSRV(BRDF LUT) failed.\n"; return false; }
    }

    // ---- 5b: Irradiance Cubemap (16×16×6 RGB8, 1 mip) ----
    // TEACHING NOTE — D3D11_RESOURCE_MISC_TEXTURECUBE
    // Setting MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE on a Texture2D
    // with ArraySize = 6 tells D3D11 that the six array slices are the six
    // faces of a cubemap.  The SRV then exposes it as a TextureCube to the
    // pixel shader.  Face order: +X, -X, +Y, -Y, +Z, -Z.
    {
        const uint32_t kCubeSize = 16;
        std::vector<uint8_t> allFaces;
        std::cout << "[IBL] Generating irradiance cubemap (" << kCubeSize << "x"
                  << kCubeSize << ") ...\n";
        for (int face = 0; face < 6; ++face)
            GenerateIrradianceFace(face, kCubeSize, 64, allFaces);

        // Build one D3D11_SUBRESOURCE_DATA per face.
        D3D11_SUBRESOURCE_DATA irrData[6] = {};
        for (int f = 0; f < 6; ++f)
        {
            irrData[f].pSysMem     = allFaces.data() + f * kCubeSize * kCubeSize * 3;
            irrData[f].SysMemPitch = kCubeSize * 3;
        }

        D3D11_TEXTURE2D_DESC td = {};
        td.Width          = kCubeSize;
        td.Height         = kCubeSize;
        td.MipLevels      = 1;
        td.ArraySize      = 6;
        td.Format         = DXGI_FORMAT_R8G8B8A8_UNORM;  // RGBA8: GPU-compatible cubemap
        td.SampleDesc     = {1, 0};
        td.Usage          = D3D11_USAGE_DEFAULT;
        td.BindFlags      = D3D11_BIND_SHADER_RESOURCE;
        td.MiscFlags      = D3D11_RESOURCE_MISC_TEXTURECUBE;

        // TEACHING NOTE — RGBA8 vs RGB8
        // D3D11 does not support RGB8 (24-bit) textures natively.  The
        // closest supported format is RGBA8 (32-bit).  We must convert our
        // packed RGB8 CPU data to RGBA8 before upload.
        std::vector<uint8_t> rgba(kCubeSize * kCubeSize * 6 * 4);
        for (size_t i = 0; i < kCubeSize * kCubeSize * 6; ++i)
        {
            rgba[i*4+0] = allFaces[i*3+0];
            rgba[i*4+1] = allFaces[i*3+1];
            rgba[i*4+2] = allFaces[i*3+2];
            rgba[i*4+3] = 255;
        }

        // Re-point subresource data at RGBA buffer.
        for (int f = 0; f < 6; ++f)
        {
            irrData[f].pSysMem     = rgba.data() + f * kCubeSize * kCubeSize * 4;
            irrData[f].SysMemPitch = kCubeSize * 4;
        }

        hr = device->CreateTexture2D(&td, irrData, &scene.irradianceTex);
        if (FAILED(hr)) { std::cerr << "[IBL] CreateTexture2D(irradiance) failed.\n"; return false; }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format                    = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.MipLevels       = 1;

        hr = device->CreateShaderResourceView(scene.irradianceTex, &srvDesc,
                                              &scene.irradianceSRV);
        if (FAILED(hr)) { std::cerr << "[IBL] CreateSRV(irradiance) failed.\n"; return false; }
    }

    // ---- 5c: Prefiltered Environment Cubemap (16×16×6 RGBA8, 5 mip levels) ----
    // TEACHING NOTE — Multi-Mip Cubemap Upload
    // Each mip level of each face is a separate D3D11_SUBRESOURCE_DATA.
    // The total number of subresources = 6 faces × 5 mip levels = 30.
    // Mip k has dimensions (16 >> k) × (16 >> k).
    // The subresource index = faceIndex * numMips + mipIndex.
    {
        const uint32_t kPFSize  = 16;
        const uint32_t kNumMips = 5;     // roughness 0.00, 0.25, 0.50, 0.75, 1.00
        std::cout << "[IBL] Generating prefiltered env cubemap ("
                  << kPFSize << "x" << kPFSize << " x " << kNumMips << " mips) ...\n";

        // One RGBA8 buffer per face per mip.
        // Layout: [face0_mip0][face0_mip1]...[face1_mip0]...[face5_mip4]
        // Total RGBA bytes = sum over mips of (6 × size² × 4).
        struct MipFaceBlock { std::vector<uint8_t> rgba; uint32_t size; };
        MipFaceBlock blocks[6][kNumMips];

        for (uint32_t mip = 0; mip < kNumMips; ++mip)
        {
            float roughness = static_cast<float>(mip) / static_cast<float>(kNumMips - 1);
            uint32_t mipSize = std::max(1u, kPFSize >> mip);
            const uint32_t numSamples = 64;

            for (int face = 0; face < 6; ++face)
            {
                std::vector<uint8_t> rgb;
                GeneratePrefilteredFace(face, mipSize, roughness, numSamples, rgb);

                // Expand RGB → RGBA.
                auto& blk = blocks[face][mip];
                blk.size = mipSize;
                blk.rgba.resize(mipSize * mipSize * 4);
                for (size_t i = 0; i < mipSize * mipSize; ++i)
                {
                    blk.rgba[i*4+0] = rgb[i*3+0];
                    blk.rgba[i*4+1] = rgb[i*3+1];
                    blk.rgba[i*4+2] = rgb[i*3+2];
                    blk.rgba[i*4+3] = 255;
                }
            }
        }

        // Build the 30 subresource descriptors.
        // D3D11 subresource index for a cubemap with N mips:
        //   index = faceIndex * N + mipIndex
        D3D11_SUBRESOURCE_DATA pfData[6 * kNumMips] = {};
        for (int face = 0; face < 6; ++face)
        {
            for (uint32_t mip = 0; mip < kNumMips; ++mip)
            {
                auto& blk = blocks[face][mip];
                uint32_t idx = face * kNumMips + mip;
                pfData[idx].pSysMem     = blk.rgba.data();
                pfData[idx].SysMemPitch = blk.size * 4;
            }
        }

        D3D11_TEXTURE2D_DESC td = {};
        td.Width          = kPFSize;
        td.Height         = kPFSize;
        td.MipLevels      = kNumMips;
        td.ArraySize      = 6;
        td.Format         = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc     = {1, 0};
        td.Usage          = D3D11_USAGE_IMMUTABLE;
        td.BindFlags      = D3D11_BIND_SHADER_RESOURCE;
        td.MiscFlags      = D3D11_RESOURCE_MISC_TEXTURECUBE;

        hr = device->CreateTexture2D(&td, pfData, &scene.prefilteredTex);
        if (FAILED(hr)) { std::cerr << "[IBL] CreateTexture2D(prefiltered) failed.\n"; return false; }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format                    = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.MipLevels       = kNumMips;

        hr = device->CreateShaderResourceView(scene.prefilteredTex, &srvDesc,
                                              &scene.prefilteredSRV);
        if (FAILED(hr)) { std::cerr << "[IBL] CreateSRV(prefiltered) failed.\n"; return false; }
    }

    // -----------------------------------------------------------------------
    // Step 6 — Create the linear clamp sampler (s0).
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Sampler State for IBL Textures
    // All three IBL textures use a linear filter sampler with CLAMP address
    // mode.  CLAMP is important for the BRDF LUT (NoV and roughness are both
    // in [0,1] so we must not wrap).  For cubemaps CLAMP is also the safest
    // choice — seamless cubemap filtering is available on DX11 feature level
    // 10_1+ via D3D11_FILTER_MIN_MAG_MIP_LINEAR on a TEXTURE_CUBE SRV.
    // -----------------------------------------------------------------------
    {
        D3D11_SAMPLER_DESC sd = {};
        sd.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.MaxAnisotropy  = 1;
        sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sd.MaxLOD         = D3D11_FLOAT32_MAX;
        hr = device->CreateSamplerState(&sd, &scene.linearSampler);
        if (FAILED(hr)) { std::cerr << "[IBL] CreateSamplerState failed.\n"; return false; }
    }

    // -----------------------------------------------------------------------
    // Step 7 — Cull-none rasterizer state (same as pbr_mesh).
    // -----------------------------------------------------------------------
    {
        D3D11_RASTERIZER_DESC rd = {};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;   // See both sides of the sphere
        rd.DepthClipEnable = TRUE;
        hr = device->CreateRasterizerState(&rd, &scene.rastState);
        if (FAILED(hr)) { std::cerr << "[IBL] CreateRasterizerState failed.\n"; return false; }
    }

    scene.loaded = true;
    std::cout << "[D3D11Renderer] LoadScene('pbr_ibl') — OK.\n";
    return true;
}

// ===========================================================================
// DrawPBRIBLMesh — render the PBR + IBL sphere (M16)
// ===========================================================================

void D3D11Renderer::DrawPBRIBLMesh()
{
    if (!m_pbrIblScene.loaded || !m_context)
        return;

    using namespace engine::math;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Per-Frame Constant Buffer Update (PBR + IBL)
    // -----------------------------------------------------------------------
    // The per-frame CB holds the world, worldInvTrans, view, and proj matrices.
    // The sphere rotates slowly around the Y axis so students can see the
    // IBL ambient changing as the reflection vector R sweeps across the
    // prefiltered environment cubemap.
    // -----------------------------------------------------------------------
    float angle  = m_sceneTime * 0.4f;  // 0.4 rad/s rotation
    Mat4 worldMat = Mat4::Rotation(Quat::FromAxisAngle(Vec3::Up(), angle));

    Vec3 eye    = { 0.0f, 0.5f, 4.0f };
    Vec3 target = { 0.0f, 0.0f, 0.0f };
    Vec3 upDir  = { 0.0f, 1.0f, 0.0f };

    Vec3 zAxis = (eye - target).Normalized();
    Vec3 xAxis = upDir.Cross(zAxis).Normalized();
    Vec3 yAxis = zAxis.Cross(xAxis);

    Mat4 viewMat;
    viewMat.m[0][0] = xAxis.x; viewMat.m[0][1] = yAxis.x; viewMat.m[0][2] = zAxis.x; viewMat.m[0][3] = 0;
    viewMat.m[1][0] = xAxis.y; viewMat.m[1][1] = yAxis.y; viewMat.m[1][2] = zAxis.y; viewMat.m[1][3] = 0;
    viewMat.m[2][0] = xAxis.z; viewMat.m[2][1] = yAxis.z; viewMat.m[2][2] = zAxis.z; viewMat.m[2][3] = 0;
    viewMat.m[3][0] = -xAxis.Dot(eye); viewMat.m[3][1] = -yAxis.Dot(eye);
    viewMat.m[3][2] = -zAxis.Dot(eye); viewMat.m[3][3] = 1;

    const float kFovY  = 3.14159265f / 3.0f;
    const float kNear  = 0.1f, kFar = 100.0f;
    float aspect = (m_height > 0) ?
        (static_cast<float>(m_width) / static_cast<float>(m_height)) : 1.0f;
    float f = 1.0f / std::tan(kFovY * 0.5f);
    Mat4 projMat;
    projMat.m[0][0] = f / aspect;
    projMat.m[1][1] = f;
    projMat.m[2][2] = -kFar / (kFar - kNear);
    projMat.m[2][3] = -1.0f;
    projMat.m[3][2] = -(kNear * kFar) / (kFar - kNear);

    struct alignas(16) PerFrameData {
        float world[4][4];
        float worldInvTrans[4][4];
        float view[4][4];
        float proj[4][4];
    } pfd;
    std::memcpy(pfd.world,        worldMat.Data(), 64);
    std::memcpy(pfd.worldInvTrans, worldMat.Data(), 64);
    std::memcpy(pfd.view,         viewMat.Data(),  64);
    std::memcpy(pfd.proj,         projMat.Data(),  64);

    D3D11_MAPPED_SUBRESOURCE msr = {};
    if (SUCCEEDED(m_context->Map(m_pbrIblScene.perFrameCB, 0,
                                  D3D11_MAP_WRITE_DISCARD, 0, &msr)))
    {
        std::memcpy(msr.pData, &pfd, sizeof(pfd));
        m_context->Unmap(m_pbrIblScene.perFrameCB, 0);
    }

    // -----------------------------------------------------------------------
    // Bind pipeline.
    // -----------------------------------------------------------------------
    UINT stride = sizeof(float) * 8;
    UINT offset = 0;
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->IASetVertexBuffers(0, 1, &m_pbrIblScene.vertexBuf, &stride, &offset);
    m_context->IASetIndexBuffer(m_pbrIblScene.indexBuf, DXGI_FORMAT_R16_UINT, 0);
    m_context->IASetInputLayout(m_pbrIblScene.inputLayout);

    m_context->VSSetShader(m_pbrIblScene.vs, nullptr, 0);
    m_context->VSSetConstantBuffers(0, 1, &m_pbrIblScene.perFrameCB);

    m_context->PSSetShader(m_pbrIblScene.ps, nullptr, 0);
    m_context->PSSetConstantBuffers(1, 1, &m_pbrIblScene.lightCB);
    m_context->PSSetConstantBuffers(2, 1, &m_pbrIblScene.materialCB);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Binding IBL Textures
    // -----------------------------------------------------------------------
    // The PS declares:
    //   t0 — Texture2D g_brdfLut
    //   t1 — TextureCube g_irradianceCube
    //   t2 — TextureCube g_prefilteredEnv
    //   t3 — Texture2D g_albedoMap
    //   t4 — Texture2D g_normalMap
    //   t5 — Texture2D g_metallicRoughnessMap
    //   t6 — Texture2D g_aoMap
    //   s0 — SamplerState g_linearSampler
    //
    // PSSetShaderResources binds SRVs to texture slots (t0, t1, t2).
    // PSSetSamplers binds the sampler to slot s0.
    // Setting all three SRVs in one call is more efficient than three
    // individual calls (single API round-trip to the driver).
    // -----------------------------------------------------------------------
    ID3D11ShaderResourceView* srvs[7] = {
        m_pbrIblScene.brdfLutSRV,
        m_pbrIblScene.irradianceSRV,
        m_pbrIblScene.prefilteredSRV,
        m_pbrIblScene.albedoMap.IsLoaded() ? m_pbrIblScene.albedoMap.GetSRV() : m_pbrIblScene.albedoFallbackSRV,
        m_pbrIblScene.normalMap.IsLoaded() ? m_pbrIblScene.normalMap.GetSRV() : m_pbrIblScene.normalFallbackSRV,
        m_pbrIblScene.metallicRoughnessMap.IsLoaded() ? m_pbrIblScene.metallicRoughnessMap.GetSRV() : m_pbrIblScene.metallicRoughnessFallbackSRV,
        m_pbrIblScene.aoMap.IsLoaded() ? m_pbrIblScene.aoMap.GetSRV() : m_pbrIblScene.aoFallbackSRV,
    };
    m_context->PSSetShaderResources(0, 7, srvs);
    m_context->PSSetSamplers(0, 1, &m_pbrIblScene.linearSampler);

    m_context->RSSetState(m_pbrIblScene.rastState);
    m_context->DrawIndexed(static_cast<UINT>(m_pbrIblScene.indexCount), 0, 0);

    // Unbind SRVs to avoid debug layer "resource still bound" warnings.
    ID3D11ShaderResourceView* nullSRVs[7] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    m_context->PSSetShaderResources(0, 7, nullSRVs);
    m_context->RSSetState(nullptr);
}

// ===========================================================================
// LoadShadowScene — build the directional shadow map demo (M17)
// ===========================================================================
// TEACHING NOTE — Two-Pass Shadow Rendering Setup
// Shadow map rendering requires:
//   (a) A depth texture that the GPU can WRITE to (DepthStencilView).
//   (b) The same texture bound as an SRV that the PS can READ from.
// D3D11 enforces a constraint: a resource bound as a DSV cannot simultaneously
// be bound as an SRV.  Between the shadow pass and the lit pass we must
// OMSetRenderTargets(0, nullptr, nullptr) to unbind the DSV before binding the
// SRV to the pixel shader.  DrawShadowScene() handles this ordering.
//
// shadow.vs.hlsl   — depth-only VS: one matrix multiply, output SV_POSITION.
// shadow_lit.vs.hlsl — lit VS: outputs worldPos + worldNrm for PCF lookup.
// shadow_lit.ps.hlsl — lit PS: 3×3 PCF shadow comparison + Lambert diffuse.
// ===========================================================================

// CB layout for the shadow pass: just the light view-projection matrix.
struct alignas(16) ShadowCBData
{
    float lightViewProj[4][4];   // 64 bytes
};

// CB layout shared by the lit VS and lit PS.
struct alignas(16) ShadowLitCBData
{
    float world[4][4];           // 64 bytes
    float view[4][4];            // 64 bytes
    float proj[4][4];            // 64 bytes
    float lightViewProj[4][4];   // 64 bytes
    float lightDir[4];           // 16 bytes (w = unused padding)
    // Total: 272 bytes — multiple of 16 ✓
};

static bool LoadShadowScene(ID3D11Device*                device,
                             const std::string&           shaderDir,
                             D3D11Renderer::ShadowScene&  scene)
{
    namespace fs = std::filesystem;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Shadow Map Texture Format
    // -----------------------------------------------------------------------
    // DXGI_FORMAT_R32_TYPELESS lets us create BOTH a DSV (write-only depth)
    // and an SRV (read-only depth) from the same texture resource.
    // The DSV uses DXGI_FORMAT_D32_FLOAT.
    // The SRV uses DXGI_FORMAT_R32_FLOAT — the depth values are accessible
    // as the R channel, which the HLSL reads as Texture2D<float>.
    //
    // Why TYPELESS?  D3D11 requires the base texture format to be "compatible"
    // with both the DSV and SRV formats.  DXGI_FORMAT_D32_FLOAT itself cannot
    // be used as an SRV format; TYPELESS is the bridge.
    // -----------------------------------------------------------------------

    // Step 1 — Create the shadow map texture (typeless for DSV + SRV).
    {
        D3D11_TEXTURE2D_DESC td = {};
        td.Width            = 512;
        td.Height           = 512;
        td.MipLevels        = 1;
        td.ArraySize        = 1;
        td.Format           = DXGI_FORMAT_R32_TYPELESS;  // typeless = bindable as DSV and SRV
        td.SampleDesc.Count = 1;
        td.Usage            = D3D11_USAGE_DEFAULT;
        td.BindFlags        = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        HRESULT hr = device->CreateTexture2D(&td, nullptr, &scene.shadowTex);
        if (FAILED(hr)) {
            std::cerr << "[Shadow] Shadow map texture creation failed.\n";
            return false;
        }
    }

    // Step 2 — Create the DSV (depth-stencil view for the shadow pass).
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC dvd = {};
        dvd.Format        = DXGI_FORMAT_D32_FLOAT;
        dvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        HRESULT hr = device->CreateDepthStencilView(scene.shadowTex, &dvd, &scene.shadowDSV);
        if (FAILED(hr)) {
            std::cerr << "[Shadow] DSV creation failed.\n";
            scene.shadowTex->Release(); scene.shadowTex = nullptr;
            return false;
        }
    }

    // Step 3 — Create the SRV (shader resource view for the lit pass).
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
        srvd.Format                    = DXGI_FORMAT_R32_FLOAT;
        srvd.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvd.Texture2D.MipLevels       = 1;
        srvd.Texture2D.MostDetailedMip = 0;
        HRESULT hr = device->CreateShaderResourceView(scene.shadowTex, &srvd, &scene.shadowSRV);
        if (FAILED(hr)) {
            std::cerr << "[Shadow] SRV creation failed.\n";
            scene.shadowDSV->Release(); scene.shadowDSV = nullptr;
            scene.shadowTex->Release(); scene.shadowTex = nullptr;
            return false;
        }
    }

    // -----------------------------------------------------------------------
    // Step 4 — Compile shadow pass shaders.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Compile Helper Lambda (same pattern as other scenes)
    // Try the .hlsl file from shaderDir first; return failure if not found.
    // The shadow shaders have no embedded fallback string — they require the
    // .hlsl files to be present in the shader directory.
    // -----------------------------------------------------------------------
    auto compile = [&](const fs::path& path, const char* entry, const char* target) -> ID3DBlob*
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
        if (FAILED(hr)) {
            if (errors) {
                std::cerr << "[Shadow] HLSL compile error ("
                          << path.filename().string() << "): "
                          << static_cast<const char*>(errors->GetBufferPointer()) << "\n";
                errors->Release();
            } else {
                std::cerr << "[Shadow] Missing shader: " << path.string() << "\n";
            }
            return nullptr;
        }
        if (errors) errors->Release();
        return code;
    };

    // Shadow pass VS (depth-only — shadow.vs.hlsl).
    ID3DBlob* shadowVsBlob = compile(fs::path(shaderDir) / "shadow.vs.hlsl", "main", "vs_4_0");
    if (!shadowVsBlob) {
        scene.shadowSRV->Release(); scene.shadowSRV = nullptr;
        scene.shadowDSV->Release(); scene.shadowDSV = nullptr;
        scene.shadowTex->Release(); scene.shadowTex = nullptr;
        return false;
    }
    HRESULT hr = device->CreateVertexShader(shadowVsBlob->GetBufferPointer(),
                                            shadowVsBlob->GetBufferSize(),
                                            nullptr, &scene.shadowVS);
    if (FAILED(hr)) {
        shadowVsBlob->Release();
        scene.shadowSRV->Release(); scene.shadowSRV = nullptr;
        scene.shadowDSV->Release(); scene.shadowDSV = nullptr;
        scene.shadowTex->Release(); scene.shadowTex = nullptr;
        return false;
    }

    // Step 5 — Create the shared input layout (POSITION + NORMAL + TEXCOORD0).
    // TEACHING NOTE — Shared Input Layout for Shadow and Lit Passes
    // Both the shadow VS and the lit VS accept the same vertex layout
    // (position + normal + UV).  We create ONE input layout using the shadow
    // VS bytecode — the layout is validated against the VS input signature.
    // The lit pass can reuse the same ID3D11InputLayout object because the
    // input element descriptors match the lit VS signature too.
    {
        const D3D11_INPUT_ELEMENT_DESC kLayout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        hr = device->CreateInputLayout(kLayout, 3,
                                       shadowVsBlob->GetBufferPointer(),
                                       shadowVsBlob->GetBufferSize(),
                                       &scene.shadowLayout);
    }
    shadowVsBlob->Release();
    if (FAILED(hr)) {
        scene.shadowVS->Release(); scene.shadowVS = nullptr;
        scene.shadowSRV->Release(); scene.shadowSRV = nullptr;
        scene.shadowDSV->Release(); scene.shadowDSV = nullptr;
        scene.shadowTex->Release(); scene.shadowTex = nullptr;
        return false;
    }

    // Step 6 — Compile lit-pass shaders (shadow_lit.vs.hlsl + shadow_lit.ps.hlsl).
    ID3DBlob* litVsBlob = compile(fs::path(shaderDir) / "shadow_lit.vs.hlsl", "main", "vs_4_0");
    if (!litVsBlob) {
        scene.shadowLayout->Release(); scene.shadowLayout = nullptr;
        scene.shadowVS->Release();     scene.shadowVS     = nullptr;
        scene.shadowSRV->Release();    scene.shadowSRV    = nullptr;
        scene.shadowDSV->Release();    scene.shadowDSV    = nullptr;
        scene.shadowTex->Release();    scene.shadowTex    = nullptr;
        return false;
    }
    hr = device->CreateVertexShader(litVsBlob->GetBufferPointer(),
                                    litVsBlob->GetBufferSize(),
                                    nullptr, &scene.litVS);
    litVsBlob->Release();
    if (FAILED(hr)) {
        scene.shadowLayout->Release(); scene.shadowLayout = nullptr;
        scene.shadowVS->Release();     scene.shadowVS     = nullptr;
        scene.shadowSRV->Release();    scene.shadowSRV    = nullptr;
        scene.shadowDSV->Release();    scene.shadowDSV    = nullptr;
        scene.shadowTex->Release();    scene.shadowTex    = nullptr;
        return false;
    }

    ID3DBlob* litPsBlob = compile(fs::path(shaderDir) / "shadow_lit.ps.hlsl", "main", "ps_4_0");
    if (!litPsBlob) {
        scene.litVS->Release();        scene.litVS        = nullptr;
        scene.shadowLayout->Release(); scene.shadowLayout = nullptr;
        scene.shadowVS->Release();     scene.shadowVS     = nullptr;
        scene.shadowSRV->Release();    scene.shadowSRV    = nullptr;
        scene.shadowDSV->Release();    scene.shadowDSV    = nullptr;
        scene.shadowTex->Release();    scene.shadowTex    = nullptr;
        return false;
    }
    hr = device->CreatePixelShader(litPsBlob->GetBufferPointer(),
                                   litPsBlob->GetBufferSize(),
                                   nullptr, &scene.litPS);
    litPsBlob->Release();
    if (FAILED(hr)) {
        scene.litVS->Release();        scene.litVS        = nullptr;
        scene.shadowLayout->Release(); scene.shadowLayout = nullptr;
        scene.shadowVS->Release();     scene.shadowVS     = nullptr;
        scene.shadowSRV->Release();    scene.shadowSRV    = nullptr;
        scene.shadowDSV->Release();    scene.shadowDSV    = nullptr;
        scene.shadowTex->Release();    scene.shadowTex    = nullptr;
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 7 — Create constant buffers.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Shadow CB (64 bytes, b0 of shadow VS)
    // The shadow pass only needs the combined lightViewProj matrix (one float4x4
    // = 64 bytes).  We upload it once at the start of each shadow draw call.
    // DYNAMIC + MAP_WRITE_DISCARD lets the CPU write new data each frame.
    // -----------------------------------------------------------------------
    {
        D3D11_BUFFER_DESC cbd = {};
        cbd.ByteWidth      = sizeof(ShadowCBData);
        cbd.Usage          = D3D11_USAGE_DYNAMIC;
        cbd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = device->CreateBuffer(&cbd, nullptr, &scene.shadowCB);
        if (FAILED(hr)) {
            std::cerr << "[Shadow] shadowCB creation failed.\n";
            scene.litPS->Release();        scene.litPS        = nullptr;
            scene.litVS->Release();        scene.litVS        = nullptr;
            scene.shadowLayout->Release(); scene.shadowLayout = nullptr;
            scene.shadowVS->Release();     scene.shadowVS     = nullptr;
            scene.shadowSRV->Release();    scene.shadowSRV    = nullptr;
            scene.shadowDSV->Release();    scene.shadowDSV    = nullptr;
            scene.shadowTex->Release();    scene.shadowTex    = nullptr;
            return false;
        }
    }

    // TEACHING NOTE — Lit CB (272 bytes, b0 of lit VS + lit PS)
    // Contains: world, view, proj, lightViewProj, lightDir.
    // Both stages share the same CB object — we bind it to VS slot 0 and
    // PS slot 0 simultaneously in DrawShadowScene().
    {
        D3D11_BUFFER_DESC cbd = {};
        cbd.ByteWidth      = sizeof(ShadowLitCBData);
        cbd.Usage          = D3D11_USAGE_DYNAMIC;
        cbd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = device->CreateBuffer(&cbd, nullptr, &scene.litCB);
        if (FAILED(hr)) {
            std::cerr << "[Shadow] litCB creation failed.\n";
            scene.shadowCB->Release();     scene.shadowCB     = nullptr;
            scene.litPS->Release();        scene.litPS        = nullptr;
            scene.litVS->Release();        scene.litVS        = nullptr;
            scene.shadowLayout->Release(); scene.shadowLayout = nullptr;
            scene.shadowVS->Release();     scene.shadowVS     = nullptr;
            scene.shadowSRV->Release();    scene.shadowSRV    = nullptr;
            scene.shadowDSV->Release();    scene.shadowDSV    = nullptr;
            scene.shadowTex->Release();    scene.shadowTex    = nullptr;
            return false;
        }
    }

    // -----------------------------------------------------------------------
    // Step 8 — Create depth stencil state (depth test + write, no stencil).
    // -----------------------------------------------------------------------
    {
        D3D11_DEPTH_STENCIL_DESC dsd = {};
        dsd.DepthEnable    = TRUE;
        dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dsd.DepthFunc      = D3D11_COMPARISON_LESS;
        hr = device->CreateDepthStencilState(&dsd, &scene.shadowDSS);
        if (FAILED(hr)) {
            scene.litCB->Release();        scene.litCB        = nullptr;
            scene.shadowCB->Release();     scene.shadowCB     = nullptr;
            scene.litPS->Release();        scene.litPS        = nullptr;
            scene.litVS->Release();        scene.litVS        = nullptr;
            scene.shadowLayout->Release(); scene.shadowLayout = nullptr;
            scene.shadowVS->Release();     scene.shadowVS     = nullptr;
            scene.shadowSRV->Release();    scene.shadowSRV    = nullptr;
            scene.shadowDSV->Release();    scene.shadowDSV    = nullptr;
            scene.shadowTex->Release();    scene.shadowTex    = nullptr;
            return false;
        }
    }

    // -----------------------------------------------------------------------
    // Step 9 — Create rasterizer state for the shadow pass.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Depth Bias for Shadow Maps
    // Shadow acne arises because the shadow map depth and the re-computed
    // surface depth differ by a tiny floating-point error.  The rasterizer
    // depth-bias feature offsets depth values written to the shadow map:
    //
    //   DepthBias            — constant offset (added after depth clamp)
    //   SlopeScaledDepthBias — scales with the surface slope (higher = more bias
    //                          on steep surfaces where acne is worst)
    //
    // Values are highly application-specific.  For a normalised D32_FLOAT map
    // the values below (bias=100, slope=1.5) are a reasonable starting point.
    //
    // CullMode = BACK: standard back-face culling is used during the shadow
    // pass.  Front-face culling (CULL_FRONT) is an alternative that can reduce
    // "peter-panning" on thick objects, but requires closed meshes and is not
    // used here to keep the demo geometry requirements minimal.
    // -----------------------------------------------------------------------
    {
        D3D11_RASTERIZER_DESC rd = {};
        rd.FillMode              = D3D11_FILL_SOLID;
        rd.CullMode              = D3D11_CULL_BACK;
        rd.FrontCounterClockwise = FALSE;
        rd.DepthBias             = 100;
        rd.DepthBiasClamp        = 0.0f;
        rd.SlopeScaledDepthBias  = 1.5f;
        rd.DepthClipEnable       = TRUE;
        hr = device->CreateRasterizerState(&rd, &scene.shadowRast);
        if (FAILED(hr)) {
            scene.shadowDSS->Release();    scene.shadowDSS    = nullptr;
            scene.litCB->Release();        scene.litCB        = nullptr;
            scene.shadowCB->Release();     scene.shadowCB     = nullptr;
            scene.litPS->Release();        scene.litPS        = nullptr;
            scene.litVS->Release();        scene.litVS        = nullptr;
            scene.shadowLayout->Release(); scene.shadowLayout = nullptr;
            scene.shadowVS->Release();     scene.shadowVS     = nullptr;
            scene.shadowSRV->Release();    scene.shadowSRV    = nullptr;
            scene.shadowDSV->Release();    scene.shadowDSV    = nullptr;
            scene.shadowTex->Release();    scene.shadowTex    = nullptr;
            return false;
        }
    }

    // -----------------------------------------------------------------------
    // Step 10 — Create the comparison sampler for PCF shadow lookup.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — SamplerComparisonState (hardware PCF)
    // D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT enables bilinear PCF:
    //   • Takes a 2×2 neighbourhood, compares each texel, and bilinearly
    //     blends the 4 binary results.  This gives soft-edged shadows.
    //
    // D3D11_COMPARISON_LESS_EQUAL: the comparison passes (→ lit, value 1.0)
    // when the stored depth value is GREATER THAN OR EQUAL TO the reference.
    // In D3D11 shadow maps:
    //   stored value = depth of nearest occluder from the light.
    //   reference    = depth of current surface from the light (minus bias).
    // If stored ≥ reference → surface is CLOSER to the light than occluder → LIT.
    // If stored < reference → surface is DEEPER than occluder → IN SHADOW.
    // -----------------------------------------------------------------------
    {
        D3D11_SAMPLER_DESC sd = {};
        sd.Filter         = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        sd.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        sd.MaxLOD         = D3D11_FLOAT32_MAX;
        hr = device->CreateSamplerState(&sd, &scene.cmpSampler);
        if (FAILED(hr)) {
            scene.shadowRast->Release();   scene.shadowRast   = nullptr;
            scene.shadowDSS->Release();    scene.shadowDSS    = nullptr;
            scene.litCB->Release();        scene.litCB        = nullptr;
            scene.shadowCB->Release();     scene.shadowCB     = nullptr;
            scene.litPS->Release();        scene.litPS        = nullptr;
            scene.litVS->Release();        scene.litVS        = nullptr;
            scene.shadowLayout->Release(); scene.shadowLayout = nullptr;
            scene.shadowVS->Release();     scene.shadowVS     = nullptr;
            scene.shadowSRV->Release();    scene.shadowSRV    = nullptr;
            scene.shadowDSV->Release();    scene.shadowDSV    = nullptr;
            scene.shadowTex->Release();    scene.shadowTex    = nullptr;
            return false;
        }
    }

    // -----------------------------------------------------------------------
    // Step 11 — Generate UV sphere geometry (16 stacks × 16 slices).
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Reuse the UV Sphere for the Shadow Demo
    // The same sphere geometry used in the PBR scenes (M9, M16) serves as
    // both the shadow caster and the lit object in this demo.  Using a sphere
    // keeps the focus on the shadow algorithm rather than asset loading.
    // The vertex format (pos + normal + uv) matches the shadow VS input layout.
    // -----------------------------------------------------------------------
    {
        constexpr int NStacks = 16;
        constexpr int NSlices = 16;
        struct Vtx { float pos[3]; float nrm[3]; float uv[2]; };
        const int nVerts   = (NStacks + 1) * (NSlices + 1);
        const int nIndices = NStacks * NSlices * 6;
        std::vector<Vtx>      verts(static_cast<size_t>(nVerts));
        std::vector<uint16_t> idx(static_cast<size_t>(nIndices));

        for (int st = 0; st <= NStacks; ++st)
        {
            float phi = kPi * static_cast<float>(st) / static_cast<float>(NStacks);
            float y   = std::cos(phi);
            float r   = std::sin(phi);
            for (int sl = 0; sl <= NSlices; ++sl)
            {
                float theta = 2.0f * kPi * static_cast<float>(sl) / static_cast<float>(NSlices);
                float x     = r * std::cos(theta);
                float z     = r * std::sin(theta);
                int   i     = st * (NSlices + 1) + sl;
                verts[i] = { {x, y, z}, {x, y, z},
                             { static_cast<float>(sl) / NSlices,
                               static_cast<float>(st) / NStacks } };
            }
        }
        int ii = 0;
        for (int st = 0; st < NStacks; ++st)
        {
            for (int sl = 0; sl < NSlices; ++sl)
            {
                auto v0 = static_cast<uint16_t>( st      * (NSlices + 1) + sl);
                auto v1 = static_cast<uint16_t>( st      * (NSlices + 1) + (sl + 1));
                auto v2 = static_cast<uint16_t>((st + 1) * (NSlices + 1) + sl);
                auto v3 = static_cast<uint16_t>((st + 1) * (NSlices + 1) + (sl + 1));
                idx[ii++] = v0; idx[ii++] = v2; idx[ii++] = v1;
                idx[ii++] = v1; idx[ii++] = v2; idx[ii++] = v3;
            }
        }
        scene.indexCount = nIndices;

        D3D11_BUFFER_DESC vbd = {};
        vbd.ByteWidth  = static_cast<UINT>(nVerts * sizeof(Vtx));
        vbd.Usage      = D3D11_USAGE_IMMUTABLE;
        vbd.BindFlags  = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA vsd = {};
        vsd.pSysMem = verts.data();
        hr = device->CreateBuffer(&vbd, &vsd, &scene.vertexBuf);
        if (FAILED(hr)) {
            scene.cmpSampler->Release();   scene.cmpSampler   = nullptr;
            scene.shadowRast->Release();   scene.shadowRast   = nullptr;
            scene.shadowDSS->Release();    scene.shadowDSS    = nullptr;
            scene.litCB->Release();        scene.litCB        = nullptr;
            scene.shadowCB->Release();     scene.shadowCB     = nullptr;
            scene.litPS->Release();        scene.litPS        = nullptr;
            scene.litVS->Release();        scene.litVS        = nullptr;
            scene.shadowLayout->Release(); scene.shadowLayout = nullptr;
            scene.shadowVS->Release();     scene.shadowVS     = nullptr;
            scene.shadowSRV->Release();    scene.shadowSRV    = nullptr;
            scene.shadowDSV->Release();    scene.shadowDSV    = nullptr;
            scene.shadowTex->Release();    scene.shadowTex    = nullptr;
            return false;
        }

        D3D11_BUFFER_DESC ibd = {};
        ibd.ByteWidth  = static_cast<UINT>(nIndices * sizeof(uint16_t));
        ibd.Usage      = D3D11_USAGE_IMMUTABLE;
        ibd.BindFlags  = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA isd = {};
        isd.pSysMem = idx.data();
        hr = device->CreateBuffer(&ibd, &isd, &scene.indexBuf);
        if (FAILED(hr)) {
            scene.vertexBuf->Release();    scene.vertexBuf    = nullptr;
            scene.cmpSampler->Release();   scene.cmpSampler   = nullptr;
            scene.shadowRast->Release();   scene.shadowRast   = nullptr;
            scene.shadowDSS->Release();    scene.shadowDSS    = nullptr;
            scene.litCB->Release();        scene.litCB        = nullptr;
            scene.shadowCB->Release();     scene.shadowCB     = nullptr;
            scene.litPS->Release();        scene.litPS        = nullptr;
            scene.litVS->Release();        scene.litVS        = nullptr;
            scene.shadowLayout->Release(); scene.shadowLayout = nullptr;
            scene.shadowVS->Release();     scene.shadowVS     = nullptr;
            scene.shadowSRV->Release();    scene.shadowSRV    = nullptr;
            scene.shadowDSV->Release();    scene.shadowDSV    = nullptr;
            scene.shadowTex->Release();    scene.shadowTex    = nullptr;
            return false;
        }
    }

    scene.loaded = true;
    return true;
}

// ===========================================================================
// DrawShadowScene — execute both shadow pass and lit pass (M17)
// ===========================================================================

void D3D11Renderer::DrawShadowScene()
{
    if (!m_shadowScene.loaded || !m_context)
        return;

    using namespace engine::math;

    // Orthographic light camera: the "sun" comes from above-and-to-the-side.
    // Light direction (towards the scene origin) = (-0.5, -1, -0.5) normalised.
    const float kLightDirX = -0.4082f;  // normalise of (-0.5,-1,-0.5)
    const float kLightDirY = -0.8165f;
    const float kLightDirZ = -0.4082f;

    // Build orthographic light view-projection matrix.
    // TEACHING NOTE — Orthographic Projection for Directional Lights
    // A directional light has parallel rays (infinite distance), so it uses
    // an ORTHOGRAPHIC projection — no perspective foreshortening.
    // The view volume is a box: width=6, height=6, depth=10.
    // LookAt: eye = -lightDir * 5 (5 units back from scene), target = origin.
    const float eyeX = -kLightDirX * 5.0f;
    const float eyeY = -kLightDirY * 5.0f;
    const float eyeZ = -kLightDirZ * 5.0f;

    // LookAt (RH convention, row-major):
    Vec3 zAxis = Vec3{ kLightDirX, kLightDirY, kLightDirZ }.Normalized();  // eye→target normalised
    Vec3 up    = (std::abs(kLightDirY) > 0.9f)
                     ? Vec3{ 1.0f, 0.0f, 0.0f }
                     : Vec3{ 0.0f, 1.0f, 0.0f };
    Vec3 xAxis = up.Cross(zAxis).Normalized();
    Vec3 yAxis = zAxis.Cross(xAxis);

    Mat4 lightView;
    lightView.m[0][0] = xAxis.x;           lightView.m[0][1] = yAxis.x;           lightView.m[0][2] = zAxis.x;           lightView.m[0][3] = 0;
    lightView.m[1][0] = xAxis.y;           lightView.m[1][1] = yAxis.y;           lightView.m[1][2] = zAxis.y;           lightView.m[1][3] = 0;
    lightView.m[2][0] = xAxis.z;           lightView.m[2][1] = yAxis.z;           lightView.m[2][2] = zAxis.z;           lightView.m[2][3] = 0;
    lightView.m[3][0] = -(xAxis.x*eyeX + xAxis.y*eyeY + xAxis.z*eyeZ);
    lightView.m[3][1] = -(yAxis.x*eyeX + yAxis.y*eyeY + yAxis.z*eyeZ);
    lightView.m[3][2] = -(zAxis.x*eyeX + zAxis.y*eyeY + zAxis.z*eyeZ);
    lightView.m[3][3] = 1;

    // Orthographic projection: box [-3,+3] × [-3,+3] × [0.1, 10]
    const float kW = 3.0f, kH = 3.0f, kN = 0.1f, kF = 10.0f;
    Mat4 lightProj;
    lightProj.m[0][0] = 1.0f / kW;
    lightProj.m[1][1] = 1.0f / kH;
    lightProj.m[2][2] = -1.0f / (kF - kN);
    lightProj.m[3][2] = -kN / (kF - kN);
    lightProj.m[3][3] = 1.0f;

    // Combined light matrix (lightView × lightProj = row-major multiply).
    // TEACHING NOTE — Row-Major Matrix Multiply
    // In row-vector × matrix convention, the chain is:
    //   clipPos = pos_model × world × lightView × lightProj
    // For the shadow pass world is identity (sphere centred at origin).
    // We precompute lightView × lightProj on the CPU to save the VS multiply.
    Mat4 lightVP;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
        {
            lightVP.m[r][c] = 0.0f;
            for (int k = 0; k < 4; ++k)
                lightVP.m[r][c] += lightView.m[r][k] * lightProj.m[k][c];
        }

    // -----------------------------------------------------------------------
    // SHADOW PASS — render sphere depth from the light's viewpoint.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — No Colour Output
    // We bind the shadow DSV but NO colour RTV (first argument = 0, second =
    // nullptr).  The rasteriser will still write depth — only colour output
    // is suppressed.  Omitting the colour RTV halves the memory bandwidth
    // compared to a dummy colour target.
    // -----------------------------------------------------------------------

    // Save the currently-bound RTV and DSV so we can restore them for the lit pass.
    // TEACHING NOTE — OMGetRenderTargets increments the COM ref count of the
    // returned pointers.  We MUST call Release() on them after restoring.
    ID3D11RenderTargetView* prevRTV = nullptr;
    ID3D11DepthStencilView* prevDSV = nullptr;
    m_context->OMGetRenderTargets(1, &prevRTV, &prevDSV);

    uint32_t numVP = 1;
    D3D11_VIEWPORT prevVP = {};
    m_context->RSGetViewports(&numVP, &prevVP);

    // Bind only the shadow DSV (no colour RTV).
    m_context->OMSetRenderTargets(0, nullptr, m_shadowScene.shadowDSV);
    m_context->OMSetDepthStencilState(m_shadowScene.shadowDSS, 0);
    m_context->ClearDepthStencilView(m_shadowScene.shadowDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

    D3D11_VIEWPORT shadowVP = {};
    shadowVP.Width    = 512.0f;
    shadowVP.Height   = 512.0f;
    shadowVP.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &shadowVP);
    m_context->RSSetState(m_shadowScene.shadowRast);

    // Upload lightViewProj to the shadow CB.
    {
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (SUCCEEDED(m_context->Map(m_shadowScene.shadowCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            auto* cbData = static_cast<ShadowCBData*>(mapped.pData);
            std::memcpy(cbData->lightViewProj, lightVP.Data(), 64);
            m_context->Unmap(m_shadowScene.shadowCB, 0);
        }
    }

    // Set shadow pass pipeline state.
    m_context->VSSetShader(m_shadowScene.shadowVS, nullptr, 0);
    m_context->PSSetShader(nullptr, nullptr, 0);   // no PS needed for depth-only
    m_context->VSSetConstantBuffers(0, 1, &m_shadowScene.shadowCB);
    m_context->IASetInputLayout(m_shadowScene.shadowLayout);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    UINT stride = 8 * sizeof(float);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, &m_shadowScene.vertexBuf, &stride, &offset);
    m_context->IASetIndexBuffer(m_shadowScene.indexBuf, DXGI_FORMAT_R16_UINT, 0);
    m_context->DrawIndexed(static_cast<UINT>(m_shadowScene.indexCount), 0, 0);

    // Unbind the shadow DSV before binding the shadow SRV.
    // TEACHING NOTE — DSV ↔ SRV Mutual Exclusion
    // D3D11 does not allow the same sub-resource to be bound simultaneously
    // as a DSV (write) and an SRV (read).  We must unbind the DSV first by
    // restoring the previous render target, then bind the SRV for the lit pass.
    m_context->OMSetRenderTargets(1, &prevRTV, prevDSV);
    m_context->RSSetViewports(1, &prevVP);
    m_context->OMSetDepthStencilState(nullptr, 0);
    m_context->RSSetState(nullptr);

    // -----------------------------------------------------------------------
    // LIT PASS — render sphere from camera's view, sampling the shadow map.
    // -----------------------------------------------------------------------
    // Camera: eye at (0, 0.5, 4), looking at origin (same as PBR scene).
    Vec3 eye    = { 0.0f, 0.5f, 4.0f };
    Vec3 target = { 0.0f, 0.0f, 0.0f };
    Vec3 camUp  = { 0.0f, 1.0f, 0.0f };
    Vec3 camZ   = (eye - target).Normalized();
    Vec3 camX   = camUp.Cross(camZ).Normalized();
    Vec3 camY   = camZ.Cross(camX);

    Mat4 viewMat;
    viewMat.m[0][0] = camX.x;           viewMat.m[0][1] = camY.x;           viewMat.m[0][2] = camZ.x;           viewMat.m[0][3] = 0;
    viewMat.m[1][0] = camX.y;           viewMat.m[1][1] = camY.y;           viewMat.m[1][2] = camZ.y;           viewMat.m[1][3] = 0;
    viewMat.m[2][0] = camX.z;           viewMat.m[2][1] = camY.z;           viewMat.m[2][2] = camZ.z;           viewMat.m[2][3] = 0;
    viewMat.m[3][0] = -camX.Dot(eye);   viewMat.m[3][1] = -camY.Dot(eye);   viewMat.m[3][2] = -camZ.Dot(eye);   viewMat.m[3][3] = 1;

    const float kFovY = 3.14159265f / 3.0f;
    const float kNear = 0.1f;
    const float kFar  = 100.0f;
    float aspect = (prevVP.Height > 0.0f) ? (prevVP.Width / prevVP.Height) : 1.0f;
    float f      = 1.0f / std::tan(kFovY * 0.5f);
    Mat4 projMat;
    projMat.m[0][0] = f / aspect;
    projMat.m[1][1] = f;
    projMat.m[2][2] = -kFar / (kFar - kNear);
    projMat.m[2][3] = -1.0f;
    projMat.m[3][2] = -(kNear * kFar) / (kFar - kNear);

    // Sphere rotates slowly so shadows visibly change over time.
    float angle = m_sceneTime * 0.5f;
    Mat4 worldMat = Mat4::Rotation(Quat::FromAxisAngle(Vec3::Up(), angle));

    // Upload the lit CB.
    {
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (SUCCEEDED(m_context->Map(m_shadowScene.litCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            auto* cbData = static_cast<ShadowLitCBData*>(mapped.pData);
            std::memcpy(cbData->world,         worldMat.Data(), 64);
            std::memcpy(cbData->view,          viewMat.Data(),  64);
            std::memcpy(cbData->proj,          projMat.Data(),  64);
            std::memcpy(cbData->lightViewProj, lightVP.Data(),  64);
            cbData->lightDir[0] = kLightDirX;
            cbData->lightDir[1] = kLightDirY;
            cbData->lightDir[2] = kLightDirZ;
            cbData->lightDir[3] = 0.0f;
            m_context->Unmap(m_shadowScene.litCB, 0);
        }
    }

    m_context->VSSetShader(m_shadowScene.litVS, nullptr, 0);
    m_context->PSSetShader(m_shadowScene.litPS,  nullptr, 0);
    m_context->VSSetConstantBuffers(0, 1, &m_shadowScene.litCB);
    m_context->PSSetConstantBuffers(0, 1, &m_shadowScene.litCB);
    m_context->PSSetShaderResources(0, 1, &m_shadowScene.shadowSRV);
    m_context->PSSetSamplers(0, 1, &m_shadowScene.cmpSampler);

    m_context->DrawIndexed(static_cast<UINT>(m_shadowScene.indexCount), 0, 0);

    // Unbind the shadow SRV to avoid D3D11 debug layer warnings.
    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_context->PSSetShaderResources(0, 1, &nullSRV);

    // Release the COM references that OMGetRenderTargets added.
    if (prevRTV) prevRTV->Release();
    if (prevDSV) prevDSV->Release();
}

// ===========================================================================
// LoadBloomScene — build the HDR bloom post-processing demo (M17)
// ===========================================================================
// TEACHING NOTE — Offscreen Render Target Pattern
// Bloom requires rendering to TEXTURES that are not the swap-chain back buffer.
// Each bloom RT follows the same three-object pattern:
//
//   ID3D11Texture2D          — GPU texture memory (RGBA8 UNORM, 256×256).
//   ID3D11RenderTargetView   — write handle: bind as RTV to draw into it.
//   ID3D11ShaderResourceView — read handle: bind as SRV to sample from it.
//
// Creating both RTV and SRV from the same texture is allowed because we never
// write (RTV) and read (SRV) the same texture at the same time — the pipeline
// always alternates: write to tex A → read from tex A (write next tex B).
// ===========================================================================

// Helper: create a 256×256 RGBA8 render target with RTV + SRV.
static bool CreateBloomRT(ID3D11Device*             device,
                           ID3D11Texture2D**         texOut,
                           ID3D11RenderTargetView**  rtvOut,
                           ID3D11ShaderResourceView** srvOut,
                           const char*               debugName)
{
    D3D11_TEXTURE2D_DESC td = {};
    td.Width            = D3D11Renderer::BloomScene::kRTSize;
    td.Height           = D3D11Renderer::BloomScene::kRTSize;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DEFAULT;
    td.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    HRESULT hr = device->CreateTexture2D(&td, nullptr, texOut);
    if (FAILED(hr)) { std::cerr << "[Bloom] " << debugName << " texture failed.\n"; return false; }

    hr = device->CreateRenderTargetView(*texOut, nullptr, rtvOut);
    if (FAILED(hr)) {
        std::cerr << "[Bloom] " << debugName << " RTV failed.\n";
        (*texOut)->Release(); *texOut = nullptr;
        return false;
    }

    hr = device->CreateShaderResourceView(*texOut, nullptr, srvOut);
    if (FAILED(hr)) {
        std::cerr << "[Bloom] " << debugName << " SRV failed.\n";
        (*rtvOut)->Release(); *rtvOut = nullptr;
        (*texOut)->Release(); *texOut = nullptr;
        return false;
    }
    return true;
}

struct alignas(16) BloomCBData  { float threshold;     float pad[3]; };
struct alignas(16) BlurCBData   { float dirX, dirY;    float texW, texH; };
struct alignas(16) CompCBData   { float bloomStrength; float pad[3]; };

static bool LoadBloomScene(ID3D11Device*              device,
                            const std::string&         shaderDir,
                            D3D11Renderer::BloomScene& scene)
{
    namespace fs = std::filesystem;

    // -----------------------------------------------------------------------
    // Step 1 — Create four offscreen render targets.
    // -----------------------------------------------------------------------
    if (!CreateBloomRT(device, &scene.sceneTex,  &scene.sceneRTV,  &scene.sceneSRV,  "scene"))  return false;
    if (!CreateBloomRT(device, &scene.brightTex, &scene.brightRTV, &scene.brightSRV, "bright")) {
        scene.sceneSRV->Release(); scene.sceneRTV->Release(); scene.sceneTex->Release();
        scene.sceneSRV = nullptr; scene.sceneRTV = nullptr; scene.sceneTex = nullptr;
        return false;
    }
    if (!CreateBloomRT(device, &scene.blurATex, &scene.blurARTV, &scene.blurASRV, "blurA")) {
        scene.brightSRV->Release(); scene.brightRTV->Release(); scene.brightTex->Release();
        scene.sceneSRV->Release();  scene.sceneRTV->Release();  scene.sceneTex->Release();
        scene.brightSRV = nullptr; scene.brightRTV = nullptr; scene.brightTex = nullptr;
        scene.sceneSRV  = nullptr; scene.sceneRTV  = nullptr; scene.sceneTex  = nullptr;
        return false;
    }
    if (!CreateBloomRT(device, &scene.blurBTex, &scene.blurBRTV, &scene.blurBSRV, "blurB")) {
        scene.blurASRV->Release();  scene.blurARTV->Release();  scene.blurATex->Release();
        scene.brightSRV->Release(); scene.brightRTV->Release(); scene.brightTex->Release();
        scene.sceneSRV->Release();  scene.sceneRTV->Release();  scene.sceneTex->Release();
        scene.blurASRV = nullptr; scene.blurARTV = nullptr; scene.blurATex = nullptr;
        scene.brightSRV = nullptr; scene.brightRTV = nullptr; scene.brightTex = nullptr;
        scene.sceneSRV  = nullptr; scene.sceneRTV  = nullptr; scene.sceneTex  = nullptr;
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 2 — Compile shaders.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Reusing sky.vs.hlsl as the Full-Screen VS
    // The full-screen triangle trick (SV_VertexID) generates 3 vertices that
    // cover the entire viewport without any vertex buffer.  sky.vs.hlsl already
    // implements this; we compile it again here for the bloom VS slot.
    // Compiling the same file twice is fine — each compile produces an
    // independent ID3D11VertexShader object with its own COM reference count.
    // -----------------------------------------------------------------------
    auto compile = [&](const fs::path& path, const char* entry, const char* target) -> ID3DBlob*
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
        if (FAILED(hr)) {
            if (errors) {
                std::cerr << "[Bloom] HLSL compile error ("
                          << path.filename().string() << "): "
                          << static_cast<const char*>(errors->GetBufferPointer()) << "\n";
                errors->Release();
            } else {
                std::cerr << "[Bloom] Missing shader: " << path.string() << "\n";
            }
            return nullptr;
        }
        if (errors) errors->Release();
        return code;
    };

    // Full-screen VS (reuse sky.vs.hlsl — SV_VertexID trick).
    ID3DBlob* vsBlob = compile(fs::path(shaderDir) / "sky.vs.hlsl", "main", "vs_4_0");
    if (!vsBlob) goto fail_after_rts;
    {
        HRESULT hr = device->CreateVertexShader(vsBlob->GetBufferPointer(),
                                                vsBlob->GetBufferSize(),
                                                nullptr, &scene.fullscreenVS);
        vsBlob->Release(); vsBlob = nullptr;
        if (FAILED(hr)) { std::cerr << "[Bloom] fullscreenVS creation failed.\n"; goto fail_after_rts; }
    }

    // Bright-pass PS.
    {
        ID3DBlob* psBlob = compile(fs::path(shaderDir) / "bloom_bright.ps.hlsl", "main", "ps_4_0");
        if (!psBlob) goto fail_after_fs_vs;
        HRESULT hr = device->CreatePixelShader(psBlob->GetBufferPointer(),
                                               psBlob->GetBufferSize(),
                                               nullptr, &scene.brightPS);
        psBlob->Release();
        if (FAILED(hr)) { std::cerr << "[Bloom] brightPS creation failed.\n"; goto fail_after_fs_vs; }
    }

    // Blur PS.
    {
        ID3DBlob* psBlob = compile(fs::path(shaderDir) / "bloom_blur.ps.hlsl", "main", "ps_4_0");
        if (!psBlob) goto fail_after_bright_ps;
        HRESULT hr = device->CreatePixelShader(psBlob->GetBufferPointer(),
                                               psBlob->GetBufferSize(),
                                               nullptr, &scene.blurPS);
        psBlob->Release();
        if (FAILED(hr)) { std::cerr << "[Bloom] blurPS creation failed.\n"; goto fail_after_bright_ps; }
    }

    // Composite PS.
    {
        ID3DBlob* psBlob = compile(fs::path(shaderDir) / "bloom_composite.ps.hlsl", "main", "ps_4_0");
        if (!psBlob) goto fail_after_blur_ps;
        HRESULT hr = device->CreatePixelShader(psBlob->GetBufferPointer(),
                                               psBlob->GetBufferSize(),
                                               nullptr, &scene.compositePS);
        psBlob->Release();
        if (FAILED(hr)) { std::cerr << "[Bloom] compositePS creation failed.\n"; goto fail_after_blur_ps; }
    }

    // -----------------------------------------------------------------------
    // Step 3 — Create constant buffers.
    // -----------------------------------------------------------------------
    {
        D3D11_BUFFER_DESC cbd = { sizeof(BloomCBData), D3D11_USAGE_DYNAMIC,
                                  D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
        HRESULT hr = device->CreateBuffer(&cbd, nullptr, &scene.bloomCB);
        if (FAILED(hr)) { std::cerr << "[Bloom] bloomCB failed.\n"; goto fail_after_composite_ps; }
    }
    {
        D3D11_BUFFER_DESC cbd = { sizeof(BlurCBData), D3D11_USAGE_DYNAMIC,
                                  D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
        HRESULT hr = device->CreateBuffer(&cbd, nullptr, &scene.blurCB);
        if (FAILED(hr)) { std::cerr << "[Bloom] blurCB failed.\n"; goto fail_after_bloom_cb; }
    }
    {
        D3D11_BUFFER_DESC cbd = { sizeof(CompCBData), D3D11_USAGE_DYNAMIC,
                                  D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
        HRESULT hr = device->CreateBuffer(&cbd, nullptr, &scene.compCB);
        if (FAILED(hr)) { std::cerr << "[Bloom] compCB failed.\n"; goto fail_after_blur_cb; }
    }

    // -----------------------------------------------------------------------
    // Step 4 — Create a linear clamp sampler for all bloom passes.
    // -----------------------------------------------------------------------
    {
        D3D11_SAMPLER_DESC sd = {};
        sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.MaxLOD   = D3D11_FLOAT32_MAX;
        HRESULT hr = device->CreateSamplerState(&sd, &scene.linearSampler);
        if (FAILED(hr)) { std::cerr << "[Bloom] linearSampler failed.\n"; goto fail_after_comp_cb; }
    }

    scene.loaded = true;
    return true;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Structured Cleanup via goto Labels
    // -----------------------------------------------------------------------
    // Cascaded cleanup with goto mimics the RAII pattern in systems where
    // constructors are not available (C-style APIs, COM).  Each label releases
    // only the resources created up to that point.  This avoids nested if/else
    // and keeps the happy path readable at the top of the function.
    // In production C++ code, RAII wrappers (ComPtr<T>, std::unique_ptr) are
    // preferred — goto is used here only for pedagogical clarity.
    // -----------------------------------------------------------------------
fail_after_comp_cb:
    scene.compCB->Release(); scene.compCB = nullptr;
fail_after_blur_cb:
    scene.blurCB->Release(); scene.blurCB = nullptr;
fail_after_bloom_cb:
    scene.bloomCB->Release(); scene.bloomCB = nullptr;
fail_after_composite_ps:
    scene.compositePS->Release(); scene.compositePS = nullptr;
fail_after_blur_ps:
    scene.blurPS->Release(); scene.blurPS = nullptr;
fail_after_bright_ps:
    scene.brightPS->Release(); scene.brightPS = nullptr;
fail_after_fs_vs:
    scene.fullscreenVS->Release(); scene.fullscreenVS = nullptr;
fail_after_rts:
    scene.blurBSRV->Release();  scene.blurBRTV->Release();  scene.blurBTex->Release();
    scene.blurASRV->Release();  scene.blurARTV->Release();  scene.blurATex->Release();
    scene.brightSRV->Release(); scene.brightRTV->Release(); scene.brightTex->Release();
    scene.sceneSRV->Release();  scene.sceneRTV->Release();  scene.sceneTex->Release();
    scene.blurBSRV = nullptr; scene.blurBRTV = nullptr; scene.blurBTex = nullptr;
    scene.blurASRV = nullptr; scene.blurARTV = nullptr; scene.blurATex = nullptr;
    scene.brightSRV = nullptr; scene.brightRTV = nullptr; scene.brightTex = nullptr;
    scene.sceneSRV  = nullptr; scene.sceneRTV  = nullptr; scene.sceneTex  = nullptr;
    return false;
}

// ===========================================================================
// DrawBloomScene — execute the four-pass bloom pipeline (M17)
// ===========================================================================

void D3D11Renderer::DrawBloomScene()
{
    if (!m_bloomScene.loaded || !m_context)
        return;

    // Save caller's RTV (we restore it for the final composite pass).
    ID3D11RenderTargetView* prevRTV = nullptr;
    ID3D11DepthStencilView* prevDSV = nullptr;
    m_context->OMGetRenderTargets(1, &prevRTV, &prevDSV);
    uint32_t numVP = 1;
    D3D11_VIEWPORT prevVP = {};
    m_context->RSGetViewports(&numVP, &prevVP);

    // All bloom passes use 256×256 viewport.
    D3D11_VIEWPORT bloomVP = {};
    bloomVP.Width    = static_cast<float>(D3D11Renderer::BloomScene::kRTSize);
    bloomVP.Height   = static_cast<float>(D3D11Renderer::BloomScene::kRTSize);
    bloomVP.MaxDepth = 1.0f;

    // No vertex buffer / input layout needed (SV_VertexID generates vertices).
    m_context->IASetInputLayout(nullptr);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_bloomScene.fullscreenVS, nullptr, 0);
    m_context->PSSetSamplers(0, 1, &m_bloomScene.linearSampler);
    m_context->OMSetDepthStencilState(nullptr, 0);

    // -----------------------------------------------------------------------
    // Step 1 — Fill the scene RT with a bright test colour (simulates scene).
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Simulated Scene Content For Bloom
    // For the bloom demo we bypass a full scene render and simply clear the
    // scene RT to a bright orange test colour stored in a UNORM render target.
    // In a production engine the scene RT is filled by the main render pass
    // (geometry + lighting) and the bloom pipeline then processes its output.
    // Using ClearRenderTargetView keeps the demo self-contained and focuses
    // attention on the bloom pipeline itself.
    //
    // This demo does not use a floating-point HDR render target: the colour
    // stays in the normal [0, 1] range of DXGI_FORMAT_R8G8B8A8_UNORM.
    // With the threshold at 0.7, this bright orange clear colour still
    // produces bright-pass output without needing true HDR values.
    // -----------------------------------------------------------------------
    {
        m_context->RSSetViewports(1, &bloomVP);
        float bright[] = { 1.0f, 0.85f, 0.2f, 1.0f };  // bright orange — channels exceed the bright-pass threshold
        m_context->OMSetRenderTargets(1, &m_bloomScene.sceneRTV, nullptr);
        m_context->ClearRenderTargetView(m_bloomScene.sceneRTV, bright);
    }

    // -----------------------------------------------------------------------
    // Step 2 — Bright-pass: extract pixels with luminance > threshold.
    // -----------------------------------------------------------------------
    {
        // Upload BloomCB: threshold = 0.7.
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (SUCCEEDED(m_context->Map(m_bloomScene.bloomCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            auto* cb   = static_cast<BloomCBData*>(mapped.pData);
            cb->threshold = 0.7f;
            cb->pad[0] = cb->pad[1] = cb->pad[2] = 0.0f;
            m_context->Unmap(m_bloomScene.bloomCB, 0);
        }
        m_context->OMSetRenderTargets(1, &m_bloomScene.brightRTV, nullptr);
        m_context->PSSetShader(m_bloomScene.brightPS, nullptr, 0);
        m_context->PSSetConstantBuffers(0, 1, &m_bloomScene.bloomCB);
        m_context->PSSetShaderResources(0, 1, &m_bloomScene.sceneSRV);
        m_context->Draw(3, 0);
    }

    // -----------------------------------------------------------------------
    // Step 3 — Horizontal Gaussian blur (brightRTV → blurARTV).
    // -----------------------------------------------------------------------
    {
        const float kTexel = 1.0f / static_cast<float>(D3D11Renderer::BloomScene::kRTSize);
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (SUCCEEDED(m_context->Map(m_bloomScene.blurCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            auto* cb   = static_cast<BlurCBData*>(mapped.pData);
            cb->dirX   = 1.0f; cb->dirY = 0.0f;  // horizontal
            cb->texW   = kTexel; cb->texH = kTexel;
            m_context->Unmap(m_bloomScene.blurCB, 0);
        }
        // Unbind brightSRV from the output and bind it as input.
        ID3D11ShaderResourceView* nullSRV = nullptr;
        m_context->PSSetShaderResources(0, 1, &nullSRV);
        m_context->OMSetRenderTargets(1, &m_bloomScene.blurARTV, nullptr);
        m_context->PSSetShader(m_bloomScene.blurPS, nullptr, 0);
        m_context->PSSetConstantBuffers(0, 1, &m_bloomScene.blurCB);
        m_context->PSSetShaderResources(0, 1, &m_bloomScene.brightSRV);
        m_context->Draw(3, 0);
    }

    // -----------------------------------------------------------------------
    // Step 4 — Vertical Gaussian blur (blurARTV → blurBRTV).
    // -----------------------------------------------------------------------
    {
        const float kTexel = 1.0f / static_cast<float>(D3D11Renderer::BloomScene::kRTSize);
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (SUCCEEDED(m_context->Map(m_bloomScene.blurCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            auto* cb   = static_cast<BlurCBData*>(mapped.pData);
            cb->dirX   = 0.0f; cb->dirY = 1.0f;  // vertical
            cb->texW   = kTexel; cb->texH = kTexel;
            m_context->Unmap(m_bloomScene.blurCB, 0);
        }
        ID3D11ShaderResourceView* nullSRV = nullptr;
        m_context->PSSetShaderResources(0, 1, &nullSRV);
        m_context->OMSetRenderTargets(1, &m_bloomScene.blurBRTV, nullptr);
        m_context->PSSetShader(m_bloomScene.blurPS, nullptr, 0);
        m_context->PSSetShaderResources(0, 1, &m_bloomScene.blurASRV);
        m_context->Draw(3, 0);
    }

    // -----------------------------------------------------------------------
    // Step 5 — Composite: sceneRTV + blurBRTV → caller's RTV (back buffer or offscreen).
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Restoring the Caller's Render Target
    // The final composite outputs to the render target that the CALLER set up
    // (either the swap-chain back buffer in windowed mode, or the 64×64 off-
    // screen RT in headless CI mode).  By restoring prevRTV here, DrawBloomScene
    // works transparently in both contexts without needing a "mode" parameter.
    {
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (SUCCEEDED(m_context->Map(m_bloomScene.compCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            auto* cb            = static_cast<CompCBData*>(mapped.pData);
            cb->bloomStrength   = 1.0f;
            cb->pad[0] = cb->pad[1] = cb->pad[2] = 0.0f;
            m_context->Unmap(m_bloomScene.compCB, 0);
        }
        ID3D11ShaderResourceView* nullSRV = nullptr;
        m_context->PSSetShaderResources(0, 1, &nullSRV);
        m_context->OMSetRenderTargets(1, &prevRTV, prevDSV);
        m_context->RSSetViewports(1, &prevVP);
        m_context->PSSetShader(m_bloomScene.compositePS, nullptr, 0);
        m_context->PSSetConstantBuffers(0, 1, &m_bloomScene.compCB);
        ID3D11ShaderResourceView* srvs[2] = { m_bloomScene.sceneSRV, m_bloomScene.blurBSRV };
        m_context->PSSetShaderResources(0, 2, srvs);
        m_context->Draw(3, 0);
    }

    // Unbind all SRVs.
    ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
    m_context->PSSetShaderResources(0, 2, nullSRVs);

    // Release OMGetRenderTargets COM references.
    if (prevRTV) prevRTV->Release();
    if (prevDSV) prevDSV->Release();
}

} // namespace rendering
} // namespace engine
