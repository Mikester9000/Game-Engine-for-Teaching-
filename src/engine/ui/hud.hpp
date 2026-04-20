/**
 * @file hud.hpp
 * @brief In-game HUD state — HP/MP bars, ATB gauge, quest tracker — M8.5.
 *
 * ============================================================================
 * TEACHING NOTE — HUD Architecture
 * ============================================================================
 * The HUD (Heads-Up Display) shows the player all game-critical information
 * without obscuring gameplay.  In FF15:
 *
 *   • HP/MP bars + ATB gauge (combat readiness)
 *   • Equipped magic flask icon
 *   • Party member portraits with HP bars
 *   • Active quest objective text (top-right)
 *   • Button prompts (context-sensitive, near NPCs etc.)
 *
 * ─── Data vs Rendering Separation ────────────────────────────────────────────
 * HudState is a PURE DATA struct: it contains no rendering code and no
 * Win32/ImGui headers.  The rendering layer (D3D11Renderer or editor ImGui
 * overlay) reads HudState and draws it however it likes.
 *
 * This separation means:
 *   • The HUD works in headless CI mode (prints to stdout).
 *   • Adding a new renderer (Vulkan) doesn't change any gameplay code.
 *   • The HUD can be unit-tested by checking HudState fields directly.
 *
 * ─── ImGui Overlay (windowed D3D11) ─────────────────────────────────────────
 * When ENGINE_ENABLE_IMGUI is defined (future: imgui linked to engine_sandbox),
 * Hud::Render() draws a proper ImGui overlay with progress bars and icons.
 * Until then, Hud::PrintDebug() writes a compact status line to stdout every
 * N frames so CI logs show live game state.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2025
 * C++ Standard: C++17
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "engine/ecs/ECS.hpp"   // World, EntityID, all components

// ===========================================================================
// PartyMemberHudEntry — one entry in the HUD party bar
// ===========================================================================

/**
 * @struct PartyMemberHudEntry
 * @brief HUD data for one party member's portrait bar.
 */
struct PartyMemberHudEntry
{
    std::string name;       ///< Character name ("Noctis", "Ignis", …)
    int         hp    = 0;  ///< Current HP.
    int         maxHp = 1;  ///< Maximum HP.
    int         mp    = 0;  ///< Current MP.
    int         maxMp = 1;  ///< Maximum MP.
    bool        isDowned = false; ///< True = downed (red outline in FF15).
};

// ===========================================================================
// HudState — snapshot of all HUD-relevant game state
// ===========================================================================

/**
 * @struct HudState
 * @brief Snapshot of all game state the HUD needs to render one frame.
 *
 * TEACHING NOTE — Why a Snapshot?
 * ─────────────────────────────────
 * Rather than passing the entire ECS World to the renderer, we extract a
 * compact snapshot each frame.  This has several advantages:
 *
 *   1. The renderer is decoupled from ECS — it doesn't need to know about
 *      component types or World queries.
 *   2. The snapshot can be captured on the game thread and handed to a
 *      render thread without locking (immutable after capture).
 *   3. It is trivially serialisable for replay recording.
 */
struct HudState
{
    // ---- Player stats ----

    int   playerHp      = 0;   ///< Current player HP.
    int   playerMaxHp   = 1;   ///< Maximum player HP.
    int   playerMp      = 0;   ///< Current player MP.
    int   playerMaxMp   = 1;   ///< Maximum player MP.
    float hpFraction()  const { return playerMaxHp > 0 ? float(playerHp)  / float(playerMaxHp)  : 0.0f; }
    float mpFraction()  const { return playerMaxMp > 0 ? float(playerMp)  / float(playerMaxMp)  : 0.0f; }

    // ---- ATB gauge ----
    //
    // TEACHING NOTE — ATB (Active Time Battle)
    // ATB is a timer that fills over time.  When it reaches 1.0 the player
    // can perform an action.  FF15 uses a real-time variant called the
    // "Armiger chain meter" that fills as the player deals damage.
    // Here we use the simpler classic ATB: 0 = empty, 1 = full / action ready.

    float atbGauge      = 0.0f; ///< 0.0 = empty, 1.0 = action ready.
    bool  atbReady      = false; ///< True when atbGauge >= 1.0.

    // ---- Combat state ----
    bool isInCombat     = false; ///< True if the player is in an active encounter.
    int  activeEnemies  = 0;     ///< Number of living enemies in the current zone.

    // ---- Equipped magic ----
    std::string equippedSpell;   ///< Name of the currently equipped magic ("Firaga", …)

    // ---- Party portraits ----
    std::vector<PartyMemberHudEntry> partyMembers;

    // ---- Quest tracker ----
    std::string activeQuestName;       ///< Current main-quest title.
    std::string activeObjectiveText;   ///< Current objective description.

    // ---- Debug / validation ----
    int   frameCount    = 0;     ///< Total frames since GameRuntime::Init().
    float gameTimeSecs  = 0.0f;  ///< Total game time elapsed (real seconds).
};

// ===========================================================================
// Hud
// ===========================================================================

/**
 * @class Hud
 * @brief Reads ECS component state and produces a HudState snapshot.
 *
 * Usage:
 * @code
 *   Hud hud;
 *   // Per frame:
 *   HudState state = hud.ReadFromWorld(world, playerID, frameCount, gameTime);
 *   hud.PrintDebug(state);   // stdout (headless CI / debug)
 *   // hud.Render(state);    // (future: ImGui overlay)
 * @endcode
 */
class Hud
{
public:
    Hud()  = default;
    ~Hud() = default;

    // =========================================================================
    // Snapshot extraction
    // =========================================================================

    /**
     * @brief Extract current game state from the ECS World into a HudState.
     *
     * Reads HealthComponent, CombatComponent, MagicComponent, PartyComponent,
     * and QuestComponent for the player entity.
     *
     * @param world       ECS World.
     * @param playerID    Player entity to read.
     * @param frameCount  Current frame number (for debug output).
     * @param gameTime    Accumulated game time in seconds.
     * @return            Populated HudState snapshot.
     */
    HudState ReadFromWorld(World& world, EntityID playerID,
                           int frameCount, float gameTime) const;

    // =========================================================================
    // Debug output (headless CI + development)
    // =========================================================================

    /**
     * @brief Print a compact HUD summary line to stdout.
     *
     * Output format (one line):
     *   [HUD] Frame=60 HP=450/450 MP=120/120 ATB=0.72 Quest="Hunt the Behemoth"
     *
     * @param state   HudState snapshot to print.
     * @param every   Only print every N frames (default 60 = once per second).
     */
    void PrintDebug(const HudState& state, int every = 60) const;
};
