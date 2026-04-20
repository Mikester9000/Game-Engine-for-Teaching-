/**
 * @file async_loader.hpp
 * @brief AsyncLoader — background worker-thread job queue for world streaming.
 *
 * ============================================================================
 * TEACHING NOTE — Why an Async Loader?
 * ============================================================================
 * Loading a world cell involves reading kilobytes–megabytes of data from disk
 * and constructing ECS entities.  If done on the main thread, the player sees
 * a frame spike (or a full loading screen) every time the camera crosses a
 * cell boundary.  Final Fantasy XV famously eliminated loading screens for
 * the open world by streaming cells asynchronously while the player runs.
 *
 * The AsyncLoader splits loading into two phases:
 *
 *   1. ENQUEUE  (main thread, cheap)
 *      Post a LoadJob to a thread-safe queue.  Returns immediately.
 *
 *   2. EXECUTE  (worker thread, expensive)
 *      The background thread picks up jobs from the queue, executes the
 *      supplied callable (e.g. Zone::Load), and places the result in a
 *      "completed" queue.
 *
 *   3. PUMP     (main thread, cheap)
 *      Once per frame, call PumpMainThreadCompletions().  This drains the
 *      completed queue and invokes each completion callback on the main
 *      thread — safe to update ECS or rendering state here.
 *
 * ─── Threading Model ────────────────────────────────────────────────────────
 *
 *  Main thread          │  Worker thread
 *  ─────────────────────┼──────────────────────────────────────
 *  EnqueueJob()         │
 *    lock m_pendingMtx  │
 *    push to m_pending  │
 *    notify m_cv        │
 *                       │  (wakes up)
 *                       │  pop from m_pending
 *                       │  execute job.work()
 *                       │  lock m_completedMtx
 *                       │  push result to m_completed
 *  PumpMainThreadCompletions()
 *    lock m_completedMtx
 *    swap m_completed → local
 *    invoke callbacks (main thread, safe for ECS writes)
 *
 * ─── Design Choice: One Worker Thread ───────────────────────────────────────
 * A single worker thread is used in this skeleton to keep the code simple and
 * teachable.  A production loader (like in FF15) uses a thread pool with
 * priority lanes (streaming IO vs. texture decompression).  Once the single-
 * thread design is understood, extending to a pool is a natural exercise.
 *
 * ─── TODO (M7 full implementation) ──────────────────────────────────────────
 *   • Replace std::mutex + condition_variable with a lock-free SPSC queue for
 *     even lower latency between EnqueueJob and worker wake-up.
 *   • Add a priority field to LoadJob (HIGH = immediate vicinity, LOW = prefetch).
 *   • Add a cancellation token so in-flight jobs for evicted cells can be
 *     discarded rather than completed and immediately destroyed.
 *   • Integrate with the AssetLoader (M2) to read cooked cell data from disk.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Platform: All (std::thread is available on Windows, Linux, macOS)
 */

#pragma once

#include <functional>       // std::function
#include <deque>            // std::deque  (replaces std::queue for frame-budget cap)
#include <thread>           // std::thread
#include <mutex>            // std::mutex
#include <condition_variable>  // std::condition_variable
#include <atomic>           // std::atomic<bool>
#include <cstdint>          // uint32_t
#include <string>           // std::string
#include <unordered_set>    // std::unordered_set — cancelled job IDs (M7.3)

namespace engine {
namespace world {

// ===========================================================================
// LoadJob
// ===========================================================================

/**
 * @brief A single unit of asynchronous work posted to the AsyncLoader.
 *
 * TEACHING NOTE — std::function as a generic callable
 * ────────────────────────────────────────────────────
 * std::function<void()> can hold any callable: a plain function pointer, a
 * lambda, or a bound member function.  Using it here means LoadJob has no
 * dependency on the concrete Zone type, keeping async_loader.hpp decoupled
 * from the game-layer Zone system.
 *
 * The caller (WorldStreamingManager) captures whatever context it needs
 * inside the lambda — e.g.:
 *
 *   job.work = [zonePtr, &world]() { zonePtr->Load(data); };
 *   job.onComplete = [zonePtr, &world](bool ok) {
 *       if (ok) zonePtr->SpawnEnemies(world);
 *   };
 */
struct LoadJob
{
    /// Unique ID of the cell being loaded (maps to WorldPartition::CellId).
    uint32_t    cellId   = 0;

