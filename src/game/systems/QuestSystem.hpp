/**
 * @file QuestSystem.hpp
 * @brief Quest tracking and progression system for the educational RPG engine.
 *
 * ============================================================================
 * TEACHING NOTE — Quest System Architecture
 * ============================================================================
 *
 * Quest systems are fundamentally a STATE MACHINE with DATABASE LOOKUP.
 *
 * ─── Quest State Machine ────────────────────────────────────────────────────
 *
 * Each quest in the QuestComponent cycles through these states:
 *
 *   NOT_ACCEPTED → [AcceptQuest] → IN_PROGRESS
 *   IN_PROGRESS  → [CompleteQuest] → COMPLETED
 *   IN_PROGRESS  → [FailQuest]     → FAILED
 *
 * "In progress" itself contains an OBJECTIVE sub-machine:
 *
 *   objective[0] IN_PROGRESS → objective[0] COMPLETE → objective[1] IN_PROGRESS → …
 *
 * ─── Data vs Logic ──────────────────────────────────────────────────────────
 *
 * QuestData (in GameData.hpp) is the STATIC definition of a quest:
 *   - What are the objectives?
 *   - What are the rewards?
 *   - What are the prerequisites?
 *
 * QuestEntry (in ECS.hpp / QuestComponent) is the RUNTIME state:
 *   - Which objective is the player on?
 *   - How much progress has been made?
 *   - Is the quest completed or failed?
 *
 * The QuestSystem bridges the two: it reads static data to validate actions,
 * and writes to the runtime QuestComponent to record progress.
 *
 * ─── Event-Driven Objective Tracking ────────────────────────────────────────
 *
 * Rather than polling ("does the player have 3 goblins killed this frame?"),
 * other systems call the QuestSystem's callback methods:
 *
 *   CombatSystem calls OnEnemyKilled() when an enemy dies.
 *   InventorySystem calls OnItemCollected() when items are picked up.
 *   Zone calls OnLocationReached() when the player enters a trigger zone.
 *
 * This is the Observer pattern in reverse: the QuestSystem RECEIVES events
 * rather than publishing them.  It then publishes QuestEvents itself to
 * notify the UI.
 *
 * ─── Prerequisite System ────────────────────────────────────────────────────
 *
 * QuestData::prereqQuestIDs lists quests that must be completed before this
 * one can be accepted.  CanAcceptQuest() iterates this list and checks the
 * player's QuestComponent for COMPLETED entries.  This enables branching
 * narrative: quest chains, side-quests gated by story progress, etc.
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
// QuestSystem class
// ===========================================================================

/**
 * @class QuestSystem
 * @brief Manages all quest lifecycle operations: acceptance, progress, and
 *        completion for any entity that has a QuestComponent.
 *
 * USAGE EXAMPLE:
 * @code
 *   QuestSystem quests(world,
 *                      &EventBus<QuestEvent>::Instance(),
 *                      &EventBus<UIEvent>::Instance());
 *
 *   // Accept quest 1 ("The Road to Dawn"):
 *   quests.AcceptQuest(playerID, 1);
 *
 *   // Called by CombatSystem when a goblin dies:
 *   quests.OnEnemyKilled(playerID, goblinDataID);
 *
 *   // Accept automatically completes objective[0] after 3 kills:
 *   // UI fires QUEST_COMPLETE event automatically.
 * @endcode
 */
class QuestSystem {
public:

    /**
     * @brief Construct the quest system with its required dependencies.
     *
     * @param world     Non-owning pointer to the ECS World.
     * @param questBus  Non-owning pointer to the quest event bus.
     * @param uiBus     Non-owning pointer to the UI event bus.
     */
    QuestSystem(World* world,
                EventBus<QuestEvent>* questBus,
                EventBus<UIEvent>*   uiBus);

    // -----------------------------------------------------------------------
    // Quest lifecycle
    // -----------------------------------------------------------------------

    /**
     * @brief Accept quest `questID` for `player`.
     *
     * Preconditions (checked internally):
     *  - CanAcceptQuest() returns true.
     *  - player has a QuestComponent with a free slot.
     *
     * On success:
     *  - Adds a QuestEntry to QuestComponent.
     *  - Fires QuestEvent::QUEST_ACCEPTED.
     *  - Fires UIEvent::SHOW_NOTIFICATION "Quest Accepted: …".
     *
     * @return true if the quest was successfully accepted.
     */
    bool AcceptQuest(EntityID player, uint32_t questID);

