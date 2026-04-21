/**
 * @file font_renderer.hpp
 * @brief SDF (Signed Distance Field) font renderer — D3D11 screen-space text.
 *
 * ============================================================================
 * TEACHING NOTE — SDF Font Rendering Overview
 * ============================================================================
 * This module implements a complete GPU-accelerated text rendering system
 * using Signed Distance Field glyphs.  The pipeline is:
 *
 *   1. CPU: Embed an 8×8 bitmap font for ASCII 32-127 (96 printable chars).
 *   2. CPU: For each glyph, compute a signed distance field from the bitmap.
 *   3. GPU: Upload the SDF data as an R8_UNORM texture (the "atlas").
 *   4. GPU: At render time, build dynamic quads from the text string.
 *   5. GPU: In the pixel shader, threshold the SDF to recover crisp glyphs.
 *
 * Why is this useful for a teaching engine?
 *   • Shows how bitmap data is transformed into a more versatile representation.
 *   • Demonstrates CPU-side atlas generation and GPU texture upload.
 *   • Shows dynamic vertex buffer (Map/WRITE_DISCARD) update pattern.
 *   • The shaders (sdf_text.vs/ps.hlsl) are self-contained and heavily annotated.
 *
 * ============================================================================
 * TEACHING NOTE — Atlas Layout
 * ============================================================================
 * All 96 printable ASCII characters (32-127) are packed in a 128×48 pixel
 * texture:
 *
 *   kAtlasCols = 16 glyphs per row
 *   kAtlasRows =  6 rows
 *   kGlyphPx   =  8 pixels per glyph (both width and height)
 *
 *   ASCII ' ' (32) → column 0, row 0  → atlas pixel (0,0)..(7,7)
 *   ASCII '!' (33) → column 1, row 0  → atlas pixel (8,0)..(15,7)
 *   ASCII 'A' (65) → column 1, row 2  → atlas pixel (8,16)..(15,23)
 *
 * Character index = (char - 32).
 *   col = index % 16
 *   row = index / 16
 *
 * ============================================================================
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 * C++ Standard: C++17
 * Target: Windows (MSVC, D3D11)
 * Requires: d3d11.lib, d3dcompiler.lib (Windows SDK)
 */
#pragma once

#ifdef ENGINE_ENABLE_D3D11

#include <d3d11.h>
#include <string>
#include <cstdint>

namespace engine { namespace ui {

// ---------------------------------------------------------------------------
// TEACHING NOTE — GlyphInfo
// ---------------------------------------------------------------------------
// For each character in the atlas we store the UV corners (normalised 0..1)
// and the advance width.  For our fixed-width (monospace) 8×8 font the
// advance is always exactly 1 glyph cell, i.e. advance = 1.0.
//
// A proportional-width font would store different advances per character.
// ---------------------------------------------------------------------------
struct GlyphInfo
{
    float u0, v0;   // top-left  UV of glyph in atlas (0..1)
    float u1, v1;   // bottom-right UV of glyph in atlas (0..1)
    float advance;  // horizontal advance in glyph-cell widths (1.0 for monospace)
};

// ---------------------------------------------------------------------------
// FontRenderer
// ---------------------------------------------------------------------------
// Renders ASCII text to the D3D11 render target using an SDF glyph atlas.
//
// Usage pattern:
//   FontRenderer fr;
//   fr.Init(device, context, shaderDir);       // once at startup
//   // each frame (after scene rendering):
//   fr.RenderText("Score: 42", 10, 10, 16, 1,1,1,1, 1280, 720);
//   fr.Shutdown();                              // at teardown
// ---------------------------------------------------------------------------
class FontRenderer
{
public:
    FontRenderer();
    ~FontRenderer();

    // No copy — holds D3D11 COM resources.
    FontRenderer(const FontRenderer&)            = delete;
    FontRenderer& operator=(const FontRenderer&) = delete;

    // -----------------------------------------------------------------------
    // Init — build atlas, compile shaders, create GPU resources.
    //
    // Parameters:
    //   device     — the D3D11 logical device (for resource creation)
    //   context    — the immediate device context (for drawing)
    //   shaderDir  — directory containing sdf_text.vs.hlsl + sdf_text.ps.hlsl
    //
    // Returns true on full success; false if any D3D11 call fails.
    // -----------------------------------------------------------------------
    bool Init(ID3D11Device*        device,
              ID3D11DeviceContext* context,
              const std::string&   shaderDir);

    // -----------------------------------------------------------------------
    // Shutdown — release all D3D11 COM resources (COM Release() in reverse
    // creation order).
    // -----------------------------------------------------------------------
    void Shutdown();

