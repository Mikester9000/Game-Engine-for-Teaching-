/**
 * @file GameData.hpp
 * @brief Header-only game content database for the educational action RPG engine.
 *
 * ============================================================================
 * TEACHING NOTE — Header-Only Libraries & The "Data Layer"
 * ============================================================================
 *
 * This file is a **header-only** data definition layer.  Every struct,
 * enum, and database lookup function lives here with no corresponding .cpp.
 *
 * WHY HEADER-ONLY?
 * ────────────────
 * In small-to-medium projects, putting all read-only game content in a single
 * header is practical:
 *   • A single `#include "GameData.hpp"` gives any system access to every
 *     item, spell, enemy, and quest without linking against a separate library.
 *   • The C++17 `inline` keyword on static member functions ensures that the
 *     One-Definition Rule (ODR) is satisfied — every translation unit that
 *     includes this header shares the same function body without duplicate-
 *     symbol linker errors.
 *   • Static local variables inside `inline` functions are thread-safe in
 *     C++11+ (guaranteed by the standard), making our lazy-init database
 *     safe even in multi-threaded scenarios.
 *
 * DESIGN PATTERN — Flyweight
 * ──────────────────────────
 * Rather than embedding full item/enemy structs in every entity, components
 * store only a lightweight uint32_t ID.  The ID is used as a key into
 * GameDatabase to retrieve the full data object *on demand*.  This is the
 * classic "Flyweight" pattern:
 *   Shared intrinsic state  → GameDatabase (read-only, common to all copies)
 *   Per-instance state      → Components on each ECS entity (health, position)
 *
 * DESIGN PATTERN — Repository / Service Locator
 * ──────────────────────────────────────────────
 * GameDatabase acts as a global read-only repository.  Callers never need to
 * instantiate it — they call static methods:
 *   const ItemData* item = GameDatabase::FindItem(23);
 * This mirrors the "Service Locator" pattern common in game engines.
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

// ---------------------------------------------------------------------------
// Includes
// ---------------------------------------------------------------------------
#include "../engine/core/Types.hpp"  // ElementType, StatusEffect, etc.

#include <string>       // std::string
#include <vector>       // std::vector — dynamic array for lists of data
#include <cstdint>      // uint32_t, int32_t


// ===========================================================================
// Section 1 — Item Enumerations
// ===========================================================================

/**
 * @enum ItemType
 * @brief Broad category that determines how an item behaves.
 *
 * TEACHING NOTE — Enumeration Design
 * ─────────────────────────────────────
 * Using an enum class (scoped enum) for categories prevents accidental
 * integer arithmetic on them:
 *   WEAPON + 1  →  compile error with enum class  (good!)
 *   WEAPON + 1  →  compiles silently with plain enum (dangerous)
 *
 * Each ItemType maps to different UI displays, inventory filters,
 * equip slots, and scripting behaviour.
 */
enum class ItemType : uint8_t {
    WEAPON,       ///< Melee or ranged weapon — goes in the weapon equip slot.
    ARMOR,        ///< Body protection — occupies head/body/legs slots.
    ACCESSORY,    ///< Rings, bracelets, amulets — up to 2 equipped at once.
    CONSUMABLE,   ///< Single-use items (potions, ethers, revival items).
    KEY_ITEM,     ///< Quest-critical items that cannot be discarded or sold.
    MAGIC_FLASK   ///< Elemancy flasks used to cast elemental magic (FF15 style).
};

/**
 * @enum WeaponType
 * @brief Sub-category for weapons, determining attack animations and range.
 *
 * TEACHING NOTE — Nested Categorisation
 * ──────────────────────────────────────
 * A weapon's ItemType is always WEAPON, but the WeaponType refines what
 * *kind* of weapon it is.  This two-level categorisation is common in RPGs:
 *   Level 1: ItemType::WEAPON  (is it a weapon at all?)
 *   Level 2: WeaponType::SWORD (what *kind* of weapon?)
 *
 * The combat system reads WeaponType to choose attack combos and hit boxes.
 */
enum class WeaponType : uint8_t {
    NONE,         ///< Non-weapon items — field set to NONE for clarity.
    SWORD,        ///< One-handed sword: fast, balanced.
    GREATSWORD,   ///< Two-handed sword: slow, devastating power.
    POLEARM,      ///< Spear/lance: long reach, good vs. aerial foes.
    SHIELD,       ///< Defensive off-hand: reduces damage when blocking.
    DAGGERS,      ///< Dual daggers: extremely fast, lower damage per hit.
    FIREARM       ///< Ranged weapon: hits all enemies in a line or area.
};


// ===========================================================================
// Section 2 — Item Data
// ===========================================================================

/**
 * @struct ItemData
 * @brief Complete definition of one item type in the game world.
 *
 * TEACHING NOTE — Data-Driven Design
 * ────────────────────────────────────
 * Rather than hard-coding item behaviour in C++ classes (e.g.
 * `class FlametongueItem : public SwordItem`), we describe items as
 * *data records*.  The game engine reads fields like `element` and `attack`
 * and applies generic logic.  Adding a new item only requires a new row in
 * the database — no new C++ code.
 *
 * This "data-driven" approach is industry standard.  Major studios store item
 * data in spreadsheets (Excel/Google Sheets) that are exported to JSON or
 * binary and loaded at runtime.  For this educational engine we inline the
 * data directly in C++ initialiser lists, which is equivalent and keeps
 * everything in one place for students.
 *
 * Lua Callbacks
 * ─────────────
 * `luaEquipCallback` stores the name of a Lua function that is called when
 * this item is equipped.  Example:
 *   "onEquipFlametongue"  →  Lua function that adds fire damage to attacks.
 * This lets designers write item effects in Lua without recompiling C++.
 * See the Scripting section of the engine documentation for details.
 */
struct ItemData {
    uint32_t    id          = 0;        ///< Unique item ID (must match array position).
    std::string name;                   ///< Display name (e.g. "Flametongue").
    std::string description;            ///< Flavour text shown in the item menu.

    ItemType    itemType    = ItemType::CONSUMABLE;  ///< Broad category.
    WeaponType  weaponType  = WeaponType::NONE;      ///< Weapon sub-type (NONE if non-weapon).

    ElementType element     = ElementType::NONE;     ///< Elemental affinity.

    // ── Stat modifiers (applied when equipped / used) ─────────────────────
    int32_t  attack    = 0;   ///< Bonus physical attack (weapons).
    int32_t  defense   = 0;   ///< Bonus physical defence (armour).
    int32_t  magic     = 0;   ///< Bonus magic power.
    int32_t  hpRestore = 0;   ///< HP restored on use (consumables).
    int32_t  mpRestore = 0;   ///< MP restored on use (consumables, ethers).

    uint32_t price     = 0;   ///< Base shop buy price in Gil.
    char     iconChar  = '?'; ///< ASCII/Unicode icon for terminal rendering.

    bool isKeyItem    = false; ///< Key items cannot be sold or discarded.
    bool isStackable  = true;  ///< True for consumables (stack up to 99 in one slot).

    /// Lua function name called when the item is equipped.
    /// Empty string = no script callback.
    std::string luaEquipCallback;
};


// ===========================================================================
// Section 3 — Spell Data
// ===========================================================================

/**
 * @struct SpellData
 * @brief Complete definition of one magic spell.
 *
 * TEACHING NOTE — Spell as Data vs Spell as Code
 * ───────────────────────────────────────────────
 * It is tempting to model each spell as a C++ class:
 *   class FireSpell : public Spell { void Cast() override { … } }
 * But this leads to hundreds of small classes and a tangled hierarchy.
 *
 * A better approach: spells are DATA, and a single `SpellSystem` reads the
 * data and executes generic behaviour:
 *   - Deal `baseDamage` multiplied by the caster's magic stat.
 *   - Apply `element` for weakness calculations.
 *   - Deduct `mpCost` from the caster's MP.
 *   - Call `luaCastCallback` for any special behaviour.
 *
 * This makes spell balancing trivial: designers change numbers in the database
 * without touching C++.
 */
struct SpellData {
    uint32_t    id          = 0;     ///< Unique spell ID.
    std::string name;                ///< Display name (e.g. "Firaga").
    std::string description;         ///< Tooltip text.

    ElementType element     = ElementType::NONE; ///< Elemental type for weakness calcs.

    int32_t  mpCost     = 0;    ///< MP consumed on cast.
    int32_t  baseDamage = 0;    ///< Base damage before caster's Magic stat scaling.
    int32_t  aoeRadius  = 0;    ///< Area-of-effect radius (0 = single target).
    float    castTime   = 1.0f; ///< Seconds of cast animation before the spell fires.

    char iconChar = '*'; ///< Icon for spell menu display.

    /// Lua function called when this spell is cast.
    /// Receives caster EntityID and target EntityID as parameters.
    std::string luaCastCallback;
};


// ===========================================================================
// Section 4 — Enemy Data
// ===========================================================================

/**
 * @struct EnemyData
 * @brief Template definition for one enemy species.
 *
 * TEACHING NOTE — Species Template vs Instance
 * ─────────────────────────────────────────────
 * EnemyData is a *template* (species definition), not a live entity.
 * When the Zone spawns a Goblin, it reads EnemyData{id=1} to know the
 * goblin's stats, then creates an ECS entity and copies those values into
 * HealthComponent, StatsComponent, etc.
 *
 * Multiple live Goblins share one EnemyData record — this is the Flyweight
 * pattern in action.  If a designer buffs the Goblin's HP from 50 to 60,
 * every future Goblin spawn benefits.
 *
 * Drop Tables
 * ───────────
 * `dropItemIDs` is a simple list of item IDs this enemy *can* drop.
 * The actual drop resolution (random selection, drop rates) is handled by
 * the CombatSystem using the enemy's `luck` stat as a multiplier.
 *
 * AI Callback
 * ───────────
 * `luaAICallback` names a Lua function invoked each AI tick.  This allows
 * complex bosses (Iron Giant, Adamantoise) to have hand-crafted behaviours
 * without hard-coding them in C++.
 */
