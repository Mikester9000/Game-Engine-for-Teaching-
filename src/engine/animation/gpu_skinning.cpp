/**
 * @file gpu_skinning.cpp
 * @brief D3D11 GPU skinning constant buffer implementation.
 *
 * ============================================================================
 * TEACHING NOTE — D3D11 Dynamic Constant Buffer Update Pattern
 * ============================================================================
 * Every frame the CPU has fresh joint matrices from the AnimationSystem.
 * We need to get those 4096 bytes into VRAM so the vertex shader can read them.
 *
 * D3D11 offers several update strategies:
 *
 *   1. UpdateSubresource()  — copies from CPU to a DEFAULT-usage resource.
 *                             Causes a pipeline stall if the GPU is still
 *                             reading the resource.  Slower for per-frame data.
 *
 *   2. Map(WRITE_DISCARD)   — maps a DYNAMIC resource for CPU writes.
 *                             The driver "discards" the old data (returns a new
 *                             memory region) so no GPU stall occurs.
 *                             Fastest for small, frequently-updated buffers.
 *
 *   3. Map(WRITE_NO_OVERWRITE) — reuse the same physical memory, but CPU
 *                                promises not to overwrite data still in flight.
 *                                Useful for ring-buffer streaming.
 *
 * We use strategy 2 (WRITE_DISCARD) for simplicity and correctness.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Platform: Windows (D3D11 only)
 */

#include "engine/animation/gpu_skinning.hpp"

#ifdef ENGINE_ENABLE_D3D11

#include <cstring>    // memcpy, memset
#include <iostream>

namespace engine {
namespace animation {

// ===========================================================================
// GpuSkinningBuffer
// ===========================================================================

bool GpuSkinningBuffer::Init(ID3D11Device* device, int maxJoints)
{
    if (!device || maxJoints <= 0 || maxJoints > 256)
    {
        std::cerr << "[GpuSkinningBuffer] Invalid Init() arguments.\n";
        return false;
    }

    m_maxJoints = maxJoints;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Constant Buffer Size Rules
    // -----------------------------------------------------------------------
    // D3D11 requires the ByteWidth of a constant buffer to be a non-zero
    // multiple of 16 bytes.  sizeof(math::Mat4) = 64 bytes (16 floats × 4
    // bytes), so maxJoints × 64 is always a multiple of 64 which is also a
    // multiple of 16.
    // -----------------------------------------------------------------------
    const UINT byteWidth = static_cast<UINT>(maxJoints) *
                           static_cast<UINT>(sizeof(math::Mat4));

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth          = byteWidth;
    // TEACHING NOTE — USAGE_DYNAMIC marks this buffer as CPU-writable.
    // D3D11 places it in write-combined memory accessible by both CPU and GPU.
    desc.Usage              = D3D11_USAGE_DYNAMIC;
    // TEACHING NOTE — BIND_CONSTANT_BUFFER makes it visible in the shader
    // as a cbuffer.  Only one bind flag is allowed for DYNAMIC resources.
    desc.BindFlags          = D3D11_BIND_CONSTANT_BUFFER;
    // TEACHING NOTE — CPU_ACCESS_WRITE enables Map() with WRITE_DISCARD.
    desc.CPUAccessFlags     = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags          = 0;
    desc.StructureByteStride = 0;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — nullptr pInitialData for DYNAMIC resources
    // -----------------------------------------------------------------------
    // We do not supply initial data because the CPU will write the joint
    // matrices via Upload() every frame before the first draw.  Passing
    // nullptr here is allowed for DYNAMIC usage; the initial contents of the
    // buffer are undefined but will be fully overwritten before first use.
    // -----------------------------------------------------------------------
    const HRESULT hr = device->CreateBuffer(&desc, nullptr, &m_cbuffer);
    if (FAILED(hr))
    {
        std::cerr << "[GpuSkinningBuffer] CreateBuffer failed. HRESULT=0x"
                  << std::hex << static_cast<unsigned long>(hr) << std::dec << "\n";
        m_cbuffer = nullptr;
        return false;
    }

    return true;
}

void GpuSkinningBuffer::Upload(ID3D11DeviceContext* context,
                                const math::Mat4*   matrices,
                                int                 count)
{
    if (!m_cbuffer || !context || count <= 0)
        return;

    // Clamp to the buffer's capacity.
    const int toWrite = (count < m_maxJoints) ? count : m_maxJoints;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Map / WRITE_DISCARD / Unmap
    // -----------------------------------------------------------------------
    // Map() returns a pointer to GPU-accessible memory.
    //
    //   D3D11_MAP_WRITE_DISCARD:
    //     The driver allocates a fresh memory region.  The previous contents
    //     are "discarded" (the GPU may still be reading the old region, but
    //     that's fine because we got a NEW region).  This avoids stalls.
    //
    //   The mapped resource's pData pointer is only valid between Map and Unmap.
    //   Never cache or use pData outside this window.
    //
    //   Unmap() commits the write to the driver and invalidates pData.
    // -----------------------------------------------------------------------
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    const HRESULT hr = context->Map(m_cbuffer,
                                    0,                        // subresource 0
                                    D3D11_MAP_WRITE_DISCARD,  // discard old data
                                    0,                        // no flags
                                    &mapped);
    if (FAILED(hr))
    {
        std::cerr << "[GpuSkinningBuffer] Map failed. HRESULT=0x"
                  << std::hex << static_cast<unsigned long>(hr) << std::dec << "\n";
        return;
    }

    // Write the joint matrices into the mapped region.
    const size_t writeSz = static_cast<size_t>(toWrite) * sizeof(math::Mat4);
    std::memcpy(mapped.pData, matrices, writeSz);

    // TEACHING NOTE — Fill unused slots with identity matrices.
    // If count < maxJoints, any joints beyond count get identity so vertices
    // that reference those joint indices are not deformed unexpectedly.
    if (toWrite < m_maxJoints)
    {
        // Build an identity Mat4 and repeat it for remaining slots.
        const math::Mat4 identity = math::Mat4::Identity();
        auto* dst = static_cast<math::Mat4*>(mapped.pData);
        for (int i = toWrite; i < m_maxJoints; ++i)
            dst[i] = identity;
    }

    context->Unmap(m_cbuffer, 0);
}

void GpuSkinningBuffer::Bind(ID3D11DeviceContext* context, UINT slot) const
{
    if (!m_cbuffer || !context)
        return;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — VSSetConstantBuffers
    // -----------------------------------------------------------------------
    // This binds m_cbuffer to VS constant-buffer slot `slot` (b<slot> in HLSL).
    // The vertex shader can then access all 64 joint matrices by indexing into
    // the cbuffer array.
    //
    // Note: we only set the VS stage here.  If the same joint data were needed
    // in the geometry or pixel shader, we would also call
    // GSSetConstantBuffers / PSSetConstantBuffers with the same buffer.
    // -----------------------------------------------------------------------
    context->VSSetConstantBuffers(slot, 1, &m_cbuffer);
}

void GpuSkinningBuffer::Unbind(ID3D11DeviceContext* context, UINT slot) const
{
    if (!context)
        return;

    ID3D11Buffer* nullCB = nullptr;
    context->VSSetConstantBuffers(slot, 1, &nullCB);
}

void GpuSkinningBuffer::Shutdown()
{
    if (m_cbuffer)
    {
        m_cbuffer->Release();
        m_cbuffer = nullptr;
    }
    m_maxJoints = 0;
}

} // namespace animation
} // namespace engine

#endif // ENGINE_ENABLE_D3D11
