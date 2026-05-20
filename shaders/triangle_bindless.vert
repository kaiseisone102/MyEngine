// =============================================================================
// triangle_bindless.vert - Phase 1D-2: bindless texture sampling
// =============================================================================
// Same vertex layout as triangle.vert. Differences:
//   - push constant carries an int albedoIdx (the bindless texture slot)
//   - vertex shader just forwards the index to fragment shader; the actual
//     texture lookup happens in triangle_bindless.frag
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
layout(location = 6) flat out int fragAlbedoIdx;

layout(set = 0, binding = 0) uniform UBO {
    FrameUBO frame;
} ubo;

layout(push_constant) uniform PC {
    StaticBindlessPushConstants push;
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
    fragAlbedoIdx = push.albedoIdx;
}
