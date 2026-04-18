/**
 * @file Logger.hpp
 * @brief Thread-safe singleton logger for the educational game engine.
 *
 * ----------------------------------------------------------------------------
 * TEACHING NOTE — What is a Logger?
 * ----------------------------------------------------------------------------
 * A logger is a diagnostic tool that records messages about what the program
 * is doing at runtime.  Good logging is invaluable for:
 *
 *  • Debugging: "Why did the enemy not spawn?" → check the log.
 *  • Performance profiling: log frame times, asset loads.
 *  • Crash reports: the last few log lines before a crash tell you where
 *    to look.
 *  • Education: students can add log calls to understand execution flow.
 *
 * This logger writes to BOTH the console (for immediate feedback during
 * development) and a file (for post-mortem analysis after crashes).
 *
 * ----------------------------------------------------------------------------
 * TEACHING NOTE — The Singleton Pattern
 * ----------------------------------------------------------------------------
 * A Singleton ensures a class has EXACTLY ONE instance, accessible globally.
 *
 * Implementation:
 *   1. Make constructors private/deleted so callers cannot create instances.
 *   2. Provide a static `Instance()` method that creates the one instance
 *      the first time it is called, then returns it every time after.
 *
 * Trade-offs:
 *   Pro : Easy global access without passing objects around everywhere.
 *   Con : Hard to unit-test (global state), potential initialisation order
 *         issues across translation units.
 *
 * The Meyers Singleton (static local variable inside Instance()) is the
 * preferred C++11 way because:
 *   • Construction is thread-safe (C++11 guarantees it).
 *   • Lifetime extends to end-of-program (static storage duration).
 *
 * ----------------------------------------------------------------------------
 * TEACHING NOTE — Thread Safety
 * ----------------------------------------------------------------------------
 * Games often run subsystems on multiple threads (rendering, audio, physics).
 * If two threads write to the same file or console simultaneously, output
 * interleaves and becomes garbled, OR (worse) causes undefined behaviour.
 *
 * Solution: use a std::mutex.  Only one thread can hold a mutex lock at a
 * time.  Any thread that tries to lock a held mutex waits (blocks) until the
 * lock is released.  This serialises access to shared resources.
 *
 *   std::lock_guard<std::mutex> lock(m_mutex);
 *   // Only ONE thread runs here at a time.
 *
 * std::lock_guard is a RAII wrapper: it locks on construction, unlocks on
 * destruction (when it goes out of scope).  Even if an exception is thrown,
 * the mutex is correctly unlocked.  This is the safest pattern to use.
 *
 * ----------------------------------------------------------------------------
 * TEACHING NOTE — Macros
 * ----------------------------------------------------------------------------
 * The LOG_DEBUG / LOG_INFO / LOG_WARN / LOG_ERROR / LOG_CRITICAL macros at
 * the bottom of this file:
 *
 *  1. Prepend file name and line number automatically via __FILE__ / __LINE__.
 *  2. Allow calls like:  LOG_INFO("Player spawned at " << x << ", " << y);
 *     using C++ stream syntax, which composes strings without explicit
 *     std::to_string calls.
 *  3. Can be compiled away in release builds (define NDEBUG to strip
 *     LOG_DEBUG calls completely, saving performance).
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 */

#pragma once

// ---------------------------------------------------------------------------
// Includes
// ---------------------------------------------------------------------------
#include <string>       // std::string — for log level names and messages.
#include <fstream>      // std::ofstream — writing log lines to a file.
#include <iostream>     // std::cout / std::cerr — console output.
#include <sstream>      // std::ostringstream — building log lines safely.
#include <mutex>        // std::mutex, std::lock_guard — thread safety.
#include <chrono>       // std::chrono — high-resolution timestamps.
#include <ctime>        // std::localtime, std::strftime — human timestamps.
#include <iomanip>      // std::put_time — timestamp formatting.
#include <array>        // std::array — level name lookup table.


// ===========================================================================
// Log Level Enumeration
// ===========================================================================

/**
 * @enum LogLevel
 * @brief Severity levels for log messages, ordered from least to most severe.
 *
 * TEACHING NOTE — Log Level Filtering
 * ──────────────────────────────────────
 * Setting a minimum level lets you control verbosity at runtime:
 *
 *   • In development set level to DEBUG to see everything.
 *   • In release set level to WARNING to only see important events.
 *   • After a bug report set level to INFO to understand the user's session.
 *
 * Any message with a level BELOW the minimum is silently discarded.
 *
 * TEACHING NOTE — Why ERR and not ERROR?
 * ─────────────────────────────────────────
 * The Windows SDK header <wingdi.h> (included transitively through <windows.h>,
 * <d3d11.h>, and <xaudio2.h>) contains:
 *
 *   #define ERROR  0
 *
 * This is a text-substitution macro.  If the enum value were named ERROR, the
 * preprocessor would replace it before the compiler sees the declaration,
 * producing the illegal expression  0 = 3  → MSVC error C2143.
 *
 * The idiomatic fix is to choose a name the Windows headers do not define.
 * ERR is safe.  Callers continue to use the LOG_ERROR() convenience macro —
 * the rename is invisible at call-sites.
 */
