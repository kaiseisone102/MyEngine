// =============================================================================
// triangle_skinned.vert - Phase 1A2: uses shared types.h
// =============================================================================
#version 450
#extension GL_GOOGLE_include_directive : require
#include "shared/types.h"

layout(location = 0) in vec3  inPosition;
layout(location = 1) in vec3  inColor;
layout(location = 2) in vec2  inTexCoord;
layout(location = 3) in vec3  inNormal;
layout(location = 4) in ivec4 inJointIndices;
layout(location = 5) in vec4  inJointWeights;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragWorldPos;
layout(location = 4) out vec4 fragLightPos;
layout(location = 5) out float fragAlpha;

layout(set = 0, binding = 0) uniform UBO {
    FrameUBO frame;
} ubo;

layout(set = 2, binding = 0) readonly buffer SkinMatrices {
    mat4 boneMatrices[];
} skin;

layout(push_constant) uniform PC {
    SkinnedPushConstants push;
};

void main() {
    int base = push.skinOffset;
    mat4 skinMatrix =
        skin.boneMatrices[base + inJointIndices.x] * inJointWeights.x
      + skin.boneMatrices[base + inJointIndices.y] * inJointWeights.y
      + skin.boneMatrices[base + inJointIndices.z] * inJointWeights.z
      + skin.boneMatrices[base + inJointIndices.w] * inJointWeights.w;

    vec4 skinnedPos = skinMatrix * vec4(inPosition, 1.0);
    vec3 skinnedNormal = mat3(skinMatrix) * inNormal;

    vec4 worldPos = push.model * skinnedPos;
    gl_Position = ubo.frame.proj * ubo.frame.view * worldPos;

    fragWorldPos = vec3(worldPos);
    fragLightPos = ubo.frame.lightVP * worldPos;
    fragNormal = normalize(mat3(push.model) * skinnedNormal);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragAlpha = push.alpha;
}
