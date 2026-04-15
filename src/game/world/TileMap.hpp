/**
 * @file TileMap.hpp
 * @brief Tile-based world map with FOV, dungeon generation, and serialisation.
 *
 * ============================================================================
 * TEACHING NOTE — Tile Maps in 2D Games
 * ============================================================================
 *
 * A tile map divides the world into a regular grid of equal-sized cells
 * (tiles).  This is the foundation of countless games from the NES era
 * (Zelda, Final Fantasy 1–6) through modern roguelikes (Caves of Qud,
 * Dwarf Fortress).
 *
 * WHY TILE MAPS?
 * ──────────────
 * 1. Memory efficiency:  A 40×40 map is just 1600 Tile structs instead of
 *    1600 heap-allocated objects.  A contiguous flat array is cache-friendly.
 * 2. Grid math:  Pathfinding (A*), FOV, and dungeon generation are much
 *    simpler on a grid than on arbitrary meshes.
 * 3. Indexed access:  Any tile can be reached in O(1) by its (x, y)
 *    coordinate:  `tile = m_tiles[y * width + x]`.
 * 4. Serialisation:  A tile map serialises trivially — write each tile's
 *    type as a character, one row per line.
 *
 * COORDINATE SYSTEM
 * ─────────────────
 * This engine uses the convention:
 *   (0, 0)  = top-left tile
 *   +X      = rightward (East)
 *   +Y      = downward  (South)
 *
 * This matches screen-space coordinates and makes row-major array indexing
 * ( index = y * width + x ) natural.
 *
 * ============================================================================
 * TEACHING NOTE — Field of View (FOV)
 * ============================================================================
 *
 * In dungeon games the player can only see tiles within a certain radius
 * that are not blocked by walls.  This is called the "Field of View" (FOV)
 * or "Line of Sight" (LOS) computation.
 *
 * NAIVE APPROACH — Raycasting
 * ────────────────────────────
 * Cast a ray from the player to every tile in the radius.  If the ray
 * doesn't hit a wall, the tile is visible.  Simple, but:
 *   • O(n²) rays, each of O(radius) steps → O(n² * radius) total.
 *   • Artifacts: some tiles incorrectly hidden, some incorrectly shown,
 *     depending on ray angle quantisation.
 *
 * BETTER APPROACH — Recursive Shadowcasting (Björn Bergström, 2001)
 * ──────────────────────────────────────────────────────────────────
 * Shadowcasting solves FOV by tracking "shadows" cast by opaque tiles.
 * Key ideas:
 *   1. Divide the visible area into 8 octants (45° wedges).
 *   2. For each octant, scan rows from the origin outward.
 *   3. Track the start and end "slope" of the currently lit region.
 *   4. When an opaque (wall) tile is encountered, it casts a shadow —
 *      everything behind it at steeper slopes is dark.
 *   5. Recursively cast light for the lit portion to the left of the wall.
 *
 * Complexity: O(r²) — proportional to the visible area.
 * Quality:    Symmetric, no artifacts, handles thick walls correctly.
 *
 * Reference:  http://www.roguebasin.com/index.php/FOV_using_recursive_shadowcasting
 *
 * ============================================================================
 * TEACHING NOTE — Procedural Dungeon Generation
 * ============================================================================
 *
 * Procedural generation creates levels algorithmically, providing infinite
 * variety without manual authoring.  Our algorithm:
 *
 * ROOM-CORRIDOR ALGORITHM
 * ───────────────────────
 * 1. Fill the entire map with WALL tiles.
 * 2. Attempt to place N randomly-sized rectangular rooms:
 *    a. Pick a random position and size.
 *    b. Check it doesn't overlap existing rooms (plus a 1-tile padding).
 *    c. If valid, carve out FLOOR tiles in the room.
 * 3. For each new room after the first, connect it to the previous room
 *    with an L-shaped corridor:
 *    a. Randomly decide: horizontal-then-vertical, or vertical-then-horizontal.
 *    b. Carve FLOOR tiles along both legs of the L.
 * 4. Place STAIRS_UP in the first room, STAIRS_DOWN in the last.
 *
 * This produces dungeon layouts where all rooms are reachable from any other
 * room — a key requirement for playability.
 *
 * Variations to explore:
 *   • BSP (Binary Space Partitioning) for more evenly distributed rooms.
 *   • Cellular automata for organic cave systems.
 *   • Wave Function Collapse for pattern-coherent levels.
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
// Standard library includes
// ---------------------------------------------------------------------------
#include <vector>       // std::vector — dynamic array for tile storage
#include <string>       // std::string — for serialisation
#include <cstdint>      // uint32_t, int32_t — fixed-width integers
#include <utility>      // std::pair — used for colour pairs

// ---------------------------------------------------------------------------
// Engine includes
// ---------------------------------------------------------------------------
#include "../../engine/core/Types.hpp"    // Color, TileCoord, Vec3, etc.
#include "../../engine/core/Logger.hpp"   // LOG_DEBUG, LOG_WARN, etc.


// ===========================================================================
// Section 1 — TileType Enumeration
// ===========================================================================

/**
 * @enum TileType
 * @brief Classification of what a tile fundamentally represents.
 *
 * TEACHING NOTE — Enum-Driven Tile Behaviour
 * ───────────────────────────────────────────
 * Rather than storing separate boolean fields for every possible tile
 * property (isWalkable, isTransparent, isInteractable, …), we use a single
 * TileType enum.  The engine derives all properties from this one value:
 *
 *   IsPassable()  →  switch(type): WALL=false, WATER=false, else=true
 *   IsOpaque()    →  switch(type): WALL=true, else=false
 *   GetChar()     →  switch(type): FLOOR='.', WALL='#', …
 *
 * This keeps the Tile struct small and makes adding new tile types easy:
 * add an enum value and update the switch statements — no structural changes.
 *
 * TEACHING NOTE — Enum Value Ranges
 * ───────────────────────────────────
 * Keeping enum values compact (no gaps) lets us use them as array indices,
 * e.g. for a lookup table of colours or characters.  Always start at 0.
 */
