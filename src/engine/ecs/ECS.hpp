/**
 * @file ECS.hpp
 * @brief Complete Entity Component System (ECS) implementation.
 *
 * ============================================================================
 * TEACHING NOTE — What Is an Entity Component System?
 * ============================================================================
 *
 * The ECS architecture is the dominant paradigm in modern game engines
 * (Unity's DOTS, Unreal's Mass, EnTT, Flecs, etc.).  Understanding it is
 * essential for any game developer.
 *
 * ─── The Problem with Traditional OOP ──────────────────────────────────────
 *
 * In a classical object-oriented game design you might write:
 *
 *   class Enemy : public Character { … };
 *   class FlyingEnemy : public Enemy { … };
 *   class FlyingBossEnemy : public FlyingEnemy { … };
 *
 * The inheritance hierarchy grows complex very quickly.  Adding a new
 * behaviour (e.g. "can set on fire") requires inserting it at exactly the
 * right level — or copy-pasting it across unrelated branches.  This is
 * sometimes called the "diamond problem" or "God class" anti-pattern.
 *
 * ─── ECS: Composition over Inheritance ────────────────────────────────────
 *
 * ECS replaces deep inheritance with three concepts:
 *
 *  ENTITY     — A unique ID (just an integer).  Has NO data or behaviour.
 *               Think of it as a row key in a database.
 *
 *  COMPONENT  — A plain data struct attached to an entity.
 *               Examples: HealthComponent, TransformComponent, RenderComponent.
 *               Components have NO behaviour (no methods beyond helpers).
 *
 *  SYSTEM     — A function or class that iterates entities which have a
 *               specific set of components and updates them.
 *               Examples: MovementSystem, CombatSystem, RenderSystem.
 *
 * An "Enemy" is just an entity with components:
 *   TransformComponent + HealthComponent + AIComponent + RenderComponent
 *
 * A "FlyingEnemy" is the same, plus MovementComponent with airborne=true.
 * A "FlyingBoss" additionally has a CombatComponent with boss abilities.
 *
 * No inheritance required!  Composition gives unlimited flexibility.
 *
 * ─── Why ECS is Cache-Friendly ─────────────────────────────────────────────
 *
 * Modern CPUs are much faster than RAM.  When iterating 10,000 enemies,
 * accessing components stored in a compact contiguous array (dense storage)
 * causes far fewer cache misses than following pointer chains to separate
 * heap-allocated objects.  ECS exploits this for significant performance gains.
 *
 * ─── This Implementation ──────────────────────────────────────────────────
 *
 *  ComponentPool<T>  — Dense contiguous array storing one component type T.
 *  EntityManager     — Creates/destroys entities; manages free ID list.
 *  SystemBase        — Abstract base class for all systems.
 *  World             — Owns EntityManager + all ComponentPools + all Systems.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 */

#pragma once

// ---------------------------------------------------------------------------
// Includes
// ---------------------------------------------------------------------------
#include "../core/Types.hpp"     // EntityID, ComponentID, NULL_ENTITY, etc.
#include "../core/Logger.hpp"    // LOG_DEBUG / LOG_WARN

#include <array>           // std::array — fixed-size storage.
#include <vector>          // std::vector — dynamic arrays.
#include <unordered_map>   // std::unordered_map — sparse entity→dense index map.
#include <bitset>          // std::bitset — component presence mask per entity.
#include <memory>          // std::unique_ptr — owning component pool pointers.
#include <cassert>         // assert() — debug precondition checks.
#include <typeindex>       // std::type_index — used for type-based component ID lookup.
#include <string>          // std::string — name components.
#include <optional>        // std::optional — nullable component references.
#include <queue>           // std::queue — recycled entity ID pool.
#include <unordered_set>   // std::unordered_set — alive entity set, system entity sets.
#include <tuple>           // std::tuple — used in View iteration.
#include <functional>      // std::function — system update callbacks.
#include <cstdint>         // Fixed-width integers.


// ===========================================================================
// Section 1 — ComponentTypeRegistry
// ===========================================================================

/**
 * @class ComponentTypeRegistry
 * @brief Assigns a globally unique integer ID to each component type at startup.
 *
 * TEACHING NOTE — Static Type IDs (Compile-time vs Runtime)
 * ──────────────────────────────────────────────────────────
 * We need a way to map "HealthComponent" → 0, "TransformComponent" → 1, etc.
 * so we can use the ID as an index into arrays.
 *
 * Approach: a static counter inside a templated function.  Each time
 * GetID<T>() is instantiated for a new type T, it captures the next
 * counter value.  Because `static` local variables are initialised only once,
 * the same type always gets the same ID within a single program execution.
 *
 * IMPORTANT: IDs are assigned in the order components are first registered
 * (first call to GetID<T>() for each type).  They are NOT stable across
 * runs unless registration order is deterministic (which it is if you call
 * World::RegisterComponent<T>() in a fixed order at startup).
 */
class ComponentTypeRegistry {
public:
    /**
     * @brief Return the unique ComponentID for component type T.
     *
     * The first call for a given T assigns the next available ID.
     * Subsequent calls return the same ID.
     *
     * @tparam T  A component struct type.
     * @return    A stable ComponentID for T within this process.
     */
    template<typename T>
    static ComponentID GetID() {
        // static local: constructed once per template instantiation.
        // TEACHING NOTE: Each GetID<T>() instantiation is a SEPARATE function,
        // so each T gets its own `static ComponentID id` variable.
        static ComponentID id = s_nextID++;
        return id;
    }

    /// Return the number of component types registered so far.
    static ComponentID GetCount() { return s_nextID; }

private:
    // Shared counter across all instantiations of GetID<T>().
    // TEACHING NOTE: `inline static` (C++17) initialises the variable in the
    // header without needing a separate .cpp definition.
    inline static ComponentID s_nextID = 0;
};


// ===========================================================================
// Section 2 — IComponentPool (abstract base)
// ===========================================================================

/**
 * @class IComponentPool
 * @brief Abstract interface for component pools (type-erased).
 *
 * TEACHING NOTE — Type Erasure
 * ──────────────────────────────
 * The World class needs to store pools for ALL component types in a single
 * container.  Since each pool has a different element type
 * (ComponentPool<HealthComponent>, ComponentPool<TransformComponent> …),
 * they cannot share a single std::vector without type erasure.
 *
 * Solution: all pools inherit from this abstract base class.
 * The World holds `std::unique_ptr<IComponentPool>`.
 * When it needs to call a type-specific method, it casts to
 * `ComponentPool<T>*` using static_cast (safe because we track which
 * pool index corresponds to which type).
 */
class IComponentPool {
public:
    virtual ~IComponentPool() = default;

    /**
     * @brief Remove the component for the given entity (if present).
     *
     * This virtual method is called by World::DestroyEntity() to clean up
     * all components without knowing their concrete types.
     */
    virtual void RemoveEntity(EntityID entity) = 0;

    /// Return the number of component instances stored in this pool.
    virtual std::size_t Size() const = 0;
};


// ===========================================================================
// Section 3 — ComponentPool<T>
// ===========================================================================

/**
 * @class ComponentPool
 * @brief Dense contiguous storage for one component type.
 *
 * @tparam T  The component struct type to store.
 *
 * TEACHING NOTE — Dense vs Sparse Storage
 * ─────────────────────────────────────────
 * A *sparse* approach: std::unordered_map<EntityID, T>
 *   Pro:  Simple.
 *   Con:  Random memory layout — cache-unfriendly for iteration.
 *         O(1) average access, but with significant constant factor.
 *
 * A *dense* approach (this implementation):
 *   - Keep components in a contiguous std::vector<T> (dense array).
 *   - Keep a sparse array: entityID → index into the dense array.
 *   - Keep a reverse map: dense index → entityID (for iterating).
 *
 * This is the "sparse-set" structure used by EnTT and similar libraries.
 *
 * Characteristics:
 *   • O(1) add / remove / get (with constant that involves one array lookup).
 *   • Iteration walks a tight cache line — CPU loves it.
 *   • Memory overhead: one extra sparse array and one reverse-map array.
 *
 * ─── Visual Example ─────────────────────────────────────────────────────────
 *
 *  Entity IDs in the world: 0, 1, 2, 3, 4 ...
 *  Suppose entities 1, 3, 4 have a HealthComponent.
 *
 *  m_sparse (indexed by EntityID):
 *    [_, 0, _, 1, 2, _, ...]    (_ = INVALID, not present)
 *
 *  m_dense (contiguous component array):
 *    [HP{entity=1, hp=100}, HP{entity=3, hp=50}, HP{entity=4, hp=200}]
 *
 *  m_entityDense (dense index → EntityID, for reverse lookup):
 *    [1, 3, 4]
 *
 *  Iterating ALL health components: just walk m_dense — perfect cache locality.
 *  Finding entity 3's health: m_sparse[3] = 1 → m_dense[1].
 *  Removing entity 3's health: swap-and-pop in m_dense, update m_sparse.
 */
template<typename T>
class ComponentPool : public IComponentPool {
public:
    // -----------------------------------------------------------------------
    // Constructor
    // -----------------------------------------------------------------------
    ComponentPool() {
        // Pre-allocate sparse array large enough for all potential entities.
        // We use std::numeric_limits<std::size_t>::max() as "not present".
        m_sparse.assign(MAX_ENTITIES, INVALID_INDEX);
    }

    // -----------------------------------------------------------------------
    // AddComponent
    // -----------------------------------------------------------------------

