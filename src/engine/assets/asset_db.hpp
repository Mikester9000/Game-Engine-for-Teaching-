/**
 * @file asset_db.hpp
 * @brief AssetDB — runtime lookup table that maps asset GUIDs to cooked paths.
 *
 * ============================================================================
 * TEACHING NOTE — What is an Asset Database?
 * ============================================================================
 * In a game engine, "assets" are data files: textures, audio clips, meshes,
 * animation clips, scenes, etc.  An asset database (AssetDB) is a lookup
 * table that answers the question:
 *
 *   "Given an asset GUID, where is its cooked (runtime-ready) file?"
 *
 * Why GUIDs instead of file paths?
 *   • Paths change when a file is moved or renamed; GUIDs never change.
 *   • Two files can have the same name in different directories; GUIDs are
 *     globally unique, so there is never ambiguity.
 *   • The engine always refers to assets by GUID in scene files, scripts, and
 *     code, so renaming a file on disk does not break anything.
 *
 * How does it work at runtime?
 *   1. At engine startup (or project load), call AssetDB::Load() with the
 *      path to the cooked assetdb.json file.
 *   2. AssetDB parses the JSON and builds an in-memory hash map:
 *        GUID (string) → cooked file path (string)
 *   3. Any system that needs a file calls AssetDB::GetCookedPath(guid).
 *
 * ============================================================================
 * TEACHING NOTE — assetdb.json vs AssetRegistry.json
 * ============================================================================
 * AssetRegistry.json  — source-of-truth registry maintained by the editors
 *                         and cook scripts.  Contains source paths, hashes,
 *                         and metadata.  Read by the cooker.
 *
 * assetdb.json        — lightweight runtime index produced by the cooker.
 *                         Contains only what the engine needs at load time:
 *                         { id → cooked_path }.  Read by AssetDB::Load().
 *
 * Keeping these separate means the runtime never has to parse large metadata
 * blobs that are only relevant to the editor pipeline.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 */

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

// ============================================================================
// TEACHING NOTE — Forward-declaring vs full include
// ============================================================================
// This header includes only the standard library headers needed for the
// public interface: std::string, std::unordered_map, std::vector.
// We deliberately avoid including heavy headers (json, filesystem) here —
// those are implementation details in asset_db.cpp.
// Keeping headers lightweight makes compilation faster and reduces coupling.
// ============================================================================

namespace engine::assets
{

// ============================================================================
// AssetDB class
// ============================================================================

/**
 * @class AssetDB
 * @brief Maps stable asset GUIDs → cooked file paths for runtime use.
 *
 * Typical usage:
 * @code
 *   engine::assets::AssetDB db;
 *   if (!db.Load("samples/vertical_slice_project/Cooked/assetdb.json")) {
 *       LOG_ERROR("AssetDB failed to load");
 *       return false;
 *   }
 *
 *   if (db.Has("f3f044c7-2a10-4be8-980d-7ee517382415")) {
 *       std::string path = db.GetCookedPath("f3f044c7-2a10-4be8-980d-7ee517382415");
 *       // path == "Cooked/Maps/MainTown.scene.json"
 *   }
 * @endcode
 */
class AssetDB
{
public:
    // -----------------------------------------------------------------------
    // Construction / destruction (default — no resources until Load() called)
    // -----------------------------------------------------------------------
    AssetDB()  = default;
    ~AssetDB() = default;

    // Non-copyable: we do not want two DBs with the same data accidentally.
    // TEACHING NOTE — Rule of Zero
    // Because this class owns only an std::unordered_map (which manages its
    // own memory), the compiler-generated copy/move/destructor are all correct
    // by default.  We only need to delete copy operations to express intent.
    AssetDB(const AssetDB&)            = delete;
    AssetDB& operator=(const AssetDB&) = delete;

    // Move is fine — lets you std::move an AssetDB into a larger context.
    AssetDB(AssetDB&&)            = default;
    AssetDB& operator=(AssetDB&&) = default;

