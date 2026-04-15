/**
 * @file QuestSystem.cpp
 * @brief Implementation of the quest lifecycle system.
 *
 * TEACHING NOTE — Separation of Concerns
 * ────────────────────────────────────────
 * The .cpp file contains ONLY implementation — no new types or macros.
 * This keeps the public interface (.hpp) clean and easy to read.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 */

#include "QuestSystem.hpp"
#include "InventorySystem.hpp"   // for GrantItemRewards helper

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

QuestSystem::QuestSystem(World* world,
                         EventBus<QuestEvent>* questBus,
                         EventBus<UIEvent>*    uiBus)
    : m_world(world), m_questBus(questBus), m_uiBus(uiBus)
{
    LOG_INFO("QuestSystem initialised.");
}

// ---------------------------------------------------------------------------
// AcceptQuest
// ---------------------------------------------------------------------------

bool QuestSystem::AcceptQuest(EntityID player, uint32_t questID)
{
    // 1. Check the quest exists in the database.
    const QuestData* data = GameDatabase::FindQuest(questID);
    if (!data) {
        LOG_WARN("AcceptQuest: quest " + std::to_string(questID) + " not found.");
        return false;
    }

    // 2. Check prerequisites.
    if (!CanAcceptQuest(player, questID)) {
        LOG_INFO("AcceptQuest: prerequisites not met for quest " + data->title);
        return false;
    }

    // 3. Check the player already has or is doing this quest.
    if (IsQuestActive(player, questID) || IsQuestComplete(player, questID)) {
        LOG_INFO("AcceptQuest: quest " + data->title + " already active/complete.");
        return false;
    }

    // 4. Find a free slot in QuestComponent.
    auto& qc = m_world->GetComponent<QuestComponent>(player);
    if (qc.activeCount >= MAX_QUESTS) {
        LOG_WARN("AcceptQuest: quest log full.");
        return false;
    }

    // 5. Populate a new QuestEntry.
    // TEACHING NOTE — QuestEntry stores RUNTIME progress while QuestData holds
    // the static definition.  We copy only the IDs, not the full definition,
    // to keep memory usage low.
    QuestEntry& entry = qc.quests[qc.activeCount++];
    entry.questID    = questID;
    entry.objective  = 0;  // first objective
    entry.progress   = 0;
    entry.required   = static_cast<int32_t>(
        data->objectives.empty() ? 1 : data->objectives[0].requiredCount);
    entry.isComplete = false;
    entry.isFailed   = false;

    // 6. Fire events.
    PublishQuestEvent(QuestEvent::Type::QUEST_ACCEPTED, player, questID, 0, 0);

    if (m_uiBus) {
        UIEvent ui;
        ui.type    = UIEvent::Type::SHOW_NOTIFICATION;
        ui.text = "Quest Accepted: " + data->title;
        m_uiBus->Publish(ui);
    }

    LOG_INFO("Quest accepted: " + data->title);
    return true;
}

// ---------------------------------------------------------------------------
// UpdateObjective
// ---------------------------------------------------------------------------

