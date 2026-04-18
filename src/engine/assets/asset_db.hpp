#pragma once
// ============================================================================
// asset_db.hpp — Runtime Asset Database
// ============================================================================
//
// TEACHING NOTE — Asset Database Pattern
// =========================================
// In a game engine the "asset database" (AssetDB) is the runtime equivalent
// of the editor's asset registry.  It answers one central question:
//
//   "Given a stable GUID, where is the cooked file?"
//
// Design decisions here:
//   • Loaded once at startup from  assetdb.json  (written by cook.exe / cook_assets.py).
//   • Immutable after load — no hot-reload in this teaching build.
//   • O(1) lookup via  std::unordered_map<string, Entry>.
//   • No external dependencies — uses only <string>, <unordered_map>,
//     and our own minimal JSON reader so this file compiles without vcpkg.
//
// TEACHING NOTE — Why a separate AssetDB vs. just reading AssetRegistry.json?
// AssetRegistry.json is the *source-of-truth* manifest owned by the editor.
// assetdb.json is the *cook output* — a flat, engine-friendly snapshot that
// maps GUID → cooked path with no editor metadata.  Keeping them separate
// mirrors the AAA pipeline (e.g. Unreal's DerivedDataCache).
// ============================================================================

#include <string>
#include <unordered_map>
#include <vector>

namespace engine::assets {

// ----------------------------------------------------------------------------
// AssetEntry — metadata for a single cooked asset
// ----------------------------------------------------------------------------
struct AssetEntry {
    std::string id;      ///< Stable UUID v4.
    std::string type;    ///< "texture" | "audio_bank" | "scene" | "anim_clip" | …
    std::string name;    ///< Human-readable name (usually the file stem).
    std::string source;  ///< Relative path to the raw source asset.
    std::string cooked;  ///< Relative path to the cooked/runtime-ready file.
    std::string hash;    ///< SHA-256 hex digest of the source at cook time.
    std::vector<std::string> tags; ///< Free-form labels for pipeline filtering.
};

// ----------------------------------------------------------------------------
// AssetDB
// ----------------------------------------------------------------------------
/// Runtime-readonly asset catalogue loaded from  assetdb.json.
///
/// Usage:
/// @code
///   AssetDB db;
///   if (!db.Load("Cooked/assetdb.json")) { LOG_ERROR("AssetDB failed"); }
///   std::string path = db.GetCookedPath("f3f044c7-...");  // → "Cooked/..."
/// @endcode
class AssetDB {
public:
    // -----------------------------------------------------------------------
    // Construction / loading
    // -----------------------------------------------------------------------

    AssetDB() = default;

    /// Load the cooked asset database from @p assetDbPath.
    ///
    /// @param assetDbPath  Path to  assetdb.json  (absolute or relative to CWD).
    /// @returns            true on success, false on any file or parse error.
    ///
    /// TEACHING NOTE — Error handling strategy
    /// We return bool (not throw) to keep the engine startup robust; a
    /// missing or corrupt asset database is logged but non-fatal so that
    /// the engine can still enter a "no-assets" diagnostic mode.
    bool Load(const std::string& assetDbPath);

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------

    /// Return the cooked file path for @p id, or "" if not found.
    [[nodiscard]] std::string GetCookedPath(const std::string& id) const;

    /// Return the full entry for @p id, or nullptr if not found.
    [[nodiscard]] const AssetEntry* GetEntry(const std::string& id) const;

    /// Return true if an asset with @p id is registered.
    [[nodiscard]] bool Has(const std::string& id) const;

    /// Return the number of registered assets.
    [[nodiscard]] std::size_t Count() const { return m_entries.size(); }

    /// Return all entries (for iteration / diagnostics).
    [[nodiscard]] const std::unordered_map<std::string, AssetEntry>& AllEntries() const {
        return m_entries;
    }

    /// Return the path this database was loaded from.
    [[nodiscard]] const std::string& DbPath() const { return m_dbPath; }

private:
    std::unordered_map<std::string, AssetEntry> m_entries;
    std::string m_dbPath;

    // TEACHING NOTE — Why a private helper?
    // Parsing JSON manually without a library produces verbose but readable
    // code.  We isolate the parsing in a private helper so the public API
    // stays clean and the parsing details don't leak into callers.
    bool ParseJson(const std::string& jsonText);
};

} // namespace engine::assets