enum class LogLevel : uint8_t {
    DEBUG    = 0,  ///< Detailed development info — very verbose.
    INFO     = 1,  ///< Normal operational events (entity spawned, etc.).
    WARNING  = 2,  ///< Recoverable problems that shouldn't happen normally.
    ERR      = 3,  ///< Serious problems that will affect gameplay.
    CRITICAL = 4   ///< Fatal errors — engine cannot continue.
};


// ===========================================================================
// Logger class declaration
// ===========================================================================

/**
 * @class Logger
 * @brief Thread-safe singleton logger with file and console output.
 *
 * Usage:
 * @code
 *   // Initialise once at startup:
 *   Logger::Instance().Init("game_engine.log", LogLevel::DEBUG);
 *
 *   // Log anywhere (thread-safe):
 *   Logger::Instance().Log(LogLevel::INFO, "Player moved", __FILE__, __LINE__);
 *
 *   // Or use the convenience macros (preferred):
 *   LOG_INFO("Player health: " << hp);
 *   LOG_WARN("Asset not found: " << assetName);
 *   LOG_ERROR("Null entity ID");
 * @endcode
 */
class Logger {
public:
    // -----------------------------------------------------------------------
    // Singleton access
    // -----------------------------------------------------------------------

    /**
     * @brief Return the one and only Logger instance (Meyers Singleton).
     *
     * TEACHING NOTE — Meyers Singleton
     * ───────────────────────────────────
     * The `static` keyword on a local variable gives it *static storage
     * duration*: it is constructed the first time this function is called,
     * lives until the program ends, and construction is thread-safe in C++11.
     *
     * This avoids the classic double-checked locking anti-pattern and is
     * simpler and safer.
     */
    static Logger& Instance() {
        static Logger instance; // Constructed once; never destroyed before main() returns.
        return instance;
    }

    // -----------------------------------------------------------------------
    // Copy / move — explicitly deleted to enforce singleton semantics.
    // TEACHING NOTE: Deleting these prevents accidental copies via
    //   Logger copy = Logger::Instance();  // compile error!
    // -----------------------------------------------------------------------
    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&)                 = delete;
    Logger& operator=(Logger&&)      = delete;

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    /**
     * @brief Initialise the logger: open the log file and set minimum level.
     *
     * Call this once, early in main() before any other subsystem starts.
     *
     * @param filename     Path to the log file (created/overwritten each run).
     * @param minLevel     Minimum severity to record (default: DEBUG = log all).
     * @param echoConsole  If true, also print to stdout/stderr.
     */
    void Init(const std::string& filename    = "game_engine.log",
              LogLevel           minLevel    = LogLevel::DEBUG,
              bool               echoConsole = true);

    /**
     * @brief Flush and close the log file cleanly.
     *
     * Called automatically in the destructor, but can be called explicitly
     * before an expected crash or shutdown to ensure all lines are flushed.
     */
    void Shutdown();

    // -----------------------------------------------------------------------
    // Core logging method
    // -----------------------------------------------------------------------

    /**
     * @brief Write a log message at the given severity level.
     *
     * This is the "hot path" that all macro calls reach.  It:
     *   1. Checks whether the message exceeds the minimum level.
     *   2. Builds a formatted line (timestamp + level + location + message).
     *   3. Acquires the mutex and writes to file and/or console.
     *
     * @param level    Severity of this message.
     * @param message  The text to log (already composed by the caller).
     * @param file     Source file name (__FILE__).
     * @param line     Source line number (__LINE__).
     */
    void Log(LogLevel           level,
             const std::string& message,
             const char*        file = nullptr,
             int                line = 0);

    // -----------------------------------------------------------------------
    // Configuration accessors
    // -----------------------------------------------------------------------

    /**
     * @brief Change the minimum log level at runtime.
     *
     * Useful for toggling verbosity without restarting the engine during
     * developer sessions.
     */
    void SetMinLevel(LogLevel level);

    /// Return the current minimum log level.
    LogLevel GetMinLevel() const;

    /**
     * @brief Enable or disable echoing log output to the console.
     * @param enabled  true to echo; false to write file only.
     */
    void SetConsoleOutput(bool enabled);

    /// True if the logger has been successfully initialised.
    bool IsInitialised() const;

