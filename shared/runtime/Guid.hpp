/**
 * @file Guid.hpp
 * @brief Shared GUID (Globally Unique Identifier) utility.
 *
 * =============================================================================
 * TEACHING NOTE — Why GUIDs?
 * =============================================================================
 * In a game engine, every asset needs a stable identity that survives:
 *   • File renames  (the editor changes the path, but the GUID stays the same)
 *   • Directory moves (same problem)
 *   • Concurrent edits by multiple developers
 *
 * A GUID (also called UUID — Universally Unique Identifier) is a 128-bit
 * number formatted as "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" (36 chars).
 * RFC 4122 Version 4 GUIDs use random bits, making collisions astronomically
 * unlikely (~5.3 × 10^36 possible values).
 *
 * Unreal Engine uses FGuid for the same purpose.  Unity uses string GUIDs
 * stored in .meta sidecar files.  We follow the same pattern here.
 *
 * =============================================================================
 * Usage:
 *   Guid id  = Guid::New();          // Generate a new random GUID
 *   Guid id2 = Guid::FromString(s);  // Parse from "xxxxxxxx-xxxx-xxxx-xxxx-xxxx"
 *   std::string s = id.ToString();   // Convert back to string
 *   bool valid = id.IsValid();        // True unless default-constructed
 *   bool eq    = (id == id2);         // Equality comparison
 *
 * =============================================================================
 */

#pragma once

#include <cstdint>
#include <string>
#include <array>
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

/**
 * @class Guid
 * @brief A 128-bit GUID stored as four uint32_t values.
 *
 * This is a header-only, dependency-free implementation — no platform API
 * calls, no external libraries.  For stronger randomness on Windows you
 * can replace Generate() with a call to CoCreateGuid() or BCryptGenRandom().
 */
class Guid
{
public:
    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    /// Default-constructs a nil (all-zero) GUID — not a valid asset ID.
    Guid() : m_data{0, 0, 0, 0} {}

    /// Explicit 4-component constructor (rarely needed directly).
    Guid(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
        : m_data{a, b, c, d} {}

    // -------------------------------------------------------------------------
    // Factory methods
    // -------------------------------------------------------------------------

    /**
     * @brief Generate a new RFC 4122 Version 4 GUID.
     *
     * TEACHING NOTE — UUID v4 Format
     * In a version-4 UUID the layout is:
     *   xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
     *   where 4 = version nibble
     *         y = variant nibble (must be 8, 9, A, or B)
     * All other bits are random.
     */
    static Guid New()
    {
        // TEACHING NOTE — std::mt19937 (Mersenne Twister)
        // mt19937 is the recommended general-purpose pseudo-random engine in C++.
        // We seed it once with a non-deterministic random_device.
        // thread_local ensures each thread has its own engine state.
        thread_local static std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<uint32_t> dist;

        uint32_t a = dist(rng);
        uint32_t b = dist(rng);
        uint32_t c = dist(rng);
        uint32_t d = dist(rng);

        // Set version 4: top nibble of 3rd word = 0x4
        b = (b & 0xFFFF0FFFu) | 0x00004000u;
        // Set variant bits: top 2 bits of 4th word = 0b10
        c = (c & 0x3FFFFFFFu) | 0x80000000u;

        return Guid(a, b, c, d);
    }

    /**
     * @brief Parse a GUID from a canonical string "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx".
     * @return Nil GUID if the string is not valid.
     */
    static Guid FromString(const std::string& s)
    {
        // Expected: 36 chars with dashes at positions 8, 13, 18, 23
        if (s.size() != 36)
            return Guid{};

        // Remove dashes and parse as hex
        std::string hex;
        hex.reserve(32);
        for (char c : s)
        {
            if (c != '-') hex += static_cast<char>(std::tolower(c));
        }
        if (hex.size() != 32)
            return Guid{};

        auto parseHex4 = [&](size_t offset) -> uint32_t
        {
            uint32_t val = 0;
            for (size_t i = 0; i < 8; ++i)
            {
                char ch = hex[offset + i];
                uint32_t nibble;
                if (ch >= '0' && ch <= '9')      nibble = static_cast<uint32_t>(ch - '0');
                else if (ch >= 'a' && ch <= 'f') nibble = static_cast<uint32_t>(ch - 'a' + 10);
                else                              return 0;  // invalid char
                val = (val << 4) | nibble;
            }
            return val;
        };

        return Guid(parseHex4(0), parseHex4(8), parseHex4(16), parseHex4(24));
    }

    // -------------------------------------------------------------------------
    // Queries
    // -------------------------------------------------------------------------

    /// Returns true if this is a non-nil (non-zero) GUID.
    bool IsValid() const
    {
        return m_data[0] || m_data[1] || m_data[2] || m_data[3];
    }

    // -------------------------------------------------------------------------
    // Conversion
    // -------------------------------------------------------------------------

    /**
     * @brief Convert to the canonical string "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx".
     */
    std::string ToString() const
    {
        // TEACHING NOTE — std::ostringstream and hex formatting
        // We use setfill('0') + setw(8) + hex to zero-pad the hex output.
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        oss << std::setw(8) << m_data[0] << '-';

        // Words 1 and 2 are split into 4-hex and 4-hex segments
        uint32_t b = m_data[1];
        oss << std::setw(4) << (b >> 16) << '-';
        oss << std::setw(4) << (b & 0xFFFF) << '-';

        uint32_t c = m_data[2];
        oss << std::setw(4) << (c >> 16) << '-';
        oss << std::setw(4) << (c & 0xFFFF);

        oss << std::setw(8) << m_data[3];

        return oss.str();
    }

    // -------------------------------------------------------------------------
    // Comparison operators
    // -------------------------------------------------------------------------

    bool operator==(const Guid& rhs) const { return m_data == rhs.m_data; }
    bool operator!=(const Guid& rhs) const { return !(*this == rhs); }
    bool operator< (const Guid& rhs) const { return m_data <  rhs.m_data; }

private:
    std::array<uint32_t, 4> m_data;  ///< Raw 128-bit storage (4 × 32-bit words)
};

// TEACHING NOTE — std::hash specialization
// This lets you use Guid as a key in std::unordered_map or std::unordered_set.
namespace std
{
    template<>
    struct hash<Guid>
    {
        size_t operator()(const Guid& g) const noexcept
        {
            // Simple FNV-1a mix of the string representation
            size_t h = 14695981039346656037ULL;
            const std::string s = g.ToString();
            for (char c : s)
            {
                h ^= static_cast<size_t>(c);
                h *= 1099511628211ULL;
            }
            return h;
        }
    };
}
