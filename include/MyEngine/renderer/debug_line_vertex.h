#pragma once
// =============================================================================
// debug_line_vertex.h — デバッグ線描画用の頂点
// =============================================================================
// 既存の Vertex (pos + color + texCoord + normal + jointIndices + jointWeights)
// とは別の最小限頂点。 線 (LINE_LIST) と三角形塗り (TRIANGLE_LIST) で共用する。
//
// Vulkan layout:
//   location 0: pos   (vec3)  offset = 0
//   location 1: color (vec4)  offset = 12
// sizeof = 28 bytes
// =============================================================================

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

struct DebugLineVertex {
    glm::vec3 pos;
    glm::vec4 color;
};
