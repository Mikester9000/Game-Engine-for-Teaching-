/**
 * @file ShopSystem.hpp
 * @brief Vendor/shop system for the educational RPG engine.
 *
 * ============================================================================
 * TEACHING NOTE — Shop System Design
 * ============================================================================
 *
 * A shop system has three main responsibilities:
 *
 *  1. CATALOGUE  — What items does this vendor sell?  At what price?
 *  2. TRANSACTION — Validate the player's funds, move the item.
 *  3. UI FEEDBACK — Tell the player what happened.
 *
 * ─── Price Calculation ───────────────────────────────────────────────────────
 *
 * Buy price:  ItemData::price × ShopData::buyMultiplier
 * Sell price: ItemData::price × ShopData::sellMultiplier (always ≤ buy price)
 *
 * Using multipliers instead of fixed prices means:
 *   - A "black market" shop has buyMultiplier 2.0 (double price).
 *   - A "guild friend" vendor has buyMultiplier 0.8 (20% discount).
 *   - All prices update automatically when you change the base item price.
 *
 * ─── State Machine ───────────────────────────────────────────────────────────
 *
 * The ShopSystem tracks whether a shop is currently "open" via `m_isOpen`
 * and `m_activeShopID`.  Only one shop can be open at a time (the player
 * can only talk to one vendor at once).
 *
 *   CLOSED → [OpenShop]  → OPEN
 *   OPEN   → [CloseShop] → CLOSED
 *   OPEN   → [BuyItem]   → OPEN  (transaction, stays open)
 *   OPEN   → [SellItem]  → OPEN
 *
 * ─── Key Items ───────────────────────────────────────────────────────────────
 *
 * Items with ItemData::isKeyItem == true cannot be sold.  Key items are
 * plot-critical (e.g. "Crystal Key", "Prince's Ring") and should never
 * leave the player's inventory.  SellItem() checks this flag.
 *
 * ─── Gil Integration ─────────────────────────────────────────────────────────
 *
 * Gil is stored in two places:
 *   - InventoryComponent::gil  (quick access, kept in sync)
 *   - CurrencyComponent::gil   (authoritative, 64-bit to avoid overflow)
 *
 * Transactions use CurrencyComponent::SpendGil() and EarnGil() to ensure
 * correct overflow handling.  The InventoryComponent::gil is updated as
 * a cache for the UI.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 *
 * C++ Standard: C++17
 */

#pragma once

#include <vector>
#include <cstdint>
#include <string>

#include "../../engine/core/Types.hpp"
#include "../../engine/core/Logger.hpp"
#include "../../engine/core/EventBus.hpp"
#include "../../engine/ecs/ECS.hpp"
#include "../GameData.hpp"


// ===========================================================================
// ShopSystem class
// ===========================================================================

/**
 * @class ShopSystem
 * @brief Handles player interactions with in-game vendors.
 *
 * USAGE EXAMPLE:
 * @code
 *   ShopSystem shop(world, &EventBus<UIEvent>::Instance());
 *
 *   // Player walks up to a vendor:
 *   shop.OpenShop(playerID, 1);       // shopID=1 is "Hammerhead Shop"
 *
 *   // Player buys 2 Potions (itemID=1):
 *   shop.BuyItem(playerID, 1, 1, 2);  // shopID, itemID, qty
 *
 *   // Player sells one old sword (itemID=10, qty=1):
 *   shop.SellItem(playerID, 10, 1);
 *
 *   // Player leaves the shop menu:
 *   shop.CloseShop();
 * @endcode
 */
class ShopSystem {
public:

    /// The shop ID that is currently open, or 0 if none.
    uint32_t activeShopID = 0;

    /**
     * @brief Construct the shop system.
     *
     * @param world   Non-owning pointer to ECS World.
     * @param uiBus   Non-owning pointer to the UI event bus.
     */
    ShopSystem(World* world, EventBus<UIEvent>* uiBus);

    // -----------------------------------------------------------------------
    // Shop state management
    // -----------------------------------------------------------------------

