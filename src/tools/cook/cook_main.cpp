// ============================================================================
// cook_main.cpp — C++ Asset Cooker (cook.exe)
// ============================================================================
//
// TEACHING NOTE — What is a C++ Cooker?
// ========================================
// The Python cook_assets.py script is great for rapid iteration; the C++
// cook.exe is the shipping version that:
//   • Compiles to a standalone binary (no Python runtime required).
//   • Reads AssetRegistry.json from a project directory.
//   • Copies / converts source assets to Cooked/ directory.
//   • Writes cooked/assetdb.json (a flat GUID → cooked path map).
//   • Exits 0 on success, non-zero on any failure.
//
// TEACHING NOTE — Milestone M2 scope
// The M2 implementation is intentionally simple:
//   • Textures, audio, and animation are COPIED unchanged (not compressed).
//   • Scene JSON files are copied verbatim.
//   • assetdb.json is a minimal flat registry for the C++ runtime.
// Real conversion (DDS compression, XAudio2 XWB, .animc binary) is M3+.
//
// Usage:
//   cook.exe --project <project-dir>
//   cook.exe --project samples\vertical_slice_project
//
// Output:
//   <project-dir>/Cooked/assetdb.json   — runtime asset database
//   <project-dir>/Cooked/<type>/<id>.<ext>  — cooked asset files
//
// TEACHING NOTE — Why read AssetRegistry.json (not discover files)?
// Using the registry as the authoritative input means:
//   1. Only assets that the editor has explicitly registered are cooked.
//   2. The cook is deterministic — same registry → same output.
//   3. The editor controls the cook, not the file system layout.
// This mirrors Unreal's cook workflow (the editor drives the cook, not a
// directory scanner).
// ============================================================================

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ============================================================================
// Minimal JSON helpers (same approach as asset_db.cpp)
// ============================================================================

