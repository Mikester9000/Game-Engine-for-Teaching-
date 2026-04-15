/**
 * @file EventBus.hpp
 * @brief Generic publish-subscribe event bus for decoupled communication.
 *
 * ----------------------------------------------------------------------------
 * TEACHING NOTE — Why a Publish-Subscribe (Observer) Pattern?
 * ----------------------------------------------------------------------------
 * In a complex game, many systems need to react to things that other systems
 * do.  Naïve approach: every system holds a pointer to every other system
 * it might need to notify.  Problems:
 *
 *   • Tight coupling: changing the CombatSystem requires updating every
 *     other system that has a pointer to it.
 *   • Hard to add new systems: to add an AchievementSystem you'd need to
 *     modify CombatSystem, QuestSystem, etc. to know about it.
 *   • Circular dependencies: SystemA → SystemB → SystemA can cause compile
 *     errors and undefined behaviour.
 *
 * The Observer pattern (also called publish-subscribe or event bus) fixes
 * this by introducing a MEDIATOR:
 *
 *   Publisher → EventBus → [Subscriber1, Subscriber2, Subscriber3 …]
 *
 * The publisher knows NOTHING about subscribers.  New subscribers can be
 * added without touching the publisher.  Systems are fully decoupled.
 *
 * Real-world analogies:
 *   • A newspaper: the publisher prints once; any number of subscribers read.
 *   • A radio station: one broadcast, many receivers.
 *   • JavaScript DOM events: element.addEventListener('click', handler).
 *
 * ----------------------------------------------------------------------------
 * TEACHING NOTE — Template Design
 * ----------------------------------------------------------------------------
 * We use a template `EventBus<EventType>` so a single implementation works
 * for ANY event struct.  There is one bus instance per event type:
 *
 *   EventBus<CombatEvent>  — all combat notifications
 *   EventBus<QuestEvent>   — all quest notifications
 *   EventBus<UIEvent>      — all UI notifications
 *
 * This approach provides compile-time type safety: you cannot accidentally
 * subscribe a QuestEvent handler to the CombatEvent bus.
 *
 * ----------------------------------------------------------------------------
 * TEACHING NOTE — std::function and Lambda Closures
 * ----------------------------------------------------------------------------
 * std::function<void(const T&)> is a type-erased wrapper around anything
 * that can be called with a `const T&` argument: a free function, a member
 * function wrapped in std::bind, or a *lambda*.
 *
 * A lambda is an anonymous function defined inline:
 *   auto handler = [](const CombatEvent& e) { std::cout << e.damage; };
 *
 * The `[&]` or `[=]` capture clause lets the lambda capture local variables
 * from the enclosing scope by reference or by value.
 *
 * IMPORTANT: If a lambda captures `this` (a raw pointer), the subscriber
 * MUST unsubscribe before the owning object is destroyed — otherwise the
 * EventBus holds a dangling pointer (use-after-free bug).  The
 * SubscriptionToken / Unsubscribe() mechanism exists precisely for this.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 */

#pragma once

// ---------------------------------------------------------------------------
// Includes
// ---------------------------------------------------------------------------
#include <functional>    // std::function — type-erased callable wrapper.
#include <unordered_map> // std::unordered_map — O(1) average lookup by token.
#include <vector>        // std::vector — ordered list of subscriber calls.
#include <cstdint>       // uint64_t — token type.
#include <atomic>        // std::atomic — lock-free token generation.
#include <mutex>         // std::mutex, std::lock_guard — thread safety.
#include <string>        // std::string — event names in structs.
#include <stdexcept>     // std::runtime_error — error on bad token.

// Forward-include our vocabulary types for event struct definitions below.
#include "Types.hpp"


// ===========================================================================
// Section 1 — Event Structs
// ===========================================================================

/**
 * @defgroup EventStructs Game Event Structs
 * @{
 *
 * TEACHING NOTE — Event Struct Design
 * ─────────────────────────────────────
 * Each event struct is a *value type* (copyable struct, no virtual methods)
 * carrying all the data a subscriber might need.  This lets subscribers
 * react without storing pointers back to the source (which might become
 * invalid).
 *
 * Keep event structs small and cheap to copy.  If you need to pass large
 * data, store it elsewhere and pass only an ID or a shared_ptr.
 */

