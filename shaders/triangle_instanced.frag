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

layout(location = 0) out vec4 outColor;

const float kShadowBias = 0.0015;

void main() {
    // No texture sampling; base color is the vertex color.
    vec3 baseColor = fragColor;

    vec3 N = normalize(fragNormal);
    vec3 V = normalize(ubo.frame.viewPos.xyz - fragWorldPos);
    vec3 L = normalize(-ubo.frame.lightDir.xyz);

    // instanced draws have no material SSBO; use neutral dielectric defaults.
    float metallic = 0.0;
    float roughness = 0.5;
    vec3 albedo = baseColor;
    vec3 radiance = ubo.frame.lightColor.rgb;

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
    outColor = vec4(color, fragAlpha);
}
