/**
 * @file CombatSystem.hpp
 * @brief Real-time action combat system for the educational RPG engine.
 *
 * ============================================================================
 * TEACHING NOTE — Combat System Architecture
 * ============================================================================
 *
 * What makes combat systems interesting to study is that they must balance
 * three competing concerns at once:
 *
 *  1. GAME DESIGN  — Is fighting fun?  Is it fair?  Is it easy to balance?
 *  2. PERFORMANCE  — Can we evaluate 6 enemies every frame without lag?
 *  3. READABILITY  — Can a new programmer understand the code in minutes?
 *
 * ─── Turn-Based vs Real-Time ────────────────────────────────────────────────
 *
 * Classic JRPGs (Final Fantasy 1-10) use strict turn-based combat: you pick
 * an action, then the enemy picks an action, then both resolve.  This is
 * easy to implement and reason about, but can feel slow.
 *
 * Modern action RPGs (FF15, FF16, Kingdom Hearts) use real-time combat where
 * button presses trigger immediate attacks.  Under the hood, however, most
 * still have a discrete "action resolution" step — they just hide it behind
 * animations that mask the latency.
 *
 * OUR APPROACH:  We implement TURN-BASED resolution (simple, teachable) while
 * using a `turnTimer` float to make it FEEL real-time.  When the timer ticks
 * down, the current actor takes their action and the timer resets.  This is
 * called an "Active Time Battle" (ATB) system, first introduced in FF4.
 *
 * ─── ECS Integration ────────────────────────────────────────────────────────
 *
 * The combat system reads from components (StatsComponent, HealthComponent,
 * etc.) but does NOT store per-entity combat state itself.  Temporary combat
 * state (status effects, cooldowns) lives in StatusEffectsComponent on the
 * entity.  The system only orchestrates WHAT happens, not what entities ARE.
 *
 * ─── Damage Formula ─────────────────────────────────────────────────────────
 *
 * Our formula is inspired by Final Fantasy XV's damage model:
 *
 *   rawDmg  = (attacker.strength * 2 + weapon.attack) - defender.defence
 *   damage  = rawDmg * elementMultiplier * critMultiplier * variance
 *
 * where:
 *   elementMultiplier  = 1.5  if defender is weak to the element
 *                      = 0.5  if defender resists the element
 *                      = 1.0  otherwise
 *   critMultiplier     = 1.5  on critical hit, 1.0 otherwise
 *   variance           = random float in [0.85, 1.15] for natural spread
 *
 * ─── Status Effects ─────────────────────────────────────────────────────────
 *
 * Status effects are stored as a bitmask in StatusEffectsComponent.  On each
 * `statusTickTimer` interval, the combat system iterates all combatants and
 * applies periodic damage (POISON, BURNED) or checks for expiry.
 *
 * ─── Warp-Strike ────────────────────────────────────────────────────────────
 *
 * Warp-Strike is a signature ability from FF15: Noctis throws his weapon to
 * a distant enemy, then warps to it for a powerful strike.  In our engine it
 * teleports the player's TransformComponent to the target, then deals
 * 1.5× normal damage.  No projectile physics needed for the tile-map scale.
 *
 * ─── Link Strikes ───────────────────────────────────────────────────────────
 *
 * When the player lands a hit, each other party member has a 30% chance to
 * automatically "link" in and add 30% of their own attack as bonus damage.
 * This models the party cooperation mechanic from FF15 Techniques.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 *
 * C++ Standard: C++17
 */

#pragma once

// ---------------------------------------------------------------------------
// Standard library includes
// ---------------------------------------------------------------------------
#include <vector>    // std::vector — list of active enemies.
#include <cstdint>   // uint32_t — item and entity IDs.
#include <string>    // std::string — ability names.
#include <random>    // std::mt19937, std::uniform_real_distribution — RNG.

// ---------------------------------------------------------------------------
// Engine includes
// ---------------------------------------------------------------------------
#include "../../engine/core/Types.hpp"      // EntityID, ElementType, StatusEffect, etc.
#include "../../engine/core/Logger.hpp"     // LOG_DEBUG, LOG_INFO, LOG_WARN.
#include "../../engine/core/EventBus.hpp"   // EventBus<CombatEvent>, EventBus<UIEvent>.
#include "../../engine/ecs/ECS.hpp"         // World, all components.

