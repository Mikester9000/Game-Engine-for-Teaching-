/**
 * @file scene_serialiser.hpp
 * @brief SceneSerialiser — JSON save/load for ECS World snapshots.
 *
 * =============================================================================
 * TEACHING NOTE — Scene Serialisation Pattern
 * =============================================================================
 * "Serialisation" converts in-memory data (an ECS World) to a storable format
 * (JSON text), and "deserialisation" does the reverse.
 *
 * This pattern appears in every commercial game engine:
 *   • Unity       — uses YAML scene files with component snapshots.
 *   • Unreal      — uses a binary format (.umap) with optional text export.
 *   • Godot       — uses a custom text format (.tscn / .tres).
 *
 * We use JSON (via nlohmann-json, MIT licence) because:
 *   1. Human-readable — you can inspect and hand-edit scene files.
 *   2. Widely understood — no proprietary format.
 *   3. nlohmann/json is already in vcpkg.json (required by cook.exe).
 *
 * =============================================================================
 * TEACHING NOTE — What Gets Serialised?
 * =============================================================================
 * We serialise the "logical state" of each entity:
 *   • EntityID                → "id" field (opaque integer for the current run,
 *                               so we also store a stable string name)
 *   • NameComponent           → "name" + "internalID" + "title"
 *   • TransformComponent      → position, rotation, scale
 *   • HealthComponent         → hp, maxHp, mp, maxMp
 *   • StatsComponent          → strength, defence, magic, spirit, speed, luck
 *   • RenderComponent         → spriteSheet, tint, zOrder, isVisible
 *
 * We do NOT serialise:
 *   • Physics body IDs (bodyID in RigidBodyComponent) — these are assigned
 *     fresh by PhysicsWorld on each play session.
 *   • AI FSM state — transient runtime state that resets when you play.
 *   • AnimatorComponent::jointMatrices — computed output, not source state.
 *
 * This matches how Unity distinguishes serialisable "fields" from transient
 * runtime state.
 *
 * =============================================================================
 * TEACHING NOTE — Scene JSON Format (extends scene.schema.json)
 * =============================================================================
 *
 * {
 *   "$schema": "../../shared/schemas/scene.schema.json",
 *   "version":  "1.0.0",
 *   "name":     "TestScene",
 *   "entities": [
 *     {
 *       "id":   "3f2504e0-4f89-11d3-9a0c-0305e82c3301",
 *       "name": "Player",
 *       "transform":  { "x": 0.0, "y": 0.0, "z": 0.0,
 *                       "rx": 0.0, "ry": 0.0, "rz": 0.0,
 *                       "sx": 1.0, "sy": 1.0, "sz": 1.0 },
 *       "components": {
 *         "HealthComponent":  { "hp": 500, "maxHp": 500, "mp": 100, "maxMp": 100 },
 *         "StatsComponent":   { "strength": 25, "defence": 15, "magic": 30 }
 *       }
 *     }
 *   ]
 * }
 *
 * =============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0.0  (M6 — Editor Shell)
 * @date    2025
 * @see     shared/schemas/scene.schema.json
 */

#pragma once

#include <string>

// Forward-declare World to keep Jolt / ECS headers out of every translation unit
// that only needs to include this header.
// TEACHING NOTE — Forward Declarations
// Including ECS.hpp pulls in ~2000 lines of templates.  Any file that only
// calls SaveScene / LoadScene does not need those templates at compile time —
// it just needs to know that "class World" exists.  We use a forward declaration
// here and include the full header only in the .cpp implementation file.
//
// Note: World is declared at global scope in ECS.hpp (not inside a namespace).
class World;


/**
 * @class SceneSerialiser
 * @brief Static helper that converts between an ECS World and a JSON scene file.
 *
 * All methods are static — there is no instance state.
 *
 * TEACHING NOTE — Static-Only Utility Classes
 * A class with only static methods is essentially a namespace with ADL
 * (argument-dependent lookup) disabled.  It is a common C++ pattern for
 * grouping related free functions under a descriptive type name.
 * Alternative: free functions in a "scene_serialiser" namespace.
 */
class SceneSerialiser
{
public:
    // Non-constructable — static-only API.
    SceneSerialiser() = delete;

    // -------------------------------------------------------------------------
    // Save
    // -------------------------------------------------------------------------

    /**
     * @brief Serialise an ECS World to a JSON scene file.
     *
     * Iterates every live entity in @p world.  For each entity that has at
     * least a NameComponent or TransformComponent, it writes a JSON object
     * containing all serialisable components.
     *
     * @param world     The ECS World to snapshot.
     * @param filePath  Absolute path to write the .scene.json file.
     *                  Parent directories are created if missing.
     * @param sceneName Human-readable scene name written into the JSON.
     * @return true on success; false if the file could not be written.
     *
     * TEACHING NOTE — Why pass by const reference?
     * We use `const engine::World&` so the serialiser cannot accidentally
     * mutate the world while reading it.  This is the C++ "const-correctness"
     * principle: pass by const ref when you only need to read.
     */
    static bool SaveScene(const World& world,
                          const std::string& filePath,
                          const std::string& sceneName = "Scene");

    // -------------------------------------------------------------------------
    // Load
    // -------------------------------------------------------------------------

    /**
     * @brief Deserialise a JSON scene file into an ECS World.
     *
     * Parses the JSON, creates one entity per entry in "entities", and adds
     * the appropriate components.  Existing entities in @p world are NOT
     * destroyed first — call world.DestroyAllEntities() if you want a clean
     * load.
     *
     * @param world     The ECS World to populate.
     * @param filePath  Absolute path to the .scene.json file to read.
     * @return true on success; false if the file is missing or malformed.
     *
     * TEACHING NOTE — Incremental vs Replace Load
     * Loading incrementally (not clearing first) allows "additive" scenes —
     * e.g. spawning enemies defined in one scene file into a world that already
     * has a player from another file.  This is how Unreal's sublevel streaming
     * works.  For a "replace" load: clear the world first, then call LoadScene.
     */
    static bool LoadScene(World& world,
                          const std::string& filePath);

    // -------------------------------------------------------------------------
    // Entity count helper (for headless validation)
    // -------------------------------------------------------------------------

    /**
     * @brief Count the number of entity entries in a scene JSON file.
     *
     * Useful for headless CI validation without creating a full World.
     *
     * @param filePath  Absolute path to a .scene.json file.
     * @return Number of entities, or -1 on read/parse failure.
     */
    static int CountEntities(const std::string& filePath);
};
