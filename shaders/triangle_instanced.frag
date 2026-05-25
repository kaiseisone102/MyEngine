// =============================================================================
// triangle_instanced.frag - Phase 1E: instanced mesh, vertex-color only
// =============================================================================
// Identical lighting/shadow to triangle.frag, but WITHOUT the set=1 texSampler.
// The instanced pipeline layout only declares set=0 (frame UBO + shadow map),
// so sampling a set=1 texture would be a layout mismatch. Base color comes
// purely from the per-vertex fragColor.
// =============================================================================
#version 450
#extension GL_GOOGLE_include_directive : require
#include "shared/types.h"
#include "shared/shadow_sampling.glsl"

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

layout(location = 0) out vec4 outColor;

const float kShadowBias = 0.0015;
const float PI = 3.14159265359;

float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}
float geometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}
float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    // No texture sampling; base color is the vertex color.
    vec3 baseColor = fragColor;

    vec3 N = normalize(fragNormal);
    vec3 V = normalize(ubo.frame.viewPos.xyz - fragWorldPos);
    vec3 L = normalize(-ubo.frame.lightDir.xyz);
    vec3 H = normalize(V + L);

    float metallic = 0.0;
    float roughness = 0.5;
    vec3 albedo = baseColor;

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 radiance = ubo.frame.lightColor.rgb;

    float D = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    vec3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 0.0001);
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;

    vec3 proj = fragLightPos.xyz / fragLightPos.w;
    proj.xy = proj.xy * 0.5 + 0.5;
    float shadow = 0.0;
    if (proj.x >= 0.0 && proj.x <= 1.0 && proj.y >= 0.0 && proj.y <= 1.0 &&
        proj.z >= 0.0 && proj.z <= 1.0) {
        int quality = int(ubo.frame.shadowParams.y + 0.5);  // 0=hard,1=Soft(PCF),2=High(PCSS)
        shadow = sampleShadowFactor(shadowMap, proj.xy, proj.z, kShadowBias, quality);
    }
    float litFactor = 1.0 - shadow * ubo.frame.shadowParams.x;

    vec3 Lo = (diffuse + specular) * radiance * NdotL * litFactor;
    vec3 ambient = ubo.frame.ambient.rgb * albedo;
    vec3 color = ambient + Lo;
    outColor = vec4(color, fragAlpha);
}
