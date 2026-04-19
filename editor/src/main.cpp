/**
 * @file main.cpp
 * @brief Creation Suite Editor -- Win32 + D3D11 + Dear ImGui entry point.
 *
 * =============================================================================
 * TEACHING NOTE -- Immediate-Mode GUI (ImGui) vs Retained-Mode GUI
 * =============================================================================
 * RETAINED-MODE (Qt, WinForms, GTK ...):
 *   - Widget objects persist between frames.
 *   - You PUSH data into them (setText, setModel ...) and register callbacks.
 *   - The framework decides when to redraw.
 *   - Typically requires a large external framework installation.
 *
 * IMMEDIATE-MODE (Dear ImGui):
 *   - No persistent widget objects -- you RE-DECLARE the entire UI every frame.
 *   - ImGui::Button("Save") returns true the frame it was clicked.
 *     That single call handles layout, rendering, AND input query.
 *   - The same model is used in Unreal Engine's Slate debug overlays,
 *     Unity's IMGUI system, Valve's in-engine tools, and many AAA studios.
 *   - Licence: MIT -- zero cost, no LGPL, no install required.
 *
 * =============================================================================
 * TEACHING NOTE -- Win32 + D3D11 Editor Bootstrap
 * =============================================================================
 * The editor is a standalone Win32 + D3D11 executable -- the same low-level
 * APIs used by the engine itself.  Steps:
 *
 *   1. RegisterClassExW + CreateWindowExW  -> HWND
 *   2. D3D11CreateDeviceAndSwapChain       -> ID3D11Device + IDXGISwapChain
 *   3. ImGui_ImplWin32_Init + ImGui_ImplDX11_Init
 *   4. Main message/render loop (see below)
 *   5. Reverse-order cleanup
 *
 * D3D11 is chosen because it ships with Windows and requires no extra SDK
 * install (same reason D3D11 is the engine's default renderer).
 *
 * =============================================================================
 */

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <tchar.h>

// Dear ImGui headers (provided by vcpkg imgui[docking,dx11-binding,win32-binding])
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

#include "EditorApp.hpp"

// ---------------------------------------------------------------------------
// Module-level D3D11 objects
// ---------------------------------------------------------------------------
// TEACHING NOTE -- Module-level (global) objects for D3D11
// We store D3D11 objects at module scope so both the WndProc and the main
// loop can access them without threading overhead.  In a larger engine these
// would live inside a Renderer class; for a single-file bootstrap, module
// scope is clear and self-documenting.
static ID3D11Device*            g_pd3dDevice         = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext   = nullptr;
static IDXGISwapChain*          g_pSwapChain          = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static bool CreateDeviceD3D(HWND hWnd);
static void CleanupDeviceD3D();
static void CreateRenderTarget();
static void CleanupRenderTarget();

