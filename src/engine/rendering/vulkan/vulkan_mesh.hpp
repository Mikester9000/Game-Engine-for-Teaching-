/**
 * @file vulkan_mesh.hpp
 * @brief A simple GPU mesh — vertex buffer + draw call abstraction.
 *
 * ============================================================================
 * TEACHING NOTE — What is a "Mesh" in a GPU Engine?
 * ============================================================================
 * A mesh is the combination of:
 *   1. Geometry data — vertex positions, normals, UVs, colours, …
 *   2. GPU buffers   — the VkBuffer objects that hold the data on the GPU.
 *   3. Draw metadata — vertex count, index count, primitive topology, …
 *
 * For M1 we keep it minimal: one vertex buffer holding 3 coloured 2D vertices
 * (no index buffer, no depth, no normals).  This is the smallest mesh that
 * can demonstrate the full Vulkan graphics pipeline.
 *
 * ============================================================================
 * TEACHING NOTE — Vertex Layout
 * ============================================================================
 * The C++ Vertex struct must exactly match the GLSL shader inputs:
 *
 *   Vertex { float position[2]; float color[3]; }  — 5 floats = 20 bytes/vertex
 *
 *   GLSL:
 *     layout(location = 0) in vec2 inPosition;   // bytes 0-7
 *     layout(location = 1) in vec3 inColor;      // bytes 8-19
 *
 * VkVertexInputBindingDescription describes the stride (20 bytes per vertex).
 * VkVertexInputAttributeDescription describes each attribute's offset, format,
 * and binding point.  Mismatching these with the struct layout is a common
 * Vulkan bug that the validation layers will catch.
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

#include "engine/rendering/vulkan/vulkan_buffer.hpp"

#include <vector>
#include <array>
#include <cstdint>

namespace engine {
namespace rendering {

// ===========================================================================
// struct Vertex
// ===========================================================================
// TEACHING NOTE — Plain-Old-Data (POD) Vertex
// We use a simple C struct (no constructors, no virtual functions) because the
// GPU reads raw binary data.  The layout of this struct in memory must match
// exactly what the shader expects at each attribute location.
//
// offsetof() gives the byte offset of a field within the struct.  We use it
// in GetAttributeDescriptions() to tell Vulkan where each field starts.
// ===========================================================================
struct Vertex
{
    float position[2];  ///< NDC position: x ∈ [-1,+1], y ∈ [-1,+1].
    float color[3];     ///< Linear RGB colour: each channel 0.0 – 1.0.
};

// ===========================================================================
// class VulkanMesh
// ===========================================================================
// Owns the vertex buffer for a mesh and provides a Draw() call that binds
// the buffer and issues the GPU draw command.
// ===========================================================================
class VulkanMesh
{
public:
    VulkanMesh()  = default;
    ~VulkanMesh() { Destroy(); }

    VulkanMesh(const VulkanMesh&)            = delete;
    VulkanMesh& operator=(const VulkanMesh&) = delete;

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    /**
     * @brief Upload vertex data to a host-visible GPU buffer.
     *
     * @param device     Logical device.
     * @param physDevice Physical device (for memory-type selection).
     * @param vertices   CPU-side vertex array to upload.
     * @return true on success.
     */
    bool Create(VkDevice                    device,
                VkPhysicalDevice            physDevice,
                const std::vector<Vertex>&  vertices);

    /**
     * @brief Destroy the vertex buffer.  Safe to call multiple times.
     */
    void Destroy();

    // -----------------------------------------------------------------------
    // Per-frame interface
    // -----------------------------------------------------------------------

    /**
     * @brief Bind the vertex buffer and issue a draw call.
     *
     * Must be called inside an active render pass.
     *
     * TEACHING NOTE — vkCmdBindVertexBuffers
     * This command tells the GPU where to fetch vertex data.  We bind to
     * "binding 0" (matching the binding in GetBindingDescription).
     *
     * TEACHING NOTE — vkCmdDraw
     * For a non-indexed draw:
     *   vertexCount     — total vertices to process.
     *   instanceCount   — 1 = no instancing.
     *   firstVertex     — offset into the vertex buffer (0 = start).
     *   firstInstance   — offset for instance ID (0 = no instancing).
     *
     * @param commandBuffer  Active recording command buffer.
     */
    void Draw(VkCommandBuffer commandBuffer) const;

    // -----------------------------------------------------------------------
    // Pipeline input descriptors (used by VulkanPipeline at creation time)
    // -----------------------------------------------------------------------

    /**
     * @brief Return the vertex binding description for the Vertex struct.
     *
     * TEACHING NOTE — Vertex Binding
     * A "binding" is a slot into which a vertex buffer is plugged at
     * draw time (vkCmdBindVertexBuffers).  We use binding 0.
     * The stride is sizeof(Vertex) — how many bytes to advance per vertex.
     * inputRate VERTEX means "advance once per vertex" (as opposed to
     * INSTANCE which advances once per rendered instance).
     */
    static VkVertexInputBindingDescription GetBindingDescription();

    /**
     * @brief Return the attribute descriptions for the Vertex struct.
     *
     * TEACHING NOTE — Vertex Attributes
     * Each attribute maps a byte range in the vertex struct to a shader
     * input location.
     *   Attribute 0 (location 0): position — VK_FORMAT_R32G32_SFLOAT (vec2).
     *   Attribute 1 (location 1): color    — VK_FORMAT_R32G32B32_SFLOAT (vec3).
     *
     * VK_FORMAT_R32G32_SFLOAT means "two 32-bit signed floats" — it
     * re-uses texture format names because Vulkan has one format enum for
     * everything (pixels, vertex data, uniform data).
     */
    static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions();

private:
    VulkanBuffer m_vertexBuffer;
    uint32_t     m_vertexCount  = 0;
    bool         m_initialised  = false;
};

} // namespace rendering
} // namespace engine
