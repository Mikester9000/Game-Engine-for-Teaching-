/**
 * @file Log.hpp
 * @brief Shared minimal logging utility for the Game Engine for Teaching.
 *
 * =============================================================================
 * TEACHING NOTE — Why a shared Log.hpp?
 * =============================================================================
 * Every subsystem (engine, editor, tools) needs to log messages.  Without a
 * shared logger, each module invents its own — some use printf, some use
 * std::cout, some add timestamps, some don't.  The result is inconsistent
 * output that's hard to filter, redirect, or silence in tests.
 *
 * This header provides five macros:
 *
 *   LOG_INFO(msg)    – Informational messages (white / default)
 *   LOG_WARN(msg)    – Warnings that don't prevent execution (yellow)
 *   LOG_ERROR(msg)   – Errors that indicate a problem (red)
 *   LOG_DEBUG(msg)   – Verbose debug output (grey, stripped in Release)
 *   LOG_FATAL(msg)   – Unrecoverable error — logs then std::terminate()
 *
 * All macros accept a C++ stream expression, so you can write:
 *
 *   LOG_INFO("Loaded scene: " << sceneName << " in " << ms << "ms");
 *
 * =============================================================================
 * Compile-time switches
 * =============================================================================
 *
 *   #define LOG_SILENT          – suppress all output (useful in unit tests)
 *   #define LOG_LEVEL_WARN      – suppress INFO and DEBUG
 *   #define LOG_LEVEL_ERROR     – suppress INFO, DEBUG, WARN
 *   #define LOG_NO_COLOUR       – disable ANSI colour codes (for Windows CMD)
 *
 * =============================================================================
 * Usage
 * =============================================================================
 *
 *   #include "shared/runtime/Log.hpp"
 *
 *   bool ok = LoadAsset(path);
 *   if (!ok) {
 *       LOG_ERROR("Failed to load asset: " << path);
 *       return false;
 *   }
 *   LOG_INFO("Asset loaded: " << path);
 *
 * =============================================================================
 */

#pragma once

#include <iostream>
#include <sstream>
#include <ctime>
#include <string>

// =============================================================================
// ANSI colour codes (disabled by LOG_NO_COLOUR or on non-ANSI terminals)
// =============================================================================

#if defined(LOG_NO_COLOUR) || defined(_WIN32)
#   define _LOG_COL_RESET  ""
#   define _LOG_COL_GREY   ""
#   define _LOG_COL_YELLOW ""
#   define _LOG_COL_RED    ""
#   define _LOG_COL_BOLD   ""
#else
#   define _LOG_COL_RESET  "\033[0m"
#   define _LOG_COL_GREY   "\033[90m"
#   define _LOG_COL_YELLOW "\033[33m"
#   define _LOG_COL_RED    "\033[31m"
#   define _LOG_COL_BOLD   "\033[1m"
#endif


// =============================================================================
// Timestamp helper (inline to avoid multiple-definition issues in headers)
// =============================================================================

namespace Log
{
    /** @brief Return a HH:MM:SS timestamp string for the current local time. */
    inline std::string Timestamp()
    {
        std::time_t t = std::time(nullptr);
        char buf[9];  // "HH:MM:SS\0"
        std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t));
        return std::string(buf);
    }

    /**
     * @brief Write one log line to stderr.
     *
     * @param level    Short label, e.g. "INFO", "WARN", "ERROR".
     * @param colour   ANSI escape code string (empty if colours disabled).
     * @param message  The already-formatted message string.
     * @param file     Source file (__FILE__).
     * @param line     Source line (__LINE__).
     */
    inline void Write(
        const char* level,
        const char* colour,
        const std::string& message,
        const char* file,
        int line)
    {
#ifdef LOG_SILENT
        (void)level; (void)colour; (void)message; (void)file; (void)line;
        return;
#endif
        // TEACHING NOTE — We write to std::cerr (not std::cout) so that log
        // output doesn't mix with program output when piping to files.
        std::cerr << colour
                  << "[" << Timestamp() << "]"
                  << "[" << level << "] "
                  << message
                  << _LOG_COL_GREY
                  << "  (" << file << ":" << line << ")"
                  << _LOG_COL_RESET
                  << "\n";
    }
}  // namespace Log


// =============================================================================
// Public macros
// =============================================================================

/**
 * @defgroup LogMacros Logging macros
 * @{
 */

#ifndef LOG_SILENT

/**
 * @brief Log an INFO-level message.
 *
 * TEACHING NOTE — Macro trick: the do { } while(0) wrapper lets you write
 *   LOG_INFO("x = " << x);
 * and have it behave as a single statement in all contexts (e.g. after if).
 */
#   define LOG_INFO(msg) \
        do { \
            std::ostringstream _log_ss; \
            _log_ss << msg; \
            Log::Write("INFO ", _LOG_COL_RESET, _log_ss.str(), __FILE__, __LINE__); \
        } while(0)

#   define LOG_WARN(msg) \
        do { \
            std::ostringstream _log_ss; \
            _log_ss << msg; \
            Log::Write("WARN ", _LOG_COL_YELLOW, _log_ss.str(), __FILE__, __LINE__); \
        } while(0)

#   define LOG_ERROR(msg) \
        do { \
            std::ostringstream _log_ss; \
            _log_ss << msg; \
            Log::Write("ERROR", _LOG_COL_RED, _log_ss.str(), __FILE__, __LINE__); \
        } while(0)

#   ifdef NDEBUG
        // TEACHING NOTE — In Release builds, LOG_DEBUG is a no-op at compile
        // time so it generates zero code.  Always safe to leave in.
#       define LOG_DEBUG(msg) do { } while(0)
#   else
#       define LOG_DEBUG(msg) \
            do { \
                std::ostringstream _log_ss; \
                _log_ss << msg; \
                Log::Write("DEBUG", _LOG_COL_GREY, _log_ss.str(), __FILE__, __LINE__); \
            } while(0)
#   endif

#   define LOG_FATAL(msg) \
        do { \
            std::ostringstream _log_ss; \
            _log_ss << msg; \
            Log::Write("FATAL", _LOG_COL_BOLD _LOG_COL_RED, _log_ss.str(), __FILE__, __LINE__); \
            std::terminate(); \
        } while(0)

#else  // LOG_SILENT — all macros are no-ops

#   define LOG_INFO(msg)  do { } while(0)
#   define LOG_WARN(msg)  do { } while(0)
#   define LOG_ERROR(msg) do { } while(0)
#   define LOG_DEBUG(msg) do { } while(0)
#   define LOG_FATAL(msg) do { std::terminate(); } while(0)

#endif  // LOG_SILENT

/** @} */
