/**
 * @file pak_main.cpp
 * @brief PAK Packager — bundles a directory into a binary .pak archive.
 *
 * ============================================================================
 * TEACHING NOTE — What Is a PAK File?
 * ============================================================================
 * Most shipped games do NOT distribute thousands of loose asset files on disk.
 * Instead they bundle them into one or a few large archive files commonly
 * called "PAK" (package) files.  Reasons:
 *
 *   1. Single-file distribution — easier to install and copy.
 *   2. Streaming efficiency — continuous data on disk means fewer seek
 *      operations; the OS can pre-fetch data sequentially.
 *   3. Compression — the entire archive can be compressed (LZ4, Zstd),
 *      giving better ratios than individual file compression.
 *   4. Obfuscation — loose asset files are easy to extract and mod; a PAK
 *      with obfuscated offsets raises the bar slightly.
 *   5. Atomic delivery — a partially-downloaded PAK is easily detected
 *      (wrong file size or bad checksum); loose files are harder to validate.
 *
 * ============================================================================
 * TEACHING NOTE — How FF15 (and AAA Games) Use PAK Files
 * ============================================================================
 * Final Fantasy XV uses CRIWARE CPK archives and Square Enix's internal
 * SQEX PAK format.  The general pattern is:
 *
 *   • Multiple PAK "chunks" for streaming: chunk_0.pak (characters),
 *     chunk_1.pak (world geometry), chunk_2.pak (cutscenes), etc.
 *   • Each chunk has a table-of-contents (TOC) at the start that the engine
 *     memory-maps for O(1) lookup by asset path hash.
 *   • Chunks are sector-aligned (typically 2048 or 4096 bytes) so the OS
 *     can issue aligned DMA reads directly to GPU memory on consoles.
 *
 * Our "PAK1" format is a simplified version of the same idea — it is
 * intentionally easy to read and extend.
 *
 * ============================================================================
 * TEACHING NOTE — PAK1 Binary File Format
 * ============================================================================
 * The format is little-endian.  All integer types are unsigned.
 *
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │ HEADER  (12 bytes)                                                      │
 * │   magic:      uint32  = 0x31_4B_41_50  ('P','A','K','1' in little-end.) │
 * │   version:    uint32  = 1                                               │
 * │   fileCount:  uint32  = N  (number of entries in the file table)       │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │ FILE TABLE  (variable size, N entries, each of variable size)          │
 * │   For each entry i = 0 .. N-1:                                         │
 * │     pathLen:    uint16  = byte length of relative path string          │
 * │     path:       char[pathLen]  (UTF-8, no null terminator)             │
 * │     dataOffset: uint64  = absolute byte offset of this file's data     │
 * │     dataSize:   uint64  = byte size of this file's data                │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │ FILE DATA  (N blobs concatenated in table order)                       │
 * │   Raw bytes of each file, packed one after another with no gaps.       │
 * └─────────────────────────────────────────────────────────────────────────┘
 *
 * TEACHING NOTE — Why an Offset Table?
 * The file table lets us seek to any file's data in O(1) time:
 *   1. Read the header to get N.
 *   2. Scan the file table for the desired path.
 *   3. fseek(dataOffset) → fread(dataSize bytes).
 * No matter how large the PAK, this is at most O(N) in the table scan and
 * O(1) in the actual data access.  A hash map over path strings reduces the
 * scan to O(1) as well.
 *
 * TEACHING NOTE — Sector Alignment Tradeoff
 * PAK1 does NOT sector-align data entries (no padding between files).  This
 * keeps the format simple and small.  A production system would insert
 * alignment padding (e.g. align to 4096 bytes) so that:
 *   a) reads start on page boundaries (avoiding read-splitting),
 *   b) DMA hardware can copy directly without intermediate buffering.
 * For teaching purposes, alignment would add complexity without benefit.
 *
 * ============================================================================
 *
 * CLI:
 *   pak.exe --input <dir>  --output <file.pak>   create archive
 *   pak.exe --list  <file.pak>                    list contents
 *   pak.exe --extract <file.pak> --output <dir>  extract all files
 *
 * Exit codes: 0=success, 1=error.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Target: Cross-platform (Windows + Linux)
 */

