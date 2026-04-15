/**
 * @file TileMap.cpp
 * @brief Implementation of TileMap: constructor, FOV (shadowcasting), dungeon generator,
 *        serialisation, and helper utilities.
 *
 * ============================================================================
 * TEACHING NOTE — Separating Interface from Implementation
 * ============================================================================
 *
 * TileMap.hpp declares WHAT the TileMap class can do (its public interface).
 * TileMap.cpp defines HOW it does it (the implementation).
 *
 * This separation benefits:
 *   1. COMPILE TIMES: Files that include TileMap.hpp do not need to recompile
 *      when only the .cpp changes (as long as the .hpp interface is unchanged).
 *   2. ENCAPSULATION: Callers cannot see private helpers like ShadowCastOctant
 *      or GenerateDungeonImpl.
 *   3. READABILITY: Users read the clean .hpp; implementors dig into the .cpp.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 */

// ---------------------------------------------------------------------------
// Own header first (catches include-order bugs)
// ---------------------------------------------------------------------------
#include "TileMap.hpp"

// ---------------------------------------------------------------------------
// Standard library
// ---------------------------------------------------------------------------
#include <cassert>    // assert() — precondition checks in debug builds
#include <sstream>    // std::istringstream / std::ostringstream — text I/O
#include <random>     // std::mt19937, std::random_device, std::uniform_int_distribution
#include <algorithm>  // std::min, std::max
#include <cmath>      // std::sqrt, std::abs


// ===========================================================================
// Construction
// ===========================================================================

/**
 * @brief Construct a TileMap of the given dimensions.
 *
 * TEACHING NOTE — Vector Initialisation
 * ──────────────────────────────────────
 * `std::vector<Tile>(w * h, Tile{})` allocates a contiguous block of
 * (w * h) default-constructed Tile objects in one allocation.
 * This is more efficient than push_back()-ing each tile individually:
 *   • Only one heap allocation.
 *   • The compiler can vectorise the zero-initialisation.
 *   • The resulting memory is contiguous — cache-friendly.
 */
TileMap::TileMap(uint32_t width, uint32_t height)
    : m_width(width), m_height(height)
{
    if (width > 0 && height > 0) {
        // Default tile: WALL, impassable, invisible, unexplored.
        m_tiles.assign(static_cast<size_t>(width) * height, Tile{});
    }
    LOG_DEBUG("TileMap created: " << width << "x" << height
              << " (" << width * height << " tiles)");
}


// ===========================================================================
// Tile Access
// ===========================================================================

/**
 * @brief Return a mutable reference to the tile at (x, y).
 *
 * TEACHING NOTE — Assertions in Debug vs Release
 * ─────────────────────────────────────────────────
 * `assert(condition)` expands to a runtime check in debug builds and
 * compiles to NOTHING in release builds (when NDEBUG is defined).
 * This means:
 *   • In debug: invalid coordinates trigger a clear, informative crash.
 *   • In release: no overhead from bounds checking (trust callers).
 *
 * If you need bounds checking in release too (e.g. for modding support),
 * use a non-assert check that returns m_outOfBoundsTile.
 */
Tile& TileMap::GetTile(int x, int y) {
    if (!InBounds(x, y)) {
        LOG_WARN("TileMap::GetTile(" << x << ", " << y << ") out of bounds ["
                 << m_width << "x" << m_height << "] — returning dummy tile");
        m_outOfBoundsTile = Tile{};  // Reset the dummy tile
        return m_outOfBoundsTile;
    }
    return m_tiles[Index(x, y)];
}

const Tile& TileMap::GetTile(int x, int y) const {
    if (!InBounds(x, y)) {
        return m_outOfBoundsTile;
    }
    return m_tiles[Index(x, y)];
}

/**
 * @brief Set the tile type and update derived fields.
 *
 * TEACHING NOTE — Single Point of Update
 * ────────────────────────────────────────
 * Always set tile types through SetTile(), never by writing to tile.type
 * directly.  This ensures isPassable stays consistent with the type.
 *
 * If callers wrote:
 *   GetTile(x,y).type = TileType::FLOOR;  // BAD: isPassable not updated!
 * then pathfinding would still treat the tile as impassable.
 *
 * SetTile() is the "single point of update" that keeps everything in sync.
 */