    /**
     * @brief Insert or overwrite the component for an entity.
     *
     * @param entity    The entity to attach the component to.
     * @param component The component data (copied or moved in).
     * @return Reference to the stored component.
     *
     * TEACHING NOTE — Perfect Forwarding with T&&
     * ──────────────────────────────────────────────
     * The parameter `T component` accepts both lvalues and rvalues.
     * `std::move(component)` transfers ownership into m_dense for move-only
     * types, or just copies for trivially-copyable types.  Using a value
     * parameter here is idiomatic for "sink" functions.
     */
    T& AddComponent(EntityID entity, T component = T{}) {
        assert(entity < MAX_ENTITIES && "Entity ID out of range");

        if (HasComponent(entity)) {
            // Overwrite existing component in-place.
            m_dense[m_sparse[entity]] = std::move(component);
            return m_dense[m_sparse[entity]];
        }

        // Append to dense array.
        std::size_t denseIndex = m_dense.size();
        m_dense.push_back(std::move(component));
        m_entityDense.push_back(entity);

        // Record the mapping in the sparse array.
        m_sparse[entity] = denseIndex;

        return m_dense[denseIndex];
    }

    // -----------------------------------------------------------------------
    // RemoveComponent
    // -----------------------------------------------------------------------

    /**
     * @brief Remove the component for an entity.
     *
     * TEACHING NOTE — Swap-and-Pop
     * ──────────────────────────────
     * Erasing from the middle of a std::vector shifts all subsequent
     * elements and invalidates indices — O(n) and expensive.
     *
     * Swap-and-pop is a classic O(1) trick:
     *   1. Swap the element to remove with the last element.
     *   2. Pop the last element (O(1)).
     *   3. Update the sparse index of the element that was swapped in.
     *
     * This keeps the dense array compact and fast to iterate, at the cost
     * of changing the order of elements (order is NOT preserved).
     */
    void RemoveComponent(EntityID entity) {
        if (!HasComponent(entity)) {
            LOG_WARN("RemoveComponent: entity " << entity << " has no component of this type");
            return;
        }

        std::size_t removedIndex = m_sparse[entity];
        std::size_t lastIndex    = m_dense.size() - 1;

        if (removedIndex != lastIndex) {
            // Swap removed element with the last element.
            std::swap(m_dense[removedIndex],      m_dense[lastIndex]);
            std::swap(m_entityDense[removedIndex], m_entityDense[lastIndex]);
            // Update the sparse entry of the element that was moved.
            EntityID movedEntity = m_entityDense[removedIndex];
            m_sparse[movedEntity] = removedIndex;
        }

        // Pop the last element.
        m_dense.pop_back();
        m_entityDense.pop_back();

        // Mark entity as no longer having this component.
        m_sparse[entity] = INVALID_INDEX;
    }

    /// IComponentPool virtual override — calls RemoveComponent.
    void RemoveEntity(EntityID entity) override {
        if (HasComponent(entity)) {
            RemoveComponent(entity);
        }
    }

    // -----------------------------------------------------------------------
    // GetComponent
    // -----------------------------------------------------------------------

    /**
     * @brief Return a reference to the component for the given entity.
     *
     * @param entity  Must have this component (checked by assert in debug).
     * @return Mutable reference to the component data.
     *
     * TEACHING NOTE — assert vs exception
     * ──────────────────────────────────────
     * `assert()` is a debug-only check (disabled when NDEBUG is defined).
     * Use assert for "this should never happen if code is correct" violations.
     * Use exceptions for "this might happen at runtime due to external input."
     * Forgetting to add a component before getting it is a programmer error,
     * so assert is appropriate here.
     */
    T& GetComponent(EntityID entity) {
        assert(HasComponent(entity) && "Entity does not have this component");
        return m_dense[m_sparse[entity]];
    }

    /// Const version — read-only access.
    const T& GetComponent(EntityID entity) const {
        assert(HasComponent(entity) && "Entity does not have this component");
        return m_dense[m_sparse[entity]];
    }

    /**
     * @brief Return an optional reference, or std::nullopt if not present.
     *
     * TEACHING NOTE — std::optional
     * ───────────────────────────────
     * std::optional<T&> cannot be used (references cannot be optional in the
     * standard).  We use std::optional<T*> as a nullable pointer.  The caller
     * must check `if (result)` before dereferencing.
     *
     * This is safer than returning nullptr because `if (optPtr)` is explicit,
     * while a raw pointer can be silently used without a null check.
     */
    T* TryGetComponent(EntityID entity) {
        if (!HasComponent(entity)) return nullptr;
        return &m_dense[m_sparse[entity]];
    }

    const T* TryGetComponent(EntityID entity) const {
        if (!HasComponent(entity)) return nullptr;
        return &m_dense[m_sparse[entity]];
    }

    // -----------------------------------------------------------------------
    // HasComponent
    // -----------------------------------------------------------------------

    /// Return true if the entity has a component in this pool.
    bool HasComponent(EntityID entity) const {
        return entity < MAX_ENTITIES && m_sparse[entity] != INVALID_INDEX;
    }

    // -----------------------------------------------------------------------
    // Iteration
    // -----------------------------------------------------------------------

    /**
     * @brief Return a pointer to the first component in the dense array.
     *
     * TEACHING NOTE — Range-based for with raw pointers
     * ───────────────────────────────────────────────────
     * begin() / end() returning pointers allows range-based for:
     *
     *   for (auto& hp : hpPool) { hp.current -= poison; }
     *
     * This is valid because std::vector guarantees contiguous storage and
     * data() returns a pointer to the first element.
     */
    T*       begin()       { return m_dense.data(); }
    T*       end()         { return m_dense.data() + m_dense.size(); }
    const T* begin() const { return m_dense.data(); }
    const T* end()   const { return m_dense.data() + m_dense.size(); }

    /// Return pointer to the raw dense array.
    T* Data() { return m_dense.data(); }

    /// Return the EntityID of the component at dense index i.
    EntityID GetEntityAtIndex(std::size_t i) const {
        assert(i < m_entityDense.size());
        return m_entityDense[i];
    }

    /// IComponentPool override — number of components stored.
    std::size_t Size() const override { return m_dense.size(); }

private:
    // -----------------------------------------------------------------------
    // Storage
    // -----------------------------------------------------------------------

    /// Dense contiguous array of component values — hot for iteration.
    std::vector<T> m_dense;

    /// Parallel array: dense index → EntityID (reverse map).
    std::vector<EntityID> m_entityDense;

    /**
     * @brief Sparse array: EntityID → index into m_dense.
     *
     * Sized MAX_ENTITIES, indexed directly by EntityID.
     * INVALID_INDEX means the entity does NOT have this component.
     *
     * TEACHING NOTE — Trade-off: this uses MAX_ENTITIES * sizeof(size_t)
     * bytes even if only a few entities have this component.  For 64k
     * entities and 8-byte size_t that is 512 KB per component type.
     * Real ECS engines use more memory-efficient structures for sparse
     * components, but this is clear and educational.
     */
    std::vector<std::size_t> m_sparse;

    static constexpr std::size_t INVALID_INDEX = std::numeric_limits<std::size_t>::max();
};


// ===========================================================================
// Section 4 — EntityManager
// ===========================================================================

/**
 * @class EntityManager
 * @brief Creates, destroys, and recycles EntityIDs.
 *
 * TEACHING NOTE — Entity Lifecycle
 * ─────────────────────────────────
 * Entities are just IDs.  The EntityManager is responsible for:
 *   1. Handing out fresh IDs when entities are created.
 *   2. Recycling IDs when entities are destroyed so the ID space doesn't
 *      exhaust.  (With 65536 slots, recycling is critical for long sessions.)
 *   3. Tracking which IDs are currently alive (the "living set").
 *   4. Tracking the component presence mask for each entity.
 *
 * ─── Component Signature / Bitmask ──────────────────────────────────────────
 * Each entity carries a std::bitset<MAX_COMPONENTS> called its "signature".
 * Bit i is set if the entity currently has the component with ComponentID i.
 *
 * Systems use a signature-based check to quickly determine whether an entity
 * "belongs" to their update loop:
 *
 *   systemSignature & entitySignature == systemSignature
 *
 * This is an O(MAX_COMPONENTS/64) bitwise AND — extremely fast.
 */
class EntityManager {
public:
    using Signature = std::bitset<MAX_COMPONENTS>;

    EntityManager() {
        // Pre-fill the free list with all possible entity IDs.
        // TEACHING NOTE: We push in reverse order so entity 0 is at the
        // front of the queue (first to be handed out).
        for (EntityID id = static_cast<EntityID>(MAX_ENTITIES - 1);
             id != NULL_ENTITY; --id)
        {
            m_freeIDs.push(id);
        }
        // Edge case: push ID 0 explicitly because the loop stops before it.
        m_freeIDs.push(0);
    }

    // -----------------------------------------------------------------------
    // CreateEntity
    // -----------------------------------------------------------------------

    /**
     * @brief Allocate and return a new EntityID.
     *
     * @return A fresh EntityID, or NULL_ENTITY if the pool is exhausted.
     *
     * TEACHING NOTE — Free List
     * ──────────────────────────
     * A "free list" (here, a std::queue) holds IDs that are available for
     * reuse.  When an entity is created we pop from the front; when one is
     * destroyed we push to the back.  This is O(1) and avoids fragmentation.
     */
    EntityID CreateEntity() {
        if (m_freeIDs.empty()) {
            LOG_ERROR("EntityManager: entity pool exhausted (MAX_ENTITIES = "
                      << MAX_ENTITIES << ")");
            return NULL_ENTITY;
        }

        EntityID id = m_freeIDs.front();
        m_freeIDs.pop();
        m_livingEntities.insert(id);
        m_signatures[id].reset();   // Clear all component bits.
        ++m_livingCount;

        LOG_DEBUG("Entity created: ID=" << id
                  << " (alive=" << m_livingCount << ")");
        return id;
    }

