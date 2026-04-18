/**
 * @file Logger.cpp
 * @brief Implementation of the thread-safe singleton Logger.
 *
 * TEACHING NOTE — Separation of Declaration and Definition
 * ──────────────────────────────────────────────────────────
 * In C++ a *declaration* (in the .hpp) tells the compiler the *shape* of a
 * class: what members and methods exist.  A *definition* (in the .cpp)
 * provides the *body* of each method.
 *
 * Why split?
 *  • Faster compilation: other .cpp files include Logger.hpp and see only
 *    declarations.  They do NOT re-compile the function bodies every time.
 *  • Encapsulation: users of Logger.hpp see the public interface; the
 *    private implementation details are hidden in Logger.cpp.
 *  • Link-time coherence: the linker combines all compiled .cpp files into
 *    the final executable.  Each function body appears in exactly ONE .cpp.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 */

#include "Logger.hpp"

// ---------------------------------------------------------------------------
// Additional includes needed only in the implementation
// ---------------------------------------------------------------------------
#include <algorithm>   // std::min — used when extracting the base file name.

// ---------------------------------------------------------------------------
// Anonymous namespace — internal helper, not visible outside this .cpp.
//
// TEACHING NOTE — Anonymous Namespaces
// ───────────────────────────────────────
// Wrapping internal-only helpers in `namespace { … }` gives them *internal
// linkage*: the linker treats them as if they were declared `static`, meaning
// they are invisible to other translation units.  This prevents accidental
// name clashes when two .cpp files each define a local helper with the same
// name.
// ---------------------------------------------------------------------------
namespace {

/**
 * @brief Extract the file's base name from a full path.
 *
 * __FILE__ expands to the full path like
 *   "/home/dev/project/src/engine/core/Logger.cpp"
 * For concise log output we want only "Logger.cpp".
 *
 * We search backward for '/' or '\\' and return the pointer past it.
 *
 * @param path  Pointer to the null-terminated path string.
 * @return      Pointer to the start of the base name within path.
 */
const char* BaseName(const char* path) {
    if (!path) return "?";
    const char* last = path;
    for (const char* p = path; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') {
            last = p + 1;  // Point to character after the separator.
        }
    }
    return last;
}

} // anonymous namespace


// ===========================================================================
// Logger — private constructor
// ===========================================================================

/**
 * @brief Default-construct the Logger in an uninitialised state.
 *
 * TEACHING NOTE — Constructor Initialiser Lists
 * ───────────────────────────────────────────────
 * Member variables should be initialised in the *member initialiser list*
 * (the `: m_minLevel(…), m_console(…)` part) rather than assigned inside
 * the constructor body.  The initialiser list:
 *
 *  1. Constructs each member directly — no "default construct then assign".
 *  2. Is the ONLY way to initialise const members and members without
 *     default constructors.
 *  3. Runs before the constructor body, so the body can safely use members.
 */
Logger::Logger()
    : m_minLevel(LogLevel::DEBUG)
    , m_console(true)
    , m_initialised(false)
{
    // Body intentionally empty — Init() performs actual setup.
}


// ===========================================================================
// Logger — destructor
// ===========================================================================

/**
 * @brief Flush and close the log file when the Logger is destroyed.
 *
 * TEACHING NOTE — RAII (Resource Acquisition Is Initialisation)
 * ───────────────────────────────────────────────────────────────
 * The destructor is the natural place to release resources (close files,
 * free memory, unlock mutexes).  By putting cleanup in the destructor we
 * guarantee it runs even if the program exits via an exception, because
 * C++ calls destructors for all objects with static storage duration at
 * programme exit.  This is the cornerstone of the RAII idiom.
 */
Logger::~Logger() {
    Shutdown();
}


// ===========================================================================
// Logger::Init
// ===========================================================================

