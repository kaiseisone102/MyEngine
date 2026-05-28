// include/MyEngine/renderer/projection.h
#pragma once
// =============================================================================
// projection.h - Reverse-Z + Infinite far perspective for Vulkan
// =============================================================================
// NDC z convention (Vulkan 0..1 clip depth + reverse-Z):
//   - 1.0 at the near plane (closest to camera)
//   - 0.0 at infinity (farthest from camera)
// Pair with:
//   - VkPipelineDepthStencilStateCreateInfo::depthCompareOp = VK_COMPARE_OP_GREATER
//   - VkClearValue::depthStencil = {0.0f, 0}
//   - VkViewport::minDepth/maxDepth = 0.0f/1.0f (unchanged from forward-Z)
//
// View-space depth recovery from NDC z (when sampling the depth texture):
//   view_z = -zNear / ndc_z         (reverse-Z infinite far)
//
// Future additions and their reverse-Z conventions:
//   * Skybox / atmosphere: render at NDC z = 0.0 with
//     depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL so real geometry wins.
//   * Depth-based fog: linearize via the formula above, do not assume z grows
//     with distance.
//   * TAA / SSAO / SSGI / SSR / soft particles / decals: depth-texture readers
//     must treat 1 as near and 0 as far.
//
// Shadow pass keeps a separate non-reverse-Z orthographic projection (closed
// subsystem in shadow_light.h + shadow_sampling.glsl). Do not change it here.
//
// References:
//   - https://gist.github.com/pezcode/1609b61a1eedd207ec8c5acf6f94f53a
//   - https://tomhultonharrop.com/posts/reverse-z/
// =============================================================================

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <cmath>

inline glm::mat4 makeReversedZInfinitePerspective(float fovYRad, float aspect, float zNear) {
    const float f = 1.0f / std::tan(fovYRad * 0.5f);
    glm::mat4 m(0.0f);
    m[0][0] = f / aspect;
    m[1][1] = -f;        // Vulkan Y flip baked in
    m[2][3] = -1.0f;
    m[3][2] = zNear;
    return m;
}