    /**
     * @brief Increment progress for objective `objectiveIdx` of quest `questID`.
     *
     * If progress reaches the objective's required count, marks the objective
     * complete and advances to the next objective (or completes the quest if
     * it was the last one).
     *
     * Fires QuestEvent::OBJECTIVE_UPDATED.
     *
     * @param player        Player entity.
     * @param questID       Which quest to update.
     * @param objectiveIdx  Index into QuestData::objectives.
     * @param progress      Amount to add to the counter (usually 1).
     *
     * TEACHING NOTE — Structured Progress Tracking
     * ─────────────────────────────────────────────
     * Rather than storing progress per-objective in the QuestComponent
     * (which would require a 2D array), we advance a single `objective`
     * counter in QuestEntry.  When objective == objectives.size(), the
     * quest is complete.  Individual objective progress is tracked in
     * QuestEntry::progress against QuestEntry::required.
     */
    void UpdateObjective(EntityID player, uint32_t questID,
                         uint32_t objectiveIdx, uint32_t progress);

    /**
     * @brief Mark quest `questID` as complete and award rewards.
     *
     * Grants xpReward via LevelComponent::GainXP, gilReward via
     * CurrencyComponent::EarnGil, and itemRewards via InventorySystem.
     *
     * Fires QuestEvent::QUEST_COMPLETED and QuestEvent::REWARD_GRANTED.
     *
     * @return true if the quest existed and was completed successfully.
     */
    bool CompleteQuest(EntityID player, uint32_t questID);

    /**
     * @brief Mark quest `questID` as failed.
     *
     * Sets QuestEntry::isFailed.  Fires QuestEvent::QUEST_FAILED.
     * Failed quests can be re-accepted if they are still available.
     *
     * @param player   Player entity.
     * @param questID  Quest to fail.
     */
    void FailQuest(EntityID player, uint32_t questID);

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------

    /**
     * @brief Return pointers to QuestData for all active (in-progress) quests.
     *
     * "Active" means the quest has been accepted and is not yet complete or failed.
     *
     * @return Vector of const pointers into the static GameDatabase — safe to
     *         read until program end (the database is never modified).
     */
    std::vector<const QuestData*> GetActiveQuests(EntityID player) const;

    /// @return true if the player has completed quest `questID`.
    bool IsQuestComplete(EntityID player, uint32_t questID) const;

    /// @return true if quest `questID` is currently in progress.
    bool IsQuestActive(EntityID player, uint32_t questID) const;

    /**
     * @brief Check whether the player meets the prerequisites to accept a quest.
     *
     * Iterates QuestData::prereqQuestIDs and confirms each is COMPLETED in
     * the player's QuestComponent.
     *
     * @return true if all prerequisites are satisfied.
     */
    bool CanAcceptQuest(EntityID player, uint32_t questID) const;

    // -----------------------------------------------------------------------
    // Event-driven objective hooks (called by other systems)
    // -----------------------------------------------------------------------

    /**
     * @brief Notify the quest system that the player killed an enemy.
     *
     * Scans all active quests for objectives of type KILL_ENEMY with
     * `targetID == enemyDataID` and calls UpdateObjective() for each.
     *
     * Called by CombatSystem after CheckDeaths() grants the kill.
     *
     * @param player      Player entity that made the kill.
     * @param enemyDataID The EnemyData::id of the defeated enemy type.
     */
    void OnEnemyKilled(EntityID player, uint32_t enemyDataID);

    /**
     * @brief Notify the quest system that the player collected items.
     *
     * Scans active quests for COLLECT_ITEM objectives matching `itemID`.
     * Called by InventorySystem::AddItem().
     *
     * @param player  Player entity.
     * @param itemID  ID of the item collected.
     * @param qty     Quantity collected in this batch.
     */
    void OnItemCollected(EntityID player, uint32_t itemID, uint32_t qty);

    /**
     * @brief Notify the quest system that the player entered a zone.
     *
     * Scans active quests for REACH_LOCATION objectives matching `zoneID`.
     * Called by the Zone class when the player enters its trigger area.
     *
     * @param player  Player entity.
     * @param zoneID  The ZoneData::id of the entered zone.
     */
    void OnLocationReached(EntityID player, uint32_t zoneID);

private:

    /**
     * @brief Helper to publish a QuestEvent with common fields filled in.
     */
    void PublishQuestEvent(QuestEvent::Type type, EntityID player,
                           uint32_t questID, uint32_t objectiveID = 0,
                           int32_t delta = 0) const;

    /**
     * @brief Grant item rewards from QuestData to the player.
     *
     * Calls InventorySystem::AddItem for each ID in questData.itemRewards.
     * Creates a temporary InventorySystem for reward distribution.
     */
    void GrantItemRewards(EntityID player, const QuestData& questData);

    World*                m_world     = nullptr;
    EventBus<QuestEvent>* m_questBus  = nullptr;
    EventBus<UIEvent>*    m_uiBus     = nullptr;
};
