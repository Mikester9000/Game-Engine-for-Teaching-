/**
 * @file RendererFactory.hpp
 * @brief Factory that creates the appropriate IRenderer backend at runtime.
 *
 * ============================================================================
 * TEACHING NOTE — Factory Pattern for Renderer Selection
 * ============================================================================
 * The factory pattern decouples object creation from usage.  The caller says
 * "give me a renderer" and the factory decides which concrete type to
 * instantiate based on a runtime RendererBackend enum value.
 *
 * This keeps main.cpp clean — it only knows about IRenderer, not about
 * D3D11Renderer or VulkanRenderer directly.  Swapping the backend is a
 * one-line change to the --renderer flag.
 *
 * ============================================================================
 * TEACHING NOTE — Runtime Backend Selection vs Compile-Time
 * ============================================================================
 * We could also select the backend at compile time via preprocessor macros,
 * but runtime selection is more flexible for testing:
 *
 *   engine_sandbox.exe                  → D3D11 (default)
 *   engine_sandbox.exe --renderer d3d11 → D3D11 explicit
 *   engine_sandbox.exe --renderer vulkan → Vulkan (requires ENGINE_ENABLE_VULKAN)
 *
 * If a backend was not compiled in, the factory logs an error and returns
 * nullptr so the caller can gracefully exit.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Target: Windows (MSVC)
 */

#pragma once

#include "engine/rendering/IRenderer.hpp"

#include <memory>   // std::unique_ptr
#include <string>
#include <iostream>

// Include concrete backends based on compile-time feature flags.
// Each flag is set by CMakeLists.txt via target_compile_definitions.
#if defined(ENGINE_ENABLE_D3D11)
    #include "engine/rendering/d3d11/D3D11Renderer.hpp"
#endif

#if defined(ENGINE_ENABLE_VULKAN)
    #include "engine/rendering/vulkan/VulkanRenderer.hpp"
#endif

namespace engine {
namespace rendering {

// ---------------------------------------------------------------------------
// TEACHING NOTE — RendererBackend Enum
// ---------------------------------------------------------------------------
// A strongly-typed enum class prevents accidental integer-to-enum conversions
// and avoids polluting the enclosing namespace with the enumerator names.
// ---------------------------------------------------------------------------
enum class RendererBackend
{
    D3D11,   // Direct3D 11 — GT610-compatible baseline (default on Windows)
    Vulkan,  // Vulkan 1.0+ — modern / high-end path
};

// ---------------------------------------------------------------------------
// ParseRendererBackend — convert a CLI string to the RendererBackend enum
// ---------------------------------------------------------------------------
// TEACHING NOTE — Parsing Command-Line Enum Values
// We use a simple string comparison.  The default is D3D11 so any unrecognised
// value falls through to D3D11 with a warning.
// ---------------------------------------------------------------------------
inline RendererBackend ParseRendererBackend(const std::string& s)
{
    if (s == "vulkan") return RendererBackend::Vulkan;
    if (s == "d3d11")  return RendererBackend::D3D11;

    // Default: D3D11 (compatible with GT610 and CI runners).
    if (!s.empty())
    {
        std::cerr << "[RendererFactory] Unknown backend '" << s
                  << "'; defaulting to d3d11.\n";
    }
    return RendererBackend::D3D11;
}

// ---------------------------------------------------------------------------
// CreateRenderer — factory function
// ---------------------------------------------------------------------------
// Returns a heap-allocated IRenderer wrapped in unique_ptr, or nullptr if
// the requested backend was not compiled in.
// ---------------------------------------------------------------------------
inline std::unique_ptr<IRenderer> CreateRenderer(RendererBackend backend)
{
    switch (backend)
    {
        case RendererBackend::D3D11:
        {
#if defined(ENGINE_ENABLE_D3D11)
            return std::make_unique<D3D11Renderer>();
#else
            std::cerr << "[RendererFactory] D3D11 backend requested but "
                         "ENGINE_ENABLE_D3D11 was not set at compile time.\n";
            return nullptr;
#endif
        }

        case RendererBackend::Vulkan:
        {
#if defined(ENGINE_ENABLE_VULKAN)
            return std::make_unique<VulkanRenderer>();
#else
            std::cerr << "[RendererFactory] Vulkan backend requested but "
                         "ENGINE_ENABLE_VULKAN was not set at compile time.\n"
                      << "  Rebuild with -DENGINE_ENABLE_VULKAN=ON and a "
                         "Vulkan SDK installed.\n";
            return nullptr;
#endif
        }

        default:
            std::cerr << "[RendererFactory] Unknown backend enum value.\n";
            return nullptr;
    }
}

} // namespace rendering
} // namespace engine