    // -----------------------------------------------------------------------
    // DestroyEntity
    // -----------------------------------------------------------------------

    /**
     * @brief Mark an entity as dead and recycle its ID.
     *
     * NOTE: This does NOT remove the entity's components from component pools.
     * The World::DestroyEntity() method is responsible for that.
     *
     * @param entity  EntityID to destroy.  Must be alive.
     */
    void DestroyEntity(EntityID entity) {
        if (!IsAlive(entity)) {
            LOG_WARN("DestroyEntity: entity " << entity << " is not alive");
            return;
        }

        m_signatures[entity].reset();
        m_livingEntities.erase(entity);
        m_freeIDs.push(entity);  // Recycle the ID.
        --m_livingCount;

        LOG_DEBUG("Entity destroyed: ID=" << entity
                  << " (alive=" << m_livingCount << ")");
    }

    // -----------------------------------------------------------------------
    // Component signature management
    // -----------------------------------------------------------------------

    /// Set bit `componentID` in the entity's signature (component added).
    void SetSignatureBit(EntityID entity, ComponentID componentID, bool value) {
        assert(IsAlive(entity));
        m_signatures[entity].set(componentID, value);
    }

    /// Return the full component signature (bitmask) for an entity.
    const Signature& GetSignature(EntityID entity) const {
        assert(entity < MAX_ENTITIES);
        return m_signatures[entity];
    }

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------

    /// True if the entity ID is currently alive (not recycled).
    bool IsAlive(EntityID entity) const {
        return entity < MAX_ENTITIES && m_livingEntities.count(entity) > 0;
    }

    /// Return the number of currently living entities.
    uint32_t GetLivingCount() const { return m_livingCount; }

    /**
     * @brief Fill a vector with all living entity IDs.
     *
     * More efficient than GetAllEntities() for iteration because vectors
     * are cache-friendly.
     */
    void GetLivingEntities(std::vector<EntityID>& out) const {
        out.clear();
        out.reserve(m_livingCount);
        for (EntityID id : m_livingEntities) {
            out.push_back(id);
        }
    }

private:
    // -----------------------------------------------------------------------
    // Storage
    // -----------------------------------------------------------------------

    std::queue<EntityID>                    m_freeIDs;       ///< Recycled / available IDs.
    std::unordered_set<EntityID>            m_livingEntities;///< Currently alive IDs.
    std::array<Signature, MAX_ENTITIES>     m_signatures;    ///< Per-entity component bitmask.
    uint32_t                                m_livingCount = 0;///< Quick count.
};


// ===========================================================================
// Section 5 — SystemBase
// ===========================================================================

/**
 * @class SystemBase
 * @brief Abstract base class for all game systems.
 *
 * TEACHING NOTE — Systems in ECS
 * ────────────────────────────────
 * A System encapsulates behaviour that operates on a specific combination
 * of components.  Key design rules:
 *
 *  1. Systems do NOT store data — components store data.
 *  2. Each system declares a "required signature": the set of components
 *     an entity MUST have for the system to process it.
 *  3. Each system maintains a cached set of matching entities, updated
 *     whenever components are added or removed.
 *
 * Example — MovementSystem:
 *   Required: TransformComponent + MovementComponent.
 *   Update(): for each matching entity, apply velocity to position.
 *
 * Example — CombatSystem:
 *   Required: TransformComponent + CombatComponent + HealthComponent.
 *   Update(): process attack inputs, apply damage to nearby enemies.
 *
 * By caching the entity set, systems avoid re-scanning all entities every
 * frame.  The World updates the cache when components change.
 */
class SystemBase {
public:
    virtual ~SystemBase() = default;

    /**
     * @brief Called once per frame (or at fixed timestep) to update logic.
     * @param deltaTime  Seconds since the last update call.
     */
    virtual void Update(float deltaTime) = 0;

    /**
     * @brief Optional init hook called after the system is registered.
     *
     * Use this to subscribe to EventBus channels, pre-load assets, etc.
     */
    virtual void Init() {}

    /**
     * @brief Optional teardown hook called before the system is removed.
     */
    virtual void Shutdown() {}

    // -----------------------------------------------------------------------
    // Signature / entity cache management (called by World)
    // -----------------------------------------------------------------------

    using Signature = std::bitset<MAX_COMPONENTS>;

    /// Set which components this system requires.
    void SetRequiredSignature(const Signature& sig) { m_required = sig; }

    /// Return the required component signature.
    const Signature& GetRequiredSignature() const { return m_required; }

    /**
     * @brief Notify the system that an entity's component set changed.
     *
     * Called by World whenever AddComponent / RemoveComponent / DestroyEntity
     * is called.  The system adds or removes the entity from m_entities.
     *
     * @param entity     The entity that changed.
     * @param signature  The entity's NEW full signature after the change.
     */
    void OnEntitySignatureChanged(EntityID entity, const Signature& signature) {
        if ((signature & m_required) == m_required) {
            // Entity now qualifies — add to set (idempotent if already there).
            m_entities.insert(entity);
        } else {
            // Entity no longer qualifies — remove.
            m_entities.erase(entity);
        }
    }

    /// Access the set of entities this system should process.
    const std::unordered_set<EntityID>& GetEntities() const { return m_entities; }

protected:
    /**
     * @brief The set of entities whose signatures match m_required.
     *
     * TEACHING NOTE — Why std::unordered_set?
     * ──────────────────────────────────────────
     * We need fast insert, erase, and membership check (O(1) average for all
     * three).  std::unordered_set provides this via hash-based lookup.
     *
     * In a production engine you might use a sorted std::vector and binary
     * search, or a dedicated bitset of alive entities, for better cache
     * behaviour during the actual Update() iteration.  For readability,
     * unordered_set is clearest.
     */
    std::unordered_set<EntityID> m_entities;
    Signature                    m_required; ///< Required component bitmask.
};


// ===========================================================================
// Section 6 — Component Struct Definitions (the game's vocabulary types)
// ===========================================================================

/**
 * @defgroup Components ECS Component Structs
 * @{
 *
 * TEACHING NOTE — Component Design Guidelines
 * ─────────────────────────────────────────────
 *  1. Components are PLAIN DATA — prefer simple structs, no virtual methods.
 *  2. Keep components small and focused.  A "GodComponent" holding 50 fields
 *     defeats the purpose of ECS.
 *  3. Choose field types carefully:
 *     - Use float for continuous values (position, velocity, time).
 *     - Use int32_t / uint32_t for discrete values (HP, currency, count).
 *     - Use bool for toggles (isGrounded, isVisible).
 *     - Use enum class for state machines (face direction, AI state).
 *  4. Initialise all fields to safe defaults in the struct definition.
 */

// ---------------------------------------------------------------------------
// TransformComponent — position, rotation, scale
// ---------------------------------------------------------------------------

/**
 * @struct TransformComponent
 * @brief Stores world-space position, rotation, and scale for an entity.
 *
 * The transform is the most fundamental component.  Almost every visible
 * entity (character, item, prop) needs a TransformComponent.
 *
 * TEACHING NOTE — Right-hand coordinate system:
 *   +X = right, +Y = up, +Z = toward viewer (out of screen in top-down).
 */
struct TransformComponent {
    Vec3  position    = Vec3::Zero();  ///< World-space position.
    Vec3  rotation    = Vec3::Zero();  ///< Euler angles in degrees (pitch, yaw, roll).
    Vec3  scale       = {1,1,1};      ///< Scale factor per axis (1 = no scaling).
    Vec3  velocity    = Vec3::Zero();  ///< Current movement velocity (units/sec).
    Vec3  lastPosition= Vec3::Zero(); ///< Position on the previous frame (for interpolation).

    /// True if the transform changed this frame (dirty flag for renderer).
    bool  isDirty = true;

    /// Forward direction vector derived from yaw rotation.
    Vec3 Forward() const {
        float yaw = rotation.y * 3.14159265f / 180.0f;
        return { std::sin(yaw), 0.0f, std::cos(yaw) };
    }

    /// Move the entity by the given offset.
    void Translate(const Vec3& delta) {
        lastPosition = position;
        position     += delta;
        isDirty       = true;
    }
};

// ---------------------------------------------------------------------------
// HealthComponent — HP / MP management
// ---------------------------------------------------------------------------

/**
 * @struct HealthComponent
 * @brief Tracks current and maximum HP for an entity.
 *
 * Also stores a "downed" threshold: in FF15-style games a character can be
 * DOWNED (at 0 HP) and be revived, or DEAD (timer expired while downed).
 */
struct HealthComponent {
    // ── Short-form field names ─────────────────────────────────────────────
    // TEACHING NOTE — Naming Conventions
    // We use short names (`hp`, `mp`) instead of `currentHP`/`currentMP` so
    // game-system code stays readable:
    //   hc.hp -= damage;   ← concise and clear
    //   hc.currentHP -= damage;  ← verbose for a very common operation
    int32_t hp          = 100;  ///< Current hit points.
    int32_t maxHp       = 100;  ///< Maximum hit points.
    int32_t mp          = 50;   ///< Current magic points.
    int32_t maxMp       = 50;   ///< Maximum magic points.
    bool    isDowned    = false; ///< True when HP == 0 but can still be revived.
    bool    isDead      = false; ///< True when revive window expired.
    float   downedTimer = 0.0f; ///< Seconds remaining before downed → dead.
    float   regenRate   = 0.0f; ///< Passive HP regen per second (out-of-combat).
    float   mpRegenRate = 2.0f; ///< Passive MP regen per second.

