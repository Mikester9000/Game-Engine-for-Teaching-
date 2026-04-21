/**
 * @file font_renderer.cpp
 * @brief SDF FontRenderer implementation — D3D11 text rendering.
 *
 * ============================================================================
 * TEACHING NOTE — Implementation Overview
 * ============================================================================
 * This file implements the FontRenderer pipeline in four stages:
 *
 *   1. BuildAtlas()   — converts an embedded 8×8 bitmap font into a Signed
 *                       Distance Field atlas on the CPU.
 *   2. UploadAtlas()  — uploads the R8 atlas bytes to a D3D11 texture.
 *   3. CreateShaders()— compiles HLSL shaders at runtime (D3DCompileFromFile)
 *                       and creates the vertex input layout + blend state.
 *   4. CreateBuffers()— allocates a dynamic VB, static IB, and a constant
 *                       buffer for the screen dimensions.
 *
 * At render time, RenderText() fills the dynamic VB with one quad per
 * character and issues a single DrawIndexed() call.
 * ============================================================================
 *
 * C++ Standard: C++17
 * Target: Windows (MSVC, D3D11)
 */

#ifdef ENGINE_ENABLE_D3D11

#include "engine/ui/font_renderer.hpp"
#include "engine/core/Logger.hpp"

#include <d3dcompiler.h>   // D3DCompileFromFile
#include <cmath>           // std::sqrt, std::min
#include <algorithm>       // std::min, std::max, std::sort
#include <cstring>         // std::memset

