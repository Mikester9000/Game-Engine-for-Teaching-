/**
 * @file AISystem.hpp
 * @brief Enemy AI state machine and A* pathfinding for the educational RPG engine.
 *
 * ============================================================================
 * TEACHING NOTE — Enemy AI Architecture
 * ============================================================================
 *
 * Artificial Intelligence in games is NOT about making "smart" opponents.
 * It is about making opponents that are FUN TO PLAY AGAINST — which often
 * means making them predictable enough that players can learn their patterns.
 *
 * ─── Finite State Machine (FSM) ─────────────────────────────────────────────
 *
 * The simplest and most common AI architecture is the Finite State Machine.
 * An entity is always in exactly ONE state, and TRANSITIONS happen based on
 * conditions (distance to player, HP percentage, timer expiry).
 *
 * Our enemy FSM:
 *
 *   ┌──────┐  always     ┌───────────┐  player in range  ┌─────────┐
 *   │ IDLE │ ──────────► │ WANDERING │ ────────────────► │ CHASING │
 *   └──────┘             └───────────┘                   └────┬────┘
 *       ▲                      ▲                              │
 *       │  player lost         │  player far away             │ player adjacent
 *       │                      └──────────────────────────────│──────────────┐
 *       │                                                      ▼              │
 *       │                                               ┌──────────┐         │
 *       │  HP < 25%                                     │ ATTACKING│ ◄───────┘
 *       │ ◄─────────────────────────────────────────── └──────────┘
 *       │                                                    │
 *       ▼                                                    │ HP < 25%
 *   ┌─────────┐                                             ▼
 *   │ FLEEING │                                         ┌──────┐
 *   └─────────┘                                         │ DEAD │
 *                                                       └──────┘
 *
 * TEACHING NOTE — Why FSM?
 * The FSM pattern is:
 *   ✓ Easy to understand and debug (always know what state an entity is in).
 *   ✓ Easy to extend (add a STUNNED state without touching other states).
 *   ✓ Deterministic (same inputs → same outputs; reproducible bugs).
 *   ✗ Doesn't scale to complex behaviours (use Behaviour Trees for that).
 *
 * ─── Behaviour Trees (Advanced Concept) ─────────────────────────────────────
 *
 * Behaviour Trees are a more powerful alternative to FSMs used in production
 * games (Halo, The Last of Us, Unreal Engine).  They represent behaviour as
 * a tree of nodes:
 *
 *   Selector (try children left to right until one succeeds)
 *     ├── Sequence (is enemy adjacent AND has cooldown expired?)
 *     │     ├── CheckAdjacent(player)
 *     │     └── AttackAction(player)
 *     └── Sequence (can enemy see player?)
 *           ├── CheckLineOfSight(player)
 *           └── MoveTowardAction(player)
 *
 * We don't implement a full BT here to keep the code educational, but
 * students are encouraged to research it as a next step.
 *
 * ─── A* Pathfinding ──────────────────────────────────────────────────────────
 *
 * A* (pronounced "A star") is the gold-standard 2D grid pathfinding algorithm.
 * It finds the SHORTEST path from start to goal, guaranteed.
 *
 * A* extends Dijkstra's algorithm with a HEURISTIC ESTIMATE of the remaining
 * distance, making it much faster in practice.
 *
 * Key concepts:
 *
 *   g(n) = exact cost to reach node n from start (number of steps so far).
 *   h(n) = HEURISTIC estimate of distance from n to goal (Manhattan distance).
 *   f(n) = g(n) + h(n) = total estimated cost through n.
 *
 *   OPEN SET   = nodes to be evaluated (priority queue ordered by f).
 *   CLOSED SET = nodes already evaluated.
 *
 * Algorithm:
 *   1. Add start to open set with f=h(start).
 *   2. Pop the node with lowest f from open set.
 *   3. If it is the goal, reconstruct path and return.
 *   4. For each passable neighbour:
 *      - Compute g' = g(current) + stepCost.
 *      - If g' < g(neighbour) (found a shorter path), update neighbour and
 *        add it to the open set with new f = g' + h(neighbour).
 *   5. If open set is empty, no path exists.
 *
 * MANHATTAN HEURISTIC: h(n) = |n.x - goal.x| + |n.y - goal.y|
 * Admissible (never overestimates on a 4-directional grid without diagonal).
 *
 * ─── TileCoord ───────────────────────────────────────────────────────────────
 *
 * Pathfinding works in TILE coordinates (integers), not world coordinates
 * (floats).  The TileCoord struct from Types.hpp holds (tileX, tileY).
 * The TransformComponent stores (x, y) in WORLD units; we divide by
 * TILE_SIZE to convert.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 *
 * C++ Standard: C++17
 */

