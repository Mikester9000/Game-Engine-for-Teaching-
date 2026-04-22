/**
 * @file combo_system.hpp
 * @brief Combo FSM — tracks player button sequences and recognises named combos.
 *
 * ============================================================================
 * TEACHING NOTE — Action Combat vs Turn-Based ATB
 * ============================================================================
 *
 * The existing CombatSystem implements an ATB (Active Time Battle) system:
 * a timer fires at regular intervals and a turn is resolved.  That is the
 * backbone of the combat simulation — damage, status effects, flee, loot.
 *
 * The ComboSystem layers ACTION input on top of the ATB backbone:
 *
 *   Player presses:   ATTACK → ATTACK → ATTACK
 *   Timer fires:      Avalanche Chain (180% damage) is resolved this turn
 *
 * This mirrors Final Fantasy XV, where:
 *   - The engine runs a continuous combat simulation under the hood.
 *   - Player button presses queue combo inputs.
 *   - A "combo interpreter" turns button sequences into named abilities.
 *
 * ============================================================================
 * TEACHING NOTE — Finite State Machine (FSM) Design
 * ============================================================================
 *
 * A Finite State Machine (FSM) is one of the oldest and most reliable patterns
 * in game programming.  The ComboSystem FSM has three states:
 *
 *   IDLE      No combo is building.  Waiting for the first input.
 *
 *   BUILDING  The player has pressed at least one button.  A "combo window"
 *             timer counts down.  If the next input arrives before the window
 *             expires, it is appended to the current sequence.  If the window
 *             expires first, the sequence is discarded (IDLE).
 *
 *   COOLDOWN  A combo was recognised and executed.  The player must wait for
 *             the cooldown to expire before starting the next combo.
 *
 * Transitions:
 *
 *   IDLE    + button press          → BUILDING (start sequence)
 *   BUILDING + button press         → BUILDING (extend sequence) or COOLDOWN (match found)
 *   BUILDING + window expires       → IDLE     (sequence cancelled)
 *   COOLDOWN + cooldown expires     → IDLE     (ready for next input)
 *
 * ============================================================================
 * TEACHING NOTE — Combo Window (Input Leniency)
 * ============================================================================
 *
 * The "combo window" is the maximum gap allowed between successive button
 * presses.  If it is too short, the combo becomes impossible to execute
 * consistently; if too long, players can accidentally trigger combos while
 * mashing.  FF15 uses about 0.4–0.6 seconds per input step.
 *
 * In our engine the window is loaded from combat_config.json so that game
 * designers can tune it without recompiling:
 *
 *   "comboWindowSeconds": 0.5
 *
 * ============================================================================
 * TEACHING NOTE — Data-Driven Design via combat_config.json
 * ============================================================================
 *
 * Every combo sequence (name, buttons, damage multiplier, element, MP cost,
 * cooldown) lives in Content/combat_config.json, NOT in C++ source.
 *
 * WHY data-driven?
 *   1. Designers can add combos without touching C++ code.
 *   2. The engine is shipped once; content ships separately.
 *   3. JSON diffs are readable in pull requests; C++ diffs are noisy.
 *
 * LoadConfig() parses the JSON file and populates m_combos.
 * The JSON code is guarded by ENGINE_ENABLE_JSON, which is defined when
 * nlohmann/json is found via vcpkg.  Without it, combos must be added
 * programmatically via AddCombo().
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

#include <string>
#include <vector>
#include <functional>
#include <cstdint>


// ===========================================================================
// ComboInput — atomic player action buttons
// ===========================================================================

/**
 * @enum ComboInput
 * @brief Single button press that can be part of a combo sequence.
 *
 * TEACHING NOTE — Enumeration vs String Keys
 * ──────────────────────────────────────────
 * We store each input as a typed enum rather than a raw string "ATTACK".
 * This means typos ("ATTCK") are caught at compile time, not at runtime,
 * and switch-case dispatch is O(1) rather than O(n) string compares.
 *
 * The JSON loader (LoadConfig) converts string tokens → ComboInput values.
 */
