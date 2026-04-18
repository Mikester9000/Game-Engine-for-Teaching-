/**
 * @file VulkanRenderer.cpp
 * @brief Minimal Vulkan renderer implementation — "clear screen" starter.
 *
 * ============================================================================
 * TEACHING NOTE — Reading This File
 * ============================================================================
 * Read the private helper functions in the order they are called from Init():
 *
 *   CreateInstance()       — §1
 *   SetupDebugMessenger()  — §2
 *   CreateSurface()        — §3
 *   PickPhysicalDevice()   — §4
 *   CreateLogicalDevice()  — §5
 *   CreateSwapchain()      — §6
 *   CreateImageViews()     — §7
 *   CreateRenderPass()     — §8
 *   CreateFramebuffers()   — §9
 *   CreateCommandPool()    — §10
 *   CreateCommandBuffers() — §11
 *   CreateSyncObjects()    — §12
 *
 * Then read DrawFrame() to see how a single frame is rendered.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 */

#include "engine/rendering/vulkan/VulkanRenderer.hpp"
#include "engine/rendering/vulkan/vulkan_pipeline.hpp"
#include "engine/rendering/vulkan/vulkan_mesh.hpp"

#include <iostream>
#include <stdexcept>
#include <set>
#include <limits>
#include <algorithm>
#include <cstring>    // strcmp

namespace engine {
namespace rendering {

// ---------------------------------------------------------------------------
// Constructor — defined here (not in the header) so that MSVC does not try to
// instantiate unique_ptr<VulkanMesh>::~unique_ptr() at the point where the
// class body is parsed (where VulkanMesh is only forward-declared).  Defining
// = default here, after the full includes above, satisfies the "complete type"
// requirement.
VulkanRenderer::VulkanRenderer() = default;

// ---------------------------------------------------------------------------
// Destructor — defined here (not in the header) because m_pipeline and
// m_triangleMesh are unique_ptr<T> with T only forward-declared in the
// header.  The complete types VulkanPipeline and VulkanMesh are included
// above, so the compiler can generate unique_ptr<T>::~unique_ptr() here.
// ---------------------------------------------------------------------------
VulkanRenderer::~VulkanRenderer()
{
    Shutdown();
}

// ---------------------------------------------------------------------------
// Convenience macro for Vulkan error checking.
// TEACHING NOTE — VK_CHECK
// Vulkan functions return VkResult.  VK_SUCCESS = 0; anything else is a
// warning or error.  This macro prints the failing call and returns false
// from the enclosing function, which propagates the failure up to Init().
// ---------------------------------------------------------------------------
#define VK_CHECK(call)                                                     \
    do {                                                                   \
        VkResult _result = (call);                                         \
        if (_result != VK_SUCCESS) {                                       \
            std::cerr << "[VulkanRenderer] " #call " failed with VkResult=" \
                      << _result << " (" << __FILE__ << ":" << __LINE__    \
                      << ")\n";                                            \
            return false;                                                  \
        }                                                                  \
    } while (0)

// ===========================================================================
// §1 — CreateInstance
// ===========================================================================
// TEACHING NOTE — VkInstance
// The instance is the connection between your application and the Vulkan
// runtime library (vulkan-1.dll on Windows).  It holds global state (layers,
// extensions) and is the root object from which all other Vulkan objects
// are created.
//
// Required instance extensions for a Win32 window:
//   VK_KHR_surface           — base surface extension (platform-agnostic)
//   VK_KHR_win32_surface     — Win32-specific surface creation
//   VK_EXT_debug_utils       — debug callbacks (Debug builds only)
// ===========================================================================
bool VulkanRenderer::CreateInstance()
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — VkApplicationInfo
    // This struct is purely informational — the driver uses it for crash
    // reports and optimisation hints.  The important field is apiVersion:
    // request Vulkan 1.2 which is widely supported on modern hardware.
    // -----------------------------------------------------------------------
    VkApplicationInfo appInfo  = {};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "EngineSandbox";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "Educational Game Engine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_2;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Validation Layer Check
    // Before requesting validation layers, confirm the Vulkan runtime actually
    // has them installed.  If the Vulkan SDK is not installed, or the SDK
    // version is too old, the layer list may be empty.
    //
    // We perform this check BEFORE building the extension list so we know
    // whether to request VK_EXT_debug_utils (only needed when layers are on).
    //
    // When layers are unavailable we degrade gracefully: log a warning and
    // continue without validation.  This lets headless CI runners (which have
    // the Vulkan loader but not the full SDK) pass the bootstrap test while
    // developers on a full Vulkan SDK installation still get validation.
    // -----------------------------------------------------------------------
    if (kEnableValidationLayers)
    {
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> available(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, available.data());

        for (const char* name : kValidationLayers)
        {
            bool found = false;
            for (const auto& lp : available)
            {
                if (strcmp(name, lp.layerName) == 0) { found = true; break; }
            }
            if (!found)
            {
                std::cerr << "[VulkanRenderer] Warning: validation layer not available: "
                          << name << " — running without validation.\n"
                          << "  Install the Vulkan SDK from https://vulkan.lunarg.com/"
                             " for full debug support.\n";
                m_validationLayersActive = false;
                break;
            }
        }
    }