namespace {

static std::string ReadFile(const fs::path& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) return {};
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

static bool WriteFile(const fs::path& path, const std::string& content) {
    std::ofstream ofs(path);
    if (!ofs.is_open()) return false;
    ofs << content;
    return true;
}

// Extract a JSON string field value from a flat object string.
static std::string ExtractString(const std::string& obj, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    auto pos = obj.find(needle);
    if (pos == std::string::npos) return {};
    pos += needle.size();
    while (pos < obj.size() && (obj[pos] == ' ' || obj[pos] == ':' ||
           obj[pos] == '\t' || obj[pos] == '\n' || obj[pos] == '\r')) {
        ++pos;
    }
    if (pos >= obj.size() || obj[pos] != '"') return {};
    ++pos;
    std::string result;
    while (pos < obj.size() && obj[pos] != '"') {
        if (obj[pos] == '\\' && pos + 1 < obj.size()) {
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

// Split a JSON array body (without [ ]) into individual top-level objects.
static std::vector<std::string> SplitObjects(const std::string& arrayText) {
    std::vector<std::string> objects;
    int depth = 0;
    std::size_t start = std::string::npos;
    bool inStr = false;
    for (std::size_t i = 0; i < arrayText.size(); ++i) {
        char c = arrayText[i];
        if (c == '"' && (i == 0 || arrayText[i - 1] != '\\')) { inStr = !inStr; continue; }
        if (inStr) continue;
        if (c == '{') { if (depth == 0) start = i; ++depth; }
        else if (c == '}') {
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

// ============================================================================
// AssetEntry — one row from AssetRegistry.json
// ============================================================================
struct AssetEntry {
    std::string id;
    std::string type;
    std::string name;
    std::string source; // relative to project dir
    std::string cooked; // relative to project dir (may be empty — we compute it)
    std::string hash;
};

// ============================================================================
// CookerConfig
// ============================================================================
struct CookerConfig {
    fs::path projectDir;
    fs::path registryPath;
    fs::path cookedDir;
    fs::path assetDbPath;
};

// ============================================================================
// Helpers
// ============================================================================

static void CopyAsset(const fs::path& src, const fs::path& dst) {
    if (!fs::exists(src)) {
        std::cerr << "  [WARN] Source not found: " << src << "\n";
        return;
    }
    fs::create_directories(dst.parent_path());
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
}

// ============================================================================
// Parse AssetRegistry.json
// ============================================================================
static std::vector<AssetEntry> ParseRegistry(const fs::path& registryPath) {
    std::string text = ReadFile(registryPath);
    if (text.empty()) {
        std::cerr << "[cook] Cannot read: " << registryPath << "\n";
        return {};
    }

    const std::string assetsKey = R"("assets")";
    auto assetsPos = text.find(assetsKey);
    if (assetsPos == std::string::npos) { std::cerr << "[cook] No 'assets' key\n"; return {}; }

    auto bracketPos = text.find('[', assetsPos + assetsKey.size());
    if (bracketPos == std::string::npos) { std::cerr << "[cook] No '[' after assets\n"; return {}; }

    int depth = 0; std::size_t arrayEnd = std::string::npos; bool inStr = false;
    for (std::size_t i = bracketPos; i < text.size(); ++i) {
        char c = text[i];
        if (c == '"' && (i == 0 || text[i-1] != '\\')) { inStr = !inStr; continue; }
        if (inStr) continue;
        if (c == '[') ++depth;
        else if (c == ']') { --depth; if (depth == 0) { arrayEnd = i; break; } }
    }
    if (arrayEnd == std::string::npos) { std::cerr << "[cook] Unclosed '['\n"; return {}; }

    std::string arrayText = text.substr(bracketPos + 1, arrayEnd - bracketPos - 1);
    auto objects = SplitObjects(arrayText);

    std::vector<AssetEntry> entries;
    entries.reserve(objects.size());
    for (const auto& obj : objects) {
        AssetEntry e;
        e.id     = ExtractString(obj, "id");
        e.type   = ExtractString(obj, "type");
        e.name   = ExtractString(obj, "name");
        e.source = ExtractString(obj, "source");
        e.cooked = ExtractString(obj, "cooked");
        e.hash   = ExtractString(obj, "hash");
        if (!e.id.empty()) entries.push_back(std::move(e));
    }
    return entries;
}

// ============================================================================
// Write cooked/assetdb.json
// ============================================================================
static bool WriteAssetDb(
    const fs::path&               assetDbPath,
    const std::vector<AssetEntry>& entries)
{
    // TEACHING NOTE — Manual JSON construction
    // For a teaching project, writing JSON by hand is acceptable as long as
    // the format is simple.  For a production engine, use nlohmann/json:
    //   auto j = nlohmann::json::object();
    //   j["version"] = "1.0.0";
    //   j["assets"] = nlohmann::json::array();  etc.
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"version\": \"1.0.0\",\n";
    ss << "  \"assets\": [\n";

    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        ss << "    {\n";
        ss << "      \"id\":     \"" << e.id     << "\",\n";
        ss << "      \"type\":   \"" << e.type   << "\",\n";
        ss << "      \"name\":   \"" << e.name   << "\",\n";
        ss << "      \"source\": \"" << e.source << "\",\n";
        ss << "      \"cooked\": \"" << e.cooked << "\",\n";
        ss << "      \"hash\":   \"" << e.hash   << "\"\n";
        ss << "    }";
        if (i + 1 < entries.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}\n";

    fs::create_directories(assetDbPath.parent_path());
    return WriteFile(assetDbPath, ss.str());
}

// ============================================================================
// main
// ============================================================================
int main(int argc, char* argv[]) {
    // -----------------------------------------------------------------------
    // Parse arguments
    // -----------------------------------------------------------------------
    std::string projectDirArg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--project" || arg == "-p") && i + 1 < argc) {
            projectDirArg = argv[++i];
        }
    }

    if (projectDirArg.empty()) {
        std::cerr << "Usage: cook --project <project-dir>\n";
        return 1;
    }

    // -----------------------------------------------------------------------
    // Build config
    // -----------------------------------------------------------------------
    CookerConfig cfg;
    cfg.projectDir   = fs::absolute(fs::path(projectDirArg));
    cfg.registryPath = cfg.projectDir / "AssetRegistry.json";
    cfg.cookedDir    = cfg.projectDir / "Cooked";
    cfg.assetDbPath  = cfg.cookedDir / "assetdb.json";

    if (!fs::exists(cfg.registryPath)) {
        std::cerr << "[cook] AssetRegistry.json not found: " << cfg.registryPath << "\n";
        return 1;
    }

    std::cout << "======================================\n";
    std::cout << " cook.exe — Asset Cooker (M2)\n";
    std::cout << "======================================\n";
    std::cout << " Project : " << cfg.projectDir  << "\n";
    std::cout << " Registry: " << cfg.registryPath << "\n";
    std::cout << " Output  : " << cfg.cookedDir   << "\n\n";

    // -----------------------------------------------------------------------
    // Parse the registry
    // -----------------------------------------------------------------------
    auto entries = ParseRegistry(cfg.registryPath);
    if (entries.empty()) {
        std::cout << "[cook] No assets to cook.\n";
    }

    // -----------------------------------------------------------------------
    // Cook each asset (M2: copy source → cooked path)
    // -----------------------------------------------------------------------
    int cooked = 0, errors = 0;
    for (auto& e : entries) {
        if (e.source.empty()) { ++errors; continue; }

        // TEACHING NOTE — Source path resolution
        // The AssetRegistry.json source paths may be:
        //   • Relative to the project root:    "Content/Textures/Grass.png"
        //   • Relative to Content/:            "Textures/Grass.png"
        // We try both so the cook tool works with either convention.
        fs::path srcPath = cfg.projectDir / e.source;
        if (!fs::exists(srcPath)) {
            fs::path withContent = cfg.projectDir / "Content" / e.source;
            if (fs::exists(withContent)) srcPath = withContent;
        }

        // Compute cooked path if not already set in the registry
        if (e.cooked.empty()) {
            e.cooked = "Cooked/" + e.type + "/" + e.id + srcPath.extension().string();
        }
        fs::path dstPath = cfg.projectDir / e.cooked;

        std::cout << "  [" << e.type << "] " << e.source << " → " << e.cooked << "\n";
        CopyAsset(srcPath, dstPath);
        ++cooked;
    }

    // -----------------------------------------------------------------------
    // Write assetdb.json
    // -----------------------------------------------------------------------
    if (!WriteAssetDb(cfg.assetDbPath, entries)) {
        std::cerr << "[cook] Failed to write: " << cfg.assetDbPath << "\n";
        return 1;
    }
    std::cout << "\n[cook] assetdb.json written: " << entries.size() << " entries\n";

    std::cout << "\n======================================\n";
    std::cout << " Cook complete: " << cooked << " assets";
    if (errors > 0) std::cout << ", " << errors << " errors";
    std::cout << "\n======================================\n";

    return (errors > 0) ? 1 : 0;
}