enum class ComboInput : uint8_t {
    ATTACK,   ///< Primary attack — sword slash or basic strike.
    DODGE,    ///< Dodge / quick-step — repositioning input.
    MAGIC,    ///< Ring spell / elemancy flask input.
    SPECIAL   ///< Signature ability — warp-strike or royal technique.
};


// ===========================================================================
// ComboDefinition — one learnable combo
// ===========================================================================

/**
 * @struct ComboDefinition
 * @brief Complete specification for one combo move.
 *
 * TEACHING NOTE — Flyweight Pattern
 * ──────────────────────────────────
 * ComboDefinition is read-only shared data (loaded once from JSON).
 * At runtime, ComboSystem only READS this struct — it never modifies it.
 * This follows the Flyweight pattern: shared immutable data referenced
 * by many callers with no per-instance overhead.
 */
struct ComboDefinition {
    /// Display name shown in combo notifications ("Avalanche Chain").
    std::string name;

    /// Required button inputs in order.
    /// Example: {ATTACK, ATTACK, ATTACK} = three-hit slash chain.
    std::vector<ComboInput> sequence;

    /// Damage multiplier applied to the base attack damage.
    /// 1.0 = normal, 1.5 = 150%, 2.0 = double damage.
    float damageMultiplier = 1.0f;

    /// MP cost deducted when the combo executes (0 = free).
    int mpCost = 0;

    /// Seconds the player must wait before starting the next combo.
    float cooldownSeconds = 1.0f;

    /// Elemental type string ("physical", "fire", "ice", "lightning").
    /// Matched against EnemyData resistances/weaknesses in CombatSystem.
    std::string element = "physical";
};


// ===========================================================================
// CombatConfig — tunable parameters from combat_config.json
// ===========================================================================

/**
 * @struct CombatConfig
 * @brief Scalar combat tuning parameters loaded from combat_config.json.
 *
 * TEACHING NOTE — Separation of Config from Code
 * ─────────────────────────────────────────────────
 * All "magic numbers" in the combat formula are collected here rather than
 * scattered as named constants throughout CombatSystem.cpp.  A designer
 * changes critMultiplier from 1.5 → 1.8 in JSON; the C++ is never touched.
 *
 * Fields map 1:1 to JSON keys for easy inspection:
 *   C++ field             JSON key
 *   comboWindowSeconds  ← "comboWindowSeconds"
 *   turnDurationSeconds ← "turnDurationSeconds"
 *   critMultiplier      ← "critMultiplier"
 *   fleeBaseChance      ← "fleeBaseChance"
 *   fleeChanceCap       ← "fleeChanceCap"
 *   linkStrikeChance    ← "linkStrikeChance"
 *   linkStrikeDamageRatio ← "linkStrikeDamageRatio"
 */
struct CombatConfig {
    float comboWindowSeconds    = 0.5f;   ///< Max gap between combo inputs (seconds).
    float turnDurationSeconds   = 2.0f;   ///< ATB timer duration (seconds per turn).
    float critMultiplier        = 1.5f;   ///< Critical hit damage multiplier.
    float fleeBaseChance        = 0.30f;  ///< Base flee probability (0–1).
    float fleeChanceCap         = 0.90f;  ///< Maximum flee probability (0–1).
    float linkStrikeChance      = 0.30f;  ///< Probability each party member links in.
    float linkStrikeDamageRatio = 0.30f;  ///< Link strike bonus as fraction of normal.
};


// ===========================================================================
// ComboState — FSM states
// ===========================================================================

/**
 * @enum ComboState
 * @brief Active state of the ComboSystem finite state machine.
 *
 * TEACHING NOTE — Why Only Three States?
 * ────────────────────────────────────────
 * Keeping the FSM small (three states) makes every transition explicit and
 * easy to trace in a debugger.  More complex action games add states like
 * LINKING (performing a link-strike) or FINISHER (playing a finish animation)
 * but those are sub-states of EXECUTING in a hierarchical FSM — we keep it
 * flat here for teaching clarity.
 */