struct EnemyData {
    uint32_t    id          = 0;      ///< Unique enemy type ID.
    std::string name;                 ///< Display name in battle UI.
    std::string description;          ///< Bestiary/lore text.

    // ── Base stats (copied into components at spawn) ──────────────────────
    int32_t  hp      = 100;  ///< Starting hit points.
    int32_t  attack  = 10;   ///< Physical attack power.
    int32_t  defense = 5;    ///< Physical defence.
    int32_t  magic   = 5;    ///< Magic attack/defence.
    int32_t  speed   = 10;   ///< Determines turn order and AI reaction time.

    uint32_t level     = 1;  ///< Recommended player level to fight this enemy.
    uint32_t xpReward  = 10; ///< Experience points granted on defeat.
    uint32_t gilReward = 5;  ///< Gil (currency) dropped on defeat.

    ElementType weakness    = ElementType::NONE; ///< Takes 150% damage from this element.
    ElementType resistance  = ElementType::NONE; ///< Takes 50% damage from this element.

    char iconChar = 'E'; ///< Glyph for the minimap and battle UI.
    bool isBoss   = false; ///< Boss enemies have special music and camera behaviour.

    /// Item IDs this enemy can drop on defeat (resolved randomly by CombatSystem).
    std::vector<uint32_t> dropItemIDs;

    /// Lua function name for custom AI logic (empty = use default FSM).
    std::string luaAICallback;
};


// ===========================================================================
// Section 5 — Quest System Types
// ===========================================================================

/**
 * @enum QuestObjectiveType
 * @brief Classification of a single quest objective's win condition.
 *
 * TEACHING NOTE — Quest Systems
 * ──────────────────────────────
 * Most RPG quest systems reduce to a small set of objective patterns.
 * Rather than hard-coding each quest in C++, we define objective *types*
 * and attach numeric parameters (targetID, requiredCount).  A generic
 * QuestSystem then updates `currentCount` when relevant events occur:
 *
 *   KILL_ENEMY:       CombatSystem fires CombatEvent; QuestSystem checks if
 *                     the dead entity's EnemyData.id matches targetID.
 *   COLLECT_ITEM:     InventorySystem fires ItemEvent; QuestSystem checks
 *                     if the item's id matches targetID.
 *   REACH_LOCATION:   MovementSystem fires WorldEvent when player enters a
 *                     zone; QuestSystem checks zone.id vs targetID.
 *   TALK_TO_NPC:      DialogueSystem fires DialogueEvent on conversation end.
 *   HUNT_TARGET:      Like KILL_ENEMY but linked to a HuntData entry.
 */
enum class QuestObjectiveType : uint8_t {
    KILL_ENEMY,      ///< Slay `requiredCount` of enemy type `targetID`.
    COLLECT_ITEM,    ///< Gather `requiredCount` of item `targetID`.
    REACH_LOCATION,  ///< Travel to zone/location `targetID`.
    TALK_TO_NPC,     ///< Speak with NPC entity whose data ID is `targetID`.
    HUNT_TARGET      ///< Complete the hunt identified by `targetID`.
};

/**
 * @struct QuestObjective
 * @brief One step of a multi-step quest.
 *
 * TEACHING NOTE — Incremental Progress
 * ──────────────────────────────────────
 * `currentCount` and `requiredCount` allow objectives to track partial
 * progress ("Kill 2 / 3 Goblins").  The QuestSystem increments
 * `currentCount` on matching events, then sets `isComplete = true` when
 * currentCount >= requiredCount.
 *
 * This avoids special-casing each quest: the same increment-and-check logic
 * handles all numeric objectives generically.
 */
struct QuestObjective {
    std::string         description;   ///< Short text shown in the quest log.
    QuestObjectiveType  type          = QuestObjectiveType::KILL_ENEMY;
    uint32_t            targetID      = 0; ///< Enemy, item, zone, or NPC ID.
    uint32_t            requiredCount = 1; ///< How many times to complete the action.
    uint32_t            currentCount  = 0; ///< Current progress counter.
    bool                isComplete    = false;
};

/**
 * @struct QuestData
 * @brief Full definition of one quest in the game.
 *
 * TEACHING NOTE — Main vs Side Quests
 * ─────────────────────────────────────
 * `isMainQuest` separates the critical story path from optional content.
 * The quest log UI uses this flag to show "Main Quest" vs "Side Quest"
 * headers.  Main quests can also trigger cutscenes and world state changes
 * that side quests cannot.
 *
 * Prerequisite Quests
 * ───────────────────
 * `prereqQuestIDs` implements quest gating: "The Final Trial" only becomes
 * available after completing "The Crystal's Call" AND "Darkness Rising".
 * The QuestSystem checks all prerequisite quest IDs are completed before
 * making a new quest available to the player.
 */
struct QuestData {
    uint32_t    id          = 0;     ///< Unique quest ID.
    std::string title;               ///< Short name (e.g. "The Road to Dawn").
    std::string description;         ///< Full quest description.

    bool isMainQuest = false;        ///< True for story-critical quests.

    std::vector<QuestObjective> objectives; ///< Ordered list of objectives.

    uint32_t xpReward  = 0;          ///< Experience reward on completion.
    uint32_t gilReward = 0;          ///< Gil reward on completion.

    std::vector<uint32_t> itemRewards;     ///< Item IDs given on completion.
    std::vector<uint32_t> prereqQuestIDs;  ///< Must be complete before this unlocks.

    /// Lua function called when the quest is completed.
    std::string luaCompleteCallback;
};


// ===========================================================================
// Section 6 — Camp Cooking (Recipe System)
// ===========================================================================

/**
 * @struct RecipeIngredient
 * @brief One ingredient required by a cooking recipe.
 *
 * TEACHING NOTE — Composition for Recipes
 * ──────────────────────────────────────────
 * A recipe is composed of multiple ingredients.  Storing them as a
 * `std::vector<RecipeIngredient>` allows variable-length ingredient lists
 * without a fixed array size.  The CampSystem iterates the list and checks
 * the player's inventory for each ingredient before allowing cooking.
 */
struct RecipeIngredient {
    uint32_t itemID   = 0; ///< ID of the required item in GameDatabase::GetItems().
    uint32_t quantity = 1; ///< How many of the item are consumed.
};

/**
 * @struct RecipeData
 * @brief A meal recipe cookable at camp that grants temporary stat bonuses.
 *
 * TEACHING NOTE — Temporary Buffs (Duration-Based Effects)
 * ──────────────────────────────────────────────────────────
 * Camp meals grant bonuses that last for `durationTurns` combat turns.
 * The CampComponent on the player entity tracks the active meal and its
 * remaining duration.  This is a simple timer-based buff system:
 *
 *   On rest: copy meal bonuses into CampComponent.
 *   On each combat turn: decrement durationTurns in CampComponent.
 *   When durationTurns == 0: remove the bonuses.
 *
 * This avoids complex "aura" systems while still teaching the concept of
 * temporary effects with explicit durations.
 *
 * Inspired by the camp cooking system in Final Fantasy XV where Ignis
 * prepares meals that provide next-day buffs.
 */
struct RecipeData {
    uint32_t    id          = 0;   ///< Unique recipe ID.
    std::string name;              ///< Dish name (e.g. "Grilled Wild Barramundi").
    std::string description;       ///< Flavour text / cooking notes.
    std::string chefName;          ///< Who cooks this dish (e.g. "Ignis").

    std::vector<RecipeIngredient> ingredients; ///< Required items (consumed on cooking).

    // ── Stat bonuses granted for durationTurns combat turns ───────────────
    int32_t hpBonus      = 0;   ///< Bonus max HP while the meal is active.
    int32_t mpBonus      = 0;   ///< Bonus max MP.
    int32_t attackBonus  = 0;   ///< Bonus physical attack.
    int32_t defenseBonus = 0;   ///< Bonus physical defence.
    int32_t magicBonus   = 0;   ///< Bonus magic power.

    int32_t durationTurns = 10; ///< Combat turns the buff lasts before expiring.

    char iconChar = 'R'; ///< Icon shown in the camp menu.
};


// ===========================================================================
// Section 7 — Shop Data
// ===========================================================================

/**
 * @struct ShopData
 * @brief The inventory and pricing policy of one vendor.
 *
 * TEACHING NOTE — Price Multipliers
 * ──────────────────────────────────
 * Many RPGs use multipliers on a base item price rather than separate
 * buy/sell prices per item.  This means:
 *   buyPrice  = item.price * buyMultiplier    (usually 1.0 — full price)
 *   sellPrice = item.price * sellMultiplier   (usually 0.5 — half price)
 *
 * Benefits:
 *   • Rebalancing prices only requires changing one base value per item.
 *   • Special shops (black market) can have buyMultiplier = 1.5 (mark-up).
 *   • Player skills can modify sellMultiplier as a reward.
 *
 * The ShopSystem reads these multipliers to compute final prices during
 * the buying/selling interaction.
 */
struct ShopData {
    uint32_t    id   = 0;   ///< Unique shop ID.
    std::string name;       ///< Vendor display name (e.g. "Hammerhead Shop").

    std::vector<uint32_t> itemIDs; ///< Item IDs available for purchase.

    float buyMultiplier  = 1.0f; ///< Multiplier on base price when player buys.
    float sellMultiplier = 0.5f; ///< Multiplier on base price when player sells.
};


// ===========================================================================
// Section 8 — Hunt System
// ===========================================================================

/**
 * @struct HuntData
 * @brief Definition of a monster hunting contract.
 *
 * TEACHING NOTE — Hunt / Bounty Systems
 * ──────────────────────────────────────
 * Many open-world RPGs (FF14, FF15, Monster Hunter) use hunt/bounty
 * systems as repeatable side content.  A hunt is a special kill quest
 * with a unique target and high rewards.
 *
 * Key design decisions here:
 *   • Hunts reference EnemyData.id via `targetEnemyID` — they use the
 *     same enemy types as normal encounters, just with special conditions.
 *   • `prereqQuestID` gates hunts behind story progress (you can't hunt
 *     Adamantoise before completing the main storyline).
 *   • `isCompleted` is a runtime flag — the database record stays
 *     immutable but the player's save data tracks completion per hunt.
 *     In a real save system this flag would live in a separate SaveData
 *     struct, not the master database.
 */
