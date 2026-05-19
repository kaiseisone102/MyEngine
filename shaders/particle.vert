#version 450
// =============================================================================
// particle.vert - Phase 1A2: uses shared types.h
// + Instanced billboard, distance fade, 2-stage size shrink
// =============================================================================
#extension GL_GOOGLE_include_directive : require
#include "shared/types.h"

layout(set = 0, binding = 0) uniform UBO {
    FrameUBO frame;
} ubo;

layout(push_constant) uniform ParticlePC {
    float fadeStart;
    float fadeEnd;
    float _pad0;
    float _pad1;
} pc;

layout(location = 0) in vec2 inCorner;
layout(location = 1) in vec3 inPos;
layout(location = 2) in float inSize;
layout(location = 3) in vec4 inColor;
layout(location = 4) in float inAge01;

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;
layout(location = 2) out float vAge01;
layout(location = 3) out float vDistFade;

void main() {
    float dist = length(inPos - ubo.frame.viewPos.xyz);

    // 2-stage size shrink
    float sizeScale;
    if (dist <= 1.0) {
        sizeScale = 1.0;
    } else if (dist <= 8.0) {
        sizeScale = mix(1.0, 0.25, (dist - 1.0) / 7.0);
    } else {
        sizeScale = mix(0.25, 0.05, clamp((dist - 8.0) / 42.0, 0.0, 1.0));
    }
    float effSize = inSize * sizeScale;

    vec4 center = ubo.frame.proj * ubo.frame.view * vec4(inPos, 1.0);
    vec2 offset = inCorner * effSize;
    gl_Position = center + vec4(offset * center.w, 0.0, 0.0);

    vUV = inCorner + 0.5;
    vColor = inColor;
    vAge01 = inAge01;

    if (pc.fadeEnd <= pc.fadeStart) {
        vDistFade = 1.0;
    } else {
        vDistFade = clamp((pc.fadeEnd - dist) / (pc.fadeEnd - pc.fadeStart), 0.0, 1.0);
    }
}
