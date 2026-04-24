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
// TEACHING NOTE — Conditional JSON dependency
// ─────────────────────────────────────────────
// ENGINE_ENABLE_JSON is defined by CMake when nlohmann/json is available via
// vcpkg (see CMakeLists.txt: if(nlohmann_json_FOUND) … ENGINE_ENABLE_JSON).
// We guard the JSON include so open_world.cpp also compiles cleanly without
// vcpkg (e.g., the headless CI preset that does not install nlohmann-json).
// ---------------------------------------------------------------------------
#ifdef ENGINE_ENABLE_JSON
#  include <fstream>
#  include <nlohmann/json.hpp>
#endif

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

    // TEACHING NOTE — Overlay stations from JSON (when available)
    // ─────────────────────────────────────────────────────────────
    // TryLoadStationsFromJSON() replaces the C++ default station list with
    // data from the content file.  If the file is missing (e.g., first run
    // without content deployed) the C++ defaults from RegisterDefaultStations()
    // are kept intact.  This is the AAA "fallback data" pattern: ship sane
    // defaults in code; let external files override at runtime.
    TryLoadStationsFromJSON(
        "samples/vertical_slice_project/Content/World/teaching_stations.json");

    // TEACHING NOTE — Load station lesson content
    // ─────────────────────────────────────────────
    // station_lessons.json holds the per-station teaching explanations shown
    // when the player presses E (Interact) at a station.  Failure to load is
    // non-fatal — stations are still fully usable, just without lesson text.
    TryLoadLessonsFromJSON(
        "samples/vertical_slice_project/Content/World/station_lessons.json");

    // ── Quest & activity initialisation ──────────────────────────────────────
    // TEACHING NOTE — Chained data-driven initialisation
    // DemoQuestManager uses the same fallback pattern as stations:
    //   1. RegisterDefaults() builds valid in-code quest/activity definitions.
    //   2. TryLoadFromJSON() (inside Init) overlays content-authored data.
    // The JSON path is passed here so the quest manager can overlay from
    // demo_activities.json when ENGINE_ENABLE_JSON is active.
    m_quests.Init(
        "samples/vertical_slice_project/Content/World/demo_activities.json");

    // Start at the boot menu.
    m_state     = OpenWorldState::BOOT_MENU;
    m_stateTime = 0.f;

    std::cout << "[OpenWorld] Initialised — " << m_stations.size()
              << " teaching stations, " << m_lessons.size()
              << " lessons registered.\n";
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

            // TEACHING NOTE — Teleport is navigation only; no quest progress
            // ────────────────────────────────────────────────────────────────
            // Teleporting brings the player close to a station but does NOT
            // advance the quest or count as an "interact".  The player must
            // press E (Interact) after teleporting to trigger the lesson panel
            // and advance the current quest objective.
            //
            // This separates "navigation" (getting there) from "engagement"
            // (choosing to read the lesson) — a deliberate teaching-design
            // decision matching Issue #83's updated direction.
            //
            // We record the player's new world position and the nearest station
            // so the subsequent InteractAtStation() call can act on it.
            m_playerX          = s.worldX;
            m_playerZ          = s.worldZ;
            m_nearestStationID = stationID; // within interact range by definition
            return;
        }
    }
    std::cout << "[OpenWorld] Warning: station \"" << stationID << "\" not found.\n";
}

