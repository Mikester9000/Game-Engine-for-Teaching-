/**
 * @file terrain_renderer.cpp
 * @brief TerrainRenderer implementation — mesh generation + D3D11 rendering (M25).
 *
 * ============================================================================
 * TEACHING NOTE — Heightmap Grid Mesh Generation
 * ============================================================================
 * For a W×H heightmap, the vertex grid is W×H vertices and (W-1)×(H-1)×2
 * triangles (two triangles per quad).  Vertex positions are:
 *
 *   x = col * cellSize
 *   y = heights[row * W + col]     (the heightmap value)
 *   z = row * cellSize
 *
 * UVs are normalised:
 *   u = col / (W - 1)
 *   v = row / (H - 1)
 *
 * Normals use finite differences of the height values (see GenerateMesh).
 *
 * Triangle winding (clockwise in D3D11 default mode, front-face viewed from above):
 *   For quad at (row, col):
 *     Triangle 0: (row,   col)  → (row,   col+1) → (row+1, col)
 *     Triangle 1: (row,   col+1)→ (row+1, col+1) → (row+1, col)
 *
 * ============================================================================
 * TEACHING NOTE — Why ImmutableBuffers for Terrain?
 * ============================================================================
 * The terrain geometry does not change every frame (unlike skinned meshes or
 * particle systems), so we use D3D11_USAGE_IMMUTABLE for the vertex and index
 * buffers.  Immutable buffers reside in the fastest GPU memory tier; the driver
 * can optimise their placement aggressively.
 *
 * The constant buffer (world/view/proj matrices) DOES change each frame, so it
 * uses D3D11_USAGE_DYNAMIC with D3D11_CPU_ACCESS_WRITE — the Map/Unmap pattern
 * that avoids an internal GPU→CPU stall.
 *
 * ============================================================================
 */

#ifdef ENGINE_ENABLE_D3D11

#include "engine/rendering/d3d11/terrain_renderer.hpp"
#include "engine/core/Logger.hpp"

#include <d3dcompiler.h>
#include <cstring>    // memcpy
#include <cmath>      // sqrtf
#include <fstream>    // ifstream (LoadCooked)
#include <algorithm>  // std::min, std::max

#pragma comment(lib, "d3dcompiler.lib")

namespace engine {
namespace rendering {

// ===========================================================================
// Phase 1 — CPU geometry generation
// ===========================================================================

bool TerrainRenderer::LoadFromSamples(const float* heights, int width, int height, float cellSize)
{
    // TEACHING NOTE — Input validation
    // A terrain needs at least 2×2 samples to form one quad.  We also guard
    // against extreme sizes (> 4096 samples per axis) that would cause a
    // multi-GB vertex buffer — an easy mistake for students learning terrain.
    if (!heights || width < 2 || height < 2 || width > 4096 || height > 4096)
    {
        LOG_ERROR("[TerrainRenderer] Invalid heightmap dimensions: %d x %d", width, height);
        return false;
    }
    if (cellSize <= 0.0f)
    {
        LOG_ERROR("[TerrainRenderer] cellSize must be > 0 (got %.4f)", cellSize);
        return false;
    }

    m_width    = width;
    m_height   = height;
    m_cellSize = cellSize;
    m_heights.assign(heights, heights + width * height);

    GenerateMesh();
    m_loaded = true;
    return true;
}

bool TerrainRenderer::LoadCooked(const char* path)
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — TRN1 Binary Format Parser
    // -----------------------------------------------------------------------
    // The binary layout written by bake_terrain() in creation_engine.py:
    //
    //   Offset  Size  Type      Field
    //      0       4   char[4]  magic ("TRN1")
    //      4       2   uint16   version (1)
    //      6       2   uint16   width
    //      8       2   uint16   height
    //     10       4   float32  cellSize
    //     14    W*H*4  float32  heights (row-major, row=Z, col=X)
    //
    // We read it as raw bytes and manually unpack using memcpy to avoid
    // endianness and alignment assumptions — a defensive habit for cooked data.
    // -----------------------------------------------------------------------
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open())
    {
        LOG_ERROR("[TerrainRenderer] Cannot open cooked terrain: %s", path);
        return false;
    }

    auto fileSize = static_cast<size_t>(f.tellg());
    f.seekg(0, std::ios::beg);

    // Minimum header size: 4 + 2 + 2 + 2 + 4 = 14 bytes
    if (fileSize < 14u)
    {
        LOG_ERROR("[TerrainRenderer] Cooked terrain too small (%zu bytes)", fileSize);
        return false;
    }

    std::vector<uint8_t> blob(fileSize);
    f.read(reinterpret_cast<char*>(blob.data()), static_cast<std::streamsize>(fileSize));
    f.close();

    // Verify magic
    if (blob[0] != 'T' || blob[1] != 'R' || blob[2] != 'N' || blob[3] != '1')
    {
        LOG_ERROR("[TerrainRenderer] Invalid magic in cooked terrain: %s", path);
        return false;
    }

    // Parse header fields (little-endian)
    uint16_t version = 0, w = 0, h = 0;
    float    cs      = 0.0f;
    memcpy(&version, blob.data() + 4, 2);
    memcpy(&w,       blob.data() + 6, 2);
    memcpy(&h,       blob.data() + 8, 2);
    memcpy(&cs,      blob.data() + 10, 4);

    if (version != 1)
    {
        LOG_ERROR("[TerrainRenderer] Unsupported TRN1 version %u", version);
        return false;
    }

    size_t expectedSize = 14u + static_cast<size_t>(w) * h * sizeof(float);
    if (fileSize < expectedSize)
    {
        LOG_ERROR("[TerrainRenderer] Cooked terrain truncated (expected %zu, got %zu bytes)",
                  expectedSize, fileSize);
        return false;
    }

    // Parse height samples
    int numSamples = static_cast<int>(w) * static_cast<int>(h);
    std::vector<float> hs(numSamples);
    memcpy(hs.data(), blob.data() + 14, static_cast<size_t>(numSamples) * sizeof(float));

    return LoadFromSamples(hs.data(), static_cast<int>(w), static_cast<int>(h), cs);
}

// ---------------------------------------------------------------------------
// GenerateMesh — internal helper
// ---------------------------------------------------------------------------

