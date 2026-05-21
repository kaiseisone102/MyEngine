#version 450
// bloom_blur.frag - separable 9-tap Gaussian blur.
// Run once horizontally then once vertically (texelDir picks the axis).
layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;
layout(set = 0, binding = 0) uniform sampler2D srcTex;
layout(push_constant) uniform PushConstants {
    float threshold;
    float softKnee;
    float intensity;
    float texelDir;   // 0 = horizontal, 1 = vertical
} pc;
void main() {
    vec2 texel = 1.0 / vec2(textureSize(srcTex, 0));
    vec2 dir = (pc.texelDir < 0.5) ? vec2(texel.x, 0.0) : vec2(0.0, texel.y);
    // 9-tap Gaussian weights
    float w[5];
    w[0] = 0.227027; w[1] = 0.194594; w[2] = 0.121621; w[3] = 0.054054; w[4] = 0.016216;
    vec3 result = texture(srcTex, fragUV).rgb * w[0];
    for (int i = 1; i < 5; ++i) {
        result += texture(srcTex, fragUV + dir * float(i)).rgb * w[i];
        result += texture(srcTex, fragUV - dir * float(i)).rgb * w[i];
    }
    outColor = vec4(result, 1.0);
}