/**
 * @struct CombatEvent
 * @brief Emitted when damage is dealt, an entity dies, or a buff is applied.
 *
 * Example subscriber use:
 * @code
 *   EventBus<CombatEvent>::Instance().Subscribe([](const CombatEvent& e) {
 *       if (e.type == CombatEvent::Type::DAMAGE_DEALT) {
 *           hudSystem.ShowDamageNumber(e.targetID, e.amount);
 *       }
 *   });
 * @endcode
 */
struct CombatEvent {
    /// The kind of combat event that occurred.
    enum class Type : uint8_t {
        DAMAGE_DEALT,    ///< An entity received damage.
        HEALING_APPLIED, ///< An entity recovered HP.
        ENTITY_DIED,     ///< An entity's HP reached zero.
        STATUS_APPLIED,  ///< A status effect was applied.
        STATUS_CLEARED,  ///< A status effect was removed.
        SKILL_USED,      ///< An entity used an active skill.
        WARP_STRIKE,     ///< Noctis performed a warp strike.
        PARTY_KO         ///< The entire player party is knocked out.
    };

    Type         type      = Type::DAMAGE_DEALT; ///< What happened.
    EntityID     sourceID  = NULL_ENTITY;         ///< Entity that caused the event.
    EntityID     targetID  = NULL_ENTITY;         ///< Entity that was affected.
    int32_t      amount    = 0;                   ///< HP delta (positive = heal, negative = damage).
    DamageType   dmgType   = DamageType::PHYSICAL;///< Classification of the damage.
    ElementType  element   = ElementType::NONE;   ///< Elemental type (if any).
    StatusEffect effect    = StatusEffect::NONE;  ///< Which status (for STATUS_APPLIED/CLEARED).
    bool         isCritical= false;               ///< Was this a critical hit?
};

/**
 * @struct QuestEvent
 * @brief Emitted when quest state changes (accepted, updated, completed, failed).
 *
 * The quest system publishes these; the UI, achievement system, and
 * dialogue system subscribe.
 */
struct QuestEvent {
    /// What happened to the quest.
    enum class Type : uint8_t {
        QUEST_ACCEPTED,   ///< Player accepted a new quest.
        OBJECTIVE_UPDATED,///< A tracked objective counter changed.
        QUEST_COMPLETED,  ///< All objectives satisfied.
        QUEST_FAILED,     ///< Quest failed (time limit, wrong choice, etc.).
        REWARD_GRANTED    ///< XP / items were added to the player's inventory.
    };

    Type        type        = Type::OBJECTIVE_UPDATED;
    uint32_t    questID     = 0;     ///< ID of the affected quest.
    uint32_t    objectiveID = 0;     ///< Which objective changed (if relevant).
    int32_t     progressDelta = 0;   ///< How much the counter changed (+/-).
    EntityID    playerID    = NULL_ENTITY; ///< Which party member triggered this.
    std::string questName;            ///< Human-readable name for UI display.
};

/**
 * @struct WorldEvent
 * @brief Emitted when the game world changes state (time, weather, region).
 *
 * The WorldSystem publishes these; the renderer, audio manager, and AI
 * director subscribe to adjust lighting, music, and spawn tables.
 */
struct WorldEvent {
    /// What changed in the world.
    enum class Type : uint8_t {
        TIME_CHANGED,       ///< TimeOfDay period advanced (e.g. NOON → AFTERNOON).
        WEATHER_CHANGED,    ///< WeatherType transitioned.
        REGION_ENTERED,     ///< Player entered a new named region.
        FAST_TRAVEL,        ///< Fast travel completed — world teleport.
        ENEMY_ALERT,        ///< Enemies have detected the player party.
        ENEMY_PATROL,       ///< Enemies returned to patrol (alert cancelled).
        CAMP_STARTED,       ///< Players set up camp.
        DAY_ADVANCED        ///< Midnight passed; a new in-game day began.
    };

