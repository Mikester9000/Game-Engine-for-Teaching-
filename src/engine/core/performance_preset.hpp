/**
 * @file performance_preset.hpp
 * @brief Engine-wide performance preset definitions — M-DG-P1.
 *
 * ============================================================================
 * TEACHING NOTE — Performance Presets (Scalability Settings)
 * ============================================================================
 * Commercial games ship on a huge range of hardware: from Intel HD 4000 IGPs
 * in 2012 laptops to RTX 4090s in 2024 desktops.  The same binary must run
 * acceptably on all of them.
 *
 * The standard industry solution is a PRESET SYSTEM:
 *   • LOW    — every expensive feature OFF.  30 fps is acceptable.
 *   • MEDIUM — balanced quality/performance.  60 fps target.
 *   • HIGH   — full features at 60 fps on a mid-range GPU (GTX 1060 / RX 580).
 *   • ULTRA  — everything ON, uncapped frame rate for high-end machines.
 *
 * Examples:
 *   • Final Fantasy XV PC — uses Quality / Lite / Standard / High-Quality presets.
 *   • Cyberpunk 2077      — Low / Medium / High / Ultra / Ray Tracing tiers.
 *   • Unreal Engine       — Scalability settings r.Shadow.Quality 0–3, etc.
 *
 * ─── How Presets Work in This Engine ────────────────────────────────────────
 *
 *   1. EngineConfig holds the active preset (activePreset) plus the effective
 *      per-feature config (presetConfig).
 *   2. EngineConfig::ApplyPreset(p) fills presetConfig from PresetDefaults(p).
 *      Individual fields can be overridden in the JSON config after the fact.
 *   3. At startup, demo_main.cpp calls the renderer's Set*() methods to
 *      enforce the config.  IRenderer::Set*() methods are no-ops by default
 *      so the Vulkan backend silently ignores toggles it does not support yet.
 *
 * ─── Toggle Descriptions ─────────────────────────────────────────────────────
 *
 *   shadowsEnabled — directional shadow map pass (512×512 depth, PCF 3×3).
 *                    OFF saves the most GPU time on shadow-heavy scenes.
 *   bloomEnabled   — post-process bloom pipeline (bright-pass + Gaussian blur
 *                    + composite).  OFF removes 4 full-screen passes per frame.
 *   iblEnabled     — image-based lighting (BRDF LUT, irradiance cubemap,
 *                    prefiltered env cubemap).  OFF falls back to a simple
 *                    constant ambient term.
 *   vsync          — swap-chain sync interval.  OFF allows frames beyond
 *                    monitor refresh rate (useful for benchmarking).
 *   frameCap       — software sleep-based frame limiter in ms.
 *                    0 = unlimited, 30 = 30 fps cap, 60 = 60 fps cap.
 *   anisoLevel     — anisotropic filter max level for texture samplers.
 *                    1 = bilinear (no aniso), up to 16× for max quality.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2026
 * C++ Standard: C++17
 */

#pragma once

#include <cstdint>

