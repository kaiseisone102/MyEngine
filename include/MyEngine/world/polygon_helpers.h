#pragma once
// =============================================================================
// polygon_helpers.h - polygon (XZ plane, glm::vec2 array) generation helpers
// =============================================================================
// For passing to TerrainMesh::init's polygonXZ argument.
// Returns CCW (counter-clockwise) vertices (poly2tri input convention).
//
// Usage:
//   addTerrain(data, polygon::rectangle({0, 0}, {50, 50}), 0.0f, ...);
//   addTerrain(data, polygon::circle({-30, 0}, 25), 5.0f, ...);
//   addTerrain(data, polygon::ellipse({80, 80}, 40, 20), 5.0f, ...);
// =============================================================================
#include <cmath>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace polygon {

// Rectangle (center + size, CCW)
inline std::vector<glm::vec2> rectangle(glm::vec2 center, glm::vec2 size) {
    const float hx = size.x * 0.5f;
    const float hz = size.y * 0.5f;
    return {
        {center.x - hx, center.y - hz},
        {center.x + hx, center.y - hz},
        {center.x + hx, center.y + hz},
        {center.x - hx, center.y + hz},
    };
}

// Circle (center + radius + segments, CCW)
inline std::vector<glm::vec2> circle(glm::vec2 center, float radius, int segments = 32) {
    if (segments < 3) segments = 3;
    std::vector<glm::vec2> result;
    result.reserve(segments);
    const float twoPi = 6.28318530717958647692f;
    for (int i = 0; i < segments; ++i) {
        const float angle = twoPi * static_cast<float>(i) / static_cast<float>(segments);
        result.push_back({
            center.x + std::cos(angle) * radius,
            center.y + std::sin(angle) * radius,
        });
    }
    return result;
}

// Ellipse (center + rx + rz + segments, CCW)
inline std::vector<glm::vec2> ellipse(glm::vec2 center, float rx, float rz, int segments = 32) {
    if (segments < 3) segments = 3;
    std::vector<glm::vec2> result;
    result.reserve(segments);
    const float twoPi = 6.28318530717958647692f;
    for (int i = 0; i < segments; ++i) {
        const float angle = twoPi * static_cast<float>(i) / static_cast<float>(segments);
        result.push_back({
            center.x + std::cos(angle) * rx,
            center.y + std::sin(angle) * rz,
        });
    }
    return result;
}

}  // namespace polygon