void TileMap::SetTile(int x, int y, TileType type) {
    if (!InBounds(x, y)) {
        LOG_WARN("TileMap::SetTile(" << x << ", " << y << ") out of bounds");
        return;
    }
    Tile& t      = m_tiles[Index(x, y)];
    t.type       = type;
    t.isPassable = TileTypeIsPassable(type); // Keep derived field in sync
}

bool TileMap::IsPassable(int x, int y) const {
    if (!InBounds(x, y)) return false; // Treat map edges as walls
    return m_tiles[Index(x, y)].isPassable;
}

bool TileMap::IsVisible(int x, int y) const {
    if (!InBounds(x, y)) return false;
    return m_tiles[Index(x, y)].isVisible;
}

bool TileMap::InBounds(int x, int y) const {
    return x >= 0 && y >= 0
        && static_cast<uint32_t>(x) < m_width
        && static_cast<uint32_t>(y) < m_height;
}


// ===========================================================================
// Field of View — Recursive Shadowcasting
// ===========================================================================

/**
 * @brief Reset the isVisible flag on every tile to false.
 *
 * TEACHING NOTE — Batch Reset
 * ────────────────────────────
 * Rather than iterating and setting each flag individually (which is fine),
 * we show a range-based for loop over the vector.  The compiler will
 * auto-vectorise this tight loop (set bool to false repeatedly in a
 * contiguous array) — no manual SIMD needed for educational code.
 */
void TileMap::ResetVisibility() {
    for (Tile& t : m_tiles) {
        t.isVisible = false;
    }
}

/**
 * @brief Compute the field of view from the given origin tile.
 *
 * Algorithm: Recursive Shadowcasting by Björn Bergström.
 * Reference: http://www.roguebasin.com/index.php/FOV_using_recursive_shadowcasting
 *
 * ── Algorithm Walk-through ──────────────────────────────────────────────────
 *
 * 1. Clear all visibility flags (ResetVisibility).
 * 2. Mark the origin tile as visible (the viewer can always see itself).
 * 3. Divide the 360° circle around the origin into 8 octants of 45° each.
 * 4. For each octant, call ShadowCastOctant() to cast light outward.
 *
 * ShadowCastOctant() processes one 45° wedge:
 *   a. Scan rows from the origin outward (row = 1, 2, … radius).
 *   b. Within each row, scan columns from left (most negative slope) to
 *      right (slope 0 = straight ahead).
 *   c. For each cell, compute its left-slope and right-slope boundaries.
 *   d. If the cell falls within the current visible wedge [startSlope, endSlope]:
 *        - Mark it visible.
 *        - If it is opaque (wall): recursively cast the portion of the wedge
 *          to the left of the wall, then continue the current scan with the
 *          updated shadow boundary.
 *
 * The octant coordinate transform {xx, xy, yx, yy} maps the algorithm's
 * local (col, row) coordinates to the map's (x, y) coordinates for each
 * of the 8 octants:
 *
 *   world_x = origin_x + col * xx + row * xy
 *   world_y = origin_y + col * yx + row * yy
 *
 * The 8 transform sets cover: E, SE, S, SW, W, NW, N, NE directions.
 *
 * ────────────────────────────────────────────────────────────────────────────
 */
void TileMap::ComputeFOV(int originX, int originY, int radius) {
    // Step 1: clear previous visibility
    ResetVisibility();

    // Step 2: origin tile is always visible (the viewer is standing here)
    if (InBounds(originX, originY)) {
        Tile& origin = m_tiles[Index(originX, originY)];
        origin.isVisible  = true;
        origin.isExplored = true;
    }

    // Step 3: octant multiplier table
    // ── TEACHING NOTE ────────────────────────────────────────────────────
    // Each column corresponds to one octant (0–7).
    // Row 0: multiplier for col → world_x
    // Row 1: multiplier for row → world_x
    // Row 2: multiplier for col → world_y
    // Row 3: multiplier for row → world_y
    //
    // These are all the 8 possible ways to reflect/rotate the base octant
    // (northeast quadrant) into the full 360° circle.
    // ─────────────────────────────────────────────────────────────────────
    static const int MULT[4][8] = {
        //  oct: 0   1   2   3   4   5   6   7
               { 1,  0,  0, -1, -1,  0,  0,  1},  // col → dx
               { 0,  1, -1,  0,  0, -1,  1,  0},  // row → dx
               { 0,  1,  1,  0,  0, -1, -1,  0},  // col → dy
               { 1,  0,  0,  1, -1,  0,  0, -1}   // row → dy
    };

    // Step 4: cast light in all 8 octants
    for (int oct = 0; oct < 8; ++oct) {
        ShadowCastOctant(originX, originY, radius,
                         /*row=*/1, /*startSlope=*/1.0f, /*endSlope=*/0.0f,
                         MULT[0][oct], MULT[1][oct],
                         MULT[2][oct], MULT[3][oct]);
    }
}