    // -----------------------------------------------------------------------
    // Collect required instance extensions.
    // VK_EXT_debug_utils is only requested when validation layers are active,
    // because the debug messenger depends on the validation layer being present.
    // -----------------------------------------------------------------------
    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    };
    if (m_validationLayersActive)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkInstanceCreateInfo createInfo    = {};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (m_validationLayersActive)
    {
        createInfo.enabledLayerCount   = static_cast<uint32_t>(kValidationLayers.size());
        createInfo.ppEnabledLayerNames = kValidationLayers.data();
    }
    else
    {
        createInfo.enabledLayerCount   = 0;
    }

    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_instance));

    std::cout << "[VulkanRenderer] Vulkan instance created.\n";
    return true;
}

// ===========================================================================
// §2 — SetupDebugMessenger
// ===========================================================================
// TEACHING NOTE — Extension Functions
// VK_EXT_debug_utils adds new Vulkan functions that are not in the core API.
// You must load them at runtime via vkGetInstanceProcAddr (similar to
// GetProcAddress on Win32 or dlsym on Linux).  We load them once here and
// use them to create/destroy the debug messenger.
// ===========================================================================
bool VulkanRenderer::SetupDebugMessenger()
{
    if (!m_validationLayersActive)
        return true;   // Nothing to do when validation layers are off.

    // -----------------------------------------------------------------------
    // Load the extension functions dynamically.
    // -----------------------------------------------------------------------
    auto pfnCreate = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));

    if (!pfnCreate)
    {
        std::cerr << "[VulkanRenderer] Could not load vkCreateDebugUtilsMessengerEXT.\n";
        return false;
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Message Severity and Type Filters
    // We subscribe to ERROR and WARNING messages from all message types
    // (general Vulkan issues, validation layer checks, and performance hints).
    // VERBOSE / INFO floods the console so we leave them off by default.
    // -----------------------------------------------------------------------
    VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {};
    messengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    messengerInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    messengerInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT     |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT  |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    messengerInfo.pfnUserCallback = VulkanRenderer::DebugCallback;
    messengerInfo.pUserData       = nullptr;

    VkResult result = pfnCreate(m_instance, &messengerInfo, nullptr, &m_debugMessenger);
    if (result != VK_SUCCESS)
    {
        std::cerr << "[VulkanRenderer] vkCreateDebugUtilsMessengerEXT failed: " << result << "\n";
        return false;
    }

    std::cout << "[VulkanRenderer] Validation layers enabled.\n";
    return true;
}

// ===========================================================================
// §3 — CreateSurface
// ===========================================================================
// TEACHING NOTE — VkSurfaceKHR
// A surface is the abstract "window connection" that Vulkan uses to present
// rendered images.  On Windows we use VK_KHR_win32_surface to attach the
// surface directly to our HWND (window handle).
// ===========================================================================
bool VulkanRenderer::CreateSurface(HINSTANCE hinstance, HWND hwnd)
{
    VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
    surfaceInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.hinstance = hinstance;
    surfaceInfo.hwnd      = hwnd;

    VK_CHECK(vkCreateWin32SurfaceKHR(m_instance, &surfaceInfo, nullptr, &m_surface));

    std::cout << "[VulkanRenderer] Win32 surface created.\n";
    return true;
}

// ===========================================================================
// §4 — PickPhysicalDevice
// ===========================================================================
// TEACHING NOTE — Physical Device Selection
// A machine may have multiple GPUs (e.g. integrated + discrete).  We
// enumerate all of them and pick the first one that:
//   a) Has a graphics queue family.
//   b) Has a present queue family for our surface.
//   c) Supports the required extensions (e.g. VK_KHR_swapchain).
//   d) Has at least one swapchain format and one present mode.
// A production engine would score devices (prefer discrete over integrated,
// prefer larger VRAM, etc.) but for teaching "first suitable" is fine.
// ===========================================================================
bool VulkanRenderer::PickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        std::cerr << "[VulkanRenderer] No Vulkan-capable GPU found.\n";
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    for (const auto& device : devices)
    {
        if (IsDeviceSuitable(device))
        {
            m_physicalDevice = device;
            break;
        }
    }

    if (m_physicalDevice == VK_NULL_HANDLE)
    {
        std::cerr << "[VulkanRenderer] No suitable GPU found.\n";
        return false;
    }

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
    std::cout << "[VulkanRenderer] Using GPU: " << props.deviceName << "\n";
    return true;
}

// ===========================================================================
// §5 — CreateLogicalDevice
// ===========================================================================
// TEACHING NOTE — Logical Device vs Physical Device
// Physical device  = the actual GPU chip.
// Logical device   = your application's interface to that GPU.
//                    You choose which features and queues to enable.
//
// We create one logical device per physical device.  Most apps only have one.
// The logical device also gives us handles to the graphics and present queues.
// ===========================================================================
bool VulkanRenderer::CreateLogicalDevice()
{
    QueueFamilyIndices indices = FindQueueFamilies(m_physicalDevice);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Unique Queue Families
    // If the graphics family and present family are the same (common on
    // desktop GPUs) we only need one VkDeviceQueueCreateInfo.  Using a set
    // deduplicates the indices automatically.
    // -----------------------------------------------------------------------
    std::set<uint32_t> uniqueFamilies = {
        indices.graphicsFamily,
        indices.presentFamily
    };

    const float queuePriority = 1.0f;  // 0.0 = lowest, 1.0 = highest priority
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    for (uint32_t family : uniqueFamilies)
    {
        VkDeviceQueueCreateInfo queueInfo = {};
        queueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount       = 1;
        queueInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueInfo);
    }

    // TEACHING NOTE — VkPhysicalDeviceFeatures
    // This struct enables optional GPU features (geometry shaders, multi-viewport,
    // etc.).  We request none for the minimal clear-screen starter.
    VkPhysicalDeviceFeatures deviceFeatures = {};

    VkDeviceCreateInfo deviceInfo      = {};
    deviceInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pQueueCreateInfos       = queueCreateInfos.data();
    deviceInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size());
    deviceInfo.pEnabledFeatures        = &deviceFeatures;
    deviceInfo.enabledExtensionCount   = static_cast<uint32_t>(kDeviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = kDeviceExtensions.data();

    VK_CHECK(vkCreateDevice(m_physicalDevice, &deviceInfo, nullptr, &m_device));

    // Retrieve queue handles — index 0 because we created only one queue each.
    vkGetDeviceQueue(m_device, indices.graphicsFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, indices.presentFamily,  0, &m_presentQueue);

    std::cout << "[VulkanRenderer] Logical device created.\n";
    return true;
}

