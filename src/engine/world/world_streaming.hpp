/**
 * @file world_streaming.hpp
 * @brief WorldStreamingManager — coordinates cell load/evict with proximity.
 *
 * ============================================================================
 * TEACHING NOTE — World Streaming Architecture
 * ============================================================================
 * WorldStreamingManager is the top-level controller for M7.  It owns:
 *
 *   1. A WorldPartition  — the spatial grid that answers "which cells are
 *                          near the camera?" (pure geometry, no I/O).
 *   2. An AsyncLoader    — the worker thread that performs actual I/O without
 *                          blocking the main thread.
 *
 * Every frame (Update call), the manager:
 *   a. Queries the partition for the desired set of cells.
 *   b. Computes the DELTA: newly-required cells (→ enqueue load) and
 *      cells that are now too far away (→ enqueue evict / unload).
 *   c. Pumps the AsyncLoader's completion queue so callbacks (Zone spawn,
 *      ECS creation) execute safely on the main thread.
 *
 * ─── State Machine per Cell ──────────────────────────────────────────────────
 *
 *   UNLOADED ──EnqueueLoad()──► LOADING ──onComplete(ok)──► LOADED
 *      ▲                           │                           │
 *      │       EvictCells()        │                           │
 *      └──────────────────── CancelJob() ◄────────────────────┘
 *
 *   M7.3: LOADING → UNLOADED is now supported via CancelJob().
 *   When a cell moves out of range while still LOADING, EvictCells()
 *   calls CancelJob() and immediately marks the cell as UNLOADED — no
 *   completion callback is ever fired.
 *
 * ─── Thread Safety ───────────────────────────────────────────────────────────
 * All public methods of WorldStreamingManager MUST be called from the main
 * thread.  The AsyncLoader handles its own internal locking.
 *
 * ─── M7 Full Implementation Status ──────────────────────────────────────────
 *   ✅ Zone::Load / Zone::Unload wired in GameStreamingManager (M7.1).
 *   ✅ AssetLoader::LoadRaw for .level files in GameStreamingManager (M7.2).
 *   ✅ CancelJob + cancellation token in AsyncLoader (M7.3).
 *   ✅ Per-frame completion budget via m_maxCompletionsPerFrame (M7.4).
 *   ✅ DrawDebugOverlay (ImGui minimap) when BUILD_EDITOR is ON (M7.5).
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.1
 * @date    2024
 * C++ Standard: C++17
 * Platform: All (no platform-specific code; async_loader uses std::thread)
 */

#pragma once

#include "engine/world/async_loader.hpp"
#include "engine/world/world_partition.hpp"
#include "engine/math/math_types.hpp"

#include <unordered_map>   // std::unordered_map — cell state table
#include <unordered_set>   // std::unordered_set — loaded cell set
#include <functional>      // std::function
#include <cstdint>         // uint32_t

// Forward declaration for ECS World — avoids pulling in the 2 000-line ECS.hpp
// into every file that includes world_streaming.hpp.
// Subclasses that need to call World methods must include ECS.hpp themselves.
class World;

// Forward declaration for Dear ImGui draw list — only used when BUILD_EDITOR
// is defined.  ImDrawList is defined in imgui.h.
struct ImDrawList;

namespace engine {
namespace world {

// ===========================================================================
// CellState
// ===========================================================================

/**
 * @brief Lifecycle state of a single streaming cell.
 *
 * TEACHING NOTE — State Machine Enum
 * ────────────────────────────────────
 * Tracking per-cell state prevents duplicate loads (enqueuing LOADING cells
 * again) and dangling evictions (evicting cells that are still loading).
 * Using a named enum (rather than raw booleans) makes intent explicit and
 * debugging straightforward.
 */
enum class CellState : uint8_t
{
    Unloaded = 0,   ///< Cell has no data in memory.
    Loading,        ///< Load job is queued or in progress.
    Loaded,         ///< All cell data is in memory, entities are spawned.
    Evicting,       ///< Eviction is in progress (entities being destroyed).
};

// ===========================================================================
// WorldStreamingManager
// ===========================================================================

/**
 * @brief Top-level coordinator for proximity-based world cell streaming.
 *
 * Typical frame loop:
 * @code
 *   WorldStreamingManager mgr;
 *   mgr.Init();
 *
 *   // Each frame:
 *   engine::math::Vec3 camPos = GetCameraPosition();
 *   mgr.Update(camPos);
 *
 *   // On shutdown:
 *   mgr.Shutdown();
 * @endcode
 *
 * TEACHING NOTE — Composition over inheritance
 * ──────────────────────────────────────────────
 * WorldStreamingManager OWNS (as members) a WorldPartition and an AsyncLoader.
 * It does NOT inherit from them.  Composition gives more flexibility: the
 * streaming behaviour can be changed by swapping implementations without
 * altering the public API or breaking callers.
 */
class WorldStreamingManager
{
public:
    // =========================================================================
    // Construction / lifecycle
    // =========================================================================

