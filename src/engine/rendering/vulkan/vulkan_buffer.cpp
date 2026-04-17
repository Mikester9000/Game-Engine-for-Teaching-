/**
 * @file vulkan_buffer.cpp
 * @brief VulkanBuffer — GPU buffer allocation and upload implementation.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 */

#include "engine/rendering/vulkan/vulkan_buffer.hpp"

#include <iostream>
#include <cstring>    // std::memcpy

namespace engine {
namespace rendering {

// ===========================================================================
// Create
// ===========================================================================
// TEACHING NOTE — VkBufferCreateInfo
// Most Vulkan "CreateInfo" structs follow the same pattern:
//   sType    — identifies the struct type for Vulkan's type-safe C API.
//   pNext    — extension chain pointer; nullptr unless using extensions.
//   flags    — mostly unused for basic buffers.
//   size     — byte size of the buffer.
//   usage    — bitmask of how the buffer will be used (vertex, index, uniform…).
//   sharingMode — EXCLUSIVE means only one queue family accesses it at a time.
// ===========================================================================
bool VulkanBuffer::Create(VkDevice              device,
                          VkPhysicalDevice      physDevice,
                          VkDeviceSize          size,
                          VkBufferUsageFlags    usage,
                          VkMemoryPropertyFlags properties)
{
    m_device = device;
    m_size   = size;

    // -----------------------------------------------------------------------
    // Step 1 — Create the VkBuffer descriptor.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Buffer vs Memory
    // A VkBuffer is just a descriptor (usage, size, format).  It has NO
    // memory yet.  Memory is allocated separately and "bound" in step 3.
    // This separation lets you sub-allocate a large VkDeviceMemory block
    // across many buffers — important for performance in real engines.
    // -----------------------------------------------------------------------
    VkBufferCreateInfo bufInfo = {};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = size;
    bufInfo.usage       = usage;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateBuffer(m_device, &bufInfo, nullptr, &m_buffer);
    if (result != VK_SUCCESS)
    {
        std::cerr << "[VulkanBuffer] vkCreateBuffer failed: " << result << "\n";
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 2 — Query memory requirements.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — VkMemoryRequirements
    // After buffer creation, the driver knows:
    //   size          — actual bytes needed (may be larger than requested due
    //                   to alignment constraints).
    //   alignment     — buffer must start at a multiple of this value.
    //   memoryTypeBits — bitmask of memory types this buffer can be bound to.
    // -----------------------------------------------------------------------
    VkMemoryRequirements memReqs = {};
    vkGetBufferMemoryRequirements(m_device, m_buffer, &memReqs);

    // -----------------------------------------------------------------------
    // Step 3 — Allocate device memory.
    // -----------------------------------------------------------------------
    uint32_t memTypeIndex = FindMemoryType(physDevice,
                                           memReqs.memoryTypeBits,
                                           properties);
    if (memTypeIndex == UINT32_MAX)
    {
        std::cerr << "[VulkanBuffer] No suitable memory type found.\n";
        vkDestroyBuffer(m_device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = memTypeIndex;

    result = vkAllocateMemory(m_device, &allocInfo, nullptr, &m_memory);
    if (result != VK_SUCCESS)
    {
        std::cerr << "[VulkanBuffer] vkAllocateMemory failed: " << result << "\n";
        vkDestroyBuffer(m_device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 4 — Bind memory to the buffer.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — vkBindBufferMemory
    // The third parameter is the byte offset into the VkDeviceMemory block.
    // Using 0 means the buffer starts at the beginning of the allocation.
    // In a pool allocator, multiple buffers share one big VkDeviceMemory and
    // each uses a different offset.
    // -----------------------------------------------------------------------
    result = vkBindBufferMemory(m_device, m_buffer, m_memory, 0);
    if (result != VK_SUCCESS)
    {
        std::cerr << "[VulkanBuffer] vkBindBufferMemory failed: " << result << "\n";
        vkFreeMemory(m_device, m_memory, nullptr);
        vkDestroyBuffer(m_device, m_buffer, nullptr);
        m_memory = VK_NULL_HANDLE;
        m_buffer = VK_NULL_HANDLE;
        return false;
    }

    m_initialised = true;
    return true;
}

// ===========================================================================
// Upload
// ===========================================================================
// TEACHING NOTE — vkMapMemory / vkUnmapMemory
// For HOST_VISIBLE memory only.
// vkMapMemory returns a CPU pointer into the GPU memory.  We memcpy our
// vertex data into it, then unmap.  Because the memory is HOST_COHERENT,
// no explicit flush is needed — the GPU sees the write immediately.
// ===========================================================================
void VulkanBuffer::Upload(const void* data, VkDeviceSize size)
{
    if (!m_initialised || !data || size == 0) return;

    void* mapped = nullptr;
    vkMapMemory(m_device, m_memory, 0, size, 0, &mapped);
    std::memcpy(mapped, data, static_cast<std::size_t>(size));
    vkUnmapMemory(m_device, m_memory);
}

// ===========================================================================
// Destroy
// ===========================================================================
// TEACHING NOTE — Destruction Order
// The buffer must be destroyed BEFORE the memory.  Destroying the memory
// while the buffer still references it is undefined behaviour.
// ===========================================================================
void VulkanBuffer::Destroy()
{
    if (!m_initialised) return;
    m_initialised = false;

    if (m_buffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(m_device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
    }
    if (m_memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(m_device, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }
}

// ===========================================================================
// FindMemoryType (static helper)
// ===========================================================================
// TEACHING NOTE — Memory Type Selection
// vkGetPhysicalDeviceMemoryProperties returns a table of memory types, each
// belonging to a heap (e.g. "256 MB VRAM", "32 GB system RAM").  We iterate
// and find the first type whose index bit is set in typeBits AND whose flags
// include all of our required property flags.
// ===========================================================================
uint32_t VulkanBuffer::FindMemoryType(VkPhysicalDevice      physDevice,
                                      uint32_t              typeBits,
                                      VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProps = {};
    vkGetPhysicalDeviceMemoryProperties(physDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
    {
        bool typeCompatible = (typeBits & (1u << i)) != 0;
        bool propsMatch     = (memProps.memoryTypes[i].propertyFlags & properties)
                               == properties;
        if (typeCompatible && propsMatch)
            return i;
    }
    return UINT32_MAX;  // Not found
}

} // namespace rendering
} // namespace engine