// ===========================================================================
// §6 — CreateSwapchain
// ===========================================================================
// TEACHING NOTE — Swapchain
// The swapchain is a ring of images.  The GPU renders into one image while
// the display controller scans out another.  When the rendered image is
// ready, "present" swaps it to the front so the user sees the new frame.
//
// Key swapchain parameters we choose:
//   format      — pixel layout (e.g. BGRA8_UNORM) and colour space (SRGB).
//   presentMode — how to handle frames: FIFO = vsync, Mailbox = triple buffer.
//   extent      — resolution of the swapchain images (matches window size).
//   imageCount  — how many images in the ring (min+1 to avoid stalling).
// ===========================================================================
bool VulkanRenderer::CreateSwapchain(uint32_t width, uint32_t height)
{
    SwapchainSupportDetails support = QuerySwapchainSupport(m_physicalDevice);

    VkSurfaceFormatKHR format      = ChooseSwapSurfaceFormat(support.formats);
    VkPresentModeKHR   presentMode = ChooseSwapPresentMode(support.presentModes);
    VkExtent2D         extent      = ChooseSwapExtent(support.capabilities, width, height);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Image Count
    // We request minImageCount + 1 to ensure the CPU can keep encoding while
    // the GPU is presenting the previous frame (pipelining).  Clamp to
    // maxImageCount (0 = no limit).
    // -----------------------------------------------------------------------
    uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 &&
        imageCount > support.capabilities.maxImageCount)
    {
        imageCount = support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapInfo = {};
    swapInfo.sType           = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapInfo.surface         = m_surface;
    swapInfo.minImageCount   = imageCount;
    swapInfo.imageFormat     = format.format;
    swapInfo.imageColorSpace = format.colorSpace;
    swapInfo.imageExtent     = extent;
    swapInfo.imageArrayLayers = 1;   // Always 1 unless rendering stereo VR
    swapInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Queue Sharing Mode
    // If graphics and present queues are from different families, images
    // must be shared (VK_SHARING_MODE_CONCURRENT).  If they are the same
    // family, exclusive is more efficient.
    // -----------------------------------------------------------------------
    QueueFamilyIndices indices = FindQueueFamilies(m_physicalDevice);
    uint32_t queueFamilyIndices[] = {
        indices.graphicsFamily,
        indices.presentFamily
    };

    if (indices.graphicsFamily != indices.presentFamily)
    {
        swapInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        swapInfo.queueFamilyIndexCount = 2;
        swapInfo.pQueueFamilyIndices   = queueFamilyIndices;
    }
    else
    {
        swapInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
        swapInfo.queueFamilyIndexCount = 0;
        swapInfo.pQueueFamilyIndices   = nullptr;
    }

    swapInfo.preTransform   = support.capabilities.currentTransform;
    swapInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapInfo.presentMode    = presentMode;
    swapInfo.clipped        = VK_TRUE;   // GPU can skip pixels occluded by other windows
    swapInfo.oldSwapchain   = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(m_device, &swapInfo, nullptr, &m_swapchain));

    // Retrieve the actual image handles (Vulkan allocates them internally).
    uint32_t actualCount = 0;
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &actualCount, nullptr);
    m_swapchainImages.resize(actualCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &actualCount, m_swapchainImages.data());

    m_swapchainFormat = format.format;
    m_swapchainExtent = extent;

    std::cout << "[VulkanRenderer] Swapchain created (" << extent.width
              << "x" << extent.height << ", " << actualCount << " images).\n";
    return true;
}

// ===========================================================================
// §7 — CreateImageViews
// ===========================================================================
// TEACHING NOTE — VkImageView
// A VkImage is raw GPU memory.  A VkImageView describes HOW to interpret
// that memory — which mip-levels, array layers, and format to use.  You
// never bind a VkImage directly to a framebuffer; you always use an image view.
// ===========================================================================
bool VulkanRenderer::CreateImageViews()
{
    m_swapchainImageViews.resize(m_swapchainImages.size());

    for (size_t i = 0; i < m_swapchainImages.size(); ++i)
    {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image    = m_swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format   = m_swapchainFormat;

        // TEACHING NOTE — Component Swizzle
        // We use IDENTITY so R→R, G→G, B→B, A→A.  You could remap channels
        // here (e.g. monochrome by routing R into all channels).
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        // subresourceRange selects which mip-levels and array layers the
        // view covers.  For a simple 2D colour image: base mip 0, 1 level, 1 layer.
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        VkResult res = vkCreateImageView(m_device, &viewInfo, nullptr,
                                          &m_swapchainImageViews[i]);
        if (res != VK_SUCCESS)
        {
            std::cerr << "[VulkanRenderer] vkCreateImageView failed for index "
                      << i << ": " << res << "\n";
            return false;
        }
    }
    return true;
}