#include "engine/core/Logger.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <cstring>   // std::memcmp
#include <cstdint>
#include <algorithm> // std::sort
#include <stdexcept>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// TEACHING NOTE — PAK1 magic constant
// ---------------------------------------------------------------------------
// 0x31_4B_41_50 spells 'PAK1' when interpreted as ASCII bytes in little-endian
// byte order:  byte[0]='P'=0x50, byte[1]='A'=0x41, byte[2]='K'=0x4B, byte[3]='1'=0x31.
// A magic number at the start of a binary file lets tools and the OS quickly
// identify the format without reading the full content.
// ---------------------------------------------------------------------------
static constexpr uint32_t kMagic   = 0x31'4B'41'50u; // 'PAK1' LE
static constexpr uint32_t kVersion = 1u;

// ---------------------------------------------------------------------------
// Entry — one file's metadata during pack/unpack.
// ---------------------------------------------------------------------------
struct Entry
{
    std::string path;        // relative path (UTF-8, forward slashes)
    uint64_t    dataOffset;  // absolute byte offset in the .pak file
    uint64_t    dataSize;    // byte size of the file data
};

// ---------------------------------------------------------------------------
// Helper: write a POD value in little-endian byte order.
// ---------------------------------------------------------------------------
// TEACHING NOTE — Endianness
// On x86/x64 (Windows, Linux) the CPU is natively little-endian, so writing
// a uint32_t with fwrite() already produces the right bytes.  On a big-endian
// platform (e.g. PowerPC / old consoles) you would need to byte-swap.
// We write primitives via these helpers to make the intent explicit.
template<typename T>
static void WriteLE(std::ofstream& out, T val)
{
    out.write(reinterpret_cast<const char*>(&val), sizeof(val));
}

// ---------------------------------------------------------------------------
// Helper: read a POD value from file (little-endian).
// ---------------------------------------------------------------------------
template<typename T>
static T ReadLE(std::ifstream& in)
{
    T val{};
    in.read(reinterpret_cast<char*>(&val), sizeof(val));
    // TEACHING NOTE — Fail fast on truncated/corrupt binary input.
    // Binary container readers must validate every primitive read.  If we
    // returned a default/partially-read value here, later code could treat
    // it as a trusted count, size, or offset and attempt huge allocations or
    // invalid seeks.  Throwing at the read boundary keeps the archive parser
    // in a well-defined state and preserves correct behaviour for valid files.
    if (!in)
        throw std::runtime_error("[pak] ReadLE: truncated or corrupt pak stream.");
    return val;
}