/**
 * @brief Recursive shadowcasting for one octant.
 *
 * @param cx, cy       Viewer position.
 * @param radius       Max visibility radius (in tiles).
 * @param row          Current row being scanned (distance from origin).
 * @param startSlope   Top bound of the visible wedge (more positive = steeper).
 * @param endSlope     Bottom bound of the visible wedge.
 * @param xx,xy,yx,yy  Octant transformation coefficients.
 *
 * TEACHING NOTE — Slope Convention
 * ──────────────────────────────────
 * "Slope" here is the tangent of the angle from the viewer to the tile edge.
 * A slope of 1.0 means 45° (the maximum extent of one octant).
 * A slope of 0.0 means 0° (the central axis of the octant).
 *
 * The visible wedge narrows as walls cast shadows.  When startSlope <=
 * endSlope, the wedge has been completely shadowed and we can return.
 *
 * The function processes tiles from (row, -row) to (row, 0) in octant-local
 * coordinates, which corresponds to the full width of that scan row.
 */
void TileMap::ShadowCastOctant(int cx, int cy, int radius,
                                int row,
                                float startSlope, float endSlope,
                                int xx, int xy, int yx, int yy)
{
    // Base case: visible wedge has collapsed entirely
    if (startSlope < endSlope) return;

    float nextStartSlope = startSlope;

    // Iterate scan rows from 'row' outward to the radius
    for (int i = row; i <= radius; ++i) {

        bool blocked = false;

        // ── Scan columns within this row ─────────────────────────────────
        // In octant-local space: column goes from -i (left edge) to 0 (center).
        // At row i, the leftmost column is -i and the rightmost is 0.
        for (int dx = -i, dy = -i; dx <= 0; ++dx) {

            // Slope of the left and right edges of this tile
            // TEACHING NOTE: We use 0.5-cell offsets to get the edges of the
            // tile rather than its centre, which produces cleaner shadow boundaries.
            float lSlope = (dx - 0.5f) / (dy + 0.5f);
            float rSlope = (dx + 0.5f) / (dy - 0.5f);

            // Skip tiles that are entirely outside the visible wedge
            if (startSlope < rSlope) continue;
            if (endSlope   > lSlope) break;

            // Transform from octant-local (dx, dy) to map (ax, ay)
            int ax = cx + dx * xx + dy * xy;
            int ay = cy + dx * yx + dy * yy;

            // Mark visible if within radius (circular bound check)
            // TEACHING NOTE: We compare squared distances to avoid sqrt.
            if (dx * dx + dy * dy <= radius * radius) {
                if (InBounds(ax, ay)) {
                    Tile& tile    = m_tiles[Index(ax, ay)];
                    tile.isVisible  = true;
                    tile.isExplored = true; // Explored is permanent
                }
            }

            // ── Shadow propagation ────────────────────────────────────────
            if (blocked) {
                // We were in a blocked (shadowed) region.
                // Check if the current tile opens up into light again.
                if (!InBounds(ax, ay) || TileTypeIsOpaque(m_tiles[Index(ax, ay)].type)) {
                    // Still blocked — update the shadow boundary
                    nextStartSlope = rSlope;
                    continue;
                } else {
                    // Light resumed — reset block tracking
                    blocked    = false;
                    startSlope = nextStartSlope;
                }
            } else {
                // We are in a lit region.
                // Check if the current tile starts a new shadow.
                if (InBounds(ax, ay) && TileTypeIsOpaque(m_tiles[Index(ax, ay)].type)
                    && i < radius) {
                    // This tile is a wall — it casts a shadow behind it.
                    // Recursively process the still-lit portion to the left.
                    blocked        = true;
                    nextStartSlope = rSlope;
                    ShadowCastOctant(cx, cy, radius, i + 1,
                                     startSlope, lSlope,
                                     xx, xy, yx, yy);
                }
            }
        } // end column scan

        // If the entire row was shadowed, stop scanning further rows
        if (blocked) break;

    } // end row scan
}


