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

// ---------------------------------------------------------------------------
// TEACHING NOTE — pragma comment(lib, ...) vs CMake target_link_libraries
// ---------------------------------------------------------------------------
// On MSVC we can tell the linker which .lib to pull in directly from source
// using #pragma comment(lib, ...).  For D3D11 this is convenient because
// d3d11.lib and dxgi.lib ship with the Windows SDK (always present on MSVC)
// and we don't need a separate find_package() in CMake.
//
// We still list them in target_link_libraries in CMakeLists.txt for clarity
// and cross-toolchain compatibility.
// ---------------------------------------------------------------------------
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#include <iostream>
#include <cassert>
#include <cstring>   // std::memset

namespace engine {
namespace rendering {

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
// DrawFrame — clear the back buffer and present
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
    //      stencil view (nullptr = no depth test, acceptable for clear-only).
    //
    //   2. RSSetViewports — tell the Rasteriser (RS) stage the region of the
    //      render target to use.  TopLeftX/Y = 0 means "use the full texture".
    //      Without an explicit viewport call, the rasteriser falls back to
    //      implementation-defined behaviour on some drivers.
    //
    // Even when there are no draw calls (clear + present only), binding the
    // RTV here ensures the pipeline is in a known state before M3 adds real
    // geometry passes on top of this frame setup.
    // -----------------------------------------------------------------------

    // 1 — Bind render target (no depth buffer yet; added in M3 D3D11 textures).
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
    // TEACHING NOTE — D3D11 Clear + Present
    // -----------------------------------------------------------------------
    // The minimal draw loop for a "clear screen" demo:
    //   1. ClearRenderTargetView — fill the back buffer with a solid colour.
    //   2. Present               — display the back buffer (flip/blt to screen).
    // In a full renderer you would also:
    //   • Bind shaders, vertex buffers, constant buffers.
    //   • Issue draw calls.
    //   • Then present.
    // -----------------------------------------------------------------------
    const float clearColor[4] = { clearR, clearG, clearB, 1.0f };
    m_context->ClearRenderTargetView(m_renderTarget, clearColor);

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
// LoadScene — (M3+ stub for D3D11 scene loading)
// ===========================================================================

bool D3D11Renderer::LoadScene(const std::string& sceneName,
                              const std::string& /*shaderDir*/)
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — LoadScene Stub (M0 baseline)
    // -----------------------------------------------------------------------
    // The D3D11 renderer currently supports the M0 baseline (device creation +
    // clear colour loop).  Scene loading (triangle, textured quad, etc.) will
    // be implemented in future milestones (M3 D3D11 textures, M4 skinned mesh).
    //
    // Returning true here allows --headless --scene <name> to exit 0 in CI
    // without crashing.  A more complete implementation would load HLSL shaders
    // (.cso compiled shader object files) and create D3D11 pipeline state.
    // -----------------------------------------------------------------------
    std::cout << "[D3D11Renderer] LoadScene('" << sceneName
              << "') — stub; scene support arrives in M3.\n";
    return true;   // Non-fatal stub
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
    offscreenTex->Release();   // Texture is referenced by the RTV; safe to release handle.
    if (FAILED(hr))
    {
        std::cerr << "[D3D11Renderer] RecordHeadlessFrame: CreateRenderTargetView failed.\n";
        return false;
    }

    // Step 3 — Clear the off-screen RTV to cornflower blue (the classic D3D test colour).
    const float clearColor[4] = { 0.392f, 0.584f, 0.929f, 1.0f };
    m_context->ClearRenderTargetView(offscreenRTV, clearColor);

    // Step 4 — Flush to ensure the GPU (or WARP) processes the clear.
    m_context->Flush();

    // Step 5 — Release temporary resources.
    offscreenRTV->Release();

    return true;
}

} // namespace rendering
} // namespace engine