struct HuntData {
    uint32_t    id            = 0;   ///< Unique hunt ID.
    std::string title;               ///< Contract title (e.g. "The Lord of the Longwythe").
    std::string description;         ///< Hunt briefing text.

    uint32_t targetEnemyID   = 0;   ///< Which enemy type must be killed.
    uint32_t requiredKills   = 1;   ///< Number of kills needed (usually 1 for named bosses).
    uint32_t gilReward       = 0;   ///< Gil reward on contract completion.
    uint32_t xpReward        = 0;   ///< XP reward on contract completion.

    uint32_t prereqQuestID   = 0;   ///< 0 = always available; otherwise must complete that quest first.
    bool     isCompleted     = false; ///< Runtime flag set when the hunt is finished.
};


// ===========================================================================
// Section 9 — Zone Data
// ===========================================================================

/**
 * @struct ZoneData
 * @brief Configuration record for one explorable area of the world.
 *
 * TEACHING NOTE — Zone / Level Architecture
 * ──────────────────────────────────────────
 * The world is divided into named zones (similar to "areas" in FF15 or
 * "scenes" in Unity).  ZoneData is the configuration record read when
 * a zone is loaded.  The Zone class (Zone.hpp) owns the live runtime state
 * (tilemap, spawned enemies, NPCs).
 *
 * `recommendedLevel` is shown to the player as a difficulty hint.  If the
 * player enters a high-level zone early, they get a warning but are not
 * blocked — this is the "open-world" design philosophy.
 *
 * `dangerLevel` (1–5) is used by the AI system to scale patrol density and
 * by the atmosphere system to adjust ambient audio intensity.
 */
struct ZoneData {
    uint32_t    id                 = 0;  ///< Unique zone ID.
    std::string name;                    ///< Zone display name.
    std::string description;             ///< Brief description for the map screen.

    uint32_t recommendedLevel = 1;       ///< Suggested player level for this area.

    std::vector<uint32_t> enemyIDs; ///< Enemy data IDs that can spawn here.
    std::vector<uint32_t> npcIDs;   ///< NPC data IDs present in this zone.
    std::vector<uint32_t> shopIDs;  ///< Shop IDs accessible in this zone.

    bool isDungeon = false;   ///< True for indoor dungeons (no day/night cycle).

    uint32_t tileWidth  = 40; ///< Width of this zone's tilemap in tiles.
    uint32_t tileHeight = 40; ///< Height of this zone's tilemap in tiles.

    int dangerLevel = 1; ///< Danger rating 1 (safe) to 5 (extremely deadly).
};


// ===========================================================================
// Section 10 — GameDatabase
// ===========================================================================

/**
 * @class GameDatabase
 * @brief Central read-only repository for all game content.
 *
 * ============================================================================
 * TEACHING NOTE — Static Singleton vs Namespace
 * ============================================================================
 *
 * We use a class with only static methods rather than a namespace so that:
 *   1. We can forward-declare GameDatabase in headers without pulling in the
 *      full definition.
 *   2. It is visually clear that all members share the same "owner".
 *   3. Students can see how a static singleton is structured before learning
 *      about the more complex classic Singleton pattern.
 *
 * TEACHING NOTE — Lazy Initialisation (Meyers Singleton)
 * ─────────────────────────────────────────────────────────
 * Each Get*() method returns a reference to a `static local` vector.
 * In C++11+, static local variables are initialised the *first* time the
 * function is called — this is called "lazy initialisation" or the
 * "Meyers Singleton" pattern (named after Scott Meyers who popularised it).
 *
 * The key benefits:
 *   • Thread-safe: the C++11 standard guarantees that static locals are
 *     initialised exactly once, even under concurrent access.
 *   • No global constructor ordering issues: the vector is built only when
 *     first needed, never before main() starts.
 *   • Simple: no heap allocation, no manual memory management.
 *
 * TEACHING NOTE — Linear Search vs Hash Map
 * ──────────────────────────────────────────
 * FindItem(), FindSpell() etc. perform linear O(n) searches.  For a game
 * with ~100 items this is perfectly adequate — the dataset fits in L1 cache
 * and the call frequency is low (only during equip / battle start).
 *
 * For larger datasets (1000+ entries), replace with:
 *   std::unordered_map<uint32_t, const ItemData*>
 * keyed by ID, giving O(1) average-case lookup.
 *
 * ============================================================================
 */
class GameDatabase {
public:
    // =========================================================================
    // ITEMS
    // =========================================================================

    /**
     * @brief Return the complete item database.
     * @return Const reference to the static item vector (never null, never
     *         invalidated — the vector lives for the duration of the program).
     */
    static const std::vector<ItemData>& GetItems() {
        // ── TEACHING NOTE: Static local initialiser ───────────────────────
        // This lambda-initialised vector is constructed exactly once, the
        // first time GetItems() is called.  All subsequent calls return
        // the same reference.  No heap allocation or pointer gymnastics needed.
        static const std::vector<ItemData> items = BuildItems();
        return items;
    }

    /**
     * @brief Find an item by numeric ID.
     * @param id  The item ID to search for.
     * @return Pointer to the matching ItemData, or nullptr if not found.
     *
     * TEACHING NOTE — Returning Pointers vs References
     * ──────────────────────────────────────────────────
     * We return a pointer (not a reference) so callers can test for nullptr:
     *   if (auto* item = GameDatabase::FindItem(99)) { use(*item); }
     * Returning a reference would force us to throw or assert on failure,
     * which is less friendly for "optional" lookups.
     */
    static const ItemData* FindItem(uint32_t id) {
        for (const auto& item : GetItems())
            if (item.id == id) return &item;
        return nullptr;
    }

    /**
     * @brief Find an item by display name (case-sensitive).
     * @param name  The item's display name to search for.
     * @return Pointer to the matching ItemData, or nullptr.
     */
    static const ItemData* FindItemByName(const std::string& name) {
        for (const auto& item : GetItems())
            if (item.name == name) return &item;
        return nullptr;
    }

    // =========================================================================
    // SPELLS
    // =========================================================================

    /// Return the complete spell database.
    static const std::vector<SpellData>& GetSpells() {
        static const std::vector<SpellData> spells = BuildSpells();
        return spells;
    }

    /// Find a spell by ID; returns nullptr if not found.
    static const SpellData* FindSpell(uint32_t id) {
        for (const auto& sp : GetSpells())
            if (sp.id == id) return &sp;
        return nullptr;
    }

    // =========================================================================
    // ENEMIES
    // =========================================================================

    /// Return the complete enemy database.
    static const std::vector<EnemyData>& GetEnemies() {
        static const std::vector<EnemyData> enemies = BuildEnemies();
        return enemies;
    }

    /// Find an enemy by ID; returns nullptr if not found.
    static const EnemyData* FindEnemy(uint32_t id) {
        for (const auto& e : GetEnemies())
            if (e.id == id) return &e;
        return nullptr;
    }

    // =========================================================================
    // QUESTS
    // =========================================================================

    /// Return the complete quest database.
    static const std::vector<QuestData>& GetQuests() {
        static const std::vector<QuestData> quests = BuildQuests();
        return quests;
    }

    /// Find a quest by ID; returns nullptr if not found.
    static const QuestData* FindQuest(uint32_t id) {
        for (const auto& q : GetQuests())
            if (q.id == id) return &q;
        return nullptr;
    }

    // =========================================================================
    // RECIPES
    // =========================================================================

    /// Return the complete recipe database.
    static const std::vector<RecipeData>& GetRecipes() {
        static const std::vector<RecipeData> recipes = BuildRecipes();
        return recipes;
    }

    // =========================================================================
    // SHOPS
    // =========================================================================

    /// Return the complete shop database.
    static const std::vector<ShopData>& GetShops() {
        static const std::vector<ShopData> shops = BuildShops();
        return shops;
    }

    /// Find a shop by ID; returns nullptr if not found.
    static const ShopData* FindShop(uint32_t id) {
        for (const auto& s : GetShops())
            if (s.id == id) return &s;
        return nullptr;
    }

    // =========================================================================
    // HUNTS
    // =========================================================================

    /// Return the complete hunt database.
    static const std::vector<HuntData>& GetHunts() {
        static const std::vector<HuntData> hunts = BuildHunts();
        return hunts;
    }

    /// Find a hunt by ID; returns nullptr if not found.
    static const HuntData* FindHunt(uint32_t id) {
        for (const auto& h : GetHunts())
            if (h.id == id) return &h;
        return nullptr;
    }


private:
    // =========================================================================
    // Builder helpers — private static methods that construct each dataset.
    // =========================================================================
    // TEACHING NOTE — Separating Schema from Data
    // ─────────────────────────────────────────────
    // The public Get*() methods (above) define the *interface*.
    // The private Build*() methods (below) define the *data*.
    // This separation means a student reading the public API never has to
    // scroll past hundreds of data rows.  Conversely, a designer editing
    // item stats only touches the Build*() sections.
    // ─────────────────────────────────────────────────────────────────────

    // -------------------------------------------------------------------------
    // BuildItems
    // -------------------------------------------------------------------------

