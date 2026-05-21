// =============================================================================
// grass_instanced.vert - Phase 1F: instanced grass (cross-quad) via BDA SSBO
// =============================================================================
// Same as triangle_instanced.vert (per-instance model matrix from a buffer_
// reference SSBO indexed by gl_InstanceIndex), but also forwards the bindless
// albedo texture index to the fragment shader so the grass texture can be
// sampled from the bindless array.
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
layout(location = 6) flat out int fragAlbedoIdx;
layout(location = 7) out vec4 fragInstColor;
layout(location = 8) out vec4 fragInstParams;

layout(set = 0, binding = 0) uniform UBO {
    FrameUBO frame;
} ubo;

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer InstanceBuffer {
    InstanceData data[];
};

layout(push_constant) uniform PC {
    InstancedPushConstants push;
};

void main() {
    InstanceBuffer ib = InstanceBuffer(push.instanceBuffer);
    mat4 model = ib.data[gl_InstanceIndex].model;
    vec4 instColor = ib.data[gl_InstanceIndex].color;
    vec4 instParams = ib.data[gl_InstanceIndex].params;

    vec4 worldPos = model * vec4(inPosition, 1.0);

    // Wind sway: tip bends more than the root, phase varies by world pos.
    // instParams.y = wind enable (0 = off, skips the whole calc).
    if (instParams.y > 0.5) {
        float tsec = ubo.frame.time.x;
        float phase = worldPos.x * 0.35 + worldPos.z * 0.35;
        float bend = inPosition.y * inPosition.y;  // 0 at root, 1 at tip
        worldPos.x += sin(tsec * 1.6 + phase) * 0.18 * bend;
        worldPos.z += cos(tsec * 1.3 + phase) * 0.12 * bend;
    }

    gl_Position = ubo.frame.proj * ubo.frame.view * worldPos;

    fragWorldPos = vec3(worldPos);
    fragLightPos = ubo.frame.lightVP * worldPos;
    fragNormal = normalize(mat3(model) * inNormal);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragAlpha = push.alpha;
    fragAlbedoIdx = push.albedoIdx;
    fragInstColor = instColor;
    fragInstParams = instParams;
}