void Logger::Init(const std::string& filename,
                  LogLevel           minLevel,
                  bool               echoConsole)
{
    // Lock the mutex before touching shared state.
    std::lock_guard<std::mutex> lock(m_mutex);

    m_minLevel    = minLevel;
    m_console     = echoConsole;
    m_initialised = true;

    // Open the log file.
    // std::ios::out   — write mode.
    // std::ios::trunc — truncate (overwrite) any existing file.
    // Use std::ios::app instead of trunc to *append* across sessions.
    m_file.open(filename, std::ios::out | std::ios::trunc);

    if (!m_file.is_open()) {
        // Cannot use LOG_* macros here (we're inside the logger itself).
        std::cerr << "[Logger] WARNING: Could not open log file: "
                  << filename << std::endl;
    }

    // Write a header line so the log file is clearly identifiable.
    const std::string header =
        "=================================================================\n"
        "  Educational Game Engine — Log File\n"
        "  Started: " + GetTimestamp() + "\n"
        "=================================================================\n";

    if (m_file.is_open()) {
        m_file << header;
        m_file.flush();  // Force the OS to write the buffer to disk now.
    }
    if (m_console) {
        std::cout << header;
    }
}


// ===========================================================================
// Logger::Shutdown
// ===========================================================================

void Logger::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_file.is_open()) {
        // Write a footer and flush before closing.
        const std::string footer =
            "=================================================================\n"
            "  Log closed: " + GetTimestamp() + "\n"
            "=================================================================\n";
        m_file << footer;
        m_file.flush();
        m_file.close();
    }
    m_initialised = false;
}


// ===========================================================================
// Logger::Log  (the core method)
// ===========================================================================

/**
 * @brief Format and emit a log message.
 *
 * TEACHING NOTE — String Streams (std::ostringstream)
 * ──────────────────────────────────────────────────────
 * std::ostringstream is an in-memory string buffer that supports the same
 * << insertion operators as std::cout.  We build the complete log line in
 * memory before writing it, which minimises the time the mutex is held.
 *
 * Log line format:
 *   [2024-03-15 14:35:07.123] [INFO    ] [Logger.cpp:42] Your message here
 *    ^^^^^^^^^^^^^^^^^^^^^^    ^^^^^^^^^   ^^^^^^^^^^^^^
 *    timestamp                 level       source location
 */
void Logger::Log(LogLevel           level,
                 const std::string& message,
                 const char*        file,
                 int                line)
{
    // -----------------------------------------------------------------------
    // 1. Filter — drop messages below the minimum configured level.
    //    This happens BEFORE locking the mutex to avoid contention overhead
    //    for discarded messages.
    // -----------------------------------------------------------------------
    if (static_cast<uint8_t>(level) < static_cast<uint8_t>(m_minLevel)) {
        return;
    }

    // -----------------------------------------------------------------------
    // 2. Build the log line.
    //    We build it outside the lock to minimise lock hold time.
    // -----------------------------------------------------------------------
    std::ostringstream line_stream;

    // Timestamp
    line_stream << "[" << GetTimestamp() << "] ";

    // Level (padded to 8 chars for alignment)
    line_stream << "[" << std::left << std::setw(8) << LevelName(level) << "] ";

    // Source location (optional — only if file pointer is non-null)
    if (file) {
        line_stream << "[" << BaseName(file) << ":" << line << "] ";
    }

    // The actual message
    line_stream << message;

    const std::string formatted = line_stream.str();

    // -----------------------------------------------------------------------
    // 3. Acquire the lock and write.
    //    std::lock_guard is RAII: unlocks automatically at end of scope.
    // -----------------------------------------------------------------------
    std::lock_guard<std::mutex> lock(m_mutex);

    // Write to file
    if (m_file.is_open()) {
        m_file << formatted << '\n';
        // Flush immediately for ERROR / CRITICAL so data is on disk even if
        // the program crashes right after.
        if (level >= LogLevel::ERR) {
            m_file.flush();
        }
    }

    // Write to console with optional colour
    if (m_console) {
        // CRITICAL and ERROR go to stderr; everything else to stdout.
        std::ostream& out = (level >= LogLevel::ERR) ? std::cerr : std::cout;

        // ANSI colour prefix
        out << LevelColour(level);
        out << formatted;
        // ANSI reset suffix
        out << "\033[0m" << '\n';
    }
}