    // Quick query — returns true after a successful Init().
    bool IsInitialised() const { return m_initialised; }

    // -----------------------------------------------------------------------
    // RenderText — draw a text string at pixel-space coordinates.
    //
    // Parameters:
    //   text      — UTF-8 string (only ASCII 32-127 are rendered; others skip)
    //   x, y      — top-left screen position in pixels
    //   scale     — glyph size in pixels (base is 8px; scale=16 → 16px high)
    //   r,g,b,a   — RGBA text colour (0..1 each)
    //   screenW/H — render target dimensions for NDC transform
    //
    // TEACHING NOTE — scale parameter
    // The source glyph is 8×8 pixels.  Setting scale=16 renders each glyph
    // as a 16×16 pixel quad.  The SDF sharpness is resolution-independent so
    // the same atlas works well from scale=8 (tiny) to scale=64 (banner text).
    // -----------------------------------------------------------------------
    void RenderText(const std::string& text,
                    float x, float y, float scale,
                    float r, float g, float b, float a,
                    uint32_t screenW, uint32_t screenH);

    // -----------------------------------------------------------------------
    // Atlas dimensions — public so callers can compute glyph positions.
    // -----------------------------------------------------------------------
    static constexpr int kAtlasCols     = 16;   // glyphs per atlas row
    static constexpr int kAtlasRows     =  6;   // atlas rows
    static constexpr int kGlyphPx       =  8;   // source glyph size in pixels
    static constexpr int kAtlasWidthPx  = kAtlasCols * kGlyphPx;  // 128
    static constexpr int kAtlasHeightPx = kAtlasRows  * kGlyphPx; // 48

private:
    // Build CPU-side SDF atlas from the embedded 8×8 bitmap font.
    bool BuildAtlas();

    // Upload m_atlasData to a D3D11 R8_UNORM texture + SRV.
    bool UploadAtlas();

    // Compile sdf_text.vs/ps.hlsl at runtime, create input layout + blend state.
    bool CreateShaders(const std::string& shaderDir);

    // Create dynamic VB, static IB, and ScreenCB constant buffer.
    bool CreateBuffers();

    // -----------------------------------------------------------------------
    // TEACHING NOTE — TextVertex layout
    // -----------------------------------------------------------------------
    // Each text quad corner carries:
    //   pos (x,y)  — screen-space pixel position  (transformed in VS)
    //   uv  (u,v)  — atlas texture coordinates    (sampled in PS)
    //   col (rgba) — RGBA colour                  (modulated in PS)
    //
    // Stride = 10 floats × 4 bytes = 40 bytes per vertex.
    // -----------------------------------------------------------------------
    struct TextVertex
    {
        float x, y;       // screen-space pixels (pre-NDC-transform)
        float u, v;       // atlas UV (0..1)
        float r, g, b, a; // RGBA colour
    };

    // Maximum glyphs per single RenderText() draw call.
    static constexpr int kMaxCharsPerDraw = 1024;
    static constexpr int kVertsPerChar    = 4;
    static constexpr int kIndicesPerChar  = 6;

    // -----------------------------------------------------------------------
    // D3D11 COM objects — raw pointers, released in Shutdown().
    // -----------------------------------------------------------------------
    ID3D11Device*             m_device      = nullptr;
    ID3D11DeviceContext*      m_context     = nullptr;

    // Atlas texture + view
    ID3D11Texture2D*          m_atlasTex    = nullptr;
    ID3D11ShaderResourceView* m_atlasSRV    = nullptr;
    ID3D11SamplerState*       m_sampler     = nullptr;

    // Shader pipeline
    ID3D11VertexShader*       m_vs          = nullptr;
    ID3D11PixelShader*        m_ps          = nullptr;
    ID3D11InputLayout*        m_inputLayout = nullptr;

    // Geometry buffers
    ID3D11Buffer*             m_vertexBuf   = nullptr;
    ID3D11Buffer*             m_indexBuf    = nullptr;
    ID3D11Buffer*             m_screenCB    = nullptr;

    // Alpha blend state for text-over-scene compositing
    ID3D11BlendState*         m_blendState  = nullptr;

    // CPU-side SDF atlas — one R8 byte per texel.
    uint8_t m_atlasData[kAtlasHeightPx][kAtlasWidthPx] = {};

    // Per-glyph UV + advance, indexed by (char - 32).
    GlyphInfo m_glyphs[96] = {};

    bool m_initialised = false;
};

}} // namespace engine::ui

#endif // ENGINE_ENABLE_D3D11