    /**
     * @brief Open shop `shopID` for `player`.
     *
     * Looks up ShopData in GameDatabase.  Publishes UIEvent::OPEN_MENU
     * with the shop name so the UI can draw the shop screen.
     *
     * If a shop is already open, logs a warning and replaces it.
     *
     * @param player  Player entity browsing the shop.
     * @param shopID  ID in GameDatabase::GetShops().
     */
    void OpenShop(EntityID player, uint32_t shopID);

    /**
     * @brief Close the currently open shop.
     *
     * Resets `activeShopID` to 0 and publishes UIEvent::CLOSE_MENU.
     * Safe to call even if no shop is open.
     */
    void CloseShop();

    /// @return true if any shop is currently open.
    bool IsOpen() const { return m_isOpen; }

    // -----------------------------------------------------------------------
    // Transactions
    // -----------------------------------------------------------------------

    /**
     * @brief Purchase `qty` units of `itemID` from shop `shopID`.
     *
     * Steps:
     *  1. Look up ShopData to confirm item is stocked.
     *  2. Compute price = GetBuyPrice(shopID, itemID) × qty.
     *  3. Check the player has enough Gil (CurrencyComponent::SpendGil).
     *  4. Remove Gil, call InventorySystem::AddItem.
     *  5. Publish UIEvent::SHOW_NOTIFICATION "Purchased: …".
     *
     * @return true on success, false if: insufficient Gil, item not stocked,
     *         inventory full, or shop not found.
     *
     * TEACHING NOTE — Transactional Validation
     * ──────────────────────────────────────────
     * We check ALL preconditions BEFORE making any changes.
     * This prevents partial state: the player cannot lose Gil and then
     * fail to receive the item because their inventory is full.
     * Always validate first, then commit.
     */
    bool BuyItem(EntityID player, uint32_t shopID,
                 uint32_t itemID, uint32_t qty);

    /**
     * @brief Sell `qty` units of `itemID` from the player's inventory.
     *
     * Steps:
     *  1. Check player has enough of itemID.
     *  2. Check itemID is not a key item.
     *  3. Compute sell value = GetSellPrice(itemID) × qty.
     *  4. Remove items, add Gil.
     *  5. Publish UIEvent::SHOW_NOTIFICATION "Sold: …".
     *
     * @return true on success, false if not enough items, key item, or no shop open.
     */
    bool SellItem(EntityID player, uint32_t itemID, uint32_t qty);

    // -----------------------------------------------------------------------
    // Price queries
    // -----------------------------------------------------------------------

    /**
     * @brief Get the price to buy item `itemID` from shop `shopID`.
     *
     * Formula: (uint32_t)(ItemData::price × ShopData::buyMultiplier)
     *
     * @return Buy price in Gil, or 0 if item/shop not found.
     */
    uint32_t GetBuyPrice(uint32_t shopID, uint32_t itemID) const;

    /**
     * @brief Get the Gil the player receives for selling one of item `itemID`.
     *
     * Uses the sell multiplier of the CURRENTLY OPEN shop (activeShopID).
     * Formula: (uint32_t)(ItemData::price × ShopData::sellMultiplier)
     *
     * @return Sell value in Gil, or 0 if item not found or no shop open.
     */
    uint32_t GetSellPrice(uint32_t itemID) const;

    /**
     * @brief Return the list of item IDs available in shop `shopID`.
     *
     * @return Copy of ShopData::itemIDs, or empty vector if shop not found.
     *
     * TEACHING NOTE — Returning by Value vs Reference
     * ──────────────────────────────────────────────────
     * We return by VALUE here (copying the vector) rather than a const
     * reference to ShopData's internal vector.  This is safer: the caller
     * can store the result without worrying about the ShopData being
     * invalidated.  The copy is small (typically 5–20 item IDs).
     */
    std::vector<uint32_t> GetShopStock(uint32_t shopID) const;

private:

    World*             m_world  = nullptr;
    EventBus<UIEvent>* m_uiBus  = nullptr;
    bool               m_isOpen = false;
    EntityID           m_currentPlayer = NULL_ENTITY;
};
