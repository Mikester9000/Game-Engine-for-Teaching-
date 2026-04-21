/**
 * @file behaviour_tree.hpp
 * @brief Behaviour Tree (BT) framework for NPC and enemy AI.
 *
 * ============================================================================
 * TEACHING NOTE — What is a Behaviour Tree?
 * ============================================================================
 *
 * A Behaviour Tree (BT) is a hierarchical AI control structure used in
 * production games (Halo: Combat Evolved, The Last of Us, Unreal Engine,
 * Unity).  It answers the question: "What should this character do RIGHT NOW?"
 *
 * Compared to the Finite State Machine (FSM) in AISystem.cpp, a BT is:
 *
 *   ✓ MODULAR  — behaviour is expressed as a tree of independent nodes that
 *                can be recombined without touching other nodes.
 *   ✓ READABLE — the tree structure mirrors a natural English decision:
 *                "Try to attack; if you can't, try to flee; if you can't, wander."
 *   ✓ SCALABLE — adding complex conditions / actions is additive, not
 *                multiplicative (no state-transition matrix explosion).
 *   ✗ OVERHEAD — a tree traversal every frame is slightly more expensive than
 *                a direct switch() statement.  For 10 000 enemies you would
 *                add a "dirty flag" to skip the tick when inputs have not changed.
 *
 * ─── Node Types ─────────────────────────────────────────────────────────────
 *
 *   LEAF nodes (no children):
 *     BtCondition — test a predicate; returns SUCCESS or FAILURE instantly.
 *     BtAction    — execute a task; may return RUNNING across multiple frames.
 *
 *   COMPOSITE nodes (one or more children):
 *     BtSequence  — "AND gate": tick children left to right; return SUCCESS
 *                   only if ALL succeed; short-circuit on first FAILURE.
 *     BtSelector  — "OR gate": tick children left to right; return SUCCESS
 *                   on the FIRST child that succeeds; short-circuit on SUCCESS.
 *
 *   DECORATOR nodes (exactly one child) — not implemented here but described
 *   for reference:
 *     BtInverter  — flips SUCCESS ↔ FAILURE.
 *     BtRepeat    — re-ticks its child N times.
 *     BtSucceeder — always returns SUCCESS regardless of child.
 *
 * ─── Execution Status ────────────────────────────────────────────────────────
 *
 *   SUCCESS — the node completed its task successfully.
 *   FAILURE — the node could not complete its task.
 *   RUNNING — the node is still executing (multi-frame tasks).
 *
 *   A parent composite remembers which child returned RUNNING last frame and
 *   resumes from that child on the next tick (the "last running child" pointer).
 *
 * ─── Blackboard ──────────────────────────────────────────────────────────────
 *
 *   All nodes share a BtBlackboard: a typed key→value store.  The blackboard
 *   decouples nodes from each other — a "see player" condition writes
 *   "playerVisible=true"; a "move to player" action reads it.
 *
 *   TEACHING NOTE — Blackboard Pattern
 *   This is the SHARED MEMORY pattern from concurrent programming adapted to
 *   AI.  Each node reads/writes NAMED KEYS rather than calling each other
 *   directly.  This enables drag-and-drop reuse of nodes across different
 *   enemy types without coupling them.
 *
 * ─── Example tree for an enemy guard ────────────────────────────────────────
 *
 *   Selector   (root — try in order until one succeeds)
 *   ├── Sequence  (engage player if visible)
 *   │     ├── Condition: IsPlayerVisible
 *   │     └── Action:    AttackPlayer
 *   ├── Sequence  (flee if wounded)
 *   │     ├── Condition: IsLowHealth
 *   │     └── Action:    FleeFromPlayer
 *   └── Action: Wander   (fallback — always succeeds)
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 *
 * C++ Standard: C++17
 */

#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <variant>
#include <cassert>

// ============================================================================
// BtStatus — the three possible outcomes of a node tick
// ============================================================================

/**
 * @enum BtStatus
 * @brief Result returned by every node after a single Tick() call.
 *
 * TEACHING NOTE — Why only three values?
 * ─────────────────────────────────────────
 * Many AI systems use boolean "done / not done".  RUNNING is the critical
 * addition: it allows a single logical action (e.g. "walk to waypoint") to
 * span many frames without a separate state machine.  The tree remembers
 * which child was RUNNING and resumes it next frame.
 */
enum class BtStatus : uint8_t {
    SUCCESS = 0,   ///< Task completed successfully.
    FAILURE = 1,   ///< Task could not be completed.
    RUNNING = 2,   ///< Task is in progress (multi-frame).
};


// ============================================================================
// BtBlackboard — shared typed key-value store
// ============================================================================

