/**
 * @file InventorySystem.hpp
 * @brief Item management system for the educational RPG engine.
 *
 * ============================================================================
 * TEACHING NOTE — Inventory System Design
 * ============================================================================
 *
 * An inventory system has more depth than it first appears.  Key concerns:
 *
 * ─── Storage Model ───────────────────────────────────────────────────────────
 *
 * We use a SLOT-BASED inventory: the player has MAX_INV_SLOTS (99) slots,
 * each holding one item stack (itemID + quantity).  This matches classic
 * JRPG inventories (FF7-style) and is easy to render in a grid UI.
 *
 * Alternative: WEIGHT-BASED (Skyrim, Diablo).  Each item has a weight value;
 * the player has a carry limit.  More realistic, but requires more UI work.
 *
 * ─── Equipment Slots ─────────────────────────────────────────────────────────
 *
 * Equipment is separate from the general inventory.  When an item is equipped:
 *   1. It is removed from the inventory slot.
 *   2. Its item ID is stored in the appropriate EquipmentComponent field.
 *   3. RecalculateEquipStats() reads all equipped item stat bonuses and writes
 *      them into EquipmentComponent::bonusXxx fields (cached stats).
 *
 * On each damage calculation, the CombatSystem reads the CACHED bonuses from
 * EquipmentComponent — it doesn't re-scan equipment on every hit.  The cache
 * is marked dirty (EquipmentComponent::statsDirty) when equipment changes.
 *
 * ─── EquipSlot enum ──────────────────────────────────────────────────────────
 *
 * We define EquipSlot here (not in Types.hpp) because it is an implementation
 * detail of the inventory/equipment system.  Types.hpp holds vocabulary types
 * shared across ALL subsystems.  EquipSlot is only needed by InventorySystem,
 * UI, and serialisation — not by Combat or AI.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 *
 * C++ Standard: C++17
 */

#pragma once

#include <cstdint>   // uint32_t
#include <string>    // std::string

#include "../../engine/core/Types.hpp"
#include "../../engine/core/Logger.hpp"
#include "../../engine/core/EventBus.hpp"
#include "../../engine/ecs/ECS.hpp"
#include "../GameData.hpp"


// ===========================================================================
// EquipSlot — which equipment slot an item occupies
// ===========================================================================

/**
 * @enum EquipSlot
 * @brief Identifies one of the seven equipment slots on a character.
 *
 * TEACHING NOTE — Enum as a Switch/Array Index
 * ──────────────────────────────────────────────
 * Using `enum class` here means we can use EquipSlot values as switch-case
 * labels or as array indices (after a static_cast<int>) without ambiguity.
 * The `COUNT` sentinel at the end gives the number of valid slots:
 *
 *   std::array<uint32_t, (int)EquipSlot::COUNT> slotItems;
 *
 * This is a common C++ pattern to size arrays from enums.
 */
enum class EquipSlot : uint8_t {
    WEAPON     = 0,  ///< Main-hand weapon (sword, lance, gun, etc.).
    OFFHAND    = 1,  ///< Off-hand (shield, accessory, or secondary weapon).
    HEAD       = 2,  ///< Helmet or hat — primarily defence.
    BODY       = 3,  ///< Chest armour — highest defence value.
    LEGS       = 4,  ///< Leg armour — often speed bonus.
    ACCESSORY1 = 5,  ///< First ring/amulet — varied stat bonuses.
    ACCESSORY2 = 6,  ///< Second ring/amulet.
    COUNT      = 7   ///< Sentinel: number of valid slots.
};


// ===========================================================================
// InventorySystem class
// ===========================================================================

/**
 * @class InventorySystem
 * @brief Manages item acquisition, removal, equipping, and usage.
 *
 * The InventorySystem acts as the INTERFACE between the game world and an
 * entity's InventoryComponent / EquipmentComponent.  Direct manipulation of
 * those components is possible but discouraged — go through this system to
 * get events, validation, and stat recalculation for free.
 *
 * USAGE EXAMPLE:
 * @code
 *   InventorySystem inv(world, &EventBus<UIEvent>::Instance());
 *
 *   // Pick up 3 Potions (itemID = 1):
 *   inv.AddItem(playerID, 1, 3);
 *
 *   // Use one Potion on the player:
 *   inv.UseItem(playerID, 1);
 *
 *   // Equip a sword (itemID = 10) to the weapon slot:
 *   inv.EquipItem(playerID, 10);
 * @endcode
 */
class InventorySystem {
public:

    /**
     * @brief Construct the inventory system.
     *
     * @param world   Non-owning pointer to the ECS World.
     * @param uiBus   Non-owning pointer to the UI event bus for notifications.
     */
    InventorySystem(World* world, EventBus<UIEvent>* uiBus);

    // -----------------------------------------------------------------------
    // Basic inventory operations
    // -----------------------------------------------------------------------

    /**
     * @brief Add `quantity` of item `itemID` to the entity's inventory.
     *
     * If the entity already has the item (and it is stackable), increments
     * the existing stack.  Otherwise finds a free slot.  Returns false if
     * the inventory is full.
     *
     * @param entity    Entity that will receive the item.
     * @param itemID    ID in GameDatabase::GetItems().
     * @param quantity  Number of items to add (default 1).
     * @return true on success, false if no free slot is available.
     *
     * TEACHING NOTE — Stackable vs Non-Stackable Items
     * ──────────────────────────────────────────────────
     * Consumables (potions, ethers) are stackable: 10 potions occupy one slot.
     * Equipment items are NOT stackable: each sword takes its own slot.
     * We check ItemData::isStackable from the database to decide.
     */
    bool AddItem(EntityID entity, uint32_t itemID, uint32_t quantity = 1);

