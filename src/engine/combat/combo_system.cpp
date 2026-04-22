/**
 * @file combo_system.cpp
 * @brief Implementation of the ComboSystem FSM.
 *
 * See combo_system.hpp for detailed teaching notes on the FSM design and
 * data-driven combo loading.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 */

#include "combo_system.hpp"

// TEACHING NOTE — ENGINE_ENABLE_JSON Guard
// The nlohmann/json header is available only when the vcpkg toolchain is
// active and nlohmann-json is installed.  This matches the pattern used by
// save_system.cpp and game_streaming_manager.cpp — compiling cleanly both
// with and without the library.
#ifdef ENGINE_ENABLE_JSON
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#endif

#include <algorithm>  // std::equal, std::transform
#include <cctype>     // std::toupper
#include <iostream>   // std::cerr (fallback error logging without Logger)

// ===========================================================================
// Public API — Configuration
// ===========================================================================

bool ComboSystem::LoadConfig(const std::string& configPath)
{
#ifdef ENGINE_ENABLE_JSON
    // TEACHING NOTE — File I/O with error propagation
    // We open the file in binary mode and slurp it into a string.  This is
    // simpler than streaming and fine for small config files (< 1 MB).
    std::ifstream ifs(configPath, std::ios::binary);
    if (!ifs.is_open())
    {
        std::cerr << "[ComboSystem] LoadConfig: cannot open '" << configPath << "'.\n";
        return false;
    }

    std::ostringstream buf;
    buf << ifs.rdbuf();
    const std::string jsonStr = buf.str();

    nlohmann::json j;
    try
    {
        j = nlohmann::json::parse(jsonStr);
    }
    catch (const nlohmann::json::exception& ex)
    {
        std::cerr << "[ComboSystem] LoadConfig: JSON parse error — " << ex.what() << "\n";
        return false;
    }

    // ── Scalar combat config ────────────────────────────────────────────────
    // TEACHING NOTE — .value("key", default) pattern
    // nlohmann::json's value() member returns the field if present, or the
    // default if the key is missing.  This makes the loader robust against
    // partial config files during development.
    m_config.comboWindowSeconds    = j.value("comboWindowSeconds",    m_config.comboWindowSeconds);
    m_config.turnDurationSeconds   = j.value("turnDurationSeconds",   m_config.turnDurationSeconds);
    m_config.critMultiplier        = j.value("critMultiplier",        m_config.critMultiplier);
    m_config.fleeBaseChance        = j.value("fleeBaseChance",        m_config.fleeBaseChance);
    m_config.fleeChanceCap         = j.value("fleeChanceCap",         m_config.fleeChanceCap);
    m_config.linkStrikeChance      = j.value("linkStrikeChance",      m_config.linkStrikeChance);
    m_config.linkStrikeDamageRatio = j.value("linkStrikeDamageRatio", m_config.linkStrikeDamageRatio);

    // ── Combo definitions ───────────────────────────────────────────────────
    m_combos.clear();

    if (j.contains("combos") && j["combos"].is_array())
    {
        for (const auto& entry : j["combos"])
        {
            ComboDefinition def;
            def.name             = entry.value("name",             std::string{});
            def.damageMultiplier = entry.value("damageMultiplier", 1.0f);
            def.mpCost           = entry.value("mpCost",           0);
            def.cooldownSeconds  = entry.value("cooldownSeconds",  1.0f);
            def.element          = entry.value("element",          std::string{"physical"});

            // Parse the sequence array: ["ATTACK", "ATTACK", "ATTACK"]
            if (entry.contains("sequence") && entry["sequence"].is_array())
            {
                for (const auto& token : entry["sequence"])
                {
                    const std::string tok = token.get<std::string>();
                    ComboInput inp;
                    if (ParseInputToken(tok, inp))
                    {
                        def.sequence.push_back(inp);
                    }
                    else
                    {
                        std::cerr << "[ComboSystem] LoadConfig: unknown input token '"
                                  << tok << "' in combo '" << def.name << "'.\n";
                    }
                }
            }

            if (!def.name.empty() && !def.sequence.empty())
            {
                m_combos.push_back(std::move(def));
            }
        }
    }

    return true;

#else
    // TEACHING NOTE — Graceful degradation when nlohmann/json is absent
    // On CI builds that use the "engine-only" CMake preset (no vcpkg), the
    // JSON parsing library is not linked.  We fall through to false so the
    // caller knows to populate combos via AddCombo() instead.
    // This mirrors the pattern in GameStreamingManager::OnLoadCell() and
    // SaveSystem::Load() — a single #ifdef keeps the fallback path obvious.
    (void)configPath;
    std::cerr << "[ComboSystem] LoadConfig: ENGINE_ENABLE_JSON not defined — "
                 "use AddCombo() to populate combos programmatically.\n";
    return false;
#endif
}

// ===========================================================================
// Public API — Input feed
// ===========================================================================

