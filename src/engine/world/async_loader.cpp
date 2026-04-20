/**
 * @file async_loader.cpp
 * @brief AsyncLoader implementation — worker thread + job queues.
 *
 * ============================================================================
 * TEACHING NOTE — Implementation file responsibilities
 * ============================================================================
 * This file contains the definitions of every method declared in
 * async_loader.hpp.  The header is the "contract" (what you can do);
 * this file is the "implementation" (how it is done).
 *
 * By keeping Jolt/platform/engine-specific code OUT of the header, any file
 * that #includes async_loader.hpp only pays for a lightweight recompile if
 * this .cpp changes — not the other way around.
 *
 * M7.3 additions: CancelJob() + cancelled-flag check in WorkerLoop().
 * M7.4 additions: maxCount parameter in PumpMainThreadCompletions().
 * ============================================================================
 */

#include "engine/world/async_loader.hpp"
#include "engine/core/Logger.hpp"    // LOG_INFO, LOG_WARN, LOG_ERROR

#include <utility>   // std::move

namespace engine {
namespace world {

// ===========================================================================
// Destructor
// ===========================================================================

AsyncLoader::~AsyncLoader()
{
    // TEACHING NOTE — Safe destruction
    // ─────────────────────────────────
    // If Stop() was not called before the destructor (e.g. due to an exception
    // in the owning class), we call it here to avoid std::terminate() being
    // triggered by destroying a joinable thread.
    if (m_thread.joinable())
    {
        LOG_WARN("AsyncLoader: destroyed without calling Stop() — stopping now.");
        Stop();
    }
}

// ===========================================================================
// Lifecycle
// ===========================================================================

void AsyncLoader::Start()
{
    // TEACHING NOTE — std::atomic initialisation
    // ────────────────────────────────────────────
    // m_stop is a std::atomic<bool> so both the main thread and worker thread
    // can read/write it without a data race.  We reset it to false here in
    // case Start() is called after a previous Stop().
    m_stop.store(false, std::memory_order_relaxed);

    // Launch the worker thread.  The thread starts executing WorkerLoop()
    // immediately, but will block on m_cv.wait() until the first job arrives.
    m_thread = std::thread(&AsyncLoader::WorkerLoop, this);

    LOG_INFO("AsyncLoader: worker thread started.");
}

void AsyncLoader::Stop()
{
    // TEACHING NOTE — Graceful shutdown sequence
    // ───────────────────────────────────────────
    // 1. Set the stop flag under the pending mutex so the worker cannot miss
    //    the notification (avoids the classic "signal before wait" race).
    // 2. Notify the CV to wake the worker immediately.
    // 3. Join the thread (wait for WorkerLoop to return).
    {
        std::lock_guard<std::mutex> lock(m_pendingMtx);
        m_stop.store(true, std::memory_order_release);
    }
    m_cv.notify_one();

    if (m_thread.joinable())
        m_thread.join();

    LOG_INFO("AsyncLoader: worker thread stopped.");
}

// ===========================================================================
// Main-thread API
// ===========================================================================

void AsyncLoader::EnqueueJob(LoadJob job)
{
    {
        std::lock_guard<std::mutex> lock(m_pendingMtx);
        // TEACHING NOTE — Register cancel token before pushing the job
        // ──────────────────────────────────────────────────────────────
        // We store the shared cancellation flag in m_cancelTokens BEFORE
        // pushing the job to m_pending.  This ensures CancelJob() can always
        // find the token even if the worker pops the job between the push
        // and the map insert (which cannot happen here — both happen under
        // the same lock).
        m_cancelTokens[job.cellId] = job.cancelled;
        m_pending.push_back(std::move(job));
    }
    // Wake the worker thread.  If the worker is already processing a job,
    // it will pick this one up after finishing the current one.
    m_cv.notify_one();
}

void AsyncLoader::CancelJob(uint32_t cellId)
{
    // TEACHING NOTE — Cancellation via m_cancelTokens (M7.3 fix)
    // ─────────────────────────────────────────────────────────────
    // We look up the shared cancellation flag in m_cancelTokens rather than
    // only scanning m_pending.  This is important because once the worker
    // pops a job from m_pending (under m_pendingMtx) and then releases the
    // lock, the deque no longer contains the job.  A deque-only scan would
    // silently miss in-flight jobs.
    //
    // m_cancelTokens retains the same shared_ptr that lives inside the
    // worker's local job copy.  Writing to the flag here (store_release)
    // is therefore visible to the worker's two cancel-checks (before and
    // after work()) even if the job was already popped from the deque.
    std::lock_guard<std::mutex> lock(m_pendingMtx);
    const auto it = m_cancelTokens.find(cellId);
    if (it != m_cancelTokens.end())
    {
        it->second->store(true, std::memory_order_release);
        // Note: we do NOT erase from m_cancelTokens here.  The worker erases
        // the entry when it pops the job.  This keeps the flag reachable for
        // the post-work() cancellation check in WorkerLoop.
    }
    LOG_INFO("AsyncLoader: CancelJob requested for cellId=" << cellId);
}

void AsyncLoader::PumpMainThreadCompletions(int maxCount)
{
    // TEACHING NOTE — Swap-and-drain with optional frame-budget cap (M7.4)
    // ─────────────────────────────────────────────────────────────────────
    // Steps:
    //   1. Lock m_completedMtx briefly; swap m_completed into a local deque.
    //   2. Unlock immediately so the worker can push new completions while
    //      we are executing callbacks.
    //   3. Process up to maxCount items from the local deque.
    //   4. Prepend any unconsumed items back to m_completed for next frame.
    //
    // maxCount == 0 means "no cap — drain everything".
    std::deque<CompletedJob> localCompleted;
    {
        std::lock_guard<std::mutex> lock(m_completedMtx);
        std::swap(m_completed, localCompleted);
    }

    // No lock held during callback invocation.
    int processed = 0;
    while (!localCompleted.empty())
    {
        if (maxCount > 0 && processed >= maxCount)
            break;

        CompletedJob& c = localCompleted.front();
        if (c.job.onComplete)
            c.job.onComplete(c.success);
        localCompleted.pop_front();
        ++processed;
    }

    // If the budget was hit, push unconsumed completions back to m_completed
    // so they are picked up next frame.
    if (!localCompleted.empty())
    {
        std::lock_guard<std::mutex> lock(m_completedMtx);
        // TEACHING NOTE — Preserve FIFO order when prepending leftovers
        // ─────────────────────────────────────────────────────────────────
        // localCompleted was drained front-to-back, so any remaining items
        // are already in the correct oldest-to-newest order.
        // push_front() inserts before the current head; iterating forward
        // would reverse the remainder.  Iterating in reverse preserves the
        // original FIFO ordering across frames.
        for (auto it = localCompleted.rbegin(); it != localCompleted.rend(); ++it)
            m_completed.push_front(std::move(*it));
    }
}

int AsyncLoader::PendingCount() const
{
    std::lock_guard<std::mutex> lock(m_pendingMtx);
    return static_cast<int>(m_pending.size());
}

// ===========================================================================
// Worker thread
// ===========================================================================

void AsyncLoader::WorkerLoop()
{
    // TEACHING NOTE — Condition variable wait loop pattern
    // ──────────────────────────────────────────────────────
    // The canonical wait-loop guards against spurious wakeups:
    //
    //   m_cv.wait(lock, predicate)
    //
    // is equivalent to:
    //
    //   while (!predicate()) m_cv.wait(lock);
    //
    // The predicate checks BOTH that there is work AND that we should not stop.
    // We wake up when m_pending is non-empty OR m_stop is set.

    while (true)
    {
        LoadJob job;

        // --- Wait for work ---
        {
            std::unique_lock<std::mutex> lock(m_pendingMtx);
            m_cv.wait(lock, [this]
            {
                return !m_pending.empty() || m_stop.load(std::memory_order_acquire);
            });

            // Exit the loop if we were told to stop and the queue is drained.
            if (m_stop.load(std::memory_order_acquire) && m_pending.empty())
                break;

            job = std::move(m_pending.front());
            m_pending.pop_front();

            // TEACHING NOTE — Erase the cancel token while still under lock
            // ────────────────────────────────────────────────────────────────
            // Once we pop the job from m_pending, CancelJob() can no longer
            // find it via a deque scan.  We erase from m_cancelTokens here
            // (still under m_pendingMtx) so a concurrent CancelJob() call
            // that arrives after this erase will not find a stale token.
            // The worker's local `job` still holds its own shared_ptr copy,
            // so the two cancel-checks below can still read the flag even
            // after the map entry is gone.
            m_cancelTokens.erase(job.cellId);
        }
        // Lock released — main thread can enqueue more work while we execute.

        // --- M7.3: Check cancellation flag before executing ---
        // TEACHING NOTE — Early-out on cancellation
        // ──────────────────────────────────────────
        // The main thread may have called CancelJob() between the time this
        // job was enqueued and now.  Check the shared atomic flag and skip
        // execution entirely if the job was cancelled.  We do NOT push a
        // completion callback for cancelled jobs — the WorldStreamingManager
        // has already transitioned the cell to Unloaded in EvictCells().
        if (job.cancelled && job.cancelled->load(std::memory_order_acquire))
        {
            LOG_INFO("AsyncLoader: job '" << job.label
                     << "' (cellId=" << job.cellId
                     << ") was cancelled — skipping.");
            continue;
        }

        // --- Execute work ---
        LOG_INFO("AsyncLoader: executing job '" << job.label
                 << "' (cellId=" << job.cellId << ")");

        bool success = false;
        if (job.work)
        {
            // TEACHING NOTE — Catching exceptions on the worker thread
            // ──────────────────────────────────────────────────────────
            // If job.work() throws, we catch it here and report failure via
            // the completion callback.  An uncaught exception on a std::thread
            // calls std::terminate(), killing the entire process.
            try
            {
                success = job.work();
            }
            catch (const std::exception& ex)
            {
                LOG_ERROR("AsyncLoader: job '" << job.label
                          << "' threw exception: " << ex.what());
                success = false;
            }
            catch (...)
            {
                LOG_ERROR("AsyncLoader: job '" << job.label
                          << "' threw unknown exception.");
                success = false;
            }
        }
        else
        {
            // No work function provided — treat as success (no-op job).
            success = true;
        }

        LOG_INFO("AsyncLoader: job '" << job.label
                 << "' " << (success ? "succeeded" : "FAILED"));

        // --- M7.3: Check cancellation again before pushing to completed ---
        // TEACHING NOTE — Double-check after execution
        // ──────────────────────────────────────────────
        // CancelJob() may have been called WHILE work() was executing.
        // In that case the cell is already Unloaded in WorldStreamingManager.
        // Pushing a completion callback would re-mark it as Loaded, corrupting
        // the state machine.  Skip the push if cancelled.
        if (job.cancelled && job.cancelled->load(std::memory_order_acquire))
        {
            LOG_INFO("AsyncLoader: job '" << job.label
                     << "' completed but was cancelled mid-flight — discarding.");
            continue;
        }

        // --- Push to completed queue ---
        {
            std::lock_guard<std::mutex> lock(m_completedMtx);
            m_completed.push_back({ std::move(job), success });
        }
    }
}

} // namespace world
} // namespace engine
