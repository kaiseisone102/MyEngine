// =============================================================================
// shared/gbuffer.glsl
//
// PART4 4a-2: helpers for the opaque MRT GBuffer prepass.
//   - encodeNormal:  world-space normal -> octahedral encoded (xy in [0,1])
//                    Written to R10G10B10A2_UNORM at .rg. 20-bit precision is
//                    UE5/Frostbite-class for SSAO/SSGI/SSR/DoF.
//   - computeMotion: current vs previous frame NDC delta (RG16F).
//                    Caller must pass clip-space (pre-divide) positions.
// =============================================================================
#ifndef MYENGINE_SHARED_GBUFFER_GLSL
#define MYENGINE_SHARED_GBUFFER_GLSL

vec2 _signNonZero(vec2 v) {
    return vec2(v.x >= 0.0 ? 1.0 : -1.0,
                v.y >= 0.0 ? 1.0 : -1.0);
}

// Octahedral wrap for the lower hemisphere; used by both encode and decode.
vec2 _octWrap(vec2 v) {
    return (1.0 - abs(v.yx)) * _signNonZero(v);
}

// Encode a normalized world-space normal into [0,1]^2 (octahedral, equal-area).
// The output is meant for R10G10 storage; the remaining BA12 in R10G10B10A2
// are free for material id / roughness in later phases.
vec2 encodeNormal(vec3 n) {
    n = normalize(n);
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    vec2 e = n.z >= 0.0 ? n.xy : _octWrap(n.xy);
    return e * 0.5 + 0.5;
}

// Decode the encoded normal back to a unit vector (sampler returns [0,1]^2).
vec3 decodeNormal(vec2 e) {
    e = e * 2.0 - 1.0;
    vec3 n = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
    if (n.z < 0.0) n.xy = _octWrap(n.xy);
    return normalize(n);
}

// Motion = NDC delta from previous to current frame, ignoring jitter (TAA
// will subtract jitter on consumption). curClip / prevClip are clip-space
// (post-projection, pre-divide); we divide here so callers don't have to
// remember.
vec2 computeMotion(vec4 curClip, vec4 prevClip) {
    vec2 curNDC  = curClip.xy  / max(curClip.w, 1e-6);
    vec2 prevNDC = prevClip.xy / max(prevClip.w, 1e-6);
    return curNDC - prevNDC;
}

#endif  // MYENGINE_SHARED_GBUFFER_GLSL
