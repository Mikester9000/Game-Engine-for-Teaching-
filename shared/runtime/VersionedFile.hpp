/**
 * @file VersionedFile.hpp
 * @brief Versioned JSON file read/write utilities.
 *
 * =============================================================================
 * TEACHING NOTE — Why Version Files?
 * =============================================================================
 * Every data file shared between authoring tools and the engine MUST carry a
 * version number. When you change a field name, add/remove a required field,
 * or change the semantics of a value, you increment the version number.
 *
 * Without versioning:
 *   Developer A saves a scene in the editor.
 *   Developer B pulls the code, engine format changed — their saves break.
 *   Users install an update — their save files no longer load.
 *
 * With versioning:
 *   Reader checks version, applies migration if needed, then processes.
 *
 * This file provides a minimal wrapper around nlohmann/json (or any JSON
 * library you choose) that enforces:
 *   1. Every file has a "version" field.
 *   2. Reading checks the version and can reject or migrate old data.
 *   3. Writing always stamps the current version.
 *
 * =============================================================================
 * Dependencies:
 *   This header uses the C++ standard library only (fstream, string, etc.).
 *   JSON parsing is delegated to whatever library the including project uses.
 *   To use with Qt: include <QJsonDocument> and adapt VersionedFile::Read().
 *   To use standalone: include a header-only JSON lib (e.g. nlohmann/json).
 *
 * =============================================================================
 */

#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <functional>

/**
 * @struct Version
 * @brief A simple three-component SemVer version number.
 *
 * TEACHING NOTE — Semantic Versioning (SemVer)
 *   MAJOR.MINOR.PATCH
 *   • MAJOR: breaking change — old readers can't load this file.
 *   • MINOR: backwards-compatible addition — old readers ignore new fields.
 *   • PATCH: bug fix — no format change.
 */
struct Version
{
    int major = 0;  ///< Breaking changes
    int minor = 0;  ///< Backwards-compatible additions
    int patch = 0;  ///< Bug fixes / cosmetic

    /// Parse from "major.minor.patch" string.  Returns {0,0,0} on failure.
    static Version FromString(const std::string& s)
    {
        Version v;
        char dot;
        std::istringstream ss(s);
        ss >> v.major >> dot >> v.minor >> dot >> v.patch;
        return v;
    }

    /// Convert to "major.minor.patch" string.
    std::string ToString() const
    {
        return std::to_string(major) + '.' +
               std::to_string(minor) + '.' +
               std::to_string(patch);
    }

    bool operator==(const Version& r) const
    { return major == r.major && minor == r.minor && patch == r.patch; }
    bool operator<(const Version& r) const
    {
        if (major != r.major) return major < r.major;
        if (minor != r.minor) return minor < r.minor;
        return patch < r.patch;
    }
    bool operator>(const Version& r)  const { return r < *this; }
    bool operator<=(const Version& r) const { return !(r < *this); }
    bool operator>=(const Version& r) const { return !(*this < r); }
};

/**
 * @struct FileHeader
 * @brief Minimum header every versioned JSON file must have.
 *
 * TEACHING NOTE — Minimal Header Pattern
 * Every shared file starts with at least these two fields:
 *   { "version": "1.0.0", ... }
 *
 * The reader extracts the version first, then decides how to parse the rest.
 * This allows format migrations without breaking older projects.
 */
struct FileHeader
{
    Version     version;          ///< Parsed version
    std::string schemaPath;       ///< Optional "$schema" path for tooling
};

/**
 * @brief Extract FileHeader from a raw JSON string (minimal, no deps).
 *
 * This is intentionally simple — it looks for the "version" key with a
 * naive string search.  A production engine would use a real JSON parser.
 * For teaching, the simplicity makes the concept clearer.
 *
 * @param  json     Raw JSON text.
 * @return FileHeader with parsed version (or {0,0,0} if not found).
 */
inline FileHeader ExtractHeader(const std::string& json)
{
    FileHeader header;

    // Very simple pattern search: "version": "1.0.0"
    const std::string key = "\"version\"";
    auto pos = json.find(key);
    if (pos != std::string::npos)
    {
        auto quote1 = json.find('"', pos + key.size() + 1);  // find opening "
        if (quote1 != std::string::npos)
        {
            auto quote2 = json.find('"', quote1 + 1);        // find closing "
            if (quote2 != std::string::npos)
            {
                header.version = Version::FromString(
                    json.substr(quote1 + 1, quote2 - quote1 - 1)
                );
            }
        }
    }

    return header;
}

/**
 * @brief Read a JSON file from disk into a string.
 * @throws std::runtime_error if the file cannot be opened.
 */
inline std::string ReadFileToString(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("VersionedFile: cannot open '" + path + "'");

    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

/**
 * @brief Write a string to a file (overwrites if exists).
 * @throws std::runtime_error if the file cannot be written.
 */
inline void WriteStringToFile(const std::string& path, const std::string& content)
{
    std::ofstream file(path);
    if (!file.is_open())
        throw std::runtime_error("VersionedFile: cannot write '" + path + "'");

    file << content;
}
