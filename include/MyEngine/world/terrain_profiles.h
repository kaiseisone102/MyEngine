#pragma once
// =============================================================================
// terrain_profiles.h - height functions per stage
// =============================================================================
// Collection of "height functions" passable to TerrainMesh::HeightFunc.
// Each function takes (worldX, worldZ) and returns height (m) as a pure function.
//
// Design:
//   - sin/cos composition for perlin-noise-like smooth undulations
//   - No external library dependency (std::sin / std::cos only)
//   - Each profile uses 3-tier scale (large/mid/fine) for natural look
//   - Use per stage via `addTerrain(data, ..., terrain_profile::xxx)`
//
// How to add a new profile:
//   1. Add inline float xxx(float x, float z) here
//   2. Pass it as the 4th arg of addTerrain in the corresponding stage
// =============================================================================
#include <cmath>

namespace terrain_profile {

// flat: completely flat
// For indoor/cleared stages like Terminal.
inline float flat(float, float) { return 0.f; }

// rollingHills: gentle rolling hills (modest +/- 1m)
// Player's cube ground (flat board) deviates by at most ~1m. Modest undulation.
// 3-tier scale composition expresses "natural meadow waves".
//   large: wavelength ~30m, amplitude +/- 0.6m  (big waves)
//   mid:   wavelength ~10m, amplitude +/- 0.3m  (mid undulations)
//   fine:  wavelength ~3m,  amplitude +/- 0.1m  (fine bumps)
//   total: roughly within +/- 1m
inline float rollingHills(float x, float z) {
    const float h1 = std::sin(x * 0.21f) * std::cos(z * 0.18f) * 0.6f;
    const float h2 = std::sin(x * 0.60f + 1.3f) * std::sin(z * 0.55f + 0.7f) * 0.3f;
    const float h3 = std::sin(x * 1.90f) * std::cos(z * 2.10f) * 0.1f;
    return h1 + h2 + h3;
}

// stage1_1_terrain: Stage 1-1 (undulation + steep hill + dent)
// Based on rollingHills, adds a steep mesa (table mountain) and a dent at
// specific positions.
//
// Steep hill (mesa): center (15, 0, 15)
//   - top radius 2m: flat height +2m
//   - steep slope radius 2m to 3.5m: smoothstep interpolation 2m -> 0m
//     max slope = height 2m / width 1.5m * 1.5 (smoothstep coef) = 2.0 ~= 63deg
//     Definitely steeper than 45deg threshold, movement_system will reject it.
//   - radius >= 3.5m: flat (height 0)
//
// Dent: center (-12, -12), sigma=7m, depth 3m
//   Slope ~ 3/7 = 0.43 (23deg), can be descended normally.
inline float stage1_1_terrain(float x, float z) {
    // Base: gentle rolling hills (+/- 1m or so)
    float h = rollingHills(x, z);

    // Steep hill (mesa shape, intended to be unclimbable)
    {
        const float dx = x - 15.f;
        const float dz = z - 15.f;
        const float r = std::sqrt(dx * dx + dz * dz);
        constexpr float innerR = 2.0f;
        constexpr float outerR = 3.5f;
        constexpr float height = 2.0f;
        if (r < innerR) {
            h += height;
        } else if (r < outerR) {
            const float t = (r - innerR) / (outerR - innerR);
            const float s = t * t * (3.f - 2.f * t);  // smoothstep
            h += height * (1.f - s);
        }
    }

    // Dent (gaussian, descendable)
    {
        const float dx = x - (-12.f);
        const float dz = z - (-12.f);
        const float r2 = dx * dx + dz * dz;
        constexpr float sigma = 7.f;
        constexpr float amp = 3.f;
        h -= amp * std::exp(-r2 / (sigma * sigma));
    }

    return h;
}

// (future candidates)
// - mountainous: mountain terrain (+/- 5m, steep slopes)
// - meadow:      plain (+/- 0.3m, nearly flat)
// - canyon:      valley terrain (long valley in X direction)
// - islands:     sea + small islands

}  // namespace terrain_profile
