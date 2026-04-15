/**
 * @file CampSystem.cpp
 * @brief Implementation of the camp, rest, and cooking system.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 */

#include "CampSystem.hpp"
#include "InventorySystem.hpp"

// Static member definition — exactly one ActiveMealBuff shared across the game.
ActiveMealBuff CampSystem::currentBuff{};

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

CampSystem::CampSystem(World* world, EventBus<UIEvent>* uiBus)
    : m_world(world), m_uiBus(uiBus)
{
    LOG_INFO("CampSystem initialised.");
}

// ---------------------------------------------------------------------------
// CanCamp
// ---------------------------------------------------------------------------

bool CampSystem::CanCamp(const TileMap& map, int x, int y) const
{
    // Basic tile check: camping is only allowed on open terrain, not in dungeons.
    // TEACHING NOTE — We use const TileMap& to signal read-only access.
    const Tile& tile = map.GetTile(x, y);

    switch (tile.type) {
        case TileType::GRASS:
        case TileType::SAND:
        case TileType::ROAD:
        case TileType::FLOOR:  // allow inside outpost rooms
            return true;
        default:
            return false;
    }
}

// ---------------------------------------------------------------------------
// SetupCamp
// ---------------------------------------------------------------------------

void CampSystem::SetupCamp(EntityID player)
{
    if (!m_world->HasComponent<CampComponent>(player)) return;

    auto& cc      = m_world->GetComponent<CampComponent>(player);
    cc.isCamping  = true;

    WorldEvent wev;
    wev.type = WorldEvent::Type::CAMP_STARTED;
    EventBus<WorldEvent>::Instance().Publish(wev);

    if (m_uiBus) {
        UIEvent ev;
        ev.type = UIEvent::Type::SHOW_NOTIFICATION;
        ev.text = "Camp set up. Time to rest.";
        m_uiBus->Publish(ev);
    }

    LOG_INFO("Camp set up for entity " + std::to_string(player));
}

// ---------------------------------------------------------------------------
// BreakCamp
// ---------------------------------------------------------------------------

void CampSystem::BreakCamp(EntityID player)
{
    if (!m_world->HasComponent<CampComponent>(player)) return;

    m_world->GetComponent<CampComponent>(player).isCamping = false;

    if (m_uiBus) {
        UIEvent ev;
        ev.type = UIEvent::Type::SHOW_NOTIFICATION;
        ev.text = "Broke camp. Back on the road.";
        m_uiBus->Publish(ev);
    }

    LOG_INFO("Camp broken for entity " + std::to_string(player));
}

// ---------------------------------------------------------------------------
// IsAtCamp
// ---------------------------------------------------------------------------

bool CampSystem::IsAtCamp(EntityID player) const
{
    if (!m_world->HasComponent<CampComponent>(player)) return false;
    return m_world->GetComponent<CampComponent>(player).isCamping;
}

// ---------------------------------------------------------------------------
// CookMeal
// ---------------------------------------------------------------------------

bool CampSystem::CookMeal(EntityID player, uint32_t recipeID)
{
    // Find the recipe.
    const RecipeData* recipe = nullptr;
    for (const auto& r : GameDatabase::GetRecipes()) {
        if (r.id == recipeID) { recipe = &r; break; }
    }
    if (!recipe) {
        LOG_WARN("CookMeal: recipe " + std::to_string(recipeID) + " not found.");
        return false;
    }

    // TEACHING NOTE — Check ALL ingredients before consuming ANY.
    if (!HasIngredients(player, *recipe)) {
        if (m_uiBus) {
            UIEvent ev;
            ev.type = UIEvent::Type::SHOW_NOTIFICATION;
            ev.text = "Missing ingredients for " + recipe->name + ".";
            m_uiBus->Publish(ev);
        }
        return false;
    }

    // Consume ingredients.
    ConsumeIngredients(player, *recipe);

    // Apply the meal buff.
    currentBuff.recipeID       = recipeID;
    currentBuff.turnsRemaining = recipe->durationTurns;
    currentBuff.hpBonus        = recipe->hpBonus;
    currentBuff.mpBonus        = recipe->mpBonus;
    currentBuff.attackBonus    = recipe->attackBonus;
    currentBuff.defenseBonus   = recipe->defenseBonus;
    currentBuff.magicBonus     = recipe->magicBonus;

    // Also cache in the player's CampComponent for the combat system to read.
    if (m_world->HasComponent<CampComponent>(player)) {
        auto& cc         = m_world->GetComponent<CampComponent>(player);
        cc.currentMealID = recipeID;
        cc.mealStrBonus  = recipe->attackBonus;
        cc.mealMagBonus  = recipe->magicBonus;
        cc.mealDefBonus  = recipe->defenseBonus;
        cc.mealHPBonus   = recipe->hpBonus;
        cc.mealDuration  = static_cast<float>(recipe->durationTurns);
    }

    if (m_uiBus) {
        UIEvent ev;
        ev.type  = UIEvent::Type::SHOW_NOTIFICATION;
        ev.text  = recipe->chefName + ": \"" + recipe->name + " is served!\"";
        m_uiBus->Publish(ev);
    }

    LOG_INFO("Cooked: " + recipe->name);
    return true;
}

