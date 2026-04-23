#pragma once

/**
 * @file scoped_gpu_event.hpp
 * @brief Lightweight GPU event marker wrapper for PIX and RenderDoc.
 *
 * ============================================================================
 * TEACHING NOTE — PF-2: PIX / RenderDoc Event Markers
 * ============================================================================
 * GPU profilers like PIX (Microsoft) and RenderDoc (open-source) can capture
 * a single frame of GPU commands and let you inspect every draw call, resource
 * binding, and shader invocation.
 *
 * Without labels, a frame capture looks like an anonymous list of "Draw 3
 * indices", "Draw 6 indices", etc.  With event markers, the capture shows:
 *
 *   ▼ Shadow pass        ← SCOPED_GPU_EVENT(ctx, "Shadow pass")
 *       Draw 3 indices   ← shadow mesh
 *   ▼ Lit pass           ← SCOPED_GPU_EVENT(ctx, "Lit pass")
 *       Draw 6 indices   ← full-screen quad
 *   ▼ Bloom bright-pass  ← SCOPED_GPU_EVENT(ctx, "Bloom bright-pass")
 *       ...
 *
 * ============================================================================
 * TEACHING NOTE — Implementation Strategy
 * ============================================================================
 * PIX requires the WinPixEventRuntime NuGet/vcpkg package which may not
 * always be present.  We use D3D11's built-in ID3DUserDefinedAnnotation
 * interface instead — it ships with every D3D11 installation and is
 * automatically visible in PIX and RenderDoc without any extra package.
 *
 * How ID3DUserDefinedAnnotation works:
 *   1. Query the interface from the D3D11 device context:
 *        context->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), &ann);
 *   2. Call ann->BeginEvent(L"Shadow pass") at the start of a render pass.
 *   3. Call ann->EndEvent() at the end.
 *   4. PIX / RenderDoc show these labels in the event list.
 *
 * The SCOPED_GPU_EVENT macro wraps this in a RAII guard so EndEvent() is
 * always called even if an early return is hit.
 *
 * ============================================================================
 * TEACHING NOTE — RAII Guards
 * ============================================================================
 * A RAII guard is a local object whose destructor performs cleanup.
 * The C++ runtime guarantees destructors are called when the object goes out
 * of scope — even via early return or exception.  This eliminates the
 * "forgot to call EndEvent()" bug class.
 *
 * ============================================================================
 * TEACHING NOTE — No-op fallback
 * ============================================================================
 * On non-Windows builds (or when D3D11 is not available) all macros expand to
 * nothing so the code compiles and runs without any overhead.
 * ============================================================================
 */

#ifdef _WIN32
#  include <d3d11.h>
#  include <d3d9.h>    // D3DCOLOR_RGBA used by annotation colours

// ---------------------------------------------------------------------------
// ScopedGpuEvent — RAII wrapper around ID3DUserDefinedAnnotation.
// ---------------------------------------------------------------------------
// TEACHING NOTE — We embed this in the header so it can be inlined by the
// compiler.  The struct is only 8 bytes on 64-bit (one pointer) so the RAII
// overhead is negligible.
// ---------------------------------------------------------------------------
struct ScopedGpuEvent
{
    ID3DUserDefinedAnnotation* ann = nullptr;

    /**
     * @brief Begin a named GPU event.
     *
     * @param ctx   D3D11 device context.
     * @param name  UTF-16 event label shown in PIX / RenderDoc.
     *
     * TEACHING NOTE — QueryInterface follows the COM programming model used by
     * all of D3D11.  Every COM object can be "cast" to a different interface
     * by calling QueryInterface with the interface GUID.  If the object does
     * not implement the requested interface, QueryInterface returns E_NOINTERFACE
     * and we simply disable annotations for this event.
     */
    ScopedGpuEvent(ID3D11DeviceContext* ctx, const wchar_t* name)
    {
        if (ctx)
        {
            ctx->QueryInterface(__uuidof(ID3DUserDefinedAnnotation),
                                reinterpret_cast<void**>(&ann));
            if (ann)
                ann->BeginEvent(name);
        }
    }

    /// Automatically end the event when the guard leaves scope.
    ~ScopedGpuEvent()
    {
        if (ann)
        {
            ann->EndEvent();
            ann->Release();
        }
    }

    // Non-copyable, non-movable — the guard owns the annotation reference.
    ScopedGpuEvent(const ScopedGpuEvent&)            = delete;
    ScopedGpuEvent& operator=(const ScopedGpuEvent&) = delete;
};

// ---------------------------------------------------------------------------
// SCOPED_GPU_EVENT(ctx, nameUtf16)
// ---------------------------------------------------------------------------
// Usage:
//   SCOPED_GPU_EVENT(m_context.Get(), L"Shadow pass");
//
// The __LINE__ suffix makes the local variable name unique when multiple
// events are used in the same function scope.
// ---------------------------------------------------------------------------
#  define SCOPED_GPU_EVENT_CONCAT_(a, b) a ## b
#  define SCOPED_GPU_EVENT_CONCAT(a, b)  SCOPED_GPU_EVENT_CONCAT_(a, b)
#  define SCOPED_GPU_EVENT(ctx, name) \
     ScopedGpuEvent SCOPED_GPU_EVENT_CONCAT(_gpuEvent_, __LINE__)((ctx), (name))

#else // !_WIN32

// No-op on non-Windows platforms.
#  define SCOPED_GPU_EVENT(ctx, name)  ((void)0)

#endif // _WIN32
