// =============================================================================
// grass_instanced.frag - Phase 1F / S6-b: alpha-tested grass, unified material
// =============================================================================
// S6-b: grass now goes through the SAME materialId+bindless path as every other
// draw. The fragment reads its GpuMaterial from the global material SSBO (BDA,
// via FrameUBO) using push.materialId, then samples bindlessTextures[albedoIdx]
// exactly like triangle.frag, with a baseColorFactor fallback. After sampling,
// it ALPHA-TESTS (grass cutout) and applies grass-specific soft lighting.
// =============================================================================
#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_nonuniform_qualifier : require
#include "shared/types.h"
#include "shared/gbuffer.glsl"  // PART4 4a-2

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragWorldPos;
layout(location = 4) in vec4 fragLightPos;
layout(location = 5) in float fragAlpha;
layout(location = 7) in vec4 fragInstColor;
layout(location = 8) in vec4 fragInstParams;
// PART4 4a-2: motion vector inputs.
layout(location = 9) in vec4 fragCurClip;
layout(location = 10) in vec4 fragPrevClip;

layout(set = 0, binding = 0) uniform UBO {
    FrameUBO frame;
} ubo;
layout(set = 0, binding = 1) uniform sampler2D shadowMap;
// Bindless texture array (set=1), same as triangle.frag.
layout(set = 1, binding = 0) uniform sampler2D bindlessTextures[];

// Unified material SSBO (BDA), read-only - same as triangle.frag.
layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer MaterialBuffer {
    GpuMaterial materials[];
};
// Same push constant block as grass_instanced.vert (VERTEX|FRAGMENT).
layout(push_constant) uniform PC {
    InstancedPushConstants push;
};

layout(location = 0) out vec4 outColor;
// PART4 4a-2: GBuffer outputs.
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec2 outMotion;

const float kAlphaCutoff = 0.5;

void main() {
    // S6-b: fetch this draw's material from the SSBO by id.
    MaterialBuffer matBuf = MaterialBuffer(ubo.frame.materialBuffer.xy);
    GpuMaterial m = matBuf.materials[push.materialId];

    // albedo: sample the bindless texture if present, else the factor.
    vec4 albedo = (m.albedoIdx >= 0)
        ? texture(bindlessTextures[nonuniformEXT(m.albedoIdx)], fragTexCoord)
        : m.baseColorFactor;

    // Alpha test: discard transparent texels (grass cutout). No sorting needed.
    if (albedo.a < kAlphaCutoff) discard;

    // Soft lighting. Grass cross-quad normals are not physically meaningful,
    // so use a gentle wrap-style diffuse to avoid harsh dark sides.
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(-ubo.frame.lightDir.xyz);
    float ndl = max(dot(N, L), 0.0);
    float wrapped = ndl * 0.5 + 0.5;  // half-Lambert: keeps both sides lit
    vec3 ambient = ubo.frame.ambient.rgb * ubo.frame.lightColor.rgb;
    vec3 diffuse = ubo.frame.lightColor.rgb * wrapped;
    vec3 lighting = ambient + diffuse;

    // Per-instance tint (white = no change; varied = color variation)
    outColor = vec4(albedo.rgb * lighting * fragInstColor.rgb, 1.0);

    // PART4 4a-2: GBuffer outputs.
    outNormal = vec4(encodeNormal(N), 0.0, 0.0);
    outMotion = computeMotion(fragCurClip, fragPrevClip);
}