// =============================================================================
// shadow.vert - PART4 4-前-5: static-mesh shadow vertex reads per-draw model
// from the DrawData SSBO via BDA + gl_InstanceIndex (firstInstance == draw
// slot). Mirrors triangle.vert's PART3b path. Lets shadow_pass drive draws
// through vkCmdDrawIndexedIndirectCount (indirect_exec).
// =============================================================================
#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "shared/types.h"

layout(location = 0) in vec3 inPosition;

layout(binding = 0) uniform UBO {
    FrameUBO frame;
} ubo;

// PART4 4-前-5: same DrawData layout as static draws (PART3b). Shadow uses
// only the model field; materialId / alpha are ignored here.
layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer DrawBuffer {
    DrawData data[];
};

layout(push_constant) uniform PC {
    ShadowDrawPushConstants push;
};

void main() {
    DrawBuffer drawBuf = DrawBuffer(push.drawBuffer);
    mat4 model = drawBuf.data[gl_InstanceIndex].model;
    gl_Position = ubo.frame.lightVP * model * vec4(inPosition, 1.0);
}
