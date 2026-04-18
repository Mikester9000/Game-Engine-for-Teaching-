/**
 * @file vulkan_pipeline.hpp
 * @brief Graphics pipeline state object (PSO) creation and management.
 *
 * ============================================================================
 * TEACHING NOTE — What is a Graphics Pipeline?
 * ============================================================================
 * In Vulkan the "pipeline" is a large baked state object that captures
 * everything the GPU needs to know to execute a draw call:
 *
 *   • Shader stages   — which vertex/fragment/… programs to run.
 *   • Vertex input    — layout of the vertex buffer data.
 *   • Input assembly  — how vertices form primitives (triangles, lines, …).
 *   • Viewport/scissor — rendered area (we use dynamic state so it survives
 *                        window resizes without recreating the pipeline).
 *   • Rasterisation   — fill vs wireframe, culling, line width, depth bias.
 *   • Colour blending — how the fragment output is blended with the existing
 *                       framebuffer pixel.
 *
 * In OpenGL, state is a global bag of switches changed at any time.  Vulkan's
 * PSO approach forces you to commit all relevant state upfront, letting the
 * driver compile one optimal native-ISA shader variant — no runtime surprises.
 *
 * ============================================================================
 * TEACHING NOTE — Pipeline Layout
 * ============================================================================
 * The VkPipelineLayout declares how shader resources (uniform buffers,
 * textures) are bound — via descriptor set layouts and push constants.
 *
 * For M1 (a simple coloured triangle) we need NO resources at all: vertex
 * data comes from the vertex buffer, colour from interpolated attributes.
 * So our pipeline layout is empty (zero descriptor sets, zero push constants).
 *
 * ============================================================================
 * TEACHING NOTE — SPIR-V Shader Loading
 * ============================================================================
 * Vulkan does not accept GLSL directly.  Shaders must be compiled to SPIR-V:
 *
 *   glslc triangle.vert -o triangle.vert.spv   (Vulkan SDK tool)
 *
 * At runtime we load the .spv file as a raw byte array and pass it to
 * vkCreateShaderModule.  The driver compiles SPIR-V to GPU machine code at
 * that point (or lazily during pipeline creation on some drivers).
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Requires: Vulkan SDK
 */

#pragma once

#ifndef VK_USE_PLATFORM_WIN32_KHR
    #define VK_USE_PLATFORM_WIN32_KHR
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <windows.h>
#include <vulkan/vulkan.h>

#include <string>
#include <vector>
#include <cstdint>

namespace engine {
namespace rendering {

// ===========================================================================
// class VulkanPipeline
// ===========================================================================
// Owns the VkPipeline (PSO) and the VkPipelineLayout.
//
// TEACHING NOTE — Single Responsibility
// VulkanPipeline only creates and destroys the PSO.  It does NOT own the
// render pass (that is VulkanRenderer's responsibility).  This separation
// lets us swap pipelines without touching the render pass.
// ===========================================================================
class VulkanPipeline
{
public:
    VulkanPipeline()  = default;
    ~VulkanPipeline() { Destroy(); }

    VulkanPipeline(const VulkanPipeline&)            = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    /**
     * @brief Create the graphics pipeline from compiled SPIR-V shaders.
     *
     * @param device         Logical device.
     * @param renderPass     The render pass this pipeline will be used with.
     * @param vertSpvPath    Full path to the compiled vertex shader (.spv).
     * @param fragSpvPath    Full path to the compiled fragment shader (.spv).
     * @return true on success.
     *
     * TEACHING NOTE — Why Pass the Render Pass?
     * The pipeline must be compatible with a specific render pass subpass —
     * the attachment formats and sample counts must match.  This is Vulkan's
     * way of ensuring the pipeline was compiled for the right framebuffer format.
     */
    bool Create(VkDevice            device,
                VkRenderPass        renderPass,
                const std::string&  vertSpvPath,
                const std::string&  fragSpvPath);

    /**
     * @brief Destroy the pipeline and pipeline layout.
     *
     * Safe to call multiple times.
     */
    void Destroy();

    // -----------------------------------------------------------------------
    // Per-frame interface
    // -----------------------------------------------------------------------

    /**
     * @brief Bind this pipeline to the command buffer.
     *
     * Must be called inside an active render pass.
     *
     * TEACHING NOTE — VK_PIPELINE_BIND_POINT_GRAPHICS
     * Vulkan has separate pipeline bind points for graphics, compute, and
     * ray-tracing pipelines.  Graphics pipelines are bound to the GRAPHICS
     * bind point.
     *
     * @param commandBuffer  Recording command buffer.
     */
    void Bind(VkCommandBuffer commandBuffer) const;

    /** @return true if Create() succeeded. */
    bool IsValid() const { return m_initialised; }

private:
    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Load a SPIR-V file into a byte vector.
     *
     * @param path  Absolute or relative path to the .spv file.
     * @return Byte vector containing the SPIR-V module; empty on error.
     *
     * TEACHING NOTE — Binary File Read
     * SPIR-V is a binary format (not text).  We must open with std::ios::binary
     * to avoid the C standard library's newline translation on Windows, which
     * would corrupt the binary data (0x0D 0x0A → 0x0A would mis-align words).
     */
    static std::vector<char> LoadSpvFile(const std::string& path);

    /**
     * @brief Wrap a SPIR-V byte array in a VkShaderModule.
     *
     * @param device  Logical device.
     * @param code    SPIR-V byte array from LoadSpvFile().
     * @return VK_NULL_HANDLE on failure.
     *
     * TEACHING NOTE — VkShaderModule Lifetime
     * A shader module can be destroyed immediately after pipeline creation —
     * the driver has copied what it needs.  We destroy both modules at the
     * end of Create() to avoid holding onto them unnecessarily.
     */
    static VkShaderModule CreateShaderModule(VkDevice                  device,
                                             const std::vector<char>&  code);

    // -----------------------------------------------------------------------
    // Data members
    // -----------------------------------------------------------------------

    VkDevice         m_device         = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline       m_pipeline       = VK_NULL_HANDLE;
    bool             m_initialised    = false;
};

} // namespace rendering
} // namespace engine