    /**
     * @brief Remove `quantity` of item `itemID` from the entity's inventory.
     *
     * Decrements the stack.  If the stack reaches 0, clears the slot.
     * Returns false if the entity does not have enough of the item.
     *
     * @param entity    Entity to remove from.
     * @param itemID    ID of the item to remove.
     * @param quantity  Number to remove.
     * @return true on success, false if insufficient quantity.
     */
    bool RemoveItem(EntityID entity, uint32_t itemID, uint32_t quantity = 1);

    /**
     * @brief Apply the effect of a consumable item and remove one use.
     *
     * Reads ItemData to determine hpRestore/mpRestore, applies them to
     * `target`'s HealthComponent, then calls RemoveItem for 1.
     *
     * @param entity   Entity using the item (checks their inventory).
     * @param itemID   ID of the consumable.
     * @return true if the item was used successfully.
     */
    bool UseItem(EntityID entity, uint32_t itemID);

    // -----------------------------------------------------------------------
    // Equipment management
    // -----------------------------------------------------------------------

    /**
     * @brief Equip item `itemID` from the entity's inventory.
     *
     * Determines the slot from ItemData::itemType, unequips any existing item
     * in that slot (returns it to inventory), moves itemID to the equipment
     * slot, and calls RecalculateEquipStats().
     *
     * @param entity  Entity equipping the item.
     * @param itemID  Item to equip.
     * @return true if the item was equipped, false if it cannot be equipped.
     */
    bool EquipItem(EntityID entity, uint32_t itemID);

    /**
     * @brief Unequip the item in `slot` and return it to inventory.
     *
     * Returns false if the slot is empty or the inventory is full.
     *
     * @param entity  Entity to unequip.
     * @param slot    Which equipment slot to clear.
     * @return true on success.
     */
    bool UnequipItem(EntityID entity, EquipSlot slot);

    /**
     * @brief Return the item ID currently in the given equipment slot.
     *
     * @return Item ID, or 0 if the slot is empty.
     */
    uint32_t GetEquippedItem(EntityID entity, EquipSlot slot) const;

    /**
     * @brief Recompute all stat bonuses from equipped items.
     *
     * Clears the cached bonus fields in EquipmentComponent, then iterates
     * all seven slots summing attack/defence/magic/hp/mp bonuses.
     * Sets EquipmentComponent::statsDirty = false.
     *
     * Called automatically by EquipItem and UnequipItem.
     *
     * TEACHING NOTE — Dirty Flags (Lazy Recalculation)
     * ──────────────────────────────────────────────────
     * Recomputing stats on every combat frame would be wasteful — equipment
     * rarely changes.  We set `statsDirty = true` when equipment changes,
     * then recalculate only when the stats are actually needed (here, eagerly
     * on any equip/unequip, but could be deferred to first combat use).
     * This "dirty flag" pattern is very common in game engines.
     */
    void RecalculateEquipStats(EntityID entity);

    // -----------------------------------------------------------------------
    // Utility
    // -----------------------------------------------------------------------

    /**
     * @brief Sort the inventory to group items by type and ID.
     *
     * Moves empty slots to the end, sorts non-empty slots by itemID.
     * Cosmetic operation — does not affect game logic.
     *
     * @param entity  Entity whose inventory to sort.
     */
    void SortInventory(EntityID entity);

    /**
     * @brief Count how many of item `itemID` the entity holds.
     *
     * @return Quantity in inventory, or 0 if not present.
     */
    uint32_t GetItemCount(EntityID entity, uint32_t itemID) const;

    /**
     * @brief Move `qty` of item `itemID` from entity `from` to entity `to`.
     *
     * Combines RemoveItem and AddItem with transactional semantics:
     * if AddItem fails (target inventory full), no items are removed.
     *
     * @return true on success, false if transfer could not complete.
     */
    bool TransferItem(EntityID from, EntityID to, uint32_t itemID, uint32_t qty);

private:

    /**
     * @brief Map EquipSlot enum to the corresponding EquipmentComponent field.
     *
     * Returns a REFERENCE to the correct uint32_t field so the caller can
     * read or write it uniformly.
     *
     * TEACHING NOTE — Reference Returns from Helpers
     * ─────────────────────────────────────────────────
     * Returning a reference to a struct member avoids code duplication:
     * instead of 7 separate switch blocks in EquipItem and UnequipItem,
     * both delegate to this one helper.  The reference is valid as long as
     * the EquipmentComponent object lives (which is the World's lifetime).
     */
    uint32_t& GetSlotField(EquipmentComponent& equip, EquipSlot slot) const;

    /**
     * @brief Determine the appropriate EquipSlot for a given ItemData.
     *
     * Inspects ItemData::itemType and ItemData::weaponType to decide which
     * slot the item should go into.
     *
     * @return The EquipSlot, or EquipSlot::COUNT if item is not equippable.
     */
    EquipSlot DetermineSlot(const ItemData& item) const;

    World*             m_world  = nullptr;
    EventBus<UIEvent>* m_uiBus  = nullptr;
};
