#pragma once
// ============================================================================
// asset_loader.hpp — Synchronous Asset Loader
// ============================================================================
//
// TEACHING NOTE — AssetLoader Design (M2 synchronous stub)
// ==========================================================
// The AssetLoader is the bridge between the AssetDB (which maps GUIDs to
// file paths) and the rest of the engine (which wants raw bytes or typed
// objects).
//
// M2 design — synchronous, blocking:
//   • LoadRaw(id)  reads the cooked file from disk into a byte vector.
//   • Simple, easy to understand, works on all platforms.
//   • The engine calls this once per asset at startup (not per-frame).
//
// M7 upgrade path — async, streaming:
//   • Replace LoadRaw with an async request queue processed on a worker thread.
//   • The interface (LoadRaw + Has) stays the same; callers need no change.
//   • See  src/engine/world/async_loader.hpp  (future M7 work).
//
// TEACHING NOTE — Why return std::vector<uint8_t> (raw bytes)?
// The loader deliberately does NOT know how to deserialise the bytes — that
// is the job of per-type importers (TextureImporter, AudioImporter, …).
// Separation of concerns:
//   • AssetLoader = "give me the raw cooked bytes"
//   • *Importer   = "interpret these bytes as a Texture / AudioClip / …"
//
// This pattern is used in Unreal (FAssetData / UObject), Unity (AssetBundle /
// ScriptableObject), and most other engines.
// ============================================================================

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace engine::assets {

class AssetDB;  // forward declaration — avoid including asset_db.hpp here

// ----------------------------------------------------------------------------
/// Synchronous, blocking asset loader (M2 implementation).
///
/// Usage:
/// @code
///   AssetDB db;
///   db.Load("Cooked/assetdb.json");
///   AssetLoader loader(&db, "samples/vertical_slice_project/");
///   auto bytes = loader.LoadRaw("f3f044c7-...");
///   if (bytes) { /* bytes->data(), bytes->size() */ }
/// @endcode
// ----------------------------------------------------------------------------
class AssetLoader {
public:
    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------

    /// Construct a loader that resolves cooked paths via @p db.
    ///
    /// @param db        Pointer to a loaded AssetDB.  Must outlive the loader.
    /// @param rootDir   Directory that cooked paths are relative to.
    ///                  Typically the project root (ends with '/').
    explicit AssetLoader(const AssetDB* db, std::string rootDir = "");

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------

    /// Return true if @p id is registered in the database.
    [[nodiscard]] bool Has(const std::string& id) const;

    // -----------------------------------------------------------------------
    // Loading
    // -----------------------------------------------------------------------

    /// Load the cooked bytes for @p id from disk.
    ///
    /// @returns  Byte vector on success, std::nullopt if the asset is unknown
    ///           or the file cannot be read.
    ///
    /// TEACHING NOTE — std::optional vs. exceptions
    /// We return optional<> rather than throwing to match the engine-wide
    /// convention of "return a sentinel on failure, log, continue".
    /// Throwing across asset loads makes recovery logic harder to reason about.
    [[nodiscard]] std::optional<std::vector<uint8_t>>
    LoadRaw(const std::string& id) const;

    /// Load all registered assets and return the count successfully loaded.
    ///
    /// TEACHING NOTE — Bulk preload
    /// Calling LoadRaw in a loop at startup avoids the per-frame I/O cost.
    /// For M2 (synchronous load) this blocks the main thread.  In M7 this
    /// will be replaced by async streaming.
    std::size_t PreloadAll() const;

private:
    const AssetDB* m_db;       ///< Non-owning pointer; caller manages lifetime.
    std::string    m_rootDir;  ///< Prepended to all cooked paths.
};

} // namespace engine::assets
