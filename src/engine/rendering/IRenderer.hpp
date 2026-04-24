/**
 * @file IRenderer.hpp
 * @brief Abstract renderer interface — backend-agnostic rendering API.
 *
 * ============================================================================
 * TEACHING NOTE — Renderer Abstraction (Strategy Pattern)
 * ============================================================================
 * This interface decouples the sandbox / game loop from the underlying
 * graphics API.  The concrete implementations are:
 *
 *   D3D11Renderer  — Direct3D 11 backend; GT610-class hardware baseline.
 *                    Default on Windows.  Uses WARP software rasteriser in
 *                    headless / CI mode for zero-driver-requirement validation.
 *
 *   VulkanRenderer — Vulkan 1.0+ backend; modern / high-end path.
 *                    Requires a Vulkan ICD on the machine.
 *
 * The factory in RendererFactory.hpp decides which concrete renderer to
 * construct based on a runtime flag (--renderer d3d11 | vulkan).
 *
 * ============================================================================
 * TEACHING NOTE — Why D3D11 as the Baseline?
 * ============================================================================
 * D3D11 with Feature Level 10_0 runs on GPUs released as far back as 2006
 * (GeForce 8 series, Radeon HD 2000).  A GeForce GT 610 — a common 2012 entry-
 * level card that is still in service — supports D3D11 Feature Level 11_0.
 * D3D11 ships with every Windows 7+ installation as part of the OS, so there
 * is zero setup for end users and CI runners alike.
 *
 * Vulkan is reserved for the high-end / modern path (M3+) and is built when
 * ENGINE_ENABLE_VULKAN=ON but is not the runtime default.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Target: Windows (MSVC)
 */

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>   // uint32_t
#include <string>    // std::string

namespace engine {
namespace rendering {

// ===========================================================================
// IRenderer — abstract renderer interface
// ===========================================================================
// TEACHING NOTE — Pure Virtual Interface
// Every method is pure virtual (= 0).  This means IRenderer cannot be
// instantiated directly.  Only concrete subclasses that override every method
// can be instantiated.  The virtual destructor ensures that deleting a
// D3D11Renderer or VulkanRenderer through an IRenderer pointer calls the
// right destructor chain.
// ===========================================================================
class IRenderer
{
public:
    virtual ~IRenderer() = default;

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    /**
     * @brief Initialise the renderer and create all GPU objects.
     *
     * Must be called once after the Win32 window exists.
     *
     * @param hinstance  Win32 HINSTANCE for surface creation.
     * @param hwnd       Win32 HWND for surface / swap chain creation.
     * @param width      Initial back-buffer width in pixels.
     * @param height     Initial back-buffer height in pixels.
     * @param headless   When true, skip swap-chain creation and use a
     *                   software/WARP device.  Allows headless CI runs.
     * @return true on success.
     */
    virtual bool Init(HINSTANCE hinstance, HWND hwnd,
                      uint32_t width, uint32_t height,
                      bool headless = false) = 0;

    /**
     * @brief Destroy all renderer objects.  Safe to call multiple times.
     */
    virtual void Shutdown() = 0;

    // -----------------------------------------------------------------------
    // Per-frame
    // -----------------------------------------------------------------------

    /**
     * @brief Render one frame: clear the back buffer and present.
     *
     * @param clearR  Red   channel (0.0–1.0).
     * @param clearG  Green channel (0.0–1.0).
     * @param clearB  Blue  channel (0.0–1.0).
     */
    virtual void DrawFrame(float clearR, float clearG, float clearB) = 0;

    // -----------------------------------------------------------------------
    // Window resize
    // -----------------------------------------------------------------------

    /**
     * @brief Recreate the swap chain (and any size-dependent resources) after
     *        a window resize.
     */
    virtual void RecreateSwapchain(uint32_t width, uint32_t height) = 0;

    // -----------------------------------------------------------------------
    // Scene management (Milestone M1+)
    // -----------------------------------------------------------------------

    /**
     * @brief Load a named scene (e.g. "triangle") and create its pipeline.
     *
     * @param sceneName  Logical scene identifier.
     * @param shaderDir  Absolute path to the directory containing compiled
     *                   shader files (*.spv for Vulkan, *.cso for D3D11).
     * @return true if the scene loaded successfully.
     */
    virtual bool LoadScene(const std::string& sceneName,
                           const std::string& shaderDir) = 0;

    // -----------------------------------------------------------------------
    // Headless / CI validation
    // -----------------------------------------------------------------------

    /**
     * @brief Record and validate one headless frame without presenting.
     *
     * Used by CI to confirm that the draw pipeline is functional without a
     * real display.
     *
     * @return true if the frame was recorded/validated successfully.
     */
    virtual bool RecordHeadlessFrame() = 0;

    // -----------------------------------------------------------------------
    // Query
    // -----------------------------------------------------------------------