/**
 * @class BtBlackboard
 * @brief Typed shared memory between all nodes in a behaviour tree.
 *
 * TEACHING NOTE — Blackboard as Decoupling Mechanism
 * ────────────────────────────────────────────────────
 * Without a blackboard, node A that computes "nearest enemy position" would
 * need to directly call node B that needs that position.  This creates
 * coupling (A must know about B) and duplicates computation.
 *
 * With a blackboard:
 *   A writes:  blackboard.Set<float>("nearestEnemyDist", dist);
 *   B reads:   float d = blackboard.GetOr<float>("nearestEnemyDist", 9999.f);
 *
 * Now A and B are completely independent modules.  New nodes can read any
 * value without changing existing nodes.
 *
 * TEACHING NOTE — std::variant for type safety
 * ─────────────────────────────────────────────
 * We store values as std::variant<bool, int, float, std::string>.
 * This is safer than void* or std::any because:
 *   • The compiler enforces the type on Get<T> at compile time.
 *   • Memory layout is fixed — no heap allocation per entry (for small types).
 *   • Pattern-matching via std::visit is possible for debug dumps.
 */
class BtBlackboard {
public:
    // Supported value types.  Extend this variant if you need EntityID (uint32_t)
    // or Vec3 — just add the type to the variant list below.
    using Value = std::variant<bool, int, float, std::string>;

    // -----------------------------------------------------------------------
    // Writers
    // -----------------------------------------------------------------------

    /** Set a key to a value, overwriting any existing entry. */
    template<typename T>
    void Set(const std::string& key, T value) {
        m_data[key] = Value{ std::move(value) };
    }

    // -----------------------------------------------------------------------
    // Readers
    // -----------------------------------------------------------------------

    /**
     * @brief Get the value for @p key, or @p fallback if the key is absent.
     *
     * TEACHING NOTE — Why GetOr instead of throwing on miss?
     * ──────────────────────────────────────────────────────────
     * Throwing on a missing key makes nodes ORDER-DEPENDENT: node B must run
     * after node A or it crashes.  GetOr lets every node run in any order;
     * the fallback value represents "key not yet written" gracefully.
     */
    template<typename T>
    T GetOr(const std::string& key, T fallback) const {
        auto it = m_data.find(key);
        if (it == m_data.end()) return fallback;
        const T* ptr = std::get_if<T>(&it->second);
        return ptr ? *ptr : fallback;
    }

    /** @brief Returns true if @p key exists in the blackboard. */
    bool Has(const std::string& key) const {
        return m_data.count(key) > 0;
    }

    /** @brief Remove all entries (e.g. at the end of a behaviour episode). */
    void Clear() { m_data.clear(); }

private:
    std::unordered_map<std::string, Value> m_data;
};


// ============================================================================
// BtNode — abstract base for all tree nodes
// ============================================================================

/**
 * @class BtNode
 * @brief Abstract base class for every node in a behaviour tree.
 *
 * Every node implements Tick(), which may:
 *   - Return SUCCESS / FAILURE immediately (leaf condition/action single-step).
 *   - Return RUNNING and continue work next frame (long-running action).
 *
 * TEACHING NOTE — Polymorphism via virtual Tick()
 * ──────────────────────────────────────────────────
 * The BT framework is built entirely around ONE virtual method: Tick().
 * This is the "Command pattern" (GoF) applied to AI nodes: every node
 * is an object that knows how to execute itself.  The framework does not
 * need to know whether a node is a condition, a sequence, or a network
 * request — it just calls Tick().
 *
 * Performance note: virtual dispatch adds ~3–5 ns per call.  A tree with
 * 20 nodes ticked at 60 Hz for 500 enemies = ~300 000 virtual calls/s.
 * On a modern CPU that is well under 1 ms — negligible.  Only for tens of
 * thousands of entities would you consider data-oriented flattening.
 */
class BtNode {
public:
    virtual ~BtNode() = default;

    /**
     * @brief Evaluate this node for one simulation frame.
     *
     * @param bb  Shared blackboard (read/write).
     * @return    SUCCESS, FAILURE, or RUNNING.
     */
    virtual BtStatus Tick(BtBlackboard& bb) = 0;

    /**
     * @brief Optional reset hook called before a fresh tree evaluation begins.
     *
     * TEACHING NOTE — Reset vs. Tick
     * ───────────────────────────────
     * RUNNING nodes need to know when they have been INTERRUPTED — e.g. the
     * enemy was chasing the player but the tree root switched to FleeFromPlayer
     * mid-way.  Reset() gives each node a chance to cancel ongoing work
     * (cancel a pathfind request, release a locked door, etc.).
     * The default implementation propagates to all children.
     */
    virtual void Reset() {}

protected:
    BtNode() = default;
};


// ============================================================================
// BtComposite — base for multi-child nodes
// ============================================================================

