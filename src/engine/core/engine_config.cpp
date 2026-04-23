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

#include <fstream>
#include <iostream>

#ifdef ENGINE_ENABLE_JSON
#  include <nlohmann/json.hpp>
   using json = nlohmann::json;
#endif

namespace engine::core {

bool EngineConfig::Load(const std::string& path)
{
#ifdef ENGINE_ENABLE_JSON
    // TEACHING NOTE — std::ifstream in binary mode prevents line-ending
    // translation on Windows which could corrupt binary asset reads.
    // For text JSON we use default (text) mode here.
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

        // Resolution block — .value() returns the default if the key is absent.
        if (j.contains("resolution"))
        {
            resolution.width  = j["resolution"].value("width",  resolution.width);
            resolution.height = j["resolution"].value("height", resolution.height);
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

        std::cout << "[EngineConfig] Loaded: " << path
                  << "  resolution=" << resolution.width << "x" << resolution.height
                  << "  masterVol=" << audio.masterVolume << "\n";
        return true;
    }
    catch (const std::exception& e)
    {
        // TEACHING NOTE — Parse errors are user errors (malformed JSON).
        // We warn but do NOT crash — the engine falls back to defaults.
        std::cerr << "[EngineConfig] WARNING: Failed to parse " << path
                  << ": " << e.what() << " — using defaults.\n";
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
    {
        std::cerr << "[EngineConfig] ERROR: Cannot write to " << path << "\n";
        return false;
    }

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
