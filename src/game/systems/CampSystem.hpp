/**
 * @file CampSystem.hpp
 * @brief Camp, rest, and cooking system for the educational RPG engine.
 *
 * ============================================================================
 * TEACHING NOTE — Camp System Inspiration (FF15)
 * ============================================================================
 *
 * Final Fantasy XV's camp system is one of the most beloved features in
 * the series.  The key design insight: camping is NOT just a "save and
 * rest" button.  It is a RITUAL that:
 *
 *  1. Converts PENDING XP → ACTUAL LEVELS (delayed gratification).
 *  2. Applies MEAL BUFFS crafted by Ignis (strategic food preparation).
 *  3. ADVANCES TIME (narrative consequence — Ardyn's demons are stronger
 *     at night, so dawn camping choices are meaningful).
 *  4. Provides SOCIAL MOMENTS (party banter — not modelled in this engine
 *     but mentioned for completeness).
 *
 * Our CampSystem captures points 1-3.
 *
 * ─── Delayed XP Levelling ────────────────────────────────────────────────────
 *
 * Combat grants XP to LevelComponent::pendingXP.  It is NOT immediately
 * added to currentXP.  When Rest() is called, ApplyBankedXP() is invoked
 * on the LevelComponent, converting pending XP into actual level-ups.
 *
 * Why delay?  It creates a tension: "Do I explore more to bank more XP,
 * or rest now before I die?".  It also avoids mid-dungeon level-up UI
 * interrupting tense gameplay.
 *
 * ─── Meal Buffs ──────────────────────────────────────────────────────────────
 *
 * Each recipe (RecipeData in GameData.hpp) provides temporary stat bonuses
 * stored in CampComponent.  The CampSystem copies the bonus values from
 * RecipeData into CampComponent fields after cooking.  The CombatSystem
 * reads CampComponent to add meal bonuses to damage calculations.
 *
 * ─── ActiveMealBuff ──────────────────────────────────────────────────────────
 *
 * A single `static ActiveMealBuff currentBuff` tracks the current meal
 * across the game session.  Making it static (rather than per-entity) is
 * a simplification for this educational engine — in a multiplayer or
 * multi-party game, you'd need one buff per character.
 *
 * ─── Safe Camping ────────────────────────────────────────────────────────────
 *
 * CanCamp() checks:
 *   1. The tile at (x, y) has TileType::CAMP_SITE or is GRASS/PLAINS
 *      (not inside a dungeon — TileType::WALL or DUNGEON tiles block camping).
 *   2. No enemies are in close proximity (checked via AIComponent::state).
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
#include "../world/TileMap.hpp"


// ===========================================================================
// ActiveMealBuff — the currently active camp meal bonus
// ===========================================================================

/**
 * @struct ActiveMealBuff
 * @brief Holds the stat bonuses from the most recently cooked meal.
 *
 * TEACHING NOTE — Static Data for Game-Global State
 * ──────────────────────────────────────────────────
 * `CampSystem::currentBuff` is declared `static` because there is only
 * ONE active meal at a time in the game, regardless of how many CampSystem
 * instances exist.  Class-level (static) data is initialised ONCE for the
 * whole program.
 *
 * Alternatives:
 *   - Store in CampComponent on the player entity (preferred in production).
 *   - Store in a global singleton GameState struct.
 *
 * For this educational engine, the static field keeps the example simple
 * while still demonstrating the concept of "game-global state".
 */
struct ActiveMealBuff {
    uint32_t recipeID       = 0;   ///< 0 means no buff active.
    int      turnsRemaining = 0;   ///< Combat turns left until the buff expires.
    int      hpBonus        = 0;   ///< Bonus max HP while active.
    int      mpBonus        = 0;   ///< Bonus max MP while active.
    int      attackBonus    = 0;   ///< Bonus physical attack.
    int      defenseBonus   = 0;   ///< Bonus physical defence.
    int      magicBonus     = 0;   ///< Bonus magic power.

    /// @return true if this buff is currently active (has turns remaining).
    bool IsActive() const { return recipeID != 0 && turnsRemaining > 0; }

    /// Decrement the turn counter; clears the buff when turns reach zero.
    void Tick() {
        if (turnsRemaining > 0) {
            --turnsRemaining;
            if (turnsRemaining == 0) {
                recipeID = 0;  // buff expired
            }
        }
    }
};


// ===========================================================================
// CampSystem class
// ===========================================================================

/**
 * @class CampSystem
 * @brief Manages setting up camp, cooking meals, and resting.
 *
 * USAGE EXAMPLE:
 * @code
 *   CampSystem camp(world, &EventBus<UIEvent>::Instance());
 *
 *   // Check if the player can camp here:
 *   if (camp.CanCamp(tileMap, playerTileX, playerTileY)) {
 *       camp.SetupCamp(playerID);
 *       camp.CookMeal(playerID, 1);  // recipeID=1 "Fried Rice"
 *       camp.Rest(playerID);          // restores HP/MP, levels up, applies buff
 *   }
 * @endcode
 */
class CampSystem {
public:

    /**
     * @brief Construct the camp system.
     *
     * @param world   Non-owning pointer to ECS World.
     * @param uiBus   Non-owning pointer to UI event bus.
     */
    CampSystem(World* world, EventBus<UIEvent>* uiBus);

    // -----------------------------------------------------------------------
    // Camp setup / teardown
    // -----------------------------------------------------------------------