    /// Human-readable label for logging / debugging.
    std::string label;

    /**
     * @brief Cancellation flag — set by CancelJob() on the main thread.
     *
     * TEACHING NOTE — std::atomic<bool> for Lock-Free Cancel
     * ────────────────────────────────────────────────────────
     * Because the worker thread reads this flag WITHOUT holding any mutex,
     * it must be an atomic.  The cancel-check is a pure read (load_acquire)
     * and the cancel-set is a pure write (store_release), so the cheapest
     * memory order is sufficient — no expensive full barriers needed.
     *
     * Using a shared_ptr lets CancelJob() find the flag from the pending
     * queue (by cell ID) and set it AFTER the job has been moved off the
     * queue and into the worker thread's local copy.
     */
    std::shared_ptr<std::atomic<bool>> cancelled =
        std::make_shared<std::atomic<bool>>(false);

    /**
     * @brief The work to perform on the worker thread.
     *
     * Must be thread-safe: do NOT touch ECS World or any non-thread-safe
     * engine state inside this callable.  Safe operations: file I/O, memory
     * allocation, pure data parsing.
     *
     * @return true on success, false on failure (e.g. file not found).
     *
     * TEACHING NOTE — Returning bool vs. throwing exceptions
     * ────────────────────────────────────────────────────────
     * We return bool rather than throw so that the worker thread never sees an
     * unhandled exception terminate the whole process.  Errors are propagated
     * via the onComplete(false) callback instead.
     */
    std::function<bool()> work;

    /**
     * @brief Completion callback invoked on the MAIN thread.
     *
     * @param success  true if work() returned true; false otherwise.
     *
     * TEACHING NOTE — Main-thread safety
     * ─────────────────────────────────────
     * All ECS writes, rendering-state changes, and audio events must happen
     * on the main thread.  By invoking onComplete from PumpMainThreadCompletions()
     * (which is called on the main thread), the callback is safe to call any
     * engine API.
     */
    std::function<void(bool success)> onComplete;
};

// ===========================================================================
// AsyncLoader
// ===========================================================================

/**
 * @brief Worker-thread job queue for background cell loading.
 *
 * Typical usage in a game loop:
 * @code
 *   AsyncLoader loader;
 *   loader.Start();
 *
 *   // Enqueue a cell load from the main thread:
 *   LoadJob job;
 *   job.cellId     = 42;
 *   job.label      = "grasslands_north";
 *   job.work       = []() { return true; };           // TODO: real I/O
 *   job.onComplete = [](bool ok) { ... };
 *   loader.EnqueueJob(std::move(job));
 *
 *   // Once per frame (main thread):
 *   loader.PumpMainThreadCompletions();
 *
 *   // On shutdown:
 *   loader.Stop();
 * @endcode
 *
 * TEACHING NOTE — RAII and Lifecycle
 * ────────────────────────────────────
 * Start()/Stop() are explicit rather than constructor/destructor because the
 * loader is often a member of WorldStreamingManager, which itself has an
 * explicit Init()/Shutdown() lifecycle.  If Start() were in the constructor,
 * the thread would spin up before the owning object finished constructing.
 */
class AsyncLoader
{
public:
    // =========================================================================
    // Lifecycle
    // =========================================================================

    AsyncLoader() = default;
    ~AsyncLoader();

    // Non-copyable, movable.
    AsyncLoader(const AsyncLoader&)            = delete;
    AsyncLoader& operator=(const AsyncLoader&) = delete;
    AsyncLoader(AsyncLoader&&)                 = default;
    AsyncLoader& operator=(AsyncLoader&&)      = default;

    /**
     * @brief Spawn the worker thread and begin processing jobs.
     *
     * Must be called before EnqueueJob().
     * Calling Start() more than once without a matching Stop() is undefined.
     */
    void Start();

    /**
     * @brief Signal the worker to finish, wait for it, then clean up.
     *
     * Any queued jobs that have not yet started are abandoned (callbacks are
     * NOT invoked for them).  In-flight jobs are allowed to finish.
     *
     * TEACHING NOTE — Graceful shutdown
     * ────────────────────────────────────
     * We set m_stop = true then notify the condition variable.  The worker
     * thread checks m_stop after each job and exits cleanly.  We then call
     * m_thread.join() to wait for it.  This avoids std::terminate() being
     * called if the thread is still running at destruction.
     */
    void Stop();

