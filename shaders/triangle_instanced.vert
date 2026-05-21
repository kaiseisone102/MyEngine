// =============================================================================
// triangle_instanced.vert - Phase 1E: instanced static mesh via BDA SSBO
// =============================================================================
// Same lighting/output structure as triangle.vert, but the per-instance model
// matrix is read from a storage buffer (buffer_reference / BDA) indexed by
// gl_InstanceIndex instead of a single push-constant model.
//
// The instanceBuffer (64-bit GPU address in InstancedPushConstants) points to
// an array of mat4. vkCmdDrawIndexed's firstInstance arg sets the base, so
// gl_InstanceIndex already includes the per-draw offset into the pool.
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

// FrameUBO is defined in shared/types.h
layout(set = 0, binding = 0) uniform UBO {
    FrameUBO frame;
} ubo;

// Per-instance model matrices, accessed via BDA (same style as SkinMatrices).
layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer InstanceBuffer {
    InstanceData data[];
};

// InstancedPushConstants is defined in shared/types.h
layout(push_constant) uniform PC {
    InstancedPushConstants push;
};

void main() {
    // Cast the 64-bit address to a typed pointer, then index by instance.
    InstanceBuffer ib = InstanceBuffer(push.instanceBuffer);
    mat4 model = ib.data[gl_InstanceIndex].model;

    vec4 worldPos = model * vec4(inPosition, 1.0);
    gl_Position = ubo.frame.proj * ubo.frame.view * worldPos;

    fragWorldPos = vec3(worldPos);
    fragLightPos = ubo.frame.lightVP * worldPos;
    fragNormal = normalize(mat3(model) * inNormal);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragAlpha = push.alpha;
}