// ===========================================================================
// Serialisation
// ===========================================================================

/**
 * @brief Convert a TileType to its single-character representation.
 * Mirrors GetTileChar() but is a plain function for internal use.
 */
static char TileTypeToChar(TileType t) {
    return GetTileChar(t);
}

/**
 * @brief Convert a single character back to its TileType.
 *
 * TEACHING NOTE — Bidirectional Mapping
 * ──────────────────────────────────────
 * Serialisation requires a bijection: TileType → char (write) and
 * char → TileType (read).  The two switch statements implement both
 * directions.  Keeping them adjacent makes it easy to verify they are
 * consistent (every case in one matches a case in the other).
 *
 * If a character is unrecognised (e.g. from a newer version of the engine),
 * we default to FLOOR so the map is still playable.  A production engine
 * would log a warning and potentially refuse to load.
 */
static TileType CharToTileType(char c) {
    switch (c) {
        case '.': return TileType::FLOOR;
        case '#': return TileType::WALL;
        case '~': return TileType::WATER;
        case ',': return TileType::GRASS;
        case ':': return TileType::SAND;
        case 'T': return TileType::FOREST;
        case '^': return TileType::MOUNTAIN;
        case '=': return TileType::ROAD;
        case '+': return TileType::DOOR;
        case '<': return TileType::STAIRS_UP;
        case '>': return TileType::STAIRS_DOWN;
        case 'C': return TileType::CHEST;
        case 'S': return TileType::SAVE_POINT;
        case '$': return TileType::SHOP_TILE;
        case 'I': return TileType::INN_TILE;
        case 'B': return TileType::BRIDGE;
        default:
            LOG_WARN("CharToTileType: unknown char '" << c << "' — defaulting to FLOOR");
            return TileType::FLOOR;
    }
}

/**
 * @brief Serialise the tile map to a compact text string.
 *
 * TEACHING NOTE — std::ostringstream
 * ────────────────────────────────────
 * std::ostringstream accumulates string output like `std::cout` but
 * writes to an in-memory string buffer.  Calling `.str()` retrieves the
 * accumulated string.
 *
 * Alternative: build a `std::string` with `+=` for each character.
 * `+=` with a single char is O(1) amortised (no reallocation if capacity
 * allows).  For large maps, pre-reserving (`result.reserve(...)`) is faster
 * than using ostringstream.
 *
 * We choose ostringstream here to teach the pattern; performance-critical
 * code might use the reserve-then-append approach instead.
 */
std::string TileMap::Serialize() const {
    std::string result;
    // Pre-reserve: header "W H\n" + W chars per row + newline per row
    result.reserve(32 + (m_width + 1) * m_height);

    // Header line: width and height
    result += std::to_string(m_width) + " " + std::to_string(m_height) + "\n";

    // One row per line, one char per tile
    for (uint32_t y = 0; y < m_height; ++y) {
        for (uint32_t x = 0; x < m_width; ++x) {
            result += TileTypeToChar(m_tiles[Index(static_cast<int>(x),
                                                   static_cast<int>(y))].type);
        }
        result += '\n';
    }

    LOG_DEBUG("TileMap::Serialize: " << m_width << "x" << m_height
              << " -> " << result.size() << " bytes");
    return result;
}

/**
 * @brief Deserialise a map from a string produced by Serialize().
 *
 * TEACHING NOTE — std::istringstream for Parsing
 * ─────────────────────────────────────────────────
 * `std::istringstream` wraps a string and presents it as an input stream.
 * We use:
 *   ss >> width >> height;    to read the two integers on the header line.
 *   std::getline(ss, line);   to read each subsequent row character by character.
 *
 * The `ss.ignore()` call after reading integers skips the newline that
 * `>>` leaves unread in the stream buffer.
 *
 * TEACHING NOTE — Error Tolerance
 * ─────────────────────────────────
 * If the string is malformed (e.g. wrong number of rows), we log a warning
 * and stop rather than crashing.  Partial maps are better than crashes.
 */