enum class TileType : uint8_t {
    FLOOR       = 0,  ///< Walkable interior floor.
    WALL,             ///< Solid, impassable wall.
    WATER,            ///< Liquid surface — impassable without boat/swim.
    GRASS,            ///< Outdoor grass — passable, rustles when walked.
    SAND,             ///< Desert / beach sand — passable, slows movement.
    FOREST,           ///< Dense trees — passable but blocks distant FOV.
    MOUNTAIN,         ///< Rocky high ground — impassable.
    ROAD,             ///< Paved road — passable, grants movement speed bonus.
    DOOR,             ///< Doorway — passable, opens/closes.
    STAIRS_UP,        ///< Staircase leading to a higher floor / map.
    STAIRS_DOWN,      ///< Staircase leading to a lower floor / map.
    CHEST,            ///< Treasure chest — interactable, passable after opening.
    SAVE_POINT,       ///< Crystal save point — heals party and saves the game.
    SHOP_TILE,        ///< Shop entrance marker — triggers shop UI.
    INN_TILE,         ///< Inn tile — triggers rest/save UI (cheaper than camp).
    BRIDGE            ///< Bridge over water — passable, like ROAD but over water.
};


// ===========================================================================
// Section 2 — Free Functions for Tile Rendering
// ===========================================================================

/**
 * @brief Return the ASCII/Unicode glyph used to render this tile type.
 *
 * TEACHING NOTE — Terminal Rendering (Roguelike Style)
 * ──────────────────────────────────────────────────────
 * Even modern games sometimes use ASCII art for their maps during
 * development ("ASCII roguelike" style).  Single characters serve as
 * clear, compact tile representations that work in any terminal.
 * This function is the bridge between the TileType enum and the character
 * that the renderer draws on screen.
 *
 * In a sprite-based renderer you would replace this with a function that
 * returns a texture atlas UV rectangle.
 *
 * @param type  The TileType to look up.
 * @return      The ASCII character used to represent this tile.
 */
inline char GetTileChar(TileType type) {
    switch (type) {
        case TileType::FLOOR:       return '.';
        case TileType::WALL:        return '#';
        case TileType::WATER:       return '~';
        case TileType::GRASS:       return ',';
        case TileType::SAND:        return ':';
        case TileType::FOREST:      return 'T';
        case TileType::MOUNTAIN:    return '^';
        case TileType::ROAD:        return '=';
        case TileType::DOOR:        return '+';
        case TileType::STAIRS_UP:   return '<';
        case TileType::STAIRS_DOWN: return '>';
        case TileType::CHEST:       return 'C';
        case TileType::SAVE_POINT:  return 'S';
        case TileType::SHOP_TILE:   return '$';
        case TileType::INN_TILE:    return 'I';
        case TileType::BRIDGE:      return '=';
        default:                    return '?';
    }
}

