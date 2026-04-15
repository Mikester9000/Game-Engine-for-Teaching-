/**
 * @file MagicSystem.hpp
 * @brief Magic crafting and casting system for the educational RPG engine.
 *
 * ============================================================================
 * TEACHING NOTE — Magic System Design: FF15 Inspired
 * ============================================================================
 *
 * Final Fantasy XV introduced a unique magic system where spells are NOT
 * purchased from shops.  Instead, the player:
 *   1. Gathers elemental energy from sources in the world (Elemancy).
 *   2. Crafts spells (called "Flasks") by combining elemental energy with items.
 *   3. Throws the flasks in combat — they can hit allies too, making them
 *      dangerous to use carelessly.
 *
 * This creates interesting resource management gameplay.  We simplify slightly:
 *
 * OUR APPROACH:
 *   - Raw elemental items (collected in the world) map to ElementType.
 *   - CraftMagic() consumes ingredients and adds spell uses to MagicComponent.
 *   - CastSpell() deducts MP (the "flask" charge) and applies spell effects.
 *
 * ─── Spell as Data ──────────────────────────────────────────────────────────
 *
 * We follow the "data-driven" philosophy: spells are rows in SpellData[]
 * (in GameData.hpp), not C++ subclasses.  The MagicSystem reads SpellData and
 * executes GENERIC behaviour:
 *
 *   1. Deduct mpCost from caster's MP.
 *   2. Calculate baseDamage × caster.magic × element multiplier.
 *   3. If aoeRadius > 0, apply to all entities within radius on the TileMap.
 *   4. If luaCastCallback is set, call the Lua function for special effects.
 *
 * This makes it trivial to add new spells: just add a row to GameData.
 * No new C++ code required.
 *
 * ─── Lua Integration ────────────────────────────────────────────────────────
 *
 * Special spell effects (terrain modification, summons, time stop) are too
 * complex to encode in a flat data struct.  We delegate them to Lua scripts
 * via LuaEngine::CallFunction().  Students can modify spell behaviour by
 * editing Lua files without recompiling C++.
 *
 * ─── AoE and TileMap ────────────────────────────────────────────────────────
 *
 * Area-of-effect spells pass a TileMap reference to find all entities near
 * the impact point.  This teaches students about spatial queries on 2D grids.
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
#include "../../engine/scripting/LuaEngine.hpp"
#include "../GameData.hpp"
#include "../world/TileMap.hpp"


// ===========================================================================
// MagicCraftRecipe — describes how to craft one type of magic flask
// ===========================================================================

/**
 * @struct MagicCraftRecipe
 * @brief Defines the ingredients needed to craft a batch of elemental magic.
 *
 * TEACHING NOTE — Recipe as a Value Type
 * ─────────────────────────────────────────
 * A MagicCraftRecipe is a pure data struct (no methods, no pointers).
 * This makes it trivially copyable and safe to store in a static vector.
 *
 * The `ingredientItemIDs` vector lists item IDs that are consumed when
 * crafting.  The resulting `quantity` units are added to the entity's
 * MagicComponent as charges for spells of the given element.
 *
 * Example: crafting Fire magic requires itemID=5 (Fire Shard) × 2 and
 * produces 5 fire casts.
 */
struct MagicCraftRecipe {
    ElementType             element;          ///< Which element is produced.
    std::vector<uint32_t>   ingredientItemIDs; ///< Items consumed (one of each).
    int                     quantity = 1;      ///< Number of spell casts produced.
    std::string             name;              ///< Human-readable recipe name.
};


// ===========================================================================
// MagicSystem class
// ===========================================================================

/**
 * @class MagicSystem
 * @brief Handles spell learning, crafting, and casting.
 *
 * USAGE EXAMPLE:
 * @code
 *   MagicSystem magic(world, &LuaEngine::Get());
 *
 *   // Learn a spell:
 *   magic.LearnSpell(playerID, 3);  // spellID=3 is "Fira"
 *
 *   // Cast it:
 *   int damage = magic.CastSpell(playerID, 3, enemyID, tilemap);
 *
 *   // Craft fire magic from materials:
 *   magic.CraftMagic(playerID, ElementType::FIRE, 3);
 * @endcode
 */
class MagicSystem {
public:

    /**
     * @brief Construct the magic system.
     *
     * @param world  Non-owning pointer to ECS World.
     * @param lua    Non-owning pointer to the global LuaEngine (may be null).
     */
    MagicSystem(World* world, LuaEngine* lua);

    // -----------------------------------------------------------------------
    // Spell learning
    // -----------------------------------------------------------------------

    /**
     * @brief Add spell `spellID` to the entity's known spells list.
     *
     * Checks MagicComponent::knownSpells — does not add duplicates.
     * Returns false if the entity has no MagicComponent.
     *
     * @param entity   Entity to teach the spell to.
     * @param spellID  ID in GameDatabase::GetSpells().
     * @return true if the spell was added (false if already known or invalid).
     */
    bool LearnSpell(EntityID entity, uint32_t spellID);

    // -----------------------------------------------------------------------
    // Casting
    // -----------------------------------------------------------------------