// ===========================================================================
// §8 — CreateRenderPass
// ===========================================================================
// TEACHING NOTE — Render Pass
// A render pass describes the "shape" of a rendering operation:
//   • What attachments are written to (colour, depth, stencil)?
//   • How should they be loaded at the start of the pass (CLEAR / LOAD / DONT_CARE)?
//   • How should they be stored at the end (STORE / DONT_CARE)?
//   • How does the GPU transition image layouts through the pass?
//
// For our clear-screen demo we have ONE colour attachment.
// LOAD_OP_CLEAR means "clear the attachment to a solid colour at pass start"
// — this is how the background colour is produced.
// ===========================================================================
bool VulkanRenderer::CreateRenderPass()
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — VkAttachmentDescription
    // format       — must match the swapchain image format.
    // samples      — MSAA sample count; 1 = no anti-aliasing.
    // loadOp       — CLEAR: GPU zeros the attachment at render pass start.
    // storeOp      — STORE: GPU writes results to memory (needed for present).
    // initialLayout — image layout BEFORE the pass begins.
    // finalLayout   — image layout AFTER the pass.
    //   PRESENT_SRC_KHR = layout required for vkQueuePresentKHR.
    // -----------------------------------------------------------------------
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format         = m_swapchainFormat;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Subpass
    // A render pass can have multiple "subpasses" (e.g. G-buffer fill, then
    // lighting), allowing the GPU driver to optimise data flow between them
    // using "tile memory" on mobile GPUs.  For our demo, one subpass suffices.
    // -----------------------------------------------------------------------
    VkAttachmentReference colorRef = {};
    colorRef.attachment = 0;   // Index into the attachment descriptions array
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Subpass Dependency
    // Subpass dependencies declare memory and execution ordering between
    // subpasses (or between the render pass and external commands).
    //
    // Here we express: "wait until the previous frame's image has been
    // released from the present engine (COLOR_ATTACHMENT_OUTPUT stage) before
    // writing colour to the attachment in this frame."
    // -----------------------------------------------------------------------
    VkSubpassDependency dependency = {};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;   // "before this render pass"
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo = {};
    rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments    = &colorAttachment;
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies   = &dependency;

    VK_CHECK(vkCreateRenderPass(m_device, &rpInfo, nullptr, &m_renderPass));
    return true;
}

// ===========================================================================
// §9 — CreateFramebuffers
// ===========================================================================
// TEACHING NOTE — Framebuffer
// A framebuffer is the collection of image views that the render pass writes
// to.  We create one framebuffer per swapchain image view so the GPU knows
// WHICH image to write to for each acquired swapchain slot.
// ===========================================================================
bool VulkanRenderer::CreateFramebuffers()
{
    m_framebuffers.resize(m_swapchainImageViews.size());

    for (size_t i = 0; i < m_swapchainImageViews.size(); ++i)
    {
        VkImageView attachments[] = { m_swapchainImageViews[i] };

        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = m_renderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments    = attachments;
        fbInfo.width           = m_swapchainExtent.width;
        fbInfo.height          = m_swapchainExtent.height;
        fbInfo.layers          = 1;

        VkResult res = vkCreateFramebuffer(m_device, &fbInfo, nullptr, &m_framebuffers[i]);
        if (res != VK_SUCCESS)
        {
            std::cerr << "[VulkanRenderer] vkCreateFramebuffer failed: " << res << "\n";
            return false;
        }
    }
    return true;
}

// ===========================================================================
// §10 — CreateCommandPool
// ===========================================================================
// TEACHING NOTE — Command Pool
// Commands are recorded into VkCommandBuffers, which are allocated from a
// VkCommandPool.  The pool manages a chunk of GPU memory for command storage.
// Command pools are NOT thread-safe; each thread that records commands needs
// its own pool.  For this single-threaded demo, one pool is fine.
//
// VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT allows us to reset and
// re-record individual command buffers rather than resetting the entire pool.
// ===========================================================================
bool VulkanRenderer::CreateCommandPool()
{
    QueueFamilyIndices indices = FindQueueFamilies(m_physicalDevice);

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = indices.graphicsFamily;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_CHECK(vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool));
    return true;
}

// ===========================================================================
// §11 — CreateCommandBuffers
// ===========================================================================
// TEACHING NOTE — Command Buffers
// We allocate one command buffer per swapchain image.  In DrawFrame we will
// pick the command buffer that corresponds to the acquired image index,
// reset it, and record a new clear command into it.
//
// VK_COMMAND_BUFFER_LEVEL_PRIMARY means these can be submitted to a queue
// directly.  "Secondary" command buffers are called from primaries
// (useful for multi-threaded recording, out of scope here).
// ===========================================================================
bool VulkanRenderer::CreateCommandBuffers()
{
    m_commandBuffers.resize(m_swapchainImages.size());

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = m_commandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

    VK_CHECK(vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()));
    return true;
}

