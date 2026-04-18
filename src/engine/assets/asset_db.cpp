/**
 * @file asset_db.cpp
 * @brief AssetDB implementation — parse assetdb.json into an in-memory map.
 *
 * ============================================================================
 * TEACHING NOTE — Why parse JSON in a separate .cpp file?
 * ============================================================================
 * The JSON parsing implementation (nlohmann/json or manual parsing) is an
 * implementation detail.  By keeping it in the .cpp file we:
 *
 *  1. Avoid exposing the JSON library headers to every translation unit that
 *     #includes asset_db.hpp — this dramatically speeds up incremental builds.
 *  2. Allow swapping the JSON library without changing the public API.
 *  3. Follow the "Pimpl-light" principle: callers see only what they need.
 *
 * For M2 we use a minimal hand-written JSON parser that handles the specific
 * assetdb.json format rather than adding a third-party dependency.  This
 * teaches the student exactly what JSON parsing involves.  In M3+ we will
 * replace this with nlohmann/json (added via vcpkg) for full robustness.
 *
 * ============================================================================
 * TEACHING NOTE — assetdb.json format (produced by cook.exe)
 * ============================================================================
 * {
 *   "version": "1.0.0",
 *   "assets": [
 *     { "id": "<uuid>", "cookedPath": "<relative/path/to/cooked/file>" },
 *     ...
 *   ]
 * }
 *
 * We only need "id" and "cookedPath" from each entry.
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 */

#include "asset_db.hpp"
#include "engine/core/Logger.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

// ============================================================================
// TEACHING NOTE — Minimal JSON Parser Helpers
// ============================================================================
// Rather than pulling in a full JSON library for this first iteration, we use
// a simple line-by-line extraction approach that works correctly for the flat
// assetdb.json format produced by cook_main.cpp.
//
// The algorithm:
//   1. Read the whole file into a string.
//   2. Find each JSON object that starts with { and ends with }.
//   3. Within each object, extract "id" and "cookedPath" values.
//
// This is intentionally simple and educational.  A production engine uses a
// proper JSON library (nlohmann/json, rapidjson, etc.).
// ============================================================================
namespace
{

// ---------------------------------------------------------------------------
// extract_string_value
// ---------------------------------------------------------------------------
// Given a JSON fragment like:  "someKey": "someValue",
// and the key "someKey", extract and return "someValue".
//
// TEACHING NOTE — std::string::find + substr pattern
// This is a common "manual parsing" technique: find the key, skip past the
// opening quote, find the closing quote, extract the substring.  It is
// fragile (won't handle escaped quotes) but sufficient for machine-generated
// JSON with predictable formatting.
// ---------------------------------------------------------------------------
static std::string extract_string_value(const std::string& object,
                                        const std::string& key)
{
    const std::string search = "\"" + key + "\"";
    std::size_t pos = object.find(search);
    if (pos == std::string::npos) return "";

    // Advance past the key and the ": " separator.
    pos = object.find(':', pos + search.size());
    if (pos == std::string::npos) return "";

    // Find the opening quote of the value.
    pos = object.find('"', pos + 1);
    if (pos == std::string::npos) return "";
    const std::size_t start = pos + 1;

    // Find the closing quote (not preceded by a backslash for simplicity).
    const std::size_t end = object.find('"', start);
    if (end == std::string::npos) return "";

    return object.substr(start, end - start);
}

} // anonymous namespace

// ============================================================================
// AssetDB implementation
// ============================================================================

namespace engine::assets
{

// ----------------------------------------------------------------------------
bool AssetDB::Load(const std::string& assetDbPath)
{
    // TEACHING NOTE — std::ifstream for file reading
    // std::ifstream opens a file for reading.  We check is_open() after
    // construction because the constructor does not throw on failure by
    // default (you can enable exceptions via f.exceptions() but that pattern
    // is less common in game code which prefers explicit error checks).
    std::ifstream f(assetDbPath);
    if (!f.is_open())
    {
        LOG_ERROR("AssetDB::Load — cannot open file: " << assetDbPath);
        return false;
    }

    // Read the whole file into a string.
    // TEACHING NOTE — std::istreambuf_iterator trick
    // Constructing a std::string from two istreambuf_iterators reads ALL
    // characters in the stream in a single step — no loop required.
    // This is the idiomatic "slurp file" pattern in C++.
    std::string contents((std::istreambuf_iterator<char>(f)),
                          std::istreambuf_iterator<char>());

    // Clear any existing data before repopulating.
    m_idToPath.clear();

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Simple object extraction
    // The assetdb.json format produced by cook_main is:
    //   { "assets": [ { "id": "...", "cookedPath": "..." }, ... ] }
    // We scan for JSON object boundaries to extract each asset entry.
    // -----------------------------------------------------------------------
    std::size_t braceDepth = 0;
    std::size_t objectStart = std::string::npos;
    bool inString = false;

    for (std::size_t i = 0; i < contents.size(); ++i)
    {
        const char c = contents[i];

        // Track string boundaries so braces inside strings are ignored.
        if (c == '"' && (i == 0 || contents[i - 1] != '\\'))
        {
            inString = !inString;
        }

        if (inString) continue;

        if (c == '{')
        {
            ++braceDepth;
            if (braceDepth == 2)  // top-level object is depth 1; asset entries are depth 2
                objectStart = i;
        }
        else if (c == '}')
        {
            if (braceDepth == 2 && objectStart != std::string::npos)
            {
                // We have a complete asset object.
                const std::string obj = contents.substr(objectStart, i - objectStart + 1);
                const std::string id         = extract_string_value(obj, "id");
                const std::string cookedPath = extract_string_value(obj, "cookedPath");

                if (!id.empty() && !cookedPath.empty())
                {
                    m_idToPath[id] = cookedPath;
                }
                objectStart = std::string::npos;
            }
            if (braceDepth > 0) --braceDepth;
        }
    }

    if (m_idToPath.empty())
    {
        LOG_WARN("AssetDB::Load — no assets found in: " << assetDbPath);
        // Return true: an empty database is valid (cook produced zero assets).
        return true;
    }

    LOG_INFO("AssetDB::Load — loaded " << m_idToPath.size()
             << " assets from " << assetDbPath);
    return true;
}

// ----------------------------------------------------------------------------
void AssetDB::Clear()
{
    m_idToPath.clear();
}

// ----------------------------------------------------------------------------
bool AssetDB::Has(const std::string& id) const
{
    return m_idToPath.find(id) != m_idToPath.end();
}

// ----------------------------------------------------------------------------
std::string AssetDB::GetCookedPath(const std::string& id) const
{
    auto it = m_idToPath.find(id);
    if (it == m_idToPath.end())
    {
        LOG_ERROR("AssetDB::GetCookedPath — GUID not found: " << id);
        return "";
    }
    return it->second;
}

// ----------------------------------------------------------------------------
std::size_t AssetDB::Count() const
{
    return m_idToPath.size();
}

} // namespace engine::assets
