/**
 * @file scene_serialiser.cpp
 * @brief SceneSerialiser implementation — JSON ↔ ECS World round-trip.
 *
 * =============================================================================
 * TEACHING NOTE — Separating Interface from Implementation
 * =============================================================================
 * scene_serialiser.hpp declares the PUBLIC API: what callers can call.
 * This .cpp file contains the IMPLEMENTATION: the nlohmann/json include,
 * the ECS header, and all the serialisation details.
 *
 * This separation is important for build performance.  Including ECS.hpp
 * (~2000 lines of templates) in scene_serialiser.hpp would force EVERY
 * file that calls SceneSerialiser::SaveScene() to recompile those templates.
 * By keeping ECS.hpp in the .cpp only, the template expansion happens once.
 *
 * =============================================================================
 * TEACHING NOTE — nlohmann/json usage pattern
 * =============================================================================
 * nlohmann::json can be built as either:
 *   a) A Python-dict-like aggregate:  json j; j["hp"] = 100;
 *   b) From an initialiser list:  json j = {{"hp", 100}, {"maxHp", 200}};
 *   c) Parsed from a string/stream: json j = json::parse(stream);
 *
 * We use all three styles here as appropriate.  j.value("key", default)
 * safely reads with a fallback — it never throws on a missing key.
 *
 * =============================================================================
 */

#include "scene_serialiser.hpp"

// Full ECS headers are included HERE (not in the .hpp) to minimise
// recompilation when ECS.hpp changes.
#include "engine/ecs/ECS.hpp"
#include "engine/core/Logger.hpp"

// nlohmann/json: header-only JSON library (MIT, installed via vcpkg).
// TEACHING NOTE — <nlohmann/json.hpp> vs "nlohmann/json.hpp"
// Angle brackets search the system/vcpkg include path set by CMake.
// This is the correct form for a vcpkg-installed library.
#ifdef ENGINE_ENABLE_JSON
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#endif

#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

// =============================================================================
// Internal helpers
// =============================================================================

#ifdef ENGINE_ENABLE_JSON

