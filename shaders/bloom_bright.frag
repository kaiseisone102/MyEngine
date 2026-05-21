#version 450
// bloom_bright.frag - extract bright regions from the HDR scene for bloom.
// Soft-knee threshold so the transition into bloom is gradual, not a hard cutoff.
layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;
layout(set = 0, binding = 0) uniform sampler2D hdrColor;
layout(push_constant) uniform PushConstants {
    float threshold;   // luminance above which pixels bloom (e.g. 1.0)
    float softKnee;    // width of the soft transition (e.g. 0.5)
    float intensity;   // unused here, used at composite
    float texelDir;    // unused here
} pc;
void main() {
    vec3 hdr = texture(hdrColor, fragUV).rgb;
    float luma = dot(hdr, vec3(0.2126, 0.7152, 0.0722));
    float thr = max(pc.threshold, 0.0001);
    float knee = max(pc.softKnee, 0.0001);
    // soft-knee curve: 0 below (thr-knee), ramps up to full at (thr+knee)
    float soft = clamp((luma - thr + knee) / (2.0 * knee), 0.0, 1.0);
    soft = soft * soft;
    float contrib = max(soft, step(thr, luma));
    outColor = vec4(hdr * contrib, 1.0);
}