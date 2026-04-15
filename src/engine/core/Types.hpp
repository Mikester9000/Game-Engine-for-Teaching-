/**
 * @file Types.hpp
 * @brief Core type definitions for the educational action RPG game engine.
 *
 * ----------------------------------------------------------------------------
 * TEACHING NOTE — What is this file?
 * ----------------------------------------------------------------------------
 * In any non-trivial C++ project it pays to have ONE authoritative place that
 * defines all "vocabulary types": small structs, enums, and typedefs that
 * are shared across many translation units.  Putting them here means:
 *
 *  1. Every subsystem includes ONE header instead of defining its own
 *     conflicting int2 / Point / vec3 types.
 *  2. Changing a fundamental type (e.g. EntityID from uint32_t to uint64_t)
 *     only requires editing this one file.
 *  3. Doxygen / documentation tools can produce a single "data dictionary"
 *     for students reading the engine.
 *
 * This design follows the "Single Source of Truth" principle.
 * ----------------------------------------------------------------------------
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 *
 * C++ Standard: C++17
 *
 * Usage example:
 * @code
 *   #include "engine/core/Types.hpp"
 *
 *   Vec3 position{1.0f, 0.0f, 5.0f};
 *   position += Vec3{0.0f, 0.0f, 1.0f};  // move forward
 *
 *   EntityID hero = 42;
 *   GameState state = GameState::EXPLORING;
 * @endcode
 */

#pragma once

// ---------------------------------------------------------------------------
// Standard library includes
// ---------------------------------------------------------------------------
// <cstdint>  — fixed-width integer types (uint8_t, uint32_t, etc.)
//              Prefer these over bare 'int' when size matters (networking,
//              serialization, ECS ID packing).
#include <cstdint>

// <cmath>    — std::sqrt, std::abs, etc. for vector math helpers.
#include <cmath>

// <string>   — std::string for name fields.
#include <string>

// <limits>   — std::numeric_limits, used for sentinel / invalid-ID constants.
#include <limits>

// <array>    — std::array, used inside some compound structs.
#include <array>


// ===========================================================================
// Section 1 — Primitive type aliases
// ===========================================================================

/**
 * @defgroup PrimitiveAliases Primitive Type Aliases
 * @{
 *
 * TEACHING NOTE — Why typedef / using?
 * ─────────────────────────────────────
 * Using aliases (C++11 `using` syntax) improves readability and provides a
 * single place to swap the underlying type.  For example, if we later need
 * 64-bit entity IDs for a networked game, we change ONE line here.
 *
 * `using X = Y;`  is the modern C++11 equivalent of  `typedef Y X;`.
 * Prefer `using` because it works with templates whereas `typedef` does not.
 */

/**
 * @brief Unique identifier for an Entity in the ECS (Entity Component System).
 *
 * An Entity is just a number.  It has no data or behaviour of its own — it
 * only acts as a key that ties components together inside component pools.
 *
 * Max value (0xFFFFFFFF) is reserved as the "null" / invalid entity sentinel.
 */
using EntityID = uint32_t;

/**
 * @brief Unique identifier for a Component *type* (not instance).
 *
 * Each distinct component class (HealthComponent, TransformComponent …) gets
 * one globally unique ComponentID assigned at startup via a counter.
 * Used to index component pools in the World.
 */
using ComponentID = uint32_t;

/**
 * @brief Identifies a subscription token returned by EventBus::Subscribe().
 *
 * Storing the token lets a subscriber later call Unsubscribe(token) to clean
 * up its callback without needing a pointer to the original lambda.
 */
using SubscriptionToken = uint64_t;

/** @} */ // end of PrimitiveAliases


// ===========================================================================
// Section 2 — Sentinel / special value constants
// ===========================================================================

/**
 * @defgroup Sentinels Sentinel Constants
 * @{
 *
 * TEACHING NOTE — Sentinel Values
 * ────────────────────────────────
 * A sentinel value is a special value that signals an "invalid" or "absent"
 * state without using a pointer or std::optional.  Here we use the maximum
 * representable value as the null entity so valid IDs start at 0.
 *
 * This is the same trick used by many high-performance ECS frameworks
 * (e.g. EnTT uses entt::null which is ~0u).
 */

/// The null / invalid EntityID.  No real entity will ever have this ID.
constexpr EntityID NULL_ENTITY = std::numeric_limits<EntityID>::max();

