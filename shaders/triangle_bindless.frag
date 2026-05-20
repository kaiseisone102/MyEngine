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
    float closestDepth = texture(shadowMap, proj.xy).r;
    float currentDepth = proj.z;
    float bias = 0.005;
    return (currentDepth - bias > closestDepth) ? (1.0 - ubo.frame.shadowParams.x) : 1.0;
}

void main() {
    // === Bindless texture lookup ===
    // nonuniformEXT() tells the driver the index may diverge across invocations.
    vec4 albedo = texture(bindlessTextures[nonuniformEXT(fragAlbedoIdx)], fragTexCoord);

    vec3 N = normalize(fragNormal);
    vec3 L = normalize(-ubo.frame.lightDir.xyz);
    float NdotL = max(dot(N, L), 0.0);

    float shadow = sampleShadow(fragLightPos);
    vec3 diffuse = ubo.frame.lightColor.rgb * NdotL * shadow;
    vec3 ambient = ubo.frame.ambient.rgb;

    vec3 finalColor = albedo.rgb * (diffuse + ambient);
    outColor = vec4(finalColor, albedo.a * fragAlpha);
}
