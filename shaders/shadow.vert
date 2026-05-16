// =============================================================================
// shadow.vert
// =============================================================================
#version 450

layout(location = 0) in vec3 inPosition;

layout(binding = 0) uniform UniformBufferObject {
    mat4  vp;
    mat4  lightVP;
    vec3  lightPos;
    vec3  lightColor;
    vec3  viewPos;
    float ambient;
    float specular;
    float shadowStrength;
    float shadowBias;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
} push;

void main() {
    gl_Position = ubo.lightVP * push.model * vec4(inPosition, 1.0);
}
