// =============================================================================
// triangle_skinned.vert - Phase 2G: PASSTHROUGH (compute-skinned).
// =============================================================================
// Skinning now happens once per frame in skinning.comp (Phase 2G), which writes
// model-LOCAL skinned position + normal into the SkinnedVertexPool streams. This
// vertex shader no longer does any bone math: it pulls the skinned position +
// normal from those streams (BDA) and applies only the per-draw model matrix.
//
// Per-draw data (model / materialId / alpha / vertex bases) lives in a
// SkinnedDrawData SSBO indexed by gl_InstanceIndex (firstInstance carries the
// slot) -- the same indirect-ready shape as the static triangle.vert. uv/color
// still come from the original block's vertex input (skin-invariant, not copied
// into the skinned streams = deinterleaved layout).
//
//   poolIndex = dstVertexBase + (gl_VertexIndex - srcVertexBase)
// =============================================================================
#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "shared/types.h"

// Original block vertex input: only the skin-invariant attributes are read.
// pos (loc 0) / normal (loc 3) / joints (4) / weights (5) are still bound by the
// pipeline's vertex layout but unused here (skinned values come from BDA).
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragWorldPos;
layout(location = 4) out vec4 fragLightPos;
layout(location = 5) out float fragAlpha;
layout(location = 6) flat out uint fragMaterialId;
// PART4 4a-2: motion vector clip positions (camera motion only for now).
layout(location = 7) out vec4 fragCurClip;
layout(location = 8) out vec4 fragPrevClip;

layout(set = 0, binding = 0) uniform UBO {
    FrameUBO frame;
} ubo;

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer SkinnedDrawBuffer {
    SkinnedDrawData data[];
};
layout(buffer_reference, std430, buffer_reference_align = 4) readonly buffer SkinnedPos {
    float p[];
};
layout(buffer_reference, std430, buffer_reference_align = 4) readonly buffer SkinnedNormal {
    uint oct[];
};

layout(push_constant) uniform PC {
    SkinnedDrawPushConstants push;
};

// Decode octahedral 2x SNORM16 (packed by skinning.comp's packOct16).
vec3 decodeOct16(uint packed) {
    int qx = int(packed << 16) >> 16;  // sign-extend low 16 bits
    int qy = int(packed) >> 16;        // sign-extend high 16 bits
    vec2 e = clamp(vec2(float(qx), float(qy)) / 32767.0, vec2(-1.0), vec2(1.0));
    vec3 n = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
    if (n.z < 0.0) {
        n.xy = (1.0 - abs(n.yx)) * vec2(n.x >= 0.0 ? 1.0 : -1.0, n.y >= 0.0 ? 1.0 : -1.0);
    }
    return normalize(n);
}

void main() {
    SkinnedDrawBuffer db = SkinnedDrawBuffer(push.drawBuffer);
    SkinnedDrawData d = db.data[gl_InstanceIndex];

    SkinnedPos    posBuf = SkinnedPos(push.posBuffer);
    SkinnedNormal nrmBuf = SkinnedNormal(push.normalBuffer);

    uint poolIdx = d.dstVertexBase + (uint(gl_VertexIndex) - d.srcVertexBase);
    vec3 skinnedPos = vec3(posBuf.p[poolIdx * 3u + 0u],
                           posBuf.p[poolIdx * 3u + 1u],
                           posBuf.p[poolIdx * 3u + 2u]);
    vec3 skinnedNormal = decodeOct16(nrmBuf.oct[poolIdx]);

    vec4 worldPos = d.model * vec4(skinnedPos, 1.0);
    vec4 curClip  = ubo.frame.proj * ubo.frame.view * worldPos;
    gl_Position = curClip;

    fragWorldPos = vec3(worldPos);
    fragLightPos = ubo.frame.lightVP * worldPos;
    fragNormal = normalize(mat3(d.model) * skinnedNormal);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragAlpha = d.alpha;
    fragMaterialId = d.materialId;
    // Per-object prev-pose history is a Phase 3 addition; current motion
    // captures camera movement (dominant TAA signal), matching triangle.vert.
    fragCurClip  = curClip;
    fragPrevClip = ubo.frame.prevViewProj * worldPos;
}