    Type        type        = Type::TIME_CHANGED;
    TimeOfDay   timeOfDay   = TimeOfDay::NOON;
    WeatherType weather     = WeatherType::CLEAR;
    std::string regionName;          ///< New region name (for REGION_ENTERED).
    Vec3        eventPosition;       ///< World position where the event originated.
};

/**
 * @struct UIEvent
 * @brief Emitted to drive the HUD and menu system reactively.
 *
 * Rather than the HUD polling game state every frame, it subscribes to
 * UIEvents and updates only when something actually changed.  This is more
 * efficient and decoupled.
 */
struct UIEvent {
    /// What the UI should do.
    enum class Type : uint8_t {
        OPEN_MENU,           ///< Push a new menu screen.
        CLOSE_MENU,          ///< Pop the current menu screen.
        SHOW_NOTIFICATION,   ///< Brief on-screen popup (item obtained, etc.).
        UPDATE_HP_BAR,       ///< A party member's HP changed — redraw HP bar.
        UPDATE_MP_BAR,       ///< MP changed.
        UPDATE_QUEST_LOG,    ///< Quest log entry needs refresh.
        SHOW_DAMAGE_NUMBER,  ///< Floating damage number above an entity.
        HIDE_HUD,            ///< Hide the HUD (cutscene).
        SHOW_HUD             ///< Restore the HUD after a cutscene.
    };

    Type        type        = Type::SHOW_NOTIFICATION;
    EntityID    entityID    = NULL_ENTITY; ///< Relevant entity (e.g. whose HP changed).
    std::string text;                       ///< Text string (notification message).
    int32_t     value       = 0;            ///< Numeric value (HP amount, etc.).
    Color       colour      = Color::White();///< Tint for the UI element.
    float       duration    = 3.0f;         ///< How long (seconds) to show a notification.
    Vec2        screenPos;                  ///< Screen position for floating numbers.
};

/** @} */ // end of EventStructs


// ===========================================================================
// Section 2 — EventBus template
// ===========================================================================

/**
 * @class EventBus
 * @brief A type-safe publish-subscribe event bus.
 *
 * @tparam EventType  The event struct type this bus carries.
 *
 * TEACHING NOTE — Template Singleton
 * ────────────────────────────────────
 * EventBus<CombatEvent> and EventBus<QuestEvent> are completely independent
 * classes generated by the compiler from the same template.  Each has its
 * own Instance(), its own subscriber list, its own mutex.  There is no
 * interference between buses for different event types.
 *
 * This technique is called "static polymorphism" or "template-based type
 * dispatch" — we get per-type behaviour without virtual functions.
 *
 * Thread Safety:
 *   Subscribe, Unsubscribe, and Publish are all guarded by m_mutex.
 *   A subscriber added from thread A is visible to Publish calls from
 *   thread B after the Subscribe call returns.
 *
 *   IMPORTANT: Publish holds the mutex while iterating subscribers.
 *   If a subscriber calls Subscribe or Publish on the SAME bus inside
 *   its callback, that will DEADLOCK on a non-recursive mutex.
 *   Solution: use std::recursive_mutex, or (better) buffer events and
 *   dispatch them outside the lock — the "deferred dispatch" pattern.
 *   For this educational engine we use a simple mutex and document the
 *   re-entrancy restriction.
 */
template<typename EventType>
class EventBus {
public:
    // -----------------------------------------------------------------------
    // Type aliases (local to the template instantiation)
    // -----------------------------------------------------------------------

    /**
     * @brief The callback type subscribers must provide.
     *
     * A subscriber is any callable that accepts a `const EventType&` and
     * returns void.  std::function wraps lambdas, free functions, and
     * std::bind results uniformly.
     *
     * TEACHING NOTE — std::function overhead
     * ─────────────────────────────────────────
     * std::function has a small overhead compared to a raw function pointer:
     * it heap-allocates the closure if the captured state is large ("small
     * buffer optimisation" avoids this for small closures on most STL
     * implementations).  For event systems the overhead is negligible.
     */
    using Callback = std::function<void(const EventType&)>;