// ===========================================================================
// Create — pack a directory into a .pak file.
// ===========================================================================
static int CreatePak(const std::string& inputDir, const std::string& outputPath)
{
    fs::path baseDir(inputDir);
    if (!fs::exists(baseDir) || !fs::is_directory(baseDir))
    {
        std::cerr << "[pak] ERROR: input directory does not exist: "
                  << inputDir << "\n";
        return 1;
    }

    // TEACHING NOTE — Recursive Directory Walk
    // std::filesystem::recursive_directory_iterator (C++17) visits every
    // file and directory under baseDir in an unspecified order.  We collect
    // all regular files, then SORT them to get a deterministic output.
    // Determinism matters for:
    //   • Reproducible builds (same input → same output hash).
    //   • Diffing two PAKs (entries appear in the same order).
    //   • Cache-friendly streaming (related assets grouped by name prefix).
    std::vector<fs::path> filePaths;
    for (const auto& entry : fs::recursive_directory_iterator(baseDir))
    {
        if (entry.is_regular_file())
            filePaths.push_back(entry.path());
    }
    std::sort(filePaths.begin(), filePaths.end());

    const uint32_t fileCount = static_cast<uint32_t>(filePaths.size());
    std::cout << "[pak] Packing " << fileCount << " file(s) from "
              << inputDir << " → " << outputPath << "\n";

    // -----------------------------------------------------------------------
    // Pass 1 — Compute the file table size to determine the first data offset.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Two-Pass Layout
    // We need to know each file's dataOffset before writing anything.
    // The dataOffset = HEADER_SIZE + FILE_TABLE_SIZE + sum of preceding files.
    // So we compute the file table size first (pass 1), then write everything
    // in a single forward pass (pass 2).
    //
    // Header: 4 (magic) + 4 (version) + 4 (fileCount) = 12 bytes.
    // File table entry:
    //   2 (pathLen) + pathBytes + 8 (dataOffset) + 8 (dataSize) = variable.
    // -----------------------------------------------------------------------
    constexpr uint64_t kHeaderSize = 12;

    // Collect entries with relative paths and sizes.
    std::vector<Entry> entries;
    entries.reserve(fileCount);

    uint64_t tableSize = 0;
    for (const auto& fp : filePaths)
    {
        Entry e;
        // Compute relative path (normalise separator to '/').
        fs::path rel = fs::relative(fp, baseDir);
        std::string relStr = rel.string();
        // Normalise to forward slashes (POSIX convention, cross-platform safe).
        std::replace(relStr.begin(), relStr.end(), '\\', '/');
        e.path     = relStr;
        e.dataSize = static_cast<uint64_t>(fs::file_size(fp));
        e.dataOffset = 0;  // will be filled in below

        tableSize += 2 + static_cast<uint64_t>(relStr.size()) + 8 + 8;
        entries.push_back(std::move(e));
    }

    // Fill in data offsets: first file starts immediately after the file table.
    uint64_t dataStart = kHeaderSize + tableSize;
    uint64_t cursor    = dataStart;
    for (auto& e : entries)
    {
        e.dataOffset = cursor;
        cursor       += e.dataSize;
    }

    // -----------------------------------------------------------------------
    // Pass 2 — Write header, file table, then file data.
    // -----------------------------------------------------------------------
    std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        std::cerr << "[pak] ERROR: cannot open output file: " << outputPath << "\n";
        return 1;
    }

    // Write header.
    WriteLE(out, kMagic);
    WriteLE(out, kVersion);
    WriteLE(out, fileCount);

    // Write file table.
    for (const auto& e : entries)
    {
        // TEACHING NOTE — Path length guard before uint16_t truncation.
        // The PAK1 format stores pathLen as uint16_t (max 65535 bytes).
        // A path exceeding this limit would silently truncate, desynchronising
        // the reader from the file table.  Reject oversized paths early so
        // the archive is always well-formed.
        if (e.path.size() > 0xFFFF)
        {
            std::cerr << "[pak] ERROR: path too long (>" << 0xFFFF
                      << " bytes) for PAK1 format: " << e.path << "\n";
            return 1;
        }
        uint16_t pathLen = static_cast<uint16_t>(e.path.size());
        WriteLE(out, pathLen);
        out.write(e.path.data(), pathLen);
        WriteLE(out, e.dataOffset);
        WriteLE(out, e.dataSize);
    }

    // Write file data.
    std::vector<char> copyBuf(1 << 16); // 64 KiB copy buffer
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const Entry& e = entries[i];
        std::ifstream src(filePaths[i], std::ios::binary);
        if (!src)
        {
            std::cerr << "[pak] ERROR: cannot read source file: "
                      << filePaths[i] << "\n";
            return 1;
        }

        uint64_t remaining = e.dataSize;
        while (remaining > 0)
        {
            uint64_t toRead = std::min(remaining,
                                       static_cast<uint64_t>(copyBuf.size()));
            src.read(copyBuf.data(), static_cast<std::streamsize>(toRead));
            auto got = static_cast<uint64_t>(src.gcount());
            if (got == 0)
            {
                std::cerr << "[pak] ERROR: unexpected EOF in " << e.path << "\n";
                return 1;
            }
            out.write(copyBuf.data(), static_cast<std::streamsize>(got));
            remaining -= got;
        }
        std::cout << "  + " << e.path << "  (" << e.dataSize << " bytes)\n";
    }

    uint64_t totalSize = kHeaderSize + tableSize + (cursor - dataStart);
    std::cout << "[pak] Created " << outputPath
              << " (" << totalSize << " bytes total)\n";
    return 0;
}