/// The null / invalid ComponentID.
constexpr ComponentID NULL_COMPONENT = std::numeric_limits<ComponentID>::max();

/// Maximum number of component types the engine supports at compile-time.
/// Increase if you add more than 64 component types.
constexpr std::size_t MAX_COMPONENTS = 64;

/// Maximum number of entities that can be alive simultaneously.
/// 65 536 entities is plenty for an action-RPG — increase for MMO-scale.
constexpr std::size_t MAX_ENTITIES = 65536;

/** @} */ // end of Sentinels


// ===========================================================================
// Section 3 — 2D Vector
// ===========================================================================

/**
 * @struct Vec2
 * @brief A two-dimensional floating-point vector.
 *
 * TEACHING NOTE — Structs vs Classes
 * ────────────────────────────────────
 * In C++ a `struct` is identical to a `class` except members are public by
 * default.  For small "plain-old-data" (POD) types like vectors, structs
 * are conventional because all members are naturally public.
 *
 * We define arithmetic operators inline so code like
 *   Vec2 velocity = direction * speed;
 * reads naturally, just like mathematical notation.
 *
 * TEACHING NOTE — Operator Overloading
 * ──────────────────────────────────────
 * C++ lets you define what `+`, `-`, `*`, `/`, `==`, etc. mean for your own
 * types.  Rules to follow:
 *   • Prefer member `operator+=` / `-=` and implement `+` in terms of `+=`
 *     (this avoids code duplication).
 *   • Return by value (not reference) from binary operators like `+`.
 *   • `const` member functions do NOT modify the object — mark them `const`.
 */
struct Vec2 {
    float x = 0.0f; ///< Horizontal component (right is positive).
    float y = 0.0f; ///< Vertical component (up or down, depends on convention).

    // -----------------------------------------------------------------------
    // Constructors
    // -----------------------------------------------------------------------

    /// Default constructor — initialises to zero vector.
    Vec2() = default;

    /**
     * @brief Construct a Vec2 from explicit x, y values.
     * @param x  Horizontal component.
     * @param y  Vertical component.
     */
    constexpr Vec2(float x, float y) : x(x), y(y) {}

    // -----------------------------------------------------------------------
    // Arithmetic — compound assignment operators (mutate in-place)
    // -----------------------------------------------------------------------

    /// Add another vector to this one and store the result.
    Vec2& operator+=(const Vec2& rhs) { x += rhs.x; y += rhs.y; return *this; }
    /// Subtract another vector from this one and store the result.
    Vec2& operator-=(const Vec2& rhs) { x -= rhs.x; y -= rhs.y; return *this; }
    /// Multiply every component by a scalar.
    Vec2& operator*=(float s)         { x *= s;     y *= s;     return *this; }
    /// Divide every component by a scalar.
    Vec2& operator/=(float s)         { x /= s;     y /= s;     return *this; }

    // -----------------------------------------------------------------------
    // Arithmetic — binary operators (return new vector, leave 'this' intact)
    // TEACHING NOTE: defined using compound-assignment above — no duplication.
    // -----------------------------------------------------------------------

    /// Return the sum of two vectors.
    Vec2 operator+(const Vec2& rhs) const { Vec2 r = *this; r += rhs; return r; }
    /// Return the difference of two vectors.
    Vec2 operator-(const Vec2& rhs) const { Vec2 r = *this; r -= rhs; return r; }
    /// Return this vector scaled by a scalar.
    Vec2 operator*(float s)         const { Vec2 r = *this; r *= s;   return r; }
    /// Return this vector divided by a scalar.
    Vec2 operator/(float s)         const { Vec2 r = *this; r /= s;   return r; }
    /// Unary negation — return a vector pointing in the opposite direction.
    Vec2 operator-()                const { return {-x, -y}; }

    // -----------------------------------------------------------------------
    // Comparison
    // -----------------------------------------------------------------------

    /// Exact floating-point equality.  Use with caution — floats accumulate
    /// rounding errors.  For "close enough" equality see LengthSquared trick.
    bool operator==(const Vec2& rhs) const { return x == rhs.x && y == rhs.y; }
    bool operator!=(const Vec2& rhs) const { return !(*this == rhs); }

    // -----------------------------------------------------------------------
    // Geometric helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Return the squared length (magnitude) of the vector.
     *
     * TEACHING NOTE — Why squared length?
     * ──────────────────────────────────────
     * Computing std::sqrt is more expensive than multiplication.  When you
     * only need to COMPARE magnitudes (e.g. "is the enemy within range?"),
     * compare the *squared* lengths instead and avoid the sqrt entirely.
     *
     *   if (delta.LengthSquared() < range * range) { … }
     */
    float LengthSquared() const { return x * x + y * y; }

