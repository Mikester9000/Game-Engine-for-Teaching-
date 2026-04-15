/**
 * @file WeatherSystem.hpp
 * @brief Day/night cycle and weather simulation for the educational RPG engine.
 *
 * ============================================================================
 * TEACHING NOTE — Time and Weather Systems
 * ============================================================================
 *
 * An in-game clock serves multiple gameplay purposes:
 *
 *  1. ATMOSPHERE  — Lighting changes; dawn is orange, midnight is deep blue.
 *  2. ENEMY SPAWNS — Nocturnal enemies (daemons in FF15) appear at night.
 *  3. NPC SCHEDULES — Shops close at night; NPCs go home.
 *  4. QUEST TRIGGERS — "Meet at the fountain at noon."
 *  5. WEATHER EFFECTS — Rain reduces fire spell damage; fog reduces visibility.
 *
 * ─── Time Representation ────────────────────────────────────────────────────
 *
 * We store game time as a single float: `totalGameSeconds`.
 *
 *   Time 0.0 = 06:00 on Day 1 (dawn).
 *   A full day = 86,400 GAME seconds.
 *
 * But 86,400 real seconds is 24 real hours — too slow.  We compress time:
 *   Real 1 minute = Game 1 hour  (60× compression)
 *   So a full game day = 24 real minutes.
 *
 * Implementation:
 *   totalGameSeconds += dt × TIME_SCALE   (TIME_SCALE = 60.0f)
 *
 * To find current game hour:
 *   float secondsToday = fmod(totalGameSeconds, SECONDS_PER_DAY);
 *   float gameHour     = 6.0f + secondsToday / 3600.0f;  // starts at 6am
 *
 * ─── TimeOfDay Periods ───────────────────────────────────────────────────────
 *
 * We divide the 24-hour cycle into named periods (from Types.hpp):
 *
 *   DAWN       06:00 - 07:59
 *   MORNING    08:00 - 11:59
 *   NOON       12:00 - 13:59
 *   AFTERNOON  14:00 - 17:59
 *   EVENING    18:00 - 19:59
 *   NIGHT      20:00 - 23:59
 *   MIDNIGHT   00:00 - 05:59
 *
 * A WorldEvent::TIME_CHANGED is published when the period transitions.
 *
 * ─── Weather State Machine ───────────────────────────────────────────────────
 *
 * Weather transitions are probabilistic and time-of-day dependent:
 *
 *   - CLEAR:  Can transition to CLOUDY (20%), RAIN (5%), FOG (5%) per hour.
 *   - CLOUDY: Can transition to RAIN (30%), CLEAR (15%), STORM (10%) per hour.
 *   - RAIN:  Can transition to CLEAR (10%), STORM (20%) per hour.
 *   - STORM:  Can transition to RAIN (40%) per hour.
 *   - FOG:  Can transition to CLEAR (25%) per hour.
 *   - SNOWY:  Appears only in high-altitude zones (set via SetWeather directly).
 *
 * Weather changes also fire WorldEvent::WEATHER_CHANGED.
 *
 * ─── Effect on Gameplay ──────────────────────────────────────────────────────
 *
 * The WeatherSystem itself only tracks state.  Other systems RESPOND to the
 * WorldEvent broadcast:
 *   - Renderer: adjusts tile colour tinting.
 *   - CombatSystem: applies weather damage multipliers.
 *   - AISystem: increases enemy aggro range in fog (limited visibility).
 *   - SpawnSystem: activates daemon spawns at night.
 *
 * This demonstrates the power of the event-bus pattern for decoupled reactions
 * to a single state change.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 *
 * C++ Standard: C++17
 */

#pragma once

#include <string>
#include <cstdint>
#include <cmath>       // std::fmod
#include <random>      // std::mt19937

#include "../../engine/core/Types.hpp"
#include "../../engine/core/Logger.hpp"
#include "../../engine/core/EventBus.hpp"


// ===========================================================================
// WeatherSystem class
// ===========================================================================

/**
 * @class WeatherSystem
 * @brief Simulates a compressed day/night cycle and probabilistic weather.
 *
 * USAGE EXAMPLE:
 * @code
 *   WeatherSystem weather(&EventBus<WorldEvent>::Instance());
 *
 *   // Inside game loop:
 *   weather.Update(deltaTime);
 *
 *   // Query:
 *   std::string t = weather.GetTimeString();   // "14:30"
 *   TimeOfDay   tod = weather.GetCurrentTime(); // TimeOfDay::AFTERNOON
 *   WeatherType w   = weather.GetCurrentWeather(); // WeatherType::CLOUDY
 * @endcode
 */
class WeatherSystem {
public:

    // -----------------------------------------------------------------------
    // Time scale constants
    // -----------------------------------------------------------------------

    /// Number of REAL seconds that represent one GAME day.
    /// 24 × 60 = 1440 real seconds = 24 real minutes.
    static constexpr float DAY_LENGTH_SECONDS = 24.0f * 60.0f;

    /// Number of GAME seconds in one game day (always 86400 game-seconds).
    static constexpr float GAME_SECONDS_PER_DAY = 86400.0f;

    /// Real-to-game time multiplier: 1 real second = TIME_SCALE game seconds.
    static constexpr float TIME_SCALE = GAME_SECONDS_PER_DAY / DAY_LENGTH_SECONDS;

    /// Dawn starts at 06:00 → 6 game hours = 6×3600 game seconds from midnight.
    static constexpr float DAWN_OFFSET_SECONDS = 6.0f * 3600.0f;

    /// How often (real seconds) weather may transition.
    static constexpr float WEATHER_CHECK_INTERVAL = 60.0f; // every real minute

    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------

