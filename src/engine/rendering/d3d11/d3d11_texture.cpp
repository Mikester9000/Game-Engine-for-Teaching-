/**
 * @file d3d11_texture.cpp
 * @brief D3D11 DDS texture loader implementation.
 *
 * ============================================================================
 * TEACHING NOTE — DDS Parsing without a Third-Party Library
 * ============================================================================
 * Many engines pull in DirectXTex or DirectXTK for DDS loading.  This file
 * implements a minimal, self-contained parser that handles the formats
 * needed by the M3 milestone:
 *
 *   - RGBA8 uncompressed (for UI textures, debugging).
 *   - BC1 / DXT1         (for low-quality opaque geometry).
 *   - BC3 / DXT5         (for textures with alpha channels).
 *   - BC7                (high-quality; preferred for M3 cooked assets).
 *
 * Implementing the parser from scratch makes it easy to understand what the
 * DirectXTex library does internally — and shows that DDS files are not magic,
 * just a well-specified binary format.
 *
 * ============================================================================
 * TEACHING NOTE — D3D11 Texture Upload
 * ============================================================================
 * Creating a GPU texture from CPU data (DDS bytes) in D3D11:
 *
 *   1. Fill D3D11_TEXTURE2D_DESC with width, height, mip count, DXGI format.
 *   2. Fill D3D11_SUBRESOURCE_DATA for each mip level:
 *        pSysMem     = pointer into the DDS data bytes for this mip.
 *        SysMemPitch = bytes per row (for block-compressed: (w/4)*blockSize).
 *   3. Call ID3D11Device::CreateTexture2D with desc + subresource array.
 *   4. Call ID3D11Device::CreateShaderResourceView on the texture.
 *
 * The GPU copies the data at creation time — we do not need to keep the
 * CPU-side bytes alive after CreateTexture2D returns.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Target: Windows (MSVC)
 */

#include "engine/rendering/d3d11/d3d11_texture.hpp"
#include "engine/core/Logger.hpp"

#include <fstream>    // std::ifstream (file loading)
#include <cstring>    // std::memcmp, std::memcpy
#include <algorithm>  // std::max
#include <iostream>