// ===========================================================================
// §12 — CreateSyncObjects
// ===========================================================================
// TEACHING NOTE — Synchronisation Overview
// Vulkan gives you explicit, fine-grained synchronisation.  We use:
//
//   VkSemaphore — GPU-side signal.  Used to order GPU operations:
//     • m_imageAvailableSemaphores[f]: "the swapchain image is ready to write to"
//       (signalled by vkAcquireNextImageKHR, waited on by vkQueueSubmit).
//     • m_renderFinishedSemaphores[f]: "the GPU has finished rendering"
//       (signalled by vkQueueSubmit, waited on by vkQueuePresentKHR).
//
//   VkFence — CPU-visible gate.  Used to prevent the CPU from submitting
//     frame N+MAX while the GPU is still processing it:
//     • m_inFlightFences[f]: signalled when the GPU finishes the frame.
//       The CPU waits on it at the START of DrawFrame.
//
// TEACHING NOTE — Frames in Flight vs Swapchain Image Index
// m_currentFrame cycles through 0 … kMaxFramesInFlight-1 (the "slot").
// imageIndex is the index returned by vkAcquireNextImageKHR (may be
// different from m_currentFrame if the swapchain has more images).
// m_imagesInFlight[imageIndex] tracks which in-flight fence was LAST used
// for that swapchain image, to avoid writing to an image still being presented.
// ===========================================================================
bool VulkanRenderer::CreateSyncObjects()
{
    m_imagesInFlight.assign(m_swapchainImages.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semInfo = {};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    // TEACHING NOTE — VK_FENCE_CREATE_SIGNALED_BIT
    // Fences start unsignalled by default.  If we wait on an unsignalled fence
    // before the first DrawFrame(), we deadlock.  Creating them pre-signalled
    // means the first vkWaitForFences returns immediately.
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        VkResult r1 = vkCreateSemaphore(m_device, &semInfo, nullptr, &m_imageAvailableSemaphores[i]);
        VkResult r2 = vkCreateSemaphore(m_device, &semInfo, nullptr, &m_renderFinishedSemaphores[i]);
        VkResult r3 = vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]);
        if (r1 != VK_SUCCESS || r2 != VK_SUCCESS || r3 != VK_SUCCESS)
        {
            std::cerr << "[VulkanRenderer] Failed to create sync objects for frame " << i << "\n";
            return false;
        }
    }
    return true;
}

// ===========================================================================
// Init — public entry point, calls all §1-§12 helpers in order.
// ===========================================================================
bool VulkanRenderer::Init(HINSTANCE hinstance, HWND hwnd, uint32_t width, uint32_t height)
{
    m_hinstance = hinstance;
    m_hwnd      = hwnd;

    if (!CreateInstance())        return false;
    if (!SetupDebugMessenger())   return false;
    if (!CreateSurface(hinstance, hwnd)) return false;
    if (!PickPhysicalDevice())    return false;
    if (!CreateLogicalDevice())   return false;
    if (!CreateSwapchain(width, height)) return false;
    if (!CreateImageViews())      return false;
    if (!CreateRenderPass())      return false;
    if (!CreateFramebuffers())    return false;
    if (!CreateCommandPool())     return false;
    if (!CreateCommandBuffers())  return false;
    if (!CreateSyncObjects())     return false;

    m_initialised = true;
    std::cout << "[VulkanRenderer] Initialisation complete.\n";
    return true;
}

// ===========================================================================
// DrawFrame
// ===========================================================================
// TEACHING NOTE — Per-Frame Rendering
// Every frame follows the same six steps (see DrawFrame() in VulkanRenderer.hpp).
// ===========================================================================
void VulkanRenderer::DrawFrame(float clearR, float clearG, float clearB)
{
    if (!m_initialised) return;

    // -----------------------------------------------------------------------
    // Step 1 — Wait for the previous use of this frame slot to finish.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — vkWaitForFences
    // We wait with a 1-second timeout.  In practice the GPU should finish
    // well within a single display refresh period.
    // -----------------------------------------------------------------------
    vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame],
                    VK_TRUE, UINT64_MAX);

    // -----------------------------------------------------------------------
    // Step 2 — Acquire the next swapchain image.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — vkAcquireNextImageKHR
    // This asks the swapchain "which image can I write to next?"
    // m_imageAvailableSemaphores is signalled when the image is truly ready
    // (i.e. the display has finished reading the previous frame from it).
    // The returned imageIndex might differ from m_currentFrame.
    // -----------------------------------------------------------------------
    uint32_t imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(
        m_device, m_swapchain, UINT64_MAX,
        m_imageAvailableSemaphores[m_currentFrame],
        VK_NULL_HANDLE,
        &imageIndex);

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        // Window was resized; swapchain is stale — recreate it.
        RECT rect;
        GetClientRect(m_hwnd, &rect);
        RecreateSwapchain(rect.right - rect.left, rect.bottom - rect.top);
        return;
    }
    // VK_SUBOPTIMAL_KHR means the swapchain still works but isn't optimal
    // (e.g. after a DPI change).  We continue rendering and recreate later.

    // -----------------------------------------------------------------------
    // If this swapchain image is still in use by a previous frame, wait.
    // -----------------------------------------------------------------------
    if (m_imagesInFlight[imageIndex] != VK_NULL_HANDLE)
    {
        vkWaitForFences(m_device, 1, &m_imagesInFlight[imageIndex],
                        VK_TRUE, UINT64_MAX);
    }
    m_imagesInFlight[imageIndex] = m_inFlightFences[m_currentFrame];

    // -----------------------------------------------------------------------
    // Step 3 — Reset the command buffer for this frame slot.
    // -----------------------------------------------------------------------
    vkResetCommandBuffer(m_commandBuffers[imageIndex], 0);

    // -----------------------------------------------------------------------
    // Step 4 — Record: begin render pass → (optional draw) → end render pass.
    // -----------------------------------------------------------------------
    RecordCommandBuffer(m_commandBuffers[imageIndex],
                        m_framebuffers[imageIndex],
                        clearR, clearG, clearB);

    // -----------------------------------------------------------------------
    // Step 5 — Submit to the graphics queue.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — vkQueueSubmit
    // We tell the GPU:
    //   "Wait on imageAvailableSemaphore (stage COLOR_ATTACHMENT_OUTPUT)
    //    before writing colour pixels, then execute the command buffer,
    //    then signal renderFinishedSemaphore."
    // After submission we reset and signal inFlightFence when done so
    // the next DrawFrame() call for this slot can proceed.
    // -----------------------------------------------------------------------
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &m_imageAvailableSemaphores[m_currentFrame];
    submitInfo.pWaitDstStageMask    = waitStages;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &m_commandBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &m_renderFinishedSemaphores[m_currentFrame];

    if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo,
                      m_inFlightFences[m_currentFrame]) != VK_SUCCESS)
    {
        std::cerr << "[VulkanRenderer] vkQueueSubmit failed.\n";
        return;
    }

    // -----------------------------------------------------------------------
    // Step 6 — Present.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — vkQueuePresentKHR
    // Wait for renderFinishedSemaphore (i.e. the GPU finished drawing), then
    // flip the swapchain image to the screen (or hand it back to the
    // compositor on platforms like Windows with DWM).
    // -----------------------------------------------------------------------
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &m_renderFinishedSemaphores[m_currentFrame];
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &m_swapchain;
    presentInfo.pImageIndices      = &imageIndex;

    VkResult presentResult = vkQueuePresentKHR(m_presentQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
    {
        RECT rect;
        GetClientRect(m_hwnd, &rect);
        RecreateSwapchain(rect.right - rect.left, rect.bottom - rect.top);
    }

    // Advance to the next frame slot (wraps around at kMaxFramesInFlight).
    m_currentFrame = (m_currentFrame + 1) % kMaxFramesInFlight;
}