private:
    // -----------------------------------------------------------------------
    // Private constructor (Singleton: only Instance() creates us)
    // -----------------------------------------------------------------------
    Logger();
    ~Logger();

    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Return the current wall-clock time formatted as a string.
     *
     * TEACHING NOTE — std::chrono vs std::time
     * ──────────────────────────────────────────
     * std::chrono (C++11) provides high-resolution clocks.
     * std::time / std::localtime gives calendar time (year/month/day/hour…).
     * We use both: chrono for millisecond precision, ctime for human-readable
     * calendar strings.
     *
     * @return Formatted string like "2024-03-15 14:35:07.123".
     */
    std::string GetTimestamp() const;

    /**
     * @brief Return the ASCII name of a LogLevel (e.g. "WARNING").
     * @param level  The log level.
     * @return  Null-terminated C-string constant.
     */
    static const char* LevelName(LogLevel level);

    /**
     * @brief Return the ANSI terminal colour escape code for a level.
     *
     * TEACHING NOTE — ANSI Escape Codes
     * ────────────────────────────────────
     * Most Unix/Linux terminals support ANSI escape sequences embedded in
     * strings to set text colour.  Format: "\033[<code>m"
     *
     *   "\033[0m"  — reset to default.
     *   "\033[32m" — set foreground green.
     *   "\033[31m" — set foreground red.
     *
     * Windows cmd.exe traditionally does NOT support ANSI codes, but Windows
     * Terminal and VS Code's terminal do.  We guard console colour behind
     * a runtime flag.
     */
    static const char* LevelColour(LogLevel level);

    // -----------------------------------------------------------------------
    // Member data
    // -----------------------------------------------------------------------

    mutable std::mutex m_mutex;      ///< Guards ALL access to shared state.
    std::ofstream      m_file;       ///< Output stream to the log file.
    LogLevel           m_minLevel;   ///< Drop messages below this level.
    bool               m_console;    ///< Echo to stdout/stderr?
    bool               m_initialised;///< Has Init() been called?
};


// ===========================================================================
// Convenience Macros
// ===========================================================================

/**
 * @defgroup LogMacros Logging Macros
 * @{
 *
 * TEACHING NOTE — Why Use Macros Here?
 * ──────────────────────────────────────
 * Normally macros are discouraged in modern C++ because they ignore scoping
 * rules and can cause subtle bugs.  Logging macros are one of the few
 * legitimate remaining uses because:
 *
 *  1. __FILE__ and __LINE__ are predefined preprocessor macros that expand
 *     to the current source file name and line number at the point of the
 *     macro call — impossible to replicate with a regular function call
 *     (unless using std::source_location in C++20, which not all compilers
 *     support yet).
 *
 *  2. The `do { … } while(false)` idiom wraps multiple statements in a single
 *     statement, making the macro safe inside if-else without braces:
 *
 *       if (x) LOG_INFO("x"); else LOG_INFO("no x");  // works correctly
 *
 *     Without the do-while a multi-line macro would break.
 *
 *  3. The std::ostringstream stream lets callers use << to compose messages
 *     without building temporary std::string objects manually:
 *
 *       LOG_INFO("Player at (" << x << ", " << y << ")");
 *
 * NDEBUG stripping:
 *   Wrapping LOG_DEBUG in #ifndef NDEBUG causes the compiler to generate
 *   ZERO instructions for debug logs in release builds — no performance cost.
 */

/// Log at DEBUG level — stripped in release builds when NDEBUG is defined.
#ifndef NDEBUG
#  define LOG_DEBUG(msg)                                                      \
    do {                                                                      \
        std::ostringstream _oss;                                              \
        _oss << msg;                                                          \
        Logger::Instance().Log(LogLevel::DEBUG, _oss.str(), __FILE__, __LINE__); \
    } while(false)
#else
#  define LOG_DEBUG(msg) do {} while(false)
#endif

/// Log at INFO level.
#define LOG_INFO(msg)                                                         \
    do {                                                                      \
        std::ostringstream _oss;                                              \
        _oss << msg;                                                          \
        Logger::Instance().Log(LogLevel::INFO, _oss.str(), __FILE__, __LINE__); \
    } while(false)

/// Log at WARNING level.
#define LOG_WARN(msg)                                                         \
    do {                                                                      \
        std::ostringstream _oss;                                              \
        _oss << msg;                                                          \
        Logger::Instance().Log(LogLevel::WARNING, _oss.str(), __FILE__, __LINE__); \
    } while(false)

/// Log at ERROR level.
#define LOG_ERROR(msg)                                                        \
    do {                                                                      \
        std::ostringstream _oss;                                              \
        _oss << msg;                                                          \
        Logger::Instance().Log(LogLevel::ERR, _oss.str(), __FILE__, __LINE__); \
    } while(false)

/// Log at CRITICAL level — usually precedes shutdown / abort.
#define LOG_CRITICAL(msg)                                                     \
    do {                                                                      \
        std::ostringstream _oss;                                              \
        _oss << msg;                                                          \
        Logger::Instance().Log(LogLevel::CRITICAL, _oss.str(), __FILE__, __LINE__); \
    } while(false)

/** @} */ // end of LogMacros
