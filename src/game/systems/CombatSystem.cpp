/**
 * @file CombatSystem.cpp
 * @brief Implementation of the real-time action combat system.
 *
 * See CombatSystem.hpp for detailed teaching notes on the design.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 */

#include "CombatSystem.hpp"

#include <algorithm>   // std::remove, std::max, std::min
#include <cmath>       // std::abs, std::sqrt
#include <stdexcept>   // std::out_of_range

// ===========================================================================
// Construction
// ===========================================================================

CombatSystem::CombatSystem(World* world)
    : m_world(world)
{
    // Seed the RNG with a non-deterministic value for live gameplay.
    // (Constructor uses seed 42 in the header for reproducibility in tests;
    //  this line re-seeds for production use.)
    std::random_device rd;
    m_rng.seed(rd());

    LOG_DEBUG("CombatSystem constructed.");
}

// ===========================================================================
// Combat lifecycle
// ===========================================================================

void CombatSystem::StartCombat(EntityID player,
                                std::vector<EntityID> enemies)
{
    // ── Guard: don't start a new fight if one is already running ─────────
    if (m_isActive) {
        LOG_WARN("CombatSystem::StartCombat called while combat is already active.");
        return;
    }

    // ── Validate player ────────────────────────────────────────────────────
    if (!m_world->HasComponent<HealthComponent>(player)) {
        LOG_ERROR("CombatSystem::StartCombat: player entity has no HealthComponent.");
        return;
    }

    m_playerID = player;
    m_enemies  = std::move(enemies);
    m_aliveEnemies.clear();

    // Build the alive list — exclude any enemies already dead at encounter start.
    for (EntityID e : m_enemies) {
        if (m_world->HasComponent<HealthComponent>(e)) {
            auto& hp = m_world->GetComponent<HealthComponent>(e);
            if (!hp.isDead) {
                m_aliveEnemies.push_back(e);
            }
        }
    }

    if (m_aliveEnemies.empty()) {
        LOG_WARN("CombatSystem::StartCombat: no living enemies provided.");
        return;
    }

    // ── Reset state ────────────────────────────────────────────────────────
    m_turn            = 0;        // Player goes first.
    m_turnTimer       = m_turnDuration;
    m_statusTickTimer = STATUS_TICK_INTERVAL;
    m_isActive        = true;

    // Clear the result from any previous encounter.
    m_result = CombatResult{};

    // ── Broadcast BATTLE_START ─────────────────────────────────────────────
    // TEACHING NOTE — Using the EventBus Singleton
    // EventBus<CombatEvent>::Instance() returns the global bus for CombatEvents.
    // Any system that subscribed (UI, audio, camera) will receive this event.
    CombatEvent ev;
    ev.type     = CombatEvent::Type::SKILL_USED; // Closest type for battle start.
    ev.sourceID = player;
    EventBus<CombatEvent>::Instance().Publish(ev);

    // Notify the UI to show the battle HUD.
    UIEvent uiEv;
    uiEv.type = UIEvent::Type::OPEN_MENU;
    uiEv.text = "Battle Start!";
    EventBus<UIEvent>::Instance().Publish(uiEv);

    LOG_INFO("CombatSystem: Battle started. Player vs " << m_aliveEnemies.size() << " enemies.");
}

// ---------------------------------------------------------------------------
// Update — drives the ATB (Active Time Battle) timer
// ---------------------------------------------------------------------------

void CombatSystem::Update(float dt)
{
    // Nothing to do outside of combat.
    if (!m_isActive) return;

    // ── Tick status effects on all combatants ─────────────────────────────
    // TEACHING NOTE — Status Effect Ticking
    // We accumulate time in m_statusTickTimer.  When it crosses the interval,
    // we process all DoT/buff effects and reset the timer.
    // This prevents status effects from being processed EVERY frame (wasteful)
    // while still applying them on a consistent real-time cadence.
    m_statusTickTimer -= dt;
    if (m_statusTickTimer <= 0.0f) {
        m_statusTickTimer = STATUS_TICK_INTERVAL;

        // Tick the player.
        if (m_world->HasComponent<StatusEffectsComponent>(m_playerID)) {
            TickStatusEffects(m_playerID, STATUS_TICK_INTERVAL);
        }
        // Tick each alive enemy.
        for (EntityID e : m_aliveEnemies) {
            if (m_world->HasComponent<StatusEffectsComponent>(e)) {
                TickStatusEffects(e, STATUS_TICK_INTERVAL);
            }
        }

        // After status ticks, check if anyone died.
        CheckDeaths();
    }

    // ── ATB turn timer ────────────────────────────────────────────────────
    m_turnTimer -= dt;
    if (m_turnTimer > 0.0f) return; // Not yet time for an action.

    m_turnTimer = m_turnDuration; // Reset for next turn.

    // ── Enemy turns (player turn is triggered by input, not by this timer) ─
    // TEACHING NOTE — Player vs. Enemy Action Timing
    // m_turn == 0 means it is the player's turn.  Player actions are triggered
    // by button presses (PlayerAttack, PlayerFlee, etc.), not by the ATB timer.
    // Enemy turns ARE driven by the ATB timer: when the timer fires for turn>0,
    // the enemy at index (m_turn-1) acts automatically.
    if (m_turn > 0) {
        int enemyIdx = m_turn - 1;
        if (enemyIdx < static_cast<int>(m_aliveEnemies.size())) {
            EnemyTurn(m_aliveEnemies[enemyIdx]);
        }
        AdvanceTurn();
    }
}

