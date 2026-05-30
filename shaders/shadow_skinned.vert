// =============================================================================
// shadow_skinned.vert - Phase 2G: PASSTHROUGH (compute-skinned), depth-only.
// =============================================================================
// Skinning happens once per frame in skinning.comp; this shader pulls the
// model-local skinned position from the SkinnedVertexPool stream (BDA) and
// applies the per-draw model + light view-projection. No bone math, no normal.
// Per-draw model comes from the SkinnedDrawData SSBO via gl_InstanceIndex.
//   poolIndex = dstVertexBase + (gl_VertexIndex - srcVertexBase)
// =============================================================================
#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "shared/types.h"

layout(set = 0, binding = 0) uniform UBO {
    FrameUBO frame;
} ubo;

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer SkinnedDrawBuffer {
    SkinnedDrawData data[];
};
layout(buffer_reference, std430, buffer_reference_align = 4) readonly buffer SkinnedPos {
    float p[];
};

layout(push_constant) uniform PC {
    SkinnedShadowDrawPushConstants push;
};

void main() {
    SkinnedDrawBuffer db = SkinnedDrawBuffer(push.drawBuffer);
    SkinnedDrawData d = db.data[gl_InstanceIndex];
    SkinnedPos posBuf = SkinnedPos(push.posBuffer);

    uint poolIdx = d.dstVertexBase + (uint(gl_VertexIndex) - d.srcVertexBase);
    vec3 skinnedPos = vec3(posBuf.p[poolIdx * 3u + 0u],
                           posBuf.p[poolIdx * 3u + 1u],
                           posBuf.p[poolIdx * 3u + 2u]);

    gl_Position = ubo.frame.lightVP * d.model * vec4(skinnedPos, 1.0);
}
