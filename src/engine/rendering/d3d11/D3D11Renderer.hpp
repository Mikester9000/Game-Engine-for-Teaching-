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

// ---------------------------------------------------------------------------
// TEACHING NOTE — D3D11 / DXGI Headers
// ---------------------------------------------------------------------------
// These headers ship with the Windows SDK — no separate download needed.
// d3d11.h     — D3D11 device, context, resource types.
// dxgi.h      — DXGI swap chain, adapter, factory types.
// d3dcompiler.h — runtime HLSL compilation (used in headless validation).
// ---------------------------------------------------------------------------
#include <d3d11.h>
#include <dxgi.h>

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

    D3D_FEATURE_LEVEL       m_featureLevel  = D3D_FEATURE_LEVEL_10_0;

    bool                    m_headless      = false;
    bool                    m_initialised   = false;
};

} // namespace rendering
} // namespace engine