// ===========================================================================
// Player actions
// ===========================================================================

int CombatSystem::PlayerAttack(EntityID target)
{
    if (!m_isActive || m_turn != 0) {
        LOG_WARN("PlayerAttack called outside of player's turn.");
        return 0;
    }

    // Check target is in this encounter and still alive.
    if (!m_world->HasComponent<HealthComponent>(target)) return 0;
    auto& targetHp = m_world->GetComponent<HealthComponent>(target);
    if (targetHp.isDead) return 0;

    // ── Compute base damage (weapon attack from equipment) ─────────────────
    // TEACHING NOTE — Reading Equipment Stats
    // The weapon's attack bonus is stored as a CACHED value in
    // EquipmentComponent::bonusStrength to avoid re-scanning the database
    // on every attack.  RecalculateEquipStats() must be called whenever
    // equipment changes (handled by InventorySystem).
    int weaponBonus = 0;
    if (m_world->HasComponent<EquipmentComponent>(m_playerID)) {
        auto& eq = m_world->GetComponent<EquipmentComponent>(m_playerID);
        weaponBonus = eq.bonusStrength;
    }

    int damage = CalculateDamage(m_playerID, target,
                                 weaponBonus,
                                 ElementType::NONE,
                                 DamageType::PHYSICAL);

    // ── Apply link strikes from party members ──────────────────────────────
    int linkDamage = ProcessLinkStrikes(target);
    damage += linkDamage;

    // ── Apply damage to target HP ──────────────────────────────────────────
    targetHp.hp = std::max(0, targetHp.hp - damage);

    // ── Publish event ──────────────────────────────────────────────────────
    CombatEvent ev;
    ev.type     = CombatEvent::Type::DAMAGE_DEALT;
    ev.sourceID = m_playerID;
    ev.targetID = target;
    ev.amount   = -damage;
    ev.dmgType  = DamageType::PHYSICAL;
    ev.element  = ElementType::NONE;
    EventBus<CombatEvent>::Instance().Publish(ev);

    // Show floating damage number in UI.
    UIEvent uiEv;
    uiEv.type   = UIEvent::Type::SHOW_DAMAGE_NUMBER;
    uiEv.entityID = target;
    uiEv.value  = damage;
    uiEv.colour = Color::White();
    EventBus<UIEvent>::Instance().Publish(uiEv);

    LOG_DEBUG("Player attacked enemy " << target << " for " << damage << " damage.");

    CheckDeaths();
    AdvanceTurn();
    return damage;
}

// ---------------------------------------------------------------------------
// PlayerUseSkill
// ---------------------------------------------------------------------------

int CombatSystem::PlayerUseSkill(int skillIndex, EntityID target)
{
    if (!m_isActive || m_turn != 0) return 0;

    if (!m_world->HasComponent<SkillsComponent>(m_playerID)) {
        LOG_WARN("Player has no SkillsComponent.");
        return 0;
    }

    auto& skills = m_world->GetComponent<SkillsComponent>(m_playerID);

    if (skillIndex < 0 || skillIndex >= 4) {
        LOG_WARN("PlayerUseSkill: skillIndex out of range [0-3].");
        return 0;
    }

    uint32_t skillID = skills.equippedSkills[skillIndex];
    if (skillID == 0) {
        LOG_WARN("No skill equipped in slot " << skillIndex);
        return 0;
    }

    // Find the skill entry to check cooldown.
    for (uint32_t i = 0; i < skills.skillCount; ++i) {
        auto& entry = skills.skills[i];
        if (entry.skillID == skillID && entry.isUnlocked) {
            if (entry.cooldown > 0.0f) {
                LOG_INFO("Skill " << skillID << " is on cooldown.");
                return 0;
            }

            // TEACHING NOTE — Skill Damage Scaling
            // Each skill multiplies base physical attack by a skill-specific factor.
            // For simplicity, we hardcode 1.5× for all skills here.  In a full
            // engine, the factor would be stored in a SkillData database entry.
            int weaponBonus = 0;
            if (m_world->HasComponent<EquipmentComponent>(m_playerID)) {
                weaponBonus = m_world->GetComponent<EquipmentComponent>(m_playerID).bonusStrength;
            }

            int baseDmg = weaponBonus;
            int damage  = static_cast<int>(
                CalculateDamage(m_playerID, target, baseDmg,
                                ElementType::NONE, DamageType::PHYSICAL) * 1.5f);

            if (m_world->HasComponent<HealthComponent>(target)) {
                m_world->GetComponent<HealthComponent>(target).hp =
                    std::max(0, m_world->GetComponent<HealthComponent>(target).hp - damage);
            }

            // Put skill on cooldown.
            entry.cooldown = entry.maxCooldown;

            CombatEvent ev;
            ev.type     = CombatEvent::Type::SKILL_USED;
            ev.sourceID = m_playerID;
            ev.targetID = target;
            ev.amount   = -damage;
            EventBus<CombatEvent>::Instance().Publish(ev);

            LOG_DEBUG("Player used skill " << skillID << " for " << damage << " damage.");
            CheckDeaths();
            AdvanceTurn();
            return damage;
        }
    }

    LOG_WARN("Skill ID " << skillID << " not found in SkillsComponent.");
    return 0;
}