enum class ComboState : uint8_t {
    IDLE,      ///< No inputs yet; ready to start a new combo.
    BUILDING,  ///< At least one input received; window timer counting down.
    COOLDOWN   ///< Combo executed; cannot start another until timer expires.
};


// ===========================================================================
// ComboSystem class
// ===========================================================================

/**
 * @class ComboSystem
 * @brief Tracks player button inputs, matches combo sequences, and reports
 *        which named combo (if any) has been triggered.
 *
 * ─── Typical Frame Loop ──────────────────────────────────────────────────
 * @code
 *   // Once per frame — called BEFORE checking button input:
 *   m_comboSystem.Update(dt);
 *
 *   // On each button press (from InputMapper or Win32 key handler):
 *   if (attackButtonPressed) {
 *       auto name = m_comboSystem.PressInput(ComboInput::ATTACK);
 *       if (!name.empty()) {
 *           // 'name' is the recognised combo ("Avalanche Chain", etc.)
 *           // Pass it to CombatSystem::PlayerAttack with damageMultiplier
 *           auto* def = FindCombo(name);
 *           int dmg = CombatSystem::PlayerAttack(target) * def->damageMultiplier;
 *       }
 *   }
 * @endcode
 * ─────────────────────────────────────────────────────────────────────────
 */
class ComboSystem {
public:
    // -----------------------------------------------------------------------
    // Configuration (call before first PressInput)
    // -----------------------------------------------------------------------

    /**
     * @brief Replace the current CombatConfig with @p cfg.
     *
     * @param cfg  New config to apply.
     *
     * TEACHING NOTE — Setter Injection vs Constructor Parameters
     * ───────────────────────────────────────────────────────────
     * We use a setter rather than passing config through the constructor
     * so that the system can be instantiated (no-arg) and configured later
     * once the JSON has been loaded.  This matches the common game-engine
     * pattern where objects are allocated in pool containers before their
     * data is available.
     */
    void SetConfig(const CombatConfig& cfg) { m_config = cfg; }

    /**
     * @brief Add one combo definition.
     *
     * @param def  The combo to add.
     *
     * This API is used when ENGINE_ENABLE_JSON is not available, or in unit
     * tests that need precise control over which combos are loaded.
     */
    void AddCombo(const ComboDefinition& def) { m_combos.push_back(def); }

    /**
     * @brief Load combos + config from a JSON file.
     *
     * On success, fills m_combos and m_config from the JSON data.
     * Existing combos are replaced (not merged).
     *
     * @param configPath  Absolute or relative path to combat_config.json.
     * @return true on success; false if the file cannot be read or is invalid.
     *
     * TEACHING NOTE — ENGINE_ENABLE_JSON Guard
     * ──────────────────────────────────────────
     * The JSON parsing code is compiled only when ENGINE_ENABLE_JSON is
     * defined (i.e. nlohmann/json was found via vcpkg).  Without it, the
     * function logs a warning and returns false — the caller must use
     * AddCombo() directly.  This ensures the engine compiles on machines
     * without vcpkg and on the CI engine-only preset.
     */
    bool LoadConfig(const std::string& configPath);

    // -----------------------------------------------------------------------
    // Input feed
    // -----------------------------------------------------------------------

    /**
     * @brief Feed one button press into the FSM.
     *
     * @param input  The button that was pressed.
     * @return Name of the combo that was triggered, or empty string if none.
     *
     * TEACHING NOTE — Prefix Matching
     * ─────────────────────────────────
     * After each input the system checks:
     *   1. EXACT MATCH — does the current sequence equal any combo sequence?
     *      → Yes: trigger the combo (return its name), enter COOLDOWN.
     *   2. PREFIX MATCH — is the current sequence a prefix of at least one combo?
     *      → Yes: stay in BUILDING, keep collecting inputs.
     *   3. NO MATCH — neither exact nor prefix.
     *      → Reset to IDLE.  If the single input alone matches as a prefix,
     *        restart with just that one input instead of discarding everything.
     *
     * This "smart reset" allows the player to start a new sequence mid-way
     * through a failed one without having to wait for the window to expire
     * (a technique called "input leniency" or "plinking").
     */
    std::string PressInput(ComboInput input);