// ===========================================================================
// Logger::SetMinLevel
// ===========================================================================

void Logger::SetMinLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_minLevel = level;
}


// ===========================================================================
// Logger::GetMinLevel
// ===========================================================================

LogLevel Logger::GetMinLevel() const {
    // TEACHING NOTE — mutable mutex
    // ──────────────────────────────
    // The mutex is declared `mutable` in the header so it can be locked even
    // from `const` member functions like this one.  Without `mutable`, the
    // compiler would reject `lock(m_mutex)` inside a const method.
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_minLevel;
}


// ===========================================================================
// Logger::SetConsoleOutput
// ===========================================================================

void Logger::SetConsoleOutput(bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_console = enabled;
}


// ===========================================================================
// Logger::IsInitialised
// ===========================================================================

bool Logger::IsInitialised() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_initialised;
}


// ===========================================================================
// Logger::GetTimestamp  (private)
// ===========================================================================

std::string Logger::GetTimestamp() const {
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Two-level time:
    //   std::chrono::system_clock  → wall-clock time with sub-second precision
    //   std::time_t / std::tm      → calendar representation (for formatting)
    // -----------------------------------------------------------------------

    // Get the current time point from the system clock.
    auto now_tp    = std::chrono::system_clock::now();

    // Convert to a time_t (seconds since Unix epoch, 1970-01-01 00:00:00 UTC).
    std::time_t now_t = std::chrono::system_clock::to_time_t(now_tp);

    // Extract the sub-second milliseconds part.
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now_tp.time_since_epoch()) % 1000;

    // Convert to local calendar time.
    // NOTE: std::localtime is NOT thread-safe on all platforms because it
    // returns a pointer to a shared static buffer.  We use it here for
    // simplicity; in production use localtime_r (POSIX) or localtime_s (MSVC).
    //
    // MSVC raises C4996 ("may be unsafe") for std::localtime; we use the
    // thread-safe localtime_s variant on that platform to keep /W4 builds clean.
#ifdef _WIN32
    std::tm  local_tm_buf{};
    localtime_s(&local_tm_buf, &now_t);
    std::tm* local_tm = &local_tm_buf;
#else
    std::tm* local_tm = std::localtime(&now_t);
#endif

    std::ostringstream oss;
    // std::put_time formats the tm struct like strftime.
    // "%Y-%m-%d %H:%M:%S" → "2024-03-15 14:35:07"
    oss << std::put_time(local_tm, "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}


// ===========================================================================
// Logger::LevelName  (private, static)
// ===========================================================================

const char* Logger::LevelName(LogLevel level) {
    // A static array of string literals, indexed by the enum's integer value.
    // TEACHING NOTE — switch vs array lookup:
    //   An array lookup is O(1) and branch-free; a switch compiles to a
    //   similar jump table.  Both are fine here.
    static const char* names[] = {
        "DEBUG",    // 0
        "INFO",     // 1
        "WARNING",  // 2
        "ERROR",    // 3
        "CRITICAL"  // 4
    };
    auto idx = static_cast<uint8_t>(level);
    if (idx < 5) return names[idx];
    return "UNKNOWN";
}


// ===========================================================================
// Logger::LevelColour  (private, static)
// ===========================================================================

const char* Logger::LevelColour(LogLevel level) {
    // ANSI colour codes:
    //   \033[0m  — reset
    //   \033[37m — white (DEBUG)
    //   \033[32m — green (INFO)
    //   \033[33m — yellow (WARNING)
    //   \033[31m — red (ERROR)
    //   \033[35m — magenta (CRITICAL)
    switch (level) {
        case LogLevel::DEBUG:    return "\033[37m";   // White
        case LogLevel::INFO:     return "\033[32m";   // Green
        case LogLevel::WARNING:  return "\033[33m";   // Yellow
        case LogLevel::ERR:      return "\033[31m";   // Red
        case LogLevel::CRITICAL: return "\033[35m";   // Magenta
        default:                 return "\033[0m";    // Reset
    }
}