// ---------------------------------------------------------------------------
// PlayerCastSpell
// ---------------------------------------------------------------------------

int CombatSystem::PlayerCastSpell(uint32_t spellID, EntityID target)
{
    if (!m_isActive || m_turn != 0) return 0;

    const SpellData* spell = GameDatabase::FindSpell(spellID);
    if (!spell) {
        LOG_WARN("PlayerCastSpell: spellID " << spellID << " not found in database.");
        return 0;
    }

    // Check MP.
    if (!m_world->HasComponent<HealthComponent>(m_playerID)) return 0;
    auto& hp = m_world->GetComponent<HealthComponent>(m_playerID);

    if (hp.mp < spell->mpCost) {
        LOG_INFO("Not enough MP to cast " << spell->name);
        UIEvent uiEv;
        uiEv.type = UIEvent::Type::SHOW_NOTIFICATION;
        uiEv.text = "Not enough MP!";
        uiEv.colour = Color::Red();
        EventBus<UIEvent>::Instance().Publish(uiEv);
        return 0;
    }

    // Deduct MP.
    hp.mp -= spell->mpCost;

    // ── Compute damage ─────────────────────────────────────────────────────
    // TEACHING NOTE — Magic Damage Formula
    // magic_damage = spellData.baseDamage × (caster.magic / 10.0)
    // The caster's magic stat scales spell power.  Division by 10 normalises
    // the multiplier so a magic stat of 50 → 5× base, 100 → 10× base.
    int magicStat = 10;
    if (m_world->HasComponent<StatsComponent>(m_playerID)) {
        magicStat = m_world->GetComponent<StatsComponent>(m_playerID).magic;
    }

    float elementMult  = GetElementMultiplier(target, spell->element);
    int   rawDamage    = static_cast<int>(spell->baseDamage * (magicStat / 10.0f));
    int   finalDamage  = static_cast<int>(rawDamage * elementMult);
    finalDamage        = std::max(1, finalDamage);

    if (m_world->HasComponent<HealthComponent>(target)) {
        m_world->GetComponent<HealthComponent>(target).hp =
            std::max(0, m_world->GetComponent<HealthComponent>(target).hp - finalDamage);
    }

    CombatEvent ev;
    ev.type     = CombatEvent::Type::DAMAGE_DEALT;
    ev.sourceID = m_playerID;
    ev.targetID = target;
    ev.amount   = -finalDamage;
    ev.dmgType  = DamageType::MAGICAL;
    ev.element  = spell->element;
    EventBus<CombatEvent>::Instance().Publish(ev);

    UIEvent uiEv;
    uiEv.type     = UIEvent::Type::UPDATE_MP_BAR;
    uiEv.entityID = m_playerID;
    uiEv.value    = hp.mp;
    EventBus<UIEvent>::Instance().Publish(uiEv);

    LOG_INFO("Player cast " << spell->name << " on " << target
             << " for " << finalDamage << " magic damage.");

    CheckDeaths();
    AdvanceTurn();
    return finalDamage;
}

// ---------------------------------------------------------------------------
// PlayerUseItem
// ---------------------------------------------------------------------------