// ---------------------------------------------------------------------------
// GetAvailableRecipes
// ---------------------------------------------------------------------------

std::vector<uint32_t> CampSystem::GetAvailableRecipes(EntityID player) const
{
    std::vector<uint32_t> result;
    for (const auto& recipe : GameDatabase::GetRecipes()) {
        if (HasIngredients(player, recipe)) {
            result.push_back(recipe.id);
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Rest
// ---------------------------------------------------------------------------

void CampSystem::Rest(EntityID player)
{
    // 1. Restore HP/MP to max for the player.
    if (m_world->HasComponent<HealthComponent>(player)) {
        auto& hc = m_world->GetComponent<HealthComponent>(player);
        hc.hp   = hc.maxHp;
        hc.mp   = hc.maxMp;
        hc.isDead = false;
    }

    // 2. Also restore party members if there's a PartyComponent.
    if (m_world->HasComponent<PartyComponent>(player)) {
        auto& party = m_world->GetComponent<PartyComponent>(player);
        for (uint32_t i = 0; i < party.memberCount; ++i) {
            EntityID member = party.members[i];
            if (member == NULL_ENTITY) continue;
            if (m_world->HasComponent<HealthComponent>(member)) {
                auto& mhc = m_world->GetComponent<HealthComponent>(member);
                mhc.hp    = mhc.maxHp;
                mhc.mp    = mhc.maxMp;
                mhc.isDead = false;
            }
        }
    }

    // 3. Apply banked XP → level-ups.
    // TEACHING NOTE — FF15's pending XP system: combat awards "tentative" XP
    // which only converts to real levels at camp.  This creates tension.
    if (m_world->HasComponent<LevelComponent>(player)) {
        auto& lc   = m_world->GetComponent<LevelComponent>(player);
        bool levUp = lc.pendingLevelUp;
        lc.ApplyBankedXP();

        if (levUp && m_uiBus) {
            UIEvent ev;
            ev.type  = UIEvent::Type::SHOW_NOTIFICATION;
            ev.text  = "Level up! Now level " + std::to_string(lc.level) + "!";
            m_uiBus->Publish(ev);
        }
    }

    // 4. Advance game time by 8 hours (overnight sleep).
    WorldEvent wev;
    wev.type = WorldEvent::Type::DAY_ADVANCED;
    EventBus<WorldEvent>::Instance().Publish(wev);

    if (m_uiBus) {
        UIEvent ev;
        ev.type = UIEvent::Type::SHOW_NOTIFICATION;
        ev.text = "The party rested. A new day begins.";
        m_uiBus->Publish(ev);
    }

    LOG_INFO("Party rested. HP/MP restored.");
}

// ---------------------------------------------------------------------------
// Private: HasIngredients
// ---------------------------------------------------------------------------

bool CampSystem::HasIngredients(EntityID player, const RecipeData& recipe) const
{
    if (!m_world->HasComponent<InventoryComponent>(player)) return false;
    const auto& inv = m_world->GetComponent<InventoryComponent>(player);

    for (const auto& ing : recipe.ingredients) {
        uint32_t idx = inv.FindItem(ing.itemID);
        if (idx >= MAX_INV_SLOTS) return false;
        if (inv.slots[idx].quantity < ing.quantity) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Private: ConsumeIngredients
// ---------------------------------------------------------------------------

void CampSystem::ConsumeIngredients(EntityID player, const RecipeData& recipe)
{
    InventorySystem invSys(m_world, nullptr);
    for (const auto& ing : recipe.ingredients) {
        invSys.RemoveItem(player, ing.itemID, ing.quantity);
    }
}