namespace
{
    // -------------------------------------------------------------------------
    // TEACHING NOTE — ISO-8601 Timestamp
    // -------------------------------------------------------------------------
    // A "savedAt" timestamp lets tools and version control track when a scene
    // was last authored.  We generate it from std::chrono::system_clock, the
    // same approach used by SceneEditorPanel in the editor.
    // -------------------------------------------------------------------------
    static std::string MakeTimestamp()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::ostringstream ss;
        ss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    }

    // -------------------------------------------------------------------------
    // SerialiseEntity — turn one ECS entity into a JSON object
    // -------------------------------------------------------------------------
    // TEACHING NOTE — Per-component serialisation
    // Each component type is checked with World::HasComponent<T>().
    // If present, its fields are written into the JSON components map.
    // We only write components that the entity actually owns — sparse storage
    // means it is common for an entity to have 3–5 of the 24 components.
    // -------------------------------------------------------------------------
    static json SerialiseEntity(const World& world, EntityID entity)
    {
        json je;

        // ---- Stable string ID -----------------------------------------------
        // EntityID values are recycled between sessions (they are just array
        // indices).  We serialize the numeric ID as a string to match the UUID
        // field expected by scene.schema.json.  In a production engine you would
        // assign a stable UUID to each entity and store it in a UUIDComponent.
        je["id"] = std::to_string(entity);

        // ---- NameComponent --------------------------------------------------
        if (world.HasComponent<NameComponent>(entity))
        {
            const auto& nc = world.GetComponent<NameComponent>(entity);
            je["name"]       = nc.name;
            je["internalID"] = nc.internalID;
            if (!nc.title.empty())
                je["title"] = nc.title;
        }
        else
        {
            je["name"] = "Entity_" + std::to_string(entity);
        }

        // ---- TransformComponent — stored in a flat "transform" object -------
        // TEACHING NOTE — Separate "transform" vs "components" layout
        // Transform is so fundamental that scene.schema.json promotes it to a
        // top-level "transform" field rather than nesting it inside "components".
        // This mirrors the Unity and Unreal convention where Transform is always
        // present and displayed at the top of the inspector.
        if (world.HasComponent<TransformComponent>(entity))
        {
            const auto& tc = world.GetComponent<TransformComponent>(entity);
            je["transform"] = {
                { "x",  tc.position.x }, { "y",  tc.position.y }, { "z",  tc.position.z },
                { "rx", tc.rotation.x }, { "ry", tc.rotation.y }, { "rz", tc.rotation.z },
                { "sx", tc.scale.x    }, { "sy", tc.scale.y    }, { "sz", tc.scale.z    }
            };
        }
        else
        {
            je["transform"] = { {"x",0},{"y",0},{"z",0},
                                 {"rx",0},{"ry",0},{"rz",0},
                                 {"sx",1},{"sy",1},{"sz",1} };
        }

        // ---- Optional components go into je["components"] -------------------
        json comps = json::object();

        if (world.HasComponent<HealthComponent>(entity))
        {
            const auto& hc = world.GetComponent<HealthComponent>(entity);
            comps["HealthComponent"] = {
                { "hp",         hc.hp      }, { "maxHp",     hc.maxHp     },
                { "mp",         hc.mp      }, { "maxMp",     hc.maxMp     },
                { "regenRate",  hc.regenRate }, { "mpRegenRate", hc.mpRegenRate }
            };
        }

        if (world.HasComponent<StatsComponent>(entity))
        {
            const auto& sc = world.GetComponent<StatsComponent>(entity);
            comps["StatsComponent"] = {
                { "strength",    sc.strength    }, { "defence",     sc.defence     },
                { "magic",       sc.magic       }, { "spirit",      sc.spirit      },
                { "speed",       sc.speed       }, { "luck",        sc.luck        },
                { "vitality",    sc.vitality    }, { "critRate",    sc.critRate    },
                { "critMultiplier", sc.critMultiplier                              }
            };
        }

        if (world.HasComponent<RenderComponent>(entity))
        {
            const auto& rc = world.GetComponent<RenderComponent>(entity);
            comps["RenderComponent"] = {
                { "spriteSheet", rc.spriteSheet },
                { "zOrder",      rc.zOrder      },
                { "isVisible",   rc.isVisible   }
            };
        }

        if (world.HasComponent<LevelComponent>(entity))
        {
            const auto& lc = world.GetComponent<LevelComponent>(entity);
            comps["LevelComponent"] = {
                { "level",     lc.level     },
                { "currentXP", lc.currentXP }
            };
        }

        if (world.HasComponent<AnimatorComponent>(entity))
        {
            const auto& ac = world.GetComponent<AnimatorComponent>(entity);
            comps["AnimatorComponent"] = {
                { "skeletonID",    ac.skeletonID    },
                { "currentClipID", ac.currentClipID },
                { "blendTreeID",   ac.blendTreeID   },
                { "playbackSpeed", ac.playbackSpeed }
            };
        }

        if (!comps.empty())
            je["components"] = comps;

        return je;
    }

    // -------------------------------------------------------------------------
    // DeserialiseEntity — create an ECS entity and populate its components
    // -------------------------------------------------------------------------
    static void DeserialiseEntity(World& world, const json& je)
    {
        EntityID entity = world.CreateEntity();

        // ---- NameComponent --------------------------------------------------
        {
            auto& nc     = world.AddComponent<NameComponent>(entity);
            nc.name       = je.value("name", "Entity");
            nc.internalID = je.value("internalID", "");
            nc.title      = je.value("title", "");
        }

        // ---- TransformComponent ---------------------------------------------
        {
            auto& tc = world.AddComponent<TransformComponent>(entity);
            if (je.contains("transform") && je["transform"].is_object())
            {
                const auto& t = je["transform"];
                tc.position = { t.value("x",0.f),  t.value("y",0.f),  t.value("z",0.f)  };
                tc.rotation = { t.value("rx",0.f), t.value("ry",0.f), t.value("rz",0.f) };
                tc.scale    = { t.value("sx",1.f), t.value("sy",1.f), t.value("sz",1.f) };
            }
        }

        // ---- Optional components --------------------------------------------
        if (!je.contains("components") || !je["components"].is_object())
            return;

        const auto& comps = je["components"];

        if (comps.contains("HealthComponent"))
        {
            const auto& hj  = comps["HealthComponent"];
            auto& hc         = world.AddComponent<HealthComponent>(entity);
            hc.hp            = hj.value("hp",         100);
            hc.maxHp         = hj.value("maxHp",      100);
            hc.mp            = hj.value("mp",          50);
            hc.maxMp         = hj.value("maxMp",       50);
            hc.regenRate     = hj.value("regenRate",   0.f);
            hc.mpRegenRate   = hj.value("mpRegenRate", 2.f);
        }

        if (comps.contains("StatsComponent"))
        {
            const auto& sj = comps["StatsComponent"];
            auto& sc        = world.AddComponent<StatsComponent>(entity);
            sc.strength     = sj.value("strength",    10);
            sc.defence      = sj.value("defence",      5);
            sc.magic        = sj.value("magic",       10);
            sc.spirit       = sj.value("spirit",       5);
            sc.speed        = sj.value("speed",       10);
            sc.luck         = sj.value("luck",         5);
            sc.vitality     = sj.value("vitality",    10);
            sc.critRate     = sj.value("critRate",     5);
            sc.critMultiplier = sj.value("critMultiplier", 200);
        }

        if (comps.contains("RenderComponent"))
        {
            const auto& rj  = comps["RenderComponent"];
            auto& rc         = world.AddComponent<RenderComponent>(entity);
            rc.spriteSheet   = rj.value("spriteSheet", "");
            rc.zOrder        = rj.value("zOrder",        0);
            rc.isVisible     = rj.value("isVisible",  true);
        }

        if (comps.contains("LevelComponent"))
        {
            const auto& lj = comps["LevelComponent"];
            auto& lc        = world.AddComponent<LevelComponent>(entity);
            lc.level        = lj.value("level",     1);
            lc.currentXP    = lj.value("currentXP", 0);
        }

        if (comps.contains("AnimatorComponent"))
        {
            const auto& aj    = comps["AnimatorComponent"];
            auto& ac           = world.AddComponent<AnimatorComponent>(entity);
            ac.skeletonID      = aj.value("skeletonID",    "");
            ac.currentClipID   = aj.value("currentClipID", "");
            ac.blendTreeID     = aj.value("blendTreeID",   "");
            ac.playbackSpeed   = aj.value("playbackSpeed", 1.f);
        }
    }

} // anonymous namespace

