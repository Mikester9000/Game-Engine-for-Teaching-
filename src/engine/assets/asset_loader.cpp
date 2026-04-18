// ============================================================================
// asset_loader.cpp — Synchronous Asset Loader implementation
// ============================================================================

#include "asset_loader.hpp"
#include "asset_db.hpp"

#include <fstream>
#include <iostream>
#include <iterator>

namespace engine::assets {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
AssetLoader::AssetLoader(const AssetDB* db, std::string rootDir)
    : m_db(db)
    , m_rootDir(std::move(rootDir))
{
    // TEACHING NOTE — rootDir normalisation
    // We ensure the root directory ends with a separator so that constructing
    // "rootDir + cookedRelPath" always produces a valid path.
    if (!m_rootDir.empty() &&
        m_rootDir.back() != '/' && m_rootDir.back() != '\\') {
        m_rootDir += '/';
    }
}

// ---------------------------------------------------------------------------
// Has
// ---------------------------------------------------------------------------
bool AssetLoader::Has(const std::string& id) const {
    return m_db && m_db->Has(id);
}

// ---------------------------------------------------------------------------
// LoadRaw
// ---------------------------------------------------------------------------
std::optional<std::vector<uint8_t>>
AssetLoader::LoadRaw(const std::string& id) const {
    if (!m_db) {
        std::cerr << "[AssetLoader] No AssetDB attached\n";
        return std::nullopt;
    }

    const std::string cookedRelPath = m_db->GetCookedPath(id);
    if (cookedRelPath.empty()) {
        std::cerr << "[AssetLoader] Unknown asset id: " << id << "\n";
        return std::nullopt;
    }

    // TEACHING NOTE — Path construction
    // We prepend rootDir (e.g. "samples/vertical_slice_project/") to the
    // relative cooked path stored in the AssetDB so that the engine can
    // load assets regardless of the working directory.
    const std::string fullPath = m_rootDir + cookedRelPath;

    std::ifstream ifs(fullPath, std::ios::binary);
    if (!ifs.is_open()) {
        std::cerr << "[AssetLoader] Cannot open cooked file: " << fullPath << "\n";
        return std::nullopt;
    }

    // Read entire file into a byte vector.
    // TEACHING NOTE — istreambuf_iterator
    // std::istreambuf_iterator<char> reads the raw stream buffer byte-by-byte
    // without any character conversion, making it the correct choice for
    // binary files (textures, audio, compiled shaders).
    std::vector<uint8_t> data(
        (std::istreambuf_iterator<char>(ifs)),
        std::istreambuf_iterator<char>()
    );
    return data;
}

// ---------------------------------------------------------------------------
// PreloadAll
// ---------------------------------------------------------------------------
std::size_t AssetLoader::PreloadAll() const {
    if (!m_db) return 0;

    std::size_t loaded = 0;
    for (const auto& [id, entry] : m_db->AllEntries()) {
        auto bytes = LoadRaw(id);
        if (bytes) {
            ++loaded;
        } else {
            std::cerr << "[AssetLoader] Failed to preload: " << id
                      << " (" << entry.name << ")\n";
        }
    }
    return loaded;
}

} // namespace engine::assets