    // =========================================================================
    // Main-thread API
    // =========================================================================

    /**
     * @brief Post a load job to the background queue.
     *
     * Thread-safe: may be called from any thread, though typically called
     * from the main thread by WorldStreamingManager.
     *
     * @param job  The job to execute.  Moved into the queue.
     */
    void EnqueueJob(LoadJob job);

    /**
     * @brief Mark a queued job as cancelled so the worker skips it.
     *
     * MUST be called from the main thread.
     *
     * @param cellId  Cell ID of the job to cancel.
     *
     * TEACHING NOTE — Cancellation with Atomic Flags
     * ─────────────────────────────────────────────────
     * We cannot remove a job from the pending queue while the worker thread
     * might be holding a copy of it (the worker pops then releases the lock).
     * Instead we scan the pending deque and set the job's cancelled flag on
     * any matching job.  Because each job's cancelled flag is a
     * shared_ptr<atomic<bool>>, the worker's local copy shares the same
     * flag — writing from the main thread is visible to the worker even
     * after the job was moved out of the pending queue.
     *
     * Cancelled jobs produce NO completion callback — the cell transitions
     * directly to Unloaded in WorldStreamingManager::EvictCells().
     */
    void CancelJob(uint32_t cellId);

    /**
     * @brief Drain completed jobs and invoke their callbacks.
     *
     * MUST be called from the main thread.  Typically called once per frame
     * before the gameplay update.
     *
     * @param maxCount  Maximum callbacks to invoke this call.
     *                  0 (default) = drain all, no cap.
     *                  Positive value = frame-budget cap (M7.4).
     *
     * TEACHING NOTE — Swap-and-drain with budget cap
     * ─────────────────────────────────────────────────
     * 1. Lock m_completedMtx briefly.
     * 2. Swap m_completed into a local deque.
     * 3. Unlock m_completedMtx (no lock held during callbacks).
     * 4. Invoke up to maxCount callbacks from the front of the local deque.
     * 5. Swap any remaining (budget-limited) completions back into
     *    m_completed for the next frame.
     *
     * TEACHING NOTE — Why deque instead of queue?
     * ─────────────────────────────────────────────
     * std::deque supports efficient push_back AND iteration, letting us
     * cheaply put unconsumed completions back.  std::queue wraps deque
     * but does not expose direct iteration — we use deque directly here.
     */
    void PumpMainThreadCompletions(int maxCount = 0);

    /**
     * @brief Number of jobs currently waiting in the pending queue.
     *
     * Informational only — value may change between call and use.
     */
    int PendingCount() const;

private:
    // =========================================================================
    // Worker thread implementation
    // =========================================================================

    /**
     * @brief Entry point for the worker thread.
     *
     * TEACHING NOTE — Worker loop pattern
     * ─────────────────────────────────────
     * The worker sits in a wait loop:
     *   1. Acquire m_pendingMtx.
     *   2. Wait on m_cv until m_pending is non-empty OR m_stop is true.
     *   3. Pop one job, release the lock.
     *   4. Check cancelled flag — skip job if set (M7.3).
     *   5. Execute job.work().
     *   6. Lock m_completedMtx, push result, unlock.
     *   7. Repeat until m_stop && m_pending is empty.
     *
     * Releasing the pending lock BEFORE executing work() is important:
     * it lets the main thread enqueue more jobs while the worker is busy.
     */
    void WorkerLoop();

    // =========================================================================
    // Member data
    // =========================================================================

    std::thread             m_thread;           ///< The single worker thread.
    std::atomic<bool>       m_stop{ false };    ///< Signals worker to exit.

    // Pending (main → worker)
    mutable std::mutex      m_pendingMtx;
    std::condition_variable m_cv;
    std::deque<LoadJob>     m_pending;          ///< Changed to deque for CancelJob scan.

    // Completed (worker → main)
    mutable std::mutex      m_completedMtx;

    /// Completed jobs carry a result flag alongside the original job data.
    struct CompletedJob
    {
        LoadJob job;
        bool    success = false;
    };
    std::deque<CompletedJob> m_completed;       ///< deque supports swap-remainder-back.
};

} // namespace world
} // namespace engine
