/**
 * @file triangle.vert
 * @brief Vertex shader for the M1 triangle demo.
 *
 * ============================================================================
 * TEACHING NOTE — GLSL Vertex Shader Basics
 * ============================================================================
 * A vertex shader runs once per vertex submitted to the GPU.  Its mandatory
 * job is to write gl_Position — the clip-space position of the vertex.
 *
 * Input (per-vertex, read from the vertex buffer we upload in C++):
 *   location 0 — inPosition: 2D (x,y) position in NDC-like space.
 *   location 1 — inColor:    RGB colour for this vertex.
 *
 * Output (passed to the fragment shader via the rasteriser):
 *   location 0 — fragColor: linearly interpolated across the triangle.
 *
 * ============================================================================
 * TEACHING NOTE — Vulkan NDC vs OpenGL NDC
 * ============================================================================
 * In Vulkan's Normalised Device Coordinates (NDC):
 *   x ∈ [-1, +1]  left → right
 *   y ∈ [-1, +1]  TOP → bottom  (flipped relative to OpenGL!)
 *   z ∈ [ 0, +1]  near → far    (half range relative to OpenGL's -1..+1)
 *
 * Our triangle vertices use these NDC values directly to keep the shader
 * as transparent as possible for students.
 *
 * ============================================================================
 * TEACHING NOTE — GLSL #version 450
 * ============================================================================
 * We target GLSL 4.50, which corresponds to Vulkan 1.0+ SPIR-V.
 * The shader is compiled to SPIR-V by glslc (from the Vulkan SDK):
 *   glslc triangle.vert -o triangle.vert.spv
 *
 * SPIR-V is a binary intermediate representation — the GPU driver translates
 * it to its native ISA at pipeline-creation time.  Vulkan never accepts raw
 * GLSL; SPIR-V is the only accepted shader format.
 */

#version 450

// ============================================================================
// TEACHING NOTE — Vertex Input Bindings
// ============================================================================
// layout(location = N) binds this variable to vertex attribute slot N.
// The C++ VulkanMesh struct declares matching VkVertexInputAttributeDescription
// entries that tell Vulkan: "attribute 0 is a vec2 at byte offset 0; attribute 1
// is a vec3 at byte offset 8."
// ============================================================================
layout(location = 0) in vec2 inPosition;   // x, y in NDC
layout(location = 1) in vec3 inColor;      // r, g, b (0.0 – 1.0)

// Output to fragment shader — will be interpolated across the triangle face.
layout(location = 0) out vec3 fragColor;

void main()
{
    // ========================================================================
    // TEACHING NOTE — gl_Position
    // ========================================================================
    // gl_Position is a built-in vec4 output.  We promote our 2D position to
    // a homogeneous 4D clip-space coordinate by setting z=0 (on the near
    // plane) and w=1 (no perspective division needed for a flat 2D shape).
    //
    // The GPU will perspective-divide (x/w, y/w, z/w) to get NDC, then
    // map to the viewport rectangle.
    // ========================================================================
    gl_Position = vec4(inPosition, 0.0, 1.0);
    fragColor   = inColor;
}
