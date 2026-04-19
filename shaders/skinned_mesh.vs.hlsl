/**
 * skinned_mesh.vs.hlsl
 * Vertex shader for GPU-skinned skeletal meshes.
 *
 * ============================================================================
 * TEACHING NOTE — GPU Skinning Vertex Shader
 * ============================================================================
 * This vertex shader implements "linear blend skinning" (LBS), the standard
 * skeletal deformation technique used in virtually every real-time game engine.
 *
 * Each vertex has up to 4 bone influences, each described by:
 *   boneIndex[i]  — which joint matrix to use (index into g_joints array)
 *   boneWeight[i] — how much that joint contributes (weights sum to 1.0)
 *
 * The skinned position is the weighted average of the bind-pose vertex
 * transformed by each contributing joint matrix:
 *
 *   worldPos = sum_i( boneWeight[i] * (g_joints[boneIndex[i]] * bindPos) )
 *
 * ============================================================================
 * TEACHING NOTE — Constant Buffer Layout
 * ============================================================================
 * g_joints[64] is the array of skin matrices uploaded by GpuSkinningBuffer.
 * Each skin matrix is:
 *
 *   skinMatrix[i] = invBindMatrix[i] * worldMatrix[i]
 *
 * where:
 *   invBindMatrix[i] = inverse of the joint's bind-pose world matrix.
 *   worldMatrix[i]   = current world matrix of the joint (from animation).
 *
 * Multiplying by invBindMatrix first "undoes" the bind pose (moves the vertex
 * into the joint's local space), then worldMatrix places it in current world
 * space.
 *
 * ============================================================================
 * TEACHING NOTE — Normal Transform
 * ============================================================================
 * For correct lighting, normals must be transformed by the inverse transpose
 * of the model-space matrix.  For rigid-body (rotation + translation) skin
 * matrices there is no non-uniform scale, so the inverse transpose equals
 * the original rotation sub-matrix.  We can safely use the upper 3×3 of the
 * skin matrix to transform normals — it is already a pure rotation matrix.
 *
 * ============================================================================
 * TEACHING NOTE — Shader Model 4.0 (Feature Level 10_0)
 * ============================================================================
 * We target vs_4_0 (Shader Model 4.0) to remain compatible with the project's
 * minimum hardware: GeForce GT 610 / D3D_FEATURE_LEVEL_10_0.
 * SM4.0 supports integer inputs (uint4 BLENDINDICES) and dynamic array
 * indexing into constant buffers — both required for GPU skinning.
 *
 * ============================================================================
 *
 * Shader Model: vs_4_0
 * Compile:      D3DCompileFromFile("skinned_mesh.vs.hlsl", "main", "vs_4_0")
 */

// ---------------------------------------------------------------------------
// Constant buffer b0 — joint matrices (64 × float4x4 = 4096 bytes)
// ---------------------------------------------------------------------------
// TEACHING NOTE — cbuffer register(b0)
// In D3D11, each cbuffer must be assigned to a named register (b0..b13).
// The CPU binds the GpuSkinningBuffer to VS slot 0 with:
//   context->VSSetConstantBuffers(0, 1, &m_cbuffer);
// This matches register(b0) in HLSL.
// ---------------------------------------------------------------------------
cbuffer JointCB : register(b0)
{
    float4x4 g_joints[64];   // Skin matrices for up to 64 joints
};

// ---------------------------------------------------------------------------
// Vertex input (from the vertex buffer, described by D3D11_INPUT_ELEMENT_DESC)
// ---------------------------------------------------------------------------
struct VSInput
{
    float3   pos         : POSITION;       // Bind-pose position (model space)
    float3   normal      : NORMAL;         // Bind-pose normal   (model space)
    float2   uv          : TEXCOORD0;      // Texture coordinates
    uint4    boneIndex   : BLENDINDICES;   // Up to 4 joint indices
    float4   boneWeight  : BLENDWEIGHT;    // Corresponding weights (sum = 1.0)
};

// ---------------------------------------------------------------------------
// Vertex output (interpolated and passed to the pixel shader)
// ---------------------------------------------------------------------------
struct PSInput
{
    float4 pos    : SV_POSITION;   // Clip-space position (output of VS)
    float3 normal : NORMAL;        // World-space normal for lighting
    float2 uv     : TEXCOORD0;     // Texture UV for color/texture sampling
};

// ---------------------------------------------------------------------------
// main — vertex shader entry point
// ---------------------------------------------------------------------------
PSInput main(VSInput i)
{
    // -----------------------------------------------------------------------
    // TEACHING NOTE — Weighted Sum Skinning (Linear Blend Skinning, LBS)
    // -----------------------------------------------------------------------
    // We accumulate the contribution of up to 4 joints into a single
    // world-space position and normal.
    //
    // For each bone influence b:
    //   contribution = boneWeight[b] * (g_joints[boneIndex[b]] * bindPos)
    //
    // We use float4 to carry the w=1 homogeneous coordinate through the
    // matrix multiply.  The final skinned position has w from the matrices,
    // but because our matrices have no projective component, w will always
    // be 1.0 (verified below with /w correction as a safety measure).
    // -----------------------------------------------------------------------
    float4 skinnedPos    = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float3 skinnedNormal = float3(0.0f, 0.0f, 0.0f);

    // TEACHING NOTE — [unroll] hints the compiler to unroll this loop.
    // With exactly 4 iterations the compiler generates 4 independent MADs
    // (multiply-accumulate) which execute in parallel on the ALU.
    [unroll]
    for (int b = 0; b < 4; ++b)
    {
        float  w   = i.boneWeight[b];
        uint   idx = i.boneIndex[b];

        // TEACHING NOTE — mul(row_vector, matrix) in HLSL.
        // D3D11 HLSL uses row-major convention by default.
        // mul(float4(pos, 1), M) is the standard row-vector × matrix form,
        // which matches the row-major Mat4 we upload from C++ via memcpy.
        skinnedPos    += w * mul(float4(i.pos, 1.0f), g_joints[idx]);

        // TEACHING NOTE — Normal transform using the upper 3×3 sub-matrix.
        // (float3x3)M extracts the top-left 3×3 from a float4x4.
        // For rigid-body (TRS with uniform scale) matrices this IS the
        // correct normal transform — no need for the full inverse-transpose.
        skinnedNormal += w * mul(i.normal, (float3x3)g_joints[idx]);
    }

    // -----------------------------------------------------------------------
    // Output assembly
    // -----------------------------------------------------------------------
    PSInput o;

    // TEACHING NOTE — Homogeneous divide safety.
    // After LBS, the w component should always be 1.0 for rigid-body matrices.
    // We divide by skinnedPos.w as a safety measure against unexpected matrix
    // configurations (e.g. a student adds a perspective matrix to the joints).
    // For the standard case w == 1.0 so this is free (divide by 1).
    float wInv = (abs(skinnedPos.w) > 0.0001f) ? (1.0f / skinnedPos.w) : 1.0f;
    o.pos    = float4(skinnedPos.xyz * wInv, 1.0f);

    // Normalise the accumulated blended normal.
    o.normal = normalize(skinnedNormal);
    o.uv     = i.uv;

    return o;
}
