/**
 * @file asset_loader.cpp
 * @brief AssetLoader implementation — reads cooked asset bytes from disk.
 *
 * ============================================================================
 * TEACHING NOTE — Binary File I/O in C++
 * ============================================================================
 * Opening a file with std::ios::binary is essential for non-text assets
 * (textures, audio, compiled shaders).  Without it, on Windows, the runtime
 * library translates "\r\n" line endings to "\n", corrupting binary data.
 *
 * The pattern used here:
 *
 *   std::ifstream f(path, std::ios::binary | std::ios::ate);
 *   // std::ios::ate (At The End) opens the file with the read position
 *   // at the end.  f.tellg() then returns the file size directly.
 *
 *   auto size = static_cast<std::streamsize>(f.tellg());
 *   f.seekg(0);    // Rewind to beginning.
 *
 *   std::vector<uint8_t> buf(size);
 *   f.read(reinterpret_cast<char*>(buf.data()), size);
 *   // std::ifstream::read takes char*, but our buffer is uint8_t*.
 *   // reinterpret_cast is safe here: char and uint8_t are both 1-byte types.
 *
 * Why pre-size the vector?
 * Resizing first allocates the exact amount needed in one allocation.
 * Using push_back/insert would trigger multiple re-allocations as the vector
 * grows — wasteful for known-size reads.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 */

#include "asset_loader.hpp"
#include "engine/core/Logger.hpp"

#include <fstream>
#include <ios>

namespace engine::assets
{

// ----------------------------------------------------------------------------
AssetLoader::AssetLoader(AssetDB* db)
    : m_db(db)
{
    // TEACHING NOTE — Precondition assertion style
    // In development builds we want to catch programming errors immediately.
    // Using a LOG_ERROR + early return is gentler than a hard assert but still
    // makes the problem visible in the log.  A shipping engine might use
    // [[likely]]/[[unlikely]] hints or assert() with a descriptive message.
    if (!m_db)
    {
        LOG_ERROR("AssetLoader constructed with null AssetDB pointer.");
    }
}

// ----------------------------------------------------------------------------
std::vector<uint8_t> AssetLoader::LoadRaw(const std::string& id) const
{
    if (!m_db)
    {
        LOG_ERROR("AssetLoader::LoadRaw — AssetDB pointer is null.");
        return {};
    }

    if (!m_db->Has(id))
    {
        LOG_ERROR("AssetLoader::LoadRaw — GUID not in AssetDB: " << id);
        return {};
    }

    const std::string path = m_db->GetCookedPath(id);
    return LoadRawByPath(path);
}

// ----------------------------------------------------------------------------
std::vector<uint8_t> AssetLoader::LoadRawByPath(const std::string& path) const
{
    // Open in binary mode, seeked to end so we can query file size via tellg().
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open())
    {
        LOG_ERROR("AssetLoader::LoadRawByPath — cannot open file: " << path);
        return {};
    }

    // Determine file size.
    // TEACHING NOTE — tellg() after std::ios::ate
    // When a file is opened with std::ios::ate, the initial read position
    // is at the end of the file.  tellg() (tell get-position) therefore
    // returns the file size in bytes.
    const auto fileSize = f.tellg();
    if (fileSize <= 0)
    {
        LOG_WARN("AssetLoader::LoadRawByPath — file is empty: " << path);
        return {};
    }

    // Rewind and read.
    f.seekg(0);
    std::vector<uint8_t> buf(static_cast<std::size_t>(fileSize));
    f.read(reinterpret_cast<char*>(buf.data()),
           static_cast<std::streamsize>(fileSize));

    if (!f)
    {
        LOG_ERROR("AssetLoader::LoadRawByPath — read error on file: " << path);
        return {};
    }

    LOG_DEBUG("AssetLoader::LoadRawByPath — loaded "
              << buf.size() << " bytes from " << path);
    return buf;
}

} // namespace engine::assets
