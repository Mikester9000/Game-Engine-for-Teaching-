// ============================================================================
// asset_db.cpp — Runtime Asset Database implementation
// ============================================================================
//
// TEACHING NOTE — Minimal JSON parsing without external libraries
// ================================================================
// We deliberately avoid adding a third-party JSON library for the AssetDB
// because the assetdb.json format is simple and stable:
//
//   {
//     "version": "1.0.0",
//     "assets": [
//       { "id": "...", "type": "...", "name": "...",
//         "source": "...", "cooked": "...", "hash": "...",
//         "tags": [...] },
//       ...
//     ]
//   }
//
// The parser uses a token-level approach:
//   1. Locate the "assets" array.
//   2. Split into individual object strings.
//   3. Extract named string fields with a tiny helper.
//
// For a production engine (M2+) this should be replaced with nlohmann/json
// (add "nlohmann-json" to vcpkg.json).  The interface stays the same.
//
// TEACHING NOTE — Why nlohmann/json for production?
// nlohmann/json provides:
//   • A clean, modern C++ API (json::parse, j["key"].get<std::string>()).
//   • Full RFC 8259 compliance including Unicode.
//   • Header-only, so adding it is a single line in CMakeLists.txt.
//   • Extremely well-tested (>95% branch coverage).
// ============================================================================

#include "asset_db.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <iostream>

namespace engine::assets {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

/// Read the entire contents of @p path into a string.
/// Returns an empty string and sets @p ok=false on failure.
static std::string ReadFile(const std::string& path, bool& ok) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        ok = false;
        return {};
    }
    std::ostringstream ss;
    ss << ifs.rdbuf();
    ok = true;
    return ss.str();
}

/// Extract the first JSON string value for @p key inside @p obj.
/// E.g. ExtractString(R"({"id":"abc","type":"tex"})", "id") → "abc".
/// Returns "" when the key is absent.
static std::string ExtractString(const std::string& obj, const std::string& key) {
    // TEACHING NOTE — Why not regex?
    // std::regex is available in C++11 but has poor performance on some
    // standard library implementations (libstdc++ notably).  For a simple
    // key-value extraction on well-formed JSON, a manual search is faster
    // and avoids pulling in <regex>.
    std::string needle = "\"" + key + "\"";
    auto pos = obj.find(needle);
    if (pos == std::string::npos) return {};

    pos += needle.size();
    // skip optional whitespace + colon + optional whitespace
    while (pos < obj.size() && (obj[pos] == ' ' || obj[pos] == '\t' ||
           obj[pos] == '\n' || obj[pos] == '\r' || obj[pos] == ':')) {
        ++pos;
    }
    if (pos >= obj.size() || obj[pos] != '"') return {};

    ++pos; // skip opening quote
    std::string result;
    while (pos < obj.size() && obj[pos] != '"') {
        if (obj[pos] == '\\' && pos + 1 < obj.size()) {
            // basic escape handling
            ++pos;
            switch (obj[pos]) {
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                default:   result += obj[pos]; break;
            }
        } else {
            result += obj[pos];
        }
        ++pos;
    }
    return result;
}

/// Extract a JSON array of strings for @p key inside @p obj.
/// E.g. ExtractStringArray(R"({"tags":["a","b"]})", "tags") → {"a","b"}.
static std::vector<std::string> ExtractStringArray(
    const std::string& obj, const std::string& key)
{
    std::vector<std::string> result;
    std::string needle = "\"" + key + "\"";
    auto pos = obj.find(needle);
    if (pos == std::string::npos) return result;

    pos += needle.size();
    // skip : and whitespace
    while (pos < obj.size() && (obj[pos] == ' ' || obj[pos] == ':' ||
           obj[pos] == '\t' || obj[pos] == '\n' || obj[pos] == '\r')) {
        ++pos;
    }
    if (pos >= obj.size() || obj[pos] != '[') return result;
    ++pos; // skip '['

    while (pos < obj.size() && obj[pos] != ']') {
        // skip whitespace + commas
        while (pos < obj.size() && (obj[pos] == ' ' || obj[pos] == ',' ||
               obj[pos] == '\t' || obj[pos] == '\n' || obj[pos] == '\r')) {
            ++pos;
        }
        if (pos >= obj.size() || obj[pos] == ']') break;
        if (obj[pos] == '"') {
            ++pos;
            std::string val;
            while (pos < obj.size() && obj[pos] != '"') {
                val += obj[pos++];
            }
            if (pos < obj.size()) ++pos; // skip closing '"'
            result.push_back(std::move(val));
        } else {
            ++pos; // skip unexpected character
        }
    }
    return result;
}

