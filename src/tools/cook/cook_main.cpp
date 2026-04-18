/**
 * @file cook_main.cpp
 * @brief cook.exe — the M2 asset cooker entry point.
 *
 * ============================================================================
 * TEACHING NOTE — What is cook.exe?
 * ============================================================================
 * Every commercial game engine has an "asset cooking" step that transforms
 * raw source assets (PNG, WAV, FBX, JSON) into compact, engine-optimised
 * binary files.  cook.exe is our standalone command-line cooker.
 *
 * Cooking serves three purposes:
 *  1. Format conversion: PNG → BC7 DDS (GPU-compressed texture);
 *                        WAV → OGG (smaller audio);
 *                        FBX → .mesh (engine-native binary mesh).
 *  2. Validation: ensure all referenced assets exist and are well-formed.
 *  3. Indexing: build assetdb.json (GUID → cooked path map) for the runtime.
 *
 * For M2 the cook steps are stubs: we copy the source file to Cooked/ with
 * the correct naming convention and write assetdb.json.  Real conversion will
 * be added in M3 (textures) and M4 (audio/animation).
 *
 * ============================================================================
 * TEACHING NOTE — Cook pipeline design
 * ============================================================================
 *
 *  Input:  AssetRegistry.json   (produced by editor / cook_assets.py)
 *          Content/             (raw source assets)
 *
 *  Output: Cooked/<type>/<name>.<ext>   (cooked assets — content unchanged for M2)
 *          Cooked/assetdb.json          (GUID → cookedPath index for runtime)
 *
 * The assetdb.json format is deliberately minimal — it contains only what the
 * engine runtime needs:
 *  {
 *    "version": "1.0.0",
 *    "assets": [
 *      { "id": "<uuid>", "cookedPath": "Cooked/<type>/<file>" },
 *      ...
 *    ]
 *  }
 *
 * ============================================================================
 * TEACHING NOTE — Why a standalone exe instead of a library?
 * ============================================================================
 * The cooker runs at AUTHOR time (on the developer's machine or in CI), not
 * at RUNTIME (in the shipped game).  Keeping it as a separate executable:
 *  • Means the cook logic never ships in the game binary (smaller, safer).
 *  • Can be run from CI without starting the full engine.
 *  • Is easy to invoke from Python scripts or shell scripts.
 *  • Maps exactly to how Unreal's UnrealPak and Unity's BuildAssetBundles work.
 *
 * ============================================================================
 *
 * Usage:
 *   cook.exe --project <path/to/project>
 *
 * Example:
 *   cook.exe --project samples/vertical_slice_project
 *
 * Exit codes:
 *   0  — all assets cooked successfully.
 *   1  — fatal error (missing registry, malformed JSON, I/O failure).
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 */

#include "engine/core/Logger.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>
#include <vector>
#include <cstring>  // std::strcmp

// ============================================================================
// TEACHING NOTE — Using std::filesystem (C++17)
// ============================================================================
// std::filesystem provides cross-platform directory and path manipulation.
// Key types:
//   fs::path          — a file-system path (can be relative or absolute).
//   fs::create_directories(p) — create all directories in the path.
//   fs::copy_file(src, dst)   — copy a single file.
//   fs::exists(p)             — check if a path exists.
// This replaces platform-specific mkdir() / CopyFile() calls, making the
// code portable across Windows, Linux, and macOS.
// ============================================================================
namespace fs = std::filesystem;