// ===========================================================================
// List — print the table of contents of a .pak file.
// ===========================================================================
static int ListPak(const std::string& pakPath)
{
    std::ifstream in(pakPath, std::ios::binary);
    if (!in)
    {
        std::cerr << "[pak] ERROR: cannot open PAK file: " << pakPath << "\n";
        return 1;
    }

    try
    {
        // Validate magic + version.
        uint32_t magic   = ReadLE<uint32_t>(in);
        uint32_t version = ReadLE<uint32_t>(in);

        if (magic != kMagic)
        {
            std::cerr << "[pak] ERROR: bad magic (got 0x"
                      << std::hex << magic << ", expected 0x"
                      << kMagic << std::dec << ")\n";
            return 1;
        }
        if (version != kVersion)
        {
            std::cerr << "[pak] WARNING: unexpected version " << version
                      << " (expected " << kVersion << ")\n";
        }

        uint32_t fileCount = ReadLE<uint32_t>(in);
        std::cout << "[pak] " << pakPath << "  (version=" << version
                  << ", " << fileCount << " file(s))\n";
        std::cout << "  Offset            Size            Path\n";
        std::cout << "  ──────────────    ────────────    ────────────────────────\n";

        uint64_t totalDataSize = 0;
        for (uint32_t i = 0; i < fileCount; ++i)
        {
            uint16_t pathLen = ReadLE<uint16_t>(in);
            std::string path(pathLen, '\0');
            in.read(path.data(), pathLen);
            if (!in) throw std::runtime_error("[pak] ReadLE: truncated path string.");
            uint64_t dataOffset = ReadLE<uint64_t>(in);
            uint64_t dataSize   = ReadLE<uint64_t>(in);

            // TEACHING NOTE — Column alignment using std::setw
            // We use tab characters here for a quick, readable listing.
            // For fixed-column output (e.g. when piping to other tools) you
            // would use std::left / std::setw(N) from <iomanip> instead.
            std::cout << "  " << dataOffset
                      << "\t" << dataSize
                      << "\t" << path << "\n";
            totalDataSize += dataSize;
        }
        std::cout << "[pak] Total data: " << totalDataSize << " bytes\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[pak] ERROR reading pak: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}

// ===========================================================================
// Extract — extract all files from a .pak into a directory.
// ===========================================================================
static int ExtractPak(const std::string& pakPath, const std::string& outputDir)
{
    std::ifstream in(pakPath, std::ios::binary);
    if (!in)
    {
        std::cerr << "[pak] ERROR: cannot open PAK file: " << pakPath << "\n";
        return 1;
    }

    uint32_t fileCount = 0;
    std::vector<Entry> entries;

    try
    {
        // Validate header.
        uint32_t magic   = ReadLE<uint32_t>(in);
        uint32_t version = ReadLE<uint32_t>(in);

        if (magic != kMagic)
        {
            std::cerr << "[pak] ERROR: bad magic\n";
            return 1;
        }
        (void)version;

        fileCount = ReadLE<uint32_t>(in);

        // Read the file table.
        entries.resize(fileCount);
        for (uint32_t i = 0; i < fileCount; ++i)
        {
            uint16_t pathLen = ReadLE<uint16_t>(in);
            entries[i].path.resize(pathLen);
            in.read(entries[i].path.data(), pathLen);
            if (!in) throw std::runtime_error("[pak] ReadLE: truncated path string.");
            entries[i].dataOffset = ReadLE<uint64_t>(in);
            entries[i].dataSize   = ReadLE<uint64_t>(in);
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[pak] ERROR reading pak header/table: " << ex.what() << "\n";
        return 1;
    }

    fs::path outBase = fs::weakly_canonical(fs::path(outputDir));
    fs::create_directories(outBase);

    std::vector<char> copyBuf(1 << 16);
    for (const auto& e : entries)
    {
        // TEACHING NOTE — Path Traversal Safety (iterator-based check)
        // A naive string prefix check (`canonical.string().find(base) == 0`)
        // is bypassed by paths like "C:\out" vs "C:\out2" — "C:\out" is a
        // prefix of "C:\out2" so the check falsely passes.
        //
        // The correct check uses std::filesystem path iterators:
        //   • Compute canonical output path.
        //   • Use std::mismatch on the path component iterators to find the
        //     first differing segment.
        //   • If the base exhausted first (all base components matched the
        //     start of canonical), the path is safely inside the base.
        //
        // This handles separator normalisation, case differences (on case-
        // insensitive file systems) and ".." segments correctly because
        // weakly_canonical() resolves them before we compare.
        fs::path outPath  = fs::weakly_canonical(outBase / e.path);
        auto mismatchResult = std::mismatch(outBase.begin(), outBase.end(),
                                            outPath.begin(),  outPath.end());
        if (mismatchResult.first != outBase.end())
        {
            std::cerr << "[pak] SECURITY: path traversal detected: "
                      << e.path << " — skipped.\n";
            continue;
        }

        // Create parent directories as needed.
        fs::create_directories(outPath.parent_path());

        // Seek to data offset and extract.
        in.seekg(static_cast<std::streamoff>(e.dataOffset), std::ios::beg);

        std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            std::cerr << "[pak] ERROR: cannot write: " << outPath << "\n";
            return 1;
        }

        uint64_t remaining = e.dataSize;
        while (remaining > 0)
        {
            uint64_t toRead = std::min(remaining,
                                       static_cast<uint64_t>(copyBuf.size()));
            in.read(copyBuf.data(), static_cast<std::streamsize>(toRead));
            auto got = static_cast<uint64_t>(in.gcount());
            if (got == 0)
                break;
            out.write(copyBuf.data(), static_cast<std::streamsize>(got));
            remaining -= got;
        }
        std::cout << "  x " << e.path << "  (" << e.dataSize << " bytes)\n";
    }

    std::cout << "[pak] Extracted " << fileCount
              << " file(s) to " << outputDir << "\n";
    return 0;
}

// ===========================================================================
// main
// ===========================================================================
int main(int argc, char* argv[])
{
    try
    {
        // -----------------------------------------------------------------------
        // Parse arguments.
        // -----------------------------------------------------------------------
        // TEACHING NOTE — Minimalist argument parsing
        // We use a simple linear scan (O(N)) over argv.  This avoids adding a
        // third-party arg-parsing library and keeps the code readable.  The CLI
        // is small enough that brute-force scanning has no cost.
        // -----------------------------------------------------------------------
        std::string mode;      // "pack" / "list" / "extract"
        std::string inputArg;  // --input or --extract PAK path
        std::string outputArg; // --output

        for (int i = 1; i < argc; ++i)
        {
            const std::string arg(argv[i]);
            if (arg == "--input" && i + 1 < argc)
            {
                mode = "pack";
                inputArg = argv[++i];
            }
            else if (arg == "--output" && i + 1 < argc)
            {
                outputArg = argv[++i];
            }
            else if (arg == "--list" && i + 1 < argc)
            {
                mode = "list";
                inputArg = argv[++i];
            }
            else if (arg == "--extract" && i + 1 < argc)
            {
                mode = "extract";
                inputArg = argv[++i];
            }
            else if (arg == "--help" || arg == "-h")
            {
                std::cout <<
                    "pak — PAK1 archive packager\n"
                    "\n"
                    "Usage:\n"
                    "  pak --input <dir> --output <file.pak>   create archive\n"
                    "  pak --list <file.pak>                    list contents\n"
                    "  pak --extract <file.pak> --output <dir> extract all files\n"
                    "\n"
                    "PAK1 Format (little-endian):\n"
                    "  Header (12 bytes): magic=0x31_4B_41_50 version=1 fileCount=N\n"
                    "  FileTable: N × (pathLen uint16 + path + dataOffset uint64 + dataSize uint64)\n"
                    "  FileData: N raw blobs concatenated\n";
                return 0;
            }
        }

        if (mode.empty())
        {
            std::cerr << "[pak] ERROR: no mode specified.  Use --help for usage.\n";
            return 1;
        }

        if (mode == "pack")
        {
            if (inputArg.empty() || outputArg.empty())
            {
                std::cerr << "[pak] ERROR: --input and --output are required "
                             "for pack mode.\n";
                return 1;
            }
            return CreatePak(inputArg, outputArg);
        }
        else if (mode == "list")
        {
            if (inputArg.empty())
            {
                std::cerr << "[pak] ERROR: PAK file path required.\n";
                return 1;
            }
            return ListPak(inputArg);
        }
        else if (mode == "extract")
        {
            if (inputArg.empty() || outputArg.empty())
            {
                std::cerr << "[pak] ERROR: --extract and --output are required "
                             "for extract mode.\n";
                return 1;
            }
            return ExtractPak(inputArg, outputArg);
        }

        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[pak] FATAL: " << ex.what() << "\n";
        return 1;
    }
    catch (...)
    {
        std::cerr << "[pak] FATAL: unknown exception\n";
        return 1;
    }
}