/**
 * @brief Return the foreground/background colour pair for a tile type.
 *
 * @return A std::pair where `.first` is the foreground colour (glyph) and
 *         `.second` is the background colour (cell fill).
 *
 * TEACHING NOTE — Colour Pairs in Terminal Rendering
 * ────────────────────────────────────────────────────
 * Text-mode UIs (ncurses, ANSI terminals) display each cell with an
 * independent foreground (text) and background colour.  A "colour pair"
 * encodes both at once.  The renderer uses `first` to colour the glyph
 * returned by GetTileChar(), and `second` to fill the cell background.
 *
 * TEACHING NOTE — Visual Design for Readability
 * ───────────────────────────────────────────────
 * Colour choices should convey meaning at a glance:
 *   • WALL = grey on dark grey  (opaque, solid)
 *   • WATER = white on blue     (bright waves on water)
 *   • GRASS = green on dark green
 *   • CHEST = yellow on black   (treasure! attention-grabbing)
 *   • SAVE_POINT = cyan on black (calm, restorative)
 *
 * This uses the Color struct from Types.hpp.
 */
inline std::pair<Color, Color> GetTileColorPair(TileType type) {
    // TEACHING NOTE: Using structured-binding-friendly pairs (C++17).
    //   auto [fg, bg] = GetTileColorPair(tile.type);
    switch (type) {
        case TileType::FLOOR:
            return { Color(180, 180, 180, 255), Color(40,  40,  40,  255) };
        case TileType::WALL:
            return { Color(120, 120, 120, 255), Color(60,  60,  60,  255) };
        case TileType::WATER:
            return { Color(200, 230, 255, 255), Color(0,   80,  160, 255) };
        case TileType::GRASS:
            return { Color(80,  180, 60,  255), Color(20,  60,  20,  255) };
        case TileType::SAND:
            return { Color(220, 200, 130, 255), Color(180, 150, 80,  255) };
        case TileType::FOREST:
            return { Color(30,  120, 30,  255), Color(10,  40,  10,  255) };
        case TileType::MOUNTAIN:
            return { Color(160, 140, 120, 255), Color(80,  70,  60,  255) };
        case TileType::ROAD:
            return { Color(180, 160, 120, 255), Color(120, 100, 70,  255) };
        case TileType::DOOR:
            return { Color(200, 140, 60,  255), Color(80,  50,  20,  255) };
        case TileType::STAIRS_UP:
            return { Color(255, 255, 180, 255), Color(60,  60,  100, 255) };
        case TileType::STAIRS_DOWN:
            return { Color(180, 180, 255, 255), Color(40,  40,  80,  255) };
        case TileType::CHEST:
            return { Color(255, 220, 0,   255), Color(80,  60,  0,   255) };
        case TileType::SAVE_POINT:
            return { Color(0,   255, 220, 255), Color(0,   60,  80,  255) };
        case TileType::SHOP_TILE:
            return { Color(255, 180, 0,   255), Color(80,  50,  0,   255) };
        case TileType::INN_TILE:
            return { Color(180, 100, 220, 255), Color(50,  20,  70,  255) };
        case TileType::BRIDGE:
            return { Color(160, 120, 80,  255), Color(0,   60,  120, 255) };
        default:
            return { Color::White(), Color::Black() };
    }
}

/**
 * @brief Determine whether a TileType should block movement by default.
 *
 * TEACHING NOTE — Separating Queries from Data
 * ──────────────────────────────────────────────
 * We store this logic as a free function rather than in the Tile struct
 * so that it is the single authoritative definition of "what blocks movement."
 * This avoids the tile passability flag getting out of sync with the type
 * (e.g. someone adds a new type but forgets to update IsPassable).
 *
 * The Tile struct still caches the `isPassable` boolean for fast lookup
 * during FOV and pathfinding, but it is *initialised* using this function
 * inside SetTile().
 *
 * @param type  TileType to check.
 * @return true if an entity can walk on this tile by default.
 */
