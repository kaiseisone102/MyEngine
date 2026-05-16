// =============================================================================
// shadow_skinned.vert — Phase 4 段階4-1
// =============================================================================
// 100体規模対応:
//   triangle_skinned.vert と同じく push.skinOffset を加算してボーンを参照。
// =============================================================================
#version 450

layout(location = 0) in vec3  inPosition;
layout(location = 4) in ivec4 inJointIndices;
layout(location = 5) in vec4  inJointWeights;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4  vp;
    mat4  lightVP;
    vec3  lightPos;
    vec3  lightColor;
    vec3  viewPos;
    float ambient;
    float specular;
    float shadowStrength;
    float shadowBias;
} ubo;

layout(set = 1, binding = 0) readonly buffer SkinMatrices {
    mat4 boneMatrices[];
} skin;

layout(push_constant) uniform PushConstants {
    mat4 model;
    int  skinOffset;
} push;

void main() {
    int base = push.skinOffset;
    mat4 skinMatrix =
        skin.boneMatrices[base + inJointIndices.x] * inJointWeights.x
      + skin.boneMatrices[base + inJointIndices.y] * inJointWeights.y
      + skin.boneMatrices[base + inJointIndices.z] * inJointWeights.z
      + skin.boneMatrices[base + inJointIndices.w] * inJointWeights.w;

    vec4 skinnedPos = skinMatrix * vec4(inPosition, 1.0);
    gl_Position = ubo.lightVP * push.model * skinnedPos;
}
