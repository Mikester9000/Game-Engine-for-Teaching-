/**
 * @file dialogue_system.hpp
 * @brief NPC dialogue state machine — M8.6 Dialogue System stub.
 *
 * ============================================================================
 * TEACHING NOTE — Dialogue System Architecture
 * ============================================================================
 * A dialogue system is a STATE MACHINE over conversation nodes, driven by
 * player input and game state conditions.  This implementation is a stub
 * that correctly tracks component state and can be extended into a full
 * branching dialogue tree.
 *
 * ─── Minimal Viable Dialogue Flow ────────────────────────────────────────────
 *
 *   Player approaches NPC → DialogueComponent.interactRange check → OPEN state
 *   Player presses INTERACT → advance currentNodeID → show text
 *   End of tree → CLOSED state
 *
 * ─── DialogueComponent fields (in ECS.hpp) ───────────────────────────────────
 *
 *   dialogueTreeID  — asset GUID for the JSON dialogue tree (not yet loaded)
 *   currentNodeID   — which node in the tree is active (0 = start)
 *   isInteractable  — true if the player is in range
 *   interactRange   — world-space trigger radius in tiles
 *
 * ─── Full Implementation (post-M8) ───────────────────────────────────────────
 * A complete dialogue system would:
 *   1. Load dialogue trees from JSON (keyed by dialogueTreeID GUID).
 *   2. Implement branching choices (QTE, affinity gates, item checks).
 *   3. Play voice-over audio via AudioSystem.
 *   4. Drive facial animation via AnimationSystem.
 *   5. Show subtitles via HUD.
 *   6. Fire quest events when certain nodes are reached.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2025
 * C++ Standard: C++17
 */

#pragma once

#include <string>
#include <utility>  // std::pair — used by DialogueNode::choices
#include <vector>
#include <cstdint>

#include "engine/core/Types.hpp"
#include "engine/core/Logger.hpp"
#include "engine/core/EventBus.hpp"
#include "engine/ecs/ECS.hpp"

// ===========================================================================
// DialogueNode — one conversational exchange
// ===========================================================================

/**
 * @struct DialogueNode
 * @brief A single node in a branching dialogue tree.
 *
 * TEACHING NOTE — Tree vs Linear Dialogue
 * ─────────────────────────────────────────
 * Simple RPGs (classic Final Fantasy) use *linear* dialogue: one NPC says
 * one thing in sequence.  FF15 style uses *branching trees*: the player
 * picks a response from a menu and different branches play out.
 *
 * We model both as the same tree structure — a linear sequence is just a tree
 * with one child per node.  Branching is optional (choices.size() > 1).
 */
struct DialogueNode
{
    uint32_t    id           = 0;          ///< Unique node ID within the tree.
    std::string speakerName;               ///< Who is speaking ("Ignis", "Quest NPC").
    std::string text;                      ///< Dialogue line text.

    /// TEACHING NOTE — Branch choices.
    /// Each choice has a display label and a target node ID.
    /// If choices is empty the system auto-advances to nextNodeID.
    std::vector<std::pair<std::string, uint32_t>> choices;

    uint32_t    nextNodeID   = 0;          ///< Auto-advance target (if no choices).
    bool        isTerminal   = false;      ///< True = end of conversation.
};

// ===========================================================================
// DialogueEvent — published to EventBus when dialogue state changes
// ===========================================================================

/**
 * @struct DialogueEvent
 * @brief Published by DialogueSystem when dialogue opens, advances, or closes.
 *
 * TEACHING NOTE — Event-Driven UI
 * ────────────────────────────────
 * The HUD subscribes to DialogueEvent to show / hide the text box without
 * knowing about DialogueSystem internals.  This is the Observer pattern:
 *   Publisher: DialogueSystem
 *   Observer:  HUD (subscribes to EventBus<DialogueEvent>)
 */
struct DialogueEvent
{
    enum class Type { OPENED, NODE_CHANGED, CLOSED };

    Type        type        = Type::OPENED;
    EntityID    npcEntity   = NULL_ENTITY;
    std::string speakerName;
    std::string text;
    uint32_t    currentNode = 0;
};

// ===========================================================================
// DialogueSystem
// ===========================================================================

/**
 * @class DialogueSystem
 * @brief Manages NPC dialogue interaction for all entities with DialogueComponent.
 *
 * STUB STATUS (M8.6): This implementation:
 *   ✅ Scans all entities with DialogueComponent each frame.
 *   ✅ Checks player proximity against interactRange.
 *   ✅ Sets isInteractable on the DialogueComponent.
 *   ✅ Provides BeginDialogue() and AdvanceDialogue() for HUD/input use.
 *   ✅ Publishes DialogueEvent to EventBus.
 *   ⬜ Does NOT load actual dialogue tree JSON (queued for post-M8).
 *   ⬜ Does NOT render UI choices (queued for UI system M8.5/post-M8).
 */
class DialogueSystem
{
public:

    /**
     * @brief Construct DialogueSystem.
     * @param world  Non-owning pointer to the ECS World.  Must outlive system.
     */
    explicit DialogueSystem(World* world);
    ~DialogueSystem() = default;

    // Non-copyable.
    DialogueSystem(const DialogueSystem&)            = delete;
    DialogueSystem& operator=(const DialogueSystem&) = delete;

    // =========================================================================
    // Per-frame update
    // =========================================================================

    /**
     * @brief Scan all NPC entities and update interactable flags.
     *
     * For each entity with a DialogueComponent:
     *   - Compute distance from player to NPC.
     *   - Set DialogueComponent::isInteractable = (distance < interactRange).
     *
     * @param world     ECS World reference.
     * @param playerID  Player entity (range is measured from here).
     * @param dt        Delta time in seconds (unused currently; reserved for
     *                  typing animation effects).
     */
    void Update(World& world, EntityID playerID, float dt);

    // =========================================================================
    // Interaction API (called by InputMapper / GameRuntime)
    // =========================================================================

    /**
     * @brief Begin a dialogue with the nearest interactable NPC.
     *
     * Finds the nearest entity with isInteractable=true and opens its
     * first dialogue node.  Publishes DialogueEvent::OPENED.
     *
     * @param world     ECS World.
     * @param playerID  Player entity (conversation partner).
     * @return true if a dialogue was opened, false if no NPC in range.
     */
    bool BeginDialogue(World& world, EntityID playerID);

    /**
     * @brief Advance the active dialogue to the next node.
     *
     * If choices are present the first choice is taken automatically
     * (full choice UI is planned for post-M8).
     *
     * Publishes DialogueEvent::NODE_CHANGED or DialogueEvent::CLOSED
     * depending on whether more nodes remain.
     *
     * @return true if dialogue is still active after this call.
     */
    bool AdvanceDialogue(World& world);

    /**
     * @brief Returns true if a dialogue is currently in progress.
     */
    bool IsActive() const { return m_isActive; }

    /**
     * @brief Immediately close any active dialogue (e.g. on combat start).
     */
    void CloseDialogue(World& world);

private:
    World*   m_world              = nullptr;
    EntityID m_activeNpcEntity    = NULL_ENTITY;
    uint32_t m_currentNodeID      = 0;
    bool     m_isActive           = false;

    /// TEACHING NOTE — Inline stub tree (placeholder until JSON loading).
    /// The single node says "Hello, traveller!" and terminates immediately.
    static const DialogueNode kStubNode;
};