inline bool TileTypeIsPassable(TileType type) {
    switch (type) {
        case TileType::WALL:     return false;
        case TileType::WATER:    return false; // requires boat/swim ability
        case TileType::MOUNTAIN: return false; // requires climbing ability
        default:                 return true;
    }
}

/**
 * @brief Determine whether a TileType blocks line-of-sight (is opaque).
 *
 * TEACHING NOTE — Passability vs Opacity
 * ────────────────────────────────────────
 * Not all blocking tiles are opaque and vice versa:
 *   WATER:    impassable (blocks movement) but NOT opaque (you can see across it)
 *   FOREST:   passable (you can walk in) AND partially opaque (reduces FOV)
 *   WALL:     impassable AND opaque
 *
 * For simplicity we treat WALL as the only fully opaque tile.  A more
 * sophisticated FOV could treat FOREST as partially transparent (reduces
 * view radius by 2, say) — this is left as an exercise for the student.
 *
 * @param type  TileType to check.
 * @return true if this tile blocks line-of-sight.
 */
inline bool TileTypeIsOpaque(TileType type) {
    return (type == TileType::WALL);
}


// ===========================================================================
// Section 3 — Tile Struct
// ===========================================================================

/**
 * @struct Tile
 * @brief Represents one cell of the tile map.
 *
 * TEACHING NOTE — Small Structs and Cache Efficiency
 * ─────────────────────────────────────────────────────
 * Each Tile is kept as small as possible.  A 40×40 map has 1600 tiles;
 * a 200×200 map has 40,000.  Padding matters:
 *
 *   Current layout (without padding):
 *     TileType    (uint8_t) = 1 byte
 *     bool × 3             = 3 bytes
 *     uint32_t             = 4 bytes
 *   Total:                   8 bytes → 2 cache lines for 16 tiles (64B each)
 *
 * The 40,000-tile map fits in 320 KB — well within typical L2 cache (256–1024 KB).
 * A naive pointer-per-tile design would scatter tiles across the heap, causing
 * cache misses on every access.
 *
 * TEACHING NOTE — Explored vs Visible
 * ─────────────────────────────────────
 * Two booleans track different states:
 *   isVisible  = currently in the player's FOV (bright rendering).
 *   isExplored = was visible at some point in the past (dim/grey rendering).
 *
 * This is the classic "fog of war" system:
 *   isVisible  && isExplored  → draw tile at full brightness
 *   !isVisible && isExplored  → draw tile dim/grey (remembered but unseen)
 *   !isVisible && !isExplored → draw as black (completely unknown)
 */
struct Tile {
    TileType type       = TileType::WALL; ///< What kind of tile this is.
    bool     isPassable = false;          ///< Can entities walk here? (cached).
    bool     isVisible  = false;          ///< Currently in the FOV.
    bool     isExplored = false;          ///< Has ever been seen by the player.

    /**
     * @brief EntityID of an entity occupying this tile (0 = empty).
     *
     * TEACHING NOTE — Tiles and Entities
     * ────────────────────────────────────
     * Storing an EntityID here lets us do O(1) "is this tile occupied?"
     * checks during pathfinding, collision, and AI.  The alternative —
     * scanning all entities every time — is O(n).
     *
     * Kept as uint32_t instead of EntityID to avoid including ECS.hpp
     * in this header (which would create a circular dependency).  The
     * sentinel value 0 means "no entity" (entity IDs start from 0 in
     * our ECS but the tile starts unoccupied, not entity-0).
     * In practice, entity IDs are only stored here for non-player-
     * controlled entities (chests, doors, NPCs).
     */
    uint32_t entityID = 0; ///< 0 = empty; otherwise an ECS EntityID.
};


// ===========================================================================
// Section 4 — TileMap Class
// ===========================================================================

/**
 * @class TileMap
 * @brief A 2D grid of Tile objects with FOV, dungeon generation, and I/O.
 *
 * TEACHING NOTE — Class Invariants
 * ─────────────────────────────────
 * A class invariant is a condition that is always true for a valid object.
 * TileMap's invariant:
 *   m_tiles.size() == m_width * m_height
 *
 * Every method either:
 *   (a) preserves this invariant, or
 *   (b) deliberately re-establishes it (e.g. Resize()).
 *
 * The constructor establishes the invariant.  Methods like SetTile() and
 * GetTile() assert it via bounds checking.
 *
 * TEACHING NOTE — RAII (Resource Acquisition Is Initialisation)
 * ──────────────────────────────────────────────────────────────
 * `std::vector` manages the tile array's lifetime automatically (RAII).
 * When TileMap is destroyed, the vector's destructor frees the memory.
 * No `new` / `delete` needed — and no possibility of leaks.
 */
