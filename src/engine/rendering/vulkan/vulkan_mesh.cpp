/**
 * @file vulkan_mesh.cpp
 * @brief VulkanMesh — vertex upload, binding descriptions, and draw call.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 */

#include "engine/rendering/vulkan/vulkan_mesh.hpp"

#include <iostream>

namespace engine {
namespace rendering {

// ===========================================================================
// Create
// ===========================================================================
bool VulkanMesh::Create(VkDevice                   device,
                        VkPhysicalDevice            physDevice,
                        const std::vector<Vertex>&  vertices)
{
    if (vertices.empty())
    {
        std::cerr << "[VulkanMesh] No vertices provided.\n";
        return false;
    }

    m_vertexCount = static_cast<uint32_t>(vertices.size());
    VkDeviceSize bufferSize = sizeof(Vertex) * vertices.size();

    // -----------------------------------------------------------------------
    // TEACHING NOTE — HOST_VISIBLE Vertex Buffer (M1 simplification)
    // -----------------------------------------------------------------------
    // We allocate the vertex buffer directly in host-visible memory so the
    // CPU can write to it without a staging buffer.  This is perfectly fine
    // for small static meshes.
    //
    // In M2+ we will introduce:
    //   1. A staging buffer  — temporary host-visible upload region.
    //   2. A device-local buffer — fast GPU-only memory.
    //   3. A vkCmdCopyBuffer  — GPU copy from staging → device-local.
    // The staging approach is mandatory for large assets (meshes from disk).
    // -----------------------------------------------------------------------
    bool ok = m_vertexBuffer.Create(
        device, physDevice, bufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    if (!ok)
    {
        std::cerr << "[VulkanMesh] Failed to create vertex buffer.\n";
        return false;
    }

    // Upload the vertex data.
    m_vertexBuffer.Upload(vertices.data(), bufferSize);

    m_initialised = true;
    std::cout << "[VulkanMesh] Uploaded " << m_vertexCount << " vertices ("
              << bufferSize << " bytes).\n";
    return true;
}

// ===========================================================================
// Destroy
// ===========================================================================
void VulkanMesh::Destroy()
{
    if (!m_initialised) return;
    m_initialised = false;
    m_vertexCount = 0;
    m_vertexBuffer.Destroy();
}

// ===========================================================================
// Draw
// ===========================================================================
// TEACHING NOTE — Binding and Drawing
// The command buffer must already have a pipeline bound before Draw() is
// called (see VulkanPipeline::Bind).  The sequence in a frame is:
//   1. vkCmdBeginRenderPass
//   2. vkCmdSetViewport / vkCmdSetScissor  (dynamic state)
//   3. vkCmdBindPipeline                   (pipeline::Bind)
//   4. vkCmdBindVertexBuffers              (here)
//   5. vkCmdDraw                           (here)
//   6. vkCmdEndRenderPass
// ===========================================================================
void VulkanMesh::Draw(VkCommandBuffer commandBuffer) const
{
    if (!m_initialised) return;

    // -----------------------------------------------------------------------
    // Bind the vertex buffer to binding slot 0.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Binding Slot
    // The "firstBinding" parameter (0) must match the binding in
    // GetBindingDescription().  The GPU uses this to know which vertex buffer
    // to read when it fetches attribute data.
    // -----------------------------------------------------------------------
    VkBuffer vertexBuffers[] = { m_vertexBuffer.GetBuffer() };
    VkDeviceSize offsets[]   = { 0 };

    vkCmdBindVertexBuffers(commandBuffer,
                           0,               // first binding slot
                           1,               // one buffer
                           vertexBuffers,
                           offsets);

    // -----------------------------------------------------------------------
    // Issue the draw call.
    // -----------------------------------------------------------------------
    vkCmdDraw(commandBuffer,
              m_vertexCount,  // vertex count
              1,              // instance count (no instancing)
              0,              // first vertex
              0);             // first instance
}

// ===========================================================================
// GetBindingDescription
// ===========================================================================
VkVertexInputBindingDescription VulkanMesh::GetBindingDescription()
{
    // TEACHING NOTE — Stride
    // sizeof(Vertex) = sizeof(float)*2 + sizeof(float)*3 = 20 bytes.
    // After reading one vertex, the GPU pointer advances by stride bytes
    // to find the next vertex in the buffer.
    VkVertexInputBindingDescription binding = {};
    binding.binding   = 0;                          // slot index
    binding.stride    = sizeof(Vertex);             // bytes per vertex
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // advance per vertex
    return binding;
}

// ===========================================================================
// GetAttributeDescriptions
// ===========================================================================
std::array<VkVertexInputAttributeDescription, 2>
VulkanMesh::GetAttributeDescriptions()
{
    std::array<VkVertexInputAttributeDescription, 2> attrs = {};

    // -----------------------------------------------------------------------
    // Attribute 0 — position (vec2 at byte offset 0)
    // -----------------------------------------------------------------------
    // TEACHING NOTE — VK_FORMAT_R32G32_SFLOAT
    // Vulkan reuses its texture format enum for vertex attributes.
    // R32G32_SFLOAT = two 32-bit signed floats = GLSL vec2.
    // -----------------------------------------------------------------------
    attrs[0].binding  = 0;
    attrs[0].location = 0;   // matches "layout(location = 0)" in triangle.vert
    attrs[0].format   = VK_FORMAT_R32G32_SFLOAT;
    attrs[0].offset   = offsetof(Vertex, position);   // 0

    // -----------------------------------------------------------------------
    // Attribute 1 — color (vec3 at byte offset 8)
    // -----------------------------------------------------------------------
    attrs[1].binding  = 0;
    attrs[1].location = 1;   // matches "layout(location = 1)" in triangle.vert
    attrs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset   = offsetof(Vertex, color);      // 8

    return attrs;
}

} // namespace rendering
} // namespace engine
