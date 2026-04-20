/**
 * @file save_system.cpp
 * @brief Production save system implementation — M8.8.
 *
 * TEACHING NOTE — Build-time JSON gate
 * ──────────────────────────────────────
 * All nlohmann/json calls live inside #ifdef ENGINE_ENABLE_JSON blocks.
 * When vcpkg is not configured the entire save/load path falls back to a
 * stub that logs a friendly message and returns false.  The game still runs.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2025
 */

#include "engine/save/save_system.hpp"
#include "engine/core/Logger.hpp"

#include <filesystem>   // std::filesystem::create_directories
#include <fstream>      // std::ifstream, std::ofstream
#include <sstream>
#include <chrono>
#include <ctime>

#ifdef ENGINE_ENABLE_JSON
#   include <nlohmann/json.hpp>
    using json = nlohmann::json;
#endif

namespace fs = std::filesystem;

namespace engine {
namespace save {

// ===========================================================================
// Helpers
// ===========================================================================
namespace {

/// Build an ISO 8601 timestamp for "now" (e.g. "2025-01-15T12:34:56Z").
std::string NowISO8601()
{
    const auto now  = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    struct tm  buf  {};
#ifdef _WIN32
    gmtime_s(&buf, &time);
#else
    gmtime_r(&time, &buf);
#endif
    char out[32];
    std::strftime(out, sizeof(out), "%Y-%m-%dT%H:%M:%SZ", &buf);
    return std::string(out);
}

} // namespace anonymous

// ===========================================================================
// Constructor
// ===========================================================================

SaveSystem::SaveSystem(const std::string& saveDir)
    : m_saveDir(saveDir)
{
    LOG_INFO("SaveSystem created (dir=" << m_saveDir << ")");
}

// ===========================================================================
// SetSaveDirectory
// ===========================================================================

void SaveSystem::SetSaveDirectory(const std::string& dir)
{
    m_saveDir = dir;
    LOG_INFO("SaveSystem: save directory set to " << m_saveDir);
}

// ===========================================================================
// SlotPath
// ===========================================================================

std::string SaveSystem::SlotPath(int slot) const
{
    // TEACHING NOTE — slot 15 = auto-save; slots 0–14 = numbered saves.
    const std::string name = (slot == kAutoSaveSlot)
        ? "save_auto.json"
        : ("save_" + std::to_string(slot) + ".json");
    return m_saveDir + name;
}

// ===========================================================================
// SlotExists
// ===========================================================================

bool SaveSystem::SlotExists(int slot) const
{
    return fs::exists(SlotPath(slot));
}

// ===========================================================================
// DeleteSlot
// ===========================================================================

bool SaveSystem::DeleteSlot(int slot) const
{
    const std::string path = SlotPath(slot);
    if (!fs::exists(path)) return true;
    std::error_code ec;
    fs::remove(path, ec);
    if (ec) {
        LOG_ERROR("SaveSystem::DeleteSlot(" << slot << "): " << ec.message());
        return false;
    }
    return true;
}

// ===========================================================================
// Save
// ===========================================================================

bool SaveSystem::Save(World& world, EntityID playerID,
                       int slot, float gameTimeSecs,
                       const std::string& locationName)
{
    if (slot < 0 || slot > kAutoSaveSlot)
    {
        LOG_ERROR("SaveSystem::Save: invalid slot " << slot
                  << " (valid: 0–" << kAutoSaveSlot << ")");
        return false;
    }

#ifndef ENGINE_ENABLE_JSON
    // TEACHING NOTE — Graceful degradation without JSON support.
    LOG_WARN("SaveSystem::Save: ENGINE_ENABLE_JSON not defined. "
             "Build with nlohmann/json via vcpkg to enable saving.");
    return false;
#else
    // -----------------------------------------------------------------------
    // Create the save directory if it doesn't exist.
    // -----------------------------------------------------------------------
    std::error_code ec;
    fs::create_directories(m_saveDir, ec);
    if (ec)
    {
        LOG_ERROR("SaveSystem: cannot create save directory '"
                  << m_saveDir << "': " << ec.message());
        return false;
    }

    // -----------------------------------------------------------------------
    // Build the JSON document.
    // -----------------------------------------------------------------------
    json root;
    root["version"]      = std::string(kSaveFormatVersion);
    root["savedAt"]      = NowISO8601();
    root["gameTimeSecs"] = gameTimeSecs;
    root["locationName"] = locationName;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Iterating living entities.
    // -----------------------------------------------------------------------
    // We query every entity that has at least a TransformComponent (the most
    // fundamental component), then check for each other component individually.
    // This means entities with NO TransformComponent are NOT saved — that is
    // intentional; pure "controller" entities (e.g. a camera without a body)
    // don't need to be persisted.

    json entities = json::array();

    world.View<TransformComponent>(
        [&](EntityID eid, TransformComponent& tf)
        {
            json ent;
            ent["id"] = static_cast<uint32_t>(eid);

            json comps;

            // ---- Transform ------------------------------------------------
            {
                json c;
                c["px"] = tf.position.x;  c["py"] = tf.position.y;  c["pz"] = tf.position.z;
                c["rx"] = tf.rotation.x;  c["ry"] = tf.rotation.y;  c["rz"] = tf.rotation.z;
                c["sx"] = tf.scale.x;     c["sy"] = tf.scale.y;     c["sz"] = tf.scale.z;
                comps[std::string(kTagTransform)] = c;
            }

            // ---- Health ---------------------------------------------------
            if (world.HasComponent<HealthComponent>(eid))
            {
                const auto& h = world.GetComponent<HealthComponent>(eid);
                json c;
                c["hp"] = h.hp;  c["maxHp"] = h.maxHp;
                c["mp"] = h.mp;  c["maxMp"] = h.maxMp;
                comps[std::string(kTagHealth)] = c;
            }

            // ---- Stats ----------------------------------------------------
            if (world.HasComponent<StatsComponent>(eid))
            {
                const auto& s = world.GetComponent<StatsComponent>(eid);
                json c;
                c["strength"] = s.strength; c["defence"] = s.defence;
                c["magic"]    = s.magic;    c["spirit"]  = s.spirit;
                c["speed"]    = s.speed;    c["luck"]    = s.luck;
                comps[std::string(kTagStats)] = c;
            }

            // ---- Name -----------------------------------------------------
            if (world.HasComponent<NameComponent>(eid))
            {
                const auto& n = world.GetComponent<NameComponent>(eid);
                json c;
                c["name"]       = n.name;
                c["internalID"] = n.internalID;
                c["title"]      = n.title;
                comps[std::string(kTagName)] = c;
            }

            // ---- Movement -------------------------------------------------
            if (world.HasComponent<MovementComponent>(eid))
            {
                const auto& m = world.GetComponent<MovementComponent>(eid);
                json c;
                c["moveSpeed"]   = m.moveSpeed;
                c["sprintSpeed"] = m.sprintSpeed;
                comps[std::string(kTagMovement)] = c;
            }

            // ---- Combat ---------------------------------------------------
            if (world.HasComponent<CombatComponent>(eid))
            {
                const auto& cb = world.GetComponent<CombatComponent>(eid);
                json c;
                c["isInCombat"]   = cb.isInCombat;
                c["attackRate"]   = cb.attackRate;
                c["xpReward"]     = cb.xpReward;
                c["gilReward"]    = cb.gilReward;
                comps[std::string(kTagCombat)] = c;
            }

            // ---- Level ----------------------------------------------------
            if (world.HasComponent<LevelComponent>(eid))
            {
                const auto& lv = world.GetComponent<LevelComponent>(eid);
                json c;
                c["level"]     = lv.level;
                c["currentXP"] = lv.currentXP;
                comps[std::string(kTagLevel)] = c;
            }

            // ---- Currency -------------------------------------------------
            if (world.HasComponent<CurrencyComponent>(eid))
            {
                const auto& cur = world.GetComponent<CurrencyComponent>(eid);
                json c;
                c["gil"]         = static_cast<uint64_t>(cur.gil);
                c["crownTokens"] = static_cast<uint32_t>(cur.crownTokens);
                comps[std::string(kTagCurrency)] = c;
            }

            // ---- Quest ----------------------------------------------------
            if (world.HasComponent<QuestComponent>(eid))
            {
                const auto& qc = world.GetComponent<QuestComponent>(eid);
                json questsArr = json::array();
                for (uint32_t q = 0; q < qc.activeCount; ++q)
                {
                    const auto& qe = qc.quests[q];
                    json qj;
                    qj["questID"]    = qe.questID;
                    qj["objective"]  = qe.objective;
                    qj["progress"]   = qe.progress;
                    qj["required"]   = qe.required;
                    qj["isComplete"] = qe.isComplete;
                    qj["isFailed"]   = qe.isFailed;
                    questsArr.push_back(qj);
                }
                comps[std::string(kTagQuest)] = questsArr;
            }

            // ---- Magic ----------------------------------------------------
            if (world.HasComponent<MagicComponent>(eid))
            {
                const auto& mg = world.GetComponent<MagicComponent>(eid);
                json c;
                c["equippedSpell"] = mg.equippedSpell;
                comps[std::string(kTagMagic)] = c;
            }

            ent["components"] = comps;
            entities.push_back(ent);
        });

    root["entities"] = entities;

    // Metadata for quick header reads (load-game menu).
    if (world.IsAlive(playerID))
    {
        if (world.HasComponent<NameComponent>(playerID))
            root["playerName"] = world.GetComponent<NameComponent>(playerID).name;
        if (world.HasComponent<LevelComponent>(playerID))
            root["playerLevel"] = world.GetComponent<LevelComponent>(playerID).level;
    }

    // -----------------------------------------------------------------------
    // Write to file.
    // -----------------------------------------------------------------------
    const std::string path = SlotPath(slot);
    std::ofstream ofs(path);
    if (!ofs.is_open())
    {
        LOG_ERROR("SaveSystem::Save: cannot open '" << path << "' for writing.");
        return false;
    }
    ofs << root.dump(2);
    ofs.close();

    LOG_INFO("SaveSystem: saved slot " << slot << " → " << path
             << " (" << world.GetEntityCount() << " entities)");
    return true;
#endif // ENGINE_ENABLE_JSON
}

// ===========================================================================
// AutoSave
// ===========================================================================

bool SaveSystem::AutoSave(World& world, EntityID playerID,
                           float gameTimeSecs,
                           const std::string& locationName)
{
    return Save(world, playerID, kAutoSaveSlot, gameTimeSecs, locationName);
}

// ===========================================================================
// Load
// ===========================================================================

bool SaveSystem::Load(World& world, int slot)
{
#ifndef ENGINE_ENABLE_JSON
    LOG_WARN("SaveSystem::Load: ENGINE_ENABLE_JSON not defined. "
             "Build with nlohmann/json via vcpkg to enable loading.");
    return false;
#else
    const std::string path = SlotPath(slot);
    if (!fs::exists(path))
    {
        LOG_WARN("SaveSystem::Load: slot " << slot << " not found at " << path);
        return false;
    }

    std::ifstream ifs(path);
    if (!ifs.is_open())
    {
        LOG_ERROR("SaveSystem::Load: cannot open '" << path << "' for reading.");
        return false;
    }

    json root;
    try {
        ifs >> root;
    } catch (const std::exception& ex) {
        LOG_ERROR("SaveSystem::Load: JSON parse error: " << ex.what());
        return false;
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Version check and migration ladder.
    // -----------------------------------------------------------------------
    // Compare stored version against the current format version.
    // Future migrations would be: if (storedVersion == "0.9.0") { ... }.
    // For now at version 1.0.0 there are no older versions to migrate from.

    const std::string storedVersion = root.value("version", "0.0.0");
    if (storedVersion != std::string(kSaveFormatVersion))
    {
        LOG_WARN("SaveSystem::Load: version mismatch "
                 "(file=" << storedVersion
                 << " current=" << std::string(kSaveFormatVersion)
                 << ") — attempting forward migration.");
        // Migration placeholder: if storedVersion == "X.Y.Z" transform root.
        // For now accept anyway (additive changes are backward compatible).
    }

    // -----------------------------------------------------------------------
    // Destroy all existing entities and reset the World.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Snapshot load strategy
    // We collect all living entity IDs first (modifying a collection you are
    // iterating is undefined behaviour), then destroy them in a second pass.

    std::vector<EntityID> livingNow;
    world.View<TransformComponent>(
        [&](EntityID eid, TransformComponent&) { livingNow.push_back(eid); });

    // Also catch entities without TransformComponent.
    // Since ECS does not have a "get all entities" API, we approximate by
    // collecting from a broad component + the above.
    world.View<NameComponent>(
        [&](EntityID eid, NameComponent&) {
            if (std::find(livingNow.begin(), livingNow.end(), eid) == livingNow.end())
                livingNow.push_back(eid);
        });

    for (EntityID eid : livingNow)
        world.DestroyEntity(eid);

    // -----------------------------------------------------------------------
    // Recreate entities from JSON.
    // -----------------------------------------------------------------------
    const auto& entities = root["entities"];
    for (const auto& entJson : entities)
    {
        EntityID eid = world.CreateEntity();
        if (eid == NULL_ENTITY)
        {
            LOG_ERROR("SaveSystem::Load: entity pool exhausted during load.");
            return false;
        }

        const auto& comps = entJson["components"];

        // ---- Transform ----------------------------------------------------
        if (comps.contains(std::string(kTagTransform)))
        {
            const auto& c = comps[std::string(kTagTransform)];
            auto& tf = world.AddComponent<TransformComponent>(eid);
            tf.position = { c.value("px", 0.0f), c.value("py", 0.0f), c.value("pz", 0.0f) };
            tf.rotation = { c.value("rx", 0.0f), c.value("ry", 0.0f), c.value("rz", 0.0f) };
            tf.scale    = { c.value("sx", 1.0f), c.value("sy", 1.0f), c.value("sz", 1.0f) };
        }

        // ---- Health -------------------------------------------------------
        if (comps.contains(std::string(kTagHealth)))
        {
            const auto& c = comps[std::string(kTagHealth)];
            auto& h = world.AddComponent<HealthComponent>(eid);
            h.hp    = c.value("hp",    100);
            h.maxHp = c.value("maxHp", 100);
            h.mp    = c.value("mp",    50);
            h.maxMp = c.value("maxMp", 50);
        }

        // ---- Stats --------------------------------------------------------
        if (comps.contains(std::string(kTagStats)))
        {
            const auto& c = comps[std::string(kTagStats)];
            auto& s = world.AddComponent<StatsComponent>(eid);
            s.strength = c.value("strength", 10);
            s.defence  = c.value("defence",  5);
            s.magic    = c.value("magic",    5);
            s.spirit   = c.value("spirit",   5);
            s.speed    = c.value("speed",    5);
            s.luck     = c.value("luck",     5);
        }

        // ---- Name ---------------------------------------------------------
        if (comps.contains(std::string(kTagName)))
        {
            const auto& c = comps[std::string(kTagName)];
            auto& n = world.AddComponent<NameComponent>(eid);
            n.name       = c.value("name",       "");
            n.internalID = c.value("internalID", "");
            n.title      = c.value("title",      "");
        }

        // ---- Movement -----------------------------------------------------
        if (comps.contains(std::string(kTagMovement)))
        {
            const auto& c = comps[std::string(kTagMovement)];
            auto& m = world.AddComponent<MovementComponent>(eid);
            m.moveSpeed   = c.value("moveSpeed",   4.0f);
            m.sprintSpeed = c.value("sprintSpeed", 8.0f);
        }

        // ---- Combat -------------------------------------------------------
        if (comps.contains(std::string(kTagCombat)))
        {
            const auto& c = comps[std::string(kTagCombat)];
            auto& cb = world.AddComponent<CombatComponent>(eid);
            cb.isInCombat = c.value("isInCombat", false);
            cb.attackRate = c.value("attackRate",  1.0f);
            cb.xpReward   = c.value("xpReward",    0);
            cb.gilReward  = c.value("gilReward",   0);
        }

        // ---- Level --------------------------------------------------------
        if (comps.contains(std::string(kTagLevel)))
        {
            const auto& c = comps[std::string(kTagLevel)];
            auto& lv = world.AddComponent<LevelComponent>(eid);
            lv.level     = c.value("level",     1);
            lv.currentXP = c.value("currentXP", 0);
        }

        // ---- Currency -----------------------------------------------------
        if (comps.contains(std::string(kTagCurrency)))
        {
            const auto& c = comps[std::string(kTagCurrency)];
            auto& cur = world.AddComponent<CurrencyComponent>(eid);
            cur.gil         = c.value("gil",         uint64_t(0));
            cur.crownTokens = c.value("crownTokens", uint32_t(0));
        }

        // ---- Quest --------------------------------------------------------
        if (comps.contains(std::string(kTagQuest)))
        {
            auto& qc = world.AddComponent<QuestComponent>(eid);
            const auto& questsArr = comps[std::string(kTagQuest)];
            uint32_t q = 0;
            for (const auto& qj : questsArr)
            {
                if (q >= MAX_QUESTS) break;
                auto& qe      = qc.quests[q];
                qe.questID    = qj.value("questID",    0u);
                qe.objective  = qj.value("objective",  0u);
                qe.progress   = qj.value("progress",   0);
                qe.required   = qj.value("required",   1);
                qe.isComplete = qj.value("isComplete", false);
                qe.isFailed   = qj.value("isFailed",   false);
                ++q;
            }
            qc.activeCount = q;
        }

        // ---- Magic --------------------------------------------------------
        if (comps.contains(std::string(kTagMagic)))
        {
            const auto& c = comps[std::string(kTagMagic)];
            auto& mg = world.AddComponent<MagicComponent>(eid);
            mg.equippedSpell = c.value("equippedSpell", "");
        }
    }

    LOG_INFO("SaveSystem::Load: loaded slot " << slot
             << " (version=" << storedVersion
             << ", " << world.GetEntityCount() << " entities)");
    return true;
#endif // ENGINE_ENABLE_JSON
}

// ===========================================================================
// ReadMetadata
// ===========================================================================

SaveMetadata SaveSystem::ReadMetadata(int slot) const
{
    SaveMetadata meta;
    meta.slot = slot;

    const std::string path = SlotPath(slot);
    if (!fs::exists(path)) return meta;  // exists = false by default

#ifndef ENGINE_ENABLE_JSON
    // Without JSON support we can only report existence.
    meta.exists = true;
    return meta;
#else
    std::ifstream ifs(path);
    if (!ifs.is_open()) return meta;

    try {
        json root;
        ifs >> root;

        meta.exists       = true;
        meta.version      = root.value("version",      "?");
        meta.savedAt      = root.value("savedAt",       "");
        meta.gameTimeSecs = root.value("gameTimeSecs",  0.0f);
        meta.playerName   = root.value("playerName",   "?");
        meta.playerLevel  = root.value("playerLevel",  1);
        meta.locationName = root.value("locationName", "?");
    } catch (...) {
        LOG_WARN("SaveSystem::ReadMetadata: failed to parse slot " << slot);
    }
    return meta;
#endif
}

// ===========================================================================
// ReadAllMetadata
// ===========================================================================

std::vector<SaveMetadata> SaveSystem::ReadAllMetadata() const
{
    std::vector<SaveMetadata> metas;
    metas.reserve(kTotalSlots);
    for (int s = 0; s < kTotalSlots; ++s)
        metas.push_back(ReadMetadata(s));
    return metas;
}

} // namespace save
} // namespace engine
