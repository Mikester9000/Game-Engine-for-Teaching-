/**
 * @file VulkanRenderer.hpp
 * @brief Minimal Vulkan renderer bootstrap — "clear screen" starter.
 *
 * ============================================================================
 * TEACHING NOTE — Why Vulkan?
 * ============================================================================
 * Vulkan is a modern, explicit GPU API created by the Khronos Group.
 * Unlike OpenGL (which hides GPU state management behind a driver), Vulkan
 * makes EVERY detail explicit:
 *
 *   • You create the instance, device, queues, swapchain manually.
 *   • You record command buffers and submit them yourself.
 *   • You manage synchronisation with semaphores and fences.
 *   • You decide memory layouts, pipeline stages, and image transitions.
 *
 * This verbosity is a feature for engine programmers: zero surprises, zero
 * hidden driver work.  It also maps directly to how modern GPUs actually work.
 *
 * ============================================================================
 * TEACHING NOTE — Vulkan "Bootstrap" Pattern
 * ============================================================================
 * Every Vulkan application goes through the same setup sequence:
 *
 *   1.  Instance          — connects your app to the Vulkan runtime.
 *   2.  Debug messenger   — routes validation layer messages to our callback.
 *   3.  Surface           — a renderable window (here: VkSurfaceKHR via Win32).
 *   4.  Physical device   — pick the GPU you want to render on.
 *   5.  Logical device    — an interface to the physical device + queue handles.
 *   6.  Swapchain         — a ring of presentable images tied to the window.
 *   7.  Image views       — how Vulkan "sees" each swapchain image.
 *   8.  Render pass       — describes the attachments (color, depth) and how
 *                           they are loaded/stored each frame.
 *   9.  Framebuffers      — binds each image view to the render pass.
 *  10.  Command pool/buffers — recorded GPU work packets.
 *  11.  Sync primitives   — semaphores + fences for CPU↔GPU coordination.
 *
 * DrawFrame() ties it all together:
 *   acquire → record clear → submit → present
 *
 * ============================================================================
 * TEACHING NOTE — Validation Layers
 * ============================================================================
 * In Debug builds we enable VK_LAYER_KHRONOS_validation.  This layer runs
 * inside the Vulkan driver and checks every API call for misuse — wrong
 * argument types, missing barriers, use-after-free of handles, etc.
 * It has a real CPU cost, so it is disabled in Release builds.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Requires: Vulkan SDK (https://vulkan.lunarg.com/)
 */

#pragma once

// ---------------------------------------------------------------------------
// TEACHING NOTE — Vulkan + Win32 Surface Headers
// ---------------------------------------------------------------------------
// <vulkan/vulkan.h> — core Vulkan types and functions.
// VK_USE_PLATFORM_WIN32_KHR must be defined BEFORE including vulkan.h so that
// the Win32-specific declarations (VkWin32SurfaceCreateInfoKHR etc.) are
// included.  We define it here; the CMakeLists also passes it as a compile
// definition to be safe.
// ---------------------------------------------------------------------------
#define VK_USE_PLATFORM_WIN32_KHR
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <windows.h>
#include <vulkan/vulkan.h>

// Forward declarations — keeps this header lean.
// Full types are only needed in VulkanRenderer.cpp.
namespace engine { namespace rendering {
    class VulkanPipeline;
    class VulkanMesh;
} }

// Standard library
#include <memory>       // std::unique_ptr
#include <string>       // std::string
#include <vector>       // std::vector
#include <cstdint>      // uint32_t

namespace engine {
namespace rendering {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

// TEACHING NOTE — Frames in Flight
// "Frames in flight" means we allow the CPU to record frame N+1 while the
// GPU is still rendering frame N.  2 is a common choice: more than 2 adds
// latency without much throughput gain.
static constexpr uint32_t kMaxFramesInFlight = 2;

// ===========================================================================
// class VulkanRenderer
// ===========================================================================
// Owns the entire Vulkan object graph and exposes a simple Init/DrawFrame/
// Shutdown interface.  All Vulkan objects are member variables; Init() creates
// them in dependency order, Shutdown() destroys them in reverse order.
//
// TEACHING NOTE — Object Ownership
// Each VkXxx handle is just an opaque integer (64 bits on 64-bit platforms).
// Vulkan NEVER automatically destroys anything — you must call the matching
// vkDestroyXxx function.  Missing even one destroy call is a resource leak.
// We use explicit Shutdown() rather than destructors here so the order of
// destruction is visible and teachable.
// ===========================================================================
class VulkanRenderer
{
public:
    VulkanRenderer()  = default;
    ~VulkanRenderer() { Shutdown(); }

    // No copying — Vulkan handles are not reference-counted by default.
    VulkanRenderer(const VulkanRenderer&)            = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    /**
     * @brief Create all Vulkan objects.  Must be called once after the window exists.
     *
     * @param hinstance  Win32 HINSTANCE for the surface.
     * @param hwnd       Win32 HWND for the surface.
     * @param width      Initial swapchain width (pixels).
     * @param height     Initial swapchain height (pixels).
     * @return true on success, false if any Vulkan call fails.
     */
    bool Init(HINSTANCE hinstance, HWND hwnd, uint32_t width, uint32_t height);