    /**
     * @brief Cast spell `spellID` from `caster` at `target`.
     *
     * Steps performed:
     *  1. Validate: caster knows the spell, has enough MP.
     *  2. Deduct mpCost from HealthComponent::mp.
     *  3. Compute base damage: spellData.baseDamage × caster.magic.
     *  4. Apply element multiplier against target's weakness/resistance.
     *  5. If aoeRadius > 0, find and damage all entities within radius.
     *  6. Call luaCastCallback if defined.
     *  7. Publish CombatEvent::DAMAGE_DEALT.
     *
     * @param caster  Entity casting the spell.
     * @param spellID Spell to cast.
     * @param target  Primary target entity.
     * @param map     TileMap reference (used for AoE radius checks).
     * @return        Total damage dealt to the primary target (or 0 for buffs).
     *
     * TEACHING NOTE — AoE via TileMap
     * ─────────────────────────────────
     * For AoE spells, we iterate over all entities with TransformComponent
     * and test whether their tile coordinates fall within `aoeRadius` tiles
     * of the target's position.  This is a linear scan — O(N) where N is
     * the number of entities with TransformComponents.  For small maps this
     * is acceptable.  Spatial hashing or a quadtree would be needed for
     * thousands of entities.
     */
    int CastSpell(EntityID caster, uint32_t spellID,
                  EntityID target, TileMap& map);

    // -----------------------------------------------------------------------
    // Magic crafting (Elemancy system)
    // -----------------------------------------------------------------------

    /**
     * @brief Craft `quantity` uses of elemental magic from inventory items.
     *
     * Looks up the recipe for `element` in GetCraftRecipes().  For each use:
     *   - Checks the entity's inventory for the required ingredient items.
     *   - Removes them.
     *   - Adds the spell uses (as MP or a dedicated craft counter) to
     *     MagicComponent.
     *
     * @param entity    Entity doing the crafting.
     * @param element   Which element to craft (matches a MagicCraftRecipe).
     * @param quantity  Number of uses to craft (each use consumes ingredients).
     * @return true if crafting succeeded for all `quantity` uses.
     *
     * TEACHING NOTE — Consuming Multiple Resource Types
     * ───────────────────────────────────────────────────
     * This teaches "multi-resource consumption" patterns common in crafting
     * systems.  We loop `quantity` times and remove each ingredient batch
     * with RemoveItem.  If any removal fails (not enough items), we stop
     * early and return false — no partial crafting is performed.
     */
    bool CraftMagic(EntityID entity, ElementType element, int quantity);

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------

    /**
     * @brief Return the list of spell IDs the entity currently knows.
     *
     * Returns empty vector if the entity has no MagicComponent.
     */
    std::vector<uint32_t> GetKnownSpells(EntityID entity) const;

    /**
     * @brief Check whether the entity can cast spell `spellID`.
     *
     * Conditions:
     *  - Entity has MagicComponent.
     *  - spellID is in knownSpells.
     *  - Entity's current MP >= spellData.mpCost.
     */
    bool CanCast(EntityID entity, uint32_t spellID) const;

    // -----------------------------------------------------------------------
    // Static recipe database
    // -----------------------------------------------------------------------

    /**
     * @brief Return the global list of elemental crafting recipes.
     *
     * The recipe list is built once (static local initialiser) and returned
     * by const reference.  It cannot be modified at runtime.
     *
     * TEACHING NOTE — Static Local Initialisation (C++11)
     * ─────────────────────────────────────────────────────
     * Using `static const vector<…> recipes = BuildRecipes()` inside the
     * function body means the vector is constructed exactly once, the first
     * time the function is called, and then reused on all subsequent calls.
     * This is thread-safe in C++11 (initialisation is guaranteed to happen
     * exactly once even with concurrent calls).
     */
    static const std::vector<MagicCraftRecipe>& GetCraftRecipes();

private:

    /**
     * @brief Compute damage for a spell hit on a single target.
     *
     * @param caster    Casting entity (reads StatsComponent::magic).
     * @param target    Target entity (reads weakness/resistance from EnemyData).
     * @param spellData Spell definition from GameDatabase.
     * @return Final integer damage.
     */
    int ComputeSpellDamage(EntityID caster, EntityID target,
                           const SpellData& spellData) const;

    /**
     * @brief Apply an AoE spell to all entities within radius tiles.
     *
     * Uses the target entity's TransformComponent as the origin.
     * Hits all entities (including allies!) within `radius` tiles.
     *
     * @param caster     Casting entity (not targeted by own AoE).
     * @param origin     Centre point of the explosion.
     * @param radius     Blast radius in tiles.
     * @param spellData  Spell definition.
     * @param map        TileMap for bounds checking.
     */
    void ApplyAoe(EntityID caster, TileCoord origin, int radius,
                  const SpellData& spellData, TileMap& map);

    /**
     * @brief Build the static list of elemental craft recipes.
     *
     * Defines one recipe per ElementType that has an associated crafting path.
     */
    static std::vector<MagicCraftRecipe> BuildCraftRecipes();

    World*      m_world = nullptr;
    LuaEngine*  m_lua   = nullptr;
};