class TileMap {
public:
    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /**
     * @brief Construct a TileMap of the given dimensions.
     *
     * All tiles are initialised as WALL (impassable, invisible, unexplored).
     * Use GenerateEmptyRoom() or GenerateDungeon() to populate the map.
     *
     * @param width   Number of tile columns.
     * @param height  Number of tile rows.
     *
     * TEACHING NOTE — Default Arguments
     * ────────────────────────────────────
     * Providing default arguments (=0) lets callers write:
     *   TileMap map;           // 0×0, resize later
     *   TileMap map(40, 40);   // 40×40 starting map
     * Without needing two separate constructor overloads.
     */
    explicit TileMap(uint32_t width = 0, uint32_t height = 0);

    /// Default destructor — std::vector handles cleanup automatically.
    ~TileMap() = default;

    // Moveable but not copyable (maps can be large; force explicit copy).
    TileMap(TileMap&&) noexcept            = default;
    TileMap& operator=(TileMap&&) noexcept = default;
    TileMap(const TileMap&)                = delete;
    TileMap& operator=(const TileMap&)     = delete;


    // =========================================================================
    // Tile Access (Core API)
    // =========================================================================

    /**
     * @brief Return a mutable reference to the tile at (x, y).
     *
     * @param x  Column index (0 = leftmost).
     * @param y  Row index    (0 = topmost).
     * @return   Reference to the tile — modifying it modifies the map.
     *
     * @pre  InBounds(x, y) must be true.
     *
     * TEACHING NOTE — Bounds Checking
     * ────────────────────────────────
     * Accessing `m_tiles[y * m_width + x]` with invalid coordinates is
     * undefined behaviour (UB) in C++ — it could read garbage memory or
     * crash.  We assert bounds in debug builds.
     *
     * In release builds, asserts are typically disabled for performance.
     * Consider using a safe version that returns a "null tile" sentinel
     * for release builds if the map is player-modified at runtime.
     */
    Tile& GetTile(int x, int y);

    /// Const overload — returns a read-only reference.
    const Tile& GetTile(int x, int y) const;

    /**
     * @brief Set the type of the tile at (x, y) and update isPassable.
     *
     * @param x     Column index.
     * @param y     Row index.
     * @param type  New tile type.
     *
     * TEACHING NOTE — Derived State (isPassable)
     * ────────────────────────────────────────────
     * `isPassable` is *derived* from `type`, but we cache it in the tile
     * to avoid calling TileTypeIsPassable() on every pathfinding step.
     * SetTile() re-derives and caches it whenever the type changes.
     * This is the "dirty flag" + "cached computed value" pattern.
     */
    void SetTile(int x, int y, TileType type);

    /**
     * @brief Check whether the tile at (x, y) is walkable.
     *
     * @return false if out of bounds (treat the edge as wall) or if the
     *         tile is impassable.
     */
    bool IsPassable(int x, int y) const;

    /**
     * @brief Check whether the tile at (x, y) is currently visible.
     * @return false if out of bounds or not visible.
     */
    bool IsVisible(int x, int y) const;

    /**
     * @brief Check whether (x, y) is within the map bounds.
     * @return true if 0 <= x < m_width && 0 <= y < m_height.
     */
    bool InBounds(int x, int y) const;


    // =========================================================================
    // Field of View
    // =========================================================================

    /**
     * @brief Recompute which tiles are visible from (originX, originY).
     *
     * Uses the recursive shadowcasting algorithm.  After this call:
     *   - All previously visible tiles have isVisible = false.
     *   - All tiles reachable from the origin within `radius` and not
     *     blocked by opaque tiles have isVisible = true.
     *   - Any newly visible tile also has isExplored = true permanently.
     *
     * @param originX  X-coordinate of the viewer (usually the player).
     * @param originY  Y-coordinate of the viewer.
     * @param radius   Maximum FOV distance in tiles.
     *
     * TEACHING NOTE — The isExplored Flag
     * ─────────────────────────────────────
     * `isExplored` is never reset to false during normal play — it tracks
     * the player's total exploration history.  On a new dungeon load it
     * starts false everywhere.  This implements the classic "fog of war":
     *   Dark (unexplored) → Dim (explored but not currently visible) → Bright (visible)
     */
    void ComputeFOV(int originX, int originY, int radius);