// ---------------------------------------------------------------------------
// Game includes
// ---------------------------------------------------------------------------
#include "../GameData.hpp"   // GameDatabase, ItemData, EnemyData.


// ===========================================================================
// CombatResult — the outcome of one combat encounter
// ===========================================================================

/**
 * @struct CombatResult
 * @brief Summarises what the player gained (or lost) from a combat encounter.
 *
 * TEACHING NOTE — Returning Complex Values
 * ─────────────────────────────────────────
 * Rather than passing multiple out-parameters to GetCombatResult():
 *   GetCombatResult(bool& won, int& xp, int& gil, vector<uint32_t>& items)
 * we bundle everything into one struct.  This makes the call site cleaner:
 *   auto result = combat.GetCombatResult();
 *   if (result.playerWon) { ... }
 *
 * In C++17 you can also use structured bindings:
 *   auto [won, xp, gil, drops] = combat.GetCombatResult();  // if we add ==
 * But for struct types, named member access is usually clearer.
 */
struct CombatResult {
    bool                 playerWon   = false; ///< true if all enemies were defeated.
    int                  xpGained    = 0;     ///< Total XP rewarded.
    int                  gilGained   = 0;     ///< Total Gil rewarded.
    std::vector<uint32_t> itemsDropped;        ///< Item IDs that dropped (may be empty).
};


// ===========================================================================
// CombatSystem class
// ===========================================================================

/**
 * @class CombatSystem
 * @brief Orchestrates all combat logic for the player party vs. enemies.
 *
 * USAGE EXAMPLE:
 * @code
 *   CombatSystem combat;
 *   combat.StartCombat(playerID, {goblin1, goblin2});
 *
 *   // Inside game loop:
 *   combat.Update(deltaTime);
 *
 *   // On player button press:
 *   int dmg = combat.PlayerAttack(goblin1);
 *
 *   // Check if fight is over:
 *   if (!combat.IsActive()) {
 *       auto result = combat.GetCombatResult();
 *   }
 * @endcode
 */
class CombatSystem {
public:

    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------

    /**
     * @brief Construct a CombatSystem attached to the given ECS World.
     *
     * @param world       Pointer to the ECS World — NOT owned by CombatSystem.
     *
     * TEACHING NOTE — Dependency Injection
     * ──────────────────────────────────────
     * We pass the World as a constructor parameter rather than using a global
     * variable.  This is "dependency injection": the CombatSystem DECLARES its
     * dependencies clearly, making it easier to:
     *   • Unit-test (swap in a mock World).
     *   • Reason about lifetime (the World must outlive the CombatSystem).
     *   • Avoid hidden coupling through globals.
     */
    explicit CombatSystem(World* world);

    // -----------------------------------------------------------------------
    // Combat lifecycle
    // -----------------------------------------------------------------------

    /**
     * @brief Begin a new combat encounter.
     *
     * Initialises turn order, resets timers, and fires a BATTLE_START event.
     * Does nothing if combat is already active.
     *
     * @param player   EntityID of the player / party lead.
     * @param enemies  Vector of EntityIDs of all participating enemies.
     */
    void StartCombat(EntityID player, std::vector<EntityID> enemies);

    /**
     * @brief Advance the combat simulation by `dt` seconds.
     *
     * This drives the ATB (Active Time Battle) timer.  When the timer
     * expires for the current actor, that actor takes their turn.
     *
     * Also ticks status effects every `statusTickTimer` interval.
     *
     * @param dt  Delta time in seconds since the last frame.
     *
     * TEACHING NOTE — Delta Time
     * ────────────────────────────
     * `dt` (delta time) is the elapsed real time since the last Update() call.
     * Using dt for timers instead of frame counts decouples game logic from
     * frame rate.  A 60 FPS game and a 30 FPS game both see the same timer
     * behaviour because:
     *   timer -= dt;   // at 60 FPS: timer -= 0.0167 per frame
     *                  // at 30 FPS: timer -= 0.0333 per frame
     * Both halve the timer at the same wall-clock rate.
     */
    void Update(float dt);

    // -----------------------------------------------------------------------
    // Player actions
    // -----------------------------------------------------------------------

    /**
     * @brief Execute a standard physical attack against `target`.
     *
     * Calculates damage using the core damage formula, applies crit/dodge
     * rolls, triggers link strikes from party members (30% each), and
     * publishes a CombatEvent::DAMAGE_DEALT.
     *
     * @param target  EntityID of the enemy to attack.
     * @return        Damage dealt (0 if the attack missed).
     */
    int PlayerAttack(EntityID target);