/**
 * @class BtComposite
 * @brief Base class for nodes that own a list of child nodes.
 *
 * TEACHING NOTE — Composite Pattern
 * ────────────────────────────────────
 * This is the Composite design pattern (GoF):  BtSequence and BtSelector
 * look identical from the outside (both are BtNode) but internally
 * BtComposite holds N children, each of which is also a BtNode.
 * The recursive structure lets you nest sequences inside selectors and vice
 * versa to express arbitrarily complex behaviour without changing the
 * framework code.
 */
class BtComposite : public BtNode {
public:
    void AddChild(std::unique_ptr<BtNode> child) {
        m_children.push_back(std::move(child));
    }

    void Reset() override {
        m_runningIndex = 0;
        for (auto& c : m_children) c->Reset();
    }

protected:
    std::vector<std::unique_ptr<BtNode>> m_children;

    // TEACHING NOTE — Resuming a running child
    // ──────────────────────────────────────────
    // When a child returns RUNNING we store its index here.  On the next
    // Tick() call we jump straight to that child rather than restarting
    // from index 0.  This preserves the semantic that a RUNNING action
    // continues from where it left off.
    size_t m_runningIndex = 0;
};


// ============================================================================
// BtSequence — "AND gate": all children must succeed
// ============================================================================

/**
 * @class BtSequence
 * @brief Composite node that succeeds only when ALL children succeed.
 *
 * Execution:
 *   Tick children in order, left to right.
 *   - If a child returns FAILURE → the sequence returns FAILURE immediately.
 *   - If a child returns RUNNING → the sequence returns RUNNING (resumes
 *     from that child on the next frame).
 *   - If all children return SUCCESS → the sequence returns SUCCESS.
 *
 * TEACHING NOTE — Sequence as "Check then Do"
 * ─────────────────────────────────────────────
 * The most common BT pattern is:
 *
 *   Sequence
 *     ├── Condition: CanSeePlayer
 *     └── Action:    ShootAtPlayer
 *
 * The condition acts as a GUARD: the action only runs if the condition
 * passes.  This replaces an if() statement with a composable node.
 * The advantage: you can insert another condition between them later
 * (e.g. HasAmmo) without touching the existing condition or action.
 */
class BtSequence final : public BtComposite {
public:
    BtStatus Tick(BtBlackboard& bb) override {
        // TEACHING NOTE — Start from the running child index, not 0.
        // If we restarted from 0 every frame, a long-running action (e.g.
        // "walk to waypoint") would be interrupted on every tick before
        // it finishes.
        for (size_t i = m_runningIndex; i < m_children.size(); ++i) {
            BtStatus s = m_children[i]->Tick(bb);
            if (s == BtStatus::FAILURE) {
                m_runningIndex = 0;  // reset for next evaluation
                return BtStatus::FAILURE;
            }
            if (s == BtStatus::RUNNING) {
                m_runningIndex = i;
                return BtStatus::RUNNING;
            }
            // SUCCESS — advance to the next child
        }
        m_runningIndex = 0;  // all children succeeded; reset for reuse
        return BtStatus::SUCCESS;
    }
};


// ============================================================================
// BtSelector — "OR gate": first success wins
// ============================================================================

/**
 * @class BtSelector
 * @brief Composite node that succeeds when ANY child succeeds.
 *
 * Execution:
 *   Tick children in order, left to right.
 *   - If a child returns SUCCESS → the selector returns SUCCESS immediately.
 *   - If a child returns RUNNING → the selector returns RUNNING (resumes
 *     from that child on the next frame).
 *   - If all children return FAILURE → the selector returns FAILURE.
 *
 * TEACHING NOTE — Selector as Priority List
 * ───────────────────────────────────────────
 * A Selector models PRIORITY-ordered fallback:
 *
 *   Selector
 *     ├── Sequence: [CanSeePlayer] → [AttackPlayer]   ← highest priority
 *     ├── Sequence: [IsWounded]    → [FleeToBase]      ← second priority
 *     └── Action:   Wander                              ← fallback (always succeeds)
 *
 * The enemy attacks if it can, flees if it can't attack, and wanders
 * as a last resort.  Adding a new behaviour is as simple as inserting
 * a new child at the right priority position.
 */
class BtSelector final : public BtComposite {
public:
    BtStatus Tick(BtBlackboard& bb) override {
        for (size_t i = m_runningIndex; i < m_children.size(); ++i) {
            BtStatus s = m_children[i]->Tick(bb);
            if (s == BtStatus::SUCCESS) {
                m_runningIndex = 0;
                return BtStatus::SUCCESS;
            }
            if (s == BtStatus::RUNNING) {
                m_runningIndex = i;
                return BtStatus::RUNNING;
            }
            // FAILURE — try next child
        }
        m_runningIndex = 0;  // all children failed
        return BtStatus::FAILURE;
    }
};