void CombatSystem::PlayerUseItem(uint32_t itemID, EntityID target)
{
    if (!m_isActive || m_turn != 0) return;

    const ItemData* item = GameDatabase::FindItem(itemID);
    if (!item) {
        LOG_WARN("PlayerUseItem: itemID " << itemID << " not found.");
        return;
    }

    if (!m_world->HasComponent<InventoryComponent>(m_playerID)) return;
    auto& inv = m_world->GetComponent<InventoryComponent>(m_playerID);

    if (!inv.HasItem(itemID, 1)) {
        LOG_INFO("Player does not have item " << item->name);
        return;
    }

    // Remove one from inventory.
    uint32_t slotIdx = inv.FindItem(itemID);
    if (slotIdx < MAX_INV_SLOTS) {
        inv.slots[slotIdx].quantity -= 1;
        if (inv.slots[slotIdx].quantity == 0) {
            inv.slots[slotIdx] = ItemStack{};  // Clear slot.
        }
    }

    // Apply effect to target.
    if (m_world->HasComponent<HealthComponent>(target)) {
        auto& targetHp = m_world->GetComponent<HealthComponent>(target);

        if (item->hpRestore > 0) {
            targetHp.hp = std::min(targetHp.maxHp, targetHp.hp + item->hpRestore);
            LOG_INFO("Used " << item->name << ": restored " << item->hpRestore << " HP.");
        }
        if (item->mpRestore > 0) {
            targetHp.mp = std::min(targetHp.maxMp, targetHp.mp + item->mpRestore);
            LOG_INFO("Used " << item->name << ": restored " << item->mpRestore << " MP.");
        }
    }

    UIEvent uiEv;
    uiEv.type = UIEvent::Type::SHOW_NOTIFICATION;
    uiEv.text = "Used " + item->name;
    EventBus<UIEvent>::Instance().Publish(uiEv);

    AdvanceTurn();
}

// ---------------------------------------------------------------------------
// WarpStrike
// ---------------------------------------------------------------------------

int CombatSystem::WarpStrike(EntityID target)
{
    if (!m_isActive || m_turn != 0) return 0;
    if (!m_world->HasComponent<HealthComponent>(target)) return 0;
    if (m_world->GetComponent<HealthComponent>(target).isDead) return 0;

    // ── Teleport player adjacent to target ───────────────────────────────
    // TEACHING NOTE — Warp-Strike Teleportation
    // We copy the target's TransformComponent position to the player,
    // offset by one tile.  No projectile arc is simulated in the
    // tile-based engine — the effect is instantaneous from the player's
    // perspective, matching FF15's near-instant warp animations.
    if (m_world->HasComponent<TransformComponent>(target) &&
        m_world->HasComponent<TransformComponent>(m_playerID))
    {
        auto& targetTf = m_world->GetComponent<TransformComponent>(target);
        auto& playerTf = m_world->GetComponent<TransformComponent>(m_playerID);
        // Place player one tile above the target (in tile space).
        playerTf.position.x = targetTf.position.x;
        playerTf.position.z = targetTf.position.z - TILE_SIZE;
    }

    // ── Deal 1.5× normal damage ──────────────────────────────────────────
    int weaponBonus = 0;
    if (m_world->HasComponent<EquipmentComponent>(m_playerID)) {
        weaponBonus = m_world->GetComponent<EquipmentComponent>(m_playerID).bonusStrength;
    }

    int baseDmg = static_cast<int>(
        CalculateDamage(m_playerID, target, weaponBonus,
                        ElementType::NONE, DamageType::PHYSICAL) * 1.5f);

    m_world->GetComponent<HealthComponent>(target).hp =
        std::max(0, m_world->GetComponent<HealthComponent>(target).hp - baseDmg);

    // Publish warp-strike event.
    CombatEvent ev;
    ev.type     = CombatEvent::Type::WARP_STRIKE;
    ev.sourceID = m_playerID;
    ev.targetID = target;
    ev.amount   = -baseDmg;
    EventBus<CombatEvent>::Instance().Publish(ev);

    LOG_INFO("Warp-Strike on " << target << " for " << baseDmg << " damage!");

    CheckDeaths();
    AdvanceTurn();
    return baseDmg;
}

// ---------------------------------------------------------------------------
// PlayerFlee
// ---------------------------------------------------------------------------

bool CombatSystem::PlayerFlee()
{
    if (!m_isActive) return false;

    // ── Calculate flee chance ─────────────────────────────────────────────
    // Base 30% + 1% per point of speed advantage over average enemy speed.
    int playerSpeed = 10;
    if (m_world->HasComponent<StatsComponent>(m_playerID)) {
        playerSpeed = m_world->GetComponent<StatsComponent>(m_playerID).speed;
    }

    float avgEnemySpeed = 10.0f;
    if (!m_aliveEnemies.empty()) {
        int total = 0;
        for (EntityID e : m_aliveEnemies) {
            if (m_world->HasComponent<StatsComponent>(e)) {
                total += m_world->GetComponent<StatsComponent>(e).speed;
            } else {
                total += 10;
            }
        }
        avgEnemySpeed = static_cast<float>(total) / m_aliveEnemies.size();
    }

    float speedDiff  = static_cast<float>(playerSpeed) - avgEnemySpeed;
    float fleechance = std::min(0.90f, 0.30f + speedDiff * 0.01f);

    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    bool success = dist(m_rng) < fleechance;

    if (success) {
        LOG_INFO("Player successfully fled the battle!");
        m_result.playerWon = false;
        m_isActive = false;

        UIEvent uiEv;
        uiEv.type = UIEvent::Type::SHOW_NOTIFICATION;
        uiEv.text = "Got away safely!";
        uiEv.colour = Color::Yellow();
        EventBus<UIEvent>::Instance().Publish(uiEv);
    } else {
        LOG_INFO("Player failed to flee.");
        UIEvent uiEv;
        uiEv.type = UIEvent::Type::SHOW_NOTIFICATION;
        uiEv.text = "Couldn't escape!";
        uiEv.colour = Color::Red();
        EventBus<UIEvent>::Instance().Publish(uiEv);
        AdvanceTurn(); // Fleeing costs the player's turn.
    }

    return success;
}

