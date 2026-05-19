// =============================================================================
// shadow.vert - Phase 1A2: uses shared types.h
// =============================================================================
#version 450
#extension GL_GOOGLE_include_directive : require
#include "shared/types.h"

layout(location = 0) in vec3 inPosition;

layout(binding = 0) uniform UBO {
    FrameUBO frame;
} ubo;

layout(push_constant) uniform PC {
    ShadowStaticPushConstants push;
};

void main() {
    gl_Position = ubo.frame.lightVP * push.model * vec4(inPosition, 1.0);
}
