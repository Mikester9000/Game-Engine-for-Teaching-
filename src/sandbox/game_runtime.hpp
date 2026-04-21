/**
 * @file game_runtime.hpp
 * @brief D3D11 game loop driver — M8.1 Gameplay Integration.
 *
 * ============================================================================
 * TEACHING NOTE — GameRuntime Architecture
 * ============================================================================
 * GameRuntime is the M8 replacement for the terminal-only `Game` class.
 * It owns all gameplay systems (Combat, AI, Quest, Weather, Magic, etc.) and
 * drives them from the D3D11 `engine_sandbox` main loop.
 *
 * Design goals:
 *   1. INDEPENDENCE — GameRuntime does NOT depend on any renderer type.
 *      main.cpp creates a GameRuntime alongside the renderer and calls both
 *      independently.  This mirrors how a real engine separates game thread
 *      from render thread.
 *
 *   2. SAME SYSTEMS — All terminal game systems (CombatSystem, AISystem,
 *      WeatherSystem, QuestSystem, etc.) run here unchanged.  The only
 *      difference from the terminal game is that rendering output goes to
 *      D3D11 instead of ncurses.
 *
 *   3. HEADLESS SAFE — GameRuntime::Init() and Update() do not call any
 *      Win32 API or D3D11 API.  They are pure ECS + game logic.  This means
 *      the m8_gameplay acceptance test can run in headless CI mode.
 *
 * ─── System Update Order (per frame) ────────────────────────────────────────
 *   1. InputMapper          — keyboard state → ECS component flags
 *   2. WeatherSystem        — advance day/night cycle
 *   3. AISystem             — enemy FSM + A* pathfinding
 *   4. CombatSystem         — ATB timers, attack resolution, damage
 *   5. QuestSystem          — objective progress checks
 *   6. CameraSystem         — update view/proj matrices
 *   7. Hud                  — extract HudState snapshot
 *   8. GameStreamingManager — pump async cell load/evict completions (M8.7)
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2025
 * C++ Standard: C++17
 * Platform: Windows (engine_sandbox); headless safe.
 */

#pragma once

#include <memory>   // std::unique_ptr
#include <cstdint>  // uint32_t

// ---- ECS ----
#include "engine/ecs/ECS.hpp"

// ---- Gameplay systems (terminal game) ----
#include "game/systems/CombatSystem.hpp"
#include "game/systems/AISystem.hpp"
#include "game/systems/WeatherSystem.hpp"
#include "game/systems/QuestSystem.hpp"
#include "game/systems/input_mapper.hpp"
#include "game/systems/dialogue_system.hpp"

// ---- World ----
#include "game/world/TileMap.hpp"
#include "game/world/GameStreamingManager.hpp"

// ---- Engine systems ----
#include "engine/rendering/camera_system.hpp"
#include "engine/ui/hud.hpp"
#include "engine/save/save_system.hpp"
#include "engine/core/EventBus.hpp"
#include "engine/assets/asset_db.hpp"
#include "engine/assets/asset_loader.hpp"

namespace sandbox {

// ===========================================================================
// GameRuntime
// ===========================================================================

/**
 * @class GameRuntime
 * @brief Owns and drives all gameplay systems for the D3D11 engine_sandbox.
 *
 * Lifecycle:
 *   Init() → Update(dt) per frame → Shutdown()
 *
 * main.cpp creates a GameRuntime and calls it from the win32 message loop,
 * immediately after receiving the post-update renderer draw call.
 */
class GameRuntime
{
public:
    GameRuntime();
    ~GameRuntime();

