/**
 * @file world_streaming.cpp
 * @brief WorldStreamingManager implementation.
 *
 * ============================================================================
 * TEACHING NOTE — Separation of concerns in M7
 * ============================================================================
 * This file wires together the two subsystems:
 *
 *   WorldPartition   — "which cells should be loaded right now?"
 *   AsyncLoader      — "load them without blocking the main thread"
 *
 * WorldStreamingManager is deliberately thin in this skeleton: it delegates
 * geometry queries to WorldPartition and I/O to AsyncLoader.  The real logic
 * lives in the delta computation (RequestCells / EvictCells) and the virtual
 * OnLoadCell / OnCellLoaded / OnEvictCell hooks.
 *
 * When M7 reaches full implementation, the hooks will call:
 *   • AssetLoader::LoadRaw() to read cooked .level data from disk.
 *   • Zone::Load()          to build the TileMap and register spawn points.
 *   • Zone::SpawnEnemies()  to create ECS entities on the main thread.
 *   • Zone::Unload()        to destroy ECS entities on eviction.
 *
 * ============================================================================
 */

#include "engine/world/world_streaming.hpp"
#include "engine/core/Logger.hpp"    // LOG_INFO, LOG_WARN, LOG_ERROR

#include <algorithm>   // std::remove_if (future use)
#include <utility>     // std::move

