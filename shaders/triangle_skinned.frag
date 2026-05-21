// =============================================================================
// triangle_skinned.frag - Phase 1A2: uses shared types.h
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
layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

const float kSpecular = 0.5;
const float kShadowBias = 0.0015;

void main() {
    vec4 baseColor = texture(texSampler, fragTexCoord) * vec4(fragColor, 1.0);

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
        int pcfR = int(ubo.frame.shadowParams.y + 0.5);  // 0=hard,1=3x3,2=5x5
        if (pcfR <= 0) {
            float d = texture(shadowMap, proj.xy).r;
            shadow = (currentDepth - kShadowBias > d) ? 1.0 : 0.0;
        } else {
            vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
            float cnt = 0.0;
            for (int sx = -pcfR; sx <= pcfR; ++sx) {
                for (int sy = -pcfR; sy <= pcfR; ++sy) {
                    float d = texture(shadowMap, proj.xy + vec2(sx, sy) * texelSize).r;
                    shadow += (currentDepth - kShadowBias > d) ? 1.0 : 0.0;
                    cnt += 1.0;
                }
            }
            shadow /= cnt;
        }
    }

    float litFactor = 1.0 - shadow * ubo.frame.shadowParams.x;
    vec3 lighting = ambientLight + (diffuse + specular) * litFactor;
    outColor = vec4(lighting * baseColor.rgb, baseColor.a * fragAlpha);
}