    /// Return the actual length (Euclidean norm / magnitude) of the vector.
    float Length() const { return std::sqrt(LengthSquared()); }

    /**
     * @brief Return a unit vector (length == 1) in the same direction.
     *
     * TEACHING NOTE — Normalisation
     * ───────────────────────────────
     * A normalised (unit) vector encodes only *direction*, not magnitude.
     * It is the fundamental building block of direction-based calculations:
     * movement, aiming, surface normals, etc.
     *
     * Guard against dividing by zero when the vector is already the zero
     * vector — this returns {0,0} rather than NaN.
     */
    Vec2 Normalized() const {
        float len = Length();
        if (len < 1e-6f) return {0.0f, 0.0f};
        return *this / len;
    }

    /**
     * @brief Compute the dot product with another vector.
     *
     * TEACHING NOTE — Dot Product
     * ────────────────────────────
     * dot(A, B) = |A| * |B| * cos(θ)
     * Where θ is the angle between A and B.
     *
     *  • dot == 1  → vectors point the same direction (unit vectors).
     *  • dot == 0  → vectors are perpendicular.
     *  • dot == -1 → vectors point opposite directions.
     *
     * Extremely useful for: facing checks, projecting one vector onto another,
     * and lighting calculations.
     */
    float Dot(const Vec2& rhs) const { return x * rhs.x + y * rhs.y; }

    /**
     * @brief Return the Euclidean distance to another point.
     * @param other  The other point.
     * @return  Distance (always >= 0).
     */
    float DistanceTo(const Vec2& other) const {
        return (*this - other).Length();
    }

    /**
     * @brief Linearly interpolate between this vector and target.
     *
     * TEACHING NOTE — Linear Interpolation (lerp)
     * ──────────────────────────────────────────────
     * lerp(A, B, t) = A + (B - A) * t
     *
     *   t = 0.0 → returns A
     *   t = 0.5 → returns the midpoint
     *   t = 1.0 → returns B
     *
     * Used everywhere in games: smooth camera movement, animations, colour
     * transitions, and AI pathfinding.
     *
     * @param target  End vector.
     * @param t       Interpolation factor in [0, 1].
     */
    Vec2 Lerp(const Vec2& target, float t) const {
        return *this + (target - *this) * t;
    }

    /// Zero vector constant.
    static constexpr Vec2 Zero()  { return {0.0f, 0.0f}; }
    /// Unit vector pointing right.
    static constexpr Vec2 Right() { return {1.0f, 0.0f}; }
    /// Unit vector pointing up (screen-space Y may be inverted in some APIs).
    static constexpr Vec2 Up()    { return {0.0f, 1.0f}; }
};

/// Allow scalar * Vec2 as well as Vec2 * scalar (commutativity).
inline Vec2 operator*(float s, const Vec2& v) { return v * s; }


// ===========================================================================
// Section 4 — 3D Vector
// ===========================================================================

/**
 * @struct Vec3
 * @brief A three-dimensional floating-point vector.
 *
 * TEACHING NOTE — 3D vs 2D
 * ─────────────────────────
 * The engine uses Vec3 for world-space positions even when the gameplay is
 * largely 2D, because the z-axis controls draw order (depth sorting) and
 * height for jumping/climbing.  This is the same approach taken by many
 * "2.5D" games including older Final Fantasy titles.
 *
 * The operator and helper structure mirrors Vec2 — students should recognise
 * the pattern and understand it generalises to any dimension.
 */
struct Vec3 {
    float x = 0.0f; ///< Left/Right axis.
    float y = 0.0f; ///< Up/Down axis (world-space height).
    float z = 0.0f; ///< Forward/Back axis (depth).

    Vec3() = default;
    constexpr Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    /// Construct a Vec3 from a Vec2 and an explicit z.
    constexpr Vec3(const Vec2& xy, float z = 0.0f) : x(xy.x), y(xy.y), z(z) {}

    // Compound assignment
    Vec3& operator+=(const Vec3& rhs) { x+=rhs.x; y+=rhs.y; z+=rhs.z; return *this; }
    Vec3& operator-=(const Vec3& rhs) { x-=rhs.x; y-=rhs.y; z-=rhs.z; return *this; }
    Vec3& operator*=(float s)         { x*=s;     y*=s;     z*=s;     return *this; }
    Vec3& operator/=(float s)         { x/=s;     y/=s;     z/=s;     return *this; }