    WorldStreamingManager() = default;
    ~WorldStreamingManager();

    WorldStreamingManager(const WorldStreamingManager&)            = delete;
    WorldStreamingManager& operator=(const WorldStreamingManager&) = delete;

    /**
     * @brief Initialise the streaming manager and start the async loader.
     *
     * @param cellSize      World-space side length of one cell (metres).
     * @param streamRadius  Chebyshev radius of cells to keep loaded.
     * @param world         Optional ECS World pointer passed to subclass hooks.
     *                      Stored as m_world (protected); subclasses use it in
     *                      OnCellLoaded / OnEvictCell for entity spawning.
     * @return              true on success.
     *
     * TEACHING NOTE — Optional ECS World reference
     * ──────────────────────────────────────────────
     * The base WorldStreamingManager is an engine-layer class that knows
     * nothing about ECS entities.  Passing World* here lets the game-layer
     * GameStreamingManager use the same Init() call while still receiving
     * a reference to the ECS World for Zone::SpawnEnemies etc.
     * The base class stores the pointer but never dereferences it; that is
     * solely the responsibility of subclasses.
     */
    bool Init(float  cellSize     = 256.0f,
              int    streamRadius = 1,
              World* world        = nullptr);

    /**
     * @brief Flush all pending loads and evict all cells.
     *
     * Blocks until the async loader's worker thread exits.
     */
    void Shutdown();

    // =========================================================================
    // Per-frame update
    // =========================================================================

    /**
     * @brief Drive streaming: load nearby cells, evict far cells.
     *
     * Call once per frame from the main thread.
     *
     * @param viewPos  Current camera / player world-space position.
     *
     * TEACHING NOTE — Delta computation
     * ────────────────────────────────────
     * Each frame we compare:
     *   • desired set = GetCellsNearPosition(viewPos)
     *   • current set = m_loadedCells
     *
     *   toLoad  = desired − current  (new cells that have come in range)
     *   toEvict = current − desired  (cells that have gone out of range)
     *
     * Set subtraction on unordered_sets is O(N) where N is the streaming
     * radius squared — typically ≤ 25 cells, so negligible CPU cost.
     */
    void Update(const engine::math::Vec3& viewPos);

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set the maximum number of cell-load callbacks processed per frame.
     *
     * @param n  Cap value.  0 = unlimited (drain all completions every frame).
     *           Default is 4 per the M7 spec (≤ 2 ms budget for ECS spawning).
     *
     * TEACHING NOTE — Frame Budget Cap (M7.4)
     * ─────────────────────────────────────────
     * Spawning many entities simultaneously (e.g. 25 cells all completing
     * in the same frame for a radius-2 load) can spike the main thread beyond
     * the 2 ms frame budget.  By limiting completions to N per frame, the work
     * is spread across multiple frames — each frame stays under budget.
     *
     * Remaining completions accumulate in AsyncLoader's completed queue and
     * are drained in subsequent frames.
     */
    void SetMaxCompletionsPerFrame(int n) { m_maxCompletionsPerFrame = n; }

    // =========================================================================
    // Query API
    // =========================================================================

    /**
     * @brief Return the current state of a cell.
     *
     * @param id  Cell ID (from CellIdFromCoord).
     * @return    CellState enum value; CellState::Unloaded if not tracked.
     */
    [[nodiscard]] CellState GetCellState(uint32_t id) const;

    /**
     * @brief Number of cells currently in the LOADED state.
     */
    [[nodiscard]] int LoadedCellCount() const;

    /**
     * @brief Number of cells currently in the LOADING state.
     */
    [[nodiscard]] int LoadingCellCount() const;

    // =========================================================================
    // Streaming hooks (override for real integration)
    // =========================================================================

    /**
     * @brief Called on the WORKER THREAD when a cell should be loaded.
     *
     * Override this (or replace with a lambda via SetLoadCallback) to perform
     * real I/O: read the .level file, deserialise entity data, etc.
     *
     * MUST be thread-safe.
     *
     * @param id     Cell ID being loaded.
     * @param coord  Corresponding grid coordinate.
     * @return       true on success.
     *
     * TEACHING NOTE — Callback vs. virtual method
     * ────────────────────────────────────────────
     * We expose this as a public virtual method (and also via SetLoadCallback)
     * so the class can be extended in two ways:
     *   1. Subclass and override (OOP / polymorphism approach).
     *   2. Set a std::function callback (functional / composition approach).
     * Both are valid; the callback approach is used in the skeleton to avoid
     * a forced inheritance hierarchy before M7 is complete.
     */
    virtual bool OnLoadCell(uint32_t id, CellCoord coord);

