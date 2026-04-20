/**
 * @file save_system.hpp
 * @brief Production save system — 15 slots + auto-save — M8.8.
 *
 * ============================================================================
 * TEACHING NOTE — Save System Architecture
 * ============================================================================
 * A save system needs to solve three problems:
 *
 *   1. SERIALISATION   — Convert runtime ECS World state to storable bytes.
 *   2. STORAGE         — Where do the bytes go? (file, cloud, memory card).
 *   3. VERSIONING      — Detect format mismatches and migrate old saves.
 *
 * ─── Serialisation Strategy ─────────────────────────────────────────────────
 * We walk every living entity in the World and for each one, inspect which
 * components it has.  Each component is serialised to JSON using nlohmann/json.
 *
 * The JSON schema for one save file:
 *
 *   {
 *     "version": "1.0.0",
 *     "savedAt": "2025-01-15T12:34:56Z",
 *     "gameTimeSecs": 3600.5,
 *     "entities": [
 *       {
 *         "id": 1,
 *         "components": {
 *           "Transform": { "px": 3200.0, "py": 0.0, "pz": 3200.0 },
 *           "Health":    { "hp": 450, "maxHp": 500, "mp": 120, "maxMp": 150 },
 *           "Quest":     { "quests": [...] }
 *         }
 *       },
 *       …
 *     ]
 *   }
 *
 * ─── Storage Strategy ───────────────────────────────────────────────────────
 * Files are written to a "SavedGames/" subdirectory next to the executable.
 *
 *   SavedGames/save_0.json      ← slot 0
 *   SavedGames/save_14.json     ← slot 14
 *   SavedGames/save_auto.json   ← auto-save
 *
 * ─── Versioning / Migration ─────────────────────────────────────────────────
 * When loading, the "version" field is compared to kSaveFormatVersion.
 * If they differ, a migration table is consulted.  Migrations are simple
 * lambdas that transform the JSON blob before component deserialisation.
 *
 * ─── Build-time Gate ─────────────────────────────────────────────────────────
 * All JSON code is gated by ENGINE_ENABLE_JSON (requires nlohmann/json via
 * vcpkg).  When not available, Save() and Load() return false with a message.
 * This matches the pattern used by SceneSerialiser (M6).
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2025
 * C++ Standard: C++17
 */

#pragma once

#include <string>
#include <cstdint>

#include "engine/ecs/ECS.hpp"
#include "engine/save/save_schema.hpp"

namespace engine {
namespace save {

// ===========================================================================
// SaveMetadata — a brief description of one slot (shown in the load menu)
// ===========================================================================

/**
 * @struct SaveMetadata
 * @brief Lightweight summary of one save slot (does NOT load the whole World).
 *
 * TEACHING NOTE — Lazy Loading
 * ──────────────────────────────
 * Loading a full World state can be expensive (many entities, large maps).
 * The load-game menu only needs a brief summary: player name, game time, date.
 * SaveMetadata lets us load just that header from the file without parsing the
 * full entity list.
 */
struct SaveMetadata
{
    bool        exists       = false;  ///< True if a save file exists for this slot.
    int         slot         = -1;     ///< Slot index (0–14, or 15 for auto-save).
    std::string savedAt;               ///< ISO 8601 timestamp when this save was written.
    float       gameTimeSecs = 0.0f;   ///< Accumulated game time at save point (seconds).
    std::string playerName;            ///< Name of the player character.
    int         playerLevel  = 1;      ///< Player level at save point.
    std::string locationName;          ///< Current zone/location name.
    std::string version;               ///< Save format version string.
};

// ===========================================================================
// SaveSystem
// ===========================================================================

/**
 * @class SaveSystem
 * @brief Serialises and deserialises ECS World state to/from JSON save files.
 *
 * Usage:
 * @code
 *   engine::save::SaveSystem saver;
 *   saver.SetSaveDirectory("SavedGames/");
 *
 *   // --- Save ---
 *   bool ok = saver.Save(world, playerID, slot, gameTimeSecs, locationName);
 *
 *   // --- Auto-save ---
 *   saver.AutoSave(world, playerID, gameTimeSecs, locationName);
 *
 *   // --- List saves (load menu) ---
 *   auto metas = saver.ReadAllMetadata();
 *
 *   // --- Load ---
 *   bool ok = saver.Load(world, slot);
 * @endcode
 */
class SaveSystem
{
public:

