#version 450
// =============================================================================
// bloom_downsample.frag - Jimenez (CoD:AW 2014) 13-tap downsample.
// Samples a 4x4 neighborhood as 13 bilinear taps and weights them so the result
// is stable (no pulsating). When pc.karis > 0.5 (used ONLY for the mip0->mip1
// step) each 2x2 group is weighted by 1/(1+luma) to kill fireflies.
// =============================================================================
layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;
layout(set = 0, binding = 0) uniform sampler2D srcTex;
layout(push_constant) uniform PushConstants {
    float threshold;
    float softKnee;
    float intensity;
    float param;   // downsample: karis flag (>0.5 = on); upsample: filterRadius
} pc;

float luma(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }
float karisWeight(vec3 c) { return 1.0 / (1.0 + luma(c)); }

void main() {
    vec2 t = 1.0 / vec2(textureSize(srcTex, 0));
    float dx = t.x;
    float dy = t.y;

    // 13 bilinear taps over a 4x4 footprint around fragUV.
    vec3 a = texture(srcTex, fragUV + vec2(-2*dx,  2*dy)).rgb;
    vec3 b = texture(srcTex, fragUV + vec2(  0,    2*dy)).rgb;
    vec3 c = texture(srcTex, fragUV + vec2( 2*dx,  2*dy)).rgb;

    vec3 d = texture(srcTex, fragUV + vec2(-2*dx,  0)).rgb;
    vec3 e = texture(srcTex, fragUV + vec2(  0,    0)).rgb;
    vec3 f = texture(srcTex, fragUV + vec2( 2*dx,  0)).rgb;

    vec3 g = texture(srcTex, fragUV + vec2(-2*dx, -2*dy)).rgb;
    vec3 h = texture(srcTex, fragUV + vec2(  0,   -2*dy)).rgb;
    vec3 i = texture(srcTex, fragUV + vec2( 2*dx, -2*dy)).rgb;

    vec3 j = texture(srcTex, fragUV + vec2(-dx,  dy)).rgb;
    vec3 k = texture(srcTex, fragUV + vec2( dx,  dy)).rgb;
    vec3 l = texture(srcTex, fragUV + vec2(-dx, -dy)).rgb;
    vec3 m = texture(srcTex, fragUV + vec2( dx, -dy)).rgb;

    vec3 color;
    if (pc.param > 0.5) {
        // Karis average per 2x2 group (firefly suppression for mip0->mip1).
        vec3 g0 = (j + k + l + m) * 0.25;
        vec3 g1 = (a + b + d + e) * 0.25;
        vec3 g2 = (b + c + e + f) * 0.25;
        vec3 g3 = (d + e + g + h) * 0.25;
        vec3 g4 = (e + f + h + i) * 0.25;
        float w0 = karisWeight(g0);
        float w1 = karisWeight(g1);
        float w2 = karisWeight(g2);
        float w3 = karisWeight(g3);
        float w4 = karisWeight(g4);
        float wsum = w0*0.5 + (w1 + w2 + w3 + w4)*0.125;
        color = (g0*w0*0.5 + g1*w1*0.125 + g2*w2*0.125 + g3*w3*0.125 + g4*w4*0.125) / max(wsum, 1e-5);
    } else {
        // Standard Jimenez weights: center group 0.5, corners 0.125 each.
        color  = e * 0.125;
        color += (a + c + g + i) * 0.03125;
        color += (b + d + f + h) * 0.0625;
        color += (j + k + l + m) * 0.125;
    }
    outColor = vec4(color, 1.0);
}