    /**
     * @brief Construct and return the master item list.
     *
     * TEACHING NOTE — Designated Initialisers (C++20) vs Field Order (C++17)
     * ──────────────────────────────────────────────────────────────────────────
     * In C++20 you could write:
     *   ItemData{ .id=1, .name="Broadsword", .attack=15, … }
     * In C++17 we rely on the order of fields in the struct definition,
     * so the initialiser list must match the struct layout exactly.
     *
     * For readability we construct each entry field-by-field using assignment,
     * which is unambiguous regardless of field order.
     */
    static std::vector<ItemData> BuildItems() {
        std::vector<ItemData> v;
        v.reserve(35);

        // ── WEAPONS ──────────────────────────────────────────────────────────

        // ID 1 — Broadsword
        // The most basic sword, suitable for new recruits.  Low attack but
        // easy to obtain from Hammerhead shop.
        {
            ItemData d;
            d.id = 1; d.name = "Broadsword";
            d.description = "A sturdy, double-edged sword favoured by common soldiers.";
            d.itemType = ItemType::WEAPON; d.weaponType = WeaponType::SWORD;
            d.attack = 15; d.price = 500; d.iconChar = '/';
            d.isStackable = false; d.luaEquipCallback = "onEquipBroadsword";
            v.push_back(d);
        }
        // ID 2 — Iron Sword
        {
            ItemData d;
            d.id = 2; d.name = "Iron Sword";
            d.description = "Forged from common iron.  Reliable if unspectacular.";
            d.itemType = ItemType::WEAPON; d.weaponType = WeaponType::SWORD;
            d.attack = 10; d.price = 300; d.iconChar = '/';
            d.isStackable = false;
            v.push_back(d);
        }
        // ID 3 — Flametongue
        // A fire-elemental sword.  Exploits ice-type enemy weaknesses.
        // The luaEquipCallback adds a fire damage bonus to all attacks.
        {
            ItemData d;
            d.id = 3; d.name = "Flametongue";
            d.description = "A blade that blazes with elemental fire.  Melts ice enemies.";
            d.itemType = ItemType::WEAPON; d.weaponType = WeaponType::SWORD;
            d.element = ElementType::FIRE;
            d.attack = 18; d.price = 2000; d.iconChar = '/';
            d.isStackable = false; d.luaEquipCallback = "onEquipFlametongue";
            v.push_back(d);
        }
        // ID 4 — Thunder Blade
        {
            ItemData d;
            d.id = 4; d.name = "Thunder Blade";
            d.description = "Crackling with lightning energy.  Effective against metal foes.";
            d.itemType = ItemType::WEAPON; d.weaponType = WeaponType::SWORD;
            d.element = ElementType::LIGHTNING;
            d.attack = 20; d.price = 2500; d.iconChar = '/';
            d.isStackable = false; d.luaEquipCallback = "onEquipThunderBlade";
            v.push_back(d);
        }
        // ID 5 — Ice Brand
        {
            ItemData d;
            d.id = 5; d.name = "Ice Brand";
            d.description = "A glacial blade that freezes enemies on contact.";
            d.itemType = ItemType::WEAPON; d.weaponType = WeaponType::SWORD;
            d.element = ElementType::ICE;
            d.attack = 17; d.price = 2200; d.iconChar = '/';
            d.isStackable = false; d.luaEquipCallback = "onEquipIceBrand";
            v.push_back(d);
        }
        // ID 6 — Ultima Blade
        // The strongest sword in the game.  Obtained very late via a special quest.
        {
            ItemData d;
            d.id = 6; d.name = "Ultima Blade";
            d.description = "A legendary blade of immense power.  Its edge cuts the very fabric of reality.";
            d.itemType = ItemType::WEAPON; d.weaponType = WeaponType::SWORD;
            d.attack = 50; d.magic = 10; d.price = 50000; d.iconChar = 'U';
            d.isStackable = false; d.luaEquipCallback = "onEquipUltimaBlade";
            v.push_back(d);
        }
        // ID 7 — Dragoon Lance
        {
            ItemData d;
            d.id = 7; d.name = "Dragoon Lance";
            d.description = "A polearm wielded by the legendary Dragoon knights.  Excellent reach.";
            d.itemType = ItemType::WEAPON; d.weaponType = WeaponType::POLEARM;
            d.attack = 22; d.price = 3000; d.iconChar = '|';
            d.isStackable = false; d.luaEquipCallback = "onEquipDragoonLance";
            v.push_back(d);
        }
        // ID 8 — Assassin Dagger
        {
            ItemData d;
            d.id = 8; d.name = "Assassin Dagger";
            d.description = "Twin blades of the shadow guild.  Strikes fast; poisons on critical hit.";
            d.itemType = ItemType::WEAPON; d.weaponType = WeaponType::DAGGERS;
            d.attack = 14; d.price = 1800; d.iconChar = 'i';
            d.isStackable = false; d.luaEquipCallback = "onEquipAssassinDagger";
            v.push_back(d);
        }
        // ID 9 — Mythril Sword
        {
            ItemData d;
            d.id = 9; d.name = "Mythril Sword";
            d.description = "Forged from rare mythril ore.  Lightweight yet extremely durable.";
            d.itemType = ItemType::WEAPON; d.weaponType = WeaponType::SWORD;
            d.attack = 16; d.price = 1500; d.iconChar = '/';
            d.isStackable = false;
            v.push_back(d);
        }
        // ID 10 — Engine Blade
        // The protagonist's starting weapon.  Upgradeable throughout the story.
        {
            ItemData d;
            d.id = 10; d.name = "Engine Blade";
            d.description = "The prince's ceremonial sword, a gift from the king.  Can be upgraded.";
            d.itemType = ItemType::WEAPON; d.weaponType = WeaponType::SWORD;
            d.attack = 12; d.price = 0; d.iconChar = 'E';
            d.isStackable = false; d.isKeyItem = true;
            d.luaEquipCallback = "onEquipEngineBlade";
            v.push_back(d);
        }
        // ID 11 — Dominator (Greatsword)
        {
            ItemData d;
            d.id = 11; d.name = "Dominator";
            d.description = "A colossal two-handed greatsword.  Slow, but each swing devastates.";
            d.itemType = ItemType::WEAPON; d.weaponType = WeaponType::GREATSWORD;
            d.attack = 30; d.price = 4500; d.iconChar = 'X';
            d.isStackable = false;
            v.push_back(d);
        }
        // ID 12 — Noiseblaster (Firearm)
        {
            ItemData d;
            d.id = 12; d.name = "Noiseblaster";
            d.description = "A customised firearm that fires shots in a wide cone.  Hits all enemies in range.";
            d.itemType = ItemType::WEAPON; d.weaponType = WeaponType::FIREARM;
            d.attack = 16; d.price = 3500; d.iconChar = 'f';
            d.isStackable = false; d.luaEquipCallback = "onEquipNoiseblaster";
            v.push_back(d);
        }

        // ── ARMOUR ────────────────────────────────────────────────────────────

        // ID 13 — Iron Armor
        {
            ItemData d;
            d.id = 13; d.name = "Iron Armor";
            d.description = "Heavy plate armour.  Solid protection, but slows movement slightly.";
            d.itemType = ItemType::ARMOR;
            d.defense = 15; d.price = 800; d.iconChar = 'A';
            d.isStackable = false;
            v.push_back(d);
        }
        // ID 14 — Mythril Armor
        {
            ItemData d;
            d.id = 14; d.name = "Mythril Armor";
            d.description = "Lighter than iron but far more protective.  The standard of royal guards.";
            d.itemType = ItemType::ARMOR;
            d.defense = 22; d.price = 3000; d.iconChar = 'A';
            d.isStackable = false;
            v.push_back(d);
        }
        // ID 15 — Knight's Armor
        {
            ItemData d;
            d.id = 15; d.name = "Knight's Armor";
            d.description = "Full ceremonial plate of the Crownsguard.  Impeccable defence.";
            d.itemType = ItemType::ARMOR;
            d.defense = 30; d.price = 8000; d.iconChar = 'K';
            d.isStackable = false;
            v.push_back(d);
        }
        // ID 16 — Mage Robe
        {
            ItemData d;
            d.id = 16; d.name = "Mage Robe";
            d.description = "A flowing robe woven with arcane sigils.  Boosts magic at the cost of defence.";
            d.itemType = ItemType::ARMOR;
            d.defense = 8; d.magic = 5; d.price = 2000; d.iconChar = 'M';
            d.isStackable = false;
            v.push_back(d);
        }
        // ID 17 — Royal Raiment
        {
            ItemData d;
            d.id = 17; d.name = "Royal Raiment";
            d.description = "Formal attire of the Lucian royal family.  Combines elegance with protection.";
            d.itemType = ItemType::ARMOR;
            d.defense = 20; d.magic = 5; d.price = 12000; d.iconChar = 'R';
            d.isStackable = false;
            v.push_back(d);
        }

        // ── ACCESSORIES ───────────────────────────────────────────────────────

        // ID 18 — Power Wristband
        {
            ItemData d;
            d.id = 18; d.name = "Power Wristband";
            d.description = "A heavy bracer worn by warriors to boost striking force.";
            d.itemType = ItemType::ACCESSORY;
            d.attack = 5; d.price = 1200; d.iconChar = 'o';
            d.isStackable = false;
            v.push_back(d);
        }
        // ID 19 — Magic Choker
        {
            ItemData d;
            d.id = 19; d.name = "Magic Choker";
            d.description = "A necklace imbued with raw magical energy.  Focuses arcane power.";
            d.itemType = ItemType::ACCESSORY;
            d.magic = 5; d.price = 1200; d.iconChar = 'o';
            d.isStackable = false;
            v.push_back(d);
        }
        // ID 20 — Ribbon
        // Iconic FF accessory that resists all status effects.
        {
            ItemData d;
            d.id = 20; d.name = "Ribbon";
            d.description = "A legendary hair ribbon that protects the wearer from all ailments.";
            d.itemType = ItemType::ACCESSORY;
            d.price = 10000; d.iconChar = 'r';
            d.isStackable = false; d.luaEquipCallback = "onEquipRibbon";
            v.push_back(d);
        }
        // ID 21 — Royal Sigil
        {
            ItemData d;
            d.id = 21; d.name = "Royal Sigil";
            d.description = "A signet ring bearing the Lucian royal crest.  Resonates with ancient power.";
            d.itemType = ItemType::ACCESSORY;
            d.attack = 3; d.magic = 3; d.price = 5000; d.iconChar = 'o';
            d.isStackable = false; d.luaEquipCallback = "onEquipRoyalSigil";
            v.push_back(d);
        }
        // ID 22 — Champion Belt
        {
            ItemData d;
            d.id = 22; d.name = "Champion Belt";
            d.description = "Worn by tournament champions.  Hardens the body against punishment.";
            d.itemType = ItemType::ACCESSORY;
            d.defense = 5; d.attack = 3; d.price = 4000; d.iconChar = 'o';
            d.isStackable = false;
            v.push_back(d);
        }

        // ── CONSUMABLES ───────────────────────────────────────────────────────

        // ID 23 — Potion
        // TEACHING NOTE: hpRestore is processed by the ItemSystem when the
        // item is used.  The system calls Heal(hpRestore) on the target's
        // HealthComponent.  This single generic code path handles ALL healing
        // items — no special-casing needed.
        {
            ItemData d;
            d.id = 23; d.name = "Potion";
            d.description = "A standard recovery potion.  Restores 200 HP.";
            d.itemType = ItemType::CONSUMABLE;
            d.hpRestore = 200; d.price = 100; d.iconChar = 'p';
            d.isStackable = true;
            v.push_back(d);
        }
        // ID 24 — Hi-Potion
        {
            ItemData d;
            d.id = 24; d.name = "Hi-Potion";
            d.description = "A high-grade recovery potion.  Restores 500 HP.";
            d.itemType = ItemType::CONSUMABLE;
            d.hpRestore = 500; d.price = 500; d.iconChar = 'p';
            d.isStackable = true;
            v.push_back(d);
        }
        // ID 25 — Elixir
        {
            ItemData d;
            d.id = 25; d.name = "Elixir";
            d.description = "A rare panacea.  Fully restores HP and MP of one ally.";
            d.itemType = ItemType::CONSUMABLE;
            d.hpRestore = 9999; d.mpRestore = 9999; d.price = 5000; d.iconChar = 'E';
            d.isStackable = true; d.luaEquipCallback = "onUseElixir";
            v.push_back(d);
        }
        // ID 26 — Phoenix Down
        // Revives a downed ally with 25% HP.  luaEquipCallback handles the
        // special revival logic (removing the DOWNED state).
        {
            ItemData d;
            d.id = 26; d.name = "Phoenix Down";
            d.description = "A feather from the mythical phoenix.  Revives a downed ally with some HP.";
            d.itemType = ItemType::CONSUMABLE;
            d.price = 1000; d.iconChar = 'P';
            d.isStackable = true; d.luaEquipCallback = "onUsePhoenixDown";
            v.push_back(d);
        }
        // ID 27 — Ether
        {
            ItemData d;
            d.id = 27; d.name = "Ether";
            d.description = "Restores 50 MP.";
            d.itemType = ItemType::CONSUMABLE;
            d.mpRestore = 50; d.price = 300; d.iconChar = 'e';
            d.isStackable = true;
            v.push_back(d);
        }
        // ID 28 — Hi-Ether
        {
            ItemData d;
            d.id = 28; d.name = "Hi-Ether";
            d.description = "Restores 200 MP.";
            d.itemType = ItemType::CONSUMABLE;
            d.mpRestore = 200; d.price = 1500; d.iconChar = 'e';
            d.isStackable = true;
            v.push_back(d);
        }
        // ID 29 — Antidote
        {
            ItemData d;
            d.id = 29; d.name = "Antidote";
            d.description = "Cures the Poison status effect.";
            d.itemType = ItemType::CONSUMABLE;
            d.price = 50; d.iconChar = '+';
            d.isStackable = true; d.luaEquipCallback = "onUseAntidote";
            v.push_back(d);
        }
        // ID 30 — Eye Drops
        {
            ItemData d;
            d.id = 30; d.name = "Eye Drops";
            d.description = "A soothing eyewash that cures the Blind status.";
            d.itemType = ItemType::CONSUMABLE;
            d.price = 50; d.iconChar = '+';
            d.isStackable = true; d.luaEquipCallback = "onUseEyeDrops";
            v.push_back(d);
        }
        // ID 31 — Maiden's Kiss
        {
            ItemData d;
            d.id = 31; d.name = "Maiden's Kiss";
            d.description = "A remedy from an old Lucian folktale.  Cures a variety of debilitating statuses.";
            d.itemType = ItemType::CONSUMABLE;
            d.price = 150; d.iconChar = '+';
            d.isStackable = true; d.luaEquipCallback = "onUseMaidensKiss";
            v.push_back(d);
        }
        // ID 32 — Smelling Salts
        {
            ItemData d;
            d.id = 32; d.name = "Smelling Salts";
            d.description = "Pungent salts that snap an ally out of Sleep instantly.";
            d.itemType = ItemType::CONSUMABLE;
            d.price = 100; d.iconChar = '+';
            d.isStackable = true; d.luaEquipCallback = "onUseSmellingSalts";
            v.push_back(d);
        }

        // ── MAGIC FLASKS ──────────────────────────────────────────────────────

        // ID 33 — Fire Flask
        // TEACHING NOTE: Magic flasks are crafted into spells at camp using
        // the Elemancy system.  The player collects flasks in the field and
        // combines them with secondary ingredients for more powerful spells.
        {
            ItemData d;
            d.id = 33; d.name = "Fire Flask";
            d.description = "A sealed vial of concentrated fire essence.  Used in Elemancy crafting.";
            d.itemType = ItemType::MAGIC_FLASK;
            d.element = ElementType::FIRE;
            d.price = 200; d.iconChar = 'F';
            d.isStackable = true;
            v.push_back(d);
        }
        // ID 34 — Blizzard Flask
        {
            ItemData d;
            d.id = 34; d.name = "Blizzard Flask";
            d.description = "Crystallised cold essence.  Used to craft ice-elemental spells.";
            d.itemType = ItemType::MAGIC_FLASK;
            d.element = ElementType::ICE;
            d.price = 200; d.iconChar = 'B';
            d.isStackable = true;
            v.push_back(d);
        }
        // ID 35 — Thunder Flask
        {
            ItemData d;
            d.id = 35; d.name = "Thunder Flask";
            d.description = "Captured storm energy in a reinforced vial.  Used for lightning spells.";
            d.itemType = ItemType::MAGIC_FLASK;
            d.element = ElementType::LIGHTNING;
            d.price = 200; d.iconChar = 'T';
            d.isStackable = true;
            v.push_back(d);
        }

        return v;
    }

