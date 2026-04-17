#pragma once

// ============================================================================
// shared/runtime/Log.hpp — Lightweight logging header
// ============================================================================
//
// TEACHING NOTE — Shared Runtime Utilities
// =========================================
// This header is part of shared/runtime/ which contains C++ utilities that
// are used by BOTH the main engine (src/) and any future C++ tooling.
//
// shared/runtime/ must NOT depend on engine-specific headers (no ECS, no
// platform layer).  It only depends on the C++ standard library.
//
// Log.hpp provides four logging macros:
//   LOG_INFO(msg)    — informational message (always shown)
//   LOG_WARN(msg)    — warning (non-fatal; should be investigated)
//   LOG_ERROR(msg)   — error (operation failed; recovery attempted)
//   LOG_DEBUG(msg)   — verbose debug info (compiled out in Release)
//
// All macros write to std::cerr with a [LEVEL] prefix and the file/line.
// Replace the implementation with a real Logger (e.g. spdlog) in M3.
//
// TEACHING NOTE — Why macros instead of a function?
// Macros capture __FILE__ and __LINE__ at the call site automatically.
// A function call would capture the file/line inside the logging function,
// which is useless for debugging.  Modern logging libraries (spdlog, log4cpp)
// use the same macro trick internally.
// ============================================================================

#include <iostream>
#include <string_view>

// ---------------------------------------------------------------------------
// Strip the long absolute path prefix for cleaner output.
// ---------------------------------------------------------------------------
namespace shared::detail {
    // Returns the filename component of a full path at compile time.
    constexpr const char* basename(const char* path) noexcept {
        const char* file = path;
        while (*path) {
            if (*path == '/' || *path == '\\') file = path + 1;
            ++path;
        }
        return file;
    }
} // namespace shared::detail

// ---------------------------------------------------------------------------
// Public logging macros
// ---------------------------------------------------------------------------

/// Log an informational message.
#define LOG_INFO(msg)  \
    (std::cerr << "[INFO ] " << shared::detail::basename(__FILE__) \
               << ":" << __LINE__ << "  " << (msg) << "\n")

/// Log a warning (non-fatal; investigation recommended).
#define LOG_WARN(msg)  \
    (std::cerr << "[WARN ] " << shared::detail::basename(__FILE__) \
               << ":" << __LINE__ << "  " << (msg) << "\n")

/// Log an error (operation failed; recovery may or may not be possible).
#define LOG_ERROR(msg) \
    (std::cerr << "[ERROR] " << shared::detail::basename(__FILE__) \
               << ":" << __LINE__ << "  " << (msg) << "\n")

/// Log a debug message (compiled out in Release builds).
#ifdef NDEBUG
#  define LOG_DEBUG(msg) ((void)0)
#else
#  define LOG_DEBUG(msg) \
     (std::cerr << "[DEBUG] " << shared::detail::basename(__FILE__) \
                << ":" << __LINE__ << "  " << (msg) << "\n")
#endif
