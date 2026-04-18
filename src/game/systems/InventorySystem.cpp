/**
 * @file InventorySystem.cpp
 * @brief Implementation of item management: add, remove, use, equip.
 * @author  Educational Game Engine Project  @version 1.0  @date 2024
 */
#include "InventorySystem.hpp"
#include <algorithm>

// ---------------------------------------------------------------------------
InventorySystem::InventorySystem(World* world, EventBus<UIEvent>* uiBus)
    : m_world(world), m_uiBus(uiBus) {}

// ---------------------------------------------------------------------------
// TEACHING NOTE — AddItem: Stacking vs Slot Allocation
// ---------------------------------------------------------------------------
// When adding a stackable item we first try to merge it into an existing
// slot that holds the same itemID (stacking).  Only if no existing stack
// exists do we consume a new empty slot.  This mirrors how FF15 and most
// RPGs manage inventory: potions stack to 99, weapons each take a full slot.
// The m_uiBus publish notifies the UI layer without any direct dependency
// between InventorySystem and the UI rendering code — a clean separation
// achieved via the EventBus pattern.
// ---------------------------------------------------------------------------
bool InventorySystem::AddItem(EntityID entity, uint32_t itemID, uint32_t quantity)
{
    if (!m_world->HasComponent<InventoryComponent>(entity)) return false;
    auto& inv = m_world->GetComponent<InventoryComponent>(entity);

    const ItemData* itemData = GameDatabase::FindItem(itemID);
    bool stackable = itemData ? itemData->isStackable : true;

    // Try to stack into existing slot.
    if (stackable) {
        uint32_t slotIdx = inv.FindItem(itemID);
        if (slotIdx < MAX_INV_SLOTS) {
            inv.slots[slotIdx].quantity += quantity;
            LOG_DEBUG("AddItem: stacked " << quantity << " of item " << itemID);
            if (m_uiBus && itemData) {
                UIEvent ev;
                ev.type = UIEvent::Type::SHOW_NOTIFICATION;
                ev.text = "Obtained: " + itemData->name + " x" + std::to_string(quantity);
                m_uiBus->Publish(ev);
            }
            return true;
        }
    }

    // Find empty slot.
    uint32_t emptySlot = inv.FindEmptySlot();
    if (emptySlot >= MAX_INV_SLOTS) {
        LOG_WARN("AddItem: inventory full for entity " << entity);
        return false;
    }

    inv.slots[emptySlot].itemID   = itemID;
    inv.slots[emptySlot].quantity = quantity;
    LOG_DEBUG("AddItem: added item " << itemID << " x" << quantity);

    if (m_uiBus && itemData) {
        UIEvent ev;
        ev.type = UIEvent::Type::SHOW_NOTIFICATION;
        ev.text = "Obtained: " + itemData->name + " x" + std::to_string(quantity);
        m_uiBus->Publish(ev);
    }
    return true;
}

// ---------------------------------------------------------------------------
bool InventorySystem::RemoveItem(EntityID entity, uint32_t itemID, uint32_t quantity)
{
    if (!m_world->HasComponent<InventoryComponent>(entity)) return false;
    auto& inv = m_world->GetComponent<InventoryComponent>(entity);

    uint32_t slotIdx = inv.FindItem(itemID);
    if (slotIdx >= MAX_INV_SLOTS || inv.slots[slotIdx].quantity < quantity) {
        LOG_WARN("RemoveItem: not enough of item " << itemID);
        return false;
    }

    inv.slots[slotIdx].quantity -= quantity;
    if (inv.slots[slotIdx].quantity == 0)
        inv.slots[slotIdx] = ItemStack{};

    LOG_DEBUG("RemoveItem: removed " << quantity << " of item " << itemID);
    return true;
}