namespace engine {
namespace rendering {

// ===========================================================================
// DDS format constants
// ===========================================================================

// TEACHING NOTE — DDS Magic Number
// All DDS files begin with the 4-byte magic value 0x20534444 ('DDS ').
// This lets us quickly reject non-DDS files before parsing the header.
static constexpr uint32_t DDS_MAGIC = 0x20534444u;  // 'DDS '

// DDS_PIXELFORMAT::dwFourCC values for legacy (pre-DX10) block formats.
static constexpr uint32_t DDPF_FOURCC    = 0x00000004u;
static constexpr uint32_t DDPF_RGB       = 0x00000040u;
static constexpr uint32_t DDPF_ALPHAPIXELS = 0x00000001u;

// FourCC codes for compressed formats.
static constexpr uint32_t FOURCC_DXT1    = 0x31545844u;  // 'DXT1'
static constexpr uint32_t FOURCC_DXT3    = 0x33545844u;  // 'DXT3'
static constexpr uint32_t FOURCC_DXT5    = 0x35545844u;  // 'DXT5'
static constexpr uint32_t FOURCC_DX10    = 0x30315844u;  // 'DX10' (extended header)

// DDS_HEADER::dwFlags bits we check.
static constexpr uint32_t DDSD_MIPMAPCOUNT = 0x00020000u;

// ===========================================================================
// DDS Header structures (packed to match the on-disk layout)
// ===========================================================================

// TEACHING NOTE — Packed structs
// #pragma pack ensures no compiler-inserted padding.  DDS structures must
// match the byte layout exactly as stored in the file.

#pragma pack(push, 1)

struct DDS_PIXELFORMAT {
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwFourCC;
    uint32_t dwRGBBitCount;
    uint32_t dwRBitMask;
    uint32_t dwGBitMask;
    uint32_t dwBBitMask;
    uint32_t dwABitMask;
};

struct DDS_HEADER {
    uint32_t         dwSize;         // Must be 124
    uint32_t         dwFlags;
    uint32_t         dwHeight;
    uint32_t         dwWidth;
    uint32_t         dwPitchOrLinearSize;
    uint32_t         dwDepth;
    uint32_t         dwMipMapCount;
    uint32_t         dwReserved1[11];
    DDS_PIXELFORMAT  ddspf;
    uint32_t         dwCaps;
    uint32_t         dwCaps2;
    uint32_t         dwCaps3;
    uint32_t         dwCaps4;
    uint32_t         dwReserved2;
};

// DDS_HEADER_DXT10 — present when ddspf.dwFourCC == 'DX10'
struct DDS_HEADER_DXT10 {
    uint32_t dxgiFormat;      // DXGI_FORMAT value
    uint32_t resourceDimension;
    uint32_t miscFlag;
    uint32_t arraySize;
    uint32_t miscFlags2;
};

#pragma pack(pop)

// Static size assertions — these must match the DDS spec exactly.
static_assert(sizeof(DDS_PIXELFORMAT) == 32, "DDS_PIXELFORMAT size mismatch");
static_assert(sizeof(DDS_HEADER)      == 124,"DDS_HEADER size mismatch");

// ===========================================================================
// Helper — bytes per row for a given format / width
// ===========================================================================

/**
 * @brief Compute the row pitch (bytes per row of texels) for a given format.
 *
 * TEACHING NOTE — Row Pitch for Block-Compressed Textures
 * ──────────────────────────────────────────────────────────
 * Block-compressed formats work on 4×4 texel blocks.  The minimum width is
 * one block (4 texels).  Row pitch = ceil(width/4) * bytesPerBlock.
 *
 *   BC1 / DXT1 = 8 bytes per 4×4 block (0.5 bytes/texel on average).
 *   BC3 / DXT5 = 16 bytes per 4×4 block (1 byte/texel on average).
 *   BC7        = 16 bytes per 4×4 block (same as BC3 but higher quality).
 *   RGBA8      = 4 bytes per texel.
 */
static uint32_t ComputeRowPitch(DXGI_FORMAT fmt, uint32_t width)
{
    switch (fmt)
    {
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
        return std::max(1u, (width + 3) / 4) * 8;

    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        return std::max(1u, (width + 3) / 4) * 16;

    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return width * 4;

    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
        return width * 4;

    default:
        // Unknown format — assume RGBA8 as a safe fallback.
        return width * 4;
    }
}

/**
 * @brief Compute the total byte size of one mip level.
 */
static uint32_t ComputeMipSize(DXGI_FORMAT fmt, uint32_t width, uint32_t height)
{
    switch (fmt)
    {
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
        return std::max(1u, (width + 3) / 4) * std::max(1u, (height + 3) / 4) * 8;

    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        return std::max(1u, (width + 3) / 4) * std::max(1u, (height + 3) / 4) * 16;

    default:
        return ComputeRowPitch(fmt, width) * height;
    }
}

// ===========================================================================
// D3D11Texture — constructor / destructor
// ===========================================================================

D3D11Texture::D3D11Texture()  = default;
D3D11Texture::~D3D11Texture() { Release(); }

D3D11Texture::D3D11Texture(D3D11Texture&& other) noexcept
    : m_srv(other.m_srv)
    , m_sampler(other.m_sampler)
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_format(other.m_format)
{
    other.m_srv     = nullptr;
    other.m_sampler = nullptr;
}

D3D11Texture& D3D11Texture::operator=(D3D11Texture&& other) noexcept
{
    if (this != &other) {
        Release();
        m_srv           = other.m_srv;
        m_sampler       = other.m_sampler;
        m_width         = other.m_width;
        m_height        = other.m_height;
        m_format        = other.m_format;
        other.m_srv     = nullptr;
        other.m_sampler = nullptr;
    }
    return *this;
}

// ===========================================================================
// Release
// ===========================================================================

void D3D11Texture::Release()
{
    // TEACHING NOTE — Release Order
    // The SRV holds an internal reference to the Texture2D.  We release the
    // SRV first so the Texture2D can be destroyed immediately after.
    if (m_srv)     { m_srv->Release();     m_srv     = nullptr; }
    if (m_sampler) { m_sampler->Release(); m_sampler = nullptr; }
    m_width  = 0;
    m_height = 0;
    m_format = DXGI_FORMAT_UNKNOWN;
}

// ===========================================================================
// LoadFromFile
// ===========================================================================

bool D3D11Texture::LoadFromFile(ID3D11Device*        device,
                                ID3D11DeviceContext*  context,
                                const std::string&   filePath)
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Binary File Reading
    // -----------------------------------------------------------------------
    // We open in binary mode (std::ios::binary) to prevent the C++ runtime
    // from translating CR/LF pairs on Windows, which would corrupt the data.
    // -----------------------------------------------------------------------
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        LOG_ERROR("D3D11Texture::LoadFromFile — file not found: " << filePath);
        return false;
    }

    const auto fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0);

    std::vector<uint8_t> bytes(fileSize);
    if (!file.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(fileSize)))
    {
        LOG_ERROR("D3D11Texture::LoadFromFile — read failed: " << filePath);
        return false;
    }

    return CreateFromBytes(device, context, bytes);
}

// ===========================================================================
// LoadFromMemory
// ===========================================================================