#pragma once

#include <vector>
#include <queue>
#include <unordered_map>
#include <cstdint>
#include <cmath>         // std::abs

#include "../../engine/core/Types.hpp"
#include "../../engine/core/Logger.hpp"
#include "../../engine/core/EventBus.hpp"
#include "../../engine/ecs/ECS.hpp"
#include "../world/TileMap.hpp"


// ===========================================================================
// AIState — the possible states of an enemy's behaviour
// ===========================================================================

/**
 * @enum AIState
 * @brief All possible states in the enemy finite state machine.
 *
 * TEACHING NOTE — Mirroring Component Enums
 * ───────────────────────────────────────────
 * AIComponent (in ECS.hpp) has its own `State` enum for the same values.
 * We declare a standalone `AIState` enum here for use in AISystem logic
 * (switch statements, function signatures) to keep the system header
 * self-contained.  In the Update loop, we sync between AIComponent::State
 * and AIState via static_cast (they have matching underlying values).
 */
enum class AIState : uint8_t {
    IDLE      = 0,  ///< Standing still; not actively patrolling.
    WANDERING = 1,  ///< Moving randomly; passively patrolling.
    CHASING   = 2,  ///< Player spotted; moving toward player.
    ATTACKING = 3,  ///< Adjacent to player; executing attack.
    FLEEING   = 4,  ///< Low HP; running away.
    DEAD      = 5   ///< Entity is dead; skipped in update.
};


// ===========================================================================
// PathNode — internal node used in A* pathfinding
// ===========================================================================

/**
 * @struct PathNode
 * @brief One node in the A* open set, with priority ordering.
 *
 * TEACHING NOTE — Priority Queue Ordering
 * ──────────────────────────────────────────
 * std::priority_queue is a MAX-heap by default.  For A* we need a MIN-heap
 * (process lowest f-cost first).  We achieve this by defining operator>
 * and using `std::greater<PathNode>` as the comparator:
 *
 *   std::priority_queue<PathNode, std::vector<PathNode>, std::greater<PathNode>>
 *
 * The operator> compares fCost, making the queue pop the lowest fCost.
 */
struct PathNode {
    TileCoord coord;    ///< Tile position.
    float     fCost;    ///< f(n) = g(n) + h(n).

    /// Needed for std::greater comparator in the priority queue.
    bool operator>(const PathNode& other) const {
        return fCost > other.fCost;
    }
};


// ===========================================================================
// AISystem class
// ===========================================================================

/**
 * @class AISystem
 * @brief Drives enemy state-machine behaviour and A* pathfinding.
 *
 * The AISystem iterates ALL entities with AIComponent each frame and
 * calls ProcessEntity() for each one.  ProcessEntity reads the entity's
 * current state and transitions it according to world conditions.
 *
 * USAGE EXAMPLE:
 * @code
 *   AISystem ai(world, playerID);
 *
 *   // Inside game loop:
 *   ai.Update(world, tileMap, deltaTime);
 * @endcode
 */
class AISystem {
public:

    /**
     * @brief Construct the AI system.
     *
     * @param world     Non-owning pointer to ECS World.
     * @param playerID  The player entity's ID (enemies chase this entity).
     */
    AISystem(World* world, EntityID playerID);

    // -----------------------------------------------------------------------
    // Main update
    // -----------------------------------------------------------------------

    /**
     * @brief Update all AI entities for this frame.
     *
     * Iterates every entity with an AIComponent and calls ProcessEntity().
     * Entities in state DEAD are skipped.
     *
     * @param world  ECS World reference (used to query components).
     * @param map    Current zone's tile map (used for pathfinding and bounds).
     * @param dt     Real delta time in seconds.
     */
    void Update(World& world, TileMap& map, float dt);

    /**
     * @brief Process one AI entity for this frame.
     *
     * Implements the FSM:
     *   IDLE      → start wandering after wanderTimer expires.
     *   WANDERING → chase if player is within aggroRange.
     *   CHASING   → move toward player; attack if adjacent.
     *   ATTACKING → deal damage to player; check for flee condition.
     *   FLEEING   → move away from player; idle when far enough.
     *   DEAD      → do nothing.
     *
     * @param entity  The AI entity to process.
     * @param world   ECS World.
     * @param map     Tile map.
     * @param dt      Delta time.
     */
    void ProcessEntity(EntityID entity, World& world,
                       TileMap& map, float dt);

    // -----------------------------------------------------------------------
    // Pathfinding
    // -----------------------------------------------------------------------

    /**
     * @brief Find the shortest walkable path from (sx,sy) to (tx,ty).
     *
     * Implements the A* algorithm with Manhattan distance heuristic.
     * Returns a vector of TileCoord from start (exclusive) to goal
     * (inclusive), or an empty vector if no path exists.
     *
     * @param map         Tile map — used to check which tiles are passable.
     * @param sx,sy       Source tile coordinates.
     * @param tx,ty       Target tile coordinates.
     * @return            Ordered list of tile steps from start to goal.
     *                    Empty if start == goal or no path found.
     *
     * TEACHING NOTE — A* Complexity
     * ─────────────────────────────────
     * Time:  O(E log V) where V = number of tiles, E = edges (4 neighbours each).
     * Space: O(V) for the open/closed sets and came-from map.
     *
     * For a 40×40 tile map (1600 tiles) this is very fast (<0.1ms).
     * For very large maps, consider:
     *   - Hierarchical pathfinding (HPA*): divide map into clusters.
     *   - Pre-computed waypoint graphs for known routes.
     *   - Steering behaviours (seek, flee) for enemies that don't need
     *     exact paths.
     */
    std::vector<TileCoord> FindPath(const TileMap& map,
                                    int sx, int sy,
                                    int tx, int ty) const;

    // -----------------------------------------------------------------------
    // Movement
    // -----------------------------------------------------------------------

    /**
     * @brief Move `entity` one step toward `target` tile.
     *
     * Calls FindPath() and moves the entity to the first step in the path
     * (if the path is non-empty and the first step is passable).
     *
     * Updates TransformComponent.x and .y by TILE_SIZE per call.
     * The calling code (ProcessEntity) is responsible for calling this at
     * an appropriate rate (not every frame — too fast; every N seconds).
     *
     * @param entity  Moving entity (must have TransformComponent).
     * @param map     Tile map for passability checks.
     * @param target  Destination tile coordinate.
     */
    void MoveToward(EntityID entity, TileMap& map, TileCoord target);

    // -----------------------------------------------------------------------
    // Decision helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Decide if an enemy should aggro onto the player.
     *
     * Conditions for aggro:
     *  1. Player is alive (HealthComponent::isDead == false).
     *  2. Distance (tile) ≤ AIComponent::aggroRange.
     *  3. Enemy is not already in CHASING or ATTACKING state.
     *
     * @return true if the enemy should start chasing.
     */
    bool ShouldAggro(EntityID enemy, EntityID player, World& world) const;