// ---------------------------------------------------------------------------
// TEACHING NOTE — UseItem: Remove-Then-Apply Pattern
// ---------------------------------------------------------------------------
// We remove the item from inventory BEFORE applying its effect.  This
// prevents exploits where an effect callback might add the same item back
// and allow infinite use.  It also means a "use" that fails (e.g. HP is
// already full) still consumes the item — matching the design of most
// action RPGs including FF15.
// ---------------------------------------------------------------------------
bool InventorySystem::UseItem(EntityID entity, uint32_t itemID)
{
    const ItemData* item = GameDatabase::FindItem(itemID);
    if (!item) return false;
    if (!RemoveItem(entity, itemID, 1)) return false;

    if (m_world->HasComponent<HealthComponent>(entity)) {
        auto& hp = m_world->GetComponent<HealthComponent>(entity);
        if (item->hpRestore > 0)
            hp.hp = std::min(hp.maxHp, hp.hp + item->hpRestore);
        if (item->mpRestore > 0)
            hp.mp = std::min(hp.maxMp, hp.mp + item->mpRestore);
    }

    if (m_uiBus) {
        UIEvent ev;
        ev.type = UIEvent::Type::SHOW_NOTIFICATION;
        ev.text = "Used " + item->name;
        m_uiBus->Publish(ev);
    }
    LOG_INFO("Used item " << item->name << " on entity " << entity);
    return true;
}

// ---------------------------------------------------------------------------
bool InventorySystem::EquipItem(EntityID entity, uint32_t itemID)
{
    const ItemData* item = GameDatabase::FindItem(itemID);
    if (!item) return false;

    EquipSlot slot = DetermineSlot(*item);
    if (slot == EquipSlot::COUNT) {
        LOG_WARN("EquipItem: item " << itemID << " is not equippable.");
        return false;
    }

    if (!m_world->HasComponent<EquipmentComponent>(entity)) return false;
    auto& equip = m_world->GetComponent<EquipmentComponent>(entity);

    // Unequip existing item in the slot first.
    uint32_t& slotField = GetSlotField(equip, slot);
    if (slotField != 0) {
        // Return old item to inventory.
        AddItem(entity, slotField, 1);
        slotField = 0;
    }

    // Remove new item from inventory.
    if (!RemoveItem(entity, itemID, 1)) return false;

    slotField = itemID;
    RecalculateEquipStats(entity);

    if (m_uiBus) {
        UIEvent ev;
        ev.type = UIEvent::Type::SHOW_NOTIFICATION;
        ev.text = "Equipped: " + item->name;
        m_uiBus->Publish(ev);
    }
    LOG_INFO("Equipped item " << item->name << " on entity " << entity);
    return true;
}

// ---------------------------------------------------------------------------
bool InventorySystem::UnequipItem(EntityID entity, EquipSlot slot)
{
    if (!m_world->HasComponent<EquipmentComponent>(entity)) return false;
    auto& equip = m_world->GetComponent<EquipmentComponent>(entity);

    uint32_t& slotField = GetSlotField(equip, slot);
    if (slotField == 0) return false;

    if (!AddItem(entity, slotField, 1)) return false;  // Inventory might be full.
    slotField = 0;
    RecalculateEquipStats(entity);
    LOG_DEBUG("Unequipped slot " << static_cast<int>(slot) << " from entity " << entity);
    return true;
}

// ---------------------------------------------------------------------------
uint32_t InventorySystem::GetEquippedItem(EntityID entity, EquipSlot slot) const
{
    if (!m_world->HasComponent<EquipmentComponent>(entity)) return 0;
    auto& equip = m_world->GetComponent<EquipmentComponent>(entity);
    return const_cast<InventorySystem*>(this)->GetSlotField(equip, slot);
}

// ---------------------------------------------------------------------------
void InventorySystem::RecalculateEquipStats(EntityID entity)
{
    if (!m_world->HasComponent<EquipmentComponent>(entity)) return;
    auto& equip = m_world->GetComponent<EquipmentComponent>(entity);

    equip.bonusStrength = equip.bonusDefence = equip.bonusMagic = 0;
    equip.bonusHP = equip.bonusMP = 0;

    uint32_t slots[] = { equip.weaponID, equip.offhandID, equip.headID,
                         equip.bodyID,   equip.legsID,    equip.accessory1,
                         equip.accessory2 };

    for (uint32_t id : slots) {
        if (id == 0) continue;
        const ItemData* item = GameDatabase::FindItem(id);
        if (!item) continue;
        equip.bonusStrength += item->attack;
        equip.bonusDefence  += item->defense;
        equip.bonusMagic    += item->magic;
    }
    equip.statsDirty = false;
    LOG_DEBUG("RecalculateEquipStats: str+" << equip.bonusStrength
              << " def+" << equip.bonusDefence << " mag+" << equip.bonusMagic);
}

