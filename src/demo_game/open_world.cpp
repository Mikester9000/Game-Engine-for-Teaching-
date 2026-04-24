/**
 * @file open_world.cpp
 * @brief OpenWorld implementation — demo open-world state for Demo_Game.
 *
 * ============================================================================
 * TEACHING NOTE — Data-Driven Open World Design
 * ============================================================================
 * Rather than scattering station setup throughout gameplay code, this file
 * centralises the canonical teaching-station data in
 * RegisterDefaultStations().
 *
 * The current Demo_Game build uses these built-in C++ defaults only, so the
 * game is playable out-of-the-box even without the JSON world-data files
 * present.  The JSON files under Content/World/ are the intended authoring
 * format for a future data-driven loading path.
 *
 * TEACHING NOTE — Fallback Data Pattern
 * ──────────────────────────────────────
 * AAA engines always ship a set of hardcoded defaults so the game doesn't
 * crash when an asset is missing.  The external data files (JSON, binary
 * blobs) override those defaults at runtime.  We follow the same pattern
 * here: C++ registers the defaults first; a future runtime loader would
 * merge overrides from Content/World/teaching_stations.json.
 *
 * ─── State Machine Transitions ──────────────────────────────────────────────
 *
 *   BOOT_MENU  ──(BootSelectNewGame)──► LOADING
 *   LOADING    ──(stateTime > 1.5s)──► PLAYING
 *   PLAYING    ──(ESC / pause key)──► PAUSED
 *   PAUSED     ──(ESC / resume)──► PLAYING
 *   PAUSED     ──(quit)──► BOOT_MENU
 *
 * In headless CI mode the transitions happen automatically so the test
 * exercises the entire flow without user input.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2026
 * C++ Standard: C++17
 */

#include "demo_game/open_world.hpp"

#include <iostream>  // std::cout for headless CI log output

// ---------------------------------------------------------------------------
// BiomeName
// ---------------------------------------------------------------------------

const char* BiomeName(BiomeType b) noexcept
{
    switch (b)
    {
        case BiomeType::GRASSLAND: return "Grassland";
        case BiomeType::FOREST:    return "Forest";
        case BiomeType::SNOW:      return "Snow Highlands";
        case BiomeType::DESERT:    return "Desert";
        case BiomeType::COAST:     return "Coastline";
        default:                   return "Unknown";
    }
}

// ---------------------------------------------------------------------------
// OpenWorld — lifecycle
// ---------------------------------------------------------------------------

OpenWorld::OpenWorld() = default;

bool OpenWorld::Init()
{
    // Register the canonical teaching stations (inline data fallback).
    RegisterDefaultStations();

    // Start at the boot menu.
    m_state     = OpenWorldState::BOOT_MENU;
    m_stateTime = 0.f;

    std::cout << "[OpenWorld] Initialised — " << m_stations.size()
              << " teaching stations registered.\n";
    return true;
}

void OpenWorld::Update(float dt, bool headless)
{
    ++m_frameCount;
    m_stateTime += dt;

    switch (m_state)
    {
        case OpenWorldState::BOOT_MENU:  UpdateBootMenu(dt, headless);  break;
        case OpenWorldState::LOADING:    UpdateLoading (dt, headless);  break;
        case OpenWorldState::PLAYING:    UpdatePlaying (dt, headless);  break;
        case OpenWorldState::PAUSED:     UpdatePaused  (dt, headless);  break;
        case OpenWorldState::DEBUG_MENU: /* debug overlay; no sim tick */break;
    }
}

void OpenWorld::Shutdown()
{
    std::cout << "[OpenWorld] Shutdown after " << m_frameCount << " frames.\n";
}

// ---------------------------------------------------------------------------
// GetClearColour — biome-driven sky colour
// ---------------------------------------------------------------------------
// TEACHING NOTE — Biome Colour Palettes
// ─────────────────────────────────────────────────────────────────────────────
// Each biome maps to a distinctive sky colour so even the clear colour alone
// communicates the current area to the player.  In a full implementation these
// values would be read from the biome's SkyRenderer config (M10 SkyRenderer
// already has a time-of-day clock; here we use a static palette for simplicity).
// ─────────────────────────────────────────────────────────────────────────────

