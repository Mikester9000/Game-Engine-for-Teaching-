/**
 * @file dialogue_system.cpp
 * @brief NPC dialogue state machine implementation — M8.6 Dialogue stub.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2025
 */

#include "game/systems/dialogue_system.hpp"
#include "engine/core/Logger.hpp"
#include <cmath>     // std::sqrt
#include <limits>    // std::numeric_limits

// ===========================================================================
// Static stub dialogue node
// ===========================================================================

const DialogueNode DialogueSystem::kStubNode = {
    /* id          */ 0,
    /* speakerName */ "NPC",
    /* text        */ "Hello, traveller! Safe journeys.",
    /* choices     */ {},
    /* nextNodeID  */ 0,
    /* isTerminal  */ true
};

// ===========================================================================
// Constructor
// ===========================================================================

DialogueSystem::DialogueSystem(World* world)
    : m_world(world)
{
    LOG_INFO("DialogueSystem created (M8.6 stub)");
}

// ===========================================================================
// Update
// ===========================================================================

void DialogueSystem::Update(World& world, EntityID playerID, float dt)
{
    (void)dt;  // Reserved for typing animation timing

    if (playerID == NULL_ENTITY || !world.IsAlive(playerID))
        return;

    if (!world.HasComponent<TransformComponent>(playerID))
        return;

    const auto& playerTf = world.GetComponent<TransformComponent>(playerID);

    // TEACHING NOTE — Proximity check for interactable NPCs.
    // We iterate all entities with both a DialogueComponent and a
    // TransformComponent.  This is an O(entities) scan — acceptable for
    // < 200 NPCs per zone.  For massive crowds, maintain a spatial hash.

    world.View<DialogueComponent, TransformComponent>(
        [&](EntityID entity, DialogueComponent& dlg, TransformComponent& tf)
        {
            if (entity == playerID) return;  // Skip player itself.

            // Compute 2D (XZ plane) squared distance from player to NPC.
            const float dx = tf.position.x - playerTf.position.x;
            const float dz = tf.position.z - playerTf.position.z;
            const float distSq = dx*dx + dz*dz;
            const float range  = dlg.interactRange;

            dlg.isInteractable = (distSq < (range * range));
        });
}

// ===========================================================================
// BeginDialogue
// ===========================================================================

bool DialogueSystem::BeginDialogue(World& world, EntityID playerID)
{
    if (m_isActive)
    {
        LOG_WARN("DialogueSystem::BeginDialogue called while dialogue already active.");
        return false;
    }

    if (playerID == NULL_ENTITY || !world.IsAlive(playerID))
        return false;

    // Find the nearest interactable NPC.
    EntityID nearest    = NULL_ENTITY;
    float    nearestDist = std::numeric_limits<float>::max();

    const auto* playerTfPtr = world.HasComponent<TransformComponent>(playerID)
        ? &world.GetComponent<TransformComponent>(playerID)
        : nullptr;

    world.View<DialogueComponent, TransformComponent>(
        [&](EntityID entity, DialogueComponent& dlg, TransformComponent& tf)
        {
            if (!dlg.isInteractable) return;
            if (entity == playerID)  return;

            float dist = 0.0f;
            if (playerTfPtr)
            {
                const float dx = tf.position.x - playerTfPtr->position.x;
                const float dz = tf.position.z - playerTfPtr->position.z;
                dist = std::sqrt(dx*dx + dz*dz);
            }
            if (dist < nearestDist)
            {
                nearestDist = dist;
                nearest     = entity;
            }
        });

    if (nearest == NULL_ENTITY)
    {
        LOG_INFO("DialogueSystem::BeginDialogue: no interactable NPC in range.");
        return false;
    }

    m_activeNpcEntity = nearest;
    m_currentNodeID   = 0;
    m_isActive        = true;

    // Publish OPENED event.
    DialogueEvent ev;
    ev.type        = DialogueEvent::Type::OPENED;
    ev.npcEntity   = nearest;
    ev.speakerName = kStubNode.speakerName;
    ev.text        = kStubNode.text;
    ev.currentNode = m_currentNodeID;
    EventBus<DialogueEvent>::Instance().Publish(ev);

    LOG_INFO("DialogueSystem: dialogue opened with entity " << nearest
             << " (\"" << kStubNode.text << "\")");
    return true;
}

// ===========================================================================
// AdvanceDialogue
// ===========================================================================

bool DialogueSystem::AdvanceDialogue(World& world)
{
    if (!m_isActive) return false;

    // STUB: the only node is terminal; close immediately.
    CloseDialogue(world);
    return false;
}

// ===========================================================================
// CloseDialogue
// ===========================================================================

void DialogueSystem::CloseDialogue(World& world)
{
    if (!m_isActive) return;

    // Reset isInteractable on the NPC so it doesn't keep showing the prompt.
    if (m_activeNpcEntity != NULL_ENTITY && world.IsAlive(m_activeNpcEntity)
        && world.HasComponent<DialogueComponent>(m_activeNpcEntity))
    {
        world.GetComponent<DialogueComponent>(m_activeNpcEntity).isInteractable = false;
    }

    m_isActive        = false;
    m_activeNpcEntity = NULL_ENTITY;
    m_currentNodeID   = 0;

    // Publish CLOSED event.
    DialogueEvent ev;
    ev.type = DialogueEvent::Type::CLOSED;
    EventBus<DialogueEvent>::Instance().Publish(ev);

    LOG_INFO("DialogueSystem: dialogue closed.");
}
