// =============================================================================
// triangle.vert - Phase 2B PART3b: per-draw data via BDA SSBO (DrawData)
// =============================================================================
// model/materialId/alpha are read from a per-frame DrawData SSBO indexed by
// gl_InstanceIndex (vkCmdDrawIndexed's firstInstance carries the draw slot).
// The only push constant is the SSBO address. materialId is forwarded to the
// fragment shader as a flat varying. This is the indirect-ready form: PART3c
// swaps the CPU draw loop for vkCmdDrawIndexedIndirect with no shader change.
// =============================================================================
#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#include "shared/types.h"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragWorldPos;
layout(location = 4) out vec4 fragLightPos;
layout(location = 5) out float fragAlpha;
layout(location = 6) flat out uint fragMaterialId;

layout(set = 0, binding = 0) uniform UBO {
    FrameUBO frame;
} ubo;

// Per-draw data, accessed via BDA (same style as InstanceBuffer).
layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer DrawBuffer {
    DrawData data[];
};

layout(push_constant) uniform PC {
    StaticDrawPushConstants push;
};

void main() {
    DrawBuffer db = DrawBuffer(push.drawBuffer);
    DrawData d = db.data[gl_InstanceIndex];

    vec4 worldPos = d.model * vec4(inPosition, 1.0);
    gl_Position = ubo.frame.proj * ubo.frame.view * worldPos;
    fragWorldPos = vec3(worldPos);
    fragLightPos = ubo.frame.lightVP * worldPos;
    fragNormal = normalize(mat3(d.model) * inNormal);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragAlpha = d.alpha;
    fragMaterialId = d.materialId;
}