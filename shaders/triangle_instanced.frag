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

const float kSpecular = 0.5;
const float kShadowBias = 0.0015;

void main() {
    // No texture sampling; base color is the vertex color.
    vec3 baseColor = fragColor;

    vec3 ambientLight = ubo.frame.ambient.rgb * ubo.frame.lightColor.rgb;
    vec3 norm = normalize(fragNormal);
    vec3 lightDirToLight = normalize(-ubo.frame.lightDir.xyz);
    float diff = max(dot(norm, lightDirToLight), 0.0);
    vec3 diffuse = diff * ubo.frame.lightColor.rgb;

    vec3 viewDir = normalize(ubo.frame.viewPos.xyz - fragWorldPos);
    vec3 reflectDir = reflect(-lightDirToLight, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = kSpecular * spec * ubo.frame.lightColor.rgb;

    vec3 proj = fragLightPos.xyz / fragLightPos.w;
    proj.xy = proj.xy * 0.5 + 0.5;
    float shadow = 0.0;
    if (proj.x >= 0.0 && proj.x <= 1.0 && proj.y >= 0.0 && proj.y <= 1.0 &&
        proj.z >= 0.0 && proj.z <= 1.0) {
        float currentDepth = proj.z;
        // PCF 3x3: average 9 neighboring depth comparisons for soft edges
        vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
        for (int sx = -1; sx <= 1; ++sx) {
            for (int sy = -1; sy <= 1; ++sy) {
                float d = texture(shadowMap, proj.xy + vec2(sx, sy) * texelSize).r;
                shadow += (currentDepth - kShadowBias > d) ? 1.0 : 0.0;
            }
        }
        shadow /= 9.0;
    }
    float litFactor = 1.0 - shadow * ubo.frame.shadowParams.x;

    vec3 lighting = ambientLight + (diffuse + specular) * litFactor;
    outColor = vec4(lighting * baseColor, fragAlpha);
}