// ---------------------------------------------------------------------------
void InventorySystem::SortInventory(EntityID entity)
{
    if (!m_world->HasComponent<InventoryComponent>(entity)) return;
    auto& inv = m_world->GetComponent<InventoryComponent>(entity);

    // Stable sort: non-empty slots first, then by itemID.
    std::stable_sort(inv.slots.begin(), inv.slots.end(),
        [](const ItemStack& a, const ItemStack& b) {
            if (a.IsEmpty() != b.IsEmpty()) return !a.IsEmpty();
            return a.itemID < b.itemID;
        });
    LOG_DEBUG("Sorted inventory for entity " << entity);
}

// ---------------------------------------------------------------------------
uint32_t InventorySystem::GetItemCount(EntityID entity, uint32_t itemID) const
{
    if (!m_world->HasComponent<InventoryComponent>(entity)) return 0;
    auto& inv = m_world->GetComponent<InventoryComponent>(entity);
    uint32_t idx = inv.FindItem(itemID);
    return (idx < MAX_INV_SLOTS) ? inv.slots[idx].quantity : 0u;
}

// ---------------------------------------------------------------------------
// TEACHING NOTE — TransferItem: Pre-flight Space Check
// ---------------------------------------------------------------------------
// Before removing the item from the source we verify the destination
// inventory can accept it.  We check for an existing stack (if stackable)
// or a free slot — exactly the same logic AddItem would run.  This avoids
// the "item destroyed on transfer to full bag" bug that affects many games.
// ---------------------------------------------------------------------------
bool InventorySystem::TransferItem(EntityID from, EntityID to,
                                    uint32_t itemID, uint32_t qty)
{
    if (GetItemCount(from, itemID) < qty) return false;

    // Check destination has space (AddItem without actually adding).
    if (!m_world->HasComponent<InventoryComponent>(to)) return false;
    auto& destInv = m_world->GetComponent<InventoryComponent>(to);

    const ItemData* item = GameDatabase::FindItem(itemID);
    bool stackable = item ? item->isStackable : true;
    bool hasSpace = (stackable && destInv.FindItem(itemID) < MAX_INV_SLOTS)
                  || destInv.FindEmptySlot() < MAX_INV_SLOTS;
    if (!hasSpace) return false;

    RemoveItem(from, itemID, qty);
    AddItem(to, itemID, qty);
    return true;
}

// ---------------------------------------------------------------------------
uint32_t& InventorySystem::GetSlotField(EquipmentComponent& equip,
                                         EquipSlot slot) const
{
    // TEACHING NOTE — Switch on Enum to Map to Struct Fields
    // Each EquipSlot enum value maps to one field in EquipmentComponent.
    // We return a reference so callers can both read and write the slot.
    switch (slot) {
        case EquipSlot::WEAPON:     return equip.weaponID;
        case EquipSlot::OFFHAND:    return equip.offhandID;
        case EquipSlot::HEAD:       return equip.headID;
        case EquipSlot::BODY:       return equip.bodyID;
        case EquipSlot::LEGS:       return equip.legsID;
        case EquipSlot::ACCESSORY1: return equip.accessory1;
        case EquipSlot::ACCESSORY2: return equip.accessory2;
        default:                    return equip.weaponID;  // fallback
    }
}

// ---------------------------------------------------------------------------
EquipSlot InventorySystem::DetermineSlot(const ItemData& item) const
{
    switch (item.itemType) {
        case ItemType::WEAPON:
            return EquipSlot::WEAPON;
        case ItemType::ARMOR:
            return EquipSlot::BODY;
        case ItemType::ACCESSORY:
            return EquipSlot::ACCESSORY1;
        default:
            return EquipSlot::COUNT; // Not equippable.
    }
}