    // -----------------------------------------------------------------------
    // Loading
    // -----------------------------------------------------------------------

    /**
     * @brief Parse assetdb.json and populate the in-memory map.
     *
     * TEACHING NOTE — Why parse at load time?
     * Parsing is a one-time O(N) cost at startup.  After that every lookup
     * is O(1) because std::unordered_map uses a hash table internally.
     * This pattern — "pay the cost once, amortise over many lookups" — is
     * fundamental to game engine asset management.
     *
     * TEACHING NOTE — Base directory derivation
     * assetdb.json lives at <projectRoot>/Cooked/assetdb.json.
     * The cooker writes cookedPath values relative to the project root
     * (e.g. "Cooked/Maps/Town.json").  During Load() we derive the project
     * root as the grandparent of assetDbPath and store it as m_baseDir.
     * GetCookedPath() then joins m_baseDir with the stored relative path,
     * producing an absolute path that resolves correctly regardless of the
     * caller's working directory.
     *
     * @param assetDbPath  Absolute or relative path to assetdb.json.
     * @return true on success; false if the file cannot be opened or parsed.
     */
    bool Load(const std::string& assetDbPath);

    /**
     * @brief Clear all entries and reset the DB to an empty state.
     *
     * Call this before reloading or when switching projects.
     */
    void Clear();

    // -----------------------------------------------------------------------
    // Querying
    // -----------------------------------------------------------------------

    /**
     * @brief Return true if the given GUID is present in the database.
     * @param id  UUID v4 string (e.g. "f3f044c7-2a10-4be8-980d-7ee517382415").
     */
    bool Has(const std::string& id) const;

    /**
     * @brief Return the cooked file path for the given GUID.
     *
     * TEACHING NOTE — Absolute path returned
     * The path returned is always absolute (or at minimum resolved from the
     * project root stored during Load()).  This means callers do not need to
     * know the current working directory — the path works from any location.
     *
     * Callers should always call Has() first.  If the id is not found, this
     * function returns an empty string and logs an error.
     *
     * @param id  UUID v4 string.
     * @return Absolute cooked file path, or empty string if not found.
     */
    std::string GetCookedPath(const std::string& id) const;

    /**
     * @brief Return all GUIDs currently in the database.
     *
     * TEACHING NOTE — Enabling iteration without exposing internals
     * Rather than exposing the raw iterator pair of the internal hash map,
     * we return a snapshot vector.  This keeps the API stable if the internal
     * data structure changes (e.g. from unordered_map to a sorted map).
     * The cost is a one-time O(N) copy — acceptable for the rare cases where
     * the full list is needed (validation, debugging).
     *
     * @return Vector of all asset GUIDs in insertion-independent order.
     */
    std::vector<std::string> All() const;

    /**
     * @brief Return the total number of assets in the database.
     * @return Asset count.
     */
    std::size_t Count() const;

private:
    // -----------------------------------------------------------------------
    // TEACHING NOTE — std::unordered_map
    // -----------------------------------------------------------------------
    // std::unordered_map<Key, Value> is a hash table.  It provides:
    //   • O(1) average insert, lookup, erase.
    //   • O(N) worst-case (all keys hash to the same bucket — very rare).
    //
    // We use std::string keys (GUIDs) and std::string values (paths).
    // The std::hash<std::string> specialisation handles hashing automatically.
    // -----------------------------------------------------------------------
    std::unordered_map<std::string, std::string> m_idToPath;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Base directory for path resolution
    // -----------------------------------------------------------------------
    // assetdb.json lives at <projectRoot>/Cooked/assetdb.json.
    // cookedPath values stored in the JSON are relative to projectRoot.
    // m_baseDir is set to the absolute projectRoot during Load() so that
    // GetCookedPath() can return absolute paths regardless of the caller's
    // working directory.
    // -----------------------------------------------------------------------
    std::string m_baseDir;
};

} // namespace engine::assets