namespace engine {
namespace world {

// ===========================================================================
// Destructor
// ===========================================================================

WorldStreamingManager::~WorldStreamingManager()
{
    if (m_initialised)
        Shutdown();
}

// ===========================================================================
// Lifecycle
// ===========================================================================

bool WorldStreamingManager::Init(float cellSize, int streamRadius)
{
    if (m_initialised)
    {
        LOG_WARN("WorldStreamingManager::Init: already initialised.");
        return true;
    }

    // Construct the partition with the requested parameters.
    m_partition = WorldPartition(cellSize, streamRadius);

    // Start the worker thread.
    m_loader.Start();

    m_initialised = true;

    LOG_INFO("WorldStreamingManager: initialised. "
             "CellSize=" << cellSize << " m, "
             "StreamRadius=" << streamRadius);
    return true;
}

void WorldStreamingManager::Shutdown()
{
    if (!m_initialised)
        return;

    LOG_INFO("WorldStreamingManager: shutting down...");

    // Stop the async loader first (waits for worker thread to exit).
    m_loader.Stop();

    // Evict all currently loaded cells (synchronous, main thread).
    // TEACHING NOTE — Shutdown order
    // ─────────────────────────────────
    // We stop the loader BEFORE evicting cells because:
    //   1. New load completions after Stop() would add to m_loadedCells.
    //   2. Evicting while new completions arrive is a TOCTOU race.
    // After Stop() returns, the completion queue is drained by pumping
    // once more, then we evict all loaded cells in deterministic order.

    // Pump any final completions from the loader's completed queue.
    m_loader.PumpMainThreadCompletions();

    // Evict all loaded cells.
    std::vector<uint32_t> allLoaded(m_loadedCells.begin(), m_loadedCells.end());
    EvictCells(allLoaded);

    m_cellStates.clear();
    m_loadedCells.clear();

    m_initialised = false;
    LOG_INFO("WorldStreamingManager: shut down.");
}

// ===========================================================================
// Per-frame update
// ===========================================================================

void WorldStreamingManager::Update(const engine::math::Vec3& viewPos)
{
    if (!m_initialised)
        return;

    // --- Pump completions first (may transition cells from LOADING → LOADED) ---
    m_loader.PumpMainThreadCompletions();

    // --- Compute desired cell set ---
    const std::vector<CellCoord> desired = m_partition.GetCellsNearPosition(viewPos);

    // Build a set of desired IDs for fast lookup.
    std::unordered_set<uint32_t> desiredIds;
    desiredIds.reserve(desired.size());
    for (const CellCoord& c : desired)
        desiredIds.insert(CellIdFromCoord(c));

    // --- Compute cells to evict: loaded but no longer desired ---
    std::vector<uint32_t> toEvict;
    for (const uint32_t id : m_loadedCells)
    {
        if (desiredIds.find(id) == desiredIds.end())
            toEvict.push_back(id);
    }

    // --- Request loads for desired cells that are not yet tracked ---
    RequestCells(desired);

    // --- Evict cells that went out of range ---
    EvictCells(toEvict);
}

// ===========================================================================
// Query API
// ===========================================================================

CellState WorldStreamingManager::GetCellState(uint32_t id) const
{
    const auto it = m_cellStates.find(id);
    return (it != m_cellStates.end()) ? it->second : CellState::Unloaded;
}

int WorldStreamingManager::LoadedCellCount() const
{
    return static_cast<int>(m_loadedCells.size());
}

int WorldStreamingManager::LoadingCellCount() const
{
    int count = 0;
    for (const auto& [id, state] : m_cellStates)
    {
        if (state == CellState::Loading)
            ++count;
    }
    return count;
}

// ===========================================================================
// Streaming hooks (virtual — override in subclasses for real work)
// ===========================================================================

bool WorldStreamingManager::OnLoadCell(uint32_t id, CellCoord coord)
{
    // TEACHING NOTE — Stub implementation
    // ─────────────────────────────────────
    // In the full M7 implementation this will:
    //   1. Determine the cooked .level file path from the cell ID.
    //   2. Call AssetLoader::LoadRaw(guid) to read the file bytes.
    //   3. Deserialise the entity/tile data.
    //   4. Return true on success.
    //
    // For now, log and return true so the test harness can exercise the
    // state machine without real I/O.
    LOG_INFO("WorldStreamingManager: OnLoadCell stub — cell "
             << coord.cx << "," << coord.cz
             << " (id=" << id << ") [TODO: real I/O]");
    return true;
}

void WorldStreamingManager::OnCellLoaded(uint32_t id, CellCoord coord, bool success)
{
    // TEACHING NOTE — Stub implementation
    // ─────────────────────────────────────
    // Full M7 implementation will call:
    //   zone.SpawnEnemies(world);
    //   zone.SpawnNPCs(world);
    if (success)
    {
        LOG_INFO("WorldStreamingManager: OnCellLoaded — cell "
                 << coord.cx << "," << coord.cz
                 << " (id=" << id << ") [TODO: spawn entities]");
        m_cellStates[id] = CellState::Loaded;
        m_loadedCells.insert(id);
    }
    else
    {
        LOG_ERROR("WorldStreamingManager: OnCellLoaded — cell "
                  << coord.cx << "," << coord.cz
                  << " FAILED to load.");
        m_cellStates[id] = CellState::Unloaded;
    }
}

void WorldStreamingManager::OnEvictCell(uint32_t id, CellCoord coord)
{
    // TEACHING NOTE — Stub implementation
    // ─────────────────────────────────────
    // Full M7 implementation will call:
    //   zone.Unload(world);
    LOG_INFO("WorldStreamingManager: OnEvictCell — cell "
             << coord.cx << "," << coord.cz
             << " (id=" << id << ") [TODO: destroy entities]");
}

// ===========================================================================
// Internal helpers
// ===========================================================================

void WorldStreamingManager::RequestCells(const std::vector<CellCoord>& coords)
{
    for (const CellCoord& coord : coords)
    {
        const uint32_t id = CellIdFromCoord(coord);

        // Skip if the cell is already tracked (Loading / Loaded / Evicting).
        const auto it = m_cellStates.find(id);
        if (it != m_cellStates.end() && it->second != CellState::Unloaded)
            continue;

        // Transition: Unloaded → Loading.
        m_cellStates[id] = CellState::Loading;

        // TEACHING NOTE — Capturing by value in the lambda
        // ──────────────────────────────────────────────────
        // The lambda captures id and coord by VALUE (not by reference) because
        // these stack variables will be gone by the time the worker thread
        // executes the job.  Capturing by reference would be a dangling-reference
        // bug — one of the most common threading mistakes in C++.

        LoadJob job;
        job.cellId = id;
        job.label  = "cell_" + std::to_string(coord.cx) + "_" + std::to_string(coord.cz);

        // Work lambda: runs on the worker thread.
        job.work = [this, id, coord]() -> bool
        {
            return OnLoadCell(id, coord);
        };

        // Completion lambda: runs on the main thread via PumpMainThreadCompletions.
        job.onComplete = [this, id, coord](bool success)
        {
            OnCellLoaded(id, coord, success);
        };

        m_loader.EnqueueJob(std::move(job));

        LOG_INFO("WorldStreamingManager: requested load for cell "
                 << coord.cx << "," << coord.cz);
    }
}

void WorldStreamingManager::EvictCells(const std::vector<uint32_t>& cellIds)
{
    for (const uint32_t id : cellIds)
    {
        const auto it = m_cellStates.find(id);
        if (it == m_cellStates.end())
            continue;

        // TEACHING NOTE — Eviction guard
        // ────────────────────────────────
        // We only evict cells that are fully LOADED.  Evicting a LOADING cell
        // would require cancellation support in AsyncLoader (a TODO item).
        // For now, if a cell is still loading when it goes out of range, we
        // leave it to finish loading and evict it on the next Update() call.
        if (it->second != CellState::Loaded)
        {
            LOG_INFO("WorldStreamingManager: cell " << id
                     << " is not LOADED — skipping eviction (state="
                     << static_cast<int>(it->second) << ")");
            continue;
        }

        const CellCoord coord = CellCoordFromId(id);
        OnEvictCell(id, coord);

        m_cellStates.erase(it);
        m_loadedCells.erase(id);

        LOG_INFO("WorldStreamingManager: evicted cell "
                 << coord.cx << "," << coord.cz);
    }
}

} // namespace world
} // namespace engine
