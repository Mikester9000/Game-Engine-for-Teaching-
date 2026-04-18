/**
 * @file asset_loader.hpp
 * @brief AssetLoader — reads cooked asset file bytes from disk.
 *
 * ============================================================================
 * TEACHING NOTE — Synchronous vs Asynchronous Loading
 * ============================================================================
 * This M2 implementation is deliberately *synchronous*:
 *   • Caller asks for an asset.
 *   • Loader reads the file immediately (blocks the caller).
 *   • Caller receives the bytes.
 *
 * Why synchronous for now?
 *   Synchronous loading is simple, easy to understand, and correct.  It lets
 *   us validate the full pipeline (cook → store → load → use) before adding
 *   the complexity of threading.
 *
 * The async loader (M7) will use a worker thread + lock-free queue:
 *   • Caller posts a load request (non-blocking).
 *   • Worker thread loads the file in the background.
 *   • Caller polls for completion each frame.
 *   • On completion, the caller receives the bytes via a callback or future.
 *
 * This staged approach is how real game engines evolve: correct first,
 * then optimise.
 *
 * ============================================================================
 * TEACHING NOTE — Why return std::vector<uint8_t>?
 * ============================================================================
 * Returning raw bytes (uint8_t = unsigned byte, range 0–255) is the most
 * general format — every file can be represented as bytes.  Higher-level
 * loaders (TextureLoader, AudioLoader, etc.) call LoadRaw and then parse
 * the bytes into typed structures.
 *
 * Alternatives:
 *   • std::string  — works but semantically wrong (suggests text data).
 *   • std::span    — zero-copy view, but ownership is unclear.
 *   • std::vector<uint8_t>  — owning container; clear semantics; correct.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 */

#pragma once

#include "asset_db.hpp"

#include <cstdint>   // uint8_t
#include <string>
#include <vector>

namespace engine::assets
{

// ============================================================================
// AssetLoader class
// ============================================================================

/**
 * @class AssetLoader
 * @brief Reads cooked asset data from disk, given an asset GUID.
 *
 * AssetLoader depends on an AssetDB instance to resolve GUIDs → file paths.
 * Construct it with a pointer to the AssetDB; the AssetDB must outlive the
 * AssetLoader.
 *
 * Typical usage:
 * @code
 *   engine::assets::AssetDB    db;
 *   db.Load("Cooked/assetdb.json");
 *
 *   engine::assets::AssetLoader loader(&db);
 *
 *   auto bytes = loader.LoadRaw("f3f044c7-2a10-4be8-980d-7ee517382415");
 *   if (bytes.empty()) {
 *       LOG_ERROR("Failed to load asset");
 *   } else {
 *       // bytes contains the raw file data — parse it as JSON, binary, etc.
 *   }
 * @endcode
 */
class AssetLoader
{
public:
    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------

    /**
     * @brief Construct an AssetLoader backed by the given AssetDB.
     *
     * TEACHING NOTE — Dependency Injection
     * We take a raw pointer to AssetDB rather than owning it.  This is
     * "dependency injection" — the caller provides the dependency, giving
     * them full control over its lifetime.  The loader is a *consumer*, not
     * an *owner*, of the database.
     *
     * Raw pointers are fine here because:
     *  1. Ownership is clear (caller owns the DB).
     *  2. The DB is not dynamically allocated on behalf of the loader.
     *  3. Using std::unique_ptr/shared_ptr here would impose unnecessary
     *     ownership semantics that do not reflect the actual relationship.
     *
     * @param db  Pointer to a loaded AssetDB.  Must not be null; must outlive
     *            this AssetLoader.
     */
    explicit AssetLoader(AssetDB* db);

    ~AssetLoader() = default;

    // Non-copyable: copying would duplicate the raw pointer without
    // communicating ownership intent.
    AssetLoader(const AssetLoader&)            = delete;
    AssetLoader& operator=(const AssetLoader&) = delete;

    AssetLoader(AssetLoader&&)            = default;
    AssetLoader& operator=(AssetLoader&&) = default;

    // -----------------------------------------------------------------------
    // Loading
    // -----------------------------------------------------------------------

    /**
     * @brief Load the cooked bytes of an asset identified by GUID.
     *
     * TEACHING NOTE — std::vector as a raw-bytes buffer
     * Reading a file into a std::vector<uint8_t> is the idiomatic C++17
     * pattern for binary file I/O:
     *
     *   std::ifstream f(path, std::ios::binary | std::ios::ate);
     *   auto size = f.tellg();           // file size from end position
     *   f.seekg(0);
     *   std::vector<uint8_t> buf(size);
     *   f.read(reinterpret_cast<char*>(buf.data()), size);
     *
     * std::ios::ate opens the file positioned at the end, so tellg() returns
     * the file size directly.  seekg(0) then rewinds to the beginning.
     *
     * @param id  Asset GUID.
     * @return Cooked file bytes, or empty vector if the asset is not found or
     *         the file cannot be opened.
     */
    std::vector<uint8_t> LoadRaw(const std::string& id) const;

    /**
     * @brief Load cooked bytes by direct file path (bypasses AssetDB lookup).
     *
     * TEACHING NOTE — Why offer both overloads?
     * LoadRaw(id) is the preferred production path — always reference assets
     * by GUID so that renames do not break the engine.
     * LoadRawByPath(path) is a developer convenience for tests and tools
     * that want to load a specific file without registering it.
     *
     * @param path  Absolute or relative path to the cooked file.
     * @return File bytes, or empty vector on failure.
     */
    std::vector<uint8_t> LoadRawByPath(const std::string& path) const;

private:
    // -----------------------------------------------------------------------
    // The AssetDB pointer — not owned by this class.
    // -----------------------------------------------------------------------
    AssetDB* m_db = nullptr;
};

} // namespace engine::assets