namespace engine::core {

// ===========================================================================
// PerformancePreset — named quality tier
// ===========================================================================

/**
 * @enum PerformancePreset
 * @brief Named rendering quality tier for the engine performance preset system.
 *
 * TEACHING NOTE — Why Enum over Raw Integers?
 * ─────────────────────────────────────────────
 * Using an enum prevents passing the integer `3` when you mean LOW (0).
 * The compiler rejects invalid preset values at compile time.  The `uint8_t`
 * underlying type keeps the value 1 byte, which matters when persisted in the
 * EngineConfig JSON or transmitted in a network packet.
 */
enum class PerformancePreset : uint8_t
{
    LOW    = 0, ///< Minimal features — targets Intel HD 4000 / GT 610 at 30 fps.
    MEDIUM = 1, ///< Balanced — targets GT 1030 / RX 460 at 60 fps.
    HIGH   = 2, ///< Full features — targets GTX 1060 / RX 580 at 60 fps.
    ULTRA  = 3, ///< Maximum quality, uncapped — targets RTX 2060+ / RX 6700+.
};

// ===========================================================================
// PerformancePresetConfig — per-feature toggle set
// ===========================================================================

/**
 * @struct PerformancePresetConfig
 * @brief The effective set of feature toggles derived from a PerformancePreset.
 *
 * All fields have sensible HIGH-quality defaults so that an uninitialised
 * config does not accidentally disable features.
 *
 * TEACHING NOTE — Default Member Initializers (C++11+)
 * ──────────────────────────────────────────────────────
 * Initializing struct members at declaration (= true, = 60, etc.) means a
 * zero-argument constructor is not needed — the compiler generates a default
 * constructor that sets all fields to these values.  This reduces boilerplate
 * and makes the intended "safe default" explicit at the declaration site.
 */
struct PerformancePresetConfig
{
    bool shadowsEnabled = true;  ///< Directional shadow map pass.
    bool bloomEnabled   = true;  ///< Bloom + tonemap post-processing.
    bool iblEnabled     = true;  ///< Image-based lighting (BRDF LUT + env maps).
    bool vsync          = true;  ///< Swap-chain sync interval (v-sync ON/OFF).
    int  frameCap       = 60;    ///< Software frame cap fps; 0 = unlimited.
    int  anisoLevel     = 4;     ///< Anisotropic filtering level (1/2/4/8/16).
};

// ===========================================================================
// PresetDefaults — canonical defaults for each tier
// ===========================================================================

/**
 * @brief Return the canonical PerformancePresetConfig for the given preset tier.
 *
 * TEACHING NOTE — Data Table Pattern
 * ─────────────────────────────────────
 * Using a function (rather than a global array) keeps the values next to
 * descriptive comments.  A switch statement makes it clear what changes
 * between tiers and is impossible to accidentally index out-of-bounds (unlike
 * an array looked up with an unchecked integer).
 *
 * @param p  The desired performance tier.
 * @return   Default feature configuration for that tier.
 */
inline PerformancePresetConfig PresetDefaults(PerformancePreset p) noexcept
{
    switch (p)
    {
        case PerformancePreset::LOW:
        {
            // TEACHING NOTE — LOW preset design intent
            // Target: Intel HD 4000 (2012), GT 610, or any D3D11 FL 10_0 GPU.
            // All expensive post-processing is OFF.  30 fps cap gives extra
            // headroom so the GPU never thermal-throttles on this hardware.
            PerformancePresetConfig cfg;
            cfg.shadowsEnabled = false;
            cfg.bloomEnabled   = false;
            cfg.iblEnabled     = false;
            cfg.vsync          = true;
            cfg.frameCap       = 30;
            cfg.anisoLevel     = 1;
            return cfg;
        }
        case PerformancePreset::MEDIUM:
        {
            // TEACHING NOTE — MEDIUM preset design intent
            // Target: GT 1030 / RX 460 class (~2016 budget GPU).
            // Shadows ON (most visually impactful feature).
            // Bloom and IBL OFF to keep 60 fps on this tier.
            PerformancePresetConfig cfg;
            cfg.shadowsEnabled = true;
            cfg.bloomEnabled   = false;
            cfg.iblEnabled     = false;
            cfg.vsync          = true;
            cfg.frameCap       = 60;
            cfg.anisoLevel     = 4;
            return cfg;
        }
        case PerformancePreset::HIGH:
        {
            // TEACHING NOTE — HIGH preset design intent
            // Target: GTX 1060 / RX 580 (~2017 mainstream GPU).
            // All features ON at 60 fps.
            PerformancePresetConfig cfg;
            cfg.shadowsEnabled = true;
            cfg.bloomEnabled   = true;
            cfg.iblEnabled     = true;
            cfg.vsync          = true;
            cfg.frameCap       = 60;
            cfg.anisoLevel     = 8;
            return cfg;
        }
        case PerformancePreset::ULTRA:
        default:
        {
            // TEACHING NOTE — ULTRA preset design intent
            // Target: RTX 2060+ / RX 6700+.
            // Everything ON; frame cap OFF so players with high-refresh monitors
            // benefit from all available GPU headroom.
            PerformancePresetConfig cfg;
            cfg.shadowsEnabled = true;
            cfg.bloomEnabled   = true;
            cfg.iblEnabled     = true;
            cfg.vsync          = false; // Uncapped — let the monitor run free.
            cfg.frameCap       = 0;
            cfg.anisoLevel     = 16;
            return cfg;
        }
    }
}

// ===========================================================================
// PresetName — human-readable label
// ===========================================================================

/**
 * @brief Return the display name for a performance preset tier.
 *
 * Used by the Settings menu overlay and in the EngineConfig JSON.
 */
inline const char* PresetName(PerformancePreset p) noexcept
{
    switch (p)
    {
        case PerformancePreset::LOW:    return "Low";
        case PerformancePreset::MEDIUM: return "Medium";
        case PerformancePreset::HIGH:   return "High";
        case PerformancePreset::ULTRA:  return "Ultra";
        default:                        return "Unknown";
    }
}

/**
 * @brief Parse a preset name string back to the PerformancePreset enum.
 *
 * Case-sensitive.  Returns MEDIUM if the string is not recognized.
 *
 * TEACHING NOTE — Parsing Enum Names
 * ─────────────────────────────────────
 * We do not use C++17 std::from_chars or a map here because the set of
 * valid values is tiny and fixed at compile time.  The simple if-chain is
 * the most readable and debuggable approach.
 */
inline PerformancePreset ParsePresetName(const std::string& name) noexcept
{
    if (name == "Low")    return PerformancePreset::LOW;
    if (name == "High")   return PerformancePreset::HIGH;
    if (name == "Ultra")  return PerformancePreset::ULTRA;
    return PerformancePreset::MEDIUM; // Safe default for unrecognised strings.
}

} // namespace engine::core
