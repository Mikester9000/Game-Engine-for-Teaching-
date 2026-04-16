/**
 * @file Win32Window.cpp
 * @brief Win32 platform layer implementation.
 *
 * ============================================================================
 * TEACHING NOTE — Implementation File Organisation
 * ============================================================================
 * The .cpp file contains only the implementation.  The .hpp file (which every
 * user of this class includes) never includes windows.h directly — that
 * detail is encapsulated here.  Users of Win32Window see only the clean
 * public interface and never have to worry about name pollution from the
 * Windows headers.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 */

// ---------------------------------------------------------------------------
// Always include our own header first.
// ---------------------------------------------------------------------------
#include "engine/platform/win32/Win32Window.hpp"

// Standard library
#include <stdexcept>    // std::runtime_error
#include <iostream>     // std::cerr (fallback logging before Logger is wired up)

namespace engine {
namespace platform {

// ===========================================================================
// Destructor
// ===========================================================================
Win32Window::~Win32Window()
{
    // TEACHING NOTE — Destructor = Guaranteed Cleanup
    // Calling Shutdown() here means the window is always destroyed, even if
    // the caller forgets to call Shutdown() explicitly.
    Shutdown();
}

// ===========================================================================
// Init
// ===========================================================================
bool Win32Window::Init(const std::wstring& title, uint32_t width, uint32_t height)
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — GetModuleHandle(nullptr)
    // Every Win32 executable is a "module".  GetModuleHandle(nullptr) returns
    // the HINSTANCE (module handle) of the currently running .exe.  We need
    // it to register and create window classes.
    // -----------------------------------------------------------------------
    m_hinstance = GetModuleHandle(nullptr);
    m_width  = width;
    m_height = height;

    // -----------------------------------------------------------------------
    // Step 1 — Register a window class.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — WNDCLASSEX
    // Before creating any window, Windows requires us to "register" a window
    // class — a template that describes the window's icon, cursor, background
    // colour, and most importantly the WndProc callback.
    // Multiple windows can share the same class; we only register ours once.
    // -----------------------------------------------------------------------
    WNDCLASSEX wc   = {};
    wc.cbSize       = sizeof(WNDCLASSEX);
    wc.style        = CS_HREDRAW | CS_VREDRAW;  // Redraw on horizontal/vertical resize
    wc.lpfnWndProc  = Win32Window::WndProc;     // Our event callback
    wc.hInstance    = m_hinstance;
    wc.hCursor      = LoadCursor(nullptr, IDC_ARROW);   // Standard arrow cursor
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kWindowClassName;

