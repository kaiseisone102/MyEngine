#version 450
// =============================================================================
// bloom_upsample.frag - Jimenez 3x3 tent upsample filter.
// Reads the lower (smaller) mip with a 3x3 tent kernel scaled by filterRadius
// (pc.param). The result is ADDITIVELY blended onto the higher mip via the
// pipeline's blend state (so each level accumulates the level below).
// =============================================================================
layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;
layout(set = 0, binding = 0) uniform sampler2D srcTex;
layout(push_constant) uniform PushConstants {
    float threshold;
    float softKnee;
    float intensity;
    float param;   // filterRadius (in texels of the source mip)
} pc;

void main() {
    vec2 t = 1.0 / vec2(textureSize(srcTex, 0));
    float rx = t.x * pc.param;
    float ry = t.y * pc.param;

    vec3 a = texture(srcTex, fragUV + vec2(-rx,  ry)).rgb;
    vec3 b = texture(srcTex, fragUV + vec2(  0,  ry)).rgb;
    vec3 c = texture(srcTex, fragUV + vec2( rx,  ry)).rgb;
    vec3 d = texture(srcTex, fragUV + vec2(-rx,  0)).rgb;
    vec3 e = texture(srcTex, fragUV + vec2(  0,  0)).rgb;
    vec3 f = texture(srcTex, fragUV + vec2( rx,  0)).rgb;
    vec3 g = texture(srcTex, fragUV + vec2(-rx, -ry)).rgb;
    vec3 h = texture(srcTex, fragUV + vec2(  0, -ry)).rgb;
    vec3 i = texture(srcTex, fragUV + vec2( rx, -ry)).rgb;

    // 3x3 tent: center 4, edges 2, corners 1, normalized by 16.
    vec3 color = e*4.0 + (b + d + f + h)*2.0 + (a + c + g + i);
    color *= (1.0 / 16.0);
    outColor = vec4(color, 1.0);
}