    // Binary operators (built from compound-assignment)
    Vec3 operator+(const Vec3& rhs) const { Vec3 r=*this; r+=rhs; return r; }
    Vec3 operator-(const Vec3& rhs) const { Vec3 r=*this; r-=rhs; return r; }
    Vec3 operator*(float s)         const { Vec3 r=*this; r*=s;   return r; }
    Vec3 operator/(float s)         const { Vec3 r=*this; r/=s;   return r; }
    Vec3 operator-()                const { return {-x,-y,-z}; }

    bool operator==(const Vec3& rhs) const { return x==rhs.x && y==rhs.y && z==rhs.z; }
    bool operator!=(const Vec3& rhs) const { return !(*this==rhs); }

    float LengthSquared() const { return x*x + y*y + z*z; }
    float Length()        const { return std::sqrt(LengthSquared()); }

    Vec3 Normalized() const {
        float len = Length();
        if (len < 1e-6f) return {0.0f, 0.0f, 0.0f};
        return *this / len;
    }

    float Dot(const Vec3& rhs) const { return x*rhs.x + y*rhs.y + z*rhs.z; }

    /**
     * @brief Compute the cross product with another vector.
     *
     * TEACHING NOTE — Cross Product
     * ───────────────────────────────
     * cross(A, B) produces a vector perpendicular to both A and B.
     * Its magnitude equals |A|*|B|*sin(θ).
     *
     * Uses:
     *  • Computing surface normals (lighting, collision).
     *  • Determining whether a point is to the left or right of a line.
     *  • Torque and angular physics.
     *
     * The cross product is NOT commutative: cross(A,B) = -cross(B,A).
     */
    Vec3 Cross(const Vec3& rhs) const {
        return {
            y * rhs.z - z * rhs.y,
            z * rhs.x - x * rhs.z,
            x * rhs.y - y * rhs.x
        };
    }

    float DistanceTo(const Vec3& other) const { return (*this - other).Length(); }

    Vec3 Lerp(const Vec3& target, float t) const {
        return *this + (target - *this) * t;
    }

    /// Extract only the XZ plane components (horizontal ground plane).
    Vec2 XZ() const { return {x, z}; }
    /// Extract the XY screen-plane components.
    Vec2 XY() const { return {x, y}; }

    static constexpr Vec3 Zero()    { return {0.0f, 0.0f, 0.0f}; }
    static constexpr Vec3 Up()      { return {0.0f, 1.0f, 0.0f}; }
    static constexpr Vec3 Forward() { return {0.0f, 0.0f, 1.0f}; }
    static constexpr Vec3 Right()   { return {1.0f, 0.0f, 0.0f}; }
};

inline Vec3 operator*(float s, const Vec3& v) { return v * s; }


// ===========================================================================
// Section 5 — RGBA Color
// ===========================================================================

/**
 * @struct Color
 * @brief An RGBA colour stored as four bytes (0–255 per channel).
 *
 * TEACHING NOTE — Colour Representation
 * ────────────────────────────────────────
 * Many rendering APIs (SDL2, OpenGL, Vulkan) accept colours as four unsigned
 * bytes in the range [0, 255].  Storing each channel as a uint8_t uses
 * exactly one byte per channel (4 bytes total) — compact and cache-friendly.
 *
 * If you need high-dynamic-range (HDR) rendering, use float channels [0,1].
 */
struct Color {
    uint8_t r = 255; ///< Red channel   (0 = none, 255 = full).
    uint8_t g = 255; ///< Green channel.
    uint8_t b = 255; ///< Blue channel.
    uint8_t a = 255; ///< Alpha channel (0 = transparent, 255 = opaque).

    Color() = default;
    constexpr Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
        : r(r), g(g), b(b), a(a) {}

    bool operator==(const Color& o) const {
        return r==o.r && g==o.g && b==o.b && a==o.a;
    }

