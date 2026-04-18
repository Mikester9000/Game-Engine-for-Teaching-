/**
 * @file d3d11_texture.hpp
 * @brief D3D11 DDS texture loader — creates ID3D11ShaderResourceView from .dds files.
 *
 * ============================================================================
 * TEACHING NOTE — Why DDS for Game Textures?
 * ============================================================================
 * PNG and JPEG store full RGBA8 data.  A 2048×2048 RGBA8 texture uses 16 MB
 * of GPU VRAM.  DDS (DirectDraw Surface) with block compression:
 *
 *   BC1 (DXT1)  — 4 bits/pixel (8:1 ratio vs RGBA8).  No alpha.
 *   BC3 (DXT5)  — 8 bits/pixel (4:1 ratio).  Full alpha channel.
 *   BC7          — 8 bits/pixel (4:1 ratio).  High quality; replaces BC3.
 *
 * A 2048×2048 BC7 texture uses only 4 MB instead of 16 MB — critical for
 * GT610-class GPUs which have 1–2 GB VRAM at most.
 *
 * BC7 is hardware-decompressed on every D3D11_FEATURE_LEVEL_11_0 GPU
 * (including the GT610) with zero GPU cost.  The CPU never sees the raw pixels.
 *
 * ============================================================================
 * TEACHING NOTE — DDS File Format
 * ============================================================================
 * DDS is a Microsoft container format with this layout:
 *
 *   Offset  Size   Content
 *   ------  ----   -------
 *        0     4   Magic: 0x20534444 ('DDS ')
 *        4   124   DDS_HEADER struct
 *      128   var   DDS_HEADER_DXT10 (optional, present when FourCC == 'DX10')
 *      128+  var   Texture data (mip 0, then mip 1, ...; face 0, face 1, ...)
 *
 * The DDS_HEADER_DXT10 extension (introduced in Direct3D 10) carries the
 * DXGI_FORMAT enum, enabling formats like BC7 that didn't exist in earlier
 * DDS revisions.
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Target: Windows (MSVC)
 * Requires: d3d11.lib, dxgi.lib (Windows SDK — always present)
 */

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>

#include <cstdint>
#include <string>
#include <vector>

namespace engine {
namespace rendering {

// ===========================================================================
// D3D11Texture
// ===========================================================================

/**
 * @class D3D11Texture
 * @brief Loads a DDS file into a D3D11 shader resource view.
 *
 * TEACHING NOTE — Resource View Types
 * ──────────────────────────────────────
 * D3D11 separates resource *creation* from resource *binding*:
 *
 *   ID3D11Texture2D          — the raw texture data on the GPU.
 *   ID3D11ShaderResourceView — a "window" into that data that a shader can
 *                              read via a `Texture2D` or `sampler2D`.
 *   ID3D11SamplerState       — filtering and wrap mode settings.
 *
 * Shaders never take raw Texture2D pointers — they always bind through a
 * ShaderResourceView.  This indirection allows one texture to be bound as
 * different "views" (e.g. one mip level at a time).
 *
 * Usage:
 * @code
 *   D3D11Texture tex;
 *   if (tex.LoadFromFile(device, context, "Cooked/textures/hero.dds")) {
 *       ID3D11ShaderResourceView* srv = tex.GetSRV();
 *       ID3D11SamplerState*       smp = tex.GetSampler();
 *       ctx->PSSetShaderResources(0, 1, &srv);
 *       ctx->PSSetSamplers(0, 1, &smp);
 *   }
 * @endcode
 */
class D3D11Texture
{
public:
    D3D11Texture();
    ~D3D11Texture();

    // Non-copyable — owns COM pointers.
    D3D11Texture(const D3D11Texture&)            = delete;
    D3D11Texture& operator=(const D3D11Texture&) = delete;

    // Move semantics — transfer COM ownership.
    D3D11Texture(D3D11Texture&& other) noexcept;
    D3D11Texture& operator=(D3D11Texture&& other) noexcept;

    // -----------------------------------------------------------------------
    // Loading
    // -----------------------------------------------------------------------

    /**
     * @brief Load a DDS texture from a file path.
     *
     * Supports:
     *   - Uncompressed RGBA8 (DXGI_FORMAT_R8G8B8A8_UNORM)
     *   - BC1 / DXT1        (DXGI_FORMAT_BC1_UNORM)
     *   - BC3 / DXT5        (DXGI_FORMAT_BC3_UNORM)
     *   - BC7               (DXGI_FORMAT_BC7_UNORM, DX10 header required)
     *
     * @param device   D3D11 device (for resource creation).
     * @param context  Device context (for GenerateMips, if any).
     * @param filePath Absolute path to the .dds file.
     * @return true on success.
     */
    bool LoadFromFile(ID3D11Device*        device,
                      ID3D11DeviceContext*  context,
                      const std::string&   filePath);

    /**
     * @brief Load a DDS texture from raw bytes (e.g. from AssetLoader).
     *
     * @param device   D3D11 device.
     * @param context  Device context.
     * @param bytes    Raw .dds file bytes.
     * @return true on success.
     */
    bool LoadFromMemory(ID3D11Device*              device,
                        ID3D11DeviceContext*        context,
                        const std::vector<uint8_t>& bytes);

    /**
     * @brief Release all D3D11 resources.  Safe to call multiple times.
     */
    void Release();

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------

    /** @return The shader resource view, or nullptr if not loaded. */
    ID3D11ShaderResourceView* GetSRV()     const { return m_srv; }

    /** @return The sampler state (bilinear + anisotropic). */
    ID3D11SamplerState*       GetSampler() const { return m_sampler; }

    /** @return Width of the base mip level in texels. */
    uint32_t Width()  const { return m_width; }

    /** @return Height of the base mip level in texels. */
    uint32_t Height() const { return m_height; }

    /** @return DXGI_FORMAT of the texture. */
    DXGI_FORMAT Format() const { return m_format; }

    /** @return True if the texture was loaded successfully. */
    bool IsLoaded() const { return m_srv != nullptr; }

private:
    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Parse DDS bytes and create D3D11 resources.
     * @return true on success.
     */
    bool CreateFromBytes(ID3D11Device*              device,
                         ID3D11DeviceContext*        context,
                         const std::vector<uint8_t>& bytes);

    /**
     * @brief Create a default bilinear + anisotropic sampler state.
     * @return true on success.
     */
    bool CreateSampler(ID3D11Device* device);

    // -----------------------------------------------------------------------
    // D3D11 objects
    // -----------------------------------------------------------------------

    ID3D11ShaderResourceView* m_srv     = nullptr;
    ID3D11SamplerState*       m_sampler = nullptr;

    uint32_t    m_width   = 0;
    uint32_t    m_height  = 0;
    DXGI_FORMAT m_format  = DXGI_FORMAT_UNKNOWN;
};

} // namespace rendering
} // namespace engine
