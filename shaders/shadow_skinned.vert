// =============================================================================
// shadow_skinned.vert - Phase 1B-4b: BDA (buffer_reference) for skin matrices
// =============================================================================
// See triangle_skinned.vert for the BDA approach explanation.
// This shader uses ShadowSkinnedPushConstants (no alpha field).
// =============================================================================
#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "shared/types.h"

layout(location = 0) in vec3  inPosition;
layout(location = 4) in ivec4 inJointIndices;
layout(location = 5) in vec4  inJointWeights;

layout(set = 0, binding = 0) uniform UBO {
    FrameUBO frame;
} ubo;

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer SkinMatrices {
    mat4 boneMatrices[];
};

layout(push_constant) uniform PC {
    ShadowSkinnedPushConstants push;
};

void main() {
    SkinMatrices skin = SkinMatrices(push.skinBuffer);

    int base = push.skinOffset;
    mat4 skinMatrix =
        skin.boneMatrices[base + inJointIndices.x] * inJointWeights.x
      + skin.boneMatrices[base + inJointIndices.y] * inJointWeights.y
      + skin.boneMatrices[base + inJointIndices.z] * inJointWeights.z
      + skin.boneMatrices[base + inJointIndices.w] * inJointWeights.w;

    vec4 skinnedPos = skinMatrix * vec4(inPosition, 1.0);
    gl_Position = ubo.frame.lightVP * push.model * skinnedPos;
}