bool D3D11Texture::LoadFromMemory(ID3D11Device*              device,
                                  ID3D11DeviceContext*        context,
                                  const std::vector<uint8_t>& bytes)
{
    return CreateFromBytes(device, context, bytes);
}

// ===========================================================================
// CreateFromBytes (private)
// ===========================================================================

bool D3D11Texture::CreateFromBytes(ID3D11Device*              device,
                                   ID3D11DeviceContext*        /*context*/,
                                   const std::vector<uint8_t>& bytes)
{
    Release();

    if (bytes.size() < 4 + sizeof(DDS_HEADER))
    {
        LOG_ERROR("D3D11Texture — DDS bytes too small to contain a valid header.");
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 1 — Verify magic number.
    // -----------------------------------------------------------------------
    uint32_t magic = 0;
    std::memcpy(&magic, bytes.data(), 4);
    if (magic != DDS_MAGIC)
    {
        LOG_ERROR("D3D11Texture — DDS magic mismatch. Not a DDS file.");
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 2 — Parse DDS_HEADER.
    // -----------------------------------------------------------------------
    DDS_HEADER hdr;
    std::memcpy(&hdr, bytes.data() + 4, sizeof(DDS_HEADER));

    if (hdr.dwSize != 124)
    {
        LOG_ERROR("D3D11Texture — DDS_HEADER.dwSize != 124. Corrupted file.");
        return false;
    }

    m_width  = hdr.dwWidth;
    m_height = hdr.dwHeight;

    const uint32_t mipCount = (hdr.dwFlags & DDSD_MIPMAPCOUNT)
                               ? std::max(1u, hdr.dwMipMapCount)
                               : 1u;

    // -----------------------------------------------------------------------
    // Step 3 — Determine DXGI format.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — DDS Format Detection
    // ──────────────────────────────────────
    // Old DDS files (pre-DX10) use FourCC codes in DDS_PIXELFORMAT.
    // New DDS files (DX10 header) carry a DXGI_FORMAT value directly.
    // We detect which variant we have by checking for the 'DX10' FourCC.
    // -----------------------------------------------------------------------

    size_t dataOffset = 4 + sizeof(DDS_HEADER);
    const DDS_PIXELFORMAT& pf = hdr.ddspf;

    if ((pf.dwFlags & DDPF_FOURCC) && pf.dwFourCC == FOURCC_DX10)
    {
        // Extended header.
        if (bytes.size() < dataOffset + sizeof(DDS_HEADER_DXT10))
        {
            LOG_ERROR("D3D11Texture — DX10 header too small.");
            return false;
        }
        DDS_HEADER_DXT10 dx10;
        std::memcpy(&dx10, bytes.data() + dataOffset, sizeof(DDS_HEADER_DXT10));
        dataOffset += sizeof(DDS_HEADER_DXT10);
        m_format = static_cast<DXGI_FORMAT>(dx10.dxgiFormat);
    }
    else if (pf.dwFlags & DDPF_FOURCC)
    {
        // Legacy FourCC → DXGI_FORMAT mapping.
        switch (pf.dwFourCC)
        {
        case FOURCC_DXT1: m_format = DXGI_FORMAT_BC1_UNORM; break;
        case FOURCC_DXT3: m_format = DXGI_FORMAT_BC2_UNORM; break;
        case FOURCC_DXT5: m_format = DXGI_FORMAT_BC3_UNORM; break;
        default:
            LOG_ERROR("D3D11Texture — unsupported FourCC: 0x"
                      << std::hex << pf.dwFourCC << std::dec);
            return false;
        }
    }
    else if ((pf.dwFlags & DDPF_RGB) && pf.dwRGBBitCount == 32)
    {
        // Uncompressed 32-bit colour.  Assume RGBA8.
        m_format = DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    else
    {
        LOG_ERROR("D3D11Texture — unsupported DDS pixel format.");
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 4 — Build D3D11_SUBRESOURCE_DATA for each mip level.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Mip Map Upload
    // ───────────────────────────────
    // We pass all mip levels to CreateTexture2D in one call.
    // Each mip level has its own D3D11_SUBRESOURCE_DATA:
    //   pSysMem     = pointer to the mip's data in the DDS buffer.
    //   SysMemPitch = bytes per row for this mip's width.
    //
    // The subresource index is computed as: arraySlice * mipCount + mipIndex.
    // For a single 2D texture (arraySlice = 0), index == mipIndex.
    // -----------------------------------------------------------------------

    std::vector<D3D11_SUBRESOURCE_DATA> subresources(mipCount);
    size_t offset = dataOffset;

    for (uint32_t mip = 0; mip < mipCount; ++mip)
    {
        const uint32_t mipW = std::max(1u, m_width  >> mip);
        const uint32_t mipH = std::max(1u, m_height >> mip);
        const uint32_t pitch = ComputeRowPitch(m_format, mipW);
        const uint32_t mipBytes = ComputeMipSize(m_format, mipW, mipH);

        if (offset + mipBytes > bytes.size())
        {
            LOG_ERROR("D3D11Texture — DDS data truncated at mip " << mip);
            return false;
        }

        subresources[mip].pSysMem          = bytes.data() + offset;
        subresources[mip].SysMemPitch      = pitch;
        subresources[mip].SysMemSlicePitch = 0; // Only used for Texture3D.

        offset += mipBytes;
    }

    // -----------------------------------------------------------------------
    // Step 5 — Create ID3D11Texture2D.
    // -----------------------------------------------------------------------
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width              = m_width;
    texDesc.Height             = m_height;
    texDesc.MipLevels          = mipCount;
    texDesc.ArraySize          = 1;
    texDesc.Format             = m_format;
    texDesc.SampleDesc.Count   = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage              = D3D11_USAGE_IMMUTABLE;  // GPU read-only — fastest.
    texDesc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags     = 0;
    texDesc.MiscFlags          = 0;

    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = device->CreateTexture2D(&texDesc, subresources.data(), &tex);
    if (FAILED(hr))
    {
        LOG_ERROR("D3D11Texture — CreateTexture2D failed. HRESULT=0x"
                  << std::hex << static_cast<unsigned long>(hr) << std::dec
                  << " (" << m_width << "x" << m_height << " fmt=" << m_format << ")");
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 6 — Create ID3D11ShaderResourceView.
    // -----------------------------------------------------------------------
    // TEACHING NOTE — SRV for Texture2D with Mips
    // ────────────────────────────────────────────
    // D3D11_SRV_DIMENSION_TEXTURE2D tells D3D11 this is a 2D texture SRV.
    // MostDetailedMip = 0  → start from the highest-res mip.
    // MipLevels = -1       → include all mip levels down to the smallest.
    // -----------------------------------------------------------------------
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format                    = m_format;
    srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels       = static_cast<UINT>(-1); // all mips

    hr = device->CreateShaderResourceView(tex, &srvDesc, &m_srv);
    // Release our reference to the texture — the SRV holds its own reference.
    tex->Release();

    if (FAILED(hr))
    {
        LOG_ERROR("D3D11Texture — CreateShaderResourceView failed. HRESULT=0x"
                  << std::hex << static_cast<unsigned long>(hr) << std::dec);
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 7 — Create sampler state.
    // -----------------------------------------------------------------------
    if (!CreateSampler(device))
    {
        m_srv->Release();
        m_srv = nullptr;
        return false;
    }

    LOG_INFO("D3D11Texture loaded: " << m_width << "x" << m_height
             << " fmt=" << m_format << " mips=" << mipCount);
    return true;
}

// ===========================================================================
// CreateSampler (private)
// ===========================================================================

bool D3D11Texture::CreateSampler(ID3D11Device* device)
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Sampler States
    // -----------------------------------------------------------------------
    // A D3D11_SAMPLER_DESC controls how the GPU interpolates texels:
    //
    //   Filter       — how to blend between texels.
    //                  D3D11_FILTER_ANISOTROPIC uses the surrounding texels
    //                  weighted by angle of incidence — produces the sharpest
    //                  results on surfaces viewed at oblique angles (floors,
    //                  walls).  Bilinear is cheaper but blurrier at angles.
    //
    //   AddressU/V/W — what happens beyond [0,1] UV:
    //                  WRAP  = texture tiles (repeat).
    //                  CLAMP = edge pixel is stretched.
    //                  MIRROR = alternating flip.
    //
    //   MaxAnisotropy — quality level for anisotropic filtering (1–16).
    //                   D3D_FEATURE_LEVEL_10_0 supports up to 16×.
    //                   GT610 supports 16× anisotropic; use 4–8 for a
    //                   performance/quality balance on older hardware.
    //
    //   MipLODBias   — shifts the selected mip up or down.  0 = automatic.
    //   MinLOD/MaxLOD — clamps the mip level.  0 to FLT_MAX = all mips.
    // -----------------------------------------------------------------------
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter         = D3D11_FILTER_ANISOTROPIC;
    sampDesc.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.MaxAnisotropy  = 4;          // 4× anisotropic — good quality on GT610.
    sampDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sampDesc.MinLOD         = 0.0f;
    sampDesc.MaxLOD         = D3D11_FLOAT32_MAX;

    const HRESULT hr = device->CreateSamplerState(&sampDesc, &m_sampler);
    if (FAILED(hr))
    {
        LOG_ERROR("D3D11Texture — CreateSamplerState failed. HRESULT=0x"
                  << std::hex << static_cast<unsigned long>(hr) << std::dec);
        return false;
    }
    return true;
}

} // namespace rendering
} // namespace engine