    /**
     * @brief Activate the skill at position `skillIndex` in the player's
     *        equipped skill slots against `target`.
     *
     * @param skillIndex  Slot index (0–3) in SkillsComponent::equippedSkills.
     * @param target      EntityID of the target.
     * @return            Damage dealt, or 0 if the skill has no damage component.
     */
    int PlayerUseSkill(int skillIndex, EntityID target);

    /**
     * @brief Cast a spell by ID against `target`.
     *
     * Deducts MP, applies elemental multipliers, and calls the spell's
     * Lua callback if defined.
     *
     * @param spellID  ID into GameDatabase::GetSpells().
     * @param target   EntityID of the target.
     * @return         Damage dealt, or healing applied (positive for both).
     */
    int PlayerCastSpell(uint32_t spellID, EntityID target);

    /**
     * @brief Use a consumable item from the player's inventory during combat.
     *
     * @param itemID   Item to use (must be a consumable).
     * @param target   EntityID to apply the effect to (can be self or ally).
     */
    void PlayerUseItem(uint32_t itemID, EntityID target);

    /**
     * @brief Perform a Warp-Strike on `target`.
     *
     * Teleports the player's TransformComponent adjacent to the target,
     * then deals 1.5× normal physical damage.  This is a signature ability
     * inspired by Final Fantasy XV.
     *
     * @param target  EntityID of the enemy to warp to.
     * @return        Damage dealt.
     */
    int WarpStrike(EntityID target);

    /**
     * @brief Attempt to flee from the current encounter.
     *
     * Base flee chance: 30%.
     * Speed modifier: each point of (player.speed - average enemy.speed)
     * adds 1% to the flee chance, capped at 90%.
     *
     * @return true if the player successfully escapes, false otherwise.
     */
    bool PlayerFlee();

    // -----------------------------------------------------------------------
    // Enemy actions
    // -----------------------------------------------------------------------

    /**
     * @brief Process one turn for the given enemy entity.
     *
     * The enemy AI selects an action (attack, ability, or special) and
     * resolves it against the player.
     *
     * @param enemy  EntityID of the enemy taking its turn.
     *
     * TEACHING NOTE — Enemy AI in Combat
     * ────────────────────────────────────
     * We separate STRATEGIC AI (handled by AISystem: patrol, chase, flee)
     * from TACTICAL AI (handled here: which action to use in battle).
     * This keeps each class focused on one responsibility (Single Responsibility
     * Principle from SOLID design principles).
     */
    void EnemyTurn(EntityID enemy);

    // -----------------------------------------------------------------------
    // Shared calculation helpers (public so tests can call them)
    // -----------------------------------------------------------------------

    /**
     * @brief Compute the final damage between two combatants.
     *
     * Formula:
     *   rawDmg   = max(1, attacker.strength*2 + weapon.attack - defender.defence)
     *   scaled   = rawDmg * elementMultiplier
     *   variance = random in [0.85, 1.15]
     *   final    = (int)(scaled * variance)
     *
     * @param attacker  EntityID of the attacking entity.
     * @param defender  EntityID of the defending entity.
     * @param baseDmg   Extra flat damage bonus from the ability/weapon.
     * @param element   Elemental type of the attack.
     * @param dmgType   Physical, Magical, or True damage.
     * @return          Final integer damage to subtract from HP.
     */
    int CalculateDamage(EntityID attacker, EntityID defender,
                        int baseDmg, ElementType element,
                        DamageType dmgType);

    /**
     * @brief Apply a status effect to `target` for `duration` seconds.
     *
     * If the entity already has the effect, refreshes its duration.
     * Publishes a CombatEvent::STATUS_APPLIED event.
     *
     * @param target    Entity to affect.
     * @param effect    Which StatusEffect flag to apply.
     * @param duration  Duration in real-time seconds before expiry.
     */
    void ApplyStatusEffect(EntityID target, StatusEffect effect, float duration);

    /**
     * @brief Scan all combatants for zero HP and mark them dead.
     *
     * Sets HealthComponent::isDead, removes the entity from the active
     * enemy list, and publishes CombatEvent::ENTITY_DIED.
     * If the player dies, publishes CombatEvent::PARTY_KO and ends combat.
     */
    void CheckDeaths();

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------