    // -------------------------------------------------------------------------
    // BuildSpells
    // -------------------------------------------------------------------------

    /**
     * @brief Construct the master spell list (12 spells).
     *
     * TEACHING NOTE — Spell Tiers
     * ────────────────────────────
     * Classic FF magic comes in three tiers: base (Fire), -ra (Fira), -ga (Firaga).
     * Each tier roughly triples the damage and doubles the MP cost.
     * This progression gives players meaningful upgrade moments and a reason
     * to conserve MP: "Should I use Firaga now, or save MP for the boss?"
     *
     * The "castTime" field creates a risk-reward trade-off: stronger spells
     * take longer to cast, leaving the caster vulnerable.
     */
    static std::vector<SpellData> BuildSpells() {
        std::vector<SpellData> v;
        v.reserve(12);

        // Fire tier
        { SpellData s; s.id=1; s.name="Fire"; s.description="A small fireball that sears a single target.";
          s.element=ElementType::FIRE; s.mpCost=8; s.baseDamage=30; s.castTime=0.5f; s.iconChar='F';
          s.luaCastCallback="onCastFire"; v.push_back(s); }
        { SpellData s; s.id=2; s.name="Fira"; s.description="A stronger fire blast with wider range.";
          s.element=ElementType::FIRE; s.mpCost=15; s.baseDamage=60; s.aoeRadius=1; s.castTime=0.8f; s.iconChar='F';
          s.luaCastCallback="onCastFira"; v.push_back(s); }
        { SpellData s; s.id=3; s.name="Firaga"; s.description="A massive conflagration that engulfs all nearby enemies.";
          s.element=ElementType::FIRE; s.mpCost=25; s.baseDamage=100; s.aoeRadius=3; s.castTime=1.2f; s.iconChar='F';
          s.luaCastCallback="onCastFiraga"; v.push_back(s); }

        // Blizzard (ice) tier
        { SpellData s; s.id=4; s.name="Blizzard"; s.description="A shard of ice that pierces a single foe.";
          s.element=ElementType::ICE; s.mpCost=8; s.baseDamage=28; s.castTime=0.5f; s.iconChar='B';
          s.luaCastCallback="onCastBlizzard"; v.push_back(s); }
        { SpellData s; s.id=5; s.name="Blizzara"; s.description="A barrage of ice shards that can freeze targets.";
          s.element=ElementType::ICE; s.mpCost=15; s.baseDamage=55; s.aoeRadius=1; s.castTime=0.8f; s.iconChar='B';
          s.luaCastCallback="onCastBlizzara"; v.push_back(s); }
        { SpellData s; s.id=6; s.name="Blizzaga"; s.description="A blizzard that encases multiple enemies in ice.";
          s.element=ElementType::ICE; s.mpCost=25; s.baseDamage=95; s.aoeRadius=3; s.castTime=1.2f; s.iconChar='B';
          s.luaCastCallback="onCastBlizzaga"; v.push_back(s); }

        // Thunder (lightning) tier
        { SpellData s; s.id=7; s.name="Thunder"; s.description="A bolt of lightning strikes a single target.";
          s.element=ElementType::LIGHTNING; s.mpCost=8; s.baseDamage=32; s.castTime=0.4f; s.iconChar='T';
          s.luaCastCallback="onCastThunder"; v.push_back(s); }
        { SpellData s; s.id=8; s.name="Thundara"; s.description="Chain lightning that jumps between foes.";
          s.element=ElementType::LIGHTNING; s.mpCost=15; s.baseDamage=62; s.aoeRadius=2; s.castTime=0.7f; s.iconChar='T';
          s.luaCastCallback="onCastThundara"; v.push_back(s); }
        { SpellData s; s.id=9; s.name="Thundaga"; s.description="A massive thunderstorm raining lightning on all enemies.";
          s.element=ElementType::LIGHTNING; s.mpCost=25; s.baseDamage=105; s.aoeRadius=4; s.castTime=1.1f; s.iconChar='T';
          s.luaCastCallback="onCastThundaga"; v.push_back(s); }

        // Special spells
        // ID 10 — Drain: Absorbs HP from the target, restoring the caster's HP.
        // TEACHING NOTE: The luaCastCallback handles the "heal caster by damage dealt"
        // logic since it doesn't fit the generic "deal damage" pattern.
        { SpellData s; s.id=10; s.name="Drain"; s.description="Draws the target's life force into the caster.  Damage dealt heals the caster.";
          s.element=ElementType::NONE; s.mpCost=12; s.baseDamage=40; s.castTime=1.0f; s.iconChar='D';
          s.luaCastCallback="onCastDrain"; v.push_back(s); }

        // ID 11 — Warp Strike: Teleport to a distant enemy and deal bonus damage.
        // The teleport effect is scripted in Lua to avoid coupling the spell to
        // the movement system in C++.
        { SpellData s; s.id=11; s.name="Warp Strike"; s.description="Teleport instantly to an enemy and deliver a devastating blow.";
          s.element=ElementType::NONE; s.mpCost=10; s.baseDamage=50; s.castTime=0.0f; s.iconChar='W';
          s.luaCastCallback="onCastWarpStrike"; v.push_back(s); }

        // ID 12 — Death: Instant kill (30% chance).  Expensive MP cost.
        { SpellData s; s.id=12; s.name="Death"; s.description="A dark spell with a chance to instantly fell any non-boss enemy.";
          s.element=ElementType::NONE; s.mpCost=30; s.baseDamage=0; s.castTime=1.5f; s.iconChar='X';
          s.luaCastCallback="onCastDeath"; v.push_back(s); }

        return v;
    }