    // -----------------------------------------------------------------------
    // Per-frame update
    // -----------------------------------------------------------------------

    /**
     * @brief Advance all timers by @p dt seconds.
     *
     * Must be called once per frame.
     *
     * @param dt  Delta time in seconds since the last frame.
     *
     * TEACHING NOTE — Update Before Input
     * ──────────────────────────────────────
     * Calling Update() before PressInput() each frame ensures that a window
     * expiry that happens on the same frame as an input is processed correctly:
     * if the window expired this frame, the input starts a fresh sequence
     * rather than extending a stale one.
     */
    void Update(float dt);

    // -----------------------------------------------------------------------
    // Force reset
    // -----------------------------------------------------------------------

    /**
     * @brief Cancel the current combo sequence and return to IDLE.
     *
     * Use when a menu opens mid-combo, the player is stunned, or combat ends.
     */
    void Reset();

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------

    /// @return Current FSM state.
    ComboState GetState() const { return m_state; }

    /// @return Number of combos loaded (via AddCombo or LoadConfig).
    size_t ComboCount() const { return m_combos.size(); }

    /// @return All loaded combo definitions (read-only).
    const std::vector<ComboDefinition>& GetCombos() const { return m_combos; }

    /// @return Current configuration.
    const CombatConfig& GetConfig() const { return m_config; }

    /// @return The current partial input sequence (for HUD display).
    const std::vector<ComboInput>& GetCurrentSequence() const
    {
        return m_currentSequence;
    }

    // -----------------------------------------------------------------------
    // Optional callback
    // -----------------------------------------------------------------------

    /**
     * @brief Called whenever a combo is successfully triggered.
     *
     * @param comboName  The name of the triggered combo.
     *
     * TEACHING NOTE — Callbacks vs Event Bus
     * ──────────────────────────────────────────
     * We use a std::function callback rather than EventBus here because:
     *   • The combo trigger is 1-to-1 (one caller, one listener) — EventBus
     *     is better suited to 1-to-N fan-out like a weather change.
     *   • std::function is zero external dependencies and stack-allocated for
     *     simple lambdas.
     * For a production engine you might route through EventBus<CombatEvent>
     * to decouple the HUD, sound effects, and animation responses.
     */
    std::function<void(const std::string& comboName)> onComboTriggered;

private:
    // -----------------------------------------------------------------------
    // Private helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Convert a JSON string token to a ComboInput value.
     *
     * @param token  "ATTACK", "DODGE", "MAGIC", or "SPECIAL" (case-insensitive).
     * @param out    Receives the parsed value.
     * @return false if the token is unrecognised.
     */
    static bool ParseInputToken(const std::string& token, ComboInput& out);

    /**
     * @brief Check the current sequence against all loaded combos.
     *
     * @return Triggered combo name, or empty string if no exact match.
     */
    std::string CheckExactMatch() const;

    /**
     * @brief Return true if the current sequence is a valid prefix of at
     *        least one loaded combo.
     */
    bool HasPrefixMatch() const;

    // -----------------------------------------------------------------------
    // Member data
    // -----------------------------------------------------------------------

    std::vector<ComboDefinition> m_combos;           ///< All loaded combo definitions.
    std::vector<ComboInput>      m_currentSequence;  ///< Inputs collected so far this combo.
    ComboState                   m_state          = ComboState::IDLE;
    float                        m_windowTimer    = 0.0f; ///< Time remaining in the combo window.
    float                        m_cooldownTimer  = 0.0f; ///< Time remaining in the post-combo cooldown.
    float                        m_cooldownDuration = 0.0f; ///< Cooldown of the last executed combo.
    CombatConfig                 m_config;             ///< Tunable parameters.
};
