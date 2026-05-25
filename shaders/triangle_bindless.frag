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
#include "shared/shadow_sampling.glsl"
#include "shared/pbr.glsl"

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


float sampleShadow(vec4 lightPos) {
    vec3 proj = lightPos.xyz / lightPos.w;
    proj.xy = proj.xy * 0.5 + 0.5;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) return 1.0;
    float bias = 0.005;
    int quality = int(ubo.frame.shadowParams.y + 0.5);  // 0=hard,1=Soft(PCF),2=High(PCSS)
    float shadow = sampleShadowFactor(shadowMap, proj.xy, proj.z, bias, quality);
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

    // bindless draws have no material SSBO; use neutral dielectric defaults.
    float metallic = 0.0;
    float roughness = 0.5;
    vec3 alb = albedo.rgb;
    vec3 radiance = ubo.frame.lightColor.rgb;

    // sampleShadow() already returns litFactor (1 - shadow*strength).
    float litFactor = sampleShadow(fragLightPos);

    // One directional light today; loop here when multi-light (Phase 2A) lands.
    vec3 Lo = pbrDirectLighting(N, V, L, radiance, alb, metallic, roughness) * litFactor;
    vec3 finalColor = pbrAmbient(ubo.frame.ambient.rgb, alb) + Lo;
    outColor = vec4(finalColor, albedo.a * fragAlpha);
}