    /**
     * @brief Check if camping is permitted at tile position (x, y).
     *
     * Safe camping requires:
     *  - The tile at (x, y) is not a dungeon/wall tile.
     *  - The tile type allows camping (PLAINS, GRASS, or a designated CAMP_SITE).
     *  - No enemies with an ATTACKING or CHASING AI state are nearby.
     *
     * @param map  Reference to the current zone's tile map.
     * @param x    Tile column.
     * @param y    Tile row.
     * @return true if camping is safe here.
     *
     * TEACHING NOTE — Const References for Read-Only Parameters
     * ───────────────────────────────────────────────────────────
     * We pass `map` as a const reference because we only need to READ tile
     * data.  Using const& instead of a pointer communicates to the reader
     * that: (1) the parameter is never null, and (2) the function will not
     * modify the map.  This is a good C++ practice for "inspect but don't
     * modify" parameters.
     */
    bool CanCamp(const TileMap& map, int x, int y) const;

    /**
     * @brief Set up a camp for the player at their current location.
     *
     * Sets CampComponent::isCamping = true.
     * Fires UIEvent::SHOW_NOTIFICATION "Camp set up."
     * Fires WorldEvent::CAMP_STARTED.
     *
     * @param player  Player entity.
     */
    void SetupCamp(EntityID player);

    /**
     * @brief Break down the camp and return to exploration.
     *
     * Sets CampComponent::isCamping = false.
     * Fires UIEvent::SHOW_NOTIFICATION "Broke camp."
     *
     * @param player  Player entity.
     */
    void BreakCamp(EntityID player);

    /// @return true if the player's CampComponent::isCamping flag is set.
    bool IsAtCamp(EntityID player) const;

    // -----------------------------------------------------------------------
    // Cooking
    // -----------------------------------------------------------------------

    /**
     * @brief Cook recipe `recipeID` using items from the player's inventory.
     *
     * Steps:
     *  1. Look up RecipeData in GameDatabase.
     *  2. Check the player has all required ingredients.
     *  3. Consume ingredients (remove from inventory).
     *  4. Store meal buff data in CampComponent / currentBuff.
     *  5. Publish UIEvent::SHOW_NOTIFICATION "Ignis: Leave the cooking to me."
     *
     * @param player    Player entity (must be camping).
     * @param recipeID  ID in GameDatabase::GetRecipes().
     * @return true if cooking succeeded.
     *
     * TEACHING NOTE — Ingredient Validation
     * ──────────────────────────────────────
     * We ALWAYS check ALL ingredients before consuming ANY.  This prevents
     * the frustrating situation where the game consumes half your items and
     * then fails because of the last ingredient.  "All-or-nothing" validation
     * is a core pattern in any transaction-like operation.
     */
    bool CookMeal(EntityID player, uint32_t recipeID);

    /**
     * @brief Return IDs of all recipes the player can currently cook.
     *
     * Checks the player's inventory against each recipe in GameDatabase.
     *
     * @param player  Player entity.
     * @return IDs of cookable recipes (may be empty).
     */
    std::vector<uint32_t> GetAvailableRecipes(EntityID player) const;

    // -----------------------------------------------------------------------
    // Resting
    // -----------------------------------------------------------------------

    /**
     * @brief Rest at camp: restore HP/MP, apply banked XP, and advance time.
     *
     * Steps performed:
     *  1. Restore HealthComponent::hp to maxHp and mp to maxMp.
     *  2. Call LevelComponent::ApplyBankedXP() — converts pendingXP to levels.
     *  3. Apply the current meal buff (if any) to the CampComponent fields.
     *  4. Advance game time by 8 hours (overnight).
     *  5. Publish UIEvent::SHOW_NOTIFICATION for each level-up.
     *  6. Publish WorldEvent::DAY_ADVANCED if midnight passes.
     *
     * @param player  Player entity.
     *
     * TEACHING NOTE — Why Rest is a Rich System
     * ───────────────────────────────────────────
     * Many students initially implement "rest" as just `hp = maxHp`.
     * But rich rest mechanics create meaningful decisions:
     *   - Time advances → some quests/NPCs are time-sensitive.
     *   - Levelling happens → plan your ability upgrades.
     *   - Meal buffs expire → choose the right recipe for tomorrow's combat.
     */
    void Rest(EntityID player);

    // -----------------------------------------------------------------------
    // Global buff tracker
    // -----------------------------------------------------------------------

    /**
     * @brief The currently active meal buff for the entire game session.
     *
     * `static` means all CampSystem instances share this one buff record.
     * The CombatSystem should call `currentBuff.Tick()` at the end of each
     * combat turn to decrement the duration.
     *
     * TEACHING NOTE — When to Use Static Members
     * ────────────────────────────────────────────
     * Use static members for data that is genuinely SINGULAR: the current
     * meal, the game clock, the player's currency.  Avoid statics for data
     * that could plausibly need multiple instances in a future design.
     */
    static ActiveMealBuff currentBuff;

private:

    /**
     * @brief Check if the player's inventory contains all ingredients for recipe.
     *
     * @return true if every RecipeIngredient.quantity can be satisfied.
     */
    bool HasIngredients(EntityID player, const RecipeData& recipe) const;

    /**
     * @brief Remove all ingredients for recipe from the player's inventory.
     *
     * Called only after HasIngredients() returns true.
     */
    void ConsumeIngredients(EntityID player, const RecipeData& recipe);

    World*             m_world = nullptr;
    EventBus<UIEvent>* m_uiBus = nullptr;
};
