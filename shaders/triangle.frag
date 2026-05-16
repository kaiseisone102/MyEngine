// =============================================================================
// triangle.frag
// =============================================================================
#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragWorldPos;
layout(location = 4) in vec4 fragLightPos;

// ── frame set (UBO + shadowMap) ─────────────────────────────────
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

layout(set = 0, binding = 1) uniform sampler2D shadowMap;

// ── material set (texture) ──────────────────────────────────────
layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 baseColor = texture(texSampler, fragTexCoord) * vec4(fragColor, 1.0);

    vec3 ambientLight = ubo.ambient * ubo.lightColor;

    vec3  norm      = normalize(fragNormal);
    vec3  lightDir  = normalize(ubo.lightPos - fragWorldPos);
    float diff      = max(dot(norm, lightDir), 0.0);
    vec3  diffuse   = diff * ubo.lightColor;

    vec3  viewDir    = normalize(ubo.viewPos - fragWorldPos);
    vec3  reflectDir = reflect(-lightDir, norm);
    float spec       = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3  specular   = ubo.specular * spec * ubo.lightColor;

    vec3 proj = fragLightPos.xyz / fragLightPos.w;
    proj = proj * 0.5 + 0.5;
    float shadow = 0.0;
    if (proj.x >= 0.0 && proj.x <= 1.0 && proj.y >= 0.0 && proj.y <= 1.0 && proj.z <= 1.0) {
        float closestDepth = texture(shadowMap, proj.xy).r;
        float currentDepth = proj.z;
        shadow = (currentDepth - ubo.shadowBias > closestDepth) ? 1.0 : 0.0;
    }
    float litFactor = 1.0 - shadow * ubo.shadowStrength;

    vec3 lighting = ambientLight + (diffuse + specular) * litFactor;
    outColor = vec4(lighting * baseColor.rgb, baseColor.a);
}