    // -----------------------------------------------------------------------
    // Singleton
    // -----------------------------------------------------------------------

    /**
     * @brief Return the singleton bus for this EventType.
     *
     * Each EventBus<T> template instantiation has its OWN static instance
     * because `instance` is a static local variable inside a function
     * template — one per instantiation.
     */
    static EventBus<EventType>& Instance() {
        static EventBus<EventType> instance;
        return instance;
    }

    // Prevent copy / move (singleton semantics).
    EventBus(const EventBus&)            = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&)                 = delete;
    EventBus& operator=(EventBus&&)      = delete;

    // -----------------------------------------------------------------------
    // Subscribe
    // -----------------------------------------------------------------------

    /**
     * @brief Register a callback to be called whenever an event is published.
     *
     * TEACHING NOTE — Token-based Unsubscription
     * ─────────────────────────────────────────────
     * We return a SubscriptionToken (a unique integer ID) rather than storing
     * a pointer to the callback.  The token lets the subscriber later call
     * Unsubscribe(token) to remove itself — this is cleaner than comparing
     * std::function objects (which are not equality-comparable).
     *
     * The token is generated by atomically incrementing a global counter.
     * std::atomic<uint64_t> operations are thread-safe without a mutex.
     *
     * @param callback  A callable to invoke on each published event.
     * @return          A token you must store if you want to unsubscribe.
     *
     * Usage:
     * @code
     *   auto token = EventBus<CombatEvent>::Instance().Subscribe(
     *       [this](const CombatEvent& e) { OnCombatEvent(e); });
     *   // Later, when 'this' is destroyed:
     *   EventBus<CombatEvent>::Instance().Unsubscribe(token);
     * @endcode
     */
    SubscriptionToken Subscribe(Callback callback) {
        // Generate a unique token using a static atomic counter.
        // Atomic increment is lock-free and safe across threads.
        SubscriptionToken token = s_nextToken.fetch_add(1,
                                      std::memory_order_relaxed);

        std::lock_guard<std::mutex> lock(m_mutex);
        m_subscribers.emplace(token, std::move(callback));
        return token;
    }

    // -----------------------------------------------------------------------
    // Unsubscribe
    // -----------------------------------------------------------------------

    /**
     * @brief Remove a previously registered callback.
     *
     * TEACHING NOTE — When to Unsubscribe
     * ──────────────────────────────────────
     * Failure to unsubscribe before the subscriber is destroyed leads to
     * undefined behaviour: the EventBus will call a function that no longer
     * exists, or access memory belonging to a destroyed object.
     *
     * Best practice: in the destructor of any class that subscribes,
     * unsubscribe all tokens.  Alternatively, use a "scoped subscription"
     * RAII wrapper (see ScopedSubscription below).
     *
     * @param token  The token returned by Subscribe().
     * @return true if the token was found and removed; false if not found.
     */
    bool Unsubscribe(SubscriptionToken token) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_subscribers.find(token);
        if (it != m_subscribers.end()) {
            m_subscribers.erase(it);
            return true;
        }
        return false;
    }

    // -----------------------------------------------------------------------
    // Publish
    // -----------------------------------------------------------------------

    /**
     * @brief Emit an event to all registered subscribers.
     *
     * TEACHING NOTE — Pass by const reference
     * ──────────────────────────────────────────
     * We pass the event as `const EventType&` to avoid copying it.
     * Each subscriber receives the same single event object.  Subscribers
     * MUST NOT modify the event; if a subscriber needs to mutate state,
     * it should copy the event fields it needs.
     *
     * TEACHING NOTE — Snapshot copy
     * ───────────────────────────────
     * We copy the subscriber map into a local vector *before* calling
     * callbacks.  This prevents an issue where a callback calls Unsubscribe()
     * (modifying m_subscribers while we are iterating it), which would be
     * undefined behaviour.
     *
     * Note the mutex is released before callbacks are invoked!  This allows
     * callbacks to call Subscribe/Unsubscribe on OTHER buses.  If a callback
     * needs to modify THIS bus, use a deferred dispatch pattern.
     *
     * @param event  The event to broadcast to all subscribers.
     */
    void Publish(const EventType& event) {
        // ── Step 1: snapshot subscribers under the lock ──
        std::vector<Callback> callbackSnapshot;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            callbackSnapshot.reserve(m_subscribers.size());
            for (auto& [token, cb] : m_subscribers) {
                callbackSnapshot.push_back(cb);
            }
        } // lock released here

        // ── Step 2: invoke callbacks WITHOUT holding the lock ──
        for (auto& cb : callbackSnapshot) {
            if (cb) { // Guard against null std::function (shouldn't happen).
                cb(event);
            }
        }
    }

    // -----------------------------------------------------------------------
    // Inspection helpers
    // -----------------------------------------------------------------------

    /// Return the number of active subscribers.
    std::size_t SubscriberCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_subscribers.size();
    }

    /// Remove ALL subscribers (useful in tests or when unloading a level).
    void Clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_subscribers.clear();
    }