void OpenWorld::GetClearColour(float& r, float& g, float& b) const
{
    switch (m_currentBiome)
    {
        case BiomeType::GRASSLAND:
            r = 0.42f; g = 0.60f; b = 0.92f; // daylight blue
            break;
        case BiomeType::FOREST:
            r = 0.18f; g = 0.28f; b = 0.22f; // dim filtered green-grey
            break;
        case BiomeType::SNOW:
            r = 0.78f; g = 0.88f; b = 0.98f; // pale icy blue
            break;
        case BiomeType::DESERT:
            r = 0.76f; g = 0.55f; b = 0.22f; // hot orange-amber
            break;
        case BiomeType::COAST:
            r = 0.14f; g = 0.48f; b = 0.72f; // deep sea-blue cyan
            break;
        default:
            r = 0.10f; g = 0.12f; b = 0.14f;
            break;
    }

    // BOOT_MENU / LOADING: dim to near-black to suggest the title screen.
    if (m_state == OpenWorldState::BOOT_MENU ||
        m_state == OpenWorldState::LOADING)
    {
        r *= 0.12f; g *= 0.12f; b *= 0.12f;
    }
}

// ---------------------------------------------------------------------------
// Control
// ---------------------------------------------------------------------------

void OpenWorld::BootSelectNewGame()
{
    if (m_state == OpenWorldState::BOOT_MENU)
    {
        std::cout << "[OpenWorld] Boot: NEW GAME selected.\n";
        m_state     = OpenWorldState::LOADING;
        m_stateTime = 0.f;
    }
}

void OpenWorld::TeleportToStation(const std::string& stationID)
{
    for (const auto& s : m_stations)
    {
        if (s.id == stationID)
        {
            std::cout << "[OpenWorld] Teleport → station \"" << s.displayName
                      << "\" at (" << s.worldX << ", " << s.worldZ << ")\n";
            m_currentBiome = s.biome;
            return;
        }
    }
    std::cout << "[OpenWorld] Warning: station \"" << stationID << "\" not found.\n";
}

// ---------------------------------------------------------------------------
// Private: state update helpers
// ---------------------------------------------------------------------------

void OpenWorld::UpdateBootMenu(float /*dt*/, bool headless)
{
    // TEACHING NOTE — Headless Auto-Advance
    // ──────────────────────────────────────
    // In CI (headless) mode there is no keyboard, so we automatically select
    // "New Game" after two frames to exercise the full BOOT_MENU→LOADING→PLAYING
    // flow without user input.  The "headless" flag is the only difference
    // between CI and interactive mode in this function.
    if (headless && m_frameCount >= 2)
    {
        BootSelectNewGame();
    }
}

void OpenWorld::UpdateLoading(float /*dt*/, bool headless)
{
    // TEACHING NOTE — Simulated Loading Stall
    // ─────────────────────────────────────────
    // In a real engine this state waits for the async streaming system (M7)
    // to warm up the first ring of cells around the spawn point.  Here we
    // simply wait a brief time in headless mode (vs. a longer splash time
    // for a real windowed game so the player can read the loading screen).
    static constexpr float kHeadlessLoadTimeSeconds = 0.1f;
    static constexpr float kWindowedLoadTimeSeconds = 1.5f;
    const float waitTime = headless ? kHeadlessLoadTimeSeconds
                                    : kWindowedLoadTimeSeconds;

    if (m_stateTime >= waitTime)
    {
        std::cout << "[OpenWorld] Loading complete — entering PLAYING state.\n";
        m_state        = OpenWorldState::PLAYING;
        m_stateTime    = 0.f;
        m_currentBiome = BiomeType::GRASSLAND; // always spawn in the grassland
    }
}

void OpenWorld::UpdatePlaying(float /*dt*/, bool headless)
{
    // TEACHING NOTE — Biome Simulation
    // ──────────────────────────────────
    // In a real implementation the biome is determined by the player's world
    // coordinates (sampled from a biome map texture or Voronoi diagram).  Here
    // we cycle through biomes every kFramesPerBiome frames in headless mode
    // to exercise the full set of sky colours in CI without travelling.
    if (headless)
    {
        // kBiomeCount must match the number of BiomeType enum values (5).
        static constexpr int kBiomeCount    = 5;
        static constexpr int kFramesPerBiome = kHeadlessFrames / kBiomeCount;
        const int biomeIdx = (m_frameCount / kFramesPerBiome) % kBiomeCount;
        m_currentBiome = static_cast<BiomeType>(biomeIdx);

        if (m_frameCount >= kHeadlessFrames)
        {
            std::cout << "[OpenWorld] Headless validation complete ("
                      << m_frameCount << " frames, all biomes visited).\n";
            m_headlessDone = true;
        }
    }
}

void OpenWorld::UpdatePaused(float /*dt*/, bool headless)
{
    // Nothing to tick while paused; headless auto-resumes after 5 frames.
    if (headless && m_stateTime > (5.f / 60.f))
    {
        m_state     = OpenWorldState::PLAYING;
        m_stateTime = 0.f;
    }
}

