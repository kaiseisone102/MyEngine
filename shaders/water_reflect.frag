#version 450
// =============================================================================
// water_reflect.frag - Phase 1A2: uses shared types.h
// =============================================================================
#extension GL_GOOGLE_include_directive : require
#include "shared/types.h"

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in float fragDepthFactor;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UBO {
    FrameUBO frame;
} ubo;

layout(set = 1, binding = 0) uniform sampler2D reflectionTex;

layout(push_constant) uniform PC {
    WaterPushConstants push;
};

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(ubo.frame.viewPos.xyz - fragWorldPos);

    vec4 baseColor = mix(push.deepColor, push.shallowColor, fragDepthFactor);

    float NdotV = max(dot(normal, viewDir), 0.0);
    float fresnel = pow(1.0 - NdotV, 3.0);

    // Screen-space UV for reflection sampling
    vec2 screenSize = vec2(textureSize(reflectionTex, 0));
    vec2 reflectUV = gl_FragCoord.xy / screenSize;

    // Wave-based distortion
    float distortAmount = 0.02;
    reflectUV.xy += normal.xz * distortAmount;
    reflectUV = clamp(reflectUV, 0.001, 0.999);

    vec3 reflectionColor = texture(reflectionTex, reflectUV).rgb;

    // Specular (directional light)
    vec3 lightDirToLight = normalize(-ubo.frame.lightDir.xyz);
    vec3 halfwayDir = normalize(lightDirToLight + viewDir);
    float specPower = 64.0;
    float spec = pow(max(dot(normal, halfwayDir), 0.0), specPower);
    vec3 specular = spec * ubo.frame.lightColor.rgb * 0.8;

    // Diffuse
    float diff = max(dot(normal, lightDirToLight), 0.0);
    float ambientScalar = (ubo.frame.ambient.r + ubo.frame.ambient.g + ubo.frame.ambient.b) / 3.0;
    vec3 diffuse = baseColor.rgb * (ambientScalar + diff * 0.4) * ubo.frame.lightColor.rgb;

    vec3 finalColor = mix(diffuse, reflectionColor, fresnel) + specular;

    float baseAlpha = mix(push.deepColor.a, push.shallowColor.a, fragDepthFactor);
    float finalAlpha = mix(baseAlpha, 1.0, fresnel * 0.7);

    outColor = vec4(finalColor, finalAlpha);
}
