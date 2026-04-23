#pragma once

/**
 * @file engine_config.hpp
 * @brief User-editable engine configuration loaded from engine_config.json.
 *
 * ============================================================================
 * TEACHING NOTE — Engine Configuration (LC-3)
 * ============================================================================
 * Hard-coding values like window resolution or audio volume forces users to
 * recompile just to change a setting — a terrible user experience.
 *
 * The EngineConfig system solves this by reading a JSON file at startup:
 *
 *   engine_config.json (sits next to engine_sandbox.exe)
 *   {
 *     "resolution": { "width": 1280, "height": 720 },
 *     "audio":      { "masterVolume": 1.0 },
 *     "keys":       { "attack": "Z", "dodge": "X", "jump": "Space" }
 *   }
 *
 * If the file does not exist, sensible defaults are used automatically.
 *
 * ============================================================================
 * TEACHING NOTE — Why JSON for Config?
 * ============================================================================
 * JSON is a good config format because:
 *   • Human-readable and editable in any text editor.
 *   • Widely supported (nlohmann/json already in vcpkg.json).
 *   • Self-documenting (field names are visible in the file).
 *   • Forward compatible: unknown fields are ignored, so older binaries
 *     still work when new fields are added.
 *
 * Alternative formats: INI (simpler but no nesting), TOML (cleaner but less
 * tool support), XML (verbose), binary (fast but not human-editable).
 *
 * ============================================================================
 * TEACHING NOTE — Dependency on ENGINE_ENABLE_JSON
 * ============================================================================
 * The JSON parsing path is gated on ENGINE_ENABLE_JSON (defined when
 * nlohmann/json is available via vcpkg).  When the macro is NOT defined,
 * EngineConfig::Load() is a no-op and all members keep their default values.
 * This ensures the config system compiles even on minimal builds.
 * ============================================================================
 */

#include <string>

namespace engine::core {

// ---------------------------------------------------------------------------
// Resolution configuration
// ---------------------------------------------------------------------------
struct ResolutionConfig
{
    int width  = 1280;  ///< Window width  in pixels (default 1280).
    int height = 720;   ///< Window height in pixels (default 720).
};

// ---------------------------------------------------------------------------
// Audio configuration
// ---------------------------------------------------------------------------
struct AudioConfig
{
    float masterVolume = 1.0f;  ///< Global volume multiplier [0.0, 1.0].
};

// ---------------------------------------------------------------------------
// Key-binding configuration
// ---------------------------------------------------------------------------
// TEACHING NOTE — Key names are stored as single-character strings ("Z") or
// named keys ("Space", "Escape").  InputMapper converts them to Win32 VK codes
// at startup.
struct KeysConfig
{
    std::string attack = "Z";       ///< Attack / confirm button.
    std::string dodge  = "X";       ///< Dodge / cancel button.
    std::string jump   = "Space";   ///< Jump button.
    std::string menu   = "Escape";  ///< Open main menu.
    std::string interact = "F";     ///< Interact / talk.
};

// ---------------------------------------------------------------------------
// Top-level engine configuration
// ---------------------------------------------------------------------------
struct EngineConfig
{
    ResolutionConfig resolution;  ///< Window dimensions.
    AudioConfig      audio;       ///< Audio settings.
    KeysConfig       keys;        ///< Key bindings.

    /**
     * @brief Load configuration from a JSON file.
     *
     * @param path  Path to engine_config.json (relative or absolute).
     *              If the file does not exist or cannot be parsed, all fields
     *              keep their default values and the function returns false.
     * @return true if the file was loaded successfully; false otherwise.
     *
     * TEACHING NOTE — Fail-soft loading
     * The function intentionally returns false without throwing so the engine
     * can start with defaults if no config file is present.  A missing config
     * is normal on a fresh install; a parse error indicates a user error that
     * should be reported but not crash the engine.
     */
    bool Load(const std::string& path = "engine_config.json");

    /**
     * @brief Save the current configuration to a JSON file.
     *
     * @param path  Destination path.  Overwrites an existing file.
     * @return true on success; false if the file could not be written.
     */
    bool Save(const std::string& path = "engine_config.json") const;
};

} // namespace engine::core
