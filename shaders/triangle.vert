// =============================================================================
// triangle.vert
// =============================================================================
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragWorldPos;
layout(location = 4) out vec4 fragLightPos;

// frame set
layout(set = 0, binding = 0) uniform UniformBufferObject {
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
    vec4 worldPos    = push.model * vec4(inPosition, 1.0);
    gl_Position      = ubo.vp * worldPos;
    fragWorldPos     = vec3(worldPos);
    fragLightPos     = ubo.lightVP * worldPos;
    fragNormal       = normalize(mat3(push.model) * inNormal);
    fragColor        = inColor;
    fragTexCoord     = inTexCoord;
}