void TileMap::Deserialize(const std::string& data) {
    std::istringstream ss(data);

    uint32_t w = 0, h = 0;
    if (!(ss >> w >> h)) {
        LOG_WARN("TileMap::Deserialize: failed to read header (width height)");
        return;
    }
    ss.ignore(256, '\n'); // Skip rest of the header line (including '\n')

    // Re-initialise the map to the new dimensions (all WALL by default)
    m_width  = w;
    m_height = h;
    m_tiles.assign(static_cast<size_t>(w) * h, Tile{});

    for (uint32_t y = 0; y < h; ++y) {
        std::string line;
        if (!std::getline(ss, line)) {
            LOG_WARN("TileMap::Deserialize: expected row " << y
                     << " but stream ended — map may be incomplete");
            break;
        }
        for (uint32_t x = 0; x < w; ++x) {
            if (x < line.size()) {
                // SetTile() updates isPassable from type automatically.
                SetTile(static_cast<int>(x), static_cast<int>(y),
                        CharToTileType(line[x]));
            }
            // If line is shorter than expected, the tile stays WALL.
        }
    }

    LOG_DEBUG("TileMap::Deserialize: loaded " << w << "x" << h << " map");
}


// ===========================================================================
// Map Generation — Empty Room
// ===========================================================================

/**
 * @brief Generate a simple walled rectangular room.
 *
 * TEACHING NOTE — Two-Pass Room Generation
 * ──────────────────────────────────────────
 * We use two passes:
 *   Pass 1: fill every tile with FLOOR (faster than selective setting).
 *   Pass 2: overwrite the border tiles with WALL.
 *
 * Pass 1 is O(w * h); Pass 2 is O(2w + 2h).  Together still O(w * h).
 * An alternative is a single pass with an `if (border) WALL else FLOOR`
 * branch, but two passes are clearer and the branch-free approach can
 * actually be slower due to branch misprediction on small maps.
 */
void TileMap::GenerateEmptyRoom(uint32_t w, uint32_t h) {
    m_width  = w;
    m_height = h;
    m_tiles.assign(static_cast<size_t>(w) * h, Tile{});

    // Pass 1: fill interior with FLOOR
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            SetTile(static_cast<int>(x), static_cast<int>(y), TileType::FLOOR);
        }
    }

    // Pass 2: overwrite border with WALL
    for (uint32_t x = 0; x < w; ++x) {
        SetTile(static_cast<int>(x), 0,               TileType::WALL); // top
        SetTile(static_cast<int>(x), static_cast<int>(h - 1), TileType::WALL); // bottom
    }
    for (uint32_t y = 0; y < h; ++y) {
        SetTile(0,               static_cast<int>(y), TileType::WALL); // left
        SetTile(static_cast<int>(w - 1), static_cast<int>(y), TileType::WALL); // right
    }

    LOG_DEBUG("TileMap::GenerateEmptyRoom: " << w << "x" << h);
}


// ===========================================================================
// Map Generation — Dungeon (with seeding)
// ===========================================================================

/**
 * @brief Generate a random dungeon (random seed from std::random_device).
 *
 * TEACHING NOTE — std::random_device
 * ─────────────────────────────────────
 * `std::random_device` is a non-deterministic entropy source backed by
 * the OS (e.g. /dev/urandom on Linux, CryptGenRandom on Windows).
 * `rd()` returns a single random 32-bit value suitable for seeding a PRNG.
 *
 * Note: on some platforms (e.g. MinGW on Windows), `std::random_device`
 * may be deterministic.  Using a time-based seed as a fallback is
 * common in portable code.
 */
void TileMap::GenerateDungeon() {
    std::random_device rd;
    uint32_t seed = rd();
    LOG_INFO("TileMap::GenerateDungeon: using random seed " << seed);
    GenerateDungeonImpl(seed);
}

void TileMap::GenerateDungeon(uint32_t seed) {
    LOG_INFO("TileMap::GenerateDungeon: using fixed seed " << seed);
    GenerateDungeonImpl(seed);
}