void QuestSystem::UpdateObjective(EntityID player, uint32_t questID,
                                  uint32_t objectiveIdx, uint32_t progress)
{
    auto& qc = m_world->GetComponent<QuestComponent>(player);

    // Find the matching quest entry.
    QuestEntry* entry = qc.FindQuest(questID);
    if (!entry || entry->isComplete || entry->isFailed) return;

    // Only update the CURRENT objective.
    if (entry->objective != objectiveIdx) return;

    const QuestData* data = GameDatabase::FindQuest(questID);
    if (!data || objectiveIdx >= data->objectives.size()) return;

    // Advance progress.
    entry->progress += static_cast<int32_t>(progress);

    // TEACHING NOTE — Clamp progress so it never exceeds the required amount.
    // This prevents displaying "4/3 goblins killed" in the UI.
    if (entry->progress > entry->required) entry->progress = entry->required;

    PublishQuestEvent(QuestEvent::Type::OBJECTIVE_UPDATED, player, questID,
                      objectiveIdx, static_cast<int32_t>(progress));

    // Check if current objective is now complete.
    if (entry->progress >= entry->required) {
        entry->objective++;  // advance to next objective

        if (entry->objective >= static_cast<uint32_t>(data->objectives.size())) {
            // All objectives done — auto-complete the quest.
            CompleteQuest(player, questID);
        } else {
            // Set up the next objective's required count.
            entry->progress = 0;
            entry->required = static_cast<int32_t>(
                data->objectives[entry->objective].requiredCount);

            if (m_uiBus) {
                UIEvent ui;
                ui.type    = UIEvent::Type::SHOW_NOTIFICATION;
                ui.text = "Objective Complete! Next: " +
                             data->objectives[entry->objective].description;
                m_uiBus->Publish(ui);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// CompleteQuest
// ---------------------------------------------------------------------------

bool QuestSystem::CompleteQuest(EntityID player, uint32_t questID)
{
    auto& qc = m_world->GetComponent<QuestComponent>(player);
    QuestEntry* entry = qc.FindQuest(questID);
    if (!entry || entry->isComplete) return false;

    const QuestData* data = GameDatabase::FindQuest(questID);
    if (!data) return false;

    // Mark complete.
    entry->isComplete = true;

    // Award XP.
    if (m_world->HasComponent<LevelComponent>(player)) {
        auto& lc = m_world->GetComponent<LevelComponent>(player);
        lc.GainXP(data->xpReward);
        LOG_INFO("Quest XP awarded: " + std::to_string(data->xpReward));
    }

    // Award Gil — credit CurrencyComponent (single source of truth for Gil).
    if (m_world->HasComponent<CurrencyComponent>(player)) {
        m_world->GetComponent<CurrencyComponent>(player).EarnGil(data->gilReward);
    }

    // Award items.
    GrantItemRewards(player, *data);

    // Fire quest-complete event.
    PublishQuestEvent(QuestEvent::Type::QUEST_COMPLETED, player, questID, 0, 0);

    if (m_uiBus) {
        UIEvent ui;
        ui.type    = UIEvent::Type::SHOW_NOTIFICATION;
        ui.text = "Quest Complete: " + data->title +
                     "  (+" + std::to_string(data->xpReward) + " XP, " +
                     "+" + std::to_string(data->gilReward) + " Gil)";
        m_uiBus->Publish(ui);
    }

    LOG_INFO("Quest completed: " + data->title);
    return true;
}

// ---------------------------------------------------------------------------
// FailQuest
// ---------------------------------------------------------------------------

void QuestSystem::FailQuest(EntityID player, uint32_t questID)
{
    auto& qc = m_world->GetComponent<QuestComponent>(player);
    QuestEntry* entry = qc.FindQuest(questID);
    if (!entry || entry->isComplete || entry->isFailed) return;

    entry->isFailed = true;

    PublishQuestEvent(QuestEvent::Type::QUEST_FAILED, player, questID, 0, 0);

    const QuestData* data = GameDatabase::FindQuest(questID);
    if (m_uiBus && data) {
        UIEvent ui;
        ui.type    = UIEvent::Type::SHOW_NOTIFICATION;
        ui.text = "Quest Failed: " + data->title;
        m_uiBus->Publish(ui);
    }
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

std::vector<const QuestData*> QuestSystem::GetActiveQuests(EntityID player) const
{
    std::vector<const QuestData*> result;
    auto& qc = m_world->GetComponent<QuestComponent>(player);

    for (uint32_t i = 0; i < qc.activeCount; ++i) {
        const QuestEntry& e = qc.quests[i];
        if (!e.isComplete && !e.isFailed) {
            const QuestData* data = GameDatabase::FindQuest(e.questID);
            if (data) result.push_back(data);
        }
    }
    return result;
}

bool QuestSystem::IsQuestComplete(EntityID player, uint32_t questID) const
{
    auto& qc = m_world->GetComponent<QuestComponent>(player);
    const QuestEntry* entry = qc.FindQuest(questID);
    return entry && entry->isComplete;
}

bool QuestSystem::IsQuestActive(EntityID player, uint32_t questID) const
{
    auto& qc = m_world->GetComponent<QuestComponent>(player);
    const QuestEntry* entry = qc.FindQuest(questID);
    return entry && !entry->isComplete && !entry->isFailed;
}

bool QuestSystem::CanAcceptQuest(EntityID player, uint32_t questID) const
{
    const QuestData* data = GameDatabase::FindQuest(questID);
    if (!data) return false;

    // Check every prerequisite quest is completed.
    for (uint32_t prereqID : data->prereqQuestIDs) {
        if (!IsQuestComplete(player, prereqID)) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Event-driven hooks
// ---------------------------------------------------------------------------

void QuestSystem::OnEnemyKilled(EntityID player, uint32_t enemyDataID)
{
    auto& qc = m_world->GetComponent<QuestComponent>(player);
    for (uint32_t i = 0; i < qc.activeCount; ++i) {
        const QuestEntry& e = qc.quests[i];
        if (e.isComplete || e.isFailed) continue;

        const QuestData* data = GameDatabase::FindQuest(e.questID);
        if (!data || e.objective >= data->objectives.size()) continue;

        const QuestObjective& obj = data->objectives[e.objective];
        // TEACHING NOTE — We match objective type AND the specific enemy type.
        if (obj.type == QuestObjectiveType::KILL_ENEMY &&
            obj.targetID == enemyDataID)
        {
            UpdateObjective(player, e.questID, e.objective, 1);
        }
    }
}

void QuestSystem::OnItemCollected(EntityID player, uint32_t itemID, uint32_t qty)
{
    auto& qc = m_world->GetComponent<QuestComponent>(player);
    for (uint32_t i = 0; i < qc.activeCount; ++i) {
        const QuestEntry& e = qc.quests[i];
        if (e.isComplete || e.isFailed) continue;

        const QuestData* data = GameDatabase::FindQuest(e.questID);
        if (!data || e.objective >= data->objectives.size()) continue;

        const QuestObjective& obj = data->objectives[e.objective];
        if (obj.type == QuestObjectiveType::COLLECT_ITEM &&
            obj.targetID == itemID)
        {
            UpdateObjective(player, e.questID, e.objective, qty);
        }
    }
}

void QuestSystem::OnLocationReached(EntityID player, uint32_t zoneID)
{
    auto& qc = m_world->GetComponent<QuestComponent>(player);
    for (uint32_t i = 0; i < qc.activeCount; ++i) {
        const QuestEntry& e = qc.quests[i];
        if (e.isComplete || e.isFailed) continue;

        const QuestData* data = GameDatabase::FindQuest(e.questID);
        if (!data || e.objective >= data->objectives.size()) continue;

        const QuestObjective& obj = data->objectives[e.objective];
        if (obj.type == QuestObjectiveType::REACH_LOCATION &&
            obj.targetID == zoneID)
        {
            UpdateObjective(player, e.questID, e.objective, 1);
        }
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void QuestSystem::PublishQuestEvent(QuestEvent::Type type, EntityID player,
                                    uint32_t questID, uint32_t objectiveID,
                                    int32_t delta) const
{
    if (!m_questBus) return;
    QuestEvent ev;
    ev.type          = type;
    ev.playerID      = player;
    ev.questID       = questID;
    ev.objectiveID   = objectiveID;
    ev.progressDelta = delta;
    m_questBus->Publish(ev);
}

void QuestSystem::GrantItemRewards(EntityID player, const QuestData& questData)
{
    if (questData.itemRewards.empty()) return;
    if (!m_world->HasComponent<InventoryComponent>(player)) return;

    // Use a temporary InventorySystem to properly add items.
    InventorySystem invSys(m_world, m_uiBus);
    for (uint32_t itemID : questData.itemRewards) {
        invSys.AddItem(player, itemID, 1);
    }
}
