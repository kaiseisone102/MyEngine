// =============================================================================
// shadow_skinned.vert - Phase 1A2: uses shared types.h
// =============================================================================
#version 450
#extension GL_GOOGLE_include_directive : require
#include "shared/types.h"

layout(location = 0) in vec3  inPosition;
layout(location = 4) in ivec4 inJointIndices;
layout(location = 5) in vec4  inJointWeights;

layout(set = 0, binding = 0) uniform UBO {
    FrameUBO frame;
} ubo;

layout(set = 1, binding = 0) readonly buffer SkinMatrices {
    mat4 boneMatrices[];
} skin;

layout(push_constant) uniform PC {
    ShadowSkinnedPushConstants push;
};

void main() {
    int base = push.skinOffset;
    mat4 skinMatrix =
        skin.boneMatrices[base + inJointIndices.x] * inJointWeights.x
      + skin.boneMatrices[base + inJointIndices.y] * inJointWeights.y
      + skin.boneMatrices[base + inJointIndices.z] * inJointWeights.z
      + skin.boneMatrices[base + inJointIndices.w] * inJointWeights.w;

    vec4 skinnedPos = skinMatrix * vec4(inPosition, 1.0);
    gl_Position = ubo.frame.lightVP * push.model * skinnedPos;
}
