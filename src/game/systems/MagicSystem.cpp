/**
 * @file MagicSystem.cpp
 * @brief Implementation of the elemental magic crafting and casting system.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 */

#include "MagicSystem.hpp"
#include "InventorySystem.hpp"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

MagicSystem::MagicSystem(World* world, LuaEngine* lua)
    : m_world(world), m_lua(lua)
{
    LOG_INFO("MagicSystem initialised.");
}

// ---------------------------------------------------------------------------
// LearnSpell
// ---------------------------------------------------------------------------

bool MagicSystem::LearnSpell(EntityID entity, uint32_t spellID)
{
    // Validate spell exists.
    const SpellData* spell = GameDatabase::FindSpell(spellID);
    if (!spell) {
        LOG_WARN("LearnSpell: unknown spellID " + std::to_string(spellID));
        return false;
    }

    if (!m_world->HasComponent<MagicComponent>(entity)) return false;

    auto& mc = m_world->GetComponent<MagicComponent>(entity);

    // Avoid duplicates.
    for (uint32_t known : mc.knownSpells) {
        if (known == spellID) return false;  // already known
    }

    mc.knownSpells.push_back(spellID);
    LOG_INFO("Learned spell: " + spell->name);
    return true;
}

// ---------------------------------------------------------------------------
// CanCast
// ---------------------------------------------------------------------------

bool MagicSystem::CanCast(EntityID entity, uint32_t spellID) const
{
    if (!m_world->HasComponent<MagicComponent>(entity)) return false;
    if (!m_world->HasComponent<HealthComponent>(entity)) return false;

    const auto& mc = m_world->GetComponent<MagicComponent>(entity);
    const auto& hc = m_world->GetComponent<HealthComponent>(entity);

    // Check known.
    bool known = false;
    for (uint32_t id : mc.knownSpells) {
        if (id == spellID) { known = true; break; }
    }
    if (!known) return false;

    // Check MP.
    const SpellData* spell = GameDatabase::FindSpell(spellID);
    if (!spell) return false;

    return hc.mp >= spell->mpCost;
}

// ---------------------------------------------------------------------------
// CastSpell
// ---------------------------------------------------------------------------

int MagicSystem::CastSpell(EntityID caster, uint32_t spellID,
                            EntityID target, TileMap& map)
{
    if (!CanCast(caster, spellID)) {
        LOG_WARN("CastSpell: cannot cast spell " + std::to_string(spellID));
        return 0;
    }

    const SpellData* spell = GameDatabase::FindSpell(spellID);
    if (!spell) return 0;

    // Deduct MP from the caster.
    auto& hc = m_world->GetComponent<HealthComponent>(caster);
    hc.mp -= spell->mpCost;
    if (hc.mp < 0) hc.mp = 0;

    // Compute and apply damage to primary target.
    int damage = ComputeSpellDamage(caster, target, *spell);

    if (m_world->HasComponent<HealthComponent>(target)) {
        auto& targetHC = m_world->GetComponent<HealthComponent>(target);
        targetHC.hp -= damage;
        if (targetHC.hp <= 0) {
            targetHC.hp     = 0;
            targetHC.isDead = true;
        }
    }

    LOG_INFO("Spell " + spell->name + " dealt " + std::to_string(damage) +
             " damage to entity " + std::to_string(target));

    // Handle AoE: if radius > 0, also hit nearby entities.
    // TEACHING NOTE — We convert the target's world position to tile
    // coordinates before passing to ApplyAoe, which works in tile space.
    if (spell->aoeRadius > 0 && m_world->HasComponent<TransformComponent>(target)) {
        const auto& origin = m_world->GetComponent<TransformComponent>(target);
        TileCoord centre{
            static_cast<int>(origin.position.x / TILE_SIZE),
            static_cast<int>(origin.position.z / TILE_SIZE)
        };
        ApplyAoe(caster, centre, spell->aoeRadius, *spell, map);
    }

    // Call Lua callback if defined.
    if (m_lua && !spell->luaCastCallback.empty()) {
        // TEACHING NOTE — We pass the caster ID as the first int argument and
        // the target ID as a string (entity ID → string) to use the available
        // CallFunction overloads.  Lua scripts receive: casterID (int), targetID (string).
        m_lua->CallFunction(spell->luaCastCallback,
                            static_cast<int>(caster),
                            std::to_string(static_cast<int>(target)));
    }

    return damage;
}

// ---------------------------------------------------------------------------
// CraftMagic
// ---------------------------------------------------------------------------