    // -------------------------------------------------------------------------
    // BuildEnemies
    // -------------------------------------------------------------------------

    /**
     * @brief Construct the master enemy database (20 entries).
     *
     * TEACHING NOTE — Balancing Enemy Stats
     * ───────────────────────────────────────
     * Enemy stats follow rough guidelines for an action RPG:
     *   HP      ≈ level * 30 (normal) to level * 250 (boss)
     *   Attack  ≈ level * 2.5
     *   Defence ≈ level * 1.5
     *   XP      ≈ level * 15 (normal) to level * 100 (boss)
     *   Gil     ≈ level * 8  (normal) to level * 500 (boss)
     *
     * These are starting points — playtesters adjust them for game feel.
     * Keeping the formulas visible in comments helps future maintainers
     * understand *why* Behemoth has 1500 HP and not 500.
     */
    static std::vector<EnemyData> BuildEnemies() {
        std::vector<EnemyData> v;
        v.reserve(20);

        // ID 1 — Goblin (Level 1 tutorial enemy)
        // TEACHING NOTE: Tutorial enemies should die in 2–3 hits so players
        // learn attack controls quickly without frustration.
        { EnemyData e; e.id=1; e.name="Goblin"; e.description="A scrappy green creature that lurks near roads.";
          e.hp=50; e.attack=8; e.defense=3; e.magic=2; e.speed=8;
          e.level=1; e.xpReward=20; e.gilReward=10;
          e.weakness=ElementType::FIRE; e.iconChar='g';
          e.dropItemIDs={23,29}; v.push_back(e); }

        // ID 2 — Sabretusk (Level 3 wolf)
        { EnemyData e; e.id=2; e.name="Sabretusk"; e.description="A large wolf with sabre-like fangs.  Travels in packs.";
          e.hp=80; e.attack=14; e.defense=6; e.magic=2; e.speed=12;
          e.level=3; e.xpReward=35; e.gilReward=15; e.iconChar='w';
          e.dropItemIDs={23,24}; v.push_back(e); }

        // ID 3 — Garula (Level 5 beast, hunt target)
        { EnemyData e; e.id=3; e.name="Garula"; e.description="A massive boar-like beast with rhinoceros-thick hide.";
          e.hp=150; e.attack=20; e.defense=10; e.magic=3; e.speed=8;
          e.level=5; e.xpReward=60; e.gilReward=30; e.iconChar='G';
          e.dropItemIDs={23,24,13}; v.push_back(e); }

        // ID 4 — Imperial Soldier (Level 5 humanoid)
        { EnemyData e; e.id=4; e.name="Imperial Soldier"; e.description="A foot soldier of the Niflheim Empire.  Disciplined and dangerous.";
          e.hp=120; e.attack=18; e.defense=12; e.magic=5; e.speed=10;
          e.level=5; e.xpReward=50; e.gilReward=40; e.iconChar='I';
          e.dropItemIDs={23,24,27}; v.push_back(e); }

        // ID 5 — Magitek Armor (Level 8 machine)
        // TEACHING NOTE: Machines often resist physical attacks but are weak
        // to lightning (overloading circuits) — a good example of element design.
        { EnemyData e; e.id=5; e.name="Magitek Armor"; e.description="An imperial war machine bristling with magitek weaponry.";
          e.hp=200; e.attack=25; e.defense=20; e.magic=10; e.speed=6;
          e.level=8; e.xpReward=80; e.gilReward=60;
          e.weakness=ElementType::LIGHTNING; e.iconChar='M';
          e.dropItemIDs={24,27,35}; v.push_back(e); }

        // ID 6 — Naga (Level 10, ice weak)
        { EnemyData e; e.id=6; e.name="Naga"; e.description="A serpentine daemon with water-based magic attacks.";
          e.hp=180; e.attack=22; e.defense=8; e.magic=18; e.speed=14;
          e.level=10; e.xpReward=90; e.gilReward=50;
          e.weakness=ElementType::ICE; e.iconChar='N';
          e.dropItemIDs={24,27,34}; v.push_back(e); }

        // ID 7 — Bandersnatch (Level 12 creature)
        { EnemyData e; e.id=7; e.name="Bandersnatch"; e.description="A fearsome nocturnal predator with powerful rending claws.";
          e.hp=250; e.attack=30; e.defense=15; e.magic=5; e.speed=16;
          e.level=12; e.xpReward=110; e.gilReward=70; e.iconChar='B';
          e.dropItemIDs={24,29,30}; v.push_back(e); }

        // ID 8 — MindFlayer (Level 15 mage enemy)
        { EnemyData e; e.id=8; e.name="MindFlayer"; e.description="A psionic daemon that attacks with mind-bending magic.  Can inflict Stop.";
          e.hp=200; e.attack=18; e.defense=12; e.magic=30; e.speed=12;
          e.level=15; e.xpReward=130; e.gilReward=80; e.iconChar='m';
          e.dropItemIDs={24,27,28,31}; e.luaAICallback="aiMindFlayer"; v.push_back(e); }

        // ID 9 — Iron Giant (Level 20 boss)
        // TEACHING NOTE: Boss enemies have dramatically higher HP and isBoss=true.
        // The isBoss flag causes: special battle music, boss HP bar, cinematic
        // intro animation, and no mid-battle escape for the player.
        { EnemyData e; e.id=9; e.name="Iron Giant"; e.description="A colossal iron golem animated by ancient magitek sorcery.";
          e.hp=5000; e.attack=50; e.defense=40; e.magic=20; e.speed=8;
          e.level=20; e.xpReward=500; e.gilReward=500;
          e.weakness=ElementType::LIGHTNING; e.isBoss=true; e.iconChar='J';
          e.dropItemIDs={24,25,15,22}; e.luaAICallback="aiBossIronGiant"; v.push_back(e); }

        // ID 10 — Adamantoise (Level 50 superboss)
        // TEACHING NOTE: The Adamantoise is a post-game challenge requiring
        // max-level characters and optimal equipment.  Its extreme stats teach
        // players about endgame content design and reward exploration.
        { EnemyData e; e.id=10; e.name="Adamantoise"; e.description="A mountain-sized tortoise daemon.  One of the oldest creatures in Eos.";
          e.hp=20000; e.attack=80; e.defense=80; e.magic=30; e.speed=3;
          e.level=50; e.xpReward=5000; e.gilReward=10000;
          e.isBoss=true; e.iconChar='T';
          e.dropItemIDs={25,26,6,20}; e.luaAICallback="aiBossAdamantoise"; v.push_back(e); }

        // ID 11 — Marlboro (Level 25 status-inflicting plant)
        { EnemyData e; e.id=11; e.name="Marlboro"; e.description="A grotesque plant daemon whose breath inflicts every known status ailment.";
          e.hp=800; e.attack=35; e.defense=20; e.magic=25; e.speed=10;
          e.level=25; e.xpReward=200; e.gilReward=150;
          e.weakness=ElementType::FIRE; e.iconChar='L';
          e.dropItemIDs={29,30,31,32,25}; e.luaAICallback="aiMarlboro"; v.push_back(e); }

        // ID 12 — Behemoth (Level 30)
        { EnemyData e; e.id=12; e.name="Behemoth"; e.description="A massive horned daemon of terrifying power.  Its charge can kill in one blow.";
          e.hp=1500; e.attack=55; e.defense=35; e.magic=15; e.speed=12;
          e.level=30; e.xpReward=350; e.gilReward=300; e.iconChar='B';
          e.dropItemIDs={24,25,15,22}; v.push_back(e); }

        // ID 13 — Thunderoc (Level 14 bird, ice weak)
        { EnemyData e; e.id=13; e.name="Thunderoc"; e.description="A massive bird daemon that attacks with electrical charges.";
          e.hp=220; e.attack=28; e.defense=14; e.magic=8; e.speed=18;
          e.level=14; e.xpReward=120; e.gilReward=90;
          e.weakness=ElementType::ICE; e.iconChar='t';
          e.dropItemIDs={23,24,33}; v.push_back(e); }

        // ID 14 — Coeurlregina (Level 35 boss)
        { EnemyData e; e.id=14; e.name="Coeurlregina"; e.description="The queen of coeurl daemons.  Her petrifying beam turns victims to stone.";
          e.hp=3000; e.attack=65; e.defense=40; e.magic=35; e.speed=14;
          e.level=35; e.xpReward=800; e.gilReward=600;
          e.isBoss=true; e.iconChar='Q';
          e.dropItemIDs={25,20,17}; e.luaAICallback="aiBossCoeurlregina"; v.push_back(e); }

        // ID 15 — RedGiant (Level 40 boss)
        { EnemyData e; e.id=15; e.name="Red Giant"; e.description="A towering daemon draped in crimson armour.  Immune to magic.";
          e.hp=4000; e.attack=70; e.defense=50; e.magic=20; e.speed=6;
          e.level=40; e.xpReward=1000; e.gilReward=800;
          e.isBoss=true; e.iconChar='R';
          e.dropItemIDs={25,26,6,15}; e.luaAICallback="aiBossRedGiant"; v.push_back(e); }

        // ID 16 — Foras (Level 8 demon)
        { EnemyData e; e.id=16; e.name="Foras"; e.description="A minor demon summoned by Niflheim sorcerers to patrol ruins.";
          e.hp=160; e.attack=22; e.defense=10; e.magic=20; e.speed=12;
          e.level=8; e.xpReward=75; e.gilReward=55; e.iconChar='d';
          e.dropItemIDs={24,27,31}; v.push_back(e); }

        // ID 17 — Coeurl (Level 22 panther)
        { EnemyData e; e.id=17; e.name="Coeurl"; e.description="A sleek panther daemon with electrical whiskers.  Lightning-fast in close combat.";
          e.hp=450; e.attack=40; e.defense=25; e.magic=22; e.speed=18;
          e.level=22; e.xpReward=180; e.gilReward=130;
          e.weakness=ElementType::ICE; e.iconChar='c';
          e.dropItemIDs={24,32,34}; v.push_back(e); }

        // ID 18 — Manticore (Level 18)
        { EnemyData e; e.id=18; e.name="Manticore"; e.description="A chimeric daemon with a lion's body and a scorpion tail that inflicts poison.";
          e.hp=350; e.attack=35; e.defense=20; e.magic=10; e.speed=14;
          e.level=18; e.xpReward=150; e.gilReward=100; e.iconChar='C';
          e.dropItemIDs={24,29,32}; v.push_back(e); }

        // ID 19 — Jormungand (Level 45 dragon boss)
        { EnemyData e; e.id=19; e.name="Jormungand"; e.description="A world-serpent daemon of catastrophic power.  Its venom can slay gods.";
          e.hp=6000; e.attack=75; e.defense=55; e.magic=30; e.speed=10;
          e.level=45; e.xpReward=2000; e.gilReward=2000;
          e.weakness=ElementType::ICE; e.isBoss=true; e.iconChar='S';
          e.dropItemIDs={25,26,6,20}; e.luaAICallback="aiBossJormungand"; v.push_back(e); }

        // ID 20 — Seadevil (Level 28 aquatic, hunt target)
        { EnemyData e; e.id=20; e.name="Seadevil"; e.description="A bioluminescent deep-sea daemon that ambushes swimmers from below.";
          e.hp=700; e.attack=42; e.defense=28; e.magic=32; e.speed=14;
          e.level=28; e.xpReward=280; e.gilReward=200;
          e.weakness=ElementType::LIGHTNING; e.iconChar='S';
          e.dropItemIDs={24,25,28}; v.push_back(e); }

        return v;
    }

