/**
 * @file ShopSystem.cpp
 * @brief Implementation of the vendor/shop system.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 */

#include "ShopSystem.hpp"
#include "InventorySystem.hpp"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

ShopSystem::ShopSystem(World* world, EventBus<UIEvent>* uiBus)
    : m_world(world), m_uiBus(uiBus)
{
    LOG_INFO("ShopSystem initialised.");
}

// ---------------------------------------------------------------------------
// OpenShop
// ---------------------------------------------------------------------------

void ShopSystem::OpenShop(EntityID player, uint32_t shopID)
{
    const ShopData* shop = GameDatabase::FindShop(shopID);
    if (!shop) {
        LOG_WARN("OpenShop: shopID " + std::to_string(shopID) + " not found.");
        return;
    }

    if (m_isOpen) {
        LOG_WARN("OpenShop: shop already open. Replacing with " + shop->name);
    }

    m_isOpen         = true;
    activeShopID     = shopID;
    m_currentPlayer  = player;

    if (m_uiBus) {
        UIEvent ev;
        ev.type = UIEvent::Type::OPEN_MENU;
        ev.text = "Shop: " + shop->name;
        m_uiBus->Publish(ev);
    }

    LOG_INFO("Opened shop: " + shop->name);
}

// ---------------------------------------------------------------------------
// CloseShop
// ---------------------------------------------------------------------------

void ShopSystem::CloseShop()
{
    if (!m_isOpen) return;

    m_isOpen        = false;
    activeShopID    = 0;
    m_currentPlayer = NULL_ENTITY;

    if (m_uiBus) {
        UIEvent ev;
        ev.type = UIEvent::Type::CLOSE_MENU;
        m_uiBus->Publish(ev);
    }

    LOG_INFO("Shop closed.");
}

// ---------------------------------------------------------------------------
// BuyItem
// ---------------------------------------------------------------------------

bool ShopSystem::BuyItem(EntityID player, uint32_t shopID,
                         uint32_t itemID, uint32_t qty)
{
    // 1. Validate shop and stock.
    const ShopData* shop = GameDatabase::FindShop(shopID);
    if (!shop) {
        LOG_WARN("BuyItem: shop not found.");
        return false;
    }

    bool inStock = false;
    for (uint32_t id : shop->itemIDs) {
        if (id == itemID) { inStock = true; break; }
    }
    if (!inStock) {
        LOG_WARN("BuyItem: item " + std::to_string(itemID) + " not in stock.");
        return false;
    }

    // 2. Compute total cost.
    uint32_t unitPrice = GetBuyPrice(shopID, itemID);
    uint64_t total     = static_cast<uint64_t>(unitPrice) * qty;

    // 3. Deduct Gil — prefer CurrencyComponent (authoritative) over inv.gil.
    // TEACHING NOTE — Check ALL preconditions before mutating ANY state.
    bool hasCurrency = false;
    if (m_world->HasComponent<CurrencyComponent>(player)) {
        auto& cc = m_world->GetComponent<CurrencyComponent>(player);
        hasCurrency = (cc.gil >= total);
    } else if (m_world->HasComponent<InventoryComponent>(player)) {
        auto& inv = m_world->GetComponent<InventoryComponent>(player);
        hasCurrency = (inv.gil >= static_cast<uint32_t>(total));
    }

    if (!hasCurrency) {
        LOG_INFO("BuyItem: insufficient Gil.");
        if (m_uiBus) {
            UIEvent ev;
            ev.type = UIEvent::Type::SHOW_NOTIFICATION;
            ev.text = "Not enough Gil!";
            m_uiBus->Publish(ev);
        }
        return false;
    }

    // 4. Add to inventory — check space before spending money.
    InventorySystem invSys(m_world, m_uiBus);
    if (!invSys.AddItem(player, itemID, qty)) {
        LOG_WARN("BuyItem: inventory full.");
        if (m_uiBus) {
            UIEvent ev;
            ev.type = UIEvent::Type::SHOW_NOTIFICATION;
            ev.text = "Inventory full!";
            m_uiBus->Publish(ev);
        }
        return false;
    }

    // 5. Deduct Gil now that we know everything succeeded.
    if (m_world->HasComponent<CurrencyComponent>(player)) {
        m_world->GetComponent<CurrencyComponent>(player).SpendGil(total);
    }
    if (m_world->HasComponent<InventoryComponent>(player)) {
        auto& inv = m_world->GetComponent<InventoryComponent>(player);
        uint32_t deduct = static_cast<uint32_t>(std::min(total, static_cast<uint64_t>(inv.gil)));
        inv.gil -= deduct;
    }

    const ItemData* item = GameDatabase::FindItem(itemID);
    std::string itemName = item ? item->name : "item";

    if (m_uiBus) {
        UIEvent ev;
        ev.type  = UIEvent::Type::SHOW_NOTIFICATION;
        ev.text  = "Purchased " + std::to_string(qty) + "x " + itemName +
                   " for " + std::to_string(total) + " Gil.";
        m_uiBus->Publish(ev);
    }

    LOG_INFO("Bought " + std::to_string(qty) + "x " + itemName);
    return true;
}