#endif // ENGINE_ENABLE_JSON

// =============================================================================
// SceneSerialiser::SaveScene
// =============================================================================

bool SceneSerialiser::SaveScene(const World& world,
                                const std::string& filePath,
                                const std::string& sceneName)
{
#ifndef ENGINE_ENABLE_JSON
    LOG_ERROR("SceneSerialiser::SaveScene — ENGINE_ENABLE_JSON not defined; "
              "nlohmann/json not available.");
    (void)world; (void)filePath; (void)sceneName;
    return false;
#else
    // TEACHING NOTE — Ensure the parent directory exists
    // std::filesystem::create_directories() is a no-op if the path already
    // exists and creates ALL intermediate directories if it does not.
    // This mirrors how the Python cook script calls os.makedirs(exist_ok=True).
    fs::create_directories(fs::path(filePath).parent_path());

    json root;
    root["$schema"] = "../../shared/schemas/scene.schema.json";
    root["version"] = "1.0.0";
    root["name"]    = sceneName;
    root["meta"]    = {
        { "savedAt",       MakeTimestamp() },
        { "serialiserVersion", "1.0.0"     }
    };

    json entitiesArr = json::array();
    std::vector<EntityID> living;
    world.GetEntityManager().GetLivingEntities(living);
    for (EntityID entity : living)
        entitiesArr.push_back(SerialiseEntity(world, entity));

    root["entities"] = entitiesArr;

    std::ofstream ofs(filePath);
    if (!ofs)
    {
        LOG_ERROR("SceneSerialiser::SaveScene — cannot open file for writing: " << filePath);
        return false;
    }

    // dump(4) = 4-space indented pretty-print — readable by humans and diffs.
    ofs << root.dump(4);
    const bool ok = ofs.good();
    if (!ok)
        LOG_ERROR("SceneSerialiser::SaveScene — write error for: " << filePath);
    else
        LOG_INFO("SceneSerialiser::SaveScene — saved " << living.size()
                 << " entities to: " << filePath);
    return ok;
#endif
}

// =============================================================================
// SceneSerialiser::LoadScene
// =============================================================================

bool SceneSerialiser::LoadScene(World& world,
                                const std::string& filePath)
{
#ifndef ENGINE_ENABLE_JSON
    LOG_ERROR("SceneSerialiser::LoadScene — ENGINE_ENABLE_JSON not defined.");
    (void)world; (void)filePath;
    return false;
#else
    std::ifstream ifs(filePath);
    if (!ifs)
    {
        LOG_ERROR("SceneSerialiser::LoadScene — file not found: " << filePath);
        return false;
    }

    json root;
    try
    {
        root = json::parse(ifs);
    }
    catch (const json::exception& ex)
    {
        LOG_ERROR("SceneSerialiser::LoadScene — JSON parse error in "
                  << filePath << ": " << ex.what());
        return false;
    }

    if (!root.contains("entities") || !root["entities"].is_array())
    {
        LOG_ERROR("SceneSerialiser::LoadScene — missing 'entities' array in: " << filePath);
        return false;
    }

    int count = 0;
    for (const auto& je : root["entities"])
    {
        DeserialiseEntity(world, je);
        ++count;
    }

    LOG_INFO("SceneSerialiser::LoadScene — loaded " << count
             << " entities from: " << filePath);
    return true;
#endif
}

// =============================================================================
// SceneSerialiser::CountEntities
// =============================================================================

int SceneSerialiser::CountEntities(const std::string& filePath)
{
#ifndef ENGINE_ENABLE_JSON
    (void)filePath;
    return -1;
#else
    std::ifstream ifs(filePath);
    if (!ifs) return -1;
    json root;
    try { root = json::parse(ifs); }
    catch (...) { return -1; }
    if (!root.contains("entities") || !root["entities"].is_array())
        return -1;
    return static_cast<int>(root["entities"].size());
#endif
}
