/**
 * @file Win32Window.hpp
 * @brief Win32 platform layer — window creation, message pump, and timer.
 *
 * ============================================================================
 * TEACHING NOTE — Platform Abstraction
 * ============================================================================
 * A "platform layer" is the bridge between the operating system and the rest
 * of the engine.  It is intentionally the ONLY place that includes Windows-
 * specific headers (windows.h, etc.).  Every other engine subsystem stays
 * OS-agnostic by depending on this layer through a clean interface.
 *
 * On Windows the OS API is called "Win32" (even on 64-bit machines).
 * The key Win32 concepts we use here are:
 *
 *   HWND   — a "handle to a window" (opaque pointer owned by Windows).
 *   HINSTANCE — a handle to the current process/module.
 *   WNDCLASSEX — a struct that describes what kind of window we want.
 *   WndProc — a callback function Windows calls for every window event.
 *
 * ============================================================================
 * TEACHING NOTE — High-Resolution Timer
 * ============================================================================
 * Windows provides QueryPerformanceCounter (QPC) which counts at a very
 * high frequency (typically tens of millions of ticks per second).  Together
 * with QueryPerformanceFrequency (QPF) this gives sub-microsecond timing that
 * is essential for a smooth fixed-timestep game loop.
 *
 *   elapsed_seconds = (counter_end - counter_start) / frequency
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Target OS: Windows (Win32 API)
 */

#pragma once

// ---------------------------------------------------------------------------
// TEACHING NOTE — WIN32_LEAN_AND_MEAN
// ---------------------------------------------------------------------------
// Including <windows.h> pulls in a *massive* amount of Win32 API declarations.
// Defining WIN32_LEAN_AND_MEAN before the include trims out rarely-used
// subsystems (COM, OLE, Cryptography, etc.) and dramatically speeds up
// compilation.  Always define it in engine code.
// ---------------------------------------------------------------------------
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
    // TEACHING NOTE — NOMINMAX
    // Windows.h defines macros called `min` and `max` that clash with
    // std::min / std::max.  Defining NOMINMAX before the include disables
    // those macros.
    #define NOMINMAX
#endif
#include <windows.h>

// Standard library
#include <string>       // std::wstring for window title
#include <cstdint>      // uint32_t

namespace engine {
namespace platform {

// ===========================================================================
// TEACHING NOTE — class Win32Window
// ===========================================================================
// This class encapsulates everything the engine needs from the operating
// system:
//   1. A visible window (HWND) the user can see and close.
//   2. A message pump — Windows sends keyboard, mouse, resize, close events
//      to a registered callback (WndProc).  We must pump this queue every
//      frame or Windows will think the application has frozen and show the
//      "not responding" overlay.
//   3. A delta-time source so the game loop knows how long each frame took.
//
// Lifetime:
//   Win32Window w;
//   if (!w.Init("My Window", 1280, 720)) { /* error */ }
//   while (w.IsRunning()) {
//       w.PollEvents();
//       double dt = w.GetDeltaTime();
//       // ... render frame ...
//   }
//   w.Shutdown();   // optional — destructor calls it automatically
// ===========================================================================
class Win32Window
{
public:
    // -----------------------------------------------------------------------
    // Construction / Destruction
    // -----------------------------------------------------------------------

    /**
     * @brief Default constructor.  Does NOT create a window.
     *
     * TEACHING NOTE — Two-Phase Initialisation
     * Call Init() explicitly after construction.  This allows the object to
     * be stored as a member variable before we are ready to create the window.
     */
    Win32Window()  = default;

    /**
     * @brief Destructor — automatically destroys the window if still open.
     *
     * TEACHING NOTE — RAII
     * The destructor guarantees the window is cleaned up even if the caller
     * forgets to call Shutdown().  This prevents "zombie" windows from
     * lingering after a crash or early return.
     */
    ~Win32Window();

    // Prevent copying — a window is a unique OS resource.
    Win32Window(const Win32Window&)            = delete;
    Win32Window& operator=(const Win32Window&) = delete;

    // Move semantics are OK but omitted for simplicity in this teaching code.

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    /**
     * @brief Create the Win32 window and initialise the high-resolution timer.
     *
     * @param title   Window title bar text (wide string for Unicode support).
     * @param width   Client area width in pixels.
     * @param height  Client area height in pixels.
     * @return true on success, false if any Win32 call fails.
     *
     * TEACHING NOTE — Client Area vs Window Area
     * A Win32 window has a title bar and borders whose size depends on the
     * Windows theme.  The "client area" is the drawable region *inside* those
     * decorations.  We use AdjustWindowRect to inflate our desired client size
     * so that the final window delivers exactly the pixels we asked for.
     */
    bool Init(const std::wstring& title, uint32_t width, uint32_t height);

