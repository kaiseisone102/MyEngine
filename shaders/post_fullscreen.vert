#version 450
// =============================================================================
// post_fullscreen.vert - Phase 1H-3: fullscreen triangle (no vertex buffer)
//
// Trick: 3 vertices cover the entire screen with a single oversized triangle.
// gl_VertexIndex = 0 -> uv = (0, 0), pos = (-1, -1)
// gl_VertexIndex = 1 -> uv = (2, 0), pos = ( 3, -1)
// gl_VertexIndex = 2 -> uv = (0, 2), pos = (-1,  3)
//
// The triangle extends beyond [-1, 1] but the unseen parts are clipped.
// This avoids vertex buffer setup entirely. Used everywhere in modern engines
// for post-processing passes.
// =============================================================================

layout(location = 0) out vec2 fragUV;

void main() {
    fragUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(fragUV * 2.0 - 1.0, 0.0, 1.0);
}