// ---------------------------------------------------------------------------
// TEACHING NOTE — Embedded 8×8 Bitmap Font
// ---------------------------------------------------------------------------
// Each entry in kFont8x8[N][8] represents one glyph for ASCII character
// (N + 32).  The eight bytes are the eight pixel rows of the 8×8 glyph,
// from top (row 0) to bottom (row 7).
//
// Within each byte:
//   Bit 7 (0x80) = leftmost column  (column 0)
//   Bit 0 (0x01) = rightmost column (column 7)
//   1-bit = filled/ink pixel
//   0-bit = empty/background pixel
//
// This is the classic IBM PC "Code Page 437 / VGA 8×8 font" layout.
// It is public domain and well-known in the retro-computing community.
//
// Example — the letter 'A' (index 33 in the array = ASCII 65 = 'A'):
//   Row 0: 0x08 = 00001000  ....X...
//   Row 1: 0x14 = 00010100  ...X.X..
//   Row 2: 0x22 = 00100010  ..X...X.
//   Row 3: 0x3E = 00111110  ..XXXXX.
//   Row 4: 0x22 = 00100010  ..X...X.
//   Row 5: 0x22 = 00100010  ..X...X.
//   Row 6: 0x22 = 00100010  ..X...X.
//   Row 7: 0x00 = 00000000  ........
// ---------------------------------------------------------------------------
static const uint8_t kFont8x8[96][8] =
{
    // ASCII 32 — ' ' (space)
    { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 },
    // ASCII 33 — '!'
    { 0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00 },
    // ASCII 34 — '"'
    { 0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00 },
    // ASCII 35 — '#'
    { 0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00 },
    // ASCII 36 — '$'
    { 0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00 },
    // ASCII 37 — '%'
    { 0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00 },
    // ASCII 38 — '&'
    { 0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00 },
    // ASCII 39 — '\''
    { 0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00 },
    // ASCII 40 — '('
    { 0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00 },
    // ASCII 41 — ')'
    { 0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00 },
    // ASCII 42 — '*'
    { 0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00 },
    // ASCII 43 — '+'
    { 0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00 },
    // ASCII 44 — ','
    { 0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06 },
    // ASCII 45 — '-'
    { 0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00 },
    // ASCII 46 — '.'
    { 0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00 },
    // ASCII 47 — '/'
    { 0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00 },
    // ASCII 48 — '0'
    { 0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00 },
    // ASCII 49 — '1'
    { 0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00 },
    // ASCII 50 — '2'
    { 0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00 },
    // ASCII 51 — '3'
    { 0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00 },
    // ASCII 52 — '4'
    { 0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00 },
    // ASCII 53 — '5'
    { 0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00 },
    // ASCII 54 — '6'
    { 0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00 },
    // ASCII 55 — '7'
    { 0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00 },
    // ASCII 56 — '8'
    { 0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00 },
    // ASCII 57 — '9'
    { 0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00 },
    // ASCII 58 — ':'
    { 0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00 },
    // ASCII 59 — ';'
    { 0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06 },
    // ASCII 60 — '<'
    { 0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00 },
    // ASCII 61 — '='
    { 0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00 },
    // ASCII 62 — '>'
    { 0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00 },
    // ASCII 63 — '?'
    { 0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00 },
    // ASCII 64 — '@'
    { 0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00 },
    // ASCII 65 — 'A'
    { 0x08,0x14,0x22,0x3E,0x22,0x22,0x22,0x00 },
    // ASCII 66 — 'B'
    { 0x1E,0x22,0x22,0x1E,0x22,0x22,0x1E,0x00 },
    // ASCII 67 — 'C'
    { 0x1C,0x22,0x02,0x02,0x02,0x22,0x1C,0x00 },
    // ASCII 68 — 'D'
    { 0x0E,0x12,0x22,0x22,0x22,0x12,0x0E,0x00 },
    // ASCII 69 — 'E'
    { 0x3E,0x02,0x02,0x1E,0x02,0x02,0x3E,0x00 },
    // ASCII 70 — 'F'
    { 0x3E,0x02,0x02,0x1E,0x02,0x02,0x02,0x00 },
    // ASCII 71 — 'G'
    { 0x1C,0x22,0x02,0x3A,0x22,0x22,0x1C,0x00 },
    // ASCII 72 — 'H'
    { 0x22,0x22,0x22,0x3E,0x22,0x22,0x22,0x00 },
    // ASCII 73 — 'I'
    { 0x1C,0x08,0x08,0x08,0x08,0x08,0x1C,0x00 },
    // ASCII 74 — 'J'
    { 0x20,0x20,0x20,0x20,0x20,0x22,0x1C,0x00 },
    // ASCII 75 — 'K'
    { 0x22,0x12,0x0A,0x06,0x0A,0x12,0x22,0x00 },
    // ASCII 76 — 'L'
    { 0x02,0x02,0x02,0x02,0x02,0x02,0x3E,0x00 },
    // ASCII 77 — 'M'
    { 0x22,0x36,0x2A,0x2A,0x22,0x22,0x22,0x00 },
    // ASCII 78 — 'N'
    { 0x22,0x26,0x2A,0x32,0x22,0x22,0x22,0x00 },
    // ASCII 79 — 'O'
    { 0x1C,0x22,0x22,0x22,0x22,0x22,0x1C,0x00 },
    // ASCII 80 — 'P'
    { 0x1E,0x22,0x22,0x1E,0x02,0x02,0x02,0x00 },
    // ASCII 81 — 'Q'
    { 0x1C,0x22,0x22,0x22,0x2A,0x12,0x2C,0x00 },
    // ASCII 82 — 'R'
    { 0x1E,0x22,0x22,0x1E,0x0A,0x12,0x22,0x00 },
    // ASCII 83 — 'S'
    { 0x1C,0x22,0x02,0x1C,0x20,0x22,0x1C,0x00 },
    // ASCII 84 — 'T'
    { 0x3E,0x08,0x08,0x08,0x08,0x08,0x08,0x00 },
    // ASCII 85 — 'U'
    { 0x22,0x22,0x22,0x22,0x22,0x22,0x1C,0x00 },
    // ASCII 86 — 'V'
    { 0x22,0x22,0x22,0x14,0x14,0x08,0x08,0x00 },
    // ASCII 87 — 'W'
    { 0x22,0x22,0x22,0x2A,0x2A,0x36,0x22,0x00 },
    // ASCII 88 — 'X'
    { 0x22,0x22,0x14,0x08,0x14,0x22,0x22,0x00 },
    // ASCII 89 — 'Y'
    { 0x22,0x22,0x14,0x08,0x08,0x08,0x08,0x00 },
    // ASCII 90 — 'Z'
    { 0x3E,0x20,0x10,0x08,0x04,0x02,0x3E,0x00 },
    // ASCII 91 — '['
    { 0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00 },
    // ASCII 92 — '\'
    { 0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x00 },
    // ASCII 93 — ']'
    { 0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00 },
    // ASCII 94 — '^'
    { 0x08,0x14,0x22,0x00,0x00,0x00,0x00,0x00 },
    // ASCII 95 — '_'
    { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF },
    // ASCII 96 — '`'
    { 0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00 },
    // ASCII 97 — 'a'
    { 0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00 },
    // ASCII 98 — 'b'
    { 0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00 },
    // ASCII 99 — 'c'
    { 0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00 },
    // ASCII 100 — 'd'
    { 0x38,0x30,0x30,0x3E,0x33,0x33,0x6E,0x00 },
    // ASCII 101 — 'e'
    { 0x00,0x00,0x1E,0x33,0x3F,0x03,0x1E,0x00 },
    // ASCII 102 — 'f'
    { 0x1C,0x36,0x06,0x0F,0x06,0x06,0x0F,0x00 },
    // ASCII 103 — 'g'
    { 0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F },
    // ASCII 104 — 'h'
    { 0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00 },
    // ASCII 105 — 'i'
    { 0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00 },
    // ASCII 106 — 'j'
    { 0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E },
    // ASCII 107 — 'k'
    { 0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00 },
    // ASCII 108 — 'l'
    { 0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00 },
    // ASCII 109 — 'm'
    { 0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00 },
    // ASCII 110 — 'n'
    { 0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00 },
    // ASCII 111 — 'o'
    { 0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00 },
    // ASCII 112 — 'p'
    { 0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F },
    // ASCII 113 — 'q'
    { 0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78 },
    // ASCII 114 — 'r'
    { 0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00 },
    // ASCII 115 — 's'
    { 0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00 },
    // ASCII 116 — 't'
    { 0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00 },
    // ASCII 117 — 'u'
    { 0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00 },
    // ASCII 118 — 'v'
    { 0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00 },
    // ASCII 119 — 'w'
    { 0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00 },
    // ASCII 120 — 'x'
    { 0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00 },
    // ASCII 121 — 'y'
    { 0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F },
    // ASCII 122 — 'z'
    { 0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00 },
    // ASCII 123 — '{'
    { 0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00 },
    // ASCII 124 — '|'
    { 0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00 },
    // ASCII 125 — '}'
    { 0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00 },
    // ASCII 126 — '~'
    { 0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00 },
    // ASCII 127 — DEL (rendered as a filled block for visibility)
    { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF },
};