// ===========================================================================
// RecordCommandBuffer (private helper)
// ===========================================================================
// TEACHING NOTE — Extracted Recording
// We moved command buffer recording into its own helper so:
//   1. DrawFrame() is shorter and easier to follow.
//   2. RecordHeadlessFrame() can validate recording without a full frame.
//
// The function records:
//   • vkBeginCommandBuffer
//   • vkCmdBeginRenderPass (LOAD_OP_CLEAR provides the animated background)
//   • vkCmdSetViewport / vkCmdSetScissor   (dynamic state, set each frame)
//   • vkCmdBindPipeline + vkCmdBindVertexBuffers + vkCmdDraw (if scene loaded)
//   • vkCmdEndRenderPass
//   • vkEndCommandBuffer
// ===========================================================================
void VulkanRenderer::RecordCommandBuffer(VkCommandBuffer cmdBuf,
                                         VkFramebuffer   framebuffer,
                                         float           clearR,
                                         float           clearG,
                                         float           clearB) const
{
    // Begin recording.
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmdBuf, &beginInfo);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — VkClearValue
    // LOAD_OP_CLEAR fills the attachment with this colour before any draw
    // commands run.  The animated RGB values produce the rainbow background.
    // -----------------------------------------------------------------------
    VkClearValue clearValue = {};
    clearValue.color.float32[0] = clearR;
    clearValue.color.float32[1] = clearG;
    clearValue.color.float32[2] = clearB;
    clearValue.color.float32[3] = 1.0f;  // alpha = fully opaque

    VkRenderPassBeginInfo rpBeginInfo = {};
    rpBeginInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.renderPass        = m_renderPass;
    rpBeginInfo.framebuffer       = framebuffer;
    rpBeginInfo.renderArea.offset = {0, 0};
    rpBeginInfo.renderArea.extent = m_swapchainExtent;
    rpBeginInfo.clearValueCount   = 1;
    rpBeginInfo.pClearValues      = &clearValue;

    vkCmdBeginRenderPass(cmdBuf, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // -----------------------------------------------------------------------
    // If a scene is loaded, draw it.
    // -----------------------------------------------------------------------
    if (m_pipeline && m_pipeline->IsValid() && m_triangleMesh)
    {
        // TEACHING NOTE — Dynamic Viewport and Scissor
        // Because we declared these as dynamic pipeline states, we set them
        // here in the command buffer instead of baking them into the PSO.
        // This means window resizes only need to update these commands, not
        // recreate the whole pipeline.
        VkViewport viewport = {};
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = static_cast<float>(m_swapchainExtent.width);
        viewport.height   = static_cast<float>(m_swapchainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = m_swapchainExtent;
        vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

        // Bind the graphics pipeline (PSO), then the mesh, then draw.
        m_pipeline->Bind(cmdBuf);
        m_triangleMesh->Draw(cmdBuf);
    }

    vkCmdEndRenderPass(cmdBuf);
    vkEndCommandBuffer(cmdBuf);
}

// ===========================================================================
// LoadScene — public
// ===========================================================================
// TEACHING NOTE — Scene Dispatch Pattern
// This thin dispatcher will grow as milestones add more scene types.
// Each scene name maps to a private loader that creates the appropriate
// pipeline, meshes, and resources for that scene.
// ===========================================================================
bool VulkanRenderer::LoadScene(const std::string& sceneName,
                               const std::string& shaderDir)
{
    if (sceneName == "triangle")
        return LoadTriangleScene(shaderDir);

    std::cerr << "[VulkanRenderer] Unknown scene: " << sceneName << "\n";
    return false;
}

// ===========================================================================
// LoadTriangleScene — private
// ===========================================================================
bool VulkanRenderer::LoadTriangleScene(const std::string& shaderDir)
{
    // -----------------------------------------------------------------------
    // Create the graphics pipeline.
    // -----------------------------------------------------------------------
    m_pipeline = std::make_unique<VulkanPipeline>();
    if (!m_pipeline->Create(m_device, m_renderPass,
                             shaderDir + "triangle.vert.spv",
                             shaderDir + "triangle.frag.spv"))
    {
        std::cerr << "[VulkanRenderer] Triangle pipeline creation failed.\n";
        m_pipeline.reset();
        return false;
    }

    // -----------------------------------------------------------------------
    // Upload the triangle vertex data.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — NDC Coordinates for a Visible Triangle
    // The triangle is positioned in Vulkan NDC space:
    //   top vertex    (0,  -0.5) = centre, near top   → red
    //   right vertex  (0.5, 0.5) = lower right        → green
    //   left vertex  (-0.5, 0.5) = lower left         → blue
    //
    // In Vulkan NDC, y = -1 is the TOP of the screen, y = +1 is the BOTTOM
    // (opposite of OpenGL).  These coordinates produce a triangle that is
    // centred and fills roughly 1/4 of the screen.
    // -----------------------------------------------------------------------
    const std::vector<Vertex> vertices = {
        {{ 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},  // top   — red
        {{ 0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},  // right — green
        {{-0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},  // left  — blue
    };

    m_triangleMesh = std::make_unique<VulkanMesh>();
    if (!m_triangleMesh->Create(m_device, m_physicalDevice, vertices))
    {
        std::cerr << "[VulkanRenderer] Triangle mesh creation failed.\n";
        m_triangleMesh.reset();
        m_pipeline.reset();
        return false;
    }

    std::cout << "[VulkanRenderer] Triangle scene loaded.\n";
    return true;
}

// ===========================================================================
// RecordHeadlessFrame — public
// ===========================================================================
// TEACHING NOTE — Headless Validation
// In CI (no display) we cannot present frames, but we CAN validate that:
//   • The pipeline was created successfully.
//   • The mesh was uploaded.
//   • A command buffer can be recorded with the draw call.
// This method records commandBuffer[0] using framebuffer[0] and a fixed
// black clear colour, then returns without submitting.
// ===========================================================================
bool VulkanRenderer::RecordHeadlessFrame()
{
    if (m_commandBuffers.empty() || m_framebuffers.empty())
    {
        std::cerr << "[VulkanRenderer] No command buffers / framebuffers for headless record.\n";
        return false;
    }

    // Reset command buffer 0 and re-record with the current pipeline/mesh.
    vkResetCommandBuffer(m_commandBuffers[0], 0);
    RecordCommandBuffer(m_commandBuffers[0], m_framebuffers[0],
                        0.0f, 0.0f, 0.0f);  // black clear colour

    return true;
}

// ===========================================================================
// RecreateSwapchain
// ===========================================================================
// TEACHING NOTE — Swapchain Recreation
// On window resize or DPI change, the old swapchain is invalidated.  We must:
//   1. Wait for the GPU to finish all in-flight work (vkDeviceWaitIdle).
//   2. Destroy swapchain-dependent objects (framebuffers, image views, swapchain).
//   3. Recreate them with the new dimensions.
// Command buffers and sync objects are NOT swapchain-dependent (they survive).
// ===========================================================================
void VulkanRenderer::RecreateSwapchain(uint32_t newWidth, uint32_t newHeight)
{
    // Minimised window — width or height can be 0; skip until restored.
    if (newWidth == 0 || newHeight == 0) return;

    vkDeviceWaitIdle(m_device);

    DestroySwapchainResources();

    CreateSwapchain(newWidth, newHeight);
    CreateImageViews();
    CreateFramebuffers();

    // Update per-image in-flight fence tracking array size.
    m_imagesInFlight.assign(m_swapchainImages.size(), VK_NULL_HANDLE);

    std::cout << "[VulkanRenderer] Swapchain recreated ("
              << newWidth << "x" << newHeight << ").\n";
}

// ===========================================================================
// DestroySwapchainResources (private helper)
// ===========================================================================
void VulkanRenderer::DestroySwapchainResources()
{
    for (auto fb : m_framebuffers)
        vkDestroyFramebuffer(m_device, fb, nullptr);
    m_framebuffers.clear();

    for (auto iv : m_swapchainImageViews)
        vkDestroyImageView(m_device, iv, nullptr);
    m_swapchainImageViews.clear();

    if (m_swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

// ===========================================================================
// Shutdown
// ===========================================================================
void VulkanRenderer::Shutdown()
{
    if (!m_initialised) return;
    m_initialised = false;

    // TEACHING NOTE — vkDeviceWaitIdle
    // Before destroying ANY Vulkan object, ensure the GPU has finished all
    // submitted work.  Destroying a semaphore or fence that the GPU is still
    // waiting on is undefined behaviour.
    vkDeviceWaitIdle(m_device);

    // Destroy in reverse creation order.

    // Scene / pipeline objects must be destroyed before the device.
    m_triangleMesh.reset();
    m_pipeline.reset();

    // Sync
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
    }

    // Commands
    if (!m_commandBuffers.empty())
    {
        vkFreeCommandBuffers(m_device, m_commandPool,
            static_cast<uint32_t>(m_commandBuffers.size()),
            m_commandBuffers.data());
        m_commandBuffers.clear();
    }
    if (m_commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }

    // Swapchain-dependent objects
    DestroySwapchainResources();

    // Render pass
    if (m_renderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }

    // Logical device (implicitly destroys queues)
    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    // Surface
    if (m_surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }

    // Debug messenger (must be destroyed before the instance)
    if (m_debugMessenger != VK_NULL_HANDLE)
    {
        auto pfnDestroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (pfnDestroy)
            pfnDestroy(m_instance, m_debugMessenger, nullptr);
        m_debugMessenger = VK_NULL_HANDLE;
    }

    // Instance
    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }

    std::cout << "[VulkanRenderer] Shutdown complete.\n";
}

// ===========================================================================
// DebugCallback — static
// ===========================================================================
VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderer::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT             /*messageType*/,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*                                       /*pUserData*/)
{
    // TEACHING NOTE — Only print ERROR and WARNING (we filtered INFO/VERBOSE
    // in SetupDebugMessenger, but this is a secondary guard).
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        std::cerr << "[VulkanValidation] " << pCallbackData->pMessage << "\n";
    }

    // Return VK_FALSE = don't abort the Vulkan call that triggered the message.
    // Return VK_TRUE would abort it (useful for testing error-handling code).
    return VK_FALSE;
}

// ===========================================================================
// Helper: FindQueueFamilies
// ===========================================================================
VulkanRenderer::QueueFamilyIndices
VulkanRenderer::FindQueueFamilies(VkPhysicalDevice device) const
{
    QueueFamilyIndices indices;

    uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());

    for (uint32_t i = 0; i < familyCount; ++i)
    {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphicsFamily = i;

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
        if (presentSupport)
            indices.presentFamily = i;

        if (indices.IsComplete()) break;
    }
    return indices;
}

// ===========================================================================
// Helper: QuerySwapchainSupport
// ===========================================================================
VulkanRenderer::SwapchainSupportDetails
VulkanRenderer::QuerySwapchainSupport(VkPhysicalDevice device) const
{
    SwapchainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
    if (formatCount)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount,
                                              details.formats.data());
    }

    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &modeCount, nullptr);
    if (modeCount)
    {
        details.presentModes.resize(modeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &modeCount,
                                                   details.presentModes.data());
    }

    return details;
}