StationLesson OpenWorld::InteractAtStation()
{
    // TEACHING NOTE — Interact gate: player must be near a station
    // ─────────────────────────────────────────────────────────────
    // In a real engine with continuous movement this check would compute
    // distance from the player's live transform to each station's world
    // position each frame.  Here, m_nearestStationID is set by
    // TeleportToStation() (navigation) and cleared after a successful interact
    // so repeated presses on the same station don't spam the quest manager.
    if (m_nearestStationID.empty())
    {
        std::cout << "[OpenWorld] Interact: not near any station.\n";
        return StationLesson{}; // empty lesson = panel should not open
    }

    const std::string interactedID = m_nearestStationID;

    // Find the station record so we have display info for the log.
    const TeachingStation* stationPtr = nullptr;
    for (const auto& s : m_stations)
        if (s.id == interactedID) { stationPtr = &s; break; }

    const std::string displayName = stationPtr ? stationPtr->displayName : interactedID;
    std::cout << "[OpenWorld] Interact → station \"" << displayName << "\"\n";

    // ── Notify quest manager — this is the single trigger for quest progress ─
    // TEACHING NOTE — Single point of quest advancement
    // ───────────────────────────────────────────────────
    // DemoQuestManager::NotifyStationVisited() is called ONLY from here, not
    // from TeleportToStation().  This guarantees that quest objectives advance
    // only when the player deliberately interacts with a station via E key.
    m_quests.NotifyStationVisited(interactedID);

    // ── Return the lesson content for the lesson panel ───────────────────────
    StationLesson result;
    auto it = m_lessons.find(interactedID);
    if (it != m_lessons.end())
    {
        result = it->second;
    }
    else
    {
        // Built-in fallback lesson — always safe even without station_lessons.json.
        result.stationID   = interactedID;
        result.lessonTitle = displayName;
        result.lessonText  = "No lesson content loaded for this station.\n"
                             "Add an entry in Content/World/station_lessons.json\n"
                             "to provide teaching text and code pointers.";
    }

    return result;
}

const StationLesson* OpenWorld::GetStationLesson(const std::string& stationID) const noexcept
{
    auto it = m_lessons.find(stationID);
    return (it != m_lessons.end()) ? &it->second : nullptr;
}

void OpenWorld::NotifyEnemyKilled()
{
    m_quests.NotifyEnemyKilled();
}