namespace engine { namespace ui {

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

FontRenderer::FontRenderer()  = default;
FontRenderer::~FontRenderer() { Shutdown(); }

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------
bool FontRenderer::Init(ID3D11Device*        device,
                        ID3D11DeviceContext* context,
                        const std::string&   shaderDir)
{
    if (m_initialised)
        return true;

    m_device  = device;
    m_context = context;

    if (!BuildAtlas())
    {
        LOG_ERROR("[FontRenderer] BuildAtlas() failed.");
        return false;
    }

    if (!UploadAtlas())
    {
        LOG_ERROR("[FontRenderer] UploadAtlas() failed.");
        return false;
    }

    if (!CreateShaders(shaderDir))
    {
        LOG_ERROR("[FontRenderer] CreateShaders() failed.");
        return false;
    }

    if (!CreateBuffers())
    {
        LOG_ERROR("[FontRenderer] CreateBuffers() failed.");
        return false;
    }

    m_initialised = true;
    LOG_INFO("[FontRenderer] Initialised — SDF atlas 128×48, max %d chars/draw.",
             kMaxCharsPerDraw);
    return true;
}

// ---------------------------------------------------------------------------
// Shutdown — release COM resources in reverse creation order.
// ---------------------------------------------------------------------------
void FontRenderer::Shutdown()
{
    // TEACHING NOTE — COM Release() pattern
    // Every D3D11 COM object must be explicitly Released() when no longer
    // needed.  We set the pointer to nullptr after each Release() so that:
    //   a) a second Shutdown() call is a safe no-op.
    //   b) the destructor won't double-free.
    //
    // Release order is the reverse of creation order — blend state was the
    // last thing created, so it is released first.
    if (m_blendState)  { m_blendState->Release();  m_blendState  = nullptr; }
    if (m_screenCB)    { m_screenCB->Release();    m_screenCB    = nullptr; }
    if (m_indexBuf)    { m_indexBuf->Release();    m_indexBuf    = nullptr; }
    if (m_vertexBuf)   { m_vertexBuf->Release();   m_vertexBuf   = nullptr; }
    if (m_inputLayout) { m_inputLayout->Release(); m_inputLayout = nullptr; }
    if (m_ps)          { m_ps->Release();          m_ps          = nullptr; }
    if (m_vs)          { m_vs->Release();          m_vs          = nullptr; }
    if (m_sampler)     { m_sampler->Release();     m_sampler     = nullptr; }
    if (m_atlasSRV)    { m_atlasSRV->Release();    m_atlasSRV    = nullptr; }
    if (m_atlasTex)    { m_atlasTex->Release();    m_atlasTex    = nullptr; }

    m_initialised = false;
}

// ---------------------------------------------------------------------------
// BuildAtlas — generate SDF atlas on the CPU.
// ---------------------------------------------------------------------------
bool FontRenderer::BuildAtlas()
{
    // TEACHING NOTE — SDF Generation Algorithm
    // ──────────────────────────────────────────
    // For each glyph we:
    //   1. Expand the 8-bit row masks into a bool[8][8] pixel grid.
    //   2. For each pixel (px, py):
    //      a. Find the nearest pixel with the OPPOSITE colour within a 4-pixel
    //         search radius.
    //      b. The signed distance = +dist (inside glyph) or -dist (outside).
    //      c. Normalise to [0,1]: sdfVal = clamp((dist + 4.0) / 8.0, 0, 1).
    //         At the edge dist=0 → sdfVal=0.5. Inside max dist=4 → sdfVal=1.
    //         Outside max dist=-4 → sdfVal=0.
    //   3. Write uint8_t(sdfVal * 255) into m_atlasData.

    static constexpr float kRadius   = 4.0f;  // maximum search distance in pixels
    static constexpr float kDiameter = 2.0f * kRadius;

    for (int glyphIdx = 0; glyphIdx < 96; ++glyphIdx)
    {
        // Step 1 — Expand row bitmasks to bool grid.
        bool pixels[8][8];
        for (int row = 0; row < 8; ++row)
        {
            uint8_t mask = kFont8x8[glyphIdx][row];
            for (int col = 0; col < 8; ++col)
            {
                // Bit 7 is the leftmost column.
                pixels[row][col] = (mask & (0x80u >> col)) != 0;
            }
        }

        // Compute atlas cell origin.
        int cellCol = glyphIdx % kAtlasCols;  // 0..15
        int cellRow = glyphIdx / kAtlasCols;  // 0..5
        int originX = cellCol * kGlyphPx;
        int originY = cellRow * kGlyphPx;

        // Step 2 — Compute SDF for each texel in the 8×8 cell.
        for (int py = 0; py < kGlyphPx; ++py)
        {
            for (int px = 0; px < kGlyphPx; ++px)
            {
                bool inside = pixels[py][px];

                // Search for the nearest pixel with the opposite colour.
                float minDist = kRadius;
                bool  foundEdge = false;

                int iRadius = static_cast<int>(kRadius);
                for (int qy = py - iRadius; qy <= py + iRadius; ++qy)
                {
                    for (int qx = px - iRadius; qx <= px + iRadius; ++qx)
                    {
                        // Clamp to glyph bounds — treat out-of-bounds as background.
                        bool qInside;
                        if (qx < 0 || qx >= 8 || qy < 0 || qy >= 8)
                            qInside = false;
                        else
                            qInside = pixels[qy][qx];

                        // Only consider pixels of the opposite type.
                        if (qInside == inside)
                            continue;

                        float dx = static_cast<float>(px - qx);
                        float dy = static_cast<float>(py - qy);
                        float d  = std::sqrt(dx * dx + dy * dy);

                        if (d < minDist)
                        {
                            minDist   = d;
                            foundEdge = true;
                        }
                    }
                }

                // TEACHING NOTE — Signed distance convention
                // If we didn't find any edge within the search radius we clamp
                // to ±kRadius.  Sign: inside = positive, outside = negative.
                float signedDist;
                if (!foundEdge)
                    signedDist = inside ? kRadius : -kRadius;
                else
                    signedDist = inside ? minDist : -minDist;

                // Normalise to [0, 1] with 0.5 at the edge.
                //   formula: clamp((signedDist + radius) / diameter, 0, 1)
                float sdfVal = (signedDist + kRadius) / kDiameter;
                if (sdfVal < 0.0f) sdfVal = 0.0f;
                if (sdfVal > 1.0f) sdfVal = 1.0f;

                // Write to atlas.
                m_atlasData[originY + py][originX + px] =
                    static_cast<uint8_t>(sdfVal * 255.0f);
            }
        }

        // Step 3 — Compute GlyphInfo UV coordinates.
        // TEACHING NOTE — UV normalisation
        // Atlas size is kAtlasWidthPx × kAtlasHeightPx = 128 × 48.
        // UVs divide pixel coordinates by the atlas dimensions.
        GlyphInfo& g = m_glyphs[glyphIdx];
        g.u0      = static_cast<float>(originX)             / kAtlasWidthPx;
        g.v0      = static_cast<float>(originY)             / kAtlasHeightPx;
        g.u1      = static_cast<float>(originX + kGlyphPx)  / kAtlasWidthPx;
        g.v1      = static_cast<float>(originY + kGlyphPx)  / kAtlasHeightPx;
        g.advance = 1.0f;  // monospace: always one cell width
    }

    return true;
}

// ---------------------------------------------------------------------------
// UploadAtlas — create D3D11 R8_UNORM texture + SRV + sampler.
// ---------------------------------------------------------------------------
bool FontRenderer::UploadAtlas()
{
    // TEACHING NOTE — R8_UNORM texture format
    // R8_UNORM stores one 8-bit channel per texel, normalised to [0, 1] in
    // the shader.  It is the ideal format for single-channel data like SDF
    // distance values.  Using RGBA8 would waste 75% of bandwidth and memory.
    D3D11_TEXTURE2D_DESC td = {};
    td.Width          = kAtlasWidthPx;
    td.Height         = kAtlasHeightPx;
    td.MipLevels      = 1;
    td.ArraySize      = 1;
    td.Format         = DXGI_FORMAT_R8_UNORM;
    td.SampleDesc     = { 1, 0 };
    td.Usage          = D3D11_USAGE_IMMUTABLE;  // never modified after upload
    td.BindFlags      = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA srd = {};
    srd.pSysMem          = m_atlasData;
    srd.SysMemPitch      = kAtlasWidthPx;  // bytes per row = 1 byte × 128 cols
    srd.SysMemSlicePitch = 0;

    HRESULT hr = m_device->CreateTexture2D(&td, &srd, &m_atlasTex);
    if (FAILED(hr))
    {
        LOG_ERROR("[FontRenderer] CreateTexture2D (atlas) failed: 0x%08X", hr);
        return false;
    }

    // Create a shader resource view so the PS can sample the texture.
    D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
    srvd.Format              = DXGI_FORMAT_R8_UNORM;
    srvd.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MipLevels = 1;

    hr = m_device->CreateShaderResourceView(m_atlasTex, &srvd, &m_atlasSRV);
    if (FAILED(hr))
    {
        LOG_ERROR("[FontRenderer] CreateShaderResourceView (atlas) failed: 0x%08X", hr);
        return false;
    }

    // TEACHING NOTE — Linear sampler for sub-texel smoothness
    // A linear (bilinear) sampler interpolates between neighbouring texels
    // when the UV falls between pixel centres.  This gives smooth rendering
    // especially at non-integer scales.  We use CLAMP address mode because
    // glyphs are tightly packed in the atlas and we must not bleed into
    // adjacent glyphs.
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.MinLOD         = 0.0f;
    sd.MaxLOD         = D3D11_FLOAT32_MAX;

    hr = m_device->CreateSamplerState(&sd, &m_sampler);
    if (FAILED(hr))
    {
        LOG_ERROR("[FontRenderer] CreateSamplerState failed: 0x%08X", hr);
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// CreateShaders — compile HLSL at runtime + create input layout + blend state.
// ---------------------------------------------------------------------------
bool FontRenderer::CreateShaders(const std::string& shaderDir)
{
    // TEACHING NOTE — Runtime HLSL Compilation
    // D3DCompileFromFile() compiles .hlsl source into a bytecode blob at
    // run-time.  This avoids the need for a build-step shader compiler (FXC)
    // and makes shader iteration fast during development.
    //
    // In a shipping game you would pre-compile to .cso files and load them
    // with D3DReadFileToBlob() to avoid the runtime compiler overhead.
    //
    // We use the "legacy" D3DCompileFromFile rather than DXC to keep the
    // dependency on d3dcompiler.lib only (no DXC DLL required).

    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* errBlob = nullptr;

    // Helper lambda to convert std::string path to wide string.
    auto toWide = [](const std::string& s) -> std::wstring {
        std::wstring w(s.begin(), s.end());
        return w;
    };

    // Compile Vertex Shader.
    std::string vsPath = shaderDir + "sdf_text.vs.hlsl";
    HRESULT hr = D3DCompileFromFile(
        toWide(vsPath).c_str(),
        nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main", "vs_4_0",
        D3DCOMPILE_ENABLE_STRICTNESS, 0,
        &vsBlob, &errBlob);
    if (FAILED(hr))
    {
        if (errBlob)
        {
            LOG_ERROR("[FontRenderer] VS compile error: %s",
                      (char*)errBlob->GetBufferPointer());
            errBlob->Release();
        }
        LOG_ERROR("[FontRenderer] D3DCompileFromFile VS failed: 0x%08X (path: %s)",
                  hr, vsPath.c_str());
        return false;
    }
    if (errBlob) { errBlob->Release(); errBlob = nullptr; }

    // Compile Pixel Shader.
    std::string psPath = shaderDir + "sdf_text.ps.hlsl";
    hr = D3DCompileFromFile(
        toWide(psPath).c_str(),
        nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main", "ps_4_0",
        D3DCOMPILE_ENABLE_STRICTNESS, 0,
        &psBlob, &errBlob);
    if (FAILED(hr))
    {
        if (errBlob)
        {
            LOG_ERROR("[FontRenderer] PS compile error: %s",
                      (char*)errBlob->GetBufferPointer());
            errBlob->Release();
        }
        vsBlob->Release();
        LOG_ERROR("[FontRenderer] D3DCompileFromFile PS failed: 0x%08X (path: %s)",
                  hr, psPath.c_str());
        return false;
    }
    if (errBlob) { errBlob->Release(); errBlob = nullptr; }

    // Create shader objects.
    hr = m_device->CreateVertexShader(
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        nullptr, &m_vs);
    if (FAILED(hr))
    {
        vsBlob->Release(); psBlob->Release();
        LOG_ERROR("[FontRenderer] CreateVertexShader failed: 0x%08X", hr);
        return false;
    }

    hr = m_device->CreatePixelShader(
        psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
        nullptr, &m_ps);
    if (FAILED(hr))
    {
        vsBlob->Release(); psBlob->Release();
        LOG_ERROR("[FontRenderer] CreatePixelShader failed: 0x%08X", hr);
        return false;
    }

    // TEACHING NOTE — Input Layout
    // The input layout tells D3D11 how to interpret the raw bytes of the
    // vertex buffer as named shader inputs.  The descriptor array must
    // exactly match the VSIn struct in sdf_text.vs.hlsl and the C++
    // TextVertex struct.
    //
    //   POSITION   → TextVertex.x, .y      (R32G32_FLOAT, 8 bytes)
    //   TEXCOORD0  → TextVertex.u, .v      (R32G32_FLOAT, 8 bytes)
    //   COLOR      → TextVertex.r,.g,.b,.a (R32G32B32A32_FLOAT, 16 bytes)
    //
    // Total stride = 10 floats × 4 bytes = 40 bytes per vertex.
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0,  0,
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0,  8,
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16,
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = m_device->CreateInputLayout(
        layoutDesc, 3,
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        &m_inputLayout);
    vsBlob->Release();
    psBlob->Release();

    if (FAILED(hr))
    {
        LOG_ERROR("[FontRenderer] CreateInputLayout failed: 0x%08X", hr);
        return false;
    }

    // TEACHING NOTE — Alpha Blend State for text compositing
    // Text must be drawn on top of the scene with correct transparency.
    // We use standard straight-alpha blending:
    //   out.rgb = src.rgb * src.a + dst.rgb * (1 - src.a)
    //
    // SrcBlend  = SRC_ALPHA       (multiply source by its own alpha)
    // DestBlend = INV_SRC_ALPHA   (multiply destination by 1-srcAlpha)
    //
    // BlendEnable must be TRUE per render target.  We only write to RT[0].
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;

    D3D11_RENDER_TARGET_BLEND_DESC& rt = blendDesc.RenderTarget[0];
    rt.BlendEnable           = TRUE;
    rt.SrcBlend              = D3D11_BLEND_SRC_ALPHA;
    rt.DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOp               = D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha         = D3D11_BLEND_ONE;
    rt.DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = m_device->CreateBlendState(&blendDesc, &m_blendState);
    if (FAILED(hr))
    {
        LOG_ERROR("[FontRenderer] CreateBlendState failed: 0x%08X", hr);
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// CreateBuffers — dynamic VB, static IB, ScreenCB.
// ---------------------------------------------------------------------------
bool FontRenderer::CreateBuffers()
{
    // TEACHING NOTE — Dynamic Vertex Buffer
    // Unlike a static VB (D3D11_USAGE_DEFAULT), a DYNAMIC VB can be updated
    // by the CPU every frame via Map(WRITE_DISCARD).  WRITE_DISCARD means
    // "give me a new region of GPU memory for this frame; discard the old
    // content."  This avoids stalling the GPU pipeline waiting for the
    // previous frame's VB to finish being consumed.
    //
    // The VB is sized for kMaxCharsPerDraw × kVertsPerChar vertices.
    const UINT vbSize = kMaxCharsPerDraw * kVertsPerChar * sizeof(TextVertex);
    D3D11_BUFFER_DESC vbd = {};
    vbd.ByteWidth      = vbSize;
    vbd.Usage          = D3D11_USAGE_DYNAMIC;
    vbd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = m_device->CreateBuffer(&vbd, nullptr, &m_vertexBuf);
    if (FAILED(hr))
    {
        LOG_ERROR("[FontRenderer] CreateBuffer (VB) failed: 0x%08X", hr);
        return false;
    }

    // TEACHING NOTE — Static Index Buffer
    // The index pattern for each character quad is always the same:
    //   vertex indices: 0,1,2, 0,2,3  (two triangles making a quad)
    // We pre-fill a static IB with this pattern for kMaxCharsPerDraw quads.
    // Using an IB instead of a non-indexed draw lets the GPU reuse vertices
    // (only 4 vertices needed per quad instead of 6).
    const UINT ibSize = kMaxCharsPerDraw * kIndicesPerChar * sizeof(uint16_t);
    std::vector<uint16_t> indices(kMaxCharsPerDraw * kIndicesPerChar);
    for (int i = 0; i < kMaxCharsPerDraw; ++i)
    {
        uint16_t base = static_cast<uint16_t>(i * 4);
        indices[i * 6 + 0] = base + 0;
        indices[i * 6 + 1] = base + 1;
        indices[i * 6 + 2] = base + 2;
        indices[i * 6 + 3] = base + 0;
        indices[i * 6 + 4] = base + 2;
        indices[i * 6 + 5] = base + 3;
    }

    D3D11_BUFFER_DESC ibd = {};
    ibd.ByteWidth  = ibSize;
    ibd.Usage      = D3D11_USAGE_IMMUTABLE;
    ibd.BindFlags  = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices.data();

    hr = m_device->CreateBuffer(&ibd, &ibData, &m_indexBuf);
    if (FAILED(hr))
    {
        LOG_ERROR("[FontRenderer] CreateBuffer (IB) failed: 0x%08X", hr);
        return false;
    }

    // TEACHING NOTE — Constant Buffer for screen dimensions
    // The VS needs to know the render target size (W, H) to convert pixel
    // coordinates to NDC.  We store this in a 16-byte constant buffer
    // (float2 + float2 padding — D3D11 CBs must be a multiple of 16 bytes).
    // The CB is DYNAMIC so we can update it cheaply with Map/WRITE_DISCARD.
    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth      = 16;   // float2 screenSize + float2 _pad = 16 bytes
    cbd.Usage          = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_device->CreateBuffer(&cbd, nullptr, &m_screenCB);
    if (FAILED(hr))
    {
        LOG_ERROR("[FontRenderer] CreateBuffer (ScreenCB) failed: 0x%08X", hr);
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// RenderText — build quads and draw.
// ---------------------------------------------------------------------------
void FontRenderer::RenderText(const std::string& text,
                               float x, float y, float scale,
                               float r, float g, float b, float a,
                               uint32_t screenW, uint32_t screenH)
{
    if (!m_initialised)
        return;

    // TEACHING NOTE — Quad building
    // Each ASCII character occupies a screen-space quad:
    //   TL = (curX,        curY)
    //   TR = (curX+scale,  curY)
    //   BL = (curX,        curY+scale)
    //   BR = (curX+scale,  curY+scale)
    //
    // Four vertices (in order TL, TR, BR, BL) index as 0,1,2,3.
    // The IB uses pattern 0,1,2, 0,2,3 to form two triangles covering the quad.
    //
    // After placing each character, advance curX by `scale` pixels.

    TextVertex verts[kMaxCharsPerDraw * kVertsPerChar];
    int charCount = 0;
    float curX = x;

    for (char c : text)
    {
        if (charCount >= kMaxCharsPerDraw)
            break;

        // Skip characters outside the printable ASCII range.
        if (c < 32 || c > 127)
        {
            // Treat as a space — still advance.
            curX += scale;
            continue;
        }

        const GlyphInfo& gi = m_glyphs[static_cast<int>(c) - 32];
        int v = charCount * kVertsPerChar;

        // Vertex 0 — Top-Left
        verts[v + 0] = { curX,         y,         gi.u0, gi.v0,  r, g, b, a };
        // Vertex 1 — Top-Right
        verts[v + 1] = { curX + scale,  y,         gi.u1, gi.v0,  r, g, b, a };
        // Vertex 2 — Bottom-Right
        verts[v + 2] = { curX + scale,  y + scale, gi.u1, gi.v1,  r, g, b, a };
        // Vertex 3 — Bottom-Left
        verts[v + 3] = { curX,         y + scale, gi.u0, gi.v1,  r, g, b, a };

        curX += scale * gi.advance;
        ++charCount;
    }

    if (charCount == 0)
        return;

    // TEACHING NOTE — Map/WRITE_DISCARD vertex buffer update
    // Map() with D3D11_MAP_WRITE_DISCARD returns a pointer to a fresh region
    // of GPU memory.  We copy our vertices in, then Unmap() to flush.
    // This is the correct pattern for a per-frame dynamic VB update — it
    // never stalls the GPU because the old content is discarded immediately.
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = m_context->Map(m_vertexBuf, 0,
                                D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr))
        return;

    std::memcpy(mapped.pData, verts,
                charCount * kVertsPerChar * sizeof(TextVertex));
    m_context->Unmap(m_vertexBuf, 0);

    // TEACHING NOTE — Update ScreenCB
    // Pack screenW and screenH as two floats.  The constant buffer has
    // 16 bytes (two float2 members): screenSize and _pad.
    {
        D3D11_MAPPED_SUBRESOURCE cbMapped = {};
        if (SUCCEEDED(m_context->Map(m_screenCB, 0,
                                     D3D11_MAP_WRITE_DISCARD, 0, &cbMapped)))
        {
            float data[4] = {
                static_cast<float>(screenW),
                static_cast<float>(screenH),
                0.0f, 0.0f  // padding
            };
            std::memcpy(cbMapped.pData, data, sizeof(data));
            m_context->Unmap(m_screenCB, 0);
        }
    }

    // --- Bind pipeline state and draw ---

    // Alpha blend for text-over-scene compositing.
    float blendFactor[4] = { 0,0,0,0 };
    m_context->OMSetBlendState(m_blendState, blendFactor, 0xFFFFFFFF);

    // Shaders and input layout.
    m_context->VSSetShader(m_vs, nullptr, 0);
    m_context->PSSetShader(m_ps, nullptr, 0);
    m_context->IASetInputLayout(m_inputLayout);

    // Constant buffer at VS slot b0 (screen size).
    m_context->VSSetConstantBuffers(0, 1, &m_screenCB);

    // Vertex and index buffers.
    UINT stride = sizeof(TextVertex);  // 40 bytes (2+2+4 floats × 4)
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, &m_vertexBuf, &stride, &offset);
    m_context->IASetIndexBuffer(m_indexBuf, DXGI_FORMAT_R16_UINT, 0);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Atlas SRV at PS slot t0 + sampler at s0.
    m_context->PSSetShaderResources(0, 1, &m_atlasSRV);
    m_context->PSSetSamplers(0, 1, &m_sampler);

    // TEACHING NOTE — DrawIndexed
    // indexCount = charCount * kIndicesPerChar (6 indices per quad = 2 triangles).
    // startIndexLocation = 0  (draw from the beginning of the IB).
    // baseVertexLocation = 0  (all vertices start at vertex 0).
    m_context->DrawIndexed(static_cast<UINT>(charCount * kIndicesPerChar), 0, 0);
}

}} // namespace engine::ui

#endif // ENGINE_ENABLE_D3D11
