#version 450
// =============================================================================
// water.vert - Phase 1A2: uses shared types.h (112-byte push constant)
// =============================================================================
#extension GL_GOOGLE_include_directive : require
#include "shared/types.h"

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out float fragDepthFactor;

layout(set = 0, binding = 0) uniform UBO {
    FrameUBO frame;
} ubo;

layout(push_constant) uniform PC {
    WaterPushConstants push;
};

const float PI = 3.14159265358979323846;

float waveHeight(float worldX, float worldZ) {
    float k = 2.0 * PI / max(push.waveWavelength, 0.01);
    float w = push.waveSpeed;
    float h1 = sin(k * worldX + w * push.time);
    float h2 = sin(k * 1.43 * worldZ + w * 1.13 * push.time + 1.7);
    float h3 = sin(k * 2.7 * (worldX * 0.7 + worldZ * 0.7) + w * 0.9 * push.time + 3.4) * 0.4;
    return (h1 + h2 + h3) * push.waveAmp * 0.5;
}

vec3 waveNormal(float worldX, float worldZ) {
    const float eps = 0.05;
    float hL = waveHeight(worldX - eps, worldZ);
    float hR = waveHeight(worldX + eps, worldZ);
    float hD = waveHeight(worldX, worldZ - eps);
    float hU = waveHeight(worldX, worldZ + eps);
    vec3 n = normalize(vec3(hL - hR, 2.0 * eps, hD - hU));
    return n;
}

void main() {
    vec4 worldPos4 = push.model * vec4(inPosition, 1.0);
    vec3 worldPos = worldPos4.xyz;
    worldPos.y += waveHeight(worldPos.x, worldPos.z);

    fragWorldPos = worldPos;
    fragNormal = waveNormal(worldPos.x, worldPos.z);
    fragTexCoord = inTexCoord;

    vec2 centered = abs(fragTexCoord - 0.5) * 2.0;
    fragDepthFactor = max(centered.x, centered.y);

    gl_Position = ubo.frame.proj * ubo.frame.view * vec4(worldPos, 1.0);
}
