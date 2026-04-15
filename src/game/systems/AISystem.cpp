/**
 * @file AISystem.cpp
 * @brief Implementation of the enemy AI state machine and A* pathfinding.
 *
 * ============================================================================
 * TEACHING NOTE — A* Algorithm Overview
 * ============================================================================
 *
 * A* is the gold standard for grid pathfinding.  It extends Dijkstra's
 * algorithm with a heuristic function h(n) that estimates the remaining
 * cost from node n to the goal.
 *
 * Core formula: f(n) = g(n) + h(n)
 *   g(n) = exact cost from start to n (accumulated step costs)
 *   h(n) = estimated cost from n to goal (Manhattan distance)
 *   f(n) = priority of n in the open set
 *
 * The algorithm always expands the node with the lowest f(n) first.
 * If h(n) never overestimates (admissible), A* finds the OPTIMAL path.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 */

#include "AISystem.hpp"
#include "CombatSystem.hpp"   // for ExecuteMeleeAttack

#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// Helper: get tile-space position of an entity
// ---------------------------------------------------------------------------

static TileCoord GetTilePos(EntityID entity, const World& world)
{
    if (!world.HasComponent<TransformComponent>(entity))
        return {0, 0};
    const auto& tc = world.GetComponent<TransformComponent>(entity);
    return TileCoord{
        static_cast<int32_t>(tc.position.x / TILE_SIZE),
        static_cast<int32_t>(tc.position.z / TILE_SIZE)
    };
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

AISystem::AISystem(World* world, EntityID playerID)
    : m_world(world), m_playerID(playerID)
{
    LOG_INFO("AISystem initialised.");
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void AISystem::Update(World& world, TileMap& map, float dt)
{
    // Iterate all entities that have both TransformComponent and AIComponent.
    world.View<TransformComponent, AIComponent>(
        [&](EntityID entity, TransformComponent& /*tc*/, AIComponent& ai) {
            // Skip dead entities.
            if (ai.currentState == AIComponent::State::DEAD) return;

            // Also skip entities whose HealthComponent marks them as dead.
            if (world.HasComponent<HealthComponent>(entity)) {
                if (world.GetComponent<HealthComponent>(entity).isDead) {
                    ai.currentState = AIComponent::State::DEAD;
                    return;
                }
            }

            ProcessEntity(entity, world, map, dt);
        });
}

// ---------------------------------------------------------------------------
// ProcessEntity — the heart of the AI FSM
// ---------------------------------------------------------------------------

void AISystem::ProcessEntity(EntityID entity, World& world,
                              TileMap& map, float dt)
{
    auto& ai = world.GetComponent<AIComponent>(entity);
    ai.stateTimer += dt;

    // Get current HP percentage for flee check.
    float hpPct = 1.0f;
    if (world.HasComponent<HealthComponent>(entity)) {
        const auto& hc = world.GetComponent<HealthComponent>(entity);
        if (hc.maxHp > 0)
            hpPct = static_cast<float>(hc.hp) / static_cast<float>(hc.maxHp);
    }

    // GLOBAL TRANSITION: flee if HP drops below threshold.
    if (hpPct < ai.fleeHPPct &&
        ai.currentState != AIComponent::State::FLEE &&
        ai.currentState != AIComponent::State::DEAD)
    {
        SetState(entity, AIState::FLEEING);
        return;
    }

    switch (ai.currentState) {

        // ── IDLE ──────────────────────────────────────────────────────────
        case AIComponent::State::IDLE:
            // After 2 seconds, start wandering.
            if (ai.stateTimer >= 2.0f) {
                SetState(entity, AIState::WANDERING);
            }
            // Check for player within sight range.
            if (ShouldAggro(entity, m_playerID, world)) {
                ai.aggroTarget = m_playerID;
                SetState(entity, AIState::CHASING);
            }
            break;

        // ── WANDERING ─────────────────────────────────────────────────────
        case AIComponent::State::PATROL:  // PATROL mapped to WANDERING logic
        {
            // Random wander every 1.5 seconds.
            if (ai.stateTimer >= 1.5f) {
                ai.stateTimer = 0.0f;

                // Pick a random direction.
                std::uniform_int_distribution<int> dir(0, 3);
                int d = dir(m_rng);
                const int dx[] = {1, -1, 0, 0};
                const int dy[] = {0, 0, 1, -1};

                if (world.HasComponent<TransformComponent>(entity)) {
                    auto& tc = world.GetComponent<TransformComponent>(entity);
                    int nx = static_cast<int>(tc.position.x / TILE_SIZE) + dx[d];
                    int ny = static_cast<int>(tc.position.z / TILE_SIZE) + dy[d];
                    if (map.IsPassable(nx, ny)) {
                        tc.position.x = static_cast<float>(nx) * TILE_SIZE;
                        tc.position.z = static_cast<float>(ny) * TILE_SIZE;
                        tc.isDirty    = true;
                    }
                }
            }

            if (ShouldAggro(entity, m_playerID, world)) {
                ai.aggroTarget = m_playerID;
                SetState(entity, AIState::CHASING);
            }
            break;
        }

        // ── CHASING ───────────────────────────────────────────────────────
        case AIComponent::State::CHASE:
        {
            if (!world.HasComponent<TransformComponent>(entity)) break;

            TileCoord playerTile = GetTilePos(m_playerID, world);
            float dist           = TileDistance(entity, m_playerID);

            // If adjacent, switch to attacking.
            if (dist <= 1.5f) {
                SetState(entity, AIState::ATTACKING);
                break;
            }

            // Move toward player every 0.4 seconds (movement rate).
            if (ai.stateTimer >= 0.4f) {
                ai.stateTimer = 0.0f;
                MoveToward(entity, map, playerTile);
            }

            // If player moved far away, return to idle.
            if (dist > ai.sightRange * 1.5f) {
                SetState(entity, AIState::IDLE);
            }
            break;
        }

        // ── ATTACKING ─────────────────────────────────────────────────────
        case AIComponent::State::ATTACK:
        {
            float dist = TileDistance(entity, m_playerID);

            // Attack every 1 second.
            if (ai.stateTimer >= 1.0f) {
                ai.stateTimer = 0.0f;

                if (dist <= 1.5f) {
                    ExecuteMeleeAttack(entity, m_playerID);
                } else {
                    // Player moved away — chase again.
                    SetState(entity, AIState::CHASING);
                }
            }
            break;
        }

        // ── FLEEING ───────────────────────────────────────────────────────
        case AIComponent::State::FLEE:
        {
            // Move AWAY from the player every 0.3 seconds.
            if (ai.stateTimer >= 0.3f) {
                ai.stateTimer = 0.0f;

                if (world.HasComponent<TransformComponent>(entity) &&
                    world.HasComponent<TransformComponent>(m_playerID))
                {
                    auto& tc   = world.GetComponent<TransformComponent>(entity);
                    auto& ptc  = world.GetComponent<TransformComponent>(m_playerID);

                    // Move in the opposite direction to the player.
                    float dx = tc.position.x - ptc.position.x;
                    float dz = tc.position.z - ptc.position.z;
                    float len = std::sqrt(dx*dx + dz*dz);
                    if (len > 0.01f) {
                        int nx = static_cast<int>((tc.position.x + (dx/len) * TILE_SIZE) / TILE_SIZE);
                        int ny = static_cast<int>((tc.position.z + (dz/len) * TILE_SIZE) / TILE_SIZE);
                        if (map.IsPassable(nx, ny)) {
                            tc.position.x = static_cast<float>(nx) * TILE_SIZE;
                            tc.position.z = static_cast<float>(ny) * TILE_SIZE;
                            tc.isDirty    = true;
                        }
                    }
                }
            }

            // If HP recovered above threshold (e.g. healed), return to idle.
            if (hpPct > ai.fleeHPPct * 1.5f) {
                SetState(entity, AIState::IDLE);
            }
            break;
        }

        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// FindPath — A* implementation
// ---------------------------------------------------------------------------

std::vector<TileCoord> AISystem::FindPath(const TileMap& map,
                                           int sx, int sy,
                                           int tx, int ty) const
{
    TileCoord start{sx, sy};
    TileCoord goal{tx, ty};

    if (start == goal) return {};

    // Open set: min-heap by f-cost.
    std::priority_queue<PathNode,
                        std::vector<PathNode>,
                        std::greater<PathNode>> openSet;

    // g-cost per node.
    std::unordered_map<uint64_t, float>       gCost;
    // Parent map for path reconstruction.
    std::unordered_map<uint64_t, TileCoord>   cameFrom;
    // Closed set: already-expanded nodes.
    std::unordered_set<uint64_t>              closedSet;

    gCost[EncodeCoord(start)] = 0.0f;
    openSet.push({start, Heuristic(start, goal)});

    int iterations = 0;
    constexpr int MAX_ITER = 200; // safety cap

    while (!openSet.empty() && iterations < MAX_ITER) {
        ++iterations;

        PathNode current = openSet.top();
        openSet.pop();

        if (current.coord == goal) {
            return ReconstructPath(cameFrom, goal);
        }

        uint64_t key = EncodeCoord(current.coord);
        if (closedSet.count(key)) continue;
        closedSet.insert(key);

        for (const TileCoord& nb : GetNeighbours(current.coord, map)) {
            uint64_t nbKey = EncodeCoord(nb);
            if (closedSet.count(nbKey)) continue;

            float tentativeG = gCost[key] + 1.0f; // uniform step cost

            auto it = gCost.find(nbKey);
            if (it == gCost.end() || tentativeG < it->second) {
                gCost[nbKey]    = tentativeG;
                cameFrom[nbKey] = current.coord;
                float f         = tentativeG + Heuristic(nb, goal);
                openSet.push({nb, f});
            }
        }
    }

    return {}; // no path found
}

// ---------------------------------------------------------------------------
// MoveToward
// ---------------------------------------------------------------------------

void AISystem::MoveToward(EntityID entity, TileMap& map, TileCoord target)
{
    TileCoord current = GetTilePos(entity, *m_world);
    auto path = FindPath(map, current.tileX, current.tileY,
                              target.tileX,  target.tileY);

    if (path.empty()) return;

    // Take the first step.
    TileCoord next = path[0];
    if (map.IsPassable(next.tileX, next.tileY)) {
        auto& tc = m_world->GetComponent<TransformComponent>(entity);
        tc.position.x = static_cast<float>(next.tileX) * TILE_SIZE;
        tc.position.z = static_cast<float>(next.tileY) * TILE_SIZE;
        tc.isDirty    = true;
    }
}

// ---------------------------------------------------------------------------
// ShouldAggro
// ---------------------------------------------------------------------------

bool AISystem::ShouldAggro(EntityID enemy, EntityID player, World& world) const
{
    if (player == NULL_ENTITY) return false;
    if (!world.HasComponent<HealthComponent>(player)) return false;

    // Don't aggro on a dead player.
    if (world.GetComponent<HealthComponent>(player).isDead) return false;

    const auto& ai = world.GetComponent<AIComponent>(enemy);
    // Already chasing or attacking — handled by existing state.
    if (ai.currentState == AIComponent::State::CHASE ||
        ai.currentState == AIComponent::State::ATTACK) return false;

    return TileDistance(enemy, player) <= ai.sightRange;
}

// ---------------------------------------------------------------------------
// SelectAbility
// ---------------------------------------------------------------------------

int AISystem::SelectAbility(EntityID enemy, World& world) const
{
    if (!world.HasComponent<CombatComponent>(enemy)) return -1;
    const auto& cc = world.GetComponent<CombatComponent>(enemy);

    // Check HP for heal-preference.
    float hpPct = 1.0f;
    if (world.HasComponent<HealthComponent>(enemy)) {
        const auto& hc = world.GetComponent<HealthComponent>(enemy);
        if (hc.maxHp > 0) hpPct = static_cast<float>(hc.hp) / hc.maxHp;
    }

    // For now: random ability selection weighted by HP.
    // A real implementation would use an ability priority system.
    std::uniform_int_distribution<int> dist(0, 3);
    return dist(m_rng) == 0 ? 0 : -1; // 25% chance of using first ability
}

// ---------------------------------------------------------------------------
// Private: Heuristic
// ---------------------------------------------------------------------------

float AISystem::Heuristic(TileCoord a, TileCoord b)
{
    // TEACHING NOTE — Manhattan distance is admissible for 4-directional grids.
    return static_cast<float>(std::abs(a.tileX - b.tileX) +
                               std::abs(a.tileY - b.tileY));
}

// ---------------------------------------------------------------------------
// Private: GetNeighbours
// ---------------------------------------------------------------------------

std::vector<TileCoord> AISystem::GetNeighbours(TileCoord node,
                                                 const TileMap& map)
{
    std::vector<TileCoord> result;
    // 4-directional movement (no diagonals to prevent wall-cutting).
    const int dx[] = {0,  0, 1, -1};
    const int dy[] = {1, -1, 0,  0};

    for (int i = 0; i < 4; ++i) {
        int nx = node.tileX + dx[i];
        int ny = node.tileY + dy[i];
        if (map.IsPassable(nx, ny)) {
            result.push_back({nx, ny});
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Private: ReconstructPath
// ---------------------------------------------------------------------------

std::vector<TileCoord> AISystem::ReconstructPath(
    const std::unordered_map<uint64_t, TileCoord>& cameFrom,
    TileCoord goal)
{
    std::vector<TileCoord> path;
    TileCoord current = goal;

    while (true) {
        path.push_back(current);
        auto it = cameFrom.find(EncodeCoord(current));
        if (it == cameFrom.end()) break;
        current = it->second;
    }

    std::reverse(path.begin(), path.end());
    // Remove the start node (index 0); we only want steps AFTER the start.
    if (!path.empty()) path.erase(path.begin());
    return path;
}

// ---------------------------------------------------------------------------
// Private: EncodeCoord
// ---------------------------------------------------------------------------

uint64_t AISystem::EncodeCoord(TileCoord c)
{
    return (static_cast<uint64_t>(static_cast<uint32_t>(c.tileX)) << 32) |
            static_cast<uint32_t>(c.tileY);
}

// ---------------------------------------------------------------------------
// Private: SetState
// ---------------------------------------------------------------------------

void AISystem::SetState(EntityID entity, AIState newState)
{
    if (!m_world->HasComponent<AIComponent>(entity)) return;
    auto& ai = m_world->GetComponent<AIComponent>(entity);

    // Map AIState → AIComponent::State.
    // TEACHING NOTE — We have two enums for the same concept because the
    // AISystem header defines its own AIState for readability, while
    // AIComponent::State is the stored value.  We bridge them here.
    switch (newState) {
        case AIState::IDLE:      ai.currentState = AIComponent::State::IDLE;   break;
        case AIState::WANDERING: ai.currentState = AIComponent::State::PATROL; break;
        case AIState::CHASING:   ai.currentState = AIComponent::State::CHASE;  break;
        case AIState::ATTACKING: ai.currentState = AIComponent::State::ATTACK; break;
        case AIState::FLEEING:   ai.currentState = AIComponent::State::FLEE;   break;
        case AIState::DEAD:      ai.currentState = AIComponent::State::DEAD;   break;
    }
    ai.stateTimer = 0.0f;
}

// ---------------------------------------------------------------------------
// Private: TileDistance
// ---------------------------------------------------------------------------

float AISystem::TileDistance(EntityID a, EntityID b) const
{
    if (!m_world->HasComponent<TransformComponent>(a) ||
        !m_world->HasComponent<TransformComponent>(b)) return 9999.0f;

    const auto& ta = m_world->GetComponent<TransformComponent>(a);
    const auto& tb = m_world->GetComponent<TransformComponent>(b);

    float dx = ta.position.x - tb.position.x;
    float dz = ta.position.z - tb.position.z;
    return std::sqrt(dx*dx + dz*dz) / TILE_SIZE;
}

// ---------------------------------------------------------------------------
// Private: ExecuteMeleeAttack
// ---------------------------------------------------------------------------

void AISystem::ExecuteMeleeAttack(EntityID enemy, EntityID player)
{
    if (!m_world->HasComponent<StatsComponent>(enemy)) return;
    if (!m_world->HasComponent<HealthComponent>(player)) return;

    const auto& stats   = m_world->GetComponent<StatsComponent>(enemy);
    auto&       playerHC = m_world->GetComponent<HealthComponent>(player);

    // Basic damage formula: attacker strength − defender defence.
    int defence = 0;
    if (m_world->HasComponent<StatsComponent>(player)) {
        defence = m_world->GetComponent<StatsComponent>(player).defence;
    }
    int damage = std::max(1, stats.strength - defence);

    playerHC.hp -= damage;
    if (playerHC.hp <= 0) {
        playerHC.hp     = 0;
        playerHC.isDead = true;
    }

    // Fire combat event.
    CombatEvent ev;
    ev.type     = CombatEvent::Type::DAMAGE_DEALT;
    ev.sourceID = enemy;
    ev.targetID = player;
    ev.amount   = -damage;
    ev.dmgType  = DamageType::PHYSICAL;
    EventBus<CombatEvent>::Instance().Publish(ev);

    LOG_DEBUG("AI entity " + std::to_string(enemy) +
              " attacked player for " + std::to_string(damage) + " damage.");
}