// ============================================================================
// TEACHING NOTE — Minimal JSON helpers for cook output
// ============================================================================
// For M2 we hand-write the JSON output rather than using a JSON library.
// This is intentional: it teaches students exactly what JSON looks like and
// how to produce it programmatically.  The format is simple enough that a
// robust hand-written serialiser is fine.
//
// In M3 we will add nlohmann/json via vcpkg for both reading and writing.
// ============================================================================
namespace
{

// ---------------------------------------------------------------------------
// json_escape
// ---------------------------------------------------------------------------
// Escape special characters in a string for safe embedding in JSON.
// TEACHING NOTE — JSON string escaping rules:
//   \  → \\      (backslash)
//   "  → \"      (double quote)
//   \n → \n      (newline)
// A minimal implementation is fine for file paths and GUIDs, which never
// contain special characters.
// ---------------------------------------------------------------------------
static std::string json_escape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s)
    {
        switch (c)
        {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// extract_json_string_value
// ---------------------------------------------------------------------------
// Minimal JSON string-value extractor for reading AssetRegistry.json.
// ---------------------------------------------------------------------------
static std::string extract_json_string_value(const std::string& text,
                                              const std::string& key)
{
    const std::string search = "\"" + key + "\"";
    std::size_t pos = text.find(search);
    if (pos == std::string::npos) return "";

    pos = text.find(':', pos + search.size());
    if (pos == std::string::npos) return "";

    pos = text.find('"', pos + 1);
    if (pos == std::string::npos) return "";
    const std::size_t start = pos + 1;

    const std::size_t end = text.find('"', start);
    if (end == std::string::npos) return "";

    return text.substr(start, end - start);
}

// ---------------------------------------------------------------------------
// AssetEntry — one row from AssetRegistry.json
// ---------------------------------------------------------------------------
struct AssetEntry
{
    std::string id;
    std::string type;
    std::string name;
    std::string source;   // relative path under the project root (Content/…)
    std::string cooked;   // desired cooked output path (relative to project)
};

// ---------------------------------------------------------------------------
// parse_asset_registry
// ---------------------------------------------------------------------------
// Read AssetRegistry.json and return a list of AssetEntry records.
//
// TEACHING NOTE — Simple JSON parsing with braces tracking
// The registry contains an array of objects.  We extract each object using
// brace-depth tracking (same technique used in AssetDB::Load), then pull out
// the fields we need.  This is a teaching-grade parser; production code would
// use nlohmann/json (M3+).
// ---------------------------------------------------------------------------
static std::vector<AssetEntry> parse_asset_registry(const fs::path& registryPath)
{
    std::ifstream f(registryPath);
    if (!f.is_open())
    {
        std::cerr << "[cook] ERROR: Cannot open AssetRegistry.json: "
                  << registryPath << "\n";
        return {};
    }

    std::string contents((std::istreambuf_iterator<char>(f)),
                          std::istreambuf_iterator<char>());

    std::vector<AssetEntry> entries;
    std::size_t braceDepth = 0;
    std::size_t objectStart = std::string::npos;
    bool inString = false;

    for (std::size_t i = 0; i < contents.size(); ++i)
    {
        const char c = contents[i];
        if (c == '"' && (i == 0 || contents[i - 1] != '\\'))
            inString = !inString;
        if (inString) continue;

        if (c == '{')
        {
            ++braceDepth;
            if (braceDepth == 2)
                objectStart = i;
        }
        else if (c == '}')
        {
            if (braceDepth == 2 && objectStart != std::string::npos)
            {
                const std::string obj = contents.substr(objectStart,
                                                         i - objectStart + 1);
                AssetEntry e;
                e.id     = extract_json_string_value(obj, "id");
                e.type   = extract_json_string_value(obj, "type");
                e.name   = extract_json_string_value(obj, "name");
                e.source = extract_json_string_value(obj, "source");
                e.cooked = extract_json_string_value(obj, "cooked");

                if (!e.id.empty() && !e.source.empty())
                    entries.push_back(std::move(e));

                objectStart = std::string::npos;
            }
            if (braceDepth > 0) --braceDepth;
        }
    }

    return entries;
}

} // anonymous namespace

// ============================================================================
// TEACHING NOTE — Main function structure for a tool
// ============================================================================
// A well-structured command-line tool main() does:
//   1. Parse arguments — determine what to do.
//   2. Validate inputs — fail fast with a clear error if something is wrong.
//   3. Do work — iterate assets, copy/convert, write output.
//   4. Report results — print a summary so CI can understand what happened.
//   5. Return 0 on success, non-zero on any error — so CI scripts can check.
// ============================================================================
int main(int argc, char* argv[])
{
    // -----------------------------------------------------------------------
    // Step 1 — Argument parsing.
    // -----------------------------------------------------------------------
    std::string projectDir;
    bool        verbose = false;

    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--project") == 0 && i + 1 < argc)
        {
            projectDir = argv[++i];
        }
        else if (std::strcmp(argv[i], "--verbose") == 0 ||
                 std::strcmp(argv[i], "-v") == 0)
        {
            verbose = true;
        }
    }

    if (projectDir.empty())
    {
        std::cerr << "Usage: cook.exe --project <path/to/project> [--verbose]\n";
        std::cerr << "Example: cook.exe --project samples/vertical_slice_project\n";
        return 1;
    }

    // -----------------------------------------------------------------------
    // Step 2 — Validate project directory and locate required files.
    // -----------------------------------------------------------------------
    const fs::path projectPath(projectDir);
    if (!fs::exists(projectPath))
    {
        std::cerr << "[cook] ERROR: Project directory does not exist: "
                  << projectDir << "\n";
        return 1;
    }

    const fs::path registryPath = projectPath / "AssetRegistry.json";
    if (!fs::exists(registryPath))
    {
        std::cerr << "[cook] ERROR: AssetRegistry.json not found in: "
                  << projectDir << "\n";
        return 1;
    }

    const fs::path contentDir = projectPath / "Content";
    const fs::path cookedDir  = projectPath / "Cooked";

    std::cout << "============================================================\n";
    std::cout << " cook.exe — Asset Cooker (M2)\n";
    std::cout << "============================================================\n";
    std::cout << " Project  : " << fs::absolute(projectPath).string() << "\n";
    std::cout << " Registry : " << registryPath.filename().string() << "\n";
    std::cout << " Content  : " << contentDir.filename().string() << "/\n";
    std::cout << " Output   : " << cookedDir.filename().string() << "/\n";
    std::cout << "\n";

    // -----------------------------------------------------------------------
    // Step 3 — Parse AssetRegistry.json.
    // -----------------------------------------------------------------------
    const auto entries = parse_asset_registry(registryPath);
    if (entries.empty())
    {
        std::cout << "[cook] WARNING: No assets in registry.  Nothing to cook.\n";
        // Produce an empty assetdb.json so the engine can still start.
    }
    else
    {
        std::cout << "[cook] Registry parsed: " << entries.size()
                  << " asset(s) found.\n\n";
    }

    // Ensure Cooked/ directory exists.
    std::error_code ec;
    fs::create_directories(cookedDir, ec);
    if (ec)
    {
        std::cerr << "[cook] ERROR: Cannot create Cooked/ directory: "
                  << ec.message() << "\n";
        return 1;
    }

    // -----------------------------------------------------------------------
    // Step 4 — Cook each asset (M2: copy source → Cooked/).
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Incremental cooking (future enhancement)
    // A real cooker checks the hash of the source file against the stored
    // hash in the registry.  If they match, the asset is up-to-date and the
    // cook step is skipped.  For M2 we always recook (unconditional copy).
    // Incremental cooking will be introduced when we add the real conversion
    // steps in M3.
    // -----------------------------------------------------------------------
    int  cooked  = 0;
    int  skipped = 0;
    int  errors  = 0;

    // assetdb entries accumulated for writing to assetdb.json
    struct DbEntry { std::string id; std::string cookedPath; };
    std::vector<DbEntry> dbEntries;

    for (const auto& entry : entries)
    {
        // Resolve source path.
        // TEACHING NOTE — Source path resolution strategy
        // AssetRegistry.json stores the source path in two possible conventions:
        //   1. Relative to the project root (e.g., "Content/Maps/file.json")
        //   2. Relative to the Content/ directory (e.g., "Maps/file.json")
        // We try both conventions so cook.exe works with registries generated
        // by the Python cook_assets.py script (convention 2) and future
        // editor-generated registries (convention 1).
        fs::path srcPath;

        if (fs::path(entry.source).is_absolute())
        {
            srcPath = fs::path(entry.source);
        }
        else if (fs::exists(projectPath / entry.source))
        {
            // Convention 1: relative to project root.
            srcPath = projectPath / entry.source;
        }
        else if (fs::exists(contentDir / entry.source))
        {
            // Convention 2: relative to Content/ directory.
            srcPath = contentDir / entry.source;
        }
        else
        {
            std::cerr << "[cook] ERROR: Source file not found: "
                      << entry.source << "\n"
                      << "  Tried: " << (projectPath / entry.source).string() << "\n"
                      << "  Tried: " << (contentDir / entry.source).string() << "\n";
            ++errors;
            continue;
        }

        // Determine destination path.
        // If the registry already has a "cooked" field, use it.
        // Otherwise derive: Cooked/<type>/<filename>.
        fs::path dstPath;
        if (!entry.cooked.empty())
        {
            dstPath = fs::path(entry.cooked).is_relative()
                      ? projectPath / entry.cooked
                      : fs::path(entry.cooked);
        }
        else
        {
            // TEACHING NOTE — Derived cooked path
            // When no cooked path is specified, we place the asset under
            // Cooked/<AssetType>/<filename>.  This mirrors Unreal Engine's
            // cooked content layout where each asset type has its own folder.
            dstPath = cookedDir / entry.type / srcPath.filename();
        }

        // Create destination directory if needed.
        fs::create_directories(dstPath.parent_path(), ec);
        if (ec)
        {
            std::cerr << "[cook] ERROR: Cannot create directory: "
                      << dstPath.parent_path().string()
                      << " — " << ec.message() << "\n";
            ++errors;
            continue;
        }

        // Copy file (overwrite if already exists).
        fs::copy_file(srcPath, dstPath, fs::copy_options::overwrite_existing, ec);
        if (ec)
        {
            std::cerr << "[cook] ERROR: Copy failed: "
                      << srcPath.string() << " -> " << dstPath.string()
                      << " — " << ec.message() << "\n";
            ++errors;
            continue;
        }

        if (verbose)
        {
            std::cout << "  [" << entry.type << "] "
                      << srcPath.filename().string()
                      << "  →  "
                      << fs::relative(dstPath, projectPath).string()
                      << "\n";
        }

        // Compute relative cooked path for assetdb.json.
        // Use forward slashes for portability (engine on Linux/Windows).
        std::string relCooked = fs::relative(dstPath, projectPath).string();
        // Normalise path separators to forward slash.
        for (char& ch : relCooked)
            if (ch == '\\') ch = '/';

        dbEntries.push_back({ entry.id, relCooked });
        ++cooked;
    }

    // -----------------------------------------------------------------------
    // Step 5 — Write assetdb.json.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Why a separate assetdb.json and not just use the registry?
    // AssetRegistry.json can be very large in a real project (hundreds of
    // fields per asset).  assetdb.json is a stripped-down runtime index:
    // only the GUID and cooked path are needed.  This keeps engine startup
    // fast and reduces memory usage.
    // -----------------------------------------------------------------------
    const fs::path assetDbPath = cookedDir / "assetdb.json";

    {
        std::ofstream out(assetDbPath);
        if (!out.is_open())
        {
            std::cerr << "[cook] ERROR: Cannot write assetdb.json: "
                      << assetDbPath.string() << "\n";
            return 1;
        }

        out << "{\n";
        out << "  \"version\": \"1.0.0\",\n";
        out << "  \"assets\": [\n";

        for (std::size_t i = 0; i < dbEntries.size(); ++i)
        {
            out << "    { \"id\": \"" << json_escape(dbEntries[i].id)
                << "\", \"cookedPath\": \"" << json_escape(dbEntries[i].cookedPath)
                << "\" }";
            if (i + 1 < dbEntries.size()) out << ",";
            out << "\n";
        }

        out << "  ]\n";
        out << "}\n";
    }

    // -----------------------------------------------------------------------
    // Step 6 — Print summary and return.
    // -----------------------------------------------------------------------
    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << " Cook complete.\n";
    std::cout << "   Cooked  : " << cooked  << "\n";
    std::cout << "   Skipped : " << skipped << "\n";
    std::cout << "   Errors  : " << errors  << "\n";
    std::cout << "   AssetDB : " << assetDbPath.filename().string() << "  ("
              << dbEntries.size() << " entries)\n";
    std::cout << "============================================================\n";

    return (errors > 0) ? 1 : 0;
}
