// =============================================================================
// debug_line.vert - Phase 1A2: uses shared types.h
// =============================================================================
#version 450
#extension GL_GOOGLE_include_directive : require
#include "shared/types.h"

layout(set = 0, binding = 0) uniform UBO {
    FrameUBO frame;
} ubo;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 outColor;

void main() {
    gl_Position = ubo.frame.proj * ubo.frame.view * vec4(inPos, 1.0);
    outColor = inColor;
}