// ===========================================================================
// Enemy turn
// ===========================================================================

void CombatSystem::EnemyTurn(EntityID enemy)
{
    if (!m_world->HasComponent<HealthComponent>(enemy)) return;
    auto& enemyHp = m_world->GetComponent<HealthComponent>(enemy);
    if (enemyHp.isDead) return;

    // ── Check STOP status — frozen enemies skip their turn ─────────────────
    if (m_world->HasComponent<StatusEffectsComponent>(enemy)) {
        auto& se = m_world->GetComponent<StatusEffectsComponent>(enemy);
        if (se.Has(StatusEffect::STOP) || se.Has(StatusEffect::SLEEP)) {
            LOG_DEBUG("Enemy " << enemy << " is stopped/asleep — skipping turn.");
            return;
        }
    }

    // ── Simple enemy AI: attack the player ────────────────────────────────
    // TEACHING NOTE — Enemy Ability Selection
    // A real AI would pick from a list of abilities based on HP, MP, and
    // cooldowns.  For this base implementation, all enemies use a basic
    // physical attack.  Override this with the SelectAbility() logic from
    // AISystem for more interesting fights.
    int enemyAttack = 10;
    if (m_world->HasComponent<StatsComponent>(enemy)) {
        enemyAttack = m_world->GetComponent<StatsComponent>(enemy).strength;
    }

    int damage = CalculateDamage(enemy, m_playerID,
                                 enemyAttack,
                                 ElementType::NONE,
                                 DamageType::PHYSICAL);

    if (m_world->HasComponent<HealthComponent>(m_playerID)) {
        m_world->GetComponent<HealthComponent>(m_playerID).hp =
            std::max(0, m_world->GetComponent<HealthComponent>(m_playerID).hp - damage);
    }

    CombatEvent ev;
    ev.type     = CombatEvent::Type::DAMAGE_DEALT;
    ev.sourceID = enemy;
    ev.targetID = m_playerID;
    ev.amount   = -damage;
    ev.dmgType  = DamageType::PHYSICAL;
    EventBus<CombatEvent>::Instance().Publish(ev);

    // Update player HP bar in UI.
    UIEvent uiEv;
    uiEv.type     = UIEvent::Type::UPDATE_HP_BAR;
    uiEv.entityID = m_playerID;
    if (m_world->HasComponent<HealthComponent>(m_playerID)) {
        uiEv.value = m_world->GetComponent<HealthComponent>(m_playerID).hp;
    }
    EventBus<UIEvent>::Instance().Publish(uiEv);

    LOG_DEBUG("Enemy " << enemy << " attacked player for " << damage << " damage.");

    CheckDeaths();
}

// ===========================================================================
// Shared calculations
// ===========================================================================

int CombatSystem::CalculateDamage(EntityID attacker, EntityID defender,
                                   int baseDmg,
                                   ElementType element, DamageType dmgType)
{
    // ── Read attacker stats ───────────────────────────────────────────────
    int atkStrength = 10;
    if (m_world->HasComponent<StatsComponent>(attacker)) {
        atkStrength = m_world->GetComponent<StatsComponent>(attacker).strength;
    }

    // ── Read defender stats ───────────────────────────────────────────────
    int defDefence = 5;
    if (m_world->HasComponent<StatsComponent>(defender)) {
        if (dmgType == DamageType::MAGICAL) {
            defDefence = m_world->GetComponent<StatsComponent>(defender).spirit;
        } else {
            defDefence = m_world->GetComponent<StatsComponent>(defender).defence;
        }
    }

    // TRUE_DAMAGE bypasses all defence.
    if (dmgType == DamageType::TRUE_DAMAGE) {
        defDefence = 0;
    }

    // ── Core formula ──────────────────────────────────────────────────────
    // TEACHING NOTE — Damage Formula Breakdown
    //
    //   raw = max(1, (strength × 2 + baseDmg) - defence)
    //
    // Why multiply strength by 2?  This amplifies the STAT DIFFERENCE between
    // characters.  A hero with 30 strength vs a goblin with 5 defence deals:
    //   raw = max(1, 60 + baseDmg - 5) = 55 + baseDmg
    //
    // If we didn't multiply by 2, higher-level characters would barely outdamage
    // lower-level ones.  The ×2 creates the visible "power scaling" feel.
    int raw = std::max(1, (atkStrength * 2 + baseDmg) - defDefence);

    // ── Elemental multiplier ──────────────────────────────────────────────
    float elementMult = GetElementMultiplier(defender, element);

    // ── Critical hit ─────────────────────────────────────────────────────
    float critMult = RollCrit(attacker) ? 1.5f : 1.0f;

    // ── Dodge check ───────────────────────────────────────────────────────
    if (RollDodge(attacker, defender)) {
        LOG_DEBUG("Attack dodged by entity " << defender);
        return 0;
    }

    // ── Variance (±15%) ───────────────────────────────────────────────────
    // TEACHING NOTE — Damage Variance
    // A small random range (85–115% of base) prevents combat from feeling
    // mechanical.  The player doesn't know the exact formula, so variance
    // makes outcomes feel "organic".  Too much variance (50–200%) would
    // make planning feel pointless; too little (99–101%) makes it
    // predictable and boring.
    std::uniform_real_distribution<float> varianceDist(0.85f, 1.15f);
    float variance = varianceDist(m_rng);

    int finalDmg = static_cast<int>(raw * elementMult * critMult * variance);
    return std::max(1, finalDmg);
}