    if (!RegisterClassEx(&wc))
    {
        std::cerr << "[Win32Window] RegisterClassEx failed: " << GetLastError() << "\n";
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 2 — Compute the window rect so the CLIENT area matches width×height.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — AdjustWindowRect
    // WS_OVERLAPPEDWINDOW gives a standard title bar + min/max/close buttons.
    // The title bar and borders consume pixels.  AdjustWindowRect inflates
    // the requested rect so the *client* area (usable drawing area) ends up
    // exactly width × height pixels.
    // -----------------------------------------------------------------------
    RECT wr = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    // -----------------------------------------------------------------------
    // Step 3 — Create the window.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — CreateWindowEx
    // dwExStyle  : extended style flags (0 = defaults)
    // lpClassName: the class we registered above
    // lpWindowName: title bar text
    // dwStyle    : WS_OVERLAPPEDWINDOW = standard resizable window
    // x, y       : CW_USEDEFAULT lets Windows pick a sensible screen position
    // nWidth/nHeight: inflated rect dimensions from AdjustWindowRect
    // hWndParent : nullptr = top-level window (no parent)
    // hMenu      : nullptr = no menu bar
    // hInstance  : our module handle
    // lpParam    : extra data pointer — we pass 'this' so WndProc can recover it
    // -----------------------------------------------------------------------
    m_hwnd = CreateWindowEx(
        0,
        kWindowClassName,
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left,
        wr.bottom - wr.top,
        nullptr,
        nullptr,
        m_hinstance,
        this    // <-- passed as lpCreateParams; retrieved in WM_NCCREATE
    );

    if (!m_hwnd)
    {
        std::cerr << "[Win32Window] CreateWindowEx failed: " << GetLastError() << "\n";
        UnregisterClass(kWindowClassName, m_hinstance);
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 4 — Make the window visible.
    // -----------------------------------------------------------------------
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    // -----------------------------------------------------------------------
    // Step 5 — Initialise the high-resolution timer.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — QueryPerformanceFrequency
    // This returns the number of QPC ticks per second.  It is constant for
    // the lifetime of the process, so we query it once here.
    // -----------------------------------------------------------------------
    QueryPerformanceFrequency(&m_frequency);
    QueryPerformanceCounter(&m_lastCounter);  // Capture the starting time

    m_running     = true;
    m_initialised = true;
    return true;
}

// ===========================================================================
// Shutdown
// ===========================================================================
void Win32Window::Shutdown()
{
    if (!m_initialised)
        return;

    m_initialised = false;
    m_running     = false;

    // TEACHING NOTE — DestroyWindow vs PostQuitMessage
    // DestroyWindow sends WM_DESTROY to the window, then removes it from the
    // screen and frees its resources.  Our WndProc handles WM_DESTROY by
    // calling PostQuitMessage which enqueues WM_QUIT.  That WM_QUIT is what
    // sets m_running = false in the message pump loop.
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    // Un-register the window class so another Init() call (e.g. in tests)
    // can register it fresh without "class already exists" errors.
    if (m_hinstance)
    {
        UnregisterClass(kWindowClassName, m_hinstance);
        m_hinstance = nullptr;
    }
}

// ===========================================================================
// PollEvents
// ===========================================================================
void Win32Window::PollEvents()
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Computing Delta Time
    // -----------------------------------------------------------------------
    // Before pumping messages (which might take non-trivial time), we capture
    // the current QPC value.  The gap between this and m_lastCounter is the
    // real wall-clock time of the previous frame.
    //
    // We clamp delta time to 0.25 s to prevent the "spiral of death": if the
    // application is paused by the debugger or a long GC, a single enormous
    // dt could cause the physics simulation to explode.
    // -----------------------------------------------------------------------
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    double elapsed = static_cast<double>(now.QuadPart - m_lastCounter.QuadPart)
                     / static_cast<double>(m_frequency.QuadPart);

    m_deltaTime   = (elapsed > 0.25) ? 0.25 : elapsed;
    m_lastCounter = now;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — PeekMessage vs GetMessage
    // -----------------------------------------------------------------------
    // GetMessage BLOCKS until a message arrives — useless for a game loop.
    // PeekMessage returns immediately (PM_REMOVE removes it from the queue).
    // We drain every pending message in a tight loop so the OS never thinks
    // the app is unresponsive.
    // -----------------------------------------------------------------------
    MSG msg = {};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            m_running = false;
        }
        // TranslateMessage converts key-down/key-up pairs into WM_CHAR for
        // text input.  DispatchMessage routes the message to WndProc.
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

// ===========================================================================
// WndProc — static window procedure
// ===========================================================================
// TEACHING NOTE — Why Static?
// Windows stores a plain C function pointer in the WNDCLASSEX struct.  C++
// member functions have an implicit `this` parameter, so they are not
// compatible with plain function pointers.  The solution is a static member
// function that retrieves the instance pointer we stored via SetWindowLongPtr.
// ===========================================================================
LRESULT CALLBACK Win32Window::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // -----------------------------------------------------------------------
    // Retrieve the Win32Window* that was stored during WM_NCCREATE.
    // -----------------------------------------------------------------------
    Win32Window* self = reinterpret_cast<Win32Window*>(
        GetWindowLongPtr(hWnd, GWLP_USERDATA)
    );

    switch (msg)
    {
    // -----------------------------------------------------------------------
    // TEACHING NOTE — WM_NCCREATE
    // -----------------------------------------------------------------------
    // This is the very first message Windows sends to a new window, *before*
    // WM_CREATE.  We use it to stash our `this` pointer in the window's user
    // data slot so every subsequent message can find it via GetWindowLongPtr.
    // -----------------------------------------------------------------------
    case WM_NCCREATE:
    {
        // lParam points to a CREATESTRUCT whose lpCreateParams is our `this`.
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    // -----------------------------------------------------------------------
    // WM_SIZE — window was resized.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Swapchain Recreation
    // Vulkan swapchains are tied to a specific width and height.  When the
    // window is resized, the swapchain must be destroyed and rebuilt to match
    // the new dimensions.  We set a flag here; the renderer checks it each
    // frame and rebuilds when needed.
    // -----------------------------------------------------------------------
    case WM_SIZE:
        if (self)
        {
            self->m_width   = LOWORD(lParam);
            self->m_height  = HIWORD(lParam);
            self->m_resized = true;
        }
        return 0;

    // -----------------------------------------------------------------------
    // WM_CLOSE — user clicked the × button.
    // -----------------------------------------------------------------------
    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    // -----------------------------------------------------------------------
    // WM_DESTROY — window has been destroyed (sent after WM_CLOSE is handled).
    // -----------------------------------------------------------------------
    // TEACHING NOTE — PostQuitMessage
    // Calling PostQuitMessage(0) places a WM_QUIT message in the queue.
    // PollEvents() will see that WM_QUIT and set m_running = false, which
    // causes the main game loop to exit cleanly.
    // -----------------------------------------------------------------------
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    // -----------------------------------------------------------------------
    // WM_KEYDOWN — a keyboard key was pressed.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Virtual-Key Codes
    // wParam holds a Virtual-Key (VK) code, e.g. VK_ESCAPE.  We handle
    // Escape here to give the user a keyboard-only exit path.
    // -----------------------------------------------------------------------
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            PostQuitMessage(0);
        }
        return 0;

    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

} // namespace platform
} // namespace engine