    /**
     * @brief Destroy the window and un-register the window class.
     *
     * Safe to call multiple times (guarded by m_initialised flag).
     */
    void Shutdown();

    // -----------------------------------------------------------------------
    // Per-frame interface
    // -----------------------------------------------------------------------

    /**
     * @brief Pump the Win32 message queue.
     *
     * TEACHING NOTE — Message Pump
     * Windows is event-driven: it posts events (WM_KEYDOWN, WM_CLOSE, …) into
     * a per-thread queue.  PeekMessage / TranslateMessage / DispatchMessage
     * drains that queue and routes each event to our WndProc callback.
     *
     * We use PeekMessage (non-blocking) rather than GetMessage (blocking) so
     * that the game loop keeps running at full speed even with no user input.
     *
     * Must be called once per frame before drawing.
     */
    void PollEvents();

    /**
     * @brief Return elapsed seconds since the previous PollEvents() call.
     *
     * TEACHING NOTE — Delta Time
     * "Delta time" (dt) is the duration of the previous frame in seconds.
     * It is used in the game loop to advance simulations independently of
     * frame rate: position += velocity * dt.
     *
     * @return Delta time in seconds (clamped to a maximum of 0.25 s to
     *         prevent "spiral of death" when the app is paused/debugged).
     */
    double GetDeltaTime() const { return m_deltaTime; }

    /**
     * @brief Returns false when the user closes the window (WM_QUIT received).
     */
    bool IsRunning() const { return m_running; }

    // -----------------------------------------------------------------------
    // Accessors used by the Vulkan renderer
    // -----------------------------------------------------------------------

    /**
     * @brief Returns the Win32 window handle (HWND) needed by Vulkan surface creation.
     */
    HWND GetHWND() const { return m_hwnd; }

    /**
     * @brief Returns the HINSTANCE of the current process.
     */
    HINSTANCE GetHINSTANCE() const { return m_hinstance; }

    /**
     * @brief Returns the current client-area width in pixels.
     */
    uint32_t GetWidth()  const { return m_width; }

    /**
     * @brief Returns the current client-area height in pixels.
     */
    uint32_t GetHeight() const { return m_height; }

    /**
     * @brief Returns true if the window was resized since the last call to
     *        ClearResizedFlag().  Used by the renderer to recreate the swapchain.
     */
    bool WasResized() const { return m_resized; }

    /**
     * @brief Reset the resize flag.  Call after the renderer has rebuilt the swapchain.
     */
    void ClearResizedFlag() { m_resized = false; }

private:
    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Static Win32 window procedure (callback).
     *
     * TEACHING NOTE — WndProc
     * Windows calls this function for every event directed at our window.
     * It must be static (or a free function) because Windows stores a plain
     * C function pointer, not a C++ member function pointer.  We recover the
     * this pointer via GetWindowLongPtr(GWLP_USERDATA) which we set in Init().
     *
     * @param hWnd   The window that received the message.
     * @param msg    The message identifier (WM_CLOSE, WM_SIZE, WM_KEYDOWN, …).
     * @param wParam First parameter — meaning depends on the message.
     * @param lParam Second parameter — meaning depends on the message.
     */
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // -----------------------------------------------------------------------
    // Data members
    // -----------------------------------------------------------------------

    HWND      m_hwnd       = nullptr;   ///< Handle to our window.
    HINSTANCE m_hinstance  = nullptr;   ///< Handle to the current process/module.

    uint32_t  m_width      = 0;         ///< Client-area width (pixels).
    uint32_t  m_height     = 0;         ///< Client-area height (pixels).

    bool      m_running     = false;    ///< False after WM_QUIT is received.
    bool      m_initialised = false;    ///< Guard for double Shutdown() calls.
    bool      m_resized     = false;    ///< Set by WM_SIZE; cleared by renderer.

    // -----------------------------------------------------------------------
    // TEACHING NOTE — High-Resolution Timer State
    // -----------------------------------------------------------------------
    // QueryPerformanceCounter gives us a 64-bit counter value.
    // QueryPerformanceFrequency tells us how many counts equal one second.
    // We record m_lastCounter at the *end* of PollEvents so we can compute
    // the gap at the start of the next PollEvents call.
    // -----------------------------------------------------------------------
    LARGE_INTEGER m_frequency  = {};    ///< QPC ticks per second (constant).
    LARGE_INTEGER m_lastCounter = {};   ///< QPC value at previous frame.
    double        m_deltaTime   = 0.0;  ///< Seconds elapsed last frame.

    // Window class name — used for both registration and un-registration.
    static constexpr const wchar_t* kWindowClassName = L"EngineWndClass";
};

} // namespace platform
} // namespace engine