// ---------------------------------------------------------------------------
// ApplyStatusEffect
// ---------------------------------------------------------------------------

void CombatSystem::ApplyStatusEffect(EntityID target, StatusEffect effect,
                                      float duration)
{
    if (!m_world->HasComponent<StatusEffectsComponent>(target)) {
        m_world->AddComponent<StatusEffectsComponent>(target);
    }

    auto& se = m_world->GetComponent<StatusEffectsComponent>(target);

    // Determine damage per tick for DoT effects.
    float tickDmg = 0.0f;
    if (effect == StatusEffect::POISON || effect == StatusEffect::BURNED) {
        // TEACHING NOTE — DoT Scaling
        // Poison deals 5% of the target's max HP per tick.
        // This scales with target's health, making poison relevant at all
        // levels without needing to rebalance poison damage manually.
        if (m_world->HasComponent<HealthComponent>(target)) {
            tickDmg = m_world->GetComponent<HealthComponent>(target).maxHp * 0.05f;
        }
    }

    se.Apply(effect, duration, tickDmg, m_playerID);

    CombatEvent ev;
    ev.type     = CombatEvent::Type::STATUS_APPLIED;
    ev.targetID = target;
    ev.effect   = effect;
    EventBus<CombatEvent>::Instance().Publish(ev);

    LOG_DEBUG("Applied status " << static_cast<int>(effect)
              << " to entity " << target << " for " << duration << "s");
}

// ---------------------------------------------------------------------------
// CheckDeaths
// ---------------------------------------------------------------------------

void CombatSystem::CheckDeaths()
{
    // ── Check player death ────────────────────────────────────────────────
    if (m_world->HasComponent<HealthComponent>(m_playerID)) {
        auto& playerHp = m_world->GetComponent<HealthComponent>(m_playerID);
        if (playerHp.hp <= 0 && !playerHp.isDead) {
            playerHp.isDead = true;

            CombatEvent ev;
            ev.type     = CombatEvent::Type::PARTY_KO;
            ev.targetID = m_playerID;
            EventBus<CombatEvent>::Instance().Publish(ev);

            LOG_INFO("Player died! Combat over — game over.");
            m_result.playerWon = false;
            m_isActive = false;
            return;
        }
    }

    // ── Check enemy deaths ────────────────────────────────────────────────
    auto it = m_aliveEnemies.begin();
    while (it != m_aliveEnemies.end()) {
        EntityID e = *it;
        if (m_world->HasComponent<HealthComponent>(e)) {
            auto& hp = m_world->GetComponent<HealthComponent>(e);
            if (hp.hp <= 0 && !hp.isDead) {
                hp.isDead = true;

                // Grant loot from this enemy.
                AwardLoot(e);

                CombatEvent ev;
                ev.type     = CombatEvent::Type::ENTITY_DIED;
                ev.targetID = e;
                EventBus<CombatEvent>::Instance().Publish(ev);

                LOG_INFO("Enemy " << e << " defeated.");
                it = m_aliveEnemies.erase(it);
                continue;
            }
        }
        ++it;
    }

    // ── Check win condition ───────────────────────────────────────────────
    if (m_aliveEnemies.empty() && m_isActive) {
        LOG_INFO("All enemies defeated! Player wins.");
        m_result.playerWon = true;
        m_isActive = false;

        UIEvent uiEv;
        uiEv.type = UIEvent::Type::SHOW_NOTIFICATION;
        uiEv.text = "Victory! XP: " + std::to_string(m_result.xpGained)
                  + "  Gil: " + std::to_string(m_result.gilGained);
        uiEv.colour = Color::Yellow();
        EventBus<UIEvent>::Instance().Publish(uiEv);
    }
}

// ---------------------------------------------------------------------------
// GetCombatResult / GetActiveEnemies
// ---------------------------------------------------------------------------

std::vector<EntityID> CombatSystem::GetActiveEnemies() const
{
    return m_aliveEnemies;
}

// ===========================================================================
// Private helpers
// ===========================================================================

float CombatSystem::GetElementMultiplier(EntityID defender,
                                          ElementType element) const
{
    // If no element, no multiplier.
    if (element == ElementType::NONE) return 1.0f;

    // Try to find EnemyData for this entity.
    // TEACHING NOTE — Looking Up Enemy Data
    // We use NameComponent to find the enemy's data ID.  In a production engine
    // you'd store the EnemyData::id in a dedicated EnemyTypeComponent.
    // For simplicity, we use CombatComponent::xpReward as a proxy species ID.
    if (!m_world->HasComponent<CombatComponent>(defender)) return 1.0f;

    const auto& combat = m_world->GetComponent<CombatComponent>(defender);
    // xpReward used as a species identifier — look up enemy database entry.
    const EnemyData* enemyData = GameDatabase::FindEnemy(combat.xpReward);
    if (!enemyData) return 1.0f;

    if (enemyData->weakness   == element) return 1.5f;
    if (enemyData->resistance == element) return 0.5f;
    return 1.0f;
}

bool CombatSystem::RollCrit(EntityID attacker) const
{
    int luck = 10;
    if (m_world->HasComponent<StatsComponent>(attacker)) {
        luck = m_world->GetComponent<StatsComponent>(attacker).luck;
    }
    // Crit chance capped at 50%.
    float critChance = std::min(0.50f, luck / 100.0f);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(m_rng) < critChance;
}

bool CombatSystem::RollDodge(EntityID attacker, EntityID defender) const
{
    int atkSpeed = 10;
    int defSpeed = 10;
    if (m_world->HasComponent<StatsComponent>(attacker)) {
        atkSpeed = m_world->GetComponent<StatsComponent>(attacker).speed;
    }
    if (m_world->HasComponent<StatsComponent>(defender)) {
        defSpeed = m_world->GetComponent<StatsComponent>(defender).speed;
    }
    // Dodge chance: positive only when defender is faster.
    float dodgeChance = std::max(0.0f, (defSpeed - atkSpeed) / 200.0f);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(m_rng) < dodgeChance;
}

int CombatSystem::ProcessLinkStrikes(EntityID target)
{
    // TEACHING NOTE — Party Link Strikes
    // Each party member has a 30% chance to automatically assist the player.
    // Their contribution is 30% of their own physical attack damage.
    // This makes having a full party stronger, rewarding party management.

    if (!m_world->HasComponent<PartyComponent>(m_playerID)) return 0;

    auto& party = m_world->GetComponent<PartyComponent>(m_playerID);
    int totalLinkDamage = 0;

    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (uint32_t i = 0; i < party.memberCount; ++i) {
        EntityID member = party.members[i];
        if (member == m_playerID) continue;
        if (!m_world->HasComponent<HealthComponent>(member)) continue;
        if (m_world->GetComponent<HealthComponent>(member).isDead) continue;

        // 30% link strike chance.
        if (dist(m_rng) >= 0.30f) continue;

        int memberStrength = 10;
        if (m_world->HasComponent<StatsComponent>(member)) {
            memberStrength = m_world->GetComponent<StatsComponent>(member).strength;
        }

        // Link strike deals 30% of member's normal attack.
        int linkDmg = std::max(1, static_cast<int>(memberStrength * 0.30f));
        totalLinkDamage += linkDmg;

        LOG_DEBUG("Link strike from party member " << member << " for " << linkDmg);
    }

    return totalLinkDamage;
}

void CombatSystem::TickStatusEffects(EntityID entity, float dt)
{
    auto& se = m_world->GetComponent<StatusEffectsComponent>(entity);

    // TEACHING NOTE — Iterating and Removing from Arrays
    // We iterate backward so that erasing an entry doesn't skip the next one.
    for (uint32_t i = 0; i < se.count; ) {
        auto& entry = se.active[i];
        entry.remaining -= dt;

        // DoT tick for POISON and BURNED.
        if (entry.effect == StatusEffect::POISON ||
            entry.effect == StatusEffect::BURNED)
        {
            entry.tickTimer -= dt;
            if (entry.tickTimer <= 0.0f) {
                entry.tickTimer = 1.0f; // tick every second

                if (m_world->HasComponent<HealthComponent>(entity)) {
                    auto& hp = m_world->GetComponent<HealthComponent>(entity);
                    int dmg = static_cast<int>(entry.tickDamage);
                    hp.hp = std::max(0, hp.hp - dmg);
                    LOG_DEBUG("DoT tick on entity " << entity << ": " << dmg << " damage.");
                }
            }
        }

        // Remove expired effects.
        if (entry.remaining <= 0.0f) {
            CombatEvent ev;
            ev.type     = CombatEvent::Type::STATUS_CLEARED;
            ev.targetID = entity;
            ev.effect   = entry.effect;
            EventBus<CombatEvent>::Instance().Publish(ev);

            se.Remove(entry.effect);
            // Don't increment i — the element was swapped to position i.
        } else {
            ++i;
        }
    }
}