    /**
     * @brief Construct SaveSystem.
     * @param saveDir  Directory path for save files (created if absent).
     *                 Defaults to "SavedGames/" next to the executable.
     */
    explicit SaveSystem(const std::string& saveDir = "SavedGames/");
    ~SaveSystem() = default;

    // Non-copyable — one save system per game session.
    SaveSystem(const SaveSystem&)            = delete;
    SaveSystem& operator=(const SaveSystem&) = delete;

    // =========================================================================
    // Directory configuration
    // =========================================================================

    /**
     * @brief Override the save file directory.
     * @param dir  Path including trailing slash (created if absent).
     */
    void SetSaveDirectory(const std::string& dir);

    // =========================================================================
    // Save
    // =========================================================================

    /**
     * @brief Serialise the ECS World to slot `slot`.
     *
     * Writes all living entities and their components to JSON.
     * Overwrites any existing file in that slot.
     *
     * @param world         ECS World to serialise.
     * @param playerID      Player entity (name/level read for metadata).
     * @param slot          Slot index 0–14 (kAutoSaveSlot = 15 for auto-save).
     * @param gameTimeSecs  Accumulated game time (stored in metadata).
     * @param locationName  Human-readable location (stored in metadata).
     * @return true on success.
     *
     * TEACHING NOTE — ENGINE_ENABLE_JSON gate
     * ──────────────────────────────────────────
     * All nlohmann/json calls are inside #ifdef ENGINE_ENABLE_JSON.
     * When the macro is not defined (vcpkg not configured), Save() returns
     * false and logs a message — the game still runs, just without saves.
     */
    bool Save(World& world, EntityID playerID,
              int slot, float gameTimeSecs,
              const std::string& locationName = "Unknown");

    /**
     * @brief Convenience wrapper: save to the auto-save slot.
     */
    bool AutoSave(World& world, EntityID playerID,
                  float gameTimeSecs,
                  const std::string& locationName = "Unknown");

    // =========================================================================
    // Load
    // =========================================================================

    /**
     * @brief Deserialise a save file into an existing ECS World.
     *
     * Destroys all existing entities, recreates them from the save file,
     * and re-registers all components.
     *
     * @param world  ECS World to populate.  All existing entities are removed.
     * @param slot   Slot index 0–14, or kAutoSaveSlot (15) for auto-save.
     * @return true on success; false if file is missing or format mismatch
     *         that could not be migrated.
     *
     * TEACHING NOTE — Load = Destroy + Recreate
     * ──────────────────────────────────────────
     * Rather than patching an existing World in-place (complicated and fragile),
     * we start fresh: destroy all entities, then recreate each entity from the
     * JSON data.  This is safe because the World's component pools are reset
     * to zero, guaranteeing a clean baseline.
     */
    bool Load(World& world, int slot);

    // =========================================================================
    // Metadata
    // =========================================================================

    /**
     * @brief Read the metadata header of one slot (fast, no full parse).
     * @param slot  0–15.
     */
    SaveMetadata ReadMetadata(int slot) const;

    /**
     * @brief Read metadata for all slots simultaneously.
     * @return Vector of kTotalSlots SaveMetadata entries, indexed by slot.
     */
    std::vector<SaveMetadata> ReadAllMetadata() const;

    /**
     * @brief Return true if a save file exists for the given slot.
     */
    bool SlotExists(int slot) const;

    /**
     * @brief Delete the save file for the given slot.
     * @return true if the file was deleted or did not exist.
     */
    bool DeleteSlot(int slot) const;

private:
    // -------------------------------------------------------------------------
    // Internal helpers
    // -------------------------------------------------------------------------

    /// Return the full file path for a given slot.
    std::string SlotPath(int slot) const;

    std::string m_saveDir;  ///< Directory that holds all save files.
};

} // namespace save
} // namespace engine