// ---------------------------------------------------------------------------
// SellItem
// ---------------------------------------------------------------------------

bool ShopSystem::SellItem(EntityID player, uint32_t itemID, uint32_t qty)
{
    if (!m_isOpen) {
        LOG_WARN("SellItem: no shop is open.");
        return false;
    }

    // Check key-item protection.
    const ItemData* item = GameDatabase::FindItem(itemID);
    if (item && item->isKeyItem) {
        if (m_uiBus) {
            UIEvent ev;
            ev.type = UIEvent::Type::SHOW_NOTIFICATION;
            ev.text = "Cannot sell key items!";
            m_uiBus->Publish(ev);
        }
        return false;
    }

    // Remove from inventory.
    InventorySystem invSys(m_world, m_uiBus);
    if (!invSys.RemoveItem(player, itemID, qty)) {
        LOG_WARN("SellItem: not enough items.");
        return false;
    }

    // Add Gil.
    uint64_t earned = static_cast<uint64_t>(GetSellPrice(itemID)) * qty;

    if (m_world->HasComponent<CurrencyComponent>(player)) {
        m_world->GetComponent<CurrencyComponent>(player).EarnGil(earned);
    }
    if (m_world->HasComponent<InventoryComponent>(player)) {
        m_world->GetComponent<InventoryComponent>(player).gil +=
            static_cast<uint32_t>(std::min(earned, static_cast<uint64_t>(UINT32_MAX)));
    }

    std::string itemName = item ? item->name : "item";

    if (m_uiBus) {
        UIEvent ev;
        ev.type  = UIEvent::Type::SHOW_NOTIFICATION;
        ev.text  = "Sold " + std::to_string(qty) + "x " + itemName +
                   " for " + std::to_string(earned) + " Gil.";
        m_uiBus->Publish(ev);
    }

    LOG_INFO("Sold " + std::to_string(qty) + "x " + itemName);
    return true;
}

// ---------------------------------------------------------------------------
// GetBuyPrice
// ---------------------------------------------------------------------------

uint32_t ShopSystem::GetBuyPrice(uint32_t shopID, uint32_t itemID) const
{
    const ShopData* shop = GameDatabase::FindShop(shopID);
    const ItemData* item = GameDatabase::FindItem(itemID);
    if (!shop || !item) return 0;
    return static_cast<uint32_t>(item->price * shop->buyMultiplier);
}

// ---------------------------------------------------------------------------
// GetSellPrice
// ---------------------------------------------------------------------------

uint32_t ShopSystem::GetSellPrice(uint32_t itemID) const
{
    const ItemData* item = GameDatabase::FindItem(itemID);
    if (!item) return 0;

    float multiplier = 0.5f;
    if (activeShopID != 0) {
        const ShopData* shop = GameDatabase::FindShop(activeShopID);
        if (shop) multiplier = shop->sellMultiplier;
    }
    return static_cast<uint32_t>(item->price * multiplier);
}

// ---------------------------------------------------------------------------
// GetShopStock
// ---------------------------------------------------------------------------

std::vector<uint32_t> ShopSystem::GetShopStock(uint32_t shopID) const
{
    const ShopData* shop = GameDatabase::FindShop(shopID);
    if (!shop) return {};
    return shop->itemIDs;
}