    /**
     * @brief Called on the MAIN THREAD when a cell's load has completed.
     *
     * Override to spawn ECS entities, initialise Zone rendering, etc.
     *
     * @param id      Cell ID that finished loading.
     * @param coord   Corresponding grid coordinate.
     * @param success Whether the load succeeded.
     */
    virtual void OnCellLoaded(uint32_t id, CellCoord coord, bool success);

    /**
     * @brief Called on the MAIN THREAD when a cell should be evicted.
     *
     * @param id    Cell ID to evict.
     * @param coord Corresponding grid coordinate.
     */
    virtual void OnEvictCell(uint32_t id, CellCoord coord);

    // =========================================================================
    // Debug overlay (M7.5)
    // =========================================================================

#ifdef BUILD_EDITOR
    /**
     * @brief Draw a 2D minimap of streaming cell states using ImGui.
     *
     * MUST be called between ImGui::NewFrame() and ImGui::Render() on the
     * main thread.
     *
     * @param drawList   ImGui draw list for the current window/overlay.
     * @param originX    Screen X coordinate of the top-left corner of the grid.
     * @param originY    Screen Y coordinate of the top-left corner.
     * @param cellPxSize Pixel size of each cell rectangle.
     * @param cameraPos  Current camera world-space position (used to find
     *                   the "current cell" which is outlined in white).
     *
     * TEACHING NOTE — Decoupled Debug Visualisation
     * ────────────────────────────────────────────────
     * DrawDebugOverlay is gated by BUILD_EDITOR so there is zero overhead in
     * shipping builds.  The editor build defines BUILD_EDITOR=1 and links
     * imgui::imgui, giving it access to ImDrawList.
     *
     * Colour legend:
     *   Grey   — UNLOADED (no data in memory)
     *   Yellow — LOADING  (I/O in progress on worker thread)
     *   Green  — LOADED   (entities spawned, ready)
     *   Red    — EVICTING (entities being destroyed)
     *   White outline — the cell currently containing the camera
     */
    void DrawDebugOverlay(ImDrawList*                   drawList,
                          float                         originX,
                          float                         originY,
                          float                         cellPxSize,
                          const engine::math::Vec3&     cameraPos) const;
#endif // BUILD_EDITOR

    // =========================================================================
    // Accessors (for tests and debugging)
    // =========================================================================

    /// Read-only access to the spatial partition.
    [[nodiscard]] const WorldPartition& Partition() const noexcept
    {
        return m_partition;
    }

protected:
    // =========================================================================
    // Protected member data (accessible to game-layer subclasses)
    // =========================================================================

    /// ECS World pointer passed to Init().  nullptr if not wired.
    /// Subclasses (e.g. GameStreamingManager) use this in OnCellLoaded /
    /// OnEvictCell to call Zone::SpawnEnemies / Zone::Unload.
    World* m_world = nullptr;

    /// State of every cell this manager is aware of.
    /// Protected so subclasses can read cell states in their hook overrides.
    std::unordered_map<uint32_t, CellState> m_cellStates;

    /// IDs of cells that have finished loading.
    std::unordered_set<uint32_t>            m_loadedCells;

private:
    // =========================================================================
    // Internal helpers
    // =========================================================================

    /**
     * @brief Request that a set of cells be loaded (enqueue jobs for UNLOADED).
     *
     * TEACHING NOTE — Guard against duplicate enqueue
     * ─────────────────────────────────────────────────
     * We only enqueue a cell if its current state is Unloaded.  If it is
     * already Loading or Loaded, we skip it silently.  This prevents sending
     * two load jobs for the same cell (which would spawn entities twice).
     */
    void RequestCells(const std::vector<CellCoord>& coords);

    /**
     * @brief Evict a set of cells that are no longer in range.
     *
     * M7.3: Cells in LOADING state are now cancelled via CancelJob() and
     * immediately transitioned to UNLOADED — no completion callback fires.
     *
     * TEACHING NOTE — Cancellation for LOADING cells
     * ─────────────────────────────────────────────────
     * Without cancellation, a cell that moves out of range while loading
     * would continue loading on the worker thread and then trigger a
     * completion callback that would spawn entities for an area the player
     * has already left.  CancelJob() prevents this waste.
     */
    void EvictCells(const std::vector<uint32_t>& cellIds);

    // =========================================================================
    // Member data
    // =========================================================================

    WorldPartition m_partition;                  ///< Grid geometry queries.
    AsyncLoader    m_loader;                     ///< Background I/O thread.

    /// Maximum completions processed per Update() call (M7.4 frame budget).
    /// 0 = unlimited.  Default 4 per M7 spec.
    int m_maxCompletionsPerFrame = 4;

    bool m_initialised = false;
};

} // namespace world
} // namespace engine
