#version 450
// =============================================================================
// post_tonemap.frag - Phase 1H-4: selectable tonemapper (ACES / AgX / Khronos PBR Neutral)
//
// Input:  HDR linear color from MainPass (R16G16B16A16_SFLOAT, unbounded)
// Output: LDR sRGB color in swapchain (sRGB format auto-encodes linear->sRGB)
//
// We output LINEAR values; the VK_FORMAT_B8G8R8A8_SRGB swapchain applies the
// linear->sRGB encoding curve on write. So no manual pow(1/2.2) here.
//
// Mode selection via push constant:
//   0 = ACES Filmic (Narkowicz 2016)   - cinematic, classic
//   1 = AgX (Filament/Hill approx)      - smooth highlight desaturation
//   2 = Khronos PBR Neutral (2024)      - faithful base color, e-commerce/PBR
// =============================================================================

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D hdrColor;

layout(push_constant) uniform PushConstants {
    int   tonemapMode;  // 0=ACES, 1=AgX, 2=Khronos PBR Neutral
    float exposure;     // linear exposure multiplier (default 1.0)
} pc;

// -----------------------------------------------------------------------------
// Mode 0: ACES Filmic (Krzysztof Narkowicz 2016 approximation)
// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
// -----------------------------------------------------------------------------
vec3 tonemapACES(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// -----------------------------------------------------------------------------
// Mode 1: AgX (Filament / Troy Sobotka approximation, polynomial fit by Benjamin Wrensch)
// Reference: https://iolite-engine.com/blog_posts/minimal_agx_implementation
// Operates in a log2 domain with a 5x5-like input matrix, then a polynomial
// sigmoid, then the inverse matrix. This is the widely-used "minimal AgX".
// -----------------------------------------------------------------------------
vec3 agxDefaultContrastApprox(vec3 x) {
    vec3 x2 = x * x;
    vec3 x4 = x2 * x2;
    return  + 15.5     * x4 * x2
            - 40.14    * x4 * x
            + 31.96    * x4
            - 6.868    * x2 * x
            + 0.4298   * x2
            + 0.1191   * x
            - 0.00232;
}

vec3 tonemapAgX(vec3 color) {
    // Input transform matrix (sRGB -> AgX working space)
    const mat3 agxInputMat = mat3(
        0.842479062253094,  0.0423282422610123, 0.0423756549057051,
        0.0784335999999992, 0.878468636469772,  0.0784336,
        0.0792237451477643, 0.0791661274605434, 0.879142973793104
    );
    // Output transform matrix (AgX working space -> sRGB)
    const mat3 agxOutputMat = mat3(
         1.19687900512017,   -0.0528968517574562, -0.0529716355144438,
        -0.0980208811401368,  1.15190312990417,   -0.0980434501171241,
        -0.0990297440797205, -0.0989611768448433,  1.15107367264116
    );

    const float minEv = -12.47393;
    const float maxEv = 4.026069;

    color = agxInputMat * color;
    // Log2 encoding
    color = clamp(log2(color), minEv, maxEv);
    color = (color - minEv) / (maxEv - minEv);
    // Sigmoid approximation
    color = agxDefaultContrastApprox(color);
    // Output transform
    color = agxOutputMat * color;
    // AgX outputs in (approximately) display-linear; clamp to [0,1]
    color = clamp(color, 0.0, 1.0);
    return color;
}

// -----------------------------------------------------------------------------
// Mode 2: Khronos PBR Neutral Tone Mapper (2024, official spec)
// https://github.com/KhronosGroup/ToneMapping/blob/main/PBR_Neutral/README.md
// Faithful base-color reproduction, no hue shift, analytically invertible.
// -----------------------------------------------------------------------------
vec3 tonemapPBRNeutral(vec3 color) {
    const float startCompression = 0.8 - 0.04;  // Ks = 0.8 - F90
    const float desaturation = 0.15;            // Kd

    float x = min(color.r, min(color.g, color.b));
    float offset = (x < 0.08) ? (x - 6.25 * x * x) : 0.04;  // f: F90=0.04, 2*F90=0.08
    color -= offset;

    float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression) return color;

    float d = 1.0 - startCompression;
    float newPeak = 1.0 - d * d / (peak + d - startCompression);
    color *= newPeak / peak;

    float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    return mix(color, vec3(newPeak), g);
}

void main() {
    vec3 hdr = texture(hdrColor, fragUV).rgb;

    // Apply exposure (default 1.0 when push constant is zero-initialized -> guard)
    float exposure = (pc.exposure > 0.0) ? pc.exposure : 1.0;
    hdr *= exposure;

    vec3 ldr;
    if (pc.tonemapMode == 1) {
        ldr = tonemapAgX(hdr);
    } else if (pc.tonemapMode == 2) {
        ldr = tonemapPBRNeutral(hdr);
    } else {
        ldr = tonemapACES(hdr);
    }

    // Output linear; sRGB swapchain encodes gamma
    outColor = vec4(ldr, 1.0);
}