    /**
     * @brief Destroy all Vulkan objects in reverse-creation order.
     *
     * Safe to call multiple times.
     */
    void Shutdown();

    // -----------------------------------------------------------------------
    // Per-frame interface
    // -----------------------------------------------------------------------

    /**
     * @brief Acquire an image, record a clear command, submit, and present.
     *
     * TEACHING NOTE — Frame Lifecycle
     *   1. vkWaitForFences        — stall the CPU until the GPU finished
     *                               the *previous* use of this frame slot.
     *   2. vkAcquireNextImageKHR  — ask the swapchain for the next image.
     *   3. vkResetCommandBuffer   — clear the previously recorded commands.
     *   4. Record: begin render pass (LOAD_OP_CLEAR = GPU clears for us),
     *              end render pass.
     *   5. vkQueueSubmit          — send the command buffer to the GPU.
     *   6. vkQueuePresentKHR      — flip the rendered image to the screen.
     *
     * @param clearR  Red channel of the clear colour   (0.0 – 1.0).
     * @param clearG  Green channel of the clear colour.
     * @param clearB  Blue channel of the clear colour.
     */
    void DrawFrame(float clearR, float clearG, float clearB);

    /**
     * @brief Load a named scene, creating its pipeline and geometry.
     *
     * Supported scene names for M1:
     *   "triangle" — coloured triangle using the graphics pipeline.
     *
     * @param sceneName   Identifies which scene to load.
     * @param shaderDir   Directory containing compiled .spv shader files.
     *                    Must end with a path separator (e.g. "shaders/").
     * @return true on success.
     *
     * TEACHING NOTE — Scene Loading Pattern
     * For M1 the "scene" is just a hardcoded triangle.  In later milestones
     * LoadScene will parse a scene JSON file from the asset DB and spawn
     * entities, load meshes, and wire up scripts.
     */
    bool LoadScene(const std::string& sceneName, const std::string& shaderDir);

    /**
     * @brief Record one command buffer without submitting or presenting.
     *
     * Used in headless validation mode to confirm "Draw recorded" without
     * needing a GPU present loop.  Only valid after a successful LoadScene().
     *
     * @return true if recording succeeded.
     */
    bool RecordHeadlessFrame();

    /**
     * @brief Recreate the swapchain after a window resize.
     *
     * Called automatically by DrawFrame when it detects VK_ERROR_OUT_OF_DATE_KHR
     * or VK_SUBOPTIMAL_KHR.  Also called explicitly by the application if the
     * Win32Window::WasResized() flag is set.
     *
     * @param newWidth   New client-area width in pixels.
     * @param newHeight  New client-area height in pixels.
     */
    void RecreateSwapchain(uint32_t newWidth, uint32_t newHeight);

private:
    // -----------------------------------------------------------------------
    // Private creation helpers
    // -----------------------------------------------------------------------
    bool CreateInstance();
    bool SetupDebugMessenger();
    bool CreateSurface(HINSTANCE hinstance, HWND hwnd);
    bool PickPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateSwapchain(uint32_t width, uint32_t height);
    bool CreateImageViews();
    bool CreateRenderPass();
    bool CreateFramebuffers();
    bool CreateCommandPool();
    bool CreateCommandBuffers();
    bool CreateSyncObjects();

    void DestroySwapchainResources();

    // -----------------------------------------------------------------------
    // Scene / pipeline helpers (M1+)
    // -----------------------------------------------------------------------

    /**
     * @brief Create the triangle pipeline and upload the triangle mesh.
     *
     * @param shaderDir  Directory containing triangle.vert.spv + triangle.frag.spv.
     * @return true on success.
     */
    bool LoadTriangleScene(const std::string& shaderDir);

    /**
     * @brief Record all swapchain command buffers with the current scene.
     *
     * Called after LoadScene() and after swapchain recreation so command
     * buffers always reflect the latest pipeline and clear colour.
     *
     * TEACHING NOTE — Pre-recording vs per-frame recording
     * For a fully static scene (triangle with no animated clear colour) we
     * could pre-record once.  We keep per-frame recording here for two reasons:
     *   1. DrawFrame already re-records to animate the clear colour.
     *   2. Pre-recording with the clear-colour animation would require a
     *      separate "dynamic clear" mechanism (push constants or a UBO).
     */
    void RecordCommandBuffer(VkCommandBuffer cmdBuf,
                             VkFramebuffer   framebuffer,
                             float           clearR,
                             float           clearG,
                             float           clearB) const;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Queue Family Indices
    // -----------------------------------------------------------------------
    // A "queue" in Vulkan is a submission channel to the GPU.  Different queues
    // can run in parallel.  Queues belong to "families" — a family declares
    // which operations it supports (graphics, compute, transfer, present).
    // We need at least one family that supports GRAPHICS and one that supports
    // PRESENT (they are often the same family on desktop hardware).
    // -----------------------------------------------------------------------
    struct QueueFamilyIndices
    {
        uint32_t graphicsFamily = UINT32_MAX;  ///< UINT32_MAX = not found.
        uint32_t presentFamily  = UINT32_MAX;

