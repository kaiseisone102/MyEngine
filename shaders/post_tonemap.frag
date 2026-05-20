#version 450
// =============================================================================
// post_tonemap.frag - Phase 1H-3: ACES Filmic tonemap (Narkowicz 2016)
//
// Input:  HDR linear color from MainPass (R16G16B16A16_SFLOAT, unbounded)
// Output: LDR sRGB color in swapchain (sRGB format auto-encodes linear->sRGB)
//
// We do NOT apply pow(1/2.2) here because the swapchain format is
// VK_FORMAT_B8G8R8A8_SRGB; the GPU automatically converts linear values
// to sRGB on write. So we output linear values, and the hardware handles
// the gamma curve.
//
// ACES Filmic curve (Narkowicz approximation):
//   Maps [0, +inf) -> [0, 1] with a film-like S-curve.
//   Bright values roll off smoothly (no clipping); dark values keep contrast.
// =============================================================================

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D hdrColor;

// ACES Filmic tonemap curve, Krzysztof Narkowicz 2016 approximation
// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 ACESFilm(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdr = texture(hdrColor, fragUV).rgb;

    // Exposure adjustment (currently fixed; later expose as uniform / FrameUBO)
    const float exposure = 1.0;
    hdr *= exposure;

    vec3 ldr = ACESFilm(hdr);

    // Output linear values; sRGB swapchain handles gamma encoding
    outColor = vec4(ldr, 1.0);
}
