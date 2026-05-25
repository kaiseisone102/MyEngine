// =============================================================================
// triangle.frag - Phase 1A2: uses shared types.h
// =============================================================================
#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_nonuniform_qualifier : require
#include "shared/types.h"
#include "shared/shadow_sampling.glsl"
#include "shared/pbr.glsl"

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragWorldPos;
layout(location = 4) in vec4 fragLightPos;
layout(location = 5) in float fragAlpha;

layout(set = 0, binding = 0) uniform UBO {
    FrameUBO frame;
} ubo;

layout(set = 0, binding = 1) uniform sampler2D shadowMap;
layout(set = 1, binding = 0) uniform sampler2D bindlessTextures[];

// Phase 1K-2: unified material SSBO (BDA), read-only
layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer MaterialBuffer {
    GpuMaterial materials[];
};

// Phase 1K-2 S4-b: same push constant block as triangle.vert (VERTEX|FRAGMENT)
layout(push_constant) uniform PC {
    StaticPushConstants push;
};

layout(location = 0) out vec4 outColor;

const float kShadowBias = 0.0015;

void main() {
    // Phase 1K-2 S4-c: fetch material from the SSBO by id
    MaterialBuffer matBuf = MaterialBuffer(ubo.frame.materialBuffer.xy);
    GpuMaterial m = matBuf.materials[push.materialId];
    // albedo: sample bindless texture if present, else use the factor
    vec4 texel = (m.albedoIdx >= 0)
        ? texture(bindlessTextures[nonuniformEXT(m.albedoIdx)], fragTexCoord)
        : m.baseColorFactor;
    vec4 baseColor = texel * vec4(fragColor, 1.0);


    vec3 N = normalize(fragNormal);
    vec3 V = normalize(ubo.frame.viewPos.xyz - fragWorldPos);
    // lightDir is the direction the light TRAVELS; flip for "to-light"
    vec3 L = normalize(-ubo.frame.lightDir.xyz);

    // ─── PBR material params (Phase 1K-2: from GpuMaterial) ───
    float metallic = m.metallic;
    float roughness = m.roughness;
    vec3 albedo = baseColor.rgb;
    vec3 radiance = ubo.frame.lightColor.rgb;

    // Shadow (directional/sun). litFactor folds shadow strength in.
    vec3 proj = fragLightPos.xyz / fragLightPos.w;
    proj.xy = proj.xy * 0.5 + 0.5;
    float shadow = 0.0;
    if (proj.x >= 0.0 && proj.x <= 1.0 && proj.y >= 0.0 && proj.y <= 1.0 &&
        proj.z >= 0.0 && proj.z <= 1.0) {
        int quality = int(ubo.frame.shadowParams.y + 0.5);  // 0=hard,1=Soft(PCF),2=High(PCSS)
        shadow = sampleShadowFactor(shadowMap, proj.xy, proj.z, kShadowBias, quality);
    }
    float litFactor = 1.0 - shadow * ubo.frame.shadowParams.x;

    // One directional light today; loop here when multi-light (Phase 2A) lands.
    vec3 Lo = pbrDirectLighting(N, V, L, radiance, albedo, metallic, roughness) * litFactor;
    vec3 color = pbrAmbient(ubo.frame.ambient.rgb, albedo) + Lo;
    outColor = vec4(color, baseColor.a * fragAlpha);
}