/**
 * @brief Internal dungeon generation: Room-Corridor algorithm.
 *
 * ── Algorithm Steps ─────────────────────────────────────────────────────────
 *
 * 1. Fill entire map with WALL.
 * 2. Attempt to place up to MAX_ROOMS rooms:
 *      a. Pick random position (rx, ry) and size (rw × rh).
 *      b. Check: no overlap with existing rooms (1-tile padding).
 *      c. If valid: carve FLOOR tiles, add to room list.
 * 3. After placing each new room (after the first), connect to the previous
 *    room with an L-shaped corridor:
 *      a. Find centres of both rooms: (cx1,cy1) and (cx2,cy2).
 *      b. Coin flip: H-then-V or V-then-H.
 *      c. Carve FLOOR along both legs.
 * 4. Place STAIRS_UP in the first room's centre.
 * 5. Place STAIRS_DOWN in the last room's centre.
 * 6. Optionally place a SAVE_POINT and CHESTs in random rooms.
 *
 * TEACHING NOTE — Overlap Check
 * ──────────────────────────────
 * The overlap test uses the AABB (Axis-Aligned Bounding Box) intersection
 * test, augmented by a 1-tile padding to prevent rooms touching.  Two
 * rectangles (r1 and r2) do NOT overlap when:
 *   r1.right  <= r2.left    OR
 *   r2.right  <= r1.left    OR
 *   r1.bottom <= r2.top     OR
 *   r2.bottom <= r1.top
 * With padding=1, extend each rectangle by 1 in all directions before
 * checking.
 *
 * TEACHING NOTE — L-Shaped Corridors
 * ─────────────────────────────────────
 * An L-corridor is two perpendicular straight corridors meeting at a corner.
 * For centres (cx1,cy1) and (cx2,cy2):
 *   Option A (H then V):
 *     1. Walk from cx1 to cx2 along row cy1 (horizontal leg).
 *     2. Walk from cy1 to cy2 along column cx2 (vertical leg).
 *   Option B (V then H):
 *     1. Walk from cy1 to cy2 along column cx1.
 *     2. Walk from cx1 to cx2 along row cy2.
 * The random coin flip between options gives varied junction positions.
 *
 * ────────────────────────────────────────────────────────────────────────────
 */
void TileMap::GenerateDungeonImpl(uint32_t seed) {
    // Guard: need at least a minimal map
    if (m_width < 10 || m_height < 10) {
        LOG_WARN("TileMap::GenerateDungeon: map too small (" << m_width
                 << "x" << m_height << "), skipping generation");
        return;
    }

    // Step 1: Fill everything with WALL
    m_tiles.assign(static_cast<size_t>(m_width) * m_height, Tile{});

    // Step 2: Set up the PRNG
    // TEACHING NOTE: std::mt19937 is a Mersenne Twister PRNG.
    //   - Period: 2^19937 − 1 (astronomically long — effectively infinite).
    //   - Passes most statistical randomness tests.
    //   - Uniform distributions via std::uniform_int_distribution.
    std::mt19937 rng(seed);
    auto randInt = [&](int lo, int hi) -> int {
        // TEACHING NOTE: std::uniform_int_distribution produces integers in
        // [lo, hi] with equal probability (inclusive on both ends).
        std::uniform_int_distribution<int> dist(lo, hi);
        return dist(rng);
    };

    // ── Generation parameters ─────────────────────────────────────────────
    // TEACHING NOTE: These magic numbers control dungeon feel.
    //   Increase MAX_ROOMS for larger, more complex dungeons.
    //   Increase MAX_SIZE for bigger rooms.
    //   Increase attempts multiplier if too many rooms fail overlap checks.
    const int MAX_ROOMS    = std::min(15, static_cast<int>(m_width * m_height / 50));
    const int MIN_ROOM_W   = 4;
    const int MAX_ROOM_W   = std::min(12, static_cast<int>(m_width  / 3));
    const int MIN_ROOM_H   = 4;
    const int MAX_ROOM_H   = std::min(10, static_cast<int>(m_height / 3));
    const int MAX_ATTEMPTS = MAX_ROOMS * 5; // Give up after this many failed placements

    // ── Room list ─────────────────────────────────────────────────────────
    struct Room {
        int x, y, w, h;
        int CentreX() const { return x + w / 2; }
        int CentreY() const { return y + h / 2; }
    };
    std::vector<Room> rooms;
    rooms.reserve(MAX_ROOMS);

    // ── Attempt to place rooms ─────────────────────────────────────────────
    int attempts = 0;
    while (static_cast<int>(rooms.size()) < MAX_ROOMS && attempts < MAX_ATTEMPTS) {
        ++attempts;

        int rw = randInt(MIN_ROOM_W, MAX_ROOM_W);
        int rh = randInt(MIN_ROOM_H, MAX_ROOM_H);
        int rx = randInt(1, static_cast<int>(m_width)  - rw - 2);
        int ry = randInt(1, static_cast<int>(m_height) - rh - 2);

        Room newRoom{ rx, ry, rw, rh };

        // Check overlap (with 1-tile padding)
        bool overlaps = false;
        for (const Room& existing : rooms) {
            // AABB check with padding=1 on each side
            if (rx - 1 < existing.x + existing.w + 1 &&
                rx + rw + 1 > existing.x - 1 &&
                ry - 1 < existing.y + existing.h + 1 &&
                ry + rh + 1 > existing.y - 1) {
                overlaps = true;
                break;
            }
        }
        if (overlaps) continue;

        // ── Carve the room ─────────────────────────────────────────────────
        for (int y = ry; y < ry + rh; ++y) {
            for (int x = rx; x < rx + rw; ++x) {
                SetTile(x, y, TileType::FLOOR);
            }
        }

        // ── Connect to previous room via L-corridor ────────────────────────
        if (!rooms.empty()) {
            const Room& prev = rooms.back();
            int cx1 = prev.CentreX(), cy1 = prev.CentreY();
            int cx2 = newRoom.CentreX(), cy2 = newRoom.CentreY();

            if (randInt(0, 1) == 0) {
                // Option A: Horizontal leg first, then Vertical leg
                // Walk along row cy1 from cx1 to cx2
                for (int x = std::min(cx1, cx2); x <= std::max(cx1, cx2); ++x)
                    SetTile(x, cy1, TileType::FLOOR);
                // Walk along column cx2 from cy1 to cy2
                for (int y = std::min(cy1, cy2); y <= std::max(cy1, cy2); ++y)
                    SetTile(cx2, y, TileType::FLOOR);
            } else {
                // Option B: Vertical leg first, then Horizontal leg
                // Walk along column cx1 from cy1 to cy2
                for (int y = std::min(cy1, cy2); y <= std::max(cy1, cy2); ++y)
                    SetTile(cx1, y, TileType::FLOOR);
                // Walk along row cy2 from cx1 to cx2
                for (int x = std::min(cx1, cx2); x <= std::max(cx1, cx2); ++x)
                    SetTile(x, cy2, TileType::FLOOR);
            }
        }

        rooms.push_back(newRoom);
    }

    if (rooms.empty()) {
        LOG_WARN("TileMap::GenerateDungeon: no rooms placed — map remains all-wall");
        return;
    }

    // ── Place STAIRS_UP in first room, STAIRS_DOWN in last room ───────────
    {
        const Room& first = rooms.front();
        SetTile(first.CentreX(), first.CentreY(), TileType::STAIRS_UP);
    }
    {
        const Room& last = rooms.back();
        SetTile(last.CentreX(), last.CentreY(), TileType::STAIRS_DOWN);
    }

    // ── Place a SAVE_POINT in a random middle room (if enough rooms) ──────
    if (rooms.size() >= 3) {
        int saveRoomIdx = randInt(1, static_cast<int>(rooms.size()) - 2);
        const Room& saveRoom = rooms[static_cast<size_t>(saveRoomIdx)];
        // Offset slightly so it doesn't block the corridor junction
        int sx = saveRoom.x + 1;
        int sy = saveRoom.y + 1;
        if (InBounds(sx, sy)) SetTile(sx, sy, TileType::SAVE_POINT);
    }

    // ── Place CHESTs in random rooms ──────────────────────────────────────
    // TEACHING NOTE: A real game would fill chests with random loot here.
    // For now we just place the tile; the Zone class attaches a chest entity.
    int numChests = std::min(static_cast<int>(rooms.size()), 3);
    for (int c = 0; c < numChests; ++c) {
        const Room& r = rooms[static_cast<size_t>(randInt(0, static_cast<int>(rooms.size()) - 1))];
        int cx = r.x + randInt(1, r.w - 2);
        int cy = r.y + randInt(1, r.h - 2);
        if (InBounds(cx, cy) && m_tiles[Index(cx, cy)].type == TileType::FLOOR) {
            SetTile(cx, cy, TileType::CHEST);
        }
    }

    LOG_INFO("TileMap::GenerateDungeon: placed " << rooms.size()
             << " rooms in a " << m_width << "x" << m_height << " map (seed="
             << seed << ")");
}
