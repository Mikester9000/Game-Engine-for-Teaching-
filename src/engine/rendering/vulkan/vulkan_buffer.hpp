/**
 * @file vulkan_buffer.hpp
 * @brief GPU buffer allocation and upload helper.
 *
 * ============================================================================
 * TEACHING NOTE — Why a Separate Buffer Class?
 * ============================================================================
 * Every piece of data the GPU reads (vertex positions, uniform matrices,
 * index arrays, …) lives in a VkBuffer.  Creating a buffer involves three
 * steps that must happen together:
 *   1. Create the VkBuffer descriptor.
 *   2. Query its memory requirements (size, alignment, type constraints).
 *   3. Allocate VkDeviceMemory and bind it to the buffer.
 *
 * Wrapping this in a RAII class (VulkanBuffer) prevents the common bug of
 * forgetting step 3, or destroying objects in the wrong order.
 *
 * ============================================================================
 * TEACHING NOTE — M1 Simplification: Host-Visible Memory
 * ============================================================================
 * There are two broad GPU memory categories:
 *
 *   HOST_VISIBLE — CPU can read/write it directly (via vkMapMemory).
 *                  The GPU can still read it, but bandwidth is lower.
 *
 *   DEVICE_LOCAL  — lives entirely on the GPU's fast VRAM.
 *                  The CPU cannot access it directly.  Data must be
 *                  transferred through a "staging buffer" (a host-visible
 *                  temporary buffer) using a GPU copy command.
 *
 * For M1 (a static triangle with only 60 bytes), HOST_VISIBLE is fine.
 * M2+ will introduce staging buffers for meshes loaded from disk.
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

#define VK_USE_PLATFORM_WIN32_KHR
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <windows.h>
#include <vulkan/vulkan.h>

#include <cstddef>    // std::size_t
#include <cstdint>    // uint32_t

namespace engine {
namespace rendering {

// ===========================================================================
// class VulkanBuffer
// ===========================================================================
// Owns a VkBuffer + VkDeviceMemory pair.  Provides:
//   Create()  — allocate buffer + memory with requested usage flags.
//   Upload()  — write CPU data into host-visible buffer memory.
//   Destroy() — free buffer + memory in the correct order.
//
// TEACHING NOTE — No Copying
// Buffers represent unique GPU resources.  We delete copy semantics so the
// compiler catches accidental copies (which would double-free).
// Move semantics are not implemented here for teaching simplicity; use
// std::unique_ptr<VulkanBuffer> when ownership transfer is needed.
// ===========================================================================
class VulkanBuffer
{
public:
    VulkanBuffer()  = default;
    ~VulkanBuffer() { Destroy(); }

    VulkanBuffer(const VulkanBuffer&)            = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    /**
     * @brief Allocate a GPU buffer with the specified usage and memory type.
     *
     * @param device      Logical device to create the buffer on.
     * @param physDevice  Physical device for memory-type queries.
     * @param size        Buffer size in bytes.
     * @param usage       VkBufferUsageFlags (e.g. VK_BUFFER_USAGE_VERTEX_BUFFER_BIT).
     * @param properties  VkMemoryPropertyFlags (e.g. HOST_VISIBLE | HOST_COHERENT).
     * @return true on success.
     *
     * TEACHING NOTE — Memory Properties
     * The combination HOST_VISIBLE | HOST_COHERENT means:
     *   HOST_VISIBLE  — the CPU can vkMapMemory and get a writeable pointer.
     *   HOST_COHERENT — writes are immediately visible to the GPU without an
     *                   explicit vkFlushMappedMemoryRanges call.
     */
    bool Create(VkDevice           device,
                VkPhysicalDevice   physDevice,
                VkDeviceSize       size,
                VkBufferUsageFlags usage,
                VkMemoryPropertyFlags properties);

    /**
     * @brief Write CPU data into the buffer (only valid for HOST_VISIBLE memory).
     *
     * @param data    Pointer to the source data.
     * @param size    Bytes to copy (must be ≤ buffer size).
     *
     * TEACHING NOTE — vkMapMemory
     * vkMapMemory gives us a plain void* to the GPU memory.  We memcpy into
     * it, then vkUnmapMemory releases the mapping.  On discrete GPUs (VRAM)
     * this would be too slow for large assets — that is why staging buffers
     * exist.  For a 60-byte triangle it is perfectly fine.
     */
    void Upload(const void* data, VkDeviceSize size);

    /**
     * @brief Destroy the buffer and free its memory.
     *
     * Safe to call multiple times.
     */
    void Destroy();

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------

    /** @return The underlying VkBuffer handle. */
    VkBuffer     GetBuffer() const { return m_buffer; }

    /** @return true if Create() succeeded and Destroy() has not been called. */
    bool         IsValid()   const { return m_initialised; }

private:
    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Find the index of a memory type that satisfies both constraints.
     *
     * @param physDevice   Physical device whose memory properties to query.
     * @param typeBits     Bitmask of suitable memory-type indices from
     *                     VkMemoryRequirements::memoryTypeBits.
     * @param properties   Required VkMemoryPropertyFlags.
     * @return Memory type index on success, UINT32_MAX if not found.
     *
     * TEACHING NOTE — Memory Types
     * A physical device exposes several memory "heaps" (e.g. VRAM, system RAM)
     * and multiple "types" pointing into those heaps.  The requirements
     * object from vkGetBufferMemoryRequirements gives us a bitmask of which
     * types are compatible; we also need the type to have our desired
     * property flags.  This helper finds the first type that satisfies both.
     */
    static uint32_t FindMemoryType(VkPhysicalDevice      physDevice,
                                   uint32_t              typeBits,
                                   VkMemoryPropertyFlags properties);

    // -----------------------------------------------------------------------
    // Data members
    // -----------------------------------------------------------------------

    VkDevice       m_device     = VK_NULL_HANDLE;
    VkBuffer       m_buffer     = VK_NULL_HANDLE;
    VkDeviceMemory m_memory     = VK_NULL_HANDLE;
    VkDeviceSize   m_size       = 0;
    bool           m_initialised = false;
};

} // namespace rendering
} // namespace engine