    /// @return true if a combat encounter is currently running.
    bool IsActive() const { return m_isActive; }

    /**
     * @brief Return a copy of the current result struct.
     *
     * Only meaningful after IsActive() returns false (fight is over).
     * If called mid-combat, fields will be zero / incomplete.
     */
    CombatResult GetCombatResult() const { return m_result; }

    /// @return EntityIDs of enemies that are still alive in the current fight.
    std::vector<EntityID> GetActiveEnemies() const;

private:

    // -----------------------------------------------------------------------
    // Private helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Compute the elemental damage multiplier.
     *
     * Reads the defender's EnemyData (from GameDatabase) to find weakness
     * and resistance fields.  If the defender is a player entity, returns 1.0.
     *
     * @param defender  Entity being attacked.
     * @param element   Elemental type of the incoming attack.
     * @return 1.5 (weakness) / 0.5 (resistance) / 1.0 (neutral).
     */
    float GetElementMultiplier(EntityID defender, ElementType element) const;

    /**
     * @brief Roll for a critical hit based on the attacker's luck stat.
     *
     * Crit chance formula: min(attacker.luck / 100.0, 0.50)
     * A luck stat of 100 → 100% crit (capped at 50%).
     *
     * @return true if the hit should deal 1.5× damage.
     */
    bool RollCrit(EntityID attacker) const;

    /**
     * @brief Roll a dodge check.
     *
     * Dodge chance formula: max(0, (defender.speed - attacker.speed) / 200.0)
     * Faster defenders have a small chance to avoid attacks.
     *
     * @return true if the attack is dodged (0 damage).
     */
    bool RollDodge(EntityID attacker, EntityID defender) const;

    /**
     * @brief Process one link-strike opportunity for a party member.
     *
     * Each non-player party member that is alive has a 30% chance to auto-
     * attack the target for 30% of their normal attack damage.
     *
     * @param target  Enemy receiving the bonus hits.
     * @return        Total bonus damage from all link strikes.
     */
    int ProcessLinkStrikes(EntityID target);

    /**
     * @brief Tick all active status effects on `entity`.
     *
     * Decrements remaining durations, applies periodic damage for DoT
     * effects (POISON, BURNED), and removes effects that have expired.
     *
     * @param entity  Entity whose status effects should be processed.
     * @param dt      Delta time in seconds.
     */
    void TickStatusEffects(EntityID entity, float dt);

    /**
     * @brief Award XP and Gil to the player for defeating `enemy`.
     *
     * XP formula: base.xpReward * (1 + max(0, enemyLevel - playerLevel) * 0.1)
     * This gives a 10% XP bonus per level that the enemy exceeds the player,
     * rewarding players who fight above their level.
     */
    void AwardLoot(EntityID enemy);

    /**
     * @brief Advance the turn counter to the next alive combatant.
     *
     * Turns cycle: 0 = player, 1 = enemies[0], 2 = enemies[1], …
     * Skips dead entities automatically.
     */
    void AdvanceTurn();

    // -----------------------------------------------------------------------
    // Member data
    // -----------------------------------------------------------------------

    World*                m_world           = nullptr; ///< Non-owning ptr to ECS World.
    EntityID              m_playerID        = NULL_ENTITY;
    std::vector<EntityID> m_enemies;          ///< All enemies (including dead ones).
    std::vector<EntityID> m_aliveEnemies;     ///< Cache of living enemies.
    int                   m_turn             = 0;  ///< 0=player, 1+=enemy index.
    bool                  m_isActive         = false;
    CombatResult          m_result;

    // ATB timer — when it reaches zero, the current actor takes their turn.
    float m_turnTimer       = 0.0f;
    float m_turnDuration    = 2.0f;  ///< Seconds per turn (configurable).

    // Status effect tick interval
    float m_statusTickTimer = 0.0f;
    static constexpr float STATUS_TICK_INTERVAL = 1.0f; ///< Tick every 1 second.

    // Random number generation (Mersenne Twister — fast, good distribution)
    mutable std::mt19937 m_rng{42};  ///< Seeded for reproducibility in tests.

    // Enemy ID → EnemyData cache to avoid repeated database lookups.
    // Maps runtime EntityID to species data ID.
    // (The species data ID is stored in NameComponent or CombatComponent;
    //  here we use the entity's CombatComponent::xpReward and gilReward.)
};