    // Non-copyable.
    GameRuntime(const GameRuntime&)            = delete;
    GameRuntime& operator=(const GameRuntime&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Initialise all systems and spawn the opening scene.
     *
     * Spawns: player (Noctis) + 3 enemies (Goblin, Niffin, Anak Calf).
     * Accepts quest 1 "The Road to Dawn" for the player.
     * Creates an open-world TileMap (100×100 floor tiles).
     * Attaches a camera entity targeting the player.
     *
     * @return true on success.
     */
    bool Init();

    /**
     * @brief Advance all gameplay systems by dt seconds.
     *
     * Called once per frame by main.cpp.
     *
     * @param dt  Delta time in seconds (typically 1/60 or 1/30).
     */
    void Update(float dt);

    /**
     * @brief Shut down all systems and release resources.
     */
    void Shutdown();

    // =========================================================================
    // Renderer feedback
    // =========================================================================

    /**
     * @brief Return a clear colour reflecting current game state.
     *
     * TEACHING NOTE — Render feedback without direct renderer coupling
     * ─────────────────────────────────────────────────────────────────
     * GameRuntime communicates with the renderer through a single colour tuple
     * rather than holding a pointer to IRenderer.  This keeps the game layer
     * decoupled from the graphics layer.
     *
     * The colour encodes game state visually:
     *   Dawn / day  — warm orange tint   (r↑ g↑ b↓)
     *   Night       — deep blue          (r↓ g↓ b↑)
     *   Combat      — red pulse          (r↑ g↓ b↓)
     *   Idle        — neutral near-black (r≈ g≈ b≈)
     */
    void GetClearColour(float& r, float& g, float& b) const;

    // =========================================================================
    // Accessors (for CI acceptance tests)
    // =========================================================================

    /// Returns the ECS World (read-only; used by m8_gameplay acceptance test).
    const World& GetWorld() const { return m_world; }

    /// Returns the player entity ID.
    EntityID GetPlayerID() const { return m_playerID; }

    /// Returns how many AI state transitions have occurred this session.
    /// Used by m8_gameplay acceptance test.
    int GetAIStateTransitionCount() const { return m_aiStateTransitions; }

    /// Returns the last HUD snapshot (populated each frame after Update()).
    const HudState& GetLastHudState() const { return m_lastHudState; }

    /// Returns total number of frames simulated since Init().
    int GetFrameCount() const { return m_frameCount; }

    /// Returns the save system (for CampSystem auto-save wiring).
    engine::save::SaveSystem& GetSaveSystem() { return m_saveSystem; }

private:
    // ---- ECS World ----
    World m_world;

    // ---- Open-world navigation grid (all-floor, 100×100 tiles) ----
    TileMap m_tileMap;

    // ---- World streaming (M8.7) ----
    // TEACHING NOTE — Streaming integration in GameRuntime
    // ──────────────────────────────────────────────────────
    // GameStreamingManager, AssetDB, and AssetLoader are value members so they
    // share the GameRuntime lifetime.  AssetLoader holds a raw pointer to
    // AssetDB, so the ordering here (DB before loader) guarantees the DB
    // outlives the loader.  std::unique_ptr defers AssetLoader construction
    // until Init() has successfully loaded the database.
    GameStreamingManager                         m_streamingMgr;
    engine::assets::AssetDB                      m_assetDB;
    std::unique_ptr<engine::assets::AssetLoader> m_assetLoader;

    // ---- Gameplay systems ----
    std::unique_ptr<CombatSystem>   m_combat;
    std::unique_ptr<AISystem>       m_ai;
    std::unique_ptr<WeatherSystem>  m_weather;
    std::unique_ptr<QuestSystem>    m_quests;
    std::unique_ptr<DialogueSystem> m_dialogue;
    InputMapper                     m_inputMapper;

    // ---- Engine systems ----
    engine::rendering::CameraSystem m_camera;
    Hud                             m_hud;
    engine::save::SaveSystem        m_saveSystem;

    // ---- Entity IDs ----
    EntityID m_playerID   = NULL_ENTITY;
    EntityID m_cameraID   = NULL_ENTITY;

    // ---- State tracking ----
    float m_gameTime          = 0.0f;
    int   m_frameCount        = 0;

    // ---- AI transition tracking (for CI acceptance test) ----
    int   m_aiStateTransitions = 0;

    // Previous AI states for transition detection.
    // maps entity → last known AIState (stored as int to avoid forward-declare issues)
    std::vector<std::pair<EntityID, int>> m_prevAIStates;

    // ---- HUD snapshot ----
    HudState m_lastHudState;

    // ---- Back-buffer dimensions (updated by Resize, defaults to 1920×1080) ----
    uint32_t m_backBufferW = 1920;
    uint32_t m_backBufferH = 1080;

    // ---- Internal helpers ----
    void TrackAITransitions();
};

} // namespace sandbox