    /**
     * @brief Return the human-readable name of this backend ("D3D11", "Vulkan").
     */
    virtual const char* BackendName() const = 0;

    // -----------------------------------------------------------------------
    // Feature toggles — Performance Preset System (M-DG-P1)
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Default No-Op Virtual Methods
    // ──────────────────────────────────────────────
    // These methods are intentionally NOT pure virtual.  Instead they provide
    // empty default implementations so that:
    //   1. VulkanRenderer (which has not yet implemented these features) still
    //      compiles without change — the Vulkan backend silently ignores the
    //      toggling until Vulkan feature parity is implemented (M14).
    //   2. New renderer backends (e.g., a future Metal backend) start with
    //      safe no-op behaviour.
    //
    // The D3D11Renderer overrides all of them to honour the preset.
    // -----------------------------------------------------------------------

    /**
     * @brief Enable or disable the directional shadow-map pass.
     *
     * When disabled the shadow-test scene renders without a shadow pass,
     * and the pbr_ibl scene ambient is computed without shadow occlusion.
     *
     * @param enabled  true  = shadows ON (default after Init).
     *                 false = shadows OFF (recommended for LOW preset).
     */
    virtual void SetShadowsEnabled(bool enabled) { (void)enabled; }

    /**
     * @brief Enable or disable the bloom post-processing pipeline.
     *
     * When disabled the bloom_test scene skips the bright-pass, Gaussian blur,
     * and composite passes — the back buffer receives only the base scene colour.
     *
     * @param enabled  true  = bloom ON (default after Init).
     *                 false = bloom OFF (recommended for LOW/MEDIUM preset).
     */
    virtual void SetBloomEnabled(bool enabled) { (void)enabled; }

    /**
     * @brief Enable or disable image-based lighting (IBL).
     *
     * When disabled the pbr_ibl scene falls back to a constant ambient term
     * instead of sampling the BRDF LUT, irradiance cubemap, and prefiltered
     * environment map.
     *
     * @param enabled  true  = IBL ON (default after Init).
     *                 false = IBL OFF (recommended for LOW/MEDIUM preset).
     */
    virtual void SetIBLEnabled(bool enabled) { (void)enabled; }

    /**
     * @brief Set the swap-chain sync interval (v-sync).
     *
     * @param enabled  true  = sync to VBlank, Present(1, 0) — steady 60 fps
     *                         on a 60 Hz monitor.  Prevents screen tearing.
     *                 false = present immediately, Present(0, 0) — maximum
     *                         frame rate; tearing may be visible.
     *
     * TEACHING NOTE — VSync and Frame Pacing
     * ─────────────────────────────────────────
     * V-sync reduces tearing but introduces "frame-drop" artefacts if the GPU
     * cannot render faster than the monitor refresh.  A common compromise is
     * "G-Sync / FreeSync": the monitor adapts its refresh rate to the GPU
     * frame rate.  We don't implement that here — VSync ON/OFF is the
     * classic two-state toggle used since the early 2000s.
     */
    virtual void SetVSyncEnabled(bool enabled) { (void)enabled; }

    /**
     * @brief Set a software frame-rate cap (sleep-based limiter).
     *
     * @param fps  Target frame rate cap.
     *             0 = uncapped (no sleep).
     *             30 = cap at 30 fps (LOW preset).
     *             60 = cap at 60 fps (MEDIUM/HIGH preset).
     *
     * TEACHING NOTE — Software vs Hardware Frame Cap
     * ─────────────────────────────────────────────────
     * Hardware VSync caps the frame rate at the monitor refresh frequency.
     * A software frame cap is a `Sleep()` call at the end of each frame that
     * burns any remaining budget.  The two can combine: v-sync ON + 30 fps cap
     * means "lock to 30 fps on a 60 Hz monitor" (half refresh rate).  This is
     * how console games achieve a rock-solid 30 fps without tearing.
     */
    virtual void SetFrameCap(int fps) { (void)fps; }

    /**
     * @brief Set the anisotropic filtering level for texture samplers.
     *
     * @param level  Max anisotropy: 1 (bilinear), 2, 4, 8, or 16.
     *               Values outside 1–16 are clamped to 1–16 by the backend.
     *
     * TEACHING NOTE — Anisotropic Filtering
     * ──────────────────────────────────────
     * Bilinear filtering looks blurry on surfaces viewed at a steep angle
     * (e.g., a floor receding into the distance).  Anisotropic filtering
     * samples the texture in an elongated "footprint" aligned with the angle
     * of view.  16× anisotropy is essentially indistinguishable from perfect
     * but is ~2× the memory bandwidth of 1× on modern GPUs.
     */
    virtual void SetAnisoLevel(int level) { (void)level; }
};

} // namespace rendering
} // namespace engine
