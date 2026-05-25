// =============================================================================
// shared/shadow_sampling.glsl - Phase 1G: PCF / PCSS soft shadow filter
// =============================================================================
// Included by the lighting fragment shaders. Returns shadow OCCLUSION in
// [0,1] (0 = fully lit, 1 = fully shadowed). Each caller keeps its own
// light-space projection, in-bounds test and depth bias; only the filter
// lives here.
//
// Quality (FrameUBO.shadowParams.y, passed in as int):
//   <= 0 : hard 1-tap
//   == 1 : Soft - fixed-radius Vogel-disk PCF (per-pixel rotated)
//   >= 2 : High - PCSS (contact hardening): blocker search -> penumbra
//                 estimate -> variable-radius Vogel-disk PCF
//
// Vogel disk + interleaved-gradient-noise rotation breaks banding with few
// samples (Pascal / Quadro P620 friendly). Sample counts are conservative.
// =============================================================================
#ifndef SHARED_SHADOW_SAMPLING_GLSL
#define SHARED_SHADOW_SAMPLING_GLSL

const float SHADOW_PI = 3.14159265359;

// Sample counts (kept low for 620).
const int kPcfTaps         = 16;
const int kPcssBlockerTaps = 16;
const int kPcssFilterTaps  = 24;

// Vogel disk sample i of n on the unit disk, rotated by phi.
vec2 vogelDiskSample(int i, int n, float phi) {
    float r = sqrt((float(i) + 0.5) / float(n));
    float theta = float(i) * 2.39996323 + phi;  // golden angle
    return vec2(r * cos(theta), r * sin(theta));
}

// Interleaved gradient noise -> per-pixel rotation in radians.
float ignRotation(vec2 fragCoord) {
    float n = fract(52.9829189 * fract(dot(fragCoord, vec2(0.06711056, 0.00583715))));
    return n * 6.28318530718;
}

float sampleShadowFactor(sampler2D shadowMap, vec2 uv, float currentDepth, float bias, int quality) {
    vec2 texel = 1.0 / vec2(textureSize(shadowMap, 0));
    float phi = ignRotation(gl_FragCoord.xy);

    // Hard
    if (quality <= 0) {
        float d = texture(shadowMap, uv).r;
        return (currentDepth - bias > d) ? 1.0 : 0.0;
    }

    // Soft: fixed-radius Vogel PCF
    if (quality == 1) {
        const float radius = 3.0;  // texels
        float shadow = 0.0;
        for (int i = 0; i < kPcfTaps; ++i) {
            vec2 o = vogelDiskSample(i, kPcfTaps, phi) * radius * texel;
            float d = texture(shadowMap, uv + o).r;
            shadow += (currentDepth - bias > d) ? 1.0 : 0.0;
        }
        return shadow / float(kPcfTaps);
    }

    // High: PCSS
    // 1) Blocker search.
    const float searchRadius = 5.0;  // texels
    float blockerSum = 0.0;
    int blockerCnt = 0;
    for (int i = 0; i < kPcssBlockerTaps; ++i) {
        vec2 o = vogelDiskSample(i, kPcssBlockerTaps, phi) * searchRadius * texel;
        float d = texture(shadowMap, uv + o).r;
        if (d < currentDepth - bias) {
            blockerSum += d;
            blockerCnt += 1;
        }
    }
    if (blockerCnt == 0) return 0.0;  // no blockers -> fully lit
    float avgBlocker = blockerSum / float(blockerCnt);

    // 2) Penumbra estimate (depths in NDC [0,1]); 60.0 is a light-size tunable.
    float penumbra = (currentDepth - avgBlocker) / max(avgBlocker, 1e-4);
    float radius = clamp(penumbra * 60.0, 2.0, 16.0);  // texels

    // 3) Variable-radius Vogel PCF.
    float shadow = 0.0;
    for (int i = 0; i < kPcssFilterTaps; ++i) {
        vec2 o = vogelDiskSample(i, kPcssFilterTaps, phi) * radius * texel;
        float d = texture(shadowMap, uv + o).r;
        shadow += (currentDepth - bias > d) ? 1.0 : 0.0;
    }
    return shadow / float(kPcssFilterTaps);
}

#endif  // SHARED_SHADOW_SAMPLING_GLSL