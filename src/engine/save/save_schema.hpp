/**
 * @file save_schema.hpp
 * @brief Version constants and migration record for the save-file format.
 *
 * ============================================================================
 * TEACHING NOTE — Why Versioning Save Files?
 * ============================================================================
 * Every time the game ships an update that changes which ECS components are
 * stored (or changes a component's fields), old save files may fail to load.
 * Save-file versioning solves this by embedding a format version number in
 * every save file.  The load path can then detect old versions and migrate
 * them forward:
 *
 *   "version": "1.0.0"  →  load normally
 *   "version": "0.9.0"  →  run migration: add new field with default value
 *   "version": "0.8.0"  →  run 0.8 → 0.9 migration, then 0.9 → 1.0 migration
 *
 * This is called a "migration chain" or "upgrade ladder".
 *
 * TEACHING NOTE — Semantic Versioning for Save Formats
 * ─────────────────────────────────────────────────────
 * We use SemVer (MAJOR.MINOR.PATCH) where:
 *
 *   MAJOR — breaking change: old save files CANNOT be loaded without migration.
 *            Example: renaming a required field, removing a component.
 *
 *   MINOR — additive change: new fields added with default values.
 *            Old saves load fine (new fields get defaults); new saves can't
 *            be read by old game versions.
 *
 *   PATCH — bug fix to the save/load path that doesn't change the file layout.
 *            Old and new files are fully interchangeable.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0.0
 * @date    2025
 */

#pragma once

#include <cstdint>
#include <string_view>

namespace engine {
namespace save {

// ============================================================================
// Current save-format version
// ============================================================================

/// The version string embedded in every save file.
/// Bump this when any component stored by SaveSystem changes.
inline constexpr std::string_view kSaveFormatVersion = "1.0.0";

// ============================================================================
// Slot constants
// ============================================================================

/// TEACHING NOTE — 15 numbered save slots + 1 auto-save.
/// 15 slots mirrors the convention of many JRPG consoles where memory cards
/// held a fixed number of blocks.  The auto-save slot (index 15) is written
/// automatically at camp rest and before dangerous encounters.
inline constexpr int kNumSaveSlots  = 15;  ///< Numbered slots 0–14.
inline constexpr int kAutoSaveSlot  = 15;  ///< Slot index for the auto-save.
inline constexpr int kTotalSlots    = 16;  ///< kNumSaveSlots + auto-save.

// ============================================================================
// Component type identifiers (embedded in JSON for round-trip fidelity)
// ============================================================================

/// TEACHING NOTE — String type tags in JSON saves
/// Each component stored in the save file is tagged with a human-readable
/// type string.  This makes saves human-inspectable (open in a text editor)
/// and allows the loader to skip components it doesn't recognise rather than
/// failing loudly.  It also future-proofs the format: if a component is
/// removed from the game, old saves containing it simply ignore the orphan.

inline constexpr std::string_view kTagTransform    = "Transform";
inline constexpr std::string_view kTagHealth       = "Health";
inline constexpr std::string_view kTagStats        = "Stats";
inline constexpr std::string_view kTagName         = "Name";
inline constexpr std::string_view kTagMovement     = "Movement";
inline constexpr std::string_view kTagCombat       = "Combat";
inline constexpr std::string_view kTagInventory    = "Inventory";
inline constexpr std::string_view kTagQuest        = "Quest";
inline constexpr std::string_view kTagAI           = "AI";
inline constexpr std::string_view kTagMagic        = "Magic";
inline constexpr std::string_view kTagEquipment    = "Equipment";
inline constexpr std::string_view kTagLevel        = "Level";
inline constexpr std::string_view kTagCurrency     = "Currency";
inline constexpr std::string_view kTagCamera       = "Camera";

} // namespace save
} // namespace engine