    // -------------------------------------------------------------------------
    // BuildQuests
    // -------------------------------------------------------------------------

    /**
     * @brief Construct the master quest list (10 quests).
     *
     * TEACHING NOTE — Quest Narrative Structure
     * ───────────────────────────────────────────
     * The quests follow a classic three-act structure:
     *   Act 1 (Q1–Q3):  Introduction; player learns basic mechanics.
     *   Act 2 (Q4–Q8):  Escalation; harder challenges, story reveals.
     *   Act 3 (Q9–Q10): Climax; final boss, ultimate reward.
     *
     * Prerequisites (prereqQuestIDs) enforce narrative order without hard-
     * coding quest logic in C++.  The QuestSystem simply checks whether all
     * prerequisite quests are complete before unlocking the next step.
     */
    static std::vector<QuestData> BuildQuests() {
        std::vector<QuestData> v;
        v.reserve(10);

        // Q1 — The Road to Dawn (main quest, tutorial kill objective)
        {
            QuestData q; q.id=1; q.title="The Road to Dawn";
            q.description="Prove your worth by clearing the goblins threatening the road to Hammerhead.";
            q.isMainQuest=true;
            QuestObjective obj;
            obj.description="Kill 3 Goblins"; obj.type=QuestObjectiveType::KILL_ENEMY;
            obj.targetID=1; obj.requiredCount=3;
            q.objectives.push_back(obj);
            q.xpReward=100; q.gilReward=500;
            q.luaCompleteCallback="onCompleteRoadToDawn"; v.push_back(q);
        }

        // Q2 — Stolen Goods (side quest, collect key item)
        {
            QuestData q; q.id=2; q.title="Stolen Goods";
            q.description="A merchant's Phoenix Downs were stolen by bandits.  Recover them.";
            q.isMainQuest=false;
            QuestObjective obj;
            obj.description="Collect a Phoenix Down"; obj.type=QuestObjectiveType::COLLECT_ITEM;
            obj.targetID=26; obj.requiredCount=1;
            q.objectives.push_back(obj);
            q.xpReward=80; q.gilReward=300; v.push_back(q);
        }

        // Q3 — Missing Person (side quest, reach location)
        {
            QuestData q; q.id=3; q.title="Missing Person";
            q.description="A traveller went missing near the ruins.  Search the area.";
            q.isMainQuest=false;
            QuestObjective obj;
            obj.description="Reach the ruins"; obj.type=QuestObjectiveType::REACH_LOCATION;
            obj.targetID=2; obj.requiredCount=1;
            q.objectives.push_back(obj);
            q.xpReward=60; q.gilReward=200; v.push_back(q);
        }

        // Q4 — The Hunter's Test (side, kill sabretusk)
        {
            QuestData q; q.id=4; q.title="The Hunter's Test";
            q.description="The Hunters' Guild requires proof of combat ability: defeat a Sabretusk.";
            q.isMainQuest=false;
            QuestObjective obj;
            obj.description="Defeat the Sabretusk"; obj.type=QuestObjectiveType::KILL_ENEMY;
            obj.targetID=2; obj.requiredCount=1;
            q.objectives.push_back(obj);
            q.xpReward=90; q.gilReward=400; v.push_back(q);
        }

        // Q5 — Royal Delicacies (side, collect ingredients)
        {
            QuestData q; q.id=5; q.title="Royal Delicacies";
            q.description="Ignis needs Potions as a substitute ingredient to test a new royal recipe.";
            q.isMainQuest=false;
            QuestObjective obj;
            obj.description="Collect 3 Potions"; obj.type=QuestObjectiveType::COLLECT_ITEM;
            obj.targetID=23; obj.requiredCount=3;
            q.objectives.push_back(obj);
            q.xpReward=70; q.gilReward=300;
            q.itemRewards={33,34,35}; v.push_back(q);
        }

        // Q6 — Imperial Threat (main, kill imperial soldier)
        {
            QuestData q; q.id=6; q.title="Imperial Threat";
            q.description="Imperial forces are advancing on Hammerhead.  Drive them back.";
            q.isMainQuest=true;
            QuestObjective obj;
            obj.description="Defeat the Imperial Soldier"; obj.type=QuestObjectiveType::KILL_ENEMY;
            obj.targetID=4; obj.requiredCount=1;
            q.objectives.push_back(obj);
            q.xpReward=150; q.gilReward=800;
            q.prereqQuestIDs={1}; // Must complete The Road to Dawn first
            q.luaCompleteCallback="onCompleteImperialThreat"; v.push_back(q);
        }

        // Q7 — The Crystal's Call (main, reach location)
        {
            QuestData q; q.id=7; q.title="The Crystal's Call";
            q.description="A vision draws you toward the ancient crystal shrine at the heart of Lucis.";
            q.isMainQuest=true;
            QuestObjective obj;
            obj.description="Reach the Crystal Shrine"; obj.type=QuestObjectiveType::REACH_LOCATION;
            obj.targetID=3; obj.requiredCount=1;
            q.objectives.push_back(obj);
            q.xpReward=200; q.gilReward=1000;
            q.prereqQuestIDs={6};
            q.luaCompleteCallback="onCompleteCrystalsCall"; v.push_back(q);
        }

        // Q8 — Darkness Rising (main, boss fight)
        {
            QuestData q; q.id=8; q.title="Darkness Rising";
            q.description="An Iron Giant guardian blocks the path forward.  Defeat it to proceed.";
            q.isMainQuest=true;
            QuestObjective obj;
            obj.description="Defeat the Iron Giant"; obj.type=QuestObjectiveType::KILL_ENEMY;
            obj.targetID=9; obj.requiredCount=1;
            q.objectives.push_back(obj);
            q.xpReward=500; q.gilReward=2000;
            q.prereqQuestIDs={6};
            q.luaCompleteCallback="onCompleteDarknessRising"; v.push_back(q);
        }

        // Q9 — Hunter Rank Up (side, collect items)
        {
            QuestData q; q.id=9; q.title="Hunter Rank Up";
            q.description="The guild requires proof of your gathering skills.  Collect Hi-Potions.";
            q.isMainQuest=false;
            QuestObjective obj;
            obj.description="Collect 3 Hi-Potions"; obj.type=QuestObjectiveType::COLLECT_ITEM;
            obj.targetID=24; obj.requiredCount=3;
            q.objectives.push_back(obj);
            q.xpReward=120; q.gilReward=600;
            q.itemRewards={22}; v.push_back(q);
        }

        // Q10 — The Final Trial (main, requires Q7 AND Q8)
        // TEACHING NOTE: This quest demonstrates prerequisite gating.
        // It only becomes available once both "The Crystal's Call" and
        // "Darkness Rising" are complete — a narrative and mechanical gate
        // that forces the player to experience both story branches before
        // the climax.
        {
            QuestData q; q.id=10; q.title="The Final Trial";
            q.description="Armed with the power of the crystal and the courage forged in battle, face the world-serpent.";
            q.isMainQuest=true;
            QuestObjective obj;
            obj.description="Defeat Jormungand"; obj.type=QuestObjectiveType::KILL_ENEMY;
            obj.targetID=19; obj.requiredCount=1;
            q.objectives.push_back(obj);
            q.xpReward=1000; q.gilReward=5000;
            q.itemRewards={6,20}; // Ultima Blade + Ribbon as ultimate rewards
            q.prereqQuestIDs={7, 8}; // Both story branches required
            q.luaCompleteCallback="onCompleteFinalTrial"; v.push_back(q);
        }

        return v;
    }

