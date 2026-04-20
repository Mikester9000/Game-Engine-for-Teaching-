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
        m_pending.push(std::move(job));
    }
    // Wake the worker thread.  If the worker is already processing a job,
    // it will pick this one up after finishing the current one.
    m_cv.notify_one();
}

void AsyncLoader::PumpMainThreadCompletions()
{
    // TEACHING NOTE — Swap-and-drain pattern (see header for explanation)
    // ─────────────────────────────────────────────────────────────────────
    std::queue<CompletedJob> localCompleted;
    {
        std::lock_guard<std::mutex> lock(m_completedMtx);
        std::swap(m_completed, localCompleted);
    }
    // No lock held during callback invocation.
    while (!localCompleted.empty())
    {
        CompletedJob& c = localCompleted.front();
        if (c.job.onComplete)
            c.job.onComplete(c.success);
        localCompleted.pop();
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
            m_pending.pop();
        }
        // Lock released — main thread can enqueue more work while we execute.

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

        // --- Push to completed queue ---
        {
            std::lock_guard<std::mutex> lock(m_completedMtx);
            m_completed.push({ std::move(job), success });
        }
    }
}

} // namespace world
} // namespace engine