        bool IsComplete() const
        {
            return graphicsFamily != UINT32_MAX
                && presentFamily  != UINT32_MAX;
        }
    };

    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Swapchain Support Details
    // -----------------------------------------------------------------------
    // Before creating a swapchain we must query three things:
    //   capabilities — min/max image count, extent range, transform flags.
    //   formats      — pixel format + colour space combos the surface supports.
    //   presentModes — how images are queued for display (FIFO, Mailbox, etc.).
    // -----------------------------------------------------------------------
    struct SwapchainSupportDetails
    {
        VkSurfaceCapabilitiesKHR        capabilities = {};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR>   presentModes;
    };

    SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device) const;

    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& available) const;

    VkPresentModeKHR ChooseSwapPresentMode(
        const std::vector<VkPresentModeKHR>& available) const;

    VkExtent2D ChooseSwapExtent(
        const VkSurfaceCapabilitiesKHR& capabilities,
        uint32_t                         desiredWidth,
        uint32_t                         desiredHeight) const;

    bool IsDeviceSuitable(VkPhysicalDevice device) const;
    bool CheckDeviceExtensionSupport(VkPhysicalDevice device) const;

    // -----------------------------------------------------------------------
    // Vulkan object handles
    // -----------------------------------------------------------------------

    // Core
    VkInstance               m_instance        = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger   = VK_NULL_HANDLE;
    VkSurfaceKHR             m_surface          = VK_NULL_HANDLE;

    // Device
    VkPhysicalDevice         m_physicalDevice   = VK_NULL_HANDLE;
    VkDevice                 m_device           = VK_NULL_HANDLE;
    VkQueue                  m_graphicsQueue    = VK_NULL_HANDLE;
    VkQueue                  m_presentQueue     = VK_NULL_HANDLE;

    // Swapchain
    VkSwapchainKHR           m_swapchain        = VK_NULL_HANDLE;
    VkFormat                 m_swapchainFormat  = VK_FORMAT_UNDEFINED;
    VkExtent2D               m_swapchainExtent  = {0, 0};
    std::vector<VkImage>     m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    std::vector<VkFramebuffer> m_framebuffers;

    // Render pass
    VkRenderPass             m_renderPass       = VK_NULL_HANDLE;

    // Commands
    VkCommandPool            m_commandPool      = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;  // one per swapchain image

    // Synchronisation (indexed by frame-in-flight slot, not swapchain image)
    VkSemaphore  m_imageAvailableSemaphores[kMaxFramesInFlight] = {};
    VkSemaphore  m_renderFinishedSemaphores[kMaxFramesInFlight] = {};
    VkFence      m_inFlightFences[kMaxFramesInFlight]           = {};

    // Per-image fence tracking — prevents writing to a swapchain image that
    // is still being presented.
    std::vector<VkFence> m_imagesInFlight;

    uint32_t    m_currentFrame   = 0;   ///< Cycles 0 … kMaxFramesInFlight-1.
    bool        m_initialised    = false;

    // Saved HINSTANCE/HWND for swapchain recreation after resize.
    HINSTANCE   m_hinstance      = nullptr;
    HWND        m_hwnd           = nullptr;

    // -----------------------------------------------------------------------
    // Scene / pipeline objects (created by LoadScene, destroyed in Shutdown)
    // -----------------------------------------------------------------------
    // TEACHING NOTE — std::unique_ptr for Optional Subsystems
    // The pipeline and mesh may or may not exist (they are created only when
    // LoadScene() is called).  Using unique_ptr<T> models "optionally present"
    // cleanly: nullptr = not loaded, non-null = active.  The destructor auto-
    // calls Destroy() through the unique_ptr deleter.
    std::unique_ptr<VulkanPipeline> m_pipeline;    ///< Graphics PSO (null until LoadScene).
    std::unique_ptr<VulkanMesh>     m_triangleMesh; ///< Triangle geometry (null until LoadScene).

    // -----------------------------------------------------------------------
    // Required extensions / layers
    // -----------------------------------------------------------------------
    const std::vector<const char*> kDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

#ifdef NDEBUG
    static constexpr bool kEnableValidationLayers = false;
#else
    // TEACHING NOTE — Debug vs Release Validation
    // kEnableValidationLayers is a compile-time constant so the compiler can
    // dead-code-eliminate all validation setup in Release builds.
    static constexpr bool kEnableValidationLayers = true;
#endif

    const std::vector<const char*> kValidationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    // -----------------------------------------------------------------------
    // Debug messenger callback
    // -----------------------------------------------------------------------

    /**
     * @brief Called by the Vulkan validation layer for every message it generates.
     *
     * TEACHING NOTE — Debug Callback Signature
     * The signature must exactly match PFN_vkDebugUtilsMessengerCallbackEXT.
     * We print ERROR and WARNING messages; verbose INFO messages are ignored
     * to keep the output readable.
     */
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT             messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void*                                       pUserData);
};

} // namespace rendering
} // namespace engine
