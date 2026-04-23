/**
 * @file engine_config.cpp
 * @brief Implementation of EngineConfig: JSON load/save for user settings.
 *
 * TEACHING NOTE — Conditional compilation with ENGINE_ENABLE_JSON
 * ──────────────────────────────────────────────────────────────────
 * This file uses nlohmann/json when the ENGINE_ENABLE_JSON compile definition
 * is present (set by CMakeLists.txt when the vcpkg package is installed).
 *
 * When ENGINE_ENABLE_JSON is NOT defined (minimal build without vcpkg), the
 * Load() and Save() functions are no-ops that return false — the engine still
 * works, just with hardcoded defaults.
 *
 * This pattern is used throughout the engine for optional features:
 *   #ifdef ENGINE_ENABLE_FEATURE
 *       // feature-specific code
 *   #endif
 *
 * TEACHING NOTE — value_or() idiom for JSON defaults
 * ────────────────────────────────────────────────────
 * nlohmann/json's value() method returns a default when a key is missing:
 *   j["resolution"].value("width", 1280)
 * This makes forward-compatible deserialization trivial: old config files
 * without a new field still produce correct defaults.
 */

#include "engine_config.hpp"

#include <algorithm>
#include <fstream>

#ifdef ENGINE_ENABLE_JSON
#  include <nlohmann/json.hpp>
   using json = nlohmann::json;
#endif

namespace engine::core {

bool EngineConfig::Load(const std::string& path)
{
#ifdef ENGINE_ENABLE_JSON
    // TEACHING NOTE — std::ifstream text mode on Windows
    // We use the default (text) mode here — std::ifstream without std::ios::binary.
    // On Windows, text mode translates "\r\n" to "\n" on read.  For JSON, this
    // is harmless: the parser treats whitespace uniformly.  Binary mode is only
    // needed when reading raw byte arrays where a '\r' byte has semantic meaning.
    std::ifstream f(path);
    if (!f.is_open())
    {
        // Missing config is normal on first run — use defaults silently.
        return false;
    }

    try
    {
        json j;
        f >> j;

        // Resolution block.
        // TEACHING NOTE — Read raw JSON integers as int first, then clamp to
        // [kMinDim, kMaxDim] before storing in the uint32_t fields.  This prevents
        // a negative or zero JSON value from wrapping to a huge unsigned number
        // and producing a window dimension the OS rejects.
        if (j.contains("resolution"))
        {
            const int rawW = j["resolution"].value("width",  static_cast<int>(resolution.width));
            const int rawH = j["resolution"].value("height", static_cast<int>(resolution.height));
            resolution.width  = static_cast<uint32_t>(std::max(kResolutionMinDim, std::min(kResolutionMaxDim, rawW)));
            resolution.height = static_cast<uint32_t>(std::max(kResolutionMinDim, std::min(kResolutionMaxDim, rawH)));
        }

        // Audio block.
        if (j.contains("audio"))
        {
            audio.masterVolume = j["audio"].value("masterVolume", audio.masterVolume);
        }

        // Key bindings block.
        if (j.contains("keys"))
        {
            const auto& k = j["keys"];
            keys.attack   = k.value("attack",   keys.attack);
            keys.dodge    = k.value("dodge",    keys.dodge);
            keys.jump     = k.value("jump",     keys.jump);
            keys.menu     = k.value("menu",     keys.menu);
            keys.interact = k.value("interact", keys.interact);
        }

        // TEACHING NOTE — EngineConfig::Load() is intentionally silent.
        // Diagnostics are surfaced via the bool return value so the caller can
        // route them through the engine Logger, ensuring they appear in the
        // Saved/Logs/*.log file rather than bypassing it via direct console I/O.
        return true;
    }
    catch (const std::exception&)
    {
        // TEACHING NOTE — Parse errors are recoverable user-data problems.
        // We keep EngineConfig free of direct console I/O; higher-level startup
        // code can decide how to surface the failure through the active logger.
        return false;
    }
#else
    // ENGINE_ENABLE_JSON is not defined — JSON parsing unavailable.
    // The engine will use compiled-in default values.
    (void)path;
    return false;
#endif
}

bool EngineConfig::Save(const std::string& path) const
{
#ifdef ENGINE_ENABLE_JSON
    json j;
    j["resolution"]["width"]  = resolution.width;
    j["resolution"]["height"] = resolution.height;
    j["audio"]["masterVolume"] = audio.masterVolume;
    j["keys"]["attack"]   = keys.attack;
    j["keys"]["dodge"]    = keys.dodge;
    j["keys"]["jump"]     = keys.jump;
    j["keys"]["menu"]     = keys.menu;
    j["keys"]["interact"] = keys.interact;

    std::ofstream f(path);
    if (!f.is_open())
        return false;

    // TEACHING NOTE — dump(4) formats the JSON with 4-space indentation,
    // making it easy for users to read and edit in any text editor.
    f << j.dump(4) << "\n";
    return true;
#else
    (void)path;
    return false;
#endif
}

} // namespace engine::core
