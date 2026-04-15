/**
 * @file WeatherSystem.cpp
 * @brief Implementation of the day/night cycle and weather simulation.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 */

#include "WeatherSystem.hpp"

#include <cmath>     // std::fmod
#include <sstream>   // std::ostringstream
#include <iomanip>   // std::setfill, std::setw

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

WeatherSystem::WeatherSystem(EventBus<WorldEvent>* worldBus)
    : m_worldBus(worldBus)
{
    // Start at 06:00 on day 1 (dawn).
    totalGameSeconds = 0.0f;
    m_lastTimePeriod = TimeOfDay::DAWN;

    LOG_INFO("WeatherSystem initialised. Day starts at DAWN.");
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void WeatherSystem::Update(float dt)
{
    // 1. Advance game time.
    // TEACHING NOTE — TIME_SCALE converts real seconds to game seconds.
    // With TIME_SCALE = 60, 1 real second = 1 game minute.
    totalGameSeconds += dt * TIME_SCALE;

    // 2. Check if the time-of-day period changed.
    TimeOfDay current = GetCurrentTime();
    if (current != m_lastTimePeriod) {
        BroadcastTimeChange(current);
        m_lastTimePeriod = current;
    }

    // 3. Accumulate weather timer; check for transitions periodically.
    m_weatherTimer += dt;
    if (m_weatherTimer >= WEATHER_CHECK_INTERVAL) {
        m_weatherTimer = 0.0f;
        TryWeatherTransition();
    }
}

// ---------------------------------------------------------------------------
// GetCurrentTime
// ---------------------------------------------------------------------------

TimeOfDay WeatherSystem::GetCurrentTime() const
{
    float hour = GetGameHour();

    // TEACHING NOTE — We map 24-hour clock to 7 named periods.
    // Thresholds mirror when FF15 characters comment on the time.
    if (hour >= 5.0f  && hour < 8.0f)   return TimeOfDay::DAWN;
    if (hour >= 8.0f  && hour < 12.0f)  return TimeOfDay::MORNING;
    if (hour >= 12.0f && hour < 15.0f)  return TimeOfDay::NOON;
    if (hour >= 15.0f && hour < 18.0f)  return TimeOfDay::AFTERNOON;
    if (hour >= 18.0f && hour < 22.0f)  return TimeOfDay::EVENING;
    if (hour >= 22.0f && hour < 24.0f)  return TimeOfDay::NIGHT;
    // 0:00–5:00
    return TimeOfDay::MIDNIGHT;
}

// ---------------------------------------------------------------------------
// GetTimeString
// ---------------------------------------------------------------------------

std::string WeatherSystem::GetTimeString() const
{
    float hour = GetGameHour();
    int h = static_cast<int>(hour);
    int m = static_cast<int>((hour - h) * 60.0f);

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << h
        << ":"
        << std::setfill('0') << std::setw(2) << m;
    return oss.str();
}

// ---------------------------------------------------------------------------
// GetDayNumber
// ---------------------------------------------------------------------------

int WeatherSystem::GetDayNumber() const
{
    return 1 + static_cast<int>(totalGameSeconds / GAME_SECONDS_PER_DAY);
}

// ---------------------------------------------------------------------------
// GetGameHour
// ---------------------------------------------------------------------------

float WeatherSystem::GetGameHour() const
{
    float secondsToday = std::fmod(totalGameSeconds, GAME_SECONDS_PER_DAY);
    // Offset so that totalGameSeconds=0 maps to 06:00 (dawn start).
    float adjusted = secondsToday + DAWN_OFFSET_SECONDS;
    if (adjusted >= GAME_SECONDS_PER_DAY) adjusted -= GAME_SECONDS_PER_DAY;
    return adjusted / 3600.0f;
}

// ---------------------------------------------------------------------------
// AdvanceTime
// ---------------------------------------------------------------------------

void WeatherSystem::AdvanceTime(float hours)
{
    // TEACHING NOTE — We convert hours to game-seconds and add directly.
    // The next Update() will detect the period change and broadcast the event.
    totalGameSeconds += hours * 3600.0f;
    LOG_INFO("Time advanced by " + std::to_string(hours) + " hours. " +
             "New time: " + GetTimeString());
}

// ---------------------------------------------------------------------------
// SetWeather
// ---------------------------------------------------------------------------

void WeatherSystem::SetWeather(WeatherType weather)
{
    if (m_currentWeather == weather) return;
    m_currentWeather = weather;
    BroadcastWeatherChange(weather);
}

// ---------------------------------------------------------------------------
// TryWeatherTransition (private)
// ---------------------------------------------------------------------------

void WeatherSystem::TryWeatherTransition()
{
    // TEACHING NOTE — Probabilistic weather state machine.
    // Each weather type has different transition probabilities.
    // We roll once per WEATHER_CHECK_INTERVAL seconds.
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    float roll = dist(m_rng);

    TimeOfDay period = GetCurrentTime();

    // Night and midnight raise rain and fog chances.
    bool isNight = (period == TimeOfDay::NIGHT || period == TimeOfDay::MIDNIGHT);

    WeatherType next = m_currentWeather;

    switch (m_currentWeather) {
        case WeatherType::CLEAR:
            if      (roll < (isNight ? 0.25f : 0.15f)) next = WeatherType::CLOUDY;
            else if (roll < (isNight ? 0.30f : 0.18f)) next = WeatherType::FOGGY;
            break;

        case WeatherType::CLOUDY:
            if      (roll < 0.30f) next = WeatherType::RAINY;
            else if (roll < 0.40f) next = WeatherType::CLEAR;
            else if (roll < 0.45f) next = WeatherType::STORMY;
            break;

        case WeatherType::RAINY:
            if      (roll < 0.20f) next = WeatherType::CLEAR;
            else if (roll < 0.35f) next = WeatherType::CLOUDY;
            else if (roll < 0.40f) next = WeatherType::STORMY;
            break;

        case WeatherType::FOGGY:
            if      (roll < 0.30f) next = WeatherType::CLEAR;
            else if (roll < 0.40f) next = WeatherType::CLOUDY;
            break;

        case WeatherType::STORMY:
            if      (roll < 0.20f) next = WeatherType::RAINY;
            else if (roll < 0.25f) next = WeatherType::CLOUDY;
            break;

        default:
            if (roll < 0.20f) next = WeatherType::CLEAR;
            break;
    }

    if (next != m_currentWeather) {
        m_currentWeather = next;
        BroadcastWeatherChange(next);
    }
}

// ---------------------------------------------------------------------------
// BroadcastTimeChange (private)
// ---------------------------------------------------------------------------

void WeatherSystem::BroadcastTimeChange(TimeOfDay newPeriod) const
{
    if (!m_worldBus) return;
    WorldEvent ev;
    ev.type    = WorldEvent::Type::TIME_CHANGED;
    ev.timeOfDay = newPeriod;
    ev.weather   = m_currentWeather;
    m_worldBus->Publish(ev);
    LOG_DEBUG("Time changed to period " + std::to_string(static_cast<int>(newPeriod)));
}

// ---------------------------------------------------------------------------
// BroadcastWeatherChange (private)
// ---------------------------------------------------------------------------

void WeatherSystem::BroadcastWeatherChange(WeatherType newWeather) const
{
    if (!m_worldBus) return;
    WorldEvent ev;
    ev.type    = WorldEvent::Type::WEATHER_CHANGED;
    ev.timeOfDay = GetCurrentTime();
    ev.weather   = newWeather;
    m_worldBus->Publish(ev);
    LOG_DEBUG("Weather changed to type " + std::to_string(static_cast<int>(newWeather)));
}