void OpenWorld::NotifyItemCollected()
{
    m_quests.NotifyItemCollected();
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
// TryLoadStationsFromJSON
// ---------------------------------------------------------------------------
// TEACHING NOTE — JSON Data Override Pattern
// ─────────────────────────────────────────────────────────────────────────────
// This method replaces the C++ station list with data from a JSON content
// file.  It follows the "try-and-fallback" pattern common in AAA engines:
//
//   1. C++ code registers safe defaults (RegisterDefaultStations).
//   2. Runtime code attempts to load the content-authored overrides.
//   3. If the override file is missing or malformed, the defaults remain.
//
// ENGINE_ENABLE_JSON guards the entire implementation so the file compiles
// cleanly even when nlohmann/json is not installed (e.g., CI presets that
// skip vcpkg).
// ─────────────────────────────────────────────────────────────────────────────

bool OpenWorld::TryLoadStationsFromJSON(const std::string& jsonPath)
{
#ifdef ENGINE_ENABLE_JSON
    // ---- Open the JSON file ----
    std::ifstream file(jsonPath);
    if (!file.is_open())
    {
        // TEACHING NOTE — Silent fallback
        // We do NOT print an error here; a missing content file is a normal
        // condition during early development (the cook step may not have run
        // yet).  RegisterDefaultStations() already populated m_stations, so
        // the game is in a valid state.
        return false;
    }

    // ---- Parse ----
    nlohmann::json root;
    try
    {
        file >> root;
    }
    catch (const std::exception& ex)
    {
        std::cout << "[OpenWorld] teaching_stations.json parse error: "
                  << ex.what() << " — keeping C++ defaults.\n";
        return false;
    }

    // ---- Helper: map biome string to enum ----
    // TEACHING NOTE — String→Enum mapping
    // JSON stores biome names as human-readable strings ("grassland",
    // "forest", …).  The runtime uses the BiomeType enum for efficiency.
    // A small lambda (or free function) centralises the mapping so it is
    // easy to extend when new biomes are added.
    auto parseBiome = [](const std::string& s) -> BiomeType
    {
        if (s == "forest")    return BiomeType::FOREST;
        if (s == "snow")      return BiomeType::SNOW;
        if (s == "desert")    return BiomeType::DESERT;
        if (s == "coast")     return BiomeType::COAST;
        return BiomeType::GRASSLAND; // default for unknown strings
    };

    // ---- Build station list from JSON ----
    if (!root.contains("stations") || !root["stations"].is_array())
    {
        std::cout << "[OpenWorld] teaching_stations.json: missing \"stations\" array "
                     "— keeping C++ defaults.\n";
        return false;
    }

    std::vector<TeachingStation> loaded;
    for (const auto& obj : root["stations"])
    {
        try
        {
            TeachingStation s;
            s.id          = obj.at("id").get<std::string>();
            s.displayName = obj.at("displayName").get<std::string>();
            s.description = obj.value("description", "");
            s.biome       = parseBiome(obj.value("biome", "grassland"));
            s.worldX      = obj.value("worldX", 512.f);
            s.worldZ      = obj.value("worldZ", 512.f);
            s.sceneHint   = obj.value("sceneHint", "");

            if (!s.id.empty() && !s.displayName.empty())
                loaded.push_back(std::move(s));
        }
        catch (const std::exception& ex)
        {
            std::cout << "[OpenWorld] teaching_stations.json: skipping malformed "
                         "entry — " << ex.what() << "\n";
        }
    }

    if (loaded.empty())
    {
        std::cout << "[OpenWorld] teaching_stations.json: no valid stations found "
                     "— keeping C++ defaults.\n";
        return false;
    }

    // ---- Apply loaded stations ----
    m_stations = std::move(loaded);
    std::cout << "[OpenWorld] Loaded " << m_stations.size()
              << " teaching stations from " << jsonPath << ".\n";
    return true;

#else
    // ENGINE_ENABLE_JSON is OFF — JSON loading compiled out.
    (void)jsonPath;
    return false;
#endif // ENGINE_ENABLE_JSON
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

// ---------------------------------------------------------------------------
// TryLoadLessonsFromJSON
// ---------------------------------------------------------------------------
// TEACHING NOTE — Per-station lesson content loading
// ─────────────────────────────────────────────────────────────────────────────
// station_lessons.json stores the multi-line teaching text and code pointers
// shown in the Lesson Panel when the player presses E (Interact) at a station.
//
// The format is intentionally simple — a flat array of lesson objects keyed
// by stationID.  This makes it easy for course authors to add / edit lessons
// without touching C++ code.
//
// Same safe-fallback pattern as TryLoadStationsFromJSON:
//   • Missing file → silent fallback (built-in stub lesson in InteractAtStation).
//   • Malformed JSON → log + keep existing lessons map empty.
// ─────────────────────────────────────────────────────────────────────────────

bool OpenWorld::TryLoadLessonsFromJSON(const std::string& jsonPath)
{
#ifdef ENGINE_ENABLE_JSON
    std::ifstream file(jsonPath);
    if (!file.is_open())
        return false; // silent — missing lesson file is non-fatal

    nlohmann::json root;
    try
    {
        file >> root;
    }
    catch (const std::exception& ex)
    {
        std::cout << "[OpenWorld] station_lessons.json parse error: "
                  << ex.what() << " — lesson panel will show stub text.\n";
        return false;
    }

    if (!root.contains("lessons") || !root["lessons"].is_array())
    {
        std::cout << "[OpenWorld] station_lessons.json: missing \"lessons\" array.\n";
        return false;
    }

    for (const auto& obj : root["lessons"])
    {
        try
        {
            StationLesson lesson;
            lesson.stationID   = obj.at("stationID").get<std::string>();
            lesson.lessonTitle = obj.value("lessonTitle", "");
            lesson.lessonText  = obj.value("lessonText",  "");

            if (obj.contains("codePointers") && obj["codePointers"].is_array())
            {
                for (const auto& ptr : obj["codePointers"])
                    lesson.codePointers.push_back(ptr.get<std::string>());
            }

            if (obj.contains("relatedStations") && obj["relatedStations"].is_array())
            {
                for (const auto& rel : obj["relatedStations"])
                    lesson.relatedStations.push_back(rel.get<std::string>());
            }

            if (!lesson.stationID.empty() && lesson.IsValid())
                m_lessons[lesson.stationID] = std::move(lesson);
        }
        catch (const std::exception& ex)
        {
            std::cout << "[OpenWorld] station_lessons.json: skipping malformed "
                         "entry — " << ex.what() << "\n";
        }
    }

    std::cout << "[OpenWorld] Loaded " << m_lessons.size()
              << " station lessons from " << jsonPath << ".\n";
    return !m_lessons.empty();

#else
    (void)jsonPath;
    return false;
#endif // ENGINE_ENABLE_JSON
}
