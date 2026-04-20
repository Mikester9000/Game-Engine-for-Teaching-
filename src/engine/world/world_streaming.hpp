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
 *      └──── onComplete(!ok) ◄─────┘                           │
 *      └─────────────────────────────── EvictCells() ──────────┘
 *
 *   LOADING → EVICTING is not implemented in this skeleton (it would require
 *   a cancellation token in AsyncLoader).  TODO for full M7 implementation.
 *
 * ─── Thread Safety ───────────────────────────────────────────────────────────
 * All public methods of WorldStreamingManager MUST be called from the main
 * thread.  The AsyncLoader handles its own internal locking.
 *
 * ─── TODO (M7 full implementation) ──────────────────────────────────────────
 *   • Wire real Zone::Load / Zone::Unload calls inside the job lambdas.
 *   • Integrate AssetLoader to read cooked cell data (.level files).
 *   • Add cancellation: if a cell becomes irrelevant while in LOADING state,
 *     the completion callback should discard the result instead of spawning.
 *   • Add frame budget cap: limit PumpMainThreadCompletions to ≤ N completions
 *     per frame so ECS spawning never exceeds 2 ms per the M7 spec.
 *   • Add debug overlay (ImGui): draw loaded/loading/evicting cells as a 2D
 *     minimap for visual debugging during development.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
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
     * @return              true on success.
     */
    bool Init(float cellSize     = 256.0f,
              int   streamRadius = 1);

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
     *
     * TODO: wire real Zone::Load + AssetLoader calls here.
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
     *
     * TODO: wire Zone::SpawnEnemies / Zone::SpawnNPCs calls here.
     */
    virtual void OnCellLoaded(uint32_t id, CellCoord coord, bool success);

    /**
     * @brief Called on the MAIN THREAD when a cell should be evicted.
     *
     * @param id    Cell ID to evict.
     * @param coord Corresponding grid coordinate.
     *
     * TODO: wire Zone::Unload + ECS entity destruction here.
     */
    virtual void OnEvictCell(uint32_t id, CellCoord coord);

    // =========================================================================
    // Accessors (for tests and debugging)
    // =========================================================================

    /// Read-only access to the spatial partition.
    [[nodiscard]] const WorldPartition& Partition() const noexcept
    {
        return m_partition;
    }

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
     * Only LOADED cells are evicted synchronously in this skeleton.
     * Cells still in LOADING state are skipped (cancellation is a TODO).
     */
    void EvictCells(const std::vector<uint32_t>& cellIds);

    // =========================================================================
    // Member data
    // =========================================================================

    WorldPartition m_partition;                  ///< Grid geometry queries.
    AsyncLoader    m_loader;                     ///< Background I/O thread.

    /// State of every cell this manager is aware of.
    std::unordered_map<uint32_t, CellState> m_cellStates;

    /// IDs of cells that have finished loading (superseded by m_cellStates).
    std::unordered_set<uint32_t>            m_loadedCells;

    bool m_initialised = false;
};

} // namespace world
} // namespace engine