    // -------------------------------------------------------------------------
    // BuildRecipes
    // -------------------------------------------------------------------------

    /**
     * @brief Construct the camp cooking recipe list (8 recipes).
     *
     * TEACHING NOTE — Camp Meal Design Philosophy (inspired by FF15)
     * ───────────────────────────────────────────────────────────────
     * Each recipe has a distinct buff theme:
     *   • Survival Fried Rice:    small all-rounder (good for early game)
     *   • Grilled Barramundi:     HP/defence focus (tank role)
     *   • Hunter's Stew:         balanced ATK+DEF (frontliner)
     *   • Fried Clam Fettuccine: magic focus (mage build)
     *   • Mother and Child Bowl: party HP + MP (healer-friendly)
     *   • Sautéed Mushrooms:     magic focus, cheap ingredients
     *   • Smoked Behemoth:       attack specialist (late-game power)
     *   • Crownsguard Chowder:   ultimate defence (endgame)
     *
     * Ingredient items use IDs that correspond to existing item database
     * entries.  The camp UI checks the player's inventory for each ingredient.
     */
    static std::vector<RecipeData> BuildRecipes() {
        std::vector<RecipeData> v;
        v.reserve(8);

        // ID 1 — Survival Fried Rice
        { RecipeData r; r.id=1; r.name="Survival Fried Rice"; r.chefName="Ignis";
          r.description="A simple but nourishing meal.  Perfect when supplies are low.";
          r.ingredients={{23,1},{27,1}}; // 1 Potion + 1 Ether (as metaphor for ingredients)
          r.hpBonus=100; r.mpBonus=10; r.attackBonus=2;
          r.durationTurns=8; r.iconChar='R'; v.push_back(r); }

        // ID 2 — Grilled Wild Barramundi
        { RecipeData r; r.id=2; r.name="Grilled Wild Barramundi"; r.chefName="Ignis";
          r.description="Freshly grilled barramundi fillet.  Bolsters endurance.";
          r.ingredients={{23,1},{29,1}};
          r.hpBonus=200; r.defenseBonus=3;
          r.durationTurns=10; r.iconChar='F'; v.push_back(r); }

        // ID 3 — Hunter's Stew
        { RecipeData r; r.id=3; r.name="Hunter's Stew"; r.chefName="Ignis";
          r.description="A hearty slow-cooked stew favoured by Hunters on long patrols.";
          r.ingredients={{24,1},{23,1}};
          r.hpBonus=150; r.attackBonus=3; r.defenseBonus=2;
          r.durationTurns=12; r.iconChar='S'; v.push_back(r); }

        // ID 4 — Fried Clam Fettuccine
        { RecipeData r; r.id=4; r.name="Fried Clam Fettuccine"; r.chefName="Ignis";
          r.description="Linguine with clams in a garlic butter sauce.  Sharpens magical acuity.";
          r.ingredients={{27,1},{28,1}};
          r.hpBonus=250; r.magicBonus=5;
          r.durationTurns=10; r.iconChar='F'; v.push_back(r); }

        // ID 5 — Mother and Child Rice Bowl
        { RecipeData r; r.id=5; r.name="Mother and Child Rice Bowl"; r.chefName="Ignis";
          r.description="A classic comfort meal.  Substantially restores HP and MP pools.";
          r.ingredients={{23,2},{27,1}};
          r.hpBonus=300; r.mpBonus=50;
          r.durationTurns=10; r.iconChar='B'; v.push_back(r); }

        // ID 6 — Sautéed Wild Mushrooms
        { RecipeData r; r.id=6; r.name="Sauteed Wild Mushrooms"; r.chefName="Ignis";
          r.description="Foraged mushrooms sautéed in olive oil.  Amplifies magical power significantly.";
          r.ingredients={{29,1},{27,2}};
          r.mpBonus=100; r.magicBonus=5;
          r.durationTurns=12; r.iconChar='M'; v.push_back(r); }

        // ID 7 — Smoked Behemoth
        { RecipeData r; r.id=7; r.name="Smoked Behemoth"; r.chefName="Ignis";
          r.description="A prized cut of behemoth slow-smoked over hardwood.  Dramatically boosts strength.";
          r.ingredients={{24,2},{25,1}};
          r.hpBonus=500; r.attackBonus=10;
          r.durationTurns=15; r.iconChar='B'; v.push_back(r); }

        // ID 8 — Crownsguard Clam Chowder
        { RecipeData r; r.id=8; r.name="Crownsguard Clam Chowder"; r.chefName="Ignis";
          r.description="Traditional Lucian navy recipe.  Said to make soldiers invincible.";
          r.ingredients={{24,2},{28,1},{25,1}};
          r.hpBonus=400; r.defenseBonus=8; r.magicBonus=3;
          r.durationTurns=20; r.iconChar='C'; v.push_back(r); }

        return v;
    }

    // -------------------------------------------------------------------------
    // BuildShops
    // -------------------------------------------------------------------------

    /**
     * @brief Construct the shop database (4 shops).
     *
     * TEACHING NOTE — Shop Progression
     * ──────────────────────────────────
     * Shops are gated by location and story progress:
     *   Hammerhead:   starter area; basic weapons, common consumables.
     *   Lestallum:    mid-game city; wider selection, elemental gear.
     *   Meldacio:     mid-late game; advanced weapons, accessories.
     *   Hunter's HQ:  reward shop; rare equipment bought with gil/tokens.
     *
     * `buyMultiplier=1.0` means items sell at their listed price.
     * `sellMultiplier=0.5` means players recover half the price — standard RPG.
     * A high-tier shop might offer `sellMultiplier=0.6` as a small reward.
     */
    static std::vector<ShopData> BuildShops() {
        std::vector<ShopData> v;
        v.reserve(4);

        { ShopData s; s.id=1; s.name="Hammerhead Shop";
          s.itemIDs={1,2,13,23,24,27,29,30};
          s.buyMultiplier=1.0f; s.sellMultiplier=0.5f; v.push_back(s); }

        { ShopData s; s.id=2; s.name="Lestallum Market";
          s.itemIDs={3,4,5,9,14,15,16,23,24,25,27,28,29,30,31,33,34,35};
          s.buyMultiplier=1.0f; s.sellMultiplier=0.5f; v.push_back(s); }

        { ShopData s; s.id=3; s.name="Meldacio Shop";
          s.itemIDs={6,7,9,11,15,16,18,19,24,25,27,28,32};
          s.buyMultiplier=1.0f; s.sellMultiplier=0.55f; v.push_back(s); }

        { ShopData s; s.id=4; s.name="Hunter's HQ";
          s.itemIDs={8,10,11,12,17,20,21,22,25,26,28};
          s.buyMultiplier=1.1f; s.sellMultiplier=0.6f; v.push_back(s); }

        return v;
    }

    // -------------------------------------------------------------------------
    // BuildHunts
    // -------------------------------------------------------------------------

    /**
     * @brief Construct the hunt/bounty list (5 hunts).
     *
     * TEACHING NOTE — Hunt Design
     * ────────────────────────────
     * Hunts are optional contracts posted at the Hunter's HQ.  They are
     * designed around the idea that players voluntarily seek out powerful
     * enemies for great rewards, rather than stumbling into them.
     *
     * The `prereqQuestID` field gates high-level hunts behind story progress.
     * For example, the Iron Giant hunt requires completing "The Road to Dawn"
     * to ensure the player has at least basic equipment.
     */
    static std::vector<HuntData> BuildHunts() {
        std::vector<HuntData> v;
        v.reserve(5);

        // Hunt 1 — Garula hunt (accessible from the start)
        { HuntData h; h.id=1; h.title="The Lord of the Longwythe";
          h.description="A rampaging Garula has been terrorising travellers on the Longwythe road.  Put it down.";
          h.targetEnemyID=3; h.requiredKills=1; h.gilReward=3000; h.xpReward=500;
          h.prereqQuestID=0; v.push_back(h); }

        // Hunt 2 — Iron Giant (requires completing Q1)
        { HuntData h; h.id=2; h.title="False King";
          h.description="Reports of an Iron Giant stalking the ruins of an abandoned fortress.  Slay it.";
          h.targetEnemyID=9; h.requiredKills=1; h.gilReward=8000; h.xpReward=2000;
          h.prereqQuestID=1; v.push_back(h); }

        // Hunt 3 — Behemoth (requires Q4)
        { HuntData h; h.id=3; h.title="A Behemoth Undertaking";
          h.description="A Behemoth has claimed the old mining road.  The Hunters' Guild is offering a large bounty.";
          h.targetEnemyID=12; h.requiredKills=1; h.gilReward=6000; h.xpReward=1500;
          h.prereqQuestID=4; v.push_back(h); }

        // Hunt 4 — Marlboro (requires Q6)
        { HuntData h; h.id=4; h.title="On the Verge of Madness";
          h.description="A Marlboro has been inflicting mass status ailments near Cauthess.  Experts only.";
          h.targetEnemyID=11; h.requiredKills=1; h.gilReward=5000; h.xpReward=1200;
          h.prereqQuestID=6; v.push_back(h); }

        // Hunt 5 — Seadevil (requires Q7)
        { HuntData h; h.id=5; h.title="Messenger from the Deep";
          h.description="A Seadevil is capsizing boats near the Malmalam shore.  Bring a lightning weapon.";
          h.targetEnemyID=20; h.requiredKills=1; h.gilReward=4000; h.xpReward=1000;
          h.prereqQuestID=7; v.push_back(h); }

        return v;
    }

}; // end class GameDatabase
