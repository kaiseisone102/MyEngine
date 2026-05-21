// =============================================================================
// grass_instanced.frag - Phase 1F: alpha-tested grass with bindless texture
// =============================================================================
// Samples the grass texture from the bindless array (index from push constant
// via the vertex shader), then ALPHA-TESTS: fully transparent texels are
// discarded so no depth sorting is needed (the grass can live in the opaque
// pass). Lighting is a simple diffuse + ambient; grass normals are unreliable
// on a cross-quad, so we keep shading soft.
// =============================================================================
#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#include "shared/types.h"

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragWorldPos;
layout(location = 4) in vec4 fragLightPos;
layout(location = 5) in float fragAlpha;
layout(location = 6) flat in int fragAlbedoIdx;
layout(location = 7) in vec4 fragInstColor;
layout(location = 8) in vec4 fragInstParams;

layout(set = 0, binding = 0) uniform UBO {
    FrameUBO frame;
} ubo;
layout(set = 0, binding = 1) uniform sampler2D shadowMap;

// Bindless texture array (set=1), same as triangle_bindless.frag.
layout(set = 1, binding = 0) uniform sampler2D bindlessTextures[];

layout(location = 0) out vec4 outColor;

const float kAlphaCutoff = 0.5;

void main() {
    vec4 albedo = texture(bindlessTextures[nonuniformEXT(fragAlbedoIdx)], fragTexCoord);

    // Alpha test: discard transparent texels (grass cutout). No sorting needed.
    if (albedo.a < kAlphaCutoff) discard;

    // Soft lighting. Grass cross-quad normals are not physically meaningful,
    // so use a gentle wrap-style diffuse to avoid harsh dark sides.
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(-ubo.frame.lightDir.xyz);
    float ndl = max(dot(N, L), 0.0);
    float wrapped = ndl * 0.5 + 0.5;  // half-Lambert: keeps both sides lit

    vec3 ambient = ubo.frame.ambient.rgb * ubo.frame.lightColor.rgb;
    vec3 diffuse = ubo.frame.lightColor.rgb * wrapped;
    vec3 lighting = ambient + diffuse;

    // Per-instance tint (white = no change; varied = color variation)
    outColor = vec4(albedo.rgb * lighting * fragInstColor.rgb, 1.0);
}
