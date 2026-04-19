/**
 * @file gpu_skinning.hpp
 * @brief D3D11 GPU skinning constant buffer — uploads joint matrices to VRAM.
 *
 * ============================================================================
 * TEACHING NOTE — What is GPU Skinning?
 * ============================================================================
 * In CPU skinning the joint-matrix multiplications are done on the CPU and the
 * already-transformed vertices are sent to the GPU each frame.  That approach
 * wastes bus bandwidth and CPU cycles.
 *
 * In GPU skinning (shader skinning):
 *   1. All joint matrices are uploaded once per frame to a constant buffer.
 *   2. Each vertex stores bone indices + bone weights (4 influences).
 *   3. The vertex shader blends the vertex position using those weights:
 *
 *        worldPos = sum_i( boneWeight[i] * (jointMatrix[boneIndex[i]] * bindPos) )
 *
 *   4. The vertex buffer itself NEVER changes — it always contains the static
 *      bind-pose mesh.
 *
 * This is the standard approach used by every modern AAA game engine because:
 *   • Only 4096 bytes (64 × 64-byte Mat4) travel the bus per character per
 *     frame instead of MB of vertex data.
 *   • The GPU is already designed to execute many identical ALU operations in
 *     parallel — blending positions with 4 matrices per vertex is trivial.
 *   • The vertex buffer can be shared between multiple instances (same mesh,
 *     different skeletons, different animations).
 *
 * ============================================================================
 * TEACHING NOTE — D3D11 Constant Buffer Layout
 * ============================================================================
 * A D3D11 constant buffer is a block of bytes accessible to any shader stage.
 * Layout rules (D3D11 default packing, not cbuffer row_major):
 *   • Each float4x4 (Mat4) is 64 bytes (4 rows × 4 floats × 4 bytes).
 *   • 64 matrices × 64 bytes = 4096 bytes (4 KiB) per character.
 *   • The buffer must be a multiple of 16 bytes — 4096 satisfies this.
 *
 * The buffer is D3D11_USAGE_DYNAMIC because the CPU writes new joint matrices
 * every frame.  D3D11_MAP_WRITE_DISCARD tells the driver to return a new
 * region of memory rather than stalling until the GPU finishes reading the
 * old data (pipeline hazard avoidance).
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Platform: Windows (D3D11 only)
 */

#pragma once

#ifdef ENGINE_ENABLE_D3D11

// TEACHING NOTE — D3D11 headers require WIN32_LEAN_AND_MEAN + NOMINMAX guards.
// These are defined as compile-time macros by the CMakeLists.txt SANDBOX_DEFS
// list, so they are already active when this header is compiled as part of
// engine_sandbox.  The #ifndef guards prevent double-definition warnings if
// the macros are already set elsewhere.
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <d3d11.h>

#include "engine/math/math_types.hpp"

namespace engine {
namespace animation {

// ---------------------------------------------------------------------------
// kMaxGpuJoints — maximum number of joints supported per character
// ---------------------------------------------------------------------------
// TEACHING NOTE — Why 64?
// 64 joints covers most humanoid characters (typically 50–60 joints).
// 64 × sizeof(Mat4) = 64 × 64 = 4096 bytes — fits nicely in one D3D11
// constant buffer slot and is well within the D3D11 constant-buffer limit of
// 65536 bytes (4096 floats = 4096 × 4 bytes = 16 KiB for a cbuffer).
//
// For very complex characters (clothes, face bones) you might raise this to
// 128 or 256, but that uses more vertex-shader constant registers on older
// hardware (GT610 / FL10_0 class).
// ---------------------------------------------------------------------------
inline constexpr int kMaxGpuJoints = 64;

/**
 * @class GpuSkinningBuffer
 * @brief Manages a D3D11 DYNAMIC constant buffer for GPU joint matrices.
 *
 * Usage pattern (one per character or per skinned mesh instance):
 * @code
 *   GpuSkinningBuffer cb;
 *   cb.Init(device);
 *
 *   // Each frame:
 *   cb.Upload(context, animator.jointMatrices, animator.jointCount);
 *   cb.Bind(context, 0);   // binds to VS register b0
 *   // … issue DrawIndexed() …
 * @endcode
 */
class GpuSkinningBuffer
{
public:
    GpuSkinningBuffer()  = default;
    ~GpuSkinningBuffer() { Shutdown(); }

