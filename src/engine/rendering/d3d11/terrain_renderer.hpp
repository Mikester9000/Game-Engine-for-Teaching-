/**
 * @file terrain_renderer.hpp
 * @brief TerrainRenderer — heightmap-driven grid mesh renderer for D3D11 (M25).
 *
 * ============================================================================
 * TEACHING NOTE — What is a Heightmap Terrain?
 * ============================================================================
 * A heightmap terrain represents the world surface as a regular 2-D grid of
 * height values (floats).  For a grid of W×H samples, the engine generates
 * (W-1)×(H-1)×2 triangles whose vertex Y coordinates come from the height
 * samples.  This produces a smooth, undulating surface from a simple flat
 * data structure — exactly the technique used in games like Final Fantasy XV,
 * The Witcher 3, and the Unreal Engine terrain editor.
 *
 * Advantages over arbitrary polygon meshes:
 *   • O(1) height query at any (x,z) position — ideal for physics and AI.
 *   • Trivially divisible into streaming cells (each cell is a sub-grid).
 *   • Easy to author: height values can come from a grayscale image or a
 *     procedural noise function.
 *
 * ============================================================================
 * TEACHING NOTE — Two-Phase Initialisation
 * ============================================================================
 * TerrainRenderer uses a two-phase init pattern common in D3D11 renderers:
 *
 *   Phase 1 — CPU geometry generation (no device required):
 *     Call LoadFromSamples() or LoadCooked() to store the height array and
 *     call GenerateMesh() internally.  The resulting m_vertices and m_indices
 *     are valid CPU-side data that can be inspected / tested without a device.
 *
 *   Phase 2 — GPU resource creation (requires an ID3D11Device*):
 *     Call CreateDeviceResources() to compile the HLSL shaders, create the
 *     input layout, and upload the vertex/index data to GPU buffers.  After
 *     this call Draw() can be called each frame.
 *
 * Separating the phases is important for testing: unit tests can call
 * LoadFromSamples() and check GetVertices() without needing a WARP device.
 *
 * ============================================================================
 * TEACHING NOTE — Cooked Terrain Binary Format (TRN1)
 * ============================================================================
 * The Python baker (tools/creation_engine.py :: bake_terrain) converts a
 * JSON heightmap description to a compact binary:
 *
 *   Offset  Size  Field
 *   0       4     Magic: "TRN1"
 *   4       2     Version: 1 (uint16, little-endian)
 *   6       2     Width: number of samples along X (uint16)
 *   8       2     Height: number of samples along Z (uint16)
 *  10       4     CellSize: world-space distance between adjacent samples (float32)
 *  14       W*H*4 Height samples: row-major float32 array (row=Z, col=X)
 *
 * Total: 14 + W*H*4 bytes.
 * The runtime loads this binary with LoadCooked() using a simple byte-by-byte
 * parse — no external library required.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Platform: Windows (D3D11); gated by ENGINE_ENABLE_D3D11
 */

#pragma once

#ifdef ENGINE_ENABLE_D3D11

#include <d3d11.h>
#include <string>
#include <vector>
#include <cstdint>

namespace engine {
namespace rendering {

// ===========================================================================
// TerrainRenderer
// ===========================================================================

/**
 * @class TerrainRenderer
 * @brief Generates and renders a heightmap-driven triangle grid via D3D11.
 *
 * ============================================================================
 * TEACHING NOTE — Design Philosophy
 * ============================================================================
 * This class is intentionally self-contained: it owns its D3D11 COM pointers,
 * compiles its own HLSL shaders, and manages its own vertex/index buffers.
 * That "fat renderer" design matches real engine terrain renderers (UE4's
 * FLandscapeComponentSceneProxy, for example) where the terrain is a first-
 * class rendering citizen, not a generic static mesh.
 *
 * The class does NOT use COM smart pointers to keep the COM interaction code
 * readable for students — every COM pointer is explicitly released in Release().
 *
 * ============================================================================
 */
class TerrainRenderer
{
public:
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Terrain Vertex Layout
    // -----------------------------------------------------------------------
    // Each vertex stores position, normal and UV.  The layout must match both:
    //   1. The D3D11_INPUT_ELEMENT_DESC array in CreateDeviceResources().
    //   2. The VSInput struct in terrain.vs.hlsl.
    //
    // Fields are packed as plain C floats so the struct is trivially copyable
    // and can be uploaded to the GPU with a single memcpy.
    // -----------------------------------------------------------------------

    /// Single vertex: position + normal + UV.
    struct Vertex
    {
        float pos[3];     ///< POSITION  — x, y (= height), z in model space
        float normal[3];  ///< NORMAL    — finite-difference surface normal
        float uv[2];      ///< TEXCOORD0 — [0,1]² normalised UV over the patch
    };

    // -----------------------------------------------------------------------
    // TEACHING NOTE — TerrainCB Constant Buffer (CPU-side mirror)
    // -----------------------------------------------------------------------
    // This struct is the exact C++ mirror of the TerrainCB cbuffer declared in
    // terrain.vs.hlsl and terrain.ps.hlsl.  Both stages bind this buffer to b0.
    //
    // sizeof(TerrainCB) must be a multiple of 16 bytes (D3D11 requirement).
    //   3 × float4x4 = 3 × 64 = 192 bytes
    //   float3 + float pad =  16 bytes
    //   Total: 208 bytes  (13 × 16 ✓)
    // -----------------------------------------------------------------------

    /// GPU constant buffer data (matched to TerrainCB in terrain.vs/.ps.hlsl).
    struct alignas(16) TerrainCB
    {
        float world[16];   ///< g_world  — row-major float4x4
        float view[16];    ///< g_view   — row-major float4x4
        float proj[16];    ///< g_proj   — row-major float4x4
        float lightDir[3]; ///< g_lightDir
        float pad;         ///< g_pad0
    };

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    TerrainRenderer()  = default;
    ~TerrainRenderer() { Release(); }

    // Non-copyable — owning D3D11 COM pointers cannot be safely copied.
    TerrainRenderer(const TerrainRenderer&)            = delete;
    TerrainRenderer& operator=(const TerrainRenderer&) = delete;

    // -----------------------------------------------------------------------
    // Phase 1 — CPU geometry load (no GPU device required)
    // -----------------------------------------------------------------------

    /**
     * @brief Load a heightmap from a flat row-major float array.
     *
     * TEACHING NOTE — Row-Major Heightmap Storage
     * The heights array is stored as rows (Z direction) then columns (X
     * direction): heights[row * width + col] = Y at (col * cellSize, row * cellSize).
     * Row 0 is the "back" row (Z = 0), row (height-1) is the "front" row.
     *
     * @param heights   Pointer to width×height float values.
     * @param width     Number of samples along X (must be >= 2).
     * @param height    Number of samples along Z (must be >= 2).
     * @param cellSize  World-space distance between adjacent samples (metres).
     * @return true on success.
     */
    bool LoadFromSamples(const float* heights, int width, int height, float cellSize);

    /**
     * @brief Load a cooked terrain from a TRN1 binary file.
     *
     * @param path  Absolute or relative path to the .terrain binary.
     * @return true on success.
     */
    bool LoadCooked(const char* path);

    // -----------------------------------------------------------------------
    // Phase 2 — GPU resource creation (requires ID3D11Device*)
    // -----------------------------------------------------------------------

    /**
     * @brief Compile shaders and upload geometry to GPU buffers.
     *
     * @param dev        The D3D11 device to create resources on.
     * @param shaderDir  Directory containing terrain.vs.hlsl / terrain.ps.hlsl.
     * @return true if all D3D11 resources were created successfully.
     */
    bool CreateDeviceResources(ID3D11Device* dev, const std::string& shaderDir);

    // -----------------------------------------------------------------------
    // Rendering
    // -----------------------------------------------------------------------

    /**
     * @brief Render the terrain using the given context and matrices.
     *
     * @param ctx      Immediate device context.
     * @param worldMat Row-major 4×4 world matrix (16 floats).
     * @param viewMat  Row-major 4×4 view matrix  (16 floats).
     * @param projMat  Row-major 4×4 proj matrix  (16 floats).
     */
    void Draw(ID3D11DeviceContext* ctx,
              const float* worldMat,
              const float* viewMat,
              const float* projMat);

    // -----------------------------------------------------------------------
    // Cleanup
    // -----------------------------------------------------------------------

    /// Release all D3D11 COM resources.  Safe to call multiple times.
    void Release();

    // -----------------------------------------------------------------------
    // Accessors (mostly for headless testing)
    // -----------------------------------------------------------------------

    bool IsLoaded()   const { return m_loaded; }   ///< CPU geometry is ready.
    bool IsGpuReady() const { return m_gpuReady; } ///< D3D11 resources created.

    int   GetWidth()    const { return m_width; }
    int   GetHeight()   const { return m_height; }
    float GetCellSize() const { return m_cellSize; }

    /// Access the generated vertex data (for testing without a GPU).
    const std::vector<Vertex>&    GetVertices() const { return m_vertices; }

    /// Access the generated index data (for testing without a GPU).
    const std::vector<uint32_t>&  GetIndices()  const { return m_indices; }

    /// Access raw height samples (row-major, Z×X).
    const std::vector<float>&     GetHeights()  const { return m_heights; }

private:
    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Generate the vertex and index arrays from the height data.
     *
     * TEACHING NOTE — Finite-Difference Normal Estimation
     * The normal at grid cell (r, c) is computed from the cross-product of
     * the two tangent vectors that span the surface at that point:
     *
     *   Tx = position(r, c+1) - position(r, c-1)   (tangent along X)
     *   Tz = position(r+1, c) - position(r-1, c)   (tangent along Z)
     *   N  = normalize(cross(Tz, Tx))               (outward normal)
     *
     * Edge vertices use a one-sided (forward or backward) difference.
     * This is a classic finite-difference scheme — the same used in GPU
     * terrain shaders that sample a heightmap texture.
     */
    void GenerateMesh();

    // -----------------------------------------------------------------------
    // CPU-side data
    // -----------------------------------------------------------------------

    int                  m_width    = 0;     ///< Samples along X
    int                  m_height   = 0;     ///< Samples along Z
    float                m_cellSize = 1.0f;  ///< World metres per sample
    std::vector<float>   m_heights;          ///< Row-major height samples
    std::vector<Vertex>  m_vertices;         ///< Generated vertices
    std::vector<uint32_t> m_indices;         ///< Generated triangle indices
    bool                 m_loaded   = false; ///< Phase 1 complete

    // -----------------------------------------------------------------------
    // D3D11 GPU resources
    // -----------------------------------------------------------------------

    ID3D11VertexShader* m_vs          = nullptr; ///< Compiled vertex shader
    ID3D11PixelShader*  m_ps          = nullptr; ///< Compiled pixel shader
    ID3D11InputLayout*  m_inputLayout = nullptr; ///< Vertex buffer layout
    ID3D11Buffer*       m_vertexBuf   = nullptr; ///< GPU vertex buffer (immutable)
    ID3D11Buffer*       m_indexBuf    = nullptr; ///< GPU index buffer  (immutable)
    ID3D11Buffer*       m_terrainCB   = nullptr; ///< Dynamic constant buffer

    bool m_gpuReady = false; ///< Phase 2 complete
};

} // namespace rendering
} // namespace engine

#endif // ENGINE_ENABLE_D3D11