private:
    // Private constructor — only Instance() can create us.
    EventBus() = default;
    ~EventBus() = default;

    // -----------------------------------------------------------------------
    // Member data
    // -----------------------------------------------------------------------

    mutable std::mutex                              m_mutex;       ///< Protects m_subscribers.
    std::unordered_map<SubscriptionToken, Callback> m_subscribers; ///< token → callback.

    /**
     * @brief Global counter for generating unique SubscriptionTokens.
     *
     * TEACHING NOTE — std::atomic
     * ─────────────────────────────
     * `static` here means this counter is shared across ALL EventBus
     * instantiations (all event types share the same token namespace).
     * This guarantees that even if the same integer token were returned by
     * two different buses, they won't collide, because the counter is global.
     *
     * std::atomic ensures increment-and-read is indivisible (atomic) across
     * threads — no race condition, no mutex needed for this counter.
     */
    static inline std::atomic<SubscriptionToken> s_nextToken{1};
};


// ===========================================================================
// Section 3 — ScopedSubscription RAII helper
// ===========================================================================

/**
 * @class ScopedSubscription
 * @brief RAII wrapper that automatically unsubscribes when it goes out of scope.
 *
 * TEACHING NOTE — RAII for Resource Management
 * ───────────────────────────────────────────────
 * RAII (Resource Acquisition Is Initialisation) is one of C++'s most
 * powerful idioms.  The idea: tie the lifetime of a resource (here, a
 * subscription) to the lifetime of an object on the stack (here, a
 * ScopedSubscription).
 *
 * When the ScopedSubscription is destroyed (stack unwinds, owner destroyed),
 * the destructor automatically calls Unsubscribe().  You can NEVER forget to
 * unsubscribe — the language enforces it.
 *
 * This is the same principle behind:
 *   std::unique_ptr  — automatically frees memory
 *   std::lock_guard  — automatically unlocks mutex
 *   std::ifstream    — automatically closes file
 *
 * @tparam EventType  The event type whose bus this subscription is for.
 *
 * Usage:
 * @code
 *   class CombatHUD {
 *       ScopedSubscription<CombatEvent> m_combatSub;
 *   public:
 *       CombatHUD() {
 *           m_combatSub = ScopedSubscription<CombatEvent>(
 *               [this](const CombatEvent& e) { UpdateHUD(e); });
 *       }
 *       // Destructor auto-unsubscribes — no manual cleanup needed.
 *   };
 * @endcode
 */
template<typename EventType>
class ScopedSubscription {
public:
    ScopedSubscription() = default;

    /**
     * @brief Subscribe to the bus with the given callback.
     * @param callback  Callable to invoke on each event.
     */
    explicit ScopedSubscription(typename EventBus<EventType>::Callback callback)
        : m_token(EventBus<EventType>::Instance().Subscribe(std::move(callback)))
        , m_subscribed(true)
    {}

    /// Move constructor — transfers ownership of the token.
    ScopedSubscription(ScopedSubscription&& other) noexcept
        : m_token(other.m_token)
        , m_subscribed(other.m_subscribed)
    {
        other.m_subscribed = false;
    }