// ===========================================================================
// Helper: ChooseSwapSurfaceFormat
// ===========================================================================
// TEACHING NOTE — Surface Format
// Prefer BGRA8_SRGB with SRGB colour space because:
//   • BGRA8 maps directly to the monitor's 8-bit-per-channel BGRA layout.
//   • sRGB colour space means the hardware automatically gamma-corrects the
//     final image so colours look correct on standard monitors.
// ===========================================================================
VkSurfaceFormatKHR VulkanRenderer::ChooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& available) const
{
    for (const auto& f : available)
    {
        if (f.format     == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return f;
        }
    }
    // Fallback: just use whatever the driver prefers.
    return available[0];
}

// ===========================================================================
// Helper: ChooseSwapPresentMode
// ===========================================================================
// TEACHING NOTE — Present Modes
//   FIFO           — VSync; images queued; guaranteed to exist on all drivers.
//   Mailbox        — Triple buffering; most recently rendered image is always
//                    shown; minimal latency without tearing.  Prefer if available.
//   Immediate      — No sync; frame is shown immediately; may tear.
// ===========================================================================
VkPresentModeKHR VulkanRenderer::ChooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& available) const
{
    for (const auto& mode : available)
    {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
            return mode;
    }
    // FIFO is always available (Vulkan spec guarantees it).
    return VK_PRESENT_MODE_FIFO_KHR;
}