    /**
     * @brief Select the best ability index for this enemy to use.
     *
     * Reads CombatComponent::abilities[] and applies simple heuristics:
     *   - If enemy HP < 30%, prefer healing abilities.
     *   - If player HP < 50%, prefer high-damage abilities.
     *   - Otherwise, select randomly.
     *
     * @return Index into CombatComponent::abilities, or -1 for basic attack.
     *
     * TEACHING NOTE — Simple vs Complex AI Decision Making
     * ──────────────────────────────────────────────────────
     * This is REACTIVE AI: the enemy looks at current state and picks an
     * action.  More sophisticated AI (used in chess, StarCraft) uses
     * LOOK-AHEAD: "If I do X, what will the player do, and how should I
     * respond to THAT?".  Look-ahead requires search trees (minimax, MCTS)
     * which are far beyond a simple action-RPG's needs.
     */
    int SelectAbility(EntityID enemy, World& world) const;

private:

    // -----------------------------------------------------------------------
    // A* internals
    // -----------------------------------------------------------------------

    /**
     * @brief Compute the Manhattan distance heuristic between two tiles.
     *
     * h(n) = |n.tileX - goal.tileX| + |n.tileY - goal.tileY|
     *
     * Manhattan distance is ADMISSIBLE on a 4-directional grid (it never
     * overestimates the true shortest path).  An admissible heuristic
     * guarantees A* finds the optimal path.
     */
    static float Heuristic(TileCoord a, TileCoord b);

    /**
     * @brief Return the 4 cardinal neighbours of a tile (excluding diagonals).
     *
     * We use 4-directional movement for simplicity.  8-directional (including
     * diagonals) is possible by adding 4 more offsets, but diagonal movement
     * in tile maps can allow corner-cutting through walls.
     */
    static std::vector<TileCoord> GetNeighbours(TileCoord node,
                                                  const TileMap& map);

    /**
     * @brief Reconstruct the path from the cameFrom map.
     *
     * Follows parent pointers backward from goal to start, then reverses.
     *
     * TEACHING NOTE — Path Reconstruction
     * ──────────────────────────────────────
     * During A* we store `cameFrom[n] = parent_of_n` for each node we visit.
     * When we reach the goal, we walk backwards through cameFrom:
     *   goal → parent → grandparent → … → start
     * Then reverse the list to get start → goal order.
     */
    static std::vector<TileCoord> ReconstructPath(
        const std::unordered_map<uint64_t, TileCoord>& cameFrom,
        TileCoord goal);

    /**
     * @brief Encode a TileCoord into a single uint64_t for use as a map key.
     *
     * Key = (tileX << 32) | (uint32_t)tileY.
     * Allows use of TileCoord as a key in std::unordered_map without
     * defining a custom hash (we just hash the uint64_t directly).
     */
    static uint64_t EncodeCoord(TileCoord c);

    // -----------------------------------------------------------------------
    // State transition helpers
    // -----------------------------------------------------------------------

    /// Transition `entity` to the given AIState, updating AIComponent::state.
    void SetState(EntityID entity, AIState state);

    /**
     * @brief Return the tile distance between two entities.
     *
     * Distance = Manhattan distance between their TransformComponent positions
     * divided by TILE_SIZE.
     */
    float TileDistance(EntityID a, EntityID b) const;

    /**
     * @brief Execute a basic melee attack from `enemy` on `player`.
     *
     * Applies StatsComponent::strength damage reduced by player's defence.
     * Fires CombatEvent::DAMAGE_DEALT.
     */
    void ExecuteMeleeAttack(EntityID enemy, EntityID player);

    // -----------------------------------------------------------------------
    // Members
    // -----------------------------------------------------------------------

    World*   m_world    = nullptr;
    EntityID m_playerID = NULL_ENTITY;

    mutable std::mt19937 m_rng{99};  ///< RNG for ability selection and wandering.
};
