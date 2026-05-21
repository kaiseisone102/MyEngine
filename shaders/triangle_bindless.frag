// =============================================================================
// triangle_bindless.frag - Phase 1D-2: bindless texture sampling
// =============================================================================
// The big change here: instead of a single sampler2D bound via set=1, we have
// an unbounded array of sampler2D and pick the slot by index.
//
// GL_EXT_nonuniform_qualifier lets us index with a value that may not be
// dynamically uniform across the workgroup (i.e., different fragments may
// want different textures - which they will once we add more bindless draws).
// Even with one index per draw call (uniform), this extension is required by
// the descriptor indexing spec when the array is variable-size.
// =============================================================================
#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#include "shared/types.h"

layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragWorldPos;
layout(location = 4) in vec4 fragLightPos;
layout(location = 5) in float fragAlpha;
layout(location = 6) flat in int fragAlbedoIdx;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UBO {
    FrameUBO frame;
} ubo;

layout(set = 0, binding = 1) uniform sampler2D shadowMap;

// === The bindless texture array. Unbounded (size declared in C++ as 1024). ===
layout(set = 1, binding = 0) uniform sampler2D bindlessTextures[];

const float PI = 3.14159265359;
float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}
float geometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}
float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return geometrySchlickGGX(max(dot(N, V), 0.0), roughness) * geometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float sampleShadow(vec4 lightPos) {
    vec3 proj = lightPos.xyz / lightPos.w;
    proj.xy = proj.xy * 0.5 + 0.5;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) return 1.0;
    float currentDepth = proj.z;
    float bias = 0.005;
    int pcfR = int(ubo.frame.shadowParams.y + 0.5);  // 0=hard,1=3x3,2=5x5
    float shadow = 0.0;
    if (pcfR <= 0) {
        float d = texture(shadowMap, proj.xy).r;
        shadow = (currentDepth - bias > d) ? 1.0 : 0.0;
    } else {
        vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
        float cnt = 0.0;
        for (int sx = -pcfR; sx <= pcfR; ++sx) {
            for (int sy = -pcfR; sy <= pcfR; ++sy) {
                float d = texture(shadowMap, proj.xy + vec2(sx, sy) * texelSize).r;
                shadow += (currentDepth - bias > d) ? 1.0 : 0.0;
                cnt += 1.0;
            }
        }
        shadow /= cnt;
    }
    return 1.0 - shadow * ubo.frame.shadowParams.x;
}

void main() {
    // === Bindless texture lookup ===
    // nonuniformEXT() tells the driver the index may diverge across invocations.
    vec4 albedo = texture(bindlessTextures[nonuniformEXT(fragAlbedoIdx)], fragTexCoord);

    vec3 N = normalize(fragNormal);
    vec3 V = normalize(ubo.frame.viewPos.xyz - fragWorldPos);
    vec3 L = normalize(-ubo.frame.lightDir.xyz);
    vec3 H = normalize(V + L);

    float metallic = 0.0;
    float roughness = 0.5;
    vec3 alb = albedo.rgb;

    vec3 F0 = mix(vec3(0.04), alb, metallic);
    vec3 radiance = ubo.frame.lightColor.rgb;

    float D = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    vec3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 0.0001);
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 diffuse = kD * alb / PI;

    float shadow = sampleShadow(fragLightPos);
    vec3 Lo = (diffuse + specular) * radiance * NdotL * shadow;
    vec3 ambient = ubo.frame.ambient.rgb * alb;
    vec3 finalColor = ambient + Lo;
    outColor = vec4(finalColor, albedo.a * fragAlpha);
}
