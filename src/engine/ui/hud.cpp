/**
 * @file hud.cpp
 * @brief HUD state extraction and debug output implementation.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2025
 */

#include "engine/ui/hud.hpp"
#include "engine/core/Logger.hpp"
#include "game/GameData.hpp"       // GameDatabase::FindQuest — for quest titles
#include <algorithm> // std::max, std::min
#include <iostream>
#include <iomanip>
#include <sstream>

// ===========================================================================
// Hud::ReadFromWorld
// ===========================================================================

HudState Hud::ReadFromWorld(World& world, EntityID playerID,
                             int frameCount, float gameTime) const
{
    HudState state;
    state.frameCount   = frameCount;
    state.gameTimeSecs = gameTime;

    if (playerID == NULL_ENTITY || !world.IsAlive(playerID))
        return state;

    // ---- Health / MP --------------------------------------------------------
    if (world.HasComponent<HealthComponent>(playerID))
    {
        const auto& hp     = world.GetComponent<HealthComponent>(playerID);
        state.playerHp     = hp.hp;
        state.playerMaxHp  = hp.maxHp;
        state.playerMp     = hp.mp;
        state.playerMaxMp  = hp.maxMp;
    }

    // ---- ATB gauge ----------------------------------------------------------
    // TEACHING NOTE — ATB gauge via attack cooldown
    // The CombatComponent stores attackCooldown (time until next action).
    // ATB fullness = 1 - (cooldown / baseCooldown).  When cooldown = 0,
    // atbGauge = 1.0 (action ready).
    if (world.HasComponent<CombatComponent>(playerID))
    {
        const auto& cb   = world.GetComponent<CombatComponent>(playerID);
        state.isInCombat = cb.isInCombat;

        const float baseCooldown = (cb.attackRate > 0.0f) ? 1.0f / cb.attackRate : 1.0f;
        const float remaining    = std::max(0.0f, cb.attackCooldown);
        state.atbGauge = 1.0f - std::min(1.0f, remaining / baseCooldown);
        state.atbReady = (remaining <= 0.0f);
    }

    // ---- Equipped spell -----------------------------------------------------
    if (world.HasComponent<MagicComponent>(playerID))
    {
        const auto& mg = world.GetComponent<MagicComponent>(playerID);
        // TEACHING NOTE — equippedSpell is stored as a spell ID string.
        // The HUD shows a human-readable name; for now it shows the raw ID.
        state.equippedSpell = mg.equippedSpell.empty() ? "(none)" : mg.equippedSpell;
    }

    // ---- Active enemies in world --------------------------------------------
    // Count all living entities with an AIComponent that are not dead.
    world.View<AIComponent, HealthComponent>(
        [&](EntityID eid, AIComponent& ai, HealthComponent& hp)
        {
            (void)eid;
            if (ai.currentState != AIComponent::State::DEAD && !hp.isDead)
                ++state.activeEnemies;
        });

    // ---- Party portraits ----------------------------------------------------
    if (world.HasComponent<PartyComponent>(playerID))
    {
        const auto& party = world.GetComponent<PartyComponent>(playerID);
        for (uint32_t i = 0; i < MAX_PARTY_SIZE; ++i)
        {
            const EntityID memberId = party.members[i];
            if (memberId == NULL_ENTITY || !world.IsAlive(memberId)) continue;

            PartyMemberHudEntry entry;
            if (world.HasComponent<NameComponent>(memberId))
                entry.name = world.GetComponent<NameComponent>(memberId).name;
            if (world.HasComponent<HealthComponent>(memberId))
            {
                const auto& memberHp = world.GetComponent<HealthComponent>(memberId);
                entry.hp       = memberHp.hp;
                entry.maxHp    = memberHp.maxHp;
                entry.mp       = memberHp.mp;
                entry.maxMp    = memberHp.maxMp;
                entry.isDowned = memberHp.isDowned;
            }
            state.partyMembers.push_back(entry);
        }
    }

    // ---- Active quest -------------------------------------------------------
    if (world.HasComponent<QuestComponent>(playerID))
    {
        const auto& qc = world.GetComponent<QuestComponent>(playerID);
        for (uint32_t q = 0; q < qc.activeCount; ++q)
        {
            const auto& qe = qc.quests[q];
            if (qe.isComplete || qe.isFailed) continue;

            // Look up human-readable title from the static GameDatabase.
            const auto* qdata = GameDatabase::FindQuest(qe.questID);
            if (qdata)
            {
                state.activeQuestName = qdata->title;
                // Objective description from QuestData.
                if (qe.objective < static_cast<uint32_t>(qdata->objectives.size()))
                    state.activeObjectiveText = qdata->objectives[qe.objective].description;
            }
            else
            {
                state.activeQuestName = "Quest #" + std::to_string(qe.questID);
                state.activeObjectiveText = "Objective " + std::to_string(qe.objective)
                    + " (" + std::to_string(qe.progress) + "/"
                    + std::to_string(qe.required) + ")";
            }
            break;  // Only show one quest in the HUD tracker.
        }
    }

    return state;
}

// ===========================================================================
// Hud::PrintDebug
// ===========================================================================

void Hud::PrintDebug(const HudState& state, int every) const
{
    if (state.frameCount % every != 0)
        return;

    // TEACHING NOTE — std::fixed / std::setprecision for readable floats
    // Without these manipulators std::cout would print long decimal strings
    // like "0.71999998" instead of "0.72".
    std::ostringstream oss;
    oss << "[HUD] Frame=" << state.frameCount
        << " HP="  << state.playerHp  << "/" << state.playerMaxHp
        << " MP="  << state.playerMp  << "/" << state.playerMaxMp
        << " ATB=" << std::fixed << std::setprecision(2) << state.atbGauge
        << " Combat=" << (state.isInCombat ? "yes" : "no")
        << " Enemies=" << state.activeEnemies;

    if (!state.equippedSpell.empty())
        oss << " Spell=" << state.equippedSpell;

    if (!state.activeQuestName.empty())
        oss << " Quest=\"" << state.activeQuestName << "\"";

    if (!state.activeObjectiveText.empty())
        oss << " Obj=\"" << state.activeObjectiveText << "\"";

    std::cout << oss.str() << "\n";
}