// ---------------------------------------------------------------------------
// RegisterDefaultStations
// ---------------------------------------------------------------------------
// TEACHING NOTE — Inline Default Station Data
// ─────────────────────────────────────────────
// We register the canonical teaching stations here so that Demo_Game works
// out-of-the-box even if the Content/World/teaching_stations.json file is
// missing.  Students can override or extend these by editing the JSON.
//
// Station coordinate convention: X increases East, Z increases North.
// The world is 1024 × 1024 world-units; spawn is at (512, 512).
// ─────────────────────────────────────────────────────────────────────────────

void OpenWorld::RegisterDefaultStations()
{
    m_stations.clear();

    // ── 1. PBR Rendering Station ──────────────────────────────────────────
    m_stations.push_back({
        "rendering_pbr",
        "PBR Rendering",
        "Cook-Torrance BRDF (GGX NDF + Smith G + Schlick F), IBL, tonemapping.",
        BiomeType::GRASSLAND,
        300.f, 512.f,
        "pbr_ibl"
    });

    // ── 2. Shadow & Bloom Station ─────────────────────────────────────────
    m_stations.push_back({
        "rendering_shadows",
        "Shadows & Bloom",
        "Directional shadow map (512x512, PCF 3x3) + LDR bloom post-process.",
        BiomeType::GRASSLAND,
        350.f, 500.f,
        "shadow_test"
    });

    // ── 3. Dynamic Sky & Weather Station ─────────────────────────────────
    m_stations.push_back({
        "sky_weather",
        "Dynamic Sky & Weather",
        "Procedural time-of-day, sun direction, fog density, rain intensity.",
        BiomeType::COAST,
        512.f, 700.f,
        "dynamic_sky"
    });

    // ── 4. Physics Station ────────────────────────────────────────────────
    m_stations.push_back({
        "physics_jolt",
        "Jolt Physics",
        "Rigid-body simulation, character capsule controller, raycast, hit volumes.",
        BiomeType::DESERT,
        680.f, 400.f,
        "physics_test"
    });

    // ── 5. 3D Audio Station ───────────────────────────────────────────────
    m_stations.push_back({
        "audio_3d",
        "3D Positional Audio",
        "X3DAudio DSP, distance attenuation, listener position, per-frame update.",
        BiomeType::FOREST,
        420.f, 350.f,
        "audio_3d_test"
    });

    // ── 6. Skeletal Animation Station ────────────────────────────────────
    m_stations.push_back({
        "animation_skinning",
        "GPU Skeletal Animation",
        "64-joint skinned mesh, GpuSkinningBuffer CB, Two-Bone IK + FABRIK.",
        BiomeType::GRASSLAND,
        512.f, 450.f,
        "skinned_mesh"
    });

    // ── 7. AI & Formation Station ─────────────────────────────────────────
    m_stations.push_back({
        "ai_formation",
        "Behaviour Trees & Formation",
        "BtTree FSM, FormationSystem (LINE/V/CIRCLE), A* NavMesh pathfinding.",
        BiomeType::FOREST,
        380.f, 600.f,
        "bt_test"
    });

    // ── 8. Quest & Dialogue Station ───────────────────────────────────────
    m_stations.push_back({
        "quests_dialogue",
        "Quests & Dialogue",
        "QuestSystem objective tracking + DialogueSystem in-range NPC conversation.",
        BiomeType::GRASSLAND,
        600.f, 520.f,
        "quest_test"
    });

    // ── 9. World Streaming Station ────────────────────────────────────────
    m_stations.push_back({
        "world_streaming",
        "World Streaming",
        "WorldStreamingManager async cell load/evict, AsyncLoader worker thread.",
        BiomeType::SNOW,
        512.f, 800.f,
        "streaming_load"
    });

    // ── 10. Terrain Station ───────────────────────────────────────────────
    m_stations.push_back({
        "terrain",
        "Terrain Renderer",
        "Heightmap-driven grid mesh, TRN1 binary format, height-based colour blend.",
        BiomeType::SNOW,
        450.f, 750.f,
        "terrain_test"
    });

    // ── 11. Cinematics Station ────────────────────────────────────────────
    m_stations.push_back({
        "cinematics",
        "Cinematics",
        "CinematicSequencer + CameraRig keyframes + timed audio events.",
        BiomeType::DESERT,
        650.f, 600.f,
        "cinematic_test"
    });

    // ── 12. Combat Station ────────────────────────────────────────────────
    m_stations.push_back({
        "combat",
        "Action Combat (ComboSystem)",
        "IDLE/BUILDING/COOLDOWN FSM, prefix-match combos, damage formula.",
        BiomeType::GRASSLAND,
        560.f, 470.f,
        "combat_test"
    });
}