void TerrainRenderer::GenerateMesh()
{
    const int W = m_width;
    const int H = m_height;
    const float cs = m_cellSize;

    m_vertices.clear();
    m_vertices.reserve(static_cast<size_t>(W) * H);
    m_indices.clear();
    m_indices.reserve(static_cast<size_t>(W - 1) * (H - 1) * 6u);

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Vertex Generation
    // -----------------------------------------------------------------------
    // For each (row, col) pair we:
    //   1. Set position  (col*cs, heights[row*W+col], row*cs).
    //   2. Compute normal via finite differences (see below).
    //   3. Set UV as normalised grid coordinate.
    //
    // Finite-difference normal:
    //   hL = height at (row, col-1)  — use col if at left edge
    //   hR = height at (row, col+1)  — use col if at right edge
    //   hD = height at (row-1, col)  — use row if at bottom edge
    //   hU = height at (row+1, col)  — use row if at top edge
    //
    //   tangentX = (2*cs, hR-hL, 0)   (step 2*cs in X, height difference in Y)
    //   tangentZ = (0, hU-hD, 2*cs)   (step 2*cs in Z, height difference in Y)
    //   normal   = normalize(cross(tangentZ, tangentX))
    //
    // The cross product of tangentZ × tangentX gives an outward normal
    // (pointing generally upward) for a terrain with standard CW winding.
    // -----------------------------------------------------------------------
    auto idx = [&](int r, int c) -> float {
        return m_heights[static_cast<size_t>(r) * W + c];
    };

    for (int r = 0; r < H; ++r)
    {
        for (int c = 0; c < W; ++c)
        {
            Vertex v{};

            // Position
            v.pos[0] = static_cast<float>(c) * cs;
            v.pos[1] = idx(r, c);
            v.pos[2] = static_cast<float>(r) * cs;

            // Finite-difference height neighbours (clamp to grid boundary)
            float hL = idx(r, (c > 0      ? c - 1 : c));
            float hR = idx(r, (c < W - 1  ? c + 1 : c));
            float hD = idx((r > 0      ? r - 1 : r), c);
            float hU = idx((r < H - 1  ? r + 1 : r), c);

            // Step size: for edge samples the step is cs (half of 2*cs)
            float stepX = (c > 0 && c < W - 1) ? (2.0f * cs) : cs;
            float stepZ = (r > 0 && r < H - 1) ? (2.0f * cs) : cs;

            float tx[3] = { stepX, hR - hL, 0.0f };
            float tz[3] = { 0.0f, hU - hD, stepZ };

            // cross(tz, tx) — outward normal for CW winding viewed from +Y
            float nx = tz[1] * tx[2] - tz[2] * tx[1];
            float ny = tz[2] * tx[0] - tz[0] * tx[2];
            float nz = tz[0] * tx[1] - tz[1] * tx[0];
            float len = sqrtf(nx * nx + ny * ny + nz * nz);
            if (len > 1e-6f) { nx /= len; ny /= len; nz /= len; }
            else              { nx = 0.0f; ny = 1.0f; nz = 0.0f; }

            v.normal[0] = nx;
            v.normal[1] = ny;
            v.normal[2] = nz;

            // Normalised UV [0,1]
            v.uv[0] = static_cast<float>(c) / static_cast<float>(W - 1);
            v.uv[1] = static_cast<float>(r) / static_cast<float>(H - 1);

            m_vertices.push_back(v);
        }
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Index Generation (CW winding)
    // -----------------------------------------------------------------------
    // For each quad formed by corners (r,c), (r,c+1), (r+1,c), (r+1,c+1):
    //
    //   (r,c) -----  (r,c+1)
    //     |      \       |
    //     |     ___\     |
    //   (r+1,c) -- (r+1,c+1)
    //
    // We split into two CW-wound triangles (viewed from above, i.e. from +Y):
    //   T0: A=(r,c),   B=(r,c+1),   C=(r+1,c)
    //   T1: A=(r,c+1), B=(r+1,c+1), C=(r+1,c)
    //
    // D3D11 default front-face: CLOCKWISE.  Viewed from +Y our triangles
    // go A→B→C in clock-wise order, so the top face is front-facing.
    // -----------------------------------------------------------------------
    for (int r = 0; r < H - 1; ++r)
    {
        for (int c = 0; c < W - 1; ++c)
        {
            uint32_t a  = static_cast<uint32_t>(r       * W + c);
            uint32_t b  = static_cast<uint32_t>(r       * W + c + 1);
            uint32_t c0 = static_cast<uint32_t>((r + 1) * W + c);
            uint32_t d  = static_cast<uint32_t>((r + 1) * W + c + 1);

            // Triangle 0
            m_indices.push_back(a);
            m_indices.push_back(b);
            m_indices.push_back(c0);

            // Triangle 1
            m_indices.push_back(b);
            m_indices.push_back(d);
            m_indices.push_back(c0);
        }
    }
}

// ===========================================================================
// Phase 2 — D3D11 resource creation
// ===========================================================================

bool TerrainRenderer::CreateDeviceResources(ID3D11Device* dev, const std::string& shaderDir)
{
    if (!m_loaded)
    {
        LOG_ERROR("[TerrainRenderer] Call LoadFromSamples() or LoadCooked() before CreateDeviceResources()");
        return false;
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Runtime HLSL Compilation with D3DCompileFromFile
    // -----------------------------------------------------------------------
    // We compile the HLSL shaders at runtime (during scene load) rather than
    // at build time (offline FXC/DXC).  This allows a student to edit
    // terrain.vs.hlsl or terrain.ps.hlsl and immediately see the effect the
    // next time the engine starts — no rebuild step required.
    //
    // D3DCompileFromFile is provided by d3dcompiler.lib, which ships with
    // every Windows SDK installation (alongside d3d11.lib).
    // -----------------------------------------------------------------------

    UINT shaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    shaderFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    std::wstring vsPath(shaderDir.begin(), shaderDir.end());
    vsPath += L"/terrain.vs.hlsl";
    std::wstring psPath(shaderDir.begin(), shaderDir.end());
    psPath += L"/terrain.ps.hlsl";

    // Compile vertex shader
    ID3DBlob* vsBlob   = nullptr;
    ID3DBlob* errBlob  = nullptr;
    HRESULT hr = D3DCompileFromFile(vsPath.c_str(), nullptr, nullptr,
                                    "main", "vs_4_0", shaderFlags, 0,
                                    &vsBlob, &errBlob);
    if (FAILED(hr))
    {
        if (errBlob)
        {
            LOG_ERROR("[TerrainRenderer] VS compile error: %s",
                      static_cast<const char*>(errBlob->GetBufferPointer()));
            errBlob->Release();
        }
        LOG_ERROR("[TerrainRenderer] Failed to compile terrain.vs.hlsl (hr=0x%08X)", hr);
        return false;
    }
    if (errBlob) { errBlob->Release(); errBlob = nullptr; }

    // Compile pixel shader
    ID3DBlob* psBlob = nullptr;
    hr = D3DCompileFromFile(psPath.c_str(), nullptr, nullptr,
                            "main", "ps_4_0", shaderFlags, 0,
                            &psBlob, &errBlob);
    if (FAILED(hr))
    {
        if (errBlob)
        {
            LOG_ERROR("[TerrainRenderer] PS compile error: %s",
                      static_cast<const char*>(errBlob->GetBufferPointer()));
            errBlob->Release();
        }
        vsBlob->Release();
        LOG_ERROR("[TerrainRenderer] Failed to compile terrain.ps.hlsl (hr=0x%08X)", hr);
        return false;
    }
    if (errBlob) { errBlob->Release(); errBlob = nullptr; }

    // Create shader objects
    hr = dev->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                 nullptr, &m_vs);
    if (FAILED(hr)) { vsBlob->Release(); psBlob->Release(); return false; }

    hr = dev->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                nullptr, &m_ps);
    if (FAILED(hr)) { vsBlob->Release(); psBlob->Release(); return false; }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Input Layout
    // -----------------------------------------------------------------------
    // The input layout describes how each vertex buffer element maps to a
    // VSInput semantic in terrain.vs.hlsl.  The byte offsets must match the
    // Vertex struct layout exactly:
    //   pos    → offset 0  (3 floats = 12 bytes)
    //   normal → offset 12 (3 floats = 12 bytes)
    //   uv     → offset 24 (2 floats =  8 bytes)
    //   Total vertex stride: 32 bytes
    // -----------------------------------------------------------------------
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] =
    {
        { "POSITION",    0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",    0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = dev->CreateInputLayout(layoutDesc, 3u,
                                vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                &m_inputLayout);
    vsBlob->Release();
    psBlob->Release();
    if (FAILED(hr))
    {
        LOG_ERROR("[TerrainRenderer] CreateInputLayout failed (hr=0x%08X)", hr);
        return false;
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Immutable Vertex Buffer
    // -----------------------------------------------------------------------
    // Immutable buffers (D3D11_USAGE_IMMUTABLE) cannot be updated after creation.
    // The GPU driver can place them in the fastest available memory tier and
    // never needs to track possible writes.  Static terrain geometry is the
    // canonical use-case for immutable buffers.
    // -----------------------------------------------------------------------
    {
        D3D11_BUFFER_DESC bd{};
        bd.Usage          = D3D11_USAGE_IMMUTABLE;
        bd.ByteWidth      = static_cast<UINT>(m_vertices.size() * sizeof(Vertex));
        bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA initData{};
        initData.pSysMem  = m_vertices.data();

        hr = dev->CreateBuffer(&bd, &initData, &m_vertexBuf);
        if (FAILED(hr))
        {
            LOG_ERROR("[TerrainRenderer] CreateBuffer (vertex) failed (hr=0x%08X)", hr);
            return false;
        }
    }

    // Immutable index buffer
    {
        D3D11_BUFFER_DESC bd{};
        bd.Usage          = D3D11_USAGE_IMMUTABLE;
        bd.ByteWidth      = static_cast<UINT>(m_indices.size() * sizeof(uint32_t));
        bd.BindFlags      = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA initData{};
        initData.pSysMem  = m_indices.data();

        hr = dev->CreateBuffer(&bd, &initData, &m_indexBuf);
        if (FAILED(hr))
        {
            LOG_ERROR("[TerrainRenderer] CreateBuffer (index) failed (hr=0x%08X)", hr);
            return false;
        }
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Dynamic Constant Buffer
    // -----------------------------------------------------------------------
    // The constant buffer (matrices + light direction) changes every frame.
    // D3D11_USAGE_DYNAMIC + D3D11_CPU_ACCESS_WRITE enables the Map/Unmap
    // pattern: the CPU writes new data directly into the GPU-visible mapping,
    // the driver queues it efficiently, and we Unmap to commit.
    // -----------------------------------------------------------------------
    {
        D3D11_BUFFER_DESC bd{};
        bd.Usage          = D3D11_USAGE_DYNAMIC;
        bd.ByteWidth      = sizeof(TerrainCB);  // Must be multiple of 16
        bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        hr = dev->CreateBuffer(&bd, nullptr, &m_terrainCB);
        if (FAILED(hr))
        {
            LOG_ERROR("[TerrainRenderer] CreateBuffer (CB) failed (hr=0x%08X)", hr);
            return false;
        }
    }

    m_gpuReady = true;
    LOG_INFO("[TerrainRenderer] GPU resources created (%d vertices, %d indices)",
             static_cast<int>(m_vertices.size()),
             static_cast<int>(m_indices.size()));
    return true;
}

// ===========================================================================
// Draw
// ===========================================================================

void TerrainRenderer::Draw(ID3D11DeviceContext* ctx,
                           const float* worldMat,
                           const float* viewMat,
                           const float* projMat)
{
    if (!m_gpuReady) return;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Map / Unmap Constant Buffer Update
    // -----------------------------------------------------------------------
    // D3D11_MAP_WRITE_DISCARD signals to the driver that we are replacing the
    // entire previous contents.  The driver can give us a fresh memory region
    // (double/triple buffering internally) without stalling the GPU pipeline.
    // This is the standard per-frame CB update pattern.
    // -----------------------------------------------------------------------
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(ctx->Map(m_terrainCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        TerrainCB cb{};
        memcpy(cb.world,    worldMat, 64);
        memcpy(cb.view,     viewMat,  64);
        memcpy(cb.proj,     projMat,  64);
        // Sun direction: pointing up and slightly toward viewer (standard game sun)
        cb.lightDir[0] =  0.4f;
        cb.lightDir[1] =  0.8f;
        cb.lightDir[2] = -0.4f;
        cb.pad         =  0.0f;
        memcpy(mapped.pData, &cb, sizeof(TerrainCB));
        ctx->Unmap(m_terrainCB, 0);
    }

    // Bind shaders and constant buffer to both stages
    ctx->VSSetShader(m_vs, nullptr, 0);
    ctx->PSSetShader(m_ps, nullptr, 0);
    ctx->VSSetConstantBuffers(0, 1, &m_terrainCB);
    ctx->PSSetConstantBuffers(0, 1, &m_terrainCB);

    // Bind geometry
    ctx->IASetInputLayout(m_inputLayout);
    UINT stride = sizeof(Vertex);
    UINT offset = 0u;
    ctx->IASetVertexBuffers(0, 1, &m_vertexBuf, &stride, &offset);
    ctx->IASetIndexBuffer(m_indexBuf, DXGI_FORMAT_R32_UINT, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Draw indexed
    ctx->DrawIndexed(static_cast<UINT>(m_indices.size()), 0, 0);
}

// ===========================================================================
// Cleanup
// ===========================================================================

void TerrainRenderer::Release()
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — COM Release Idiom
    // -----------------------------------------------------------------------
    // Every D3D11 COM object must be Release()'d when we are done.  We null
    // the pointer after release so that calling Release() twice is safe (the
    // null check prevents a second Release on an already-freed object).
    // -----------------------------------------------------------------------
    if (m_terrainCB)  { m_terrainCB->Release();  m_terrainCB  = nullptr; }
    if (m_indexBuf)   { m_indexBuf->Release();   m_indexBuf   = nullptr; }
    if (m_vertexBuf)  { m_vertexBuf->Release();  m_vertexBuf  = nullptr; }
    if (m_inputLayout){ m_inputLayout->Release(); m_inputLayout = nullptr; }
    if (m_ps)         { m_ps->Release();          m_ps         = nullptr; }
    if (m_vs)         { m_vs->Release();          m_vs         = nullptr; }
    m_gpuReady = false;
}

} // namespace rendering
} // namespace engine

#endif // ENGINE_ENABLE_D3D11