    /// Move assignment — unsubscribe current token, then acquire other's.
    ScopedSubscription& operator=(ScopedSubscription&& other) noexcept {
        if (this != &other) {
            Reset();
            m_token       = other.m_token;
            m_subscribed  = other.m_subscribed;
            other.m_subscribed = false;
        }
        return *this;
    }

    // Non-copyable — a subscription token represents a unique subscription.
    ScopedSubscription(const ScopedSubscription&)            = delete;
    ScopedSubscription& operator=(const ScopedSubscription&) = delete;

    /// Destructor — automatically unsubscribes.
    ~ScopedSubscription() {
        Reset();
    }

    /**
     * @brief Manually unsubscribe before the object is destroyed.
     *
     * Useful when you need to unsubscribe early (e.g. before changing level).
     */
    void Reset() {
        if (m_subscribed) {
            EventBus<EventType>::Instance().Unsubscribe(m_token);
            m_subscribed = false;
        }
    }

    /// True if this object holds an active subscription.
    bool IsSubscribed() const { return m_subscribed; }

private:
    SubscriptionToken m_token      = 0;
    bool              m_subscribed = false;
};


// ===========================================================================
// Section 4 — Global convenience free functions
// ===========================================================================

/**
 * @brief Publish a CombatEvent to all listeners (free-function shorthand).
 *
 * TEACHING NOTE — Shorthand vs Method Call
 * ──────────────────────────────────────────
 * These free functions are thin wrappers around EventBus<T>::Instance().Publish().
 * They reduce boilerplate at call sites:
 *
 *   Publish(e);              // short
 *   EventBus<CombatEvent>::Instance().Publish(e);  // verbose
 *
 * Both compile to identical machine code — the call is inlined away.
 */

/// Shorthand: publish a CombatEvent.
inline void Publish(const CombatEvent& e)  { EventBus<CombatEvent>::Instance().Publish(e); }
/// Shorthand: publish a QuestEvent.
inline void Publish(const QuestEvent& e)   { EventBus<QuestEvent>::Instance().Publish(e);  }
/// Shorthand: publish a WorldEvent.
inline void Publish(const WorldEvent& e)   { EventBus<WorldEvent>::Instance().Publish(e);  }
/// Shorthand: publish a UIEvent.
inline void Publish(const UIEvent& e)      { EventBus<UIEvent>::Instance().Publish(e);     }


// ===========================================================================
// Section 5 — Usage examples (compiled only in documentation builds)
// ===========================================================================

/**
 * @example EventBusUsageExample
 *
 * @code
 * // --- Setup (e.g., in a system constructor) --------------------------------
 *
 * class DamageNumberSystem {
 * private:
 *     // ScopedSubscription automatically unsubscribes when 'this' is destroyed.
 *     ScopedSubscription<CombatEvent> m_sub;
 *
 * public:
 *     DamageNumberSystem() {
 *         m_sub = ScopedSubscription<CombatEvent>(
 *             [this](const CombatEvent& e) {
 *                 if (e.type == CombatEvent::Type::DAMAGE_DEALT) {
 *                     SpawnFloatingNumber(e.targetID, e.amount, e.isCritical);
 *                 }
 *             });
 *     }
 *
 *     void SpawnFloatingNumber(EntityID id, int32_t dmg, bool crit) {
 *         // Create a floating text entity above the target.
 *     }
 * };
 *
 * // --- Publishing (e.g., inside CombatSystem) --------------------------------
 *
 * void CombatSystem::ApplyDamage(EntityID src, EntityID tgt, int32_t dmg) {
 *     // ... apply damage logic ...
 *
 *     CombatEvent evt;
 *     evt.type       = CombatEvent::Type::DAMAGE_DEALT;
 *     evt.sourceID   = src;
 *     evt.targetID   = tgt;
 *     evt.amount     = -dmg;    // negative = damage
 *     evt.isCritical = (dmg > 9999);
 *
 *     Publish(evt);  // All subscribers are notified automatically.
 * }
 * @endcode
 */