// ===========================================================================
// Helper: ChooseSwapExtent
// ===========================================================================
// TEACHING NOTE — Swap Extent
// On high-DPI (HiDPI / Retina) displays, the framebuffer resolution can
// differ from the window's "logical" size.  We use the capabilities' current
// extent when available, otherwise clamp our desired size to the min/max range.
// ===========================================================================
VkExtent2D VulkanRenderer::ChooseSwapExtent(
    const VkSurfaceCapabilitiesKHR& capabilities,
    uint32_t desiredWidth, uint32_t desiredHeight) const
{
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        // The surface dictates the exact size (most platforms).
        return capabilities.currentExtent;
    }

    VkExtent2D actual = {desiredWidth, desiredHeight};
    actual.width  = std::clamp(actual.width,
                               capabilities.minImageExtent.width,
                               capabilities.maxImageExtent.width);
    actual.height = std::clamp(actual.height,
                               capabilities.minImageExtent.height,
                               capabilities.maxImageExtent.height);
    return actual;
}

// ===========================================================================
// Helper: CheckDeviceExtensionSupport
// ===========================================================================
bool VulkanRenderer::CheckDeviceExtensionSupport(VkPhysicalDevice device) const
{
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> available(extCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, available.data());

    for (const char* required : kDeviceExtensions)
    {
        bool found = false;
        for (const auto& ext : available)
        {
            if (strcmp(required, ext.extensionName) == 0) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

// ===========================================================================
// Helper: IsDeviceSuitable
// ===========================================================================
bool VulkanRenderer::IsDeviceSuitable(VkPhysicalDevice device) const
{
    QueueFamilyIndices indices = FindQueueFamilies(device);
    bool extensionsOk = CheckDeviceExtensionSupport(device);

    bool swapchainOk = false;
    if (extensionsOk)
    {
        SwapchainSupportDetails support = QuerySwapchainSupport(device);
        swapchainOk = !support.formats.empty() && !support.presentModes.empty();
    }

    return indices.IsComplete() && extensionsOk && swapchainOk;
}

} // namespace rendering
} // namespace engine