    // RAII — prevent copy/move (COM pointer ownership)
    GpuSkinningBuffer(const GpuSkinningBuffer&)            = delete;
    GpuSkinningBuffer& operator=(const GpuSkinningBuffer&) = delete;
    GpuSkinningBuffer(GpuSkinningBuffer&&)                 = delete;
    GpuSkinningBuffer& operator=(GpuSkinningBuffer&&)      = delete;

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    /**
     * @brief Allocate the D3D11 constant buffer on the given device.
     *
     * @param device     D3D11 device to create the buffer on.
     * @param maxJoints  Maximum joints the buffer will hold (default 64).
     *
     * @return true on success, false if D3D11 resource creation failed.
     *
     * TEACHING NOTE — D3D11_USAGE_DYNAMIC + CPU_ACCESS_WRITE
     * These two flags together tell D3D11 that:
     *   • The GPU only reads the buffer (no GPU writes).
     *   • The CPU will write to it every frame via Map/Unmap.
     * The driver can place it in a write-combined memory region (WC) which
     * is optimised for CPU→GPU streaming with minimal CPU-side caching.
     *
     * D3D11_MAP_WRITE_DISCARD (used in Upload) tells the driver to give us a
     * fresh memory region, avoiding any GPU stall waiting for the previous
     * frame's draw to finish reading the old data.
     */
    bool Init(ID3D11Device* device, int maxJoints = kMaxGpuJoints);

    /**
     * @brief Upload joint matrices to the GPU for this frame.
     *
     * @param context     Immediate device context.
     * @param matrices    Pointer to an array of Mat4 skin matrices.
     * @param count       Number of matrices to upload (must be ≤ maxJoints).
     *
     * TEACHING NOTE — Map / Unmap Pattern
     * Map returns a pointer to GPU-visible memory (or a staging area).
     * We memcpy the matrices, then Unmap signals to D3D11 that we are done.
     * After Unmap the CPU pointer is invalid.
     *
     * Matrices beyond `count` are set to identity so the shader never reads
     * uninitialised data even if a vertex references a joint index ≥ count.
     */
    void Upload(ID3D11DeviceContext* context,
                const math::Mat4*   matrices,
                int                 count);

    /**
     * @brief Bind this constant buffer to the vertex shader.
     *
     * @param context  Immediate device context.
     * @param slot     VS constant buffer slot (HLSL: `register(b<slot>)`).
     *                 Defaults to 0 (b0).
     *
     * TEACHING NOTE — VSSetConstantBuffers
     * A vertex shader can access up to 14 constant buffer slots (D3D11
     * minimum, more in practice).  We bind the joint matrices to b0 so the
     * HLSL shader reads them as:
     *
     *   cbuffer JointCB : register(b0) { float4x4 g_joints[64]; };
     *
     * If you have a per-object CB (world matrix, etc.) use b1 for that.
     */
    void Bind(ID3D11DeviceContext* context, UINT slot = 0) const;

    /**
     * @brief Unbind this constant buffer from the vertex shader slot.
     *
     * TEACHING NOTE — Always unbind constant buffers after drawing.
     * Leaving a resource bound and then trying to write to it later (e.g. via
     * Map) can trigger D3D11 debug-layer warnings.
     */
    void Unbind(ID3D11DeviceContext* context, UINT slot = 0) const;

    /**
     * @brief Release the D3D11 buffer. Safe to call multiple times.
     */
    void Shutdown();

    /** @return true if Init() succeeded and Shutdown() has not been called. */
    bool IsValid() const { return m_cbuffer != nullptr; }

private:
    ID3D11Buffer* m_cbuffer  = nullptr;
    int           m_maxJoints = 0;
};

} // namespace animation
} // namespace engine

#endif // ENGINE_ENABLE_D3D11