    /**
     * @brief Set isVisible = false on every tile.
     *
     * Called at the start of ComputeFOV() and when transitioning zones.
     * Does NOT reset isExplored (exploration history is permanent).
     */
    void ResetVisibility();


    // =========================================================================
    // Serialisation
    // =========================================================================

    /**
     * @brief Serialise the map to a compact text string.
     *
     * Format:
     * @code
     *   W H\n
     *   <W chars>\n          (row 0 — top row)
     *   <W chars>\n          (row 1)
     *   …
     *   <W chars>\n          (row H-1 — bottom row)
     * @endcode
     *
     * Each character is the tile's glyph as returned by GetTileChar().
     * The `isVisible` and `isExplored` fields are NOT serialised — they
     * are runtime state recomputed on each session load.
     *
     * TEACHING NOTE — Serialisation Formats
     * ───────────────────────────────────────
     * We use human-readable text so students can inspect and hand-edit maps
     * in any text editor.  For production games you would use:
     *   • Binary format (more compact, faster read, not human-readable).
     *   • JSON/XML (human-readable, self-describing, but verbose).
     *   • A custom binary format with versioning headers.
     *
     * The tile type <→ character mapping is defined in GetTileChar() /
     * CharToTileType(), making the encoding easy to look up.
     *
     * @return A std::string containing the entire serialised map.
     */
    std::string Serialize() const;

    /**
     * @brief Deserialise a previously serialised map string.
     *
     * Replaces the current map with the data from `data`.
     * On parse error, the map may be left in a partially loaded state;
     * a LOG_WARN is emitted.
     *
     * @param data  String produced by Serialize().
     */
    void Deserialize(const std::string& data);


    // =========================================================================
    // Map Generation
    // =========================================================================

    /**
     * @brief Fill the map with a single empty room bounded by walls.
     *
     * Fills all interior tiles with FLOOR and border tiles with WALL.
     * Useful for simple test maps and overworld zones.
     *
     * @param w  Room interior width  (map will be w × h including borders).
     * @param h  Room interior height.
     *
     * TEACHING NOTE — Simple Room Generation
     * ────────────────────────────────────────
     * Even a "trivial" empty room has a useful border-wall loop pattern:
     *   for x in [0, w): SetTile(x, 0, WALL);    top row
     *   for x in [0, w): SetTile(x, h-1, WALL);  bottom row
     *   for y in [0, h): SetTile(0, y, WALL);     left column
     *   for y in [0, h): SetTile(w-1, y, WALL);   right column
     * This teaches: nested loops, boundary conditions, and the distinction
     * between "whole map" and "border only" iteration.
     */
    void GenerateEmptyRoom(uint32_t w, uint32_t h);

    /**
     * @brief Generate a random dungeon using the Room-Corridor algorithm.
     *
     * Starts with a WALL-filled map and carves FLOOR rooms and corridors.
     * Places STAIRS_UP in the first room and STAIRS_DOWN in the last room.
     *
     * TEACHING NOTE — Procedural Generation & Randomness
     * ─────────────────────────────────────────────────────
     * We use `std::mt19937` (Mersenne Twister), a high-quality pseudo-random
     * number generator seeded from `std::random_device`.
     *
     *   std::mt19937 rng(std::random_device{}());
     *
     * WHY NOT `std::rand`?
     *   • std::rand produces poor-quality sequences (short period, biased).
     *   • std::mt19937 has a period of 2^19937 − 1 — effectively infinite.
     *   • Seeding with std::random_device provides OS-level entropy.
     *
     * SEEDED vs UNSEEDED:
     *   • A fixed seed (e.g. `rng(42)`) produces the same dungeon every run.
     *     Useful for testing and "seed-sharing" (players sharing dungeon codes).
     *   • A random seed (std::random_device) produces unique dungeons.
     *   • Good engines expose a "seed" parameter so both modes are possible.
     */
    void GenerateDungeon();

    /**
     * @brief Generate a dungeon with a specific random seed (reproducible).
     *
     * @param seed  Seed value for the PRNG.  Same seed → same dungeon.
     *
     * TEACHING NOTE — Reproducible Generation
     * ─────────────────────────────────────────
     * Reproducible generation is crucial for:
     *   • Speedrunning (players can practice a specific layout).
     *   • Multiplayer consistency (all clients use the same seed).
     *   • Bug reproduction ("the crash always happens on seed 12345").
     *   • Game design iteration (inspect the same map after changing gen params).
     */
    void GenerateDungeon(uint32_t seed);


    // =========================================================================
    // Accessors
    // =========================================================================

    /// Width of the map in tiles.
    uint32_t GetWidth()  const { return m_width;  }

    /// Height of the map in tiles.
    uint32_t GetHeight() const { return m_height; }

    /// Total number of tiles (width × height).
    uint32_t GetTileCount() const {
        return m_width * m_height;
    }

    /// Direct read-only access to the flat tile array (for batch processing).
    const std::vector<Tile>& GetTiles() const { return m_tiles; }


private:
    // =========================================================================
    // Private Helpers
    // =========================================================================

    /**
     * @brief Convert 2D tile coordinates to the flat array index.
     *
     * TEACHING NOTE — Row-Major vs Column-Major
     * ──────────────────────────────────────────
     * We store tiles in *row-major* order: row 0 first, then row 1, etc.
     * index = y * width + x
     *
     * Column-major would be: index = x * height + y
     *
     * Row-major is conventional for 2D grids in C++ and matches the way
     * humans read (left-to-right, top-to-bottom).  It is also optimal
     * for iterating over a full row in sequence (sequential memory access).
     *
     * @param x  Column index.
     * @param y  Row index.
     * @return   Flat array index.
     */
    size_t Index(int x, int y) const {
        return static_cast<size_t>(y) * m_width + static_cast<size_t>(x);
    }

    /**
     * @brief Core recursive shadowcasting function for one octant.
     *
     * Called by ComputeFOV() for each of 8 octants.
     *
     * @param cx, cy          Origin of the viewer in tile coordinates.
     * @param radius          Maximum visibility radius.
     * @param row             Current scan row (distance from origin). Starts at 1.
     * @param startSlope      Top slope of the currently visible wedge.
     * @param endSlope        Bottom slope of the currently visible wedge.
     * @param xx, xy, yx, yy  Octant transformation matrix coefficients.
     *
     * TEACHING NOTE — Octant Transforms
     * ────────────────────────────────────
     * The 8 octants of the circle are symmetric.  Instead of writing 8
     * separate functions, we use a 2×2 transformation matrix {xx,xy,yx,yy}
     * that remaps "octant-local" coordinates to world coordinates.
     *
     * For octant 0 (east-southeast): xx=1, xy=0, yx=0, yy=1
     *   world_dx = dx*xx + dy*xy = dx*1 + dy*0 = dx
     *   world_dy = dx*yx + dy*yy = dx*0 + dy*1 = dy
     *
     * For octant 4 (west-northwest): xx=-1, xy=0, yx=0, yy=-1
     *   world_dx = -dx
     *   world_dy = -dy
     *
     * The 8 sets of {xx,xy,yx,yy} are hard-coded in a static table inside
     * ComputeFOV().  Each call to ShadowCastOctant() uses one row.
     */
    void ShadowCastOctant(int cx, int cy, int radius,
                          int row,
                          float startSlope, float endSlope,
                          int xx, int xy, int yx, int yy);

    /**
     * @brief Internal dungeon generation helper (takes a pre-seeded PRNG).
     * @param rng  Reference to a seeded Mersenne Twister PRNG.
     */
    void GenerateDungeonImpl(uint32_t seed);


    // =========================================================================
    // Member Variables
    // =========================================================================

    uint32_t         m_width  = 0; ///< Number of tile columns.
    uint32_t         m_height = 0; ///< Number of tile rows.
    std::vector<Tile> m_tiles;     ///< Flat tile array, row-major order.

    /// A dummy "out-of-bounds" tile returned for invalid coordinates in release.
    /// Always a WALL so out-of-bounds accesses behave safely.
    mutable Tile m_outOfBoundsTile{ TileType::WALL, false, false, false, 0 };
};
