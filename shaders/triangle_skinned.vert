// =============================================================================
// triangle_skinned.vert - Phase 1B-4b: BDA (buffer_reference) for skin matrices
// =============================================================================
// The skin matrix buffer is no longer bound via descriptor set (set=2).
// Instead, its GPU virtual address is passed via push constant
// (SkinnedPushConstants::skinBuffer). The shader casts this 64-bit address
// to a typed pointer using GL_EXT_buffer_reference and reads matrices
// directly from GPU memory.
//
// This is the modern "bindless" Vulkan style: shaders dereference GPU
// pointers, descriptors are only used for textures.
// =============================================================================
#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

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
// PART4 4a-2: motion vector inputs (see triangle.vert for design notes).
layout(location = 7) out vec4 fragCurClip;
layout(location = 8) out vec4 fragPrevClip;

layout(set = 0, binding = 0) uniform UBO {
    FrameUBO frame;
} ubo;

// BDA: a typed pointer to the skin matrix array.
// std430 + buffer_reference_align=16 matches mat4 alignment in the SSBO.
layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer SkinMatrices {
    mat4 boneMatrices[];
};

layout(push_constant) uniform PC {
    SkinnedPushConstants push;
};

void main() {
    // Cast the 64-bit address in push.skinBuffer to a typed pointer.
    SkinMatrices skin = SkinMatrices(push.skinBuffer);

    int base = push.skinOffset;
    mat4 skinMatrix =
        skin.boneMatrices[base + inJointIndices.x] * inJointWeights.x
      + skin.boneMatrices[base + inJointIndices.y] * inJointWeights.y
      + skin.boneMatrices[base + inJointIndices.z] * inJointWeights.z
      + skin.boneMatrices[base + inJointIndices.w] * inJointWeights.w;

    vec4 skinnedPos = skinMatrix * vec4(inPosition, 1.0);
    vec3 skinnedNormal = mat3(skinMatrix) * inNormal;

    vec4 worldPos = push.model * skinnedPos;
    vec4 curClip  = ubo.frame.proj * ubo.frame.view * worldPos;
    gl_Position = curClip;

    fragWorldPos = vec3(worldPos);
    fragLightPos = ubo.frame.lightVP * worldPos;
    fragNormal = normalize(mat3(push.model) * skinnedNormal);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragAlpha = push.alpha;
    // PART4 4a-2: prev clip from prevViewProj + same world pos. Per-bone
    // prev-pose history is a Phase 3 addition; current motion captures camera
    // movement which is the dominant TAA signal.
    fragCurClip  = curClip;
    fragPrevClip = ubo.frame.prevViewProj * worldPos;
}