/// Split the text of a JSON array (without the outer [ ]) into individual
/// object strings.  Handles nested braces so objects with sub-objects are
/// not split incorrectly.
static std::vector<std::string> SplitJsonObjects(const std::string& arrayText) {
    std::vector<std::string> objects;
    int depth = 0;
    std::size_t start = std::string::npos;
    bool inString = false;

    for (std::size_t i = 0; i < arrayText.size(); ++i) {
        char c = arrayText[i];
        if (c == '"' && (i == 0 || arrayText[i - 1] != '\\')) {
            inString = !inString;
            continue;
        }
        if (inString) continue;

        if (c == '{') {
            if (depth == 0) start = i;
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0 && start != std::string::npos) {
                objects.push_back(arrayText.substr(start, i - start + 1));
                start = std::string::npos;
            }
        }
    }
    return objects;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// AssetDB::ParseJson
// ---------------------------------------------------------------------------
bool AssetDB::ParseJson(const std::string& jsonText) {
    // Locate the "assets" array.
    const std::string assetsKey = R"("assets")";
    auto assetsPos = jsonText.find(assetsKey);
    if (assetsPos == std::string::npos) {
        std::cerr << "[AssetDB] JSON has no 'assets' key\n";
        return false;
    }

    // Find the opening '[' after "assets":
    auto bracketPos = jsonText.find('[', assetsPos + assetsKey.size());
    if (bracketPos == std::string::npos) {
        std::cerr << "[AssetDB] 'assets' key is not followed by '['\n";
        return false;
    }

    // Find the matching closing ']'.
    int depth = 0;
    std::size_t arrayEnd = std::string::npos;
    bool inString = false;
    for (std::size_t i = bracketPos; i < jsonText.size(); ++i) {
        char c = jsonText[i];
        if (c == '"' && (i == 0 || jsonText[i - 1] != '\\')) {
            inString = !inString;
            continue;
        }
        if (inString) continue;
        if (c == '[') ++depth;
        else if (c == ']') {
            --depth;
            if (depth == 0) { arrayEnd = i; break; }
        }
    }
    if (arrayEnd == std::string::npos) {
        std::cerr << "[AssetDB] Unmatched '[' in 'assets' array\n";
        return false;
    }

    std::string arrayText = jsonText.substr(bracketPos + 1, arrayEnd - bracketPos - 1);
    auto objects = SplitJsonObjects(arrayText);

    m_entries.reserve(objects.size());
    for (const auto& obj : objects) {
        AssetEntry entry;
        entry.id     = ExtractString(obj, "id");
        entry.type   = ExtractString(obj, "type");
        entry.name   = ExtractString(obj, "name");
        entry.source = ExtractString(obj, "source");
        entry.cooked = ExtractString(obj, "cooked");
        entry.hash   = ExtractString(obj, "hash");
        entry.tags   = ExtractStringArray(obj, "tags");

        if (entry.id.empty()) {
            std::cerr << "[AssetDB] Skipping asset entry with empty id\n";
            continue;
        }
        m_entries[entry.id] = std::move(entry);
    }
    return true;
}

// ---------------------------------------------------------------------------
// AssetDB::Load
// ---------------------------------------------------------------------------
bool AssetDB::Load(const std::string& assetDbPath) {
    m_entries.clear();
    m_dbPath = assetDbPath;

    bool ok = false;
    std::string text = ReadFile(assetDbPath, ok);
    if (!ok) {
        std::cerr << "[AssetDB] Cannot open: " << assetDbPath << "\n";
        return false;
    }
    if (!ParseJson(text)) {
        std::cerr << "[AssetDB] Parse error in: " << assetDbPath << "\n";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// AssetDB query methods
// ---------------------------------------------------------------------------
std::string AssetDB::GetCookedPath(const std::string& id) const {
    auto it = m_entries.find(id);
    return (it != m_entries.end()) ? it->second.cooked : std::string{};
}

const AssetEntry* AssetDB::GetEntry(const std::string& id) const {
    auto it = m_entries.find(id);
    return (it != m_entries.end()) ? &it->second : nullptr;
}

bool AssetDB::Has(const std::string& id) const {
    return m_entries.count(id) > 0;
}

} // namespace engine::assets
