// =============================================================================
// triangle.vert - Phase 1A2: uses shared types.h (single source of truth)
// =============================================================================
#version 450
#extension GL_GOOGLE_include_directive : require
#include "shared/types.h"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragWorldPos;
layout(location = 4) out vec4 fragLightPos;
layout(location = 5) out float fragAlpha;

// FrameUBO is defined in shared/types.h
layout(set = 0, binding = 0) uniform UBO {
    FrameUBO frame;
} ubo;

// StaticPushConstants is defined in shared/types.h
layout(push_constant) uniform PC {
    StaticPushConstants push;
};

void main() {
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    gl_Position = ubo.frame.proj * ubo.frame.view * worldPos;
    fragWorldPos = vec3(worldPos);
    fragLightPos = ubo.frame.lightVP * worldPos;
    fragNormal = normalize(mat3(push.model) * inNormal);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragAlpha = push.alpha;
}
