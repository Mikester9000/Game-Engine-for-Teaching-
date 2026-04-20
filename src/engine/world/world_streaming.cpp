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
 * WorldStreamingManager is deliberately thin: it delegates geometry queries
 * to WorldPartition and I/O to AsyncLoader.  The real game logic lives in
 * the game-layer GameStreamingManager subclass which overrides the virtual
 * hooks (OnLoadCell / OnCellLoaded / OnEvictCell) to call Zone::Load etc.
 *
 * M7.3: EvictCells now cancels LOADING cells via AsyncLoader::CancelJob().
 * M7.4: Update passes m_maxCompletionsPerFrame to PumpMainThreadCompletions().
 * M7.5: DrawDebugOverlay (BUILD_EDITOR only) draws a cell-state minimap.
 *
 * ============================================================================
 */

#include "engine/world/world_streaming.hpp"
#include "engine/core/Logger.hpp"    // LOG_INFO, LOG_WARN, LOG_ERROR

#ifdef BUILD_EDITOR
#include <imgui.h>   // ImDrawList, ImVec2, IM_COL32
#endif

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

bool WorldStreamingManager::Init(float cellSize, int streamRadius, World* world)
{
    if (m_initialised)
    {
        LOG_WARN("WorldStreamingManager::Init: already initialised.");
        return true;
    }

    // Store the optional ECS World pointer for subclass hooks.
    m_world = world;

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

    // --- M7.4: Pump completions with frame-budget cap ---
    // TEACHING NOTE — Frame budget cap
    // ─────────────────────────────────
    // PumpMainThreadCompletions(m_maxCompletionsPerFrame) stops draining
    // after m_maxCompletionsPerFrame callbacks.  Remaining completions stay
    // in the AsyncLoader's queue and are processed in the next Update().
    // This prevents a spike when many cells complete simultaneously.
    m_loader.PumpMainThreadCompletions(m_maxCompletionsPerFrame);

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

    // --- Also evict LOADING cells that are no longer desired (M7.3) ---
    for (const auto& [id, state] : m_cellStates)
    {
        if (state == CellState::Loading &&
            desiredIds.find(id) == desiredIds.end())
        {
            toEvict.push_back(id);
        }
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
    // The base-class stub returns true immediately so the state machine can
    // be exercised in tests without real I/O.
    //
    // GameStreamingManager (game/world/GameStreamingManager.hpp) overrides
    // this to:
    //   1. Call AssetLoader::LoadRaw(cellGuid) to read the .level file bytes.
    //   2. Deserialise into a CellData struct.
    //   3. Return true on success.
    LOG_INFO("WorldStreamingManager: OnLoadCell stub — cell "
             << coord.cx << "," << coord.cz
             << " (id=" << id << ")");
    return true;
}

void WorldStreamingManager::OnCellLoaded(uint32_t id, CellCoord coord, bool success)
{
    // TEACHING NOTE — Stub implementation
    // ─────────────────────────────────────
    // GameStreamingManager overrides this to call:
    //   zone.SpawnEnemies(world);
    //   zone.SpawnNPCs(world);
    if (success)
    {
        LOG_INFO("WorldStreamingManager: OnCellLoaded — cell "
                 << coord.cx << "," << coord.cz
                 << " (id=" << id << ")");
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
    // GameStreamingManager overrides this to call:
    //   zone.Unload(world);
    LOG_INFO("WorldStreamingManager: OnEvictCell — cell "
             << coord.cx << "," << coord.cz
             << " (id=" << id << ")");
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

        if (it->second == CellState::Loading)
        {
            // TEACHING NOTE — M7.3: Cancel in-flight loads
            // ──────────────────────────────────────────────
            // When a cell moves out of range while still loading, call
            // CancelJob() to set its cancellation flag.  The worker will
            // skip executing the job (or discard its result if already
            // running).  We immediately mark the cell as Unloaded here —
            // no completion callback will fire for this cell.
            m_loader.CancelJob(id);
            m_cellStates.erase(it);

            LOG_INFO("WorldStreamingManager: cancelled in-flight load for cell "
                     << id);
            continue;
        }

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

// ===========================================================================
// Debug overlay (M7.5 — editor only)
// ===========================================================================

#ifdef BUILD_EDITOR
void WorldStreamingManager::DrawDebugOverlay(ImDrawList*                   drawList,
                                              float                         originX,
                                              float                         originY,
                                              float                         cellPxSize,
                                              const engine::math::Vec3&     cameraPos) const
{
    // TEACHING NOTE — Streaming Debug Minimap
    // ─────────────────────────────────────────
    // This overlay draws a 2D grid of cells as coloured rectangles, where
    // the colour encodes the CellState.  It is useful during development
    // to verify that the streaming radius is correct and that cells
    // transition through Unloaded → Loading → Loaded → Evicting as expected.
    //
    // Grid layout:
    //   diameter  = 2 * streamRadius + 1  (e.g. radius=1 → 3×3 grid)
    //   top-left  = origin (parameter)
    //   camera cell is drawn with an additional white outline border
    //
    // Colour legend (ABGR format for IM_COL32):
    //   Grey   (#808080) = UNLOADED  — no data in memory
    //   Yellow (#FFFF00) = LOADING   — worker thread is processing this cell
    //   Green  (#00C040) = LOADED    — entities are spawned and active
    //   Red    (#FF4040) = EVICTING  — entities being destroyed

    if (!drawList)
        return;

    const int   radius   = m_partition.StreamRadius();
    const int   diameter = 2 * radius + 1;
    const float pad      = 2.0f;  // gap between cell rectangles in pixels

    // Find which cell the camera is currently in.
    const CellCoord camCell = m_partition.WorldToCell(cameraPos);

    for (int dz = -radius; dz <= radius; ++dz)
    {
        for (int dx = -radius; dx <= radius; ++dx)
        {
            // Grid column/row in screen space (top-left = 0).
            const int col = dx + radius;
            const int row = dz + radius;

            const float x0 = originX + col * (cellPxSize + pad);
            const float y0 = originY + row * (cellPxSize + pad);
            const float x1 = x0 + cellPxSize;
            const float y1 = y0 + cellPxSize;

            // Determine cell ID and its current state.
            CellCoord coord;
            coord.cx = camCell.cx + dx;
            coord.cz = camCell.cz + dz;
            const uint32_t id    = CellIdFromCoord(coord);
            const CellState state = GetCellState(id);

            // Choose fill colour based on state.
            ImU32 fillColor;
            switch (state)
            {
                case CellState::Unloaded: fillColor = IM_COL32(128, 128, 128, 180); break;
                case CellState::Loading:  fillColor = IM_COL32(255, 220,   0, 200); break;
                case CellState::Loaded:   fillColor = IM_COL32(  0, 192,  64, 200); break;
                case CellState::Evicting: fillColor = IM_COL32(255,  64,  64, 200); break;
                default:                  fillColor = IM_COL32(128, 128, 128, 180); break;
            }

            drawList->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), fillColor, 2.0f);

            // White border outline for the camera's own cell.
            if (dx == 0 && dz == 0)
            {
                drawList->AddRect(ImVec2(x0 - 1.f, y0 - 1.f),
                                  ImVec2(x1 + 1.f, y1 + 1.f),
                                  IM_COL32(255, 255, 255, 255), 2.0f, 0, 2.0f);
            }

            // Cell coordinate label (small text, only if cell is large enough).
            if (cellPxSize >= 24.0f)
            {
                char label[32];
                std::snprintf(label, sizeof(label), "%d,%d", coord.cx, coord.cz);
                drawList->AddText(ImVec2(x0 + 3.f, y0 + 3.f),
                                  IM_COL32(255, 255, 255, 200),
                                  label);
            }
        }
    }

    // Legend — drawn to the right of the grid.
    (void)diameter;  // suppress unused-variable warning
    const float legendX = originX + diameter * (cellPxSize + pad) + 8.0f;
    float legendY = originY;
    const float swatchSize = 12.0f;
    const float lineH      = 16.0f;

    auto drawLegendEntry = [&](ImU32 color, const char* label)
    {
        drawList->AddRectFilled(ImVec2(legendX, legendY),
                                ImVec2(legendX + swatchSize, legendY + swatchSize),
                                color, 1.0f);
        drawList->AddText(ImVec2(legendX + swatchSize + 4.f, legendY), IM_COL32(220,220,220,255), label);
        legendY += lineH;
    };

    drawLegendEntry(IM_COL32(128, 128, 128, 200), "Unloaded");
    drawLegendEntry(IM_COL32(255, 220,   0, 200), "Loading");
    drawLegendEntry(IM_COL32(  0, 192,  64, 200), "Loaded");
    drawLegendEntry(IM_COL32(255,  64,  64, 200), "Evicting");
}
#endif // BUILD_EDITOR

} // namespace world
} // namespace engine