// ImGui Win32 message handler -- forward declared here, defined in imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ---------------------------------------------------------------------------
// Win32 window procedure
// ---------------------------------------------------------------------------
// TEACHING NOTE -- Window Procedure (WndProc)
// Every Win32 window requires a "window procedure" callback that processes
// messages sent by the OS (WM_SIZE, WM_DESTROY, mouse events, key events ...).
// Messages are dispatched by DispatchMessageW in the main loop.
//
// The ImGui Win32 backend provides ImGui_ImplWin32_WndProcHandler -- we call
// it first so ImGui can intercept mouse/keyboard input before we handle it.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;  // ImGui consumed the message

    switch (msg)
    {
    case WM_SIZE:
        // TEACHING NOTE -- Swap-chain resize on WM_SIZE
        // When the window is resized the swap chain buffers become stale.
        // We must release the old render target view, resize the swap chain,
        // then re-create the render target view with the new dimensions.
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(
                0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;

    case WM_SYSCOMMAND:
        // Disable ALT application menu so ALT key does not freeze the editor
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/,
                   LPSTR /*lpCmdLine*/, int nCmdShow)
{
    // ---- 1. Register and create the Win32 window ---------------------------
    // TEACHING NOTE -- WNDCLASSEXW
    // WNDCLASSEXW describes the properties of a window class (shared template
    // from which individual windows are created).
    //   hCursor  -- the mouse cursor shown over this window.
    //   hbrBackground -- background brush; NULL_BRUSH = we clear manually (D3D11).
    //   lpfnWndProc -- pointer to our WndProc message handler.
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.style         = CS_CLASSDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;  // D3D11 clears the background each frame
    wc.lpszClassName = L"CreationSuiteEditor";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"Creation Suite Editor -- Game Engine for Teaching",
        WS_OVERLAPPEDWINDOW,
        100, 100,   // initial position
        1280, 720,  // initial size (HD, standard for tools)
        nullptr, nullptr, hInstance, nullptr
    );

    // ---- 2. Create D3D11 device + swap chain --------------------------------
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, hInstance);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // ---- 3. Initialise Dear ImGui ------------------------------------------
    // TEACHING NOTE -- ImGui Context
    // IMGUI_CHECKVERSION() verifies the ImGui headers and library are the same
    // version (catches mismatched ABI at runtime, not just compile time).
    // CreateContext() allocates the global ImGuiContext -- ONE per application.
    // IO flags: NavEnableKeyboard lets keyboard navigate menus/widgets.
    // ConfigFlags |= ImGuiConfigFlags_DockingEnable enables the docking API.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;  // enables dockable panels
    // Persist the UI layout (dock positions, window sizes) between sessions.
    // ImGui writes an .ini file next to the executable:
    io.IniFilename = "creation-suite-editor.ini";

    ImGui::StyleColorsDark();  // dark theme -- easier on the eyes for long work sessions

    // TEACHING NOTE -- ImGui Backends
    // ImGui itself is platform-agnostic -- it only produces draw calls.
    // "Backends" translate those draw calls to a specific platform/renderer:
    //   imgui_impl_win32 -- translates Win32 messages to ImGui input events.
    //   imgui_impl_dx11  -- renders ImGui draw data via D3D11 draw calls.
    // Init order: Win32 backend first (it sets up fonts), then D3D11.
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // ---- 4. Create the editor application ----------------------------------
    EditorApp editorApp;

    // ---- 5. Main message / render loop ------------------------------------
    // TEACHING NOTE -- The Game/Editor Loop
    // An editor loop mirrors a game loop:
    //   1. Poll OS messages   -- keyboard, mouse, resize, close.
    //   2. Begin frame        -- tell ImGui a new frame starts.
    //   3. Build UI           -- call ImGui:: functions to define all panels.
    //   4. Render             -- ImGui compiles draw lists; D3D11 executes them.
    //   5. Present            -- swap chain shows the rendered frame.
    //
    // This is the same structure as engine_sandbox's main loop --
    // editors and games share the same fundamental architecture.
    bool done = false;
    while (!done)
    {
        // -- Poll messages ---------------------------------------------------
        MSG msg = {};
        while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done) break;

        // -- Begin ImGui frame -----------------------------------------------
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // -- Render all editor panels ----------------------------------------
        editorApp.Render();

        // -- Finalise ImGui draw lists and execute D3D11 commands ------------
        ImGui::Render();

        // Clear the back buffer to a dark background colour
        constexpr float clearColor[4] = { 0.12f, 0.12f, 0.12f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clearColor);

        // Execute ImGui's D3D11 draw commands
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // -- Present the frame -----------------------------------------------
        // TEACHING NOTE -- DXGI Present
        // Present(1, 0) = vsync ON (swap every 1 monitor refresh).
        // Present(0, 0) = vsync OFF (swap immediately, may tear).
        // For an editor vsync ON is correct: no need to run faster than 60fps.
        g_pSwapChain->Present(1, 0);
    }

    // ---- 6. Cleanup (reverse order of init) --------------------------------
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, hInstance);
    return 0;
}

// ---------------------------------------------------------------------------
// D3D11 helpers
// ---------------------------------------------------------------------------

// TEACHING NOTE -- CreateDeviceAndSwapChain
// D3D11CreateDeviceAndSwapChain is a single-call way to:
//   a) Create an ID3D11Device (the GPU abstraction -- create resources).
//   b) Create an ID3D11DeviceContext (issue draw/compute commands).
//   c) Create an IDXGISwapChain (manage front/back buffers).
// DXGI_SWAP_EFFECT_DISCARD is the classic single-buffered swap mode.
// For the editor we use D3D_FEATURE_LEVEL_10_0 minimum (same as engine).
static bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Width                   = 0;   // 0 = match window width
    sd.BufferDesc.Height                  = 0;   // 0 = match window height
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = hWnd;
    sd.SampleDesc.Count                   = 1;   // no MSAA for the editor UI
    sd.SampleDesc.Quality                 = 0;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,  // minimum -- same as engine (GT610 compatible)
    };

    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr,                         // adapter (nullptr = default)
        D3D_DRIVER_TYPE_HARDWARE,        // hardware GPU
        nullptr,                         // software rasterizer (not used)
        0,                               // creation flags
        featureLevelArray,
        static_cast<UINT>(std::size(featureLevelArray)),
        D3D11_SDK_VERSION,
        &sd,
        &g_pSwapChain,
        &g_pd3dDevice,
        &featureLevel,
        &g_pd3dDeviceContext
    );

    // TEACHING NOTE -- WARP software fallback
    // If hardware creation fails (e.g. in CI or on a machine without D3D11 GPU),
    // fall back to D3D_DRIVER_TYPE_WARP -- Microsoft's software rasteriser.
    // WARP is slow but correct -- perfect for automated testing.
    if (FAILED(res))
    {
        res = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
            featureLevelArray, static_cast<UINT>(std::size(featureLevelArray)),
            D3D11_SDK_VERSION, &sd, &g_pSwapChain,
            &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext
        );
        if (FAILED(res)) return false;
    }

    CreateRenderTarget();
    return true;
}

static void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer)
    {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

static void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

static void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain)        { g_pSwapChain->Release();        g_pSwapChain        = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release();  g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice)        { g_pd3dDevice->Release();         g_pd3dDevice        = nullptr; }
}