std::string ComboSystem::PressInput(ComboInput input)
{
    // Ignore input while in cooldown — the player must wait.
    if (m_state == ComboState::COOLDOWN)
        return {};

    // Append this input to the current sequence.
    m_currentSequence.push_back(input);
    m_state       = ComboState::BUILDING;
    m_windowTimer = m_config.comboWindowSeconds;  // reset the window

    // TEACHING NOTE — Exact match first, then prefix check
    // We check for an exact match BEFORE a prefix match so that shorter
    // combos can fire immediately (e.g. "ATTACK, SPECIAL" triggers before
    // the three-hit "ATTACK, ATTACK, ATTACK" even if the player pressed
    // ATTACK, SPECIAL).  This is the standard "greedy from left" rule.

    // 1. Exact match?
    std::string triggered = CheckExactMatch();
    if (!triggered.empty())
    {
        // Find the matching definition to read the cooldown.
        for (const auto& def : m_combos)
        {
            if (def.name == triggered)
            {
                m_cooldownDuration = def.cooldownSeconds;
                break;
            }
        }
        m_cooldownTimer    = m_cooldownDuration;
        m_state            = ComboState::COOLDOWN;
        m_currentSequence.clear();
        m_windowTimer      = 0.0f;

        if (onComboTriggered)
            onComboTriggered(triggered);

        return triggered;
    }

    // 2. Prefix match? Stay BUILDING.
    if (HasPrefixMatch())
        return {};

    // 3. No match — try restarting with just this input.
    // TEACHING NOTE — Smart reset (plinking)
    // If the sequence so far doesn't match any combo prefix, the player
    // may have started a new combo immediately after a failed one.
    // We restart with only the just-pressed input so the player doesn't
    // have to wait for the window to expire before beginning a fresh combo.
    m_currentSequence.clear();
    m_currentSequence.push_back(input);

    if (HasPrefixMatch())
    {
        // The single input IS a valid start — BUILDING with fresh sequence.
        m_windowTimer = m_config.comboWindowSeconds;
        return {};
    }

    // Even a single-input sequence is not a prefix — back to IDLE.
    m_currentSequence.clear();
    m_state       = ComboState::IDLE;
    m_windowTimer = 0.0f;
    return {};
}

// ===========================================================================
// Public API — Per-frame update
// ===========================================================================

void ComboSystem::Update(float dt)
{
    // TEACHING NOTE — State machine tick
    // Only one state can be active at a time.  We use a switch rather than
    // chained if/else to make the mapping from state → behaviour explicit.
    switch (m_state)
    {
    case ComboState::IDLE:
        // Nothing to tick in IDLE.
        break;

    case ComboState::BUILDING:
        m_windowTimer -= dt;
        if (m_windowTimer <= 0.0f)
        {
            // Window expired — abandon the sequence.
            m_currentSequence.clear();
            m_state       = ComboState::IDLE;
            m_windowTimer = 0.0f;
        }
        break;

    case ComboState::COOLDOWN:
        m_cooldownTimer -= dt;
        if (m_cooldownTimer <= 0.0f)
        {
            m_cooldownTimer = 0.0f;
            m_state         = ComboState::IDLE;
        }
        break;
    }
}

// ===========================================================================
// Public API — Force reset
// ===========================================================================

void ComboSystem::Reset()
{
    m_currentSequence.clear();
    m_state         = ComboState::IDLE;
    m_windowTimer   = 0.0f;
    m_cooldownTimer = 0.0f;
}

// ===========================================================================
// Private helpers
// ===========================================================================

/*static*/ bool ComboSystem::ParseInputToken(const std::string& token, ComboInput& out)
{
    // TEACHING NOTE — Case-insensitive comparison
    // We upper-case the token so "attack", "Attack", and "ATTACK" all work.
    // std::transform with ::toupper is the C++17 idiomatic way for ASCII.
    std::string upper = token;
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    if (upper == "ATTACK")  { out = ComboInput::ATTACK;  return true; }
    if (upper == "DODGE")   { out = ComboInput::DODGE;   return true; }
    if (upper == "MAGIC")   { out = ComboInput::MAGIC;   return true; }
    if (upper == "SPECIAL") { out = ComboInput::SPECIAL; return true; }
    return false;
}

std::string ComboSystem::CheckExactMatch() const
{
    for (const auto& def : m_combos)
    {
        if (def.sequence.size() == m_currentSequence.size() &&
            std::equal(def.sequence.begin(), def.sequence.end(),
                       m_currentSequence.begin()))
        {
            return def.name;
        }
    }
    return {};
}

bool ComboSystem::HasPrefixMatch() const
{
    const std::size_t len = m_currentSequence.size();
    for (const auto& def : m_combos)
    {
        if (def.sequence.size() >= len &&
            std::equal(m_currentSequence.begin(), m_currentSequence.end(),
                       def.sequence.begin()))
        {
            return true;
        }
    }
    return false;
}