    // -----------------------------------------------------------------------
    // Named colour presets
    // TEACHING NOTE: static constexpr members computed at compile time.
    //   constexpr = "constant expression" — value known at compile time,
    //   stored in read-only memory, zero runtime cost.
    // -----------------------------------------------------------------------
    static constexpr Color White()       { return {255,255,255,255}; }
    static constexpr Color Black()       { return {0,  0,  0,  255}; }
    static constexpr Color Red()         { return {255,0,  0,  255}; }
    static constexpr Color Green()       { return {0,  255,0,  255}; }
    static constexpr Color Blue()        { return {0,  0,  255,255}; }
    static constexpr Color Yellow()      { return {255,255,0,  255}; }
    static constexpr Color Cyan()        { return {0,  255,255,255}; }
    static constexpr Color Magenta()     { return {255,0,  255,255}; }
    static constexpr Color Orange()      { return {255,165,0,  255}; }
    static constexpr Color Purple()      { return {128,0,  128,255}; }
    static constexpr Color Transparent() { return {0,  0,  0,  0  }; }
    /// A warm amber — evokes fire/magic VFX.
    static constexpr Color Amber()       { return {255,191,0,  255}; }
    /// Ice blue — used for frozen / blizzard effects.
    static constexpr Color IceBlue()     { return {173,216,230,255}; }
    /// Electric violet — lightning / shock effects.
    static constexpr Color ElectricViolet(){ return {143,0,  255,255}; }

    /**
     * @brief Linearly interpolate between two colours.
     * @param other  Target colour.
     * @param t      Factor in [0,1] (0 = this, 1 = other).
     */
    Color Lerp(const Color& other, float t) const {
        auto lerp8 = [](uint8_t a, uint8_t b, float t) -> uint8_t {
            return static_cast<uint8_t>(a + (b - a) * t);
        };
        return { lerp8(r, other.r, t),
                 lerp8(g, other.g, t),
                 lerp8(b, other.b, t),
                 lerp8(a, other.a, t) };
    }
};


// ===========================================================================
// Section 6 — Enumerations
// ===========================================================================

// ---------------------------------------------------------------------------
// TEACHING NOTE — Scoped Enums (enum class)
// ---------------------------------------------------------------------------
// C++11 introduced `enum class` (also called "scoped enum").
// Prefer it over plain `enum` for two reasons:
//
//  1. No implicit integer conversion:  NORTH == 0 is a compile error with
//     `enum class Direction`, preventing subtle bugs where you accidentally
//     compare an EntityID to a Direction value.
//
//  2. No name pollution:  with a plain `enum`, NORTH, SOUTH etc. are injected
//     directly into the enclosing namespace.  With `enum class Direction`,
//     you must write Direction::NORTH, which avoids name clashes.
// ---------------------------------------------------------------------------

/**
 * @enum Direction
 * @brief Cardinal movement directions on the game map.
 *
 * Used by the movement system, pathfinding, and tilemap neighbour lookups.
 */
enum class Direction : uint8_t {
    NORTH = 0, ///< Toward negative Z (or "up" on a top-down map).
    SOUTH,     ///< Toward positive Z.
    EAST,      ///< Toward positive X.
    WEST,      ///< Toward negative X.
    NONE       ///< No movement / undecided.
};

/**
 * @enum GameState
 * @brief High-level states the game can be in at any moment.
 *
 * TEACHING NOTE — State Machine Basics
 * ──────────────────────────────────────
 * Almost every game is implemented as a *state machine*.  At each moment the
 * game is in exactly one state; transitions happen on input or events.
 *
 * For example:
 *   EXPLORING → player starts combat → COMBAT
 *   COMBAT    → all enemies dead     → EXPLORING
 *   EXPLORING → player opens menu    → INVENTORY
 *
 * This enum labels each state.  A GameStateManager class (not in this file)
 * would hold the active state and handle transitions.
 */
enum class GameState : uint8_t {
    MAIN_MENU    = 0, ///< Title / start screen.
    EXPLORING,        ///< Open-world free movement (the default play state).
    COMBAT,           ///< Real-time action combat encounter.
    DIALOGUE,         ///< NPC conversation / story cutscene.
    INVENTORY,        ///< Item management screen.
    SHOP,             ///< Buying/selling items with a vendor.
    CAMP,             ///< Camp rest screen (save, cook, level up).
    VEHICLE,          ///< Driving the Regalia / other vehicle.
    GAME_OVER,        ///< Player party was wiped.
    VICTORY,          ///< Boss defeated / chapter complete.
    WORLD_EDITOR      ///< In-engine level editor mode (dev tool).
};