bool MagicSystem::CraftMagic(EntityID entity, ElementType element, int quantity)
{
    // Find the recipe for this element.
    const auto& recipes = GetCraftRecipes();
    const MagicCraftRecipe* recipe = nullptr;
    for (const auto& r : recipes) {
        if (r.element == element) { recipe = &r; break; }
    }
    if (!recipe) {
        LOG_WARN("CraftMagic: no recipe for element " +
                 std::to_string(static_cast<int>(element)));
        return false;
    }

    if (!m_world->HasComponent<InventoryComponent>(entity)) return false;
    if (!m_world->HasComponent<MagicComponent>(entity))    return false;

    InventorySystem invSys(m_world, nullptr);

    // Check and consume ingredients for each unit of quantity.
    // TEACHING NOTE — We do a dry-run check FIRST, then consume.
    // This prevents partial-crafting side effects.
    for (int i = 0; i < quantity; ++i) {
        for (uint32_t ingredID : recipe->ingredientItemIDs) {
            if (!invSys.RemoveItem(entity, ingredID, 1)) {
                LOG_WARN("CraftMagic: insufficient ingredient " +
                         std::to_string(ingredID));
                return false;
            }
        }
    }

    // Add the produced magic flask items to inventory.
    // Flask item IDs: 33=Fire, 34=Blizzard, 35=Thunder (from GameData.hpp).
    uint32_t flaskItemID = 0;
    switch (element) {
        case ElementType::FIRE:      flaskItemID = 33; break;
        case ElementType::ICE:       flaskItemID = 34; break;
        case ElementType::LIGHTNING: flaskItemID = 35; break;
        default: break;
    }

    if (flaskItemID != 0) {
        invSys.AddItem(entity, flaskItemID, static_cast<uint32_t>(quantity * recipe->quantity));
    }

    LOG_INFO("Crafted " + std::to_string(quantity) + " " + recipe->name);
    return true;
}

// ---------------------------------------------------------------------------
// GetKnownSpells
// ---------------------------------------------------------------------------

std::vector<uint32_t> MagicSystem::GetKnownSpells(EntityID entity) const
{
    if (!m_world->HasComponent<MagicComponent>(entity)) return {};
    return m_world->GetComponent<MagicComponent>(entity).knownSpells;
}

// ---------------------------------------------------------------------------
// GetCraftRecipes (static)
// ---------------------------------------------------------------------------

const std::vector<MagicCraftRecipe>& MagicSystem::GetCraftRecipes()
{
    // TEACHING NOTE — Static local: constructed once, returned by reference.
    static const std::vector<MagicCraftRecipe> recipes = BuildCraftRecipes();
    return recipes;
}

// ---------------------------------------------------------------------------
// Private: ComputeSpellDamage
// ---------------------------------------------------------------------------

int MagicSystem::ComputeSpellDamage(EntityID caster, EntityID target,
                                    const SpellData& spell) const
{
    // Base: spell power + caster's magic stat.
    int magic = 10;
    if (m_world->HasComponent<StatsComponent>(caster)) {
        magic = m_world->GetComponent<StatsComponent>(caster).magic;
    }

    // Bonus from equipment.
    if (m_world->HasComponent<EquipmentComponent>(caster)) {
        magic += m_world->GetComponent<EquipmentComponent>(caster).bonusMagic;
    }

    int damage = spell.baseDamage + magic * 2;

    // Element multiplier: check target's NameComponent to look up EnemyData.
    float multiplier = 1.0f;
    if (m_world->HasComponent<NameComponent>(target)) {
        const std::string& name = m_world->GetComponent<NameComponent>(target).name;
        // Look up enemy by name.
        for (const auto& e : GameDatabase::GetEnemies()) {
            if (e.name == name) {
                if (e.weakness    == spell.element) multiplier = 1.5f;
                if (e.resistance  == spell.element) multiplier = 0.5f;
                break;
            }
        }
    }

    damage = static_cast<int>(damage * multiplier);
    return std::max(1, damage);
}

// ---------------------------------------------------------------------------
// Private: ApplyAoe
// ---------------------------------------------------------------------------

void MagicSystem::ApplyAoe(EntityID caster, TileCoord origin, int radius,
                            const SpellData& spell, TileMap& /*map*/)
{
    // TEACHING NOTE — Iterate all entities with TransformComponent and
    // apply damage to those within `radius` tiles of the origin.
    // We use Chebyshev distance (max of |dx|, |dy|) for a square blast:
    //   |dx| <= radius AND |dy| <= radius  →  square area of effect.
    // (Manhattan distance would give a diamond shape instead.)
    m_world->View<TransformComponent, HealthComponent>(
        [&](EntityID id, TransformComponent& tc, HealthComponent& hc) {
            if (id == caster) return;  // don't hit the caster
            // Convert world position to tile coordinates for distance check.
            int entityTileX = static_cast<int>(tc.position.x / TILE_SIZE);
            int entityTileZ = static_cast<int>(tc.position.z / TILE_SIZE);
            int dx = std::abs(entityTileX - origin.tileX);
            int dy = std::abs(entityTileZ - origin.tileY);
            if (dx <= radius && dy <= radius) {
                int aoe = ComputeSpellDamage(caster, id, spell);
                hc.hp -= aoe;
                if (hc.hp <= 0) { hc.hp = 0; hc.isDead = true; }
            }
        });
}

// ---------------------------------------------------------------------------
// Private: BuildCraftRecipes (static)
// ---------------------------------------------------------------------------

std::vector<MagicCraftRecipe> MagicSystem::BuildCraftRecipes()
{
    // itemID legend (from GameData.hpp):
    //   23 = Potion (stand-in for Fire Shard in simplified system)
    //   24 = Hi-Potion (stand-in for Ice Shard)
    //   25 = Elixir (stand-in for Thunder Shard)
    // In a full implementation these would be dedicated gathering materials.
    return {
        { ElementType::FIRE,      {23}, 3, "Fire Flask Recipe"      },
        { ElementType::ICE,       {24}, 3, "Blizzard Flask Recipe"  },
        { ElementType::LIGHTNING, {25}, 3, "Thunder Flask Recipe"   },
    };
}
