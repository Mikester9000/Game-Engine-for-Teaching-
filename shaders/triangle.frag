/**
 * @file triangle.frag
 * @brief Fragment shader for the M1 triangle demo.
 *
 * ============================================================================
 * TEACHING NOTE — GLSL Fragment Shader Basics
 * ============================================================================
 * A fragment shader runs once per pixel (more precisely, per "fragment") that
 * the rasteriser generates from a triangle.  Its job is to decide the final
 * colour of that pixel.
 *
 * Input (interpolated by the rasteriser from the 3 vertex values):
 *   location 0 — fragColor: the vertex colour, linearly interpolated based
 *                on the fragment's barycentric coordinates inside the triangle.
 *
 * Output (written to the framebuffer attachment):
 *   location 0 — outColor: RGBA colour for this pixel.
 *
 * ============================================================================
 * TEACHING NOTE — Barycentric Interpolation
 * ============================================================================
 * When the rasteriser fills the triangle, each fragment receives a colour
 * that is the weighted average of the three vertex colours.  The weights are
 * the barycentric coordinates (u, v, 1-u-v) representing how close the
 * fragment is to each vertex.
 *
 * This is why the classic "hello triangle" displays red, green, and blue
 * corners blending smoothly in the centre — it is the minimal demo that
 * exercises both the vertex pipeline AND the fragment interpolation stage.
 *
 * ============================================================================
 * TEACHING NOTE — Output Location 0
 * ============================================================================
 * layout(location = 0) out ... targets colour attachment 0 in the render
 * pass.  Our render pass has exactly one colour attachment (the swapchain
 * image), so location 0 writes directly to the screen.
 */

#version 450

// Input from vertex shader — interpolated per-fragment colour.
layout(location = 0) in vec3 fragColor;

// Output colour for this pixel — written to swapchain colour attachment 0.
layout(location = 0) out vec4 outColor;

void main()
{
    // ========================================================================
    // TEACHING NOTE — Alpha Channel
    // ========================================================================
    // outColor.a = 1.0 means fully opaque.  We set alpha here even though the
    // swapchain surface compositing usually ignores the alpha channel on most
    // desktop platforms — it is good practice for when you switch to off-screen
    // render targets that DO use alpha (e.g. UI overlays, transparent meshes).
    // ========================================================================
    outColor = vec4(fragColor, 1.0);
}