/**
 * @enum TimeOfDay
 * @brief Discrete time-of-day periods for the dynamic day/night cycle.
 *
 * Inspired by FF15's day/night system where enemies and events change based
 * on time.  The world renderer uses this to tint the sky and adjust lighting.
 * The AI system spawns nocturnal enemies only during NIGHT / MIDNIGHT.
 */
enum class TimeOfDay : uint8_t {
    DAWN        = 0, ///< ~05:00 — twilight, dim orange horizon.
    MORNING,         ///< ~08:00 — full daylight begins.
    NOON,            ///< ~12:00 — brightest light, short shadows.
    AFTERNOON,       ///< ~15:00 — warm golden light.
    EVENING,         ///< ~18:00 — sunset, long shadows.
    DUSK,            ///< ~20:00 — twilight, stars appear.
    NIGHT,           ///< ~22:00 — darkness, nocturnal daemons spawn.
    MIDNIGHT         ///< ~00:00 — peak darkness, most dangerous time.
};

/**
 * @enum WeatherType
 * @brief Environmental weather conditions.
 *
 * Weather affects gameplay (rain reduces fire magic effectiveness), particle
 * systems (rain splashes, fog particles), and AI visibility ranges.
 */
enum class WeatherType : uint8_t {
    CLEAR  = 0, ///< Blue sky, full visibility.
    CLOUDY,     ///< Overcast, slightly reduced light.
    RAIN,       ///< Rain particles, puddles, reduced fire damage by 25%.
    STORM,      ///< Heavy rain + lightning; +25% lightning damage.
    FOG         ///< Thick fog; enemy detection range halved.
};

/**
 * @enum ElementType
 * @brief Elemental affinities for magic spells and enemy weaknesses.
 *
 * TEACHING NOTE — Elemental Systems
 * ───────────────────────────────────
 * Classic JRPGs use elemental typing to add strategic depth: you learn
 * enemy weaknesses and choose spells/equipment accordingly.  This engine
 * follows the FF tradition with three elemental types plus a "no element"
 * option for neutral/physical damage.
 *
 * The CombatSystem calculates a damage multiplier based on the attacker's
 * element versus the defender's resistances (stored in StatsComponent).
 */
enum class ElementType : uint8_t {
    NONE      = 0, ///< No element — neutral; no weakness/resistance applied.
    FIRE,          ///< Fire — effective vs. ice enemies; reduced by rain.
    ICE,           ///< Ice — effective vs. fire enemies; can inflict FROZEN.
    LIGHTNING      ///< Lightning — effective vs. water/metal; can inflict SHOCKED.
};

/**
 * @enum StatusEffect
 * @brief Debuff / ailment conditions that can be applied to entities.
 *
 * Status effects are stored as a bitmask inside StatusEffectsComponent so
 * multiple effects can be active simultaneously.
 *
 * TEACHING NOTE — Bitmasks
 * ──────────────────────────
 * A bitmask stores multiple boolean flags packed into a single integer.
 * Each flag occupies one bit.  Example:
 *
 *   POISON  = 0b00000001 = 1
 *   SLEEP   = 0b00000010 = 2
 *   Both    = 0b00000011 = 3
 *
 * Check if POISON is active:  (flags & POISON) != 0
 * Remove SLEEP:                flags &= ~SLEEP
 *
 * The enum values below are powers of two so they can be OR'd together.
 */
enum class StatusEffect : uint8_t {
    NONE    = 0,          ///< No status effect active.
    POISON  = 1 << 0,     ///< Lose HP every tick.  Cured by Antidote.
    SLEEP   = 1 << 1,     ///< Cannot act until hit.  Cured by being damaged.
    STOP    = 1 << 2,     ///< Completely frozen in time.  Timed duration.
    BLIND   = 1 << 3,     ///< Physical attack accuracy halved.
    BURNED  = 1 << 4,     ///< Ongoing fire DoT.  Fire element inflicts this.
    FROZEN  = 1 << 5,     ///< Cannot move; ice element inflicts this.
    SHOCKED = 1 << 6      ///< Stunned intermittently; lightning inflicts this.
};

/// Bitwise OR for combining status effects.
inline StatusEffect operator|(StatusEffect a, StatusEffect b) {
    return static_cast<StatusEffect>(
        static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

/// Bitwise AND for checking if a specific effect is present.
inline StatusEffect operator&(StatusEffect a, StatusEffect b) {
    return static_cast<StatusEffect>(
        static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

/// Bitwise NOT for removing a status effect.
inline StatusEffect operator~(StatusEffect a) {
    return static_cast<StatusEffect>(~static_cast<uint8_t>(a));
}

/// Compound OR-assignment.
inline StatusEffect& operator|=(StatusEffect& a, StatusEffect b) {
    a = a | b; return a;
}

/// Compound AND-assignment.
inline StatusEffect& operator&=(StatusEffect& a, StatusEffect b) {
    a = a & b; return a;
}

/**
 * @brief Convenience: check whether a particular StatusEffect flag is set.
 * @param flags   The bitmask of active effects.
 * @param effect  The flag to test.
 * @return true if the effect is active.
 *
 * Usage:
 * @code
 *   if (HasStatus(entity.status, StatusEffect::POISON)) { dealPoisonDamage(); }
 * @endcode
 */
inline bool HasStatus(StatusEffect flags, StatusEffect effect) {
    return (flags & effect) != StatusEffect::NONE;
}

/**
 * @enum DamageType
 * @brief Classification of damage sources — affects mitigation calculations.
 *
 * TEACHING NOTE — Damage Formulas
 * ─────────────────────────────────
 * Separating damage into types enables fine-grained tuning:
 *
 *   PHYSICAL   → reduced by Defence stat (armour, shields).
 *   MAGICAL    → reduced by Spirit / Magic Defence stat (elemental wards).
 *   TRUE_DAMAGE → bypasses ALL defences; used for fall damage, story kills,
 *                 and some boss mechanics.  Use sparingly — players dislike
 *                 unavoidable damage they cannot mitigate.
 */
enum class DamageType : uint8_t {
    PHYSICAL    = 0, ///< Swords, bows, physical attacks.
    MAGICAL,         ///< Spells, elemental attacks.
    TRUE_DAMAGE      ///< Pierces all defences.
};


// ===========================================================================
// Section 7 — Compound Structs
// ===========================================================================

/**
 * @struct Rect
 * @brief An axis-aligned rectangle defined by top-left corner and dimensions.
 *
 * Used for: UI layout, collision AABB (axis-aligned bounding box), screen
 * regions, and texture source rectangles.
 */
struct Rect {
    float x      = 0.0f; ///< Left edge X.
    float y      = 0.0f; ///< Top edge Y.
    float width  = 0.0f; ///< Width in pixels / world units.
    float height = 0.0f; ///< Height in pixels / world units.

    Rect() = default;
    constexpr Rect(float x, float y, float w, float h)
        : x(x), y(y), width(w), height(h) {}

    /// Right edge X.
    float Right()  const { return x + width;  }
    /// Bottom edge Y.
    float Bottom() const { return y + height; }

    /// Centre of the rectangle.
    Vec2 Centre() const { return {x + width * 0.5f, y + height * 0.5f}; }

    /**
     * @brief Test whether two rectangles overlap (AABB collision).
     *
     * TEACHING NOTE — AABB Intersection
     * ────────────────────────────────────
     * Two rectangles A and B are NOT overlapping if one is entirely to the
     * left, right, above, or below the other.  Negate that condition to get
     * the overlap test.  This is the classic "separating axis" check for
     * axis-aligned boxes.
     */
    bool Intersects(const Rect& other) const {
        return !(other.x > Right()       ||
                 other.Right() < x       ||
                 other.y > Bottom()      ||
                 other.Bottom() < y);
    }

    /// True if the point (px, py) is inside this rectangle.
    bool Contains(float px, float py) const {
        return px >= x && px <= Right() && py >= y && py <= Bottom();
    }

    bool Contains(const Vec2& p) const { return Contains(p.x, p.y); }
};

/**
 * @struct TileCoord
 * @brief Integer grid coordinates for tilemap-based world navigation.
 *
 * The world map is divided into tiles (like a chess board).  TileCoord stores
 * which tile an entity occupies.  Converting to world space:
 *   worldPos = Vec3(tileX * TILE_SIZE, 0, tileY * TILE_SIZE)
 */
struct TileCoord {
    int32_t tileX = 0; ///< Column index (left = 0).
    int32_t tileY = 0; ///< Row index (top = 0).

    TileCoord() = default;
    constexpr TileCoord(int32_t x, int32_t y) : tileX(x), tileY(y) {}

    bool operator==(const TileCoord& o) const {
        return tileX == o.tileX && tileY == o.tileY;
    }
    bool operator!=(const TileCoord& o) const { return !(*this == o); }

    /// Manhattan distance — useful for simple A* heuristics.
    int32_t ManhattanDistance(const TileCoord& other) const {
        return std::abs(tileX - other.tileX) + std::abs(tileY - other.tileY);
    }

    /// Return the neighbour tile in a given Direction.
    TileCoord Neighbour(Direction dir) const {
        switch (dir) {
            case Direction::NORTH: return {tileX,     tileY - 1};
            case Direction::SOUTH: return {tileX,     tileY + 1};
            case Direction::EAST:  return {tileX + 1, tileY    };
            case Direction::WEST:  return {tileX - 1, tileY    };
            default:               return *this;
        }
    }
};

/**
 * @struct Range
 * @brief A closed interval [min, max] for floating-point values.
 *
 * Used for stat ranges, random number generation bounds, animation frame
 * ranges, and camera zoom limits.
 */
struct Range {
    float min = 0.0f; ///< Inclusive lower bound.
    float max = 1.0f; ///< Inclusive upper bound.

    Range() = default;
    constexpr Range(float min, float max) : min(min), max(max) {}

    /// True if the value falls within [min, max].
    bool Contains(float v) const { return v >= min && v <= max; }

    /// Clamp a value to [min, max].
    float Clamp(float v) const {
        if (v < min) return min;
        if (v > max) return max;
        return v;
    }

    /// Total span of the range.
    float Span() const { return max - min; }
};

/**
 * @struct WorldInfo
 * @brief Aggregated information about the current world/level state.
 *
 * Passed to rendering and audio subsystems every frame so they can react to
 * the current time-of-day, weather, and active game state.
 */
struct WorldInfo {
    TimeOfDay   timeOfDay    = TimeOfDay::NOON;       ///< Current time period.
    WeatherType weather      = WeatherType::CLEAR;    ///< Current weather.
    GameState   currentState = GameState::EXPLORING;  ///< Active game state.
    float       worldTime    = 0.0f;  ///< Seconds elapsed since world load.
    uint32_t    dayCount     = 1;     ///< In-game days elapsed (starts at 1).
    std::string regionName;           ///< Name of the current region/zone.

    /**
     * @brief Convert float worldTime to HH:MM string for the HUD clock.
     * @return Formatted string e.g. "14:35".
     */
    std::string GetTimeString() const {
        // 1 in-game day = 24 * 60 real seconds (configurable).
        constexpr float SECONDS_PER_DAY = 1440.0f;
        float dayFraction = std::fmod(worldTime, SECONDS_PER_DAY) / SECONDS_PER_DAY;
        int   hours       = static_cast<int>(dayFraction * 24.0f);
        int   minutes     = static_cast<int>((dayFraction * 24.0f - hours) * 60.0f);
        // Build "HH:MM" — pad with leading zero if needed.
        char buf[6];
        std::snprintf(buf, sizeof(buf), "%02d:%02d", hours, minutes);
        return buf;
    }
};


// ===========================================================================
// Section 8 — Engine-wide size / scale constants
// ===========================================================================

/**
 * @defgroup EngineConstants Engine Scale Constants
 * @{
 *
 * TEACHING NOTE — Magic Numbers
 * ───────────────────────────────
 * Avoid "magic numbers" scattered through the codebase (e.g. writing 64.0f
 * in 20 different files for tile size).  Centralise them here.  If you need
 * to change the tile size, change ONE constant and recompile.
 */

/// World-space width and depth of one map tile (in arbitrary world units).
constexpr float TILE_SIZE        = 64.0f;

/// Default gravity acceleration (world units per second squared, pointing -Y).
constexpr float GRAVITY          = -9.8f;

/// Number of experience points per level (simplified linear scaling).
constexpr uint32_t XP_PER_LEVEL  = 1000u;

/// Maximum character level.
constexpr uint32_t MAX_LEVEL      = 120u;

/// Maximum inventory slots per entity.
constexpr uint32_t MAX_INV_SLOTS  = 99u;

/// Maximum number of party members that can be in the active party.
constexpr uint32_t MAX_PARTY_SIZE = 4u;

/// Maximum number of active quests tracked simultaneously.
constexpr uint32_t MAX_QUESTS     = 32u;

/// Target frames per second (used for fixed-timestep calculations).
constexpr float TARGET_FPS        = 60.0f;

/// Fixed timestep in seconds: 1/60.
constexpr float FIXED_DELTA       = 1.0f / TARGET_FPS;

/** @} */ // end of EngineConstants