    /// Return HP as a [0,1] fraction for health bars.
    float HPFraction() const {
        return (maxHp > 0) ? static_cast<float>(hp) / maxHp : 0.0f;
    }

    /// Return MP as a [0,1] fraction.
    float MPFraction() const {
        return (maxMp > 0) ? static_cast<float>(mp) / maxMp : 0.0f;
    }

    /// Heal HP by amount, capped at maxHp.
    void Heal(int32_t amount) {
        hp = std::min(hp + amount, maxHp);
        if (hp > 0) { isDowned = false; }
    }

    /// Drain MP by cost; returns false if insufficient MP.
    bool SpendMP(int32_t cost) {
        if (mp < cost) return false;
        mp -= cost;
        return true;
    }
};

// ---------------------------------------------------------------------------
// StatsComponent — primary combat statistics
// ---------------------------------------------------------------------------

/**
 * @struct StatsComponent
 * @brief Base combat statistics used in damage formulas.
 *
 * Inspired by FF series stat structures.  Final damage is roughly:
 *   Physical damage = Strength * weapon_power - target.Defence
 *   Magic damage    = Magic * spell_power    - target.Spirit
 */
struct StatsComponent {
    int32_t strength    = 10; ///< Physical attack power.
    int32_t defence     = 5;  ///< Physical damage reduction.
    int32_t magic       = 10; ///< Magic attack power.
    int32_t spirit      = 5;  ///< Magic damage reduction.
    int32_t speed       = 10; ///< Determines turn order, flee chance, and dodge.
    int32_t luck        = 5;  ///< Affects critical hit rate and item drops.
    int32_t vitality    = 10; ///< Determines bonus HP per level.

    // Elemental resistances — percentage modifier (100 = normal, 50 = absorb half,
    // 150 = take 50% extra damage, 0 = immune).
    int32_t fireResist      = 100;
    int32_t iceResist       = 100;
    int32_t lightningResist = 100;

    /// Critical hit rate (0–100 percent).
    int32_t critRate = 5;

    /// Critical hit damage multiplier (e.g. 200 = double damage on crit).
    int32_t critMultiplier = 200;
};

// ---------------------------------------------------------------------------
// NameComponent — human-readable label
// ---------------------------------------------------------------------------

/**
 * @struct NameComponent
 * @brief Stores the display name of an entity.
 *
 * Used by the UI, dialogue system, and quest log to identify entities.
 * Kept as a separate component so entities without a displayed name
 * (invisible triggers, particle emitters) don't pay for string storage.
 */
struct NameComponent {
    // TEACHING NOTE — Simple string `name` is used throughout game systems.
    // The `internalID` is a separate, stable machine-readable identifier for
    // save files and scripting, so that renaming the display name in the game
    // doesn't break existing save data.
    std::string name;        ///< Shown in the HUD and menus (e.g. "Noctis").
    std::string internalID;  ///< Scripting / save system identifier (e.g. "noctis_lvl1").
    std::string title;       ///< Optional title shown in dialogue (e.g. "Crown Prince").
};

// ---------------------------------------------------------------------------
// RenderComponent — visual representation
// ---------------------------------------------------------------------------

/**
 * @struct RenderComponent
 * @brief Describes how an entity should be drawn.
 *
 * For a 2D sprite engine this holds the sprite sheet asset path and the
 * source rectangle within the sheet.  For a 3D engine it would reference
 * a mesh and material.  We keep it simple and sprite-based for this engine.
 */
struct RenderComponent {
    std::string spriteSheet;    ///< Asset path to the sprite sheet texture.
    Rect        sourceRect;     ///< Which region of the sprite sheet to draw.
    Color       tint = Color::White(); ///< Colour multiplied with the sprite.
    float       zOrder    = 0.0f;  ///< Draw order (higher = drawn on top).
    bool        isVisible = true;  ///< Set false to hide without removing component.
    bool        flipX     = false; ///< Mirror horizontally (face left vs right).
    bool        flipY     = false; ///< Mirror vertically.
    float       opacity   = 1.0f; ///< Alpha (0 = invisible, 1 = fully opaque).

    // ── Terminal (ncurses) rendering ───────────────────────────────────────
    // TEACHING NOTE — Dual-Mode Rendering
    // The engine supports both a sprite-based renderer and an ncurses terminal
    // renderer.  The fields below are used by the terminal renderer only.
    // A 'symbol' is the ASCII character drawn at the entity's tile position,
    // and 'colorPair' is an ncurses color-pair index (1–8 typically).
    char    symbol    = '@';  ///< ASCII character for terminal rendering.
    int     colorPair = 1;    ///< ncurses color-pair index for terminal rendering.

    /// Current animation frame (index into a frames array managed elsewhere).
    uint32_t currentFrame = 0;

    /// Shadow blob to draw under entity (simple depth cue).
    bool      castsShadow = true;
};

// ---------------------------------------------------------------------------
// MovementComponent — locomotion parameters
// ---------------------------------------------------------------------------

/**
 * @struct MovementComponent
 * @brief Controls how an entity moves through the world.
 */
struct MovementComponent {
    float   moveSpeed       = 5.0f;   ///< Base movement speed (world units/sec).
    float   sprintSpeed     = 10.0f;  ///< Speed while sprinting.
    float   jumpForce       = 8.0f;   ///< Initial vertical velocity on jump.
    float   gravityScale    = 1.0f;   ///< Local gravity multiplier.
    bool    isGrounded      = true;   ///< True when standing on a surface.
    bool    isSprinting     = false;  ///< True while sprint button is held.
    bool    isJumping       = false;  ///< True during a jump arc.
    bool    canFly          = false;  ///< True for flying enemies / vehicle modes.
    bool    canSwim         = false;  ///< True for aquatic entities.
    Direction facingDir     = Direction::SOUTH; ///< Which way the entity faces.
    float   dashCooldown    = 0.0f;   ///< Seconds until next dash is allowed.
    float   dashSpeed       = 20.0f;  ///< Speed during a dash.
    float   dashDuration    = 0.15f;  ///< How long a single dash lasts (seconds).
};

// ---------------------------------------------------------------------------
// CombatComponent — combat state and abilities
// ---------------------------------------------------------------------------

/**
 * @struct CombatComponent
 * @brief Tracks combat-specific state for an entity.
 *
 * Both player characters and enemies have CombatComponents.
 * The CombatSystem reads these to determine attack timing, cooldowns,
 * and which abilities are available.
 */
struct CombatComponent {
    bool    isInCombat    = false;  ///< True when engaged in battle.
    bool    isAttacking   = false;  ///< True during attack animation.
    float   attackCooldown= 0.0f;   ///< Seconds until next basic attack.
    float   attackRate    = 1.0f;   ///< Basic attacks per second.
    int32_t attackRange   = 2;      ///< Melee range in world units.
    int32_t xpReward      = 0;      ///< XP awarded to the player for defeating this entity.
    int32_t gilReward     = 0;      ///< Gil awarded to the player for defeating this entity.

    // Warp-strike (Noctis signature move): throw weapon to distant enemy,
    // warp to it, deal bonus damage.
    bool    canWarpStrike = false;
    float   warpCooldown  = 0.0f;
    int32_t warpRange     = 20;

    // Parry / block
    bool    isGuarding    = false;
    float   guardAngle    = 180.0f; ///< Degrees in front that are protected.
    float   parryWindow   = 0.1f;   ///< Seconds of perfect parry window.

    // Target
    EntityID currentTarget= NULL_ENTITY; ///< Currently locked-on enemy.

    ElementType attackElement = ElementType::NONE; ///< Current weapon element.
};

// ---------------------------------------------------------------------------
// InventoryComponent — item storage
// ---------------------------------------------------------------------------

/**
 * @struct ItemStack
 * @brief A single stack of items in an inventory slot.
 *
 * TEACHING NOTE — Item IDs vs Item Objects
 * ──────────────────────────────────────────
 * Storing item *IDs* (integers) rather than full item objects keeps
 * InventoryComponent compact and decoupled.  A separate ItemDatabase
 * maps IDs to full item data (name, description, stats, sprite).
 * This is the "flyweight" design pattern.
 */
struct ItemStack {
    uint32_t itemID    = 0;  ///< ID into the global item database.
    uint32_t quantity  = 0;  ///< How many of this item are stacked.

    bool IsEmpty() const { return quantity == 0; }
};

/**
 * @struct InventoryComponent
 * @brief Stores the items carried by an entity.
 *
 * TEACHING NOTE — Separation of Currency and Inventory
 * ──────────────────────────────────────────────────────
 * Currency (Gil) is intentionally stored in CurrencyComponent, NOT here.
 *
 * Why keep them separate?
 *   1. Single Source of Truth — having Gil in two components created a
 *      desync bug: buying an item would deduct from CurrencyComponent but
 *      InventoryComponent::gil could drift to a different value.
 *   2. Conceptual clarity — "what items do I carry?" is a different question
 *      from "how much money do I have?".  Separate components make the
 *      distinction explicit and teachable.
 *   3. Not all entities carry items AND currency.  A chest entity might have
 *      an InventoryComponent (loot) but no CurrencyComponent.
 *
 * Authoritative Gil balance: world.GetComponent<CurrencyComponent>(entity).gil
 */
struct InventoryComponent {
    std::array<ItemStack, MAX_INV_SLOTS> slots{};  ///< All item slots.

    /// Return the index of the first empty slot, or MAX_INV_SLOTS if full.
    uint32_t FindEmptySlot() const {
        for (uint32_t i = 0; i < MAX_INV_SLOTS; ++i) {
            if (slots[i].IsEmpty()) return i;
        }
        return MAX_INV_SLOTS;
    }

    /// Return the index of the slot holding itemID, or MAX_INV_SLOTS if absent.
    uint32_t FindItem(uint32_t itemID) const {
        for (uint32_t i = 0; i < MAX_INV_SLOTS; ++i) {
            if (slots[i].itemID == itemID && !slots[i].IsEmpty()) return i;
        }
        return MAX_INV_SLOTS;
    }

    /// True if the inventory has at least 'qty' of itemID.
    bool HasItem(uint32_t itemID, uint32_t qty = 1) const {
        uint32_t idx = FindItem(itemID);
        return idx < MAX_INV_SLOTS && slots[idx].quantity >= qty;
    }
};

// ---------------------------------------------------------------------------
// QuestComponent — quest tracking for NPCs / players
// ---------------------------------------------------------------------------

/**
 * @struct QuestEntry
 * @brief Tracks progress on one quest objective.
 */
struct QuestEntry {
    uint32_t questID    = 0;
    uint32_t objective  = 0;       ///< Which step of the quest.
    int32_t  progress   = 0;       ///< Current counter.
    int32_t  required   = 1;       ///< Counter needed to complete objective.
    bool     isComplete = false;
    bool     isFailed   = false;
};

/**
 * @struct QuestComponent
 * @brief Stores all active quests for a player entity.
 */
struct QuestComponent {
    std::array<QuestEntry, MAX_QUESTS> quests{};
    uint32_t activeCount = 0;  ///< Number of quests in use.

    /// Return a pointer to the QuestEntry for questID, or nullptr.
    QuestEntry* FindQuest(uint32_t questID) {
        for (uint32_t i = 0; i < activeCount; ++i) {
            if (quests[i].questID == questID) return &quests[i];
        }
        return nullptr;
    }
};

// ---------------------------------------------------------------------------
// DialogueComponent — NPC conversation data
// ---------------------------------------------------------------------------

/**
 * @struct DialogueComponent
 * @brief Enables an NPC to start a conversation with the player.
 *
 * The dialogue system reads dialogueTreeID to look up the full conversation
 * tree in a JSON or binary dialogue database.  The engine only stores the
 * trigger data here.
 */
struct DialogueComponent {
    std::string dialogueTreeID;    ///< Key into the dialogue asset database.
    float       interactRange = 3.0f; ///< How close the player must be to talk.
    bool        isInteractable = true; ///< False during combat or cutscenes.
    bool        hasNewDialogue = false;///< Show "!" indicator above NPC head.
    uint32_t    currentNodeID  = 0;   ///< Which dialogue node is currently shown.
    std::string portraitAsset;        ///< Sprite for the dialogue portrait.
};

// ---------------------------------------------------------------------------
// AIComponent — enemy / NPC behaviour
// ---------------------------------------------------------------------------

/**
 * @struct AIComponent
 * @brief State machine data for AI-controlled entities.
 *
 * TEACHING NOTE — Finite State Machine (FSM) in AI
 * ──────────────────────────────────────────────────
 * Even complex enemy AI is typically implemented as a Finite State Machine:
 * a set of states and transitions between them.
 *
 * Enemy states:
 *   IDLE    → PATROL  (after standing idle for N seconds)
 *   PATROL  → ALERT   (player detected within sightRange)
 *   ALERT   → CHASE   (player confirmed enemy)
 *   CHASE   → ATTACK  (player within attackRange)
 *   ATTACK  → CHASE   (player out of attackRange)
 *   CHASE   → PATROL  (player escaped or combat ended)
 *   ANY     → FLEE    (HP < 20%)
 */
struct AIComponent {
    enum class State : uint8_t {
        IDLE,     ///< Standing still, no target.
        PATROL,   ///< Following a waypoint path.
        ALERT,    ///< Suspicious; looking for player.
        CHASE,    ///< Moving toward the player.
        ATTACK,   ///< In attack range; using abilities.
        FLEE,     ///< Running away (low HP).
        STUNNED,  ///< Cannot act (status effect).
        DEAD      ///< Death animation playing.
    };

    State    currentState  = State::IDLE;
    float    sightRange    = 10.0f;  ///< Distance at which enemy spots player.
    float    hearRange     = 6.0f;   ///< Distance at which enemy hears footsteps.
    float    attackRange   = 2.0f;   ///< Distance to trigger Attack state.
    float    fleeHPPct     = 0.15f;  ///< HP fraction threshold for fleeing.
    float    stateTimer    = 0.0f;   ///< Seconds in current state.
    float    alertLevel    = 0.0f;   ///< 0 = calm, 1 = max alert.
    EntityID aggroTarget   = NULL_ENTITY; ///< Primary target entity.

    // Patrol waypoints
    std::vector<Vec3> waypoints;
    uint32_t          currentWaypoint = 0;
    bool              patrolLoop      = true;

    // Reaction to time of day (nocturnal enemies are stronger at night)
    bool isNocturnal = false; ///< Gains +50% power at NIGHT / MIDNIGHT.

    TimeOfDay activeTimeStart = TimeOfDay::DAWN;
    TimeOfDay activeTimeEnd   = TimeOfDay::NIGHT;
};

// ---------------------------------------------------------------------------
// PartyComponent — group membership
// ---------------------------------------------------------------------------

/**
 * @struct PartyComponent
 * @brief Stores party membership for player-controlled characters.
 *
 * In FF15, up to 4 party members travel together.  The PartyComponent on
 * the player entity tracks the full party roster.
 */
struct PartyComponent {
    std::array<EntityID, MAX_PARTY_SIZE> members{};
    uint32_t memberCount = 0;          ///< Number of active party members.
    EntityID leaderID    = NULL_ENTITY; ///< Active player-controlled leader.
    uint32_t formationID = 0;           ///< Which battle formation to use.

    bool IsFull()    const { return memberCount >= MAX_PARTY_SIZE; }
    bool IsEmpty()   const { return memberCount == 0; }

    bool HasMember(EntityID e) const {
        for (uint32_t i = 0; i < memberCount; ++i)
            if (members[i] == e) return true;
        return false;
    }

    void AddMember(EntityID e) {
        if (!IsFull() && !HasMember(e))
            members[memberCount++] = e;
    }

    void RemoveMember(EntityID e) {
        for (uint32_t i = 0; i < memberCount; ++i) {
            if (members[i] == e) {
                // Shift remaining members left.
                for (uint32_t j = i; j < memberCount - 1; ++j)
                    members[j] = members[j + 1];
                members[--memberCount] = NULL_ENTITY;
                break;
            }
        }
    }
};

// ---------------------------------------------------------------------------
// MagicComponent — spell repertoire and casting state
// ---------------------------------------------------------------------------

/**
 * @struct MagicComponent
 * @brief Tracks magic spells available and the current casting state.
 *
 * In the FF15 engine, magic is crafted from elemancy flasks.  We simplify
 * this to a list of available spell IDs and a current-cast state.
 */
struct MagicComponent {
    std::vector<uint32_t> knownSpells;      ///< IDs of spells this entity can cast.
    uint32_t    equippedSpell = 0;           ///< Currently selected spell.
    bool        isCasting     = false;       ///< True during a cast animation.
    float       castTimer     = 0.0f;        ///< Seconds remaining in cast.
    float       globalCooldown = 0.0f;       ///< GCD after any spell cast.
    int32_t     spellPower    = 10;          ///< Base magic damage bonus.
    ElementType activeElement = ElementType::NONE; ///< Current spell element.

    // Elemancy flask quantities (crafting resource)
    int32_t fireFlasks      = 0;
    int32_t iceFlasks       = 0;
    int32_t lightningFlasks = 0;
};

// ---------------------------------------------------------------------------
// EquipmentComponent — worn/wielded items
// ---------------------------------------------------------------------------

/**
 * @struct EquipmentComponent
 * @brief Tracks what an entity has equipped.
 *
 * Equipment slots mirror typical JRPG equipment:
 *   weapon, offhand (shield/accessory), head, body, legs, accessory × 2.
 */
struct EquipmentComponent {
    uint32_t weaponID    = 0; ///< 0 = unarmed / no weapon.
    uint32_t offhandID   = 0;
    uint32_t headID      = 0;
    uint32_t bodyID      = 0;
    uint32_t legsID      = 0;
    uint32_t accessory1  = 0;
    uint32_t accessory2  = 0;

    // Cached stat bonuses from all equipped items (recomputed on equip change).
    int32_t bonusStrength   = 0;
    int32_t bonusDefence    = 0;
    int32_t bonusMagic      = 0;
    int32_t bonusHP         = 0;
    int32_t bonusMP         = 0;

    /// True if equipment stats need to be recomputed (dirty flag).
    bool statsDirty = true;
};

// ---------------------------------------------------------------------------
// StatusEffectsComponent — active debuffs / buffs
// ---------------------------------------------------------------------------

/**
 * @struct ActiveStatusEntry
 * @brief One active status effect with its remaining duration.
 */
struct ActiveStatusEntry {
    StatusEffect effect    = StatusEffect::NONE;
    float        remaining = 0.0f; ///< Seconds until this effect expires.
    float        tickTimer = 0.0f; ///< Seconds until next DoT tick (for POISON).
    float        tickDamage= 5.0f; ///< Damage per DoT tick.
    EntityID     sourceID  = NULL_ENTITY; ///< Who applied this effect.
};

/**
 * @struct StatusEffectsComponent
 * @brief Stores all currently active status effects on an entity.
 */
struct StatusEffectsComponent {
    static constexpr uint32_t MAX_STATUS = 8;
    std::array<ActiveStatusEntry, MAX_STATUS> active{};
    uint32_t    count    = 0;
    StatusEffect bitmask = StatusEffect::NONE; ///< Quick presence check.

    /// Add an effect; replaces existing entry if the same effect is already active.
    void Apply(StatusEffect effect, float duration,
               float tickDmg = 0.0f, EntityID src = NULL_ENTITY) {
        // Look for existing entry to refresh duration.
        for (uint32_t i = 0; i < count; ++i) {
            if (active[i].effect == effect) {
                active[i].remaining  = std::max(active[i].remaining, duration);
                active[i].tickDamage = tickDmg;
                return;
            }
        }
        if (count < MAX_STATUS) {
            active[count++] = { effect, duration, 0.0f, tickDmg, src };
            bitmask |= effect;
        }
    }

    /// Remove all entries for a given effect.
    void Remove(StatusEffect effect) {
        for (uint32_t i = 0; i < count; ) {
            if (active[i].effect == effect) {
                active[i] = active[--count];
                active[count] = {};
            } else {
                ++i;
            }
        }
        // Recompute bitmask.
        bitmask = StatusEffect::NONE;
        for (uint32_t i = 0; i < count; ++i) bitmask |= active[i].effect;
    }

    bool Has(StatusEffect effect) const {
        return HasStatus(bitmask, effect);
    }
};

// ---------------------------------------------------------------------------
// LevelComponent — XP and level progression
// ---------------------------------------------------------------------------

/**
 * @struct LevelComponent
 * @brief Tracks character level, XP, and pending stat increases.
 *
 * TEACHING NOTE — Level-up Design
 * ────────────────────────────────
 * In FF15, characters level up only when resting at camp.  XP is accumulated
 * during combat and exploration, then "banked" to actual levels at camp.
 * We model this with pendingXP (accumulated) vs currentXP (cashed in).
 */
struct LevelComponent {
    uint32_t level       = 1;
    uint32_t currentXP   = 0;      ///< XP already applied to the current level.
    uint32_t pendingXP   = 0;      ///< XP earned in the field, not yet levelled.
    uint32_t xpToNextLevel = XP_PER_LEVEL;
    bool     pendingLevelUp = false; ///< True when resting will trigger a level-up.

    /// Grant XP and flag if a level-up is pending.
    void GainXP(uint32_t amount) {
        pendingXP += amount;
        if (pendingXP + currentXP >= xpToNextLevel && level < MAX_LEVEL) {
            pendingLevelUp = true;
        }
    }

    /// Apply banked XP (called at camp).
    void ApplyBankedXP() {
        currentXP += pendingXP;
        pendingXP  = 0;
        while (currentXP >= xpToNextLevel && level < MAX_LEVEL) {
            currentXP   -= xpToNextLevel;
            ++level;
            // Scale XP requirement: each level needs slightly more XP.
            xpToNextLevel = XP_PER_LEVEL + level * 100;
        }
        pendingLevelUp = false;
    }
};

// ---------------------------------------------------------------------------
// CurrencyComponent — various currencies
// ---------------------------------------------------------------------------

/**
 * @struct CurrencyComponent
 * @brief Stores the various currencies a player entity holds.
 *
 * In FF15 the main currency is Gil.  We add a bonus "Crown Tokens" as an
 * example of a secondary currency for special shops.
 */
struct CurrencyComponent {
    uint64_t gil          = 0;   ///< Main currency (earned from battles, quests).
    uint32_t crownTokens  = 0;   ///< Rare currency from boss drops / achievements.

    /// Attempt to spend gil.  Returns false if insufficient funds.
    bool SpendGil(uint64_t amount) {
        if (gil < amount) return false;
        gil -= amount;
        return true;
    }

    void EarnGil(uint64_t amount) { gil += amount; }
};

// ---------------------------------------------------------------------------
// SkillsComponent — active/passive abilities
// ---------------------------------------------------------------------------

/**
 * @struct SkillEntry
 * @brief One skill with its unlock status and current cooldown.
 */
struct SkillEntry {
    uint32_t skillID   = 0;
    bool     isUnlocked= false;
    bool     isEquipped= false;   ///< Placed in an active skill slot.
    float    cooldown  = 0.0f;    ///< Seconds until skill can be used again.
    float    maxCooldown = 10.0f; ///< Base cooldown duration.
    uint32_t apCost    = 0;       ///< Ability Points to unlock.
};

/**
 * @struct SkillsComponent
 * @brief Manages skill learning and cooldowns for a character.
 */
struct SkillsComponent {
    static constexpr uint32_t MAX_SKILLS = 32;
    std::array<SkillEntry, MAX_SKILLS> skills{};
    uint32_t skillCount = 0;
    uint32_t abilityPoints = 0; ///< Available points to spend on unlocking skills.

    // Active skill slots (buttons 1–4 in combat)
    std::array<uint32_t, 4> equippedSkills = {0, 0, 0, 0};
};

// ---------------------------------------------------------------------------
// CampComponent — camp/rest state for party lead
// ---------------------------------------------------------------------------

/**
 * @struct CampComponent
 * @brief Tracks camp site state and meal buffs.
 *
 * In FF15, camping applies overnight meal buffs that persist until the next
 * camp.  We store which meal is active and its stat bonuses.
 */
struct CampComponent {
    bool     isCamping        = false;
    uint32_t currentMealID    = 0;      ///< 0 = no meal active.
    float    mealDuration     = 0.0f;   ///< Seconds of meal buff remaining.
    int32_t  mealStrBonus     = 0;
    int32_t  mealMagBonus     = 0;
    int32_t  mealDefBonus     = 0;
    int32_t  mealHPBonus      = 0;
    bool     pendingLevelUp   = false;  ///< Mirrors LevelComponent flag for UI.
    uint32_t campSiteID       = 0;      ///< Which campsite is being used.
    float    campRestTimer    = 8.0f;   ///< Seconds of rest animation.

    /// Return true if a meal buff is currently active.
    bool HasMealBuff() const { return currentMealID != 0 && mealDuration > 0.0f; }
};

// ---------------------------------------------------------------------------
// VehicleComponent — Regalia car / other vehicle state
// ---------------------------------------------------------------------------

/**
 * @struct VehicleComponent
 * @brief Tracks vehicle state for the Regalia or other driveable objects.
 *
 * In FF15 the Regalia is the party's car.  This component enables vehicle
 * physics, fast-travel, and the fuel system.
 */
struct VehicleComponent {
    enum class VehicleType : uint8_t {
        REGALIA,       ///< The royal car (Regalia Type-F = flying version).
        CHOCOBO,       ///< Rideable bird — off-road exploration.
        BOAT,          ///< Aquatic vehicle.
        AIRSHIP        ///< Full airship (late-game).
    };

    VehicleType type        = VehicleType::REGALIA;
    bool        isOccupied  = false;   ///< True when party is inside.
    float       speed       = 0.0f;    ///< Current speed (units/sec).
    float       maxSpeed    = 50.0f;   ///< Maximum speed.
    float       acceleration= 5.0f;   ///< Speed increase per second.
    float       fuel        = 100.0f;  ///< Current fuel (0 = stalled).
    float       maxFuel     = 100.0f;
    float       fuelConsumption = 0.1f; ///< Fuel per second at max speed.
    bool        canFly      = false;   ///< True for Type-F Regalia.
    float       altitude    = 0.0f;   ///< Current height when flying.
    bool        autoMode    = false;   ///< AI drives on the main road.
    bool        needsFuel   = false;   ///< True when fuel == 0.

    /// Return fuel as a [0,1] fraction.
    float FuelFraction() const {
        return (maxFuel > 0.0f) ? fuel / maxFuel : 0.0f;
    }
};

// ---------------------------------------------------------------------------
// AudioSourceComponent — spatial / non-spatial audio emitter (M3)
// ---------------------------------------------------------------------------

/**
 * @struct AudioSourceComponent
 * @brief Marks an entity as an audio emitter.
 *
 * ============================================================================
 * TEACHING NOTE — Data-Driven Audio via ECS
 * ============================================================================
 * Instead of hard-coding sound triggers in gameplay code, we attach an
 * AudioSourceComponent to any entity that should make noise.  The
 * AudioSystem (src/engine/audio/audio_system.hpp) iterates all entities that
 * have this component every frame and forwards play/stop requests to the
 * XAudio2Backend.
 *
 * Fields mirror the design from docs/FF15_REQUIREMENTS_BLUEPRINT.md §8:
 *
 *   clipID       — GUID of the cooked .wav asset in the AssetDB.
 *   is3D         — Enable position-based volume attenuation.
 *                  2D (false) = same volume everywhere (UI SFX, music).
 *                  3D (true)  = attenuated by distance to listener.
 *   volume       — Master volume scalar [0.0 = silent, 1.0 = full].
 *   isLooping    — When true the source voice loops until stopped.
 *   isPlaying    — Set true to start playback; AudioSystem clears it when done.
 *   maxDistance  — World-units at which 3D audio reaches zero volume.
 *
 * ─── Usage Example ──────────────────────────────────────────────────────────
 *
 *   // Attach footstep sound to the player entity.
 *   auto& audio  = world.AddComponent<AudioSourceComponent>(player);
 *   audio.clipID = "a3f2-footstep-stone";   // AssetDB GUID
 *   audio.is3D   = false;
 *   audio.volume = 0.8f;
 *   audio.isPlaying = true;
 *
 * ============================================================================
 */
struct AudioSourceComponent {
    // -----------------------------------------------------------------------
    // Asset reference
    // -----------------------------------------------------------------------

    /// AssetDB GUID of the cooked .wav file to play.
    /// Empty string = no clip loaded.
    std::string clipID;

    // -----------------------------------------------------------------------
    // Playback control
    // -----------------------------------------------------------------------

    /// Set true to begin playback.  AudioSystem clears it once the source
    /// voice is submitted.  Toggle back on to retrigger.
    bool  isPlaying   = false;

    /// When true the voice loops indefinitely.  Stop by clearing isPlaying.
    bool  isLooping   = false;

    // -----------------------------------------------------------------------
    // Volume / spatialisation
    // -----------------------------------------------------------------------

    /// Master volume scalar (0.0 = silent, 1.0 = unity gain, >1.0 = boosted).
    float volume      = 1.0f;

    /// True = emitter is 3-D; XAudio2 applies distance-based attenuation.
    /// False = 2-D (background music, UI sounds, etc.).
    bool  is3D        = false;

    /// Distance at which 3-D audio reaches zero volume (world units).
    float maxDistance = 50.0f;

    // -----------------------------------------------------------------------
    // Internal state (written by AudioSystem — do not modify manually)
    // -----------------------------------------------------------------------

    /// Index of the source voice in XAudio2Backend's pool (-1 = not playing).
    int   voiceIndex  = -1;
};

/** @} */ // end of Components


// ===========================================================================
// Section 7 — World (the ECS facade / context)
// ===========================================================================

/**
 * @class World
 * @brief The central ECS context: owns EntityManager, ComponentPools, Systems.
 *
 * TEACHING NOTE — Facade Pattern
 * ────────────────────────────────
 * The World class is a *facade*: it provides a simple unified API over the
 * more complex subsystems (EntityManager, individual ComponentPools, Systems).
 * Users of the ECS call World methods and never interact with the internals
 * directly.
 *
 * Ownership model:
 *   World owns everything.
 *   EntityManager lives as a direct member.
 *   ComponentPools live as unique_ptr<IComponentPool> in m_pools[].
 *   Systems live as unique_ptr<SystemBase> in m_systems[].
 *
 * Lifetime: one World per "level" / "scene".  Destroying the World destroys
 * all entities, components, and systems cleanly.
 */
class World {
public:
    World()  { LOG_INFO("ECS World created"); }
    ~World() { LOG_INFO("ECS World destroyed"); }

    // Non-copyable — only one authoritative World per scene.
    World(const World&)            = delete;
    World& operator=(const World&) = delete;

    // -----------------------------------------------------------------------
    // Component Registration
    // -----------------------------------------------------------------------

    /**
     * @brief Register a component type with the World.
     *
     * MUST be called once per component type before using Add/Remove/Get.
     * Typically called in an "InitComponents()" function at startup.
     *
     * @tparam T  The component struct type to register.
     *
     * TEACHING NOTE: This allocates a ComponentPool<T> and stores it at the
     * index equal to ComponentTypeRegistry::GetID<T>().  The first call to
     * GetID<T>() assigns the ID, ensuring consistency.
     */
    template<typename T>
    void RegisterComponent() {
        ComponentID id = ComponentTypeRegistry::GetID<T>();
        assert(id < MAX_COMPONENTS && "Too many component types registered");
        assert(m_pools[id] == nullptr && "Component type already registered");
        m_pools[id] = std::make_unique<ComponentPool<T>>();
        LOG_DEBUG("Registered component type ID=" << id
                  << " (total registered: " << ComponentTypeRegistry::GetCount() << ")");
    }

    // -----------------------------------------------------------------------
    // Entity creation / destruction
    // -----------------------------------------------------------------------

    /**
     * @brief Create a new entity and return its ID.
     * @return Fresh EntityID, or NULL_ENTITY if pool exhausted.
     */
    EntityID CreateEntity() {
        return m_entityManager.CreateEntity();
    }

    /**
     * @brief Destroy an entity and remove ALL its components.
     *
     * Iterates all registered pools and removes the entity from each.
     * Then notifies all systems so they remove it from their entity sets.
     *
     * @param entity  EntityID to destroy.
     */
    void DestroyEntity(EntityID entity) {
        if (!m_entityManager.IsAlive(entity)) {
            LOG_WARN("DestroyEntity called on dead entity " << entity);
            return;
        }

        // Remove from all component pools.
        for (auto& pool : m_pools) {
            if (pool) pool->RemoveEntity(entity);
        }

        // Notify systems — they will remove entity from their entity sets.
        EntityManager::Signature empty;
        for (auto& system : m_systems) {
            if (system) system->OnEntitySignatureChanged(entity, empty);
        }

        m_entityManager.DestroyEntity(entity);
    }

    // -----------------------------------------------------------------------
    // AddComponent
    // -----------------------------------------------------------------------

    /**
     * @brief Attach a component to an entity.
     *
     * @tparam T        Component type (must be registered first).
     * @param  entity   Target entity.
     * @param  comp     Component data (copied/moved into the pool).
     * @return Reference to the stored component.
     *
     * After adding, the entity's signature is updated and all systems are
     * notified to re-evaluate whether the entity joins their update set.
     */
    template<typename T>
    T& AddComponent(EntityID entity, T comp = T{}) {
        assert(m_entityManager.IsAlive(entity) && "Cannot add component to dead entity");

        ComponentID id = ComponentTypeRegistry::GetID<T>();
        assert(m_pools[id] != nullptr && "Component type not registered");

        T& stored = GetPool<T>()->AddComponent(entity, std::move(comp));

        // Update entity signature and notify systems.
        m_entityManager.SetSignatureBit(entity, id, true);
        NotifySystems(entity);

        return stored;
    }

    // -----------------------------------------------------------------------
    // RemoveComponent
    // -----------------------------------------------------------------------

    /**
     * @brief Detach a component from an entity.
     *
     * @tparam T  Component type to remove.
     * @param entity  Entity to remove component from.
     */
    template<typename T>
    void RemoveComponent(EntityID entity) {
        assert(m_entityManager.IsAlive(entity));

        ComponentID id = ComponentTypeRegistry::GetID<T>();
        assert(m_pools[id] != nullptr && "Component type not registered");

        GetPool<T>()->RemoveComponent(entity);

        m_entityManager.SetSignatureBit(entity, id, false);
        NotifySystems(entity);
    }

    // -----------------------------------------------------------------------
    // GetComponent
    // -----------------------------------------------------------------------

    /**
     * @brief Return a mutable reference to a component (assert if absent).
     * @tparam T  Component type.
     * @param entity  Target entity.
     */
    template<typename T>
    T& GetComponent(EntityID entity) {
        ComponentID id = ComponentTypeRegistry::GetID<T>();
        assert(m_pools[id] != nullptr && "Component type not registered");
        return GetPool<T>()->GetComponent(entity);
    }

    template<typename T>
    const T& GetComponent(EntityID entity) const {
        ComponentID id = ComponentTypeRegistry::GetID<T>();
        assert(m_pools[id] != nullptr && "Component type not registered");
        return GetPool<T>()->GetComponent(entity);
    }

    // -----------------------------------------------------------------------
    // TryGetComponent — nullable access (no assert)
    // -----------------------------------------------------------------------

    /**
     * @brief Return a pointer to the component, or nullptr if absent.
     * @tparam T  Component type.
     * @param entity  Target entity.
     */
    template<typename T>
    T* TryGetComponent(EntityID entity) {
        ComponentID id = ComponentTypeRegistry::GetID<T>();
        if (!m_pools[id]) return nullptr;
        return GetPool<T>()->TryGetComponent(entity);
    }

    template<typename T>
    const T* TryGetComponent(EntityID entity) const {
        ComponentID id = ComponentTypeRegistry::GetID<T>();
        if (!m_pools[id]) return nullptr;
        return GetPool<T>()->TryGetComponent(entity);
    }

    // -----------------------------------------------------------------------
    // HasComponent
    // -----------------------------------------------------------------------

    /// Return true if the entity has component T.
    template<typename T>
    bool HasComponent(EntityID entity) const {
        ComponentID id = ComponentTypeRegistry::GetID<T>();
        if (!m_pools[id]) return false;
        return GetPool<T>()->HasComponent(entity);
    }

    // -----------------------------------------------------------------------
    // System registration
    // -----------------------------------------------------------------------

    /**
     * @brief Register and initialise a system with a required component signature.
     *
     * @tparam SystemT   Concrete system class (must inherit SystemBase).
     * @tparam Components  Variadic list of component types the system requires.
     * @return Reference to the stored system.
     *
     * TEACHING NOTE — Variadic Templates
     * ────────────────────────────────────
     * `template<typename... Components>` accepts any number of type arguments.
     * We use a fold expression `(sig.set(ComponentTypeRegistry::GetID<C>()) , ...)`
     * to set one bit per component type — expanded at compile time.
     *
     * Fold expressions were introduced in C++17:
     *   (expr , ...) applies the operator , (comma/sequencing) to each
     *   expansion of the pack.
     */
    template<typename SystemT, typename... Components>
    SystemT& RegisterSystem() {
        static_assert(std::is_base_of_v<SystemBase, SystemT>,
                      "SystemT must inherit from SystemBase");

        auto system = std::make_unique<SystemT>();
        SystemT& ref = *system;

        // Build the required signature from the component pack.
        EntityManager::Signature sig;
        // C++17 fold expression — sets one bit per component type.
        (sig.set(ComponentTypeRegistry::GetID<Components>()), ...);
        system->SetRequiredSignature(sig);

        // Add existing live entities that match this system's signature.
        std::vector<EntityID> living;
        m_entityManager.GetLivingEntities(living);
        for (EntityID id : living) {
            system->OnEntitySignatureChanged(id, m_entityManager.GetSignature(id));
        }

        system->Init();

        m_systems.push_back(std::move(system));
        LOG_INFO("Registered system " << typeid(SystemT).name());
        return ref;
    }

    // -----------------------------------------------------------------------
    // Update — call all systems
    // -----------------------------------------------------------------------

    /**
     * @brief Advance all registered systems by deltaTime.
     *
     * @param deltaTime  Seconds since the last frame.
     *
     * TEACHING NOTE — Update Order
     * ──────────────────────────────
     * Systems are updated in the order they were registered.  Order matters:
     *   • InputSystem  → read player input first.
     *   • MovementSystem → apply input to transform.
     *   • CombatSystem → process attacks.
     *   • AISystem → enemy decisions.
     *   • PhysicsSystem → resolve collisions.
     *   • RenderSystem → draw results last.
     */
    void Update(float deltaTime) {
        for (auto& system : m_systems) {
            if (system) system->Update(deltaTime);
        }
    }

    // -----------------------------------------------------------------------
    // View — iterate entities with specific components
    // -----------------------------------------------------------------------

    /**
     * @brief Iterate all entities that have ALL of the specified components.
     *
     * @tparam Components  One or more component types to require.
     * @param  callback    Function called for each matching entity.
     *                     Signature: void(EntityID, Components&...)
     *
     * TEACHING NOTE — View Pattern / Query
     * ──────────────────────────────────────
     * A "view" is an on-demand filter over living entities.  It avoids
     * maintaining a cached set (unlike System).  Good for rare or
     * one-off queries.
     *
     * Example usage:
     * @code
     *   world.View<TransformComponent, HealthComponent>(
     *       [](EntityID id, TransformComponent& t, HealthComponent& h) {
     *           if (h.isDowned) t.velocity = Vec3::Zero();
     *       });
     * @endcode
     *
     * TEACHING NOTE — `if constexpr` and Fold Expressions
     * ──────────────────────────────────────────────────────
     * The implementation uses parameter pack expansion to call HasComponent<C>
     * for each type C in Components.  The `&&` fold collapses all checks into
     * a single boolean.
     */
    template<typename... Components, typename Callback>
    void View(Callback&& callback) {
        std::vector<EntityID> living;
        m_entityManager.GetLivingEntities(living);

        for (EntityID entity : living) {
            // Check that entity has ALL required components.
            // C++17 fold with && — short-circuits on first false.
            bool hasAll = (... && HasComponent<Components>(entity));
            if (hasAll) {
                callback(entity, GetComponent<Components>(entity)...);
            }
        }
    }

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------

    /// Return the number of living entities.
    uint32_t GetEntityCount() const {
        return m_entityManager.GetLivingCount();
    }

    /// True if entity is alive.
    bool IsAlive(EntityID entity) const {
        return m_entityManager.IsAlive(entity);
    }

    /// Return reference to the EntityManager (advanced use).
    EntityManager& GetEntityManager() { return m_entityManager; }

    // -----------------------------------------------------------------------
    // Utility — create an entity pre-loaded with common components
    // -----------------------------------------------------------------------

    /**
     * @brief Create a fully-featured "character" entity.
     *
     * Adds the standard set of components that any living character needs:
     * Transform, Health, Stats, Name, Render, Movement, Combat,
     * StatusEffects, Level.
     *
     * @param name       Display name for the character.
     * @param position   Starting world position.
     * @return EntityID of the new character.
     *
     * TEACHING NOTE — Factory Methods
     * ─────────────────────────────────
     * Rather than calling AddComponent 10 times at every call site, a factory
     * method bundles common configurations.  The caller can still customise
     * individual components afterwards.
     *
     *   EntityID noctis = world.CreateCharacter("Noctis", {0,0,0});
     *   world.GetComponent<StatsComponent>(noctis).strength = 50;
     */
    EntityID CreateCharacter(const std::string& name, const Vec3& position) {
        EntityID id = CreateEntity();
        if (id == NULL_ENTITY) return NULL_ENTITY;

        // Transform
        auto& t = AddComponent<TransformComponent>(id);
        t.position = position;

        // Health
        AddComponent<HealthComponent>(id);

        // Stats
        AddComponent<StatsComponent>(id);

        // Name
        auto& n = AddComponent<NameComponent>(id);
        n.name = name;

        // Render
        AddComponent<RenderComponent>(id);

        // Movement
        AddComponent<MovementComponent>(id);

        // Combat
        AddComponent<CombatComponent>(id);

        // Status effects (empty by default)
        AddComponent<StatusEffectsComponent>(id);

        // Level (start at 1)
        AddComponent<LevelComponent>(id);

        LOG_INFO("Created character '" << name << "' as entity " << id);
        return id;
    }

    /**
     * @brief Create an enemy entity with AI.
     *
     * @param name         Display name.
     * @param position     Spawn position.
     * @param nocturnal    True if the enemy is stronger/active at night.
     * @return EntityID of the new enemy.
     */
    EntityID CreateEnemy(const std::string& name,
                         const Vec3& position,
                         bool nocturnal = false)
    {
        EntityID id = CreateCharacter(name, position);
        if (id == NULL_ENTITY) return NULL_ENTITY;

        // Add AI
        auto& ai      = AddComponent<AIComponent>(id);
        ai.isNocturnal = nocturnal;

        // Set the default XP reward in the CombatComponent.
        // Zone::SpawnOneEnemy() will overwrite this with the species-specific
        // value from EnemyData::xpReward.
        auto& combat = GetComponent<CombatComponent>(id);
        combat.xpReward = 50;  // Default XP reward (overridden by Zone on spawn).

        LOG_INFO("Created enemy '" << name << "' as entity " << id
                 << " (nocturnal=" << nocturnal << ")");
        return id;
    }

private:
    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Return a typed pointer to the pool for component T.
     *
     * TEACHING NOTE — static_cast vs dynamic_cast
     * ─────────────────────────────────────────────
     * dynamic_cast performs a runtime type check (RTTI) and returns nullptr
     * if the types don't match.  It is safe but has a small runtime cost.
     *
     * static_cast performs NO runtime check — it trusts the programmer that
     * the types are correct.  We use it here because we KNOW the pool at
     * index GetID<T>() was created by make_unique<ComponentPool<T>>() in
     * RegisterComponent<T>() — the invariant is maintained by design.
     *
     * Using static_cast in carefully controlled internal code like this is
     * fine; using it on user-supplied pointers would be dangerous.
     */
    template<typename T>
    ComponentPool<T>* GetPool() {
        ComponentID id = ComponentTypeRegistry::GetID<T>();
        assert(m_pools[id] != nullptr && "Component pool not registered");
        return static_cast<ComponentPool<T>*>(m_pools[id].get());
    }

    template<typename T>
    const ComponentPool<T>* GetPool() const {
        ComponentID id = ComponentTypeRegistry::GetID<T>();
        assert(m_pools[id] != nullptr && "Component pool not registered");
        return static_cast<const ComponentPool<T>*>(m_pools[id].get());
    }

    /**
     * @brief Tell all systems that entity's signature may have changed.
     *
     * Called after every AddComponent / RemoveComponent to keep system
     * entity sets up to date.
     *
     * @param entity  Entity whose signature just changed.
     */
    void NotifySystems(EntityID entity) {
        const auto& sig = m_entityManager.GetSignature(entity);
        for (auto& system : m_systems) {
            if (system) system->OnEntitySignatureChanged(entity, sig);
        }
    }

    // -----------------------------------------------------------------------
    // Member data
    // -----------------------------------------------------------------------

    EntityManager m_entityManager; ///< Manages entity IDs and signatures.

    /**
     * @brief One component pool slot per component type.
     *
     * Indexed by ComponentID (assigned by ComponentTypeRegistry).
     * Null until RegisterComponent<T>() is called for that type.
     *
     * std::unique_ptr provides automatic cleanup and type erasure via
     * IComponentPool.
     */
    std::array<std::unique_ptr<IComponentPool>, MAX_COMPONENTS> m_pools{};

    /**
     * @brief All registered systems, in registration order.
     *
     * Systems are updated in this order every frame.
     */
    std::vector<std::unique_ptr<SystemBase>> m_systems;
};


// ===========================================================================
// Section 8 — World setup helper: RegisterAllComponents
// ===========================================================================

/**
 * @brief Register all 20 component types with the given World.
 *
 * Call this once after constructing a World, before creating any entities.
 *
 * TEACHING NOTE — Why a free function?
 * ───────────────────────────────────────
 * Putting registration in a free function keeps the World constructor clean
 * and makes it easy to add new components without modifying World itself.
 * This follows the Open-Closed Principle: open for extension, closed for
 * modification.
 *
 * @param world  The World to register components with.
 */
inline void RegisterAllComponents(World& world) {
    world.RegisterComponent<TransformComponent>();
    world.RegisterComponent<HealthComponent>();
    world.RegisterComponent<StatsComponent>();
    world.RegisterComponent<NameComponent>();
    world.RegisterComponent<RenderComponent>();
    world.RegisterComponent<MovementComponent>();
    world.RegisterComponent<CombatComponent>();
    world.RegisterComponent<InventoryComponent>();
    world.RegisterComponent<QuestComponent>();
    world.RegisterComponent<DialogueComponent>();
    world.RegisterComponent<AIComponent>();
    world.RegisterComponent<PartyComponent>();
    world.RegisterComponent<MagicComponent>();
    world.RegisterComponent<EquipmentComponent>();
    world.RegisterComponent<StatusEffectsComponent>();
    world.RegisterComponent<LevelComponent>();
    world.RegisterComponent<CurrencyComponent>();
    world.RegisterComponent<SkillsComponent>();
    world.RegisterComponent<CampComponent>();
    world.RegisterComponent<VehicleComponent>();
    world.RegisterComponent<AudioSourceComponent>();

    LOG_INFO("All 21 component types registered with World");
}