// ============================================================================
// BtCondition — leaf node wrapping a predicate
// ============================================================================

/**
 * @class BtCondition
 * @brief Leaf node that evaluates a boolean predicate function.
 *
 * Returns SUCCESS if the predicate returns true, FAILURE otherwise.
 * Never returns RUNNING (conditions are instant).
 *
 * TEACHING NOTE — Lambda as Behaviour Node
 * ──────────────────────────────────────────
 * Using std::function<bool(BtBlackboard&)> as the predicate lets you
 * write conditions inline at the call site:
 *
 * @code
 *   auto canSee = std::make_unique<BtCondition>(
 *       [&world, playerID, entityID](BtBlackboard&) {
 *           float dist = TileDistance(world, entityID, playerID);
 *           return dist < sightRange;
 *       });
 * @endcode
 *
 * This avoids boilerplate: you don't need a new class per condition —
 * you just capture the relevant state in the lambda closure.
 */
class BtCondition final : public BtNode {
public:
    explicit BtCondition(std::function<bool(BtBlackboard&)> predicate)
        : m_predicate(std::move(predicate)) {}

    BtStatus Tick(BtBlackboard& bb) override {
        return m_predicate(bb) ? BtStatus::SUCCESS : BtStatus::FAILURE;
    }

private:
    std::function<bool(BtBlackboard&)> m_predicate;
};


// ============================================================================
// BtAction — leaf node wrapping an action functor
// ============================================================================

/**
 * @class BtAction
 * @brief Leaf node that executes a task (possibly multi-frame).
 *
 * The functor receives the blackboard and returns SUCCESS / FAILURE /
 * RUNNING.  The tree remembers RUNNING and resumes next frame.
 *
 * TEACHING NOTE — Single-frame vs. Multi-frame Actions
 * ──────────────────────────────────────────────────────
 * Single-frame: "set velocity toward player" → returns SUCCESS immediately.
 * Multi-frame:  "animate attack sequence (0.5 s)" → returns RUNNING for
 *               30 frames then SUCCESS when the animation finishes.
 *
 * The RUNNING status is what makes BTs suitable for real-time games; it
 * acts as a cooperative coroutine without threads.
 */
class BtAction final : public BtNode {
public:
    explicit BtAction(std::function<BtStatus(BtBlackboard&)> action)
        : m_action(std::move(action)) {}

    BtStatus Tick(BtBlackboard& bb) override {
        return m_action(bb);
    }

private:
    std::function<BtStatus(BtBlackboard&)> m_action;
};


// ============================================================================
// BtTree — owns the root node and drives the tick loop
// ============================================================================

/**
 * @class BtTree
 * @brief Owns the root node of a behaviour tree and drives frame-by-frame evaluation.
 *
 * TEACHING NOTE — One BtTree per entity
 * ───────────────────────────────────────
 * In a real engine each enemy entity has its OWN BtTree instance with its
 * own BtBlackboard.  The tree *structure* (the node graph) can be shared
 * (it is read-only), but the runtime state (which child is RUNNING, what
 * is on the blackboard) must be per-entity.
 *
 * For this teaching engine we allocate one BtTree per entity in BtAISystem
 * and store it alongside the AIComponent.
 *
 * USAGE:
 * @code
 *   BtTree tree;
 *
 *   auto root = std::make_unique<BtSelector>();
 *   // ... add children ...
 *   tree.SetRoot(std::move(root));
 *
 *   // Game loop:
 *   tree.Tick();  // updates the blackboard and advances the tree
 * @endcode
 */
class BtTree {
public:
    /** Assign the root node (transfers ownership). */
    void SetRoot(std::unique_ptr<BtNode> root) {
        m_root = std::move(root);
    }

    /**
     * @brief Execute one frame of the behaviour tree.
     *
     * @return The status of the root node after this tick.
     *
     * TEACHING NOTE — Full restart vs partial tick
     * ──────────────────────────────────────────────
     * BTs can be "memoryless" (restart the root every frame) or "stateful"
     * (resume from the last RUNNING leaf).  We use the stateful variant:
     * composites remember their m_runningIndex.  This is the more common
     * production style because it avoids replaying conditions that are
     * expensive to evaluate (e.g. line-of-sight raycasts).
     */
    BtStatus Tick() {
        if (!m_root) return BtStatus::FAILURE;
        return m_root->Tick(m_blackboard);
    }

    /** Reset all node state (e.g. when the entity enters a new episode). */
    void Reset() {
        if (m_root) m_root->Reset();
        m_blackboard.Clear();
    }

    /** Direct access to the shared blackboard (read/write). */
    BtBlackboard& Blackboard() { return m_blackboard; }
    const BtBlackboard& Blackboard() const { return m_blackboard; }

private:
    std::unique_ptr<BtNode> m_root;
    BtBlackboard            m_blackboard;
};