void CombatSystem::AwardLoot(EntityID enemy)
{
    // Read CombatComponent for XP/Gil rewards.
    if (!m_world->HasComponent<CombatComponent>(enemy)) return;
    auto& combat = m_world->GetComponent<CombatComponent>(enemy);

    // ── XP bonus for fighting above player level ──────────────────────────
    // TEACHING NOTE — Level-Difference XP Bonus
    // If the enemy is higher level than the player, the player earns a
    // 10% XP bonus per level difference.  This rewards the risk of
    // fighting tougher enemies and encourages exploration into danger zones.
    int playerLevel = 1;
    if (m_world->HasComponent<LevelComponent>(m_playerID)) {
        playerLevel = m_world->GetComponent<LevelComponent>(m_playerID).level;
    }
    int enemyLevel = 1;
    if (m_world->HasComponent<LevelComponent>(enemy)) {
        enemyLevel = m_world->GetComponent<LevelComponent>(enemy).level;
    }
    int levelDiff = std::max(0, enemyLevel - playerLevel);
    float xpMult  = 1.0f + levelDiff * 0.1f;
    int xpGained  = static_cast<int>(combat.xpReward * xpMult);

    m_result.xpGained  += xpGained;
    m_result.gilGained += static_cast<int>(combat.gilReward);

    // Add XP as pending (applied at camp, FF15 style).
    if (m_world->HasComponent<LevelComponent>(m_playerID)) {
        m_world->GetComponent<LevelComponent>(m_playerID).GainXP(xpGained);
    }

    // Add Gil immediately.
    if (m_world->HasComponent<CurrencyComponent>(m_playerID)) {
        m_world->GetComponent<CurrencyComponent>(m_playerID).EarnGil(combat.gilReward);
    }

    // ── Item drops ────────────────────────────────────────────────────────
    // TEACHING NOTE — Random Item Drops
    // We look up the enemy's species data to find the drop table.
    // Each item in dropItemIDs has a 30% chance to drop (simple model).
    // A production engine would use per-item drop rates and rarity tiers.
    const EnemyData* enemyData = GameDatabase::FindEnemy(combat.xpReward);
    if (enemyData) {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        for (uint32_t dropID : enemyData->dropItemIDs) {
            if (dist(m_rng) < 0.30f) {
                m_result.itemsDropped.push_back(dropID);

                // Add to player inventory if possible.
                if (m_world->HasComponent<InventoryComponent>(m_playerID)) {
                    auto& inv = m_world->GetComponent<InventoryComponent>(m_playerID);
                    uint32_t slot = inv.FindItem(dropID);
                    if (slot < MAX_INV_SLOTS) {
                        inv.slots[slot].quantity++;
                    } else {
                        uint32_t emptySlot = inv.FindEmptySlot();
                        if (emptySlot < MAX_INV_SLOTS) {
                            inv.slots[emptySlot].itemID   = dropID;
                            inv.slots[emptySlot].quantity = 1;
                        }
                    }

                    const ItemData* dropItem = GameDatabase::FindItem(dropID);
                    if (dropItem) {
                        UIEvent uiEv;
                        uiEv.type = UIEvent::Type::SHOW_NOTIFICATION;
                        uiEv.text = "Obtained: " + dropItem->name;
                        EventBus<UIEvent>::Instance().Publish(uiEv);
                    }
                }
            }
        }
    }
}

void CombatSystem::AdvanceTurn()
{
    // TEACHING NOTE — Turn Cycling
    // Turn order cycles: 0 (player) → 1 → 2 → ... → aliveEnemies.size() → 0
    // We skip turns for dead entities automatically.
    if (m_aliveEnemies.empty()) {
        m_turn = 0;
        return;
    }

    m_turn = (m_turn + 1) % (static_cast<int>(m_aliveEnemies.size()) + 1);

    // Skip dead enemies.
    int maxIterations = static_cast<int>(m_aliveEnemies.size()) + 1;
    while (maxIterations-- > 0) {
        if (m_turn == 0) break; // Player turn: always valid.

        int enemyIdx = m_turn - 1;
        if (enemyIdx < static_cast<int>(m_aliveEnemies.size())) {
            EntityID e = m_aliveEnemies[enemyIdx];
            if (m_world->HasComponent<HealthComponent>(e) &&
                !m_world->GetComponent<HealthComponent>(e).isDead)
            {
                break; // This enemy is alive; valid turn.
            }
        }
        m_turn = (m_turn + 1) % (static_cast<int>(m_aliveEnemies.size()) + 1);
    }
}
