// =============================================================================
// shared/pbr.glsl - Cook-Torrance PBR BRDF (metallic-roughness workflow).
// Included by every lighting frag (triangle / instanced / skinned / bindless).
//
// Design (Phase 1K, 2026-05-25): this file owns ONLY the light<->surface
// reflection math. It deliberately knows nothing about:
//   - how many lights there are        (caller loops and sums)
//   - light type / distance attenuation (caller folds it into `radiance`)
//   - shadowing                         (caller multiplies the result)
//   - ambient / IBL                     (separate helper below)
// That keeps the BRDF reusable as the engine grows toward an open world with
// many lights (Phase 2A), point-light attenuation, and per-light shadows: the
// scene gets richer by calling this more times / scaling radiance, never by
// editing the BRDF itself.
//
// GGX (Trowbridge-Reitz) D, Smith-Schlick G, Schlick F. F0 = 0.04 dielectric
// default, lerped toward albedo by metallic. Energy conserving (metals have no
// diffuse). glTF-compatible metallic-roughness inputs.
// =============================================================================
#ifndef MYENGINE_SHARED_PBR_GLSL
#define MYENGINE_SHARED_PBR_GLSL

const float PBR_PI = 3.14159265359;

// D: GGX / Trowbridge-Reitz normal distribution.
float pbrDistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PBR_PI * denom * denom);
}

// G: Smith's method with Schlick-GGX (direct-lighting k = (r+1)^2 / 8).
float pbrGeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}
float pbrGeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return pbrGeometrySchlickGGX(NdotV, roughness) * pbrGeometrySchlickGGX(NdotL, roughness);
}

// F: Fresnel-Schlick approximation.
vec3 pbrFresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// One light reflecting off one surface. Returns the outgoing radiance Lo for
// THIS light, with NO shadow and NO attenuation applied (caller does both).
//   N,V,L   : normalized normal / view dir / to-light dir
//   radiance: incoming light color (caller pre-scales by attenuation if any)
//   albedo / metallic / roughness: surface material (metallic-roughness)
vec3 pbrDirectLighting(vec3 N, vec3 V, vec3 L, vec3 radiance,
                       vec3 albedo, float metallic, float roughness) {
    vec3 H = normalize(V + L);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float D = pbrDistributionGGX(N, H, roughness);
    float G = pbrGeometrySmith(N, V, L, roughness);
    vec3  F = pbrFresnelSchlick(max(dot(H, V), 0.0), F0);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    vec3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 0.0001);

    // Energy conservation: metals have no diffuse.
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PBR_PI;

    return (diffuse + specular) * radiance * NdotL;
}

// Ambient / environment approximation, proportional to albedo. Single place to
// later fold in AO maps (1K-6) or IBL without touching the per-light BRDF.
vec3 pbrAmbient(vec3 ambientColor, vec3 albedo) {
    return ambientColor * albedo;
}

// =============================================================================
// Surface gradient bump mapping framework (Mikkelsen 2020; used by Unreal).
//
// WHY this and not classic per-vertex tangents or a one-off derivative TBN:
// the modern, open-world-correct way to apply normal detail is to express each
// bump contribution as a SURFACE GRADIENT (a vector in the surface tangent
// plane). Surface gradients ADD LINEARLY, so a base normal map, a detail normal
// map, terrain-layer blends, and decals can all be summed in gradient space and
// resolved ONCE against the interpolated vertex normal. Blending world/tangent
// normals directly is mathematically wrong; summing gradients is correct. We
// build this on screen-space derivatives so no per-vertex tangent attribute is
// needed (the mesh vertex format stays position/color/uv/normal).
//
// Usage in a frag:
//   vec3 grad = vec3(0.0);
//   if (hasBaseNormalMap)  grad += pbrSurfaceGradFromTangentNormal(N, P, uv, nTan);
//   // + detail / terrain / decal contributions here later (just keep adding)
//   vec3 perturbedN = pbrResolveNormal(N, grad);
// where N = normalize(interpolated world normal), P = world position,
//       uv = texcoords, nTan = sampled tangent-space normal in [-1,1].
// =============================================================================

// Cotangent frame from screen-space derivatives (Mikkelsen / thetenthplanet).
// Returns tangent (T) and bitangent (B) in world space for the given UV set,
// already consistent with the interpolated normal N. Sign-correct and robust on
// double-sided / procedural geometry (the failure case Unreal's DeriveTangentBasis
// omits). dp1/dp2 are world-pos derivatives, duv1/duv2 are uv derivatives.
void pbrCotangentFrame(vec3 N, vec3 dp1, vec3 dp2, vec2 duv1, vec2 duv2,
                       out vec3 T, out vec3 B) {
    // Solve the linear system for the tangent directions (perp-trick form).
    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);
    T = dp2perp * duv1.x + dp1perp * duv2.x;
    B = dp2perp * duv1.y + dp1perp * duv2.y;
    float invmax = inversesqrt(max(dot(T, T), dot(B, B)));
    T *= invmax;
    B *= invmax;
}

// One bump contribution -> surface gradient, in the tangent plane of N.
// nTan is the tangent-space normal already remapped to [-1,1]. The returned
// gradient can be summed with other contributions before pbrResolveNormal().
vec3 pbrSurfaceGradFromTangentNormal(vec3 N, vec3 P, vec2 uv, vec3 nTan) {
    vec3 dp1 = dFdx(P);
    vec3 dp2 = dFdy(P);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);
    vec3 T, B;
    pbrCotangentFrame(N, dp1, dp2, duv1, duv2, T, B);
    // Surface gradient of a tangent-space normal (nz>0): project the xy tilt
    // onto (T,B), scaled so that resolving against N reproduces the tangent
    // normal exactly. Guard nz away from 0 for grazing/degenerate samples.
    float nz = max(nTan.z, 1e-5);
    return (nTan.x * T + nTan.y * B) / nz;
}

// Resolve accumulated surface gradient against the geometric (interpolated)
// normal and renormalize. With zero gradient this returns N unchanged.
vec3 pbrResolveNormal(vec3 N, vec3 surfGrad) {
    return normalize(N - surfGrad);
}

#endif  // MYENGINE_SHARED_PBR_GLSL