    /**
     * @brief Construct the WeatherSystem.
     *
     * @param worldBus  Non-owning pointer to the WorldEvent bus.
     *                  Pass nullptr to disable event broadcasting (useful in
     *                  unit tests).
     */
    explicit WeatherSystem(EventBus<WorldEvent>* worldBus);

    // -----------------------------------------------------------------------
    // Update
    // -----------------------------------------------------------------------

    /**
     * @brief Advance the game clock and check for weather transitions.
     *
     * Called every game frame with the real delta time.
     *
     * Internally:
     *   1. totalGameSeconds += dt × TIME_SCALE.
     *   2. Derive new TimeOfDay period; if changed, fire WorldEvent::TIME_CHANGED.
     *   3. Accumulate m_weatherTimer; when >= WEATHER_CHECK_INTERVAL, run the
     *      probabilistic transition and reset the timer.
     *
     * @param dt  Real delta time in seconds since last frame.
     *
     * TEACHING NOTE — Floating Point Accumulation
     * ─────────────────────────────────────────────
     * Adding `dt` to a float every frame accumulates small rounding errors.
     * For game clocks this is acceptable — a few milliseconds of drift per
     * day is invisible to players.  For financial or scientific simulations,
     * use double or a fixed-point integer representation.
     */
    void Update(float dt);

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------

    /**
     * @brief Return the current named time period.
     *
     * Derived from totalGameSeconds via GetGameHour().
     */
    TimeOfDay GetCurrentTime() const;

    /// @return Current weather condition.
    WeatherType GetCurrentWeather() const { return m_currentWeather; }

    /**
     * @brief Return the current game time as a human-readable "HH:MM" string.
     *
     * Example: 6.5 game hours = "06:30"
     *
     * TEACHING NOTE — String Formatting in C++17
     * ────────────────────────────────────────────
     * We use std::to_string and manual zero-padding.  In C++20 you can use
     * std::format("{:02d}:{:02d}", hours, minutes).  For C++17 compatibility
     * we build the string manually with conditional zero-padding.
     */
    std::string GetTimeString() const;

    /**
     * @brief Return the current in-game day number (starts at 1).
     *
     * Day 1 = from time 0.0 to DAY_LENGTH_SECONDS.
     * Day 2 = from DAY_LENGTH_SECONDS to 2×DAY_LENGTH_SECONDS.  Etc.
     */
    int GetDayNumber() const;

    // -----------------------------------------------------------------------
    // Manipulation
    // -----------------------------------------------------------------------

    /**
     * @brief Skip time forward by `hours` game hours.
     *
     * Equivalent to resting or fast-travelling.  Triggers time-period
     * transitions and weather checks for each hour passed.
     *
     * @param hours  Number of game hours to advance (may cross midnight).
     */
    void AdvanceTime(float hours);

    /**
     * @brief Forcibly set the weather to a specific condition.
     *
     * Used by the Zone system to set weather appropriate for a region
     * (e.g. always SNOWY in the Glaive Mountains).
     * Fires WorldEvent::WEATHER_CHANGED.
     *
     * @param weather  New weather state.
     */
    void SetWeather(WeatherType weather);

    /**
     * @brief Return the current fractional game hour (0.0–23.999).
     *
     * 0.0 = midnight, 6.0 = 6am, 12.5 = 12:30pm, 23.75 = 11:45pm.
     */
    float GetGameHour() const;

    // -----------------------------------------------------------------------
    // Public data (read-only concept)
    // -----------------------------------------------------------------------

    /// Total accumulated real-scaled game time in game seconds.
    /// Starting from day 1 dawn (06:00).
    float totalGameSeconds = 0.0f;

    /// Length of a full game day in real seconds (default: 1440 = 24 min).
    float dayLengthSeconds = DAY_LENGTH_SECONDS;

private:

    // -----------------------------------------------------------------------
    // Private helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Run a probabilistic weather transition for the current conditions.
     *
     * Rolls a random float in [0, 1) and compares against transition thresholds
     * for the current weather type.  May change m_currentWeather.
     * Fires WorldEvent::WEATHER_CHANGED if the weather changes.
     *
     * TEACHING NOTE — Probabilistic State Machines
     * ──────────────────────────────────────────────
     * Instead of hard rules ("rain always follows clouds"), we use
     * probabilities.  This gives organic-feeling variation:
     *
     *   roll = uniform_real(0, 1)
     *   if roll < 0.20: transition to CLOUDY
     *   elif roll < 0.25: transition to RAIN
     *   elif roll < 0.30: transition to FOG
     *   else: stay CLEAR
     *
     * Adjusting the probabilities is the core of weather "feel" tuning.
     */
    void TryWeatherTransition();

    /**
     * @brief Publish a WorldEvent::TIME_CHANGED event.
     *
     * @param newPeriod  The TimeOfDay period that was just entered.
     */
    void BroadcastTimeChange(TimeOfDay newPeriod) const;

    /**
     * @brief Publish a WorldEvent::WEATHER_CHANGED event.
     *
     * @param newWeather  The WeatherType that just became active.
     */
    void BroadcastWeatherChange(WeatherType newWeather) const;

    // -----------------------------------------------------------------------
    // Member data
    // -----------------------------------------------------------------------

    WeatherType           m_currentWeather  = WeatherType::CLEAR;
    TimeOfDay             m_lastTimePeriod  = TimeOfDay::DAWN;
    float                 m_weatherTimer    = 0.0f;   ///< Accumulates real dt.

    EventBus<WorldEvent>* m_worldBus        = nullptr;

    mutable std::mt19937  m_rng{12345};  ///< RNG for weather transitions.
};
