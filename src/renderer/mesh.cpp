// src/renderer/mesh.cpp

#include "renderer/mesh.h"

#include <vector>

#include "renderer/geometry_buffer.h"
#include "renderer/vulkan_context.h"

// =============================================================================
// createCube - foot-based 1x1x1 cube generated in code (no .obj file needed)
// =============================================================================
// Vertex coords:
//   X: [-0.5, +0.5]
//   Y: [ 0  ,  1  ]   foot-based
//   Z: [-0.5, +0.5]
//
// After scale, the AABB equals AABB::fromBottomCenter(pos, scale):
//   X: [pos.x - scale.x*0.5, pos.x + scale.x*0.5]
//   Y: [pos.y, pos.y + scale.y]
//   Z: [pos.z - scale.z*0.5, pos.z + scale.z*0.5]
//
// 6 faces x 4 vertices = 24 vertices (per-face vertices; normal/UV differ).
// 6 faces x 2 triangles = 12 triangles = 36 indices.
//
// Winding: CCW seen from outside (Vulkan front face = CCW, cullMode = BACK).
// =============================================================================
void Mesh::createCube(const VulkanContext* ctx, GeometryBuffer* geom) {
    ctx_ = ctx;

    // 8 corners of the cube (foot-based: Y in [0, 1])
    //   y=0 (bottom)             y=1 (top)
    //     (-x,0,+z) (+x,0,+z)     (-x,1,+z) (+x,1,+z)
    //     (-x,0,-z) (+x,0,-z)     (-x,1,-z) (+x,1,-z)
    const float h = 0.5f;  // horizontal half-size

    // Each face: normal + 4 corners (CCW from outside). UV and triangulation shared.
    struct Face {
        glm::vec3 normal;
        glm::vec3 corners[4];
    };

    const Face faces[6] = {
        // +Y (top)
        {{0.f, 1.f, 0.f}, {{-h, 1.f, -h}, {-h, 1.f, +h}, {+h, 1.f, +h}, {+h, 1.f, -h}}},
        // -Y (bottom)
        {{0.f, -1.f, 0.f}, {{-h, 0.f, +h}, {-h, 0.f, -h}, {+h, 0.f, -h}, {+h, 0.f, +h}}},
        // +X (right)
        {{1.f, 0.f, 0.f}, {{+h, 0.f, -h}, {+h, 1.f, -h}, {+h, 1.f, +h}, {+h, 0.f, +h}}},
        // -X (left)
        {{-1.f, 0.f, 0.f}, {{-h, 0.f, +h}, {-h, 1.f, +h}, {-h, 1.f, -h}, {-h, 0.f, -h}}},
        // +Z (back)
        {{0.f, 0.f, 1.f}, {{+h, 0.f, +h}, {+h, 1.f, +h}, {-h, 1.f, +h}, {-h, 0.f, +h}}},
        // -Z (front)
        {{0.f, 0.f, -1.f}, {{-h, 0.f, -h}, {-h, 1.f, -h}, {+h, 1.f, -h}, {+h, 0.f, -h}}},
    };

    // UV per face: (0,1) -> (0,0) -> (1,0) -> (1,1)  (bot-left, top-left, top-right, bot-right)
    const glm::vec2 uvs[4] = {{0.f, 1.f}, {0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}};

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(24);
    indices.reserve(36);

    for (const Face& f : faces) {
        const uint32_t baseIdx = static_cast<uint32_t>(vertices.size());
        for (int i = 0; i < 4; ++i) {
            Vertex v{};
            v.pos = f.corners[i];
            v.color = {1.f, 1.f, 1.f};
            v.texCoord = uvs[i];
            v.normal = f.normal;
            vertices.push_back(v);
        }
        indices.push_back(baseIdx + 0);
        indices.push_back(baseIdx + 1);
        indices.push_back(baseIdx + 2);
        indices.push_back(baseIdx + 0);
        indices.push_back(baseIdx + 2);
        indices.push_back(baseIdx + 3);
    }

    const MeshHandle handle = geom->alloc(vertices, indices);
    geom_ = geom;
    firstIndex_ = handle.firstIndex;
    vertexOffset_ = handle.vertexOffset;
    blockIndex_ = handle.blockIndex;
    indexCount_ = handle.indexCount;
}

// =============================================================================
// createCrossQuad - Phase 1F: 2 vertical quads crossed at 90 deg, for grass.
// Each quad is split into 2 vertical segments (3 rows) so a future wind shader
// can bend only the upper part. The blade shape (multiple thin leaves) comes
// from the texture, not the mesh.
// =============================================================================
void Mesh::createCrossQuad(const VulkanContext* ctx, GeometryBuffer* geom) {
    ctx_ = ctx;
    const float h = 0.5f;  // half-width in X/Z

    const float rowY[3] = {0.0f, 0.5f, 1.0f};
    struct Plane { int axis; glm::vec3 normal; };
    const Plane planes[2] = {
        {0, {0.f, 0.f, 1.f}},  // width along X
        {1, {1.f, 0.f, 0.f}},  // width along Z
    };

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(2 * 3 * 2);
    indices.reserve(2 * 2 * 6);

    for (const Plane& pl : planes) {
        const uint32_t base = static_cast<uint32_t>(vertices.size());
        for (int r = 0; r < 3; ++r) {
            for (int s = 0; s < 2; ++s) {
                const float sign = (s == 0) ? -1.f : +1.f;
                Vertex v{};
                if (pl.axis == 0) v.pos = {sign * h, rowY[r], 0.f};
                else              v.pos = {0.f, rowY[r], sign * h};
                v.color = {1.f, 1.f, 1.f};
                v.texCoord = {(s == 0) ? 0.f : 1.f, 1.f - rowY[r]};
                v.normal = pl.normal;
                vertices.push_back(v);
            }
        }
        for (int r = 0; r < 2; ++r) {
            const uint32_t a = base + r * 2;
            const uint32_t b = base + r * 2 + 1;
            const uint32_t c = base + (r + 1) * 2;
            const uint32_t d = base + (r + 1) * 2 + 1;
            indices.push_back(a); indices.push_back(c); indices.push_back(d);
            indices.push_back(a); indices.push_back(d); indices.push_back(b);
        }
    }

    const MeshHandle handle = geom->alloc(vertices, indices);
    geom_ = geom;
    firstIndex_ = handle.firstIndex;
    vertexOffset_ = handle.vertexOffset;
    blockIndex_ = handle.blockIndex;
    indexCount_ = handle.indexCount;
}

void Mesh::bind(VkCommandBuffer cmd) const {
    geom_->bindBlock(cmd, blockIndex_);  // draw uses firstIndex()/vertexOffset()
}

void Mesh::bindAndDraw(VkCommandBuffer cmd, uint32_t instanceCount,
                       uint32_t firstInstance) const {
    bind(cmd);
    vkCmdDrawIndexed(cmd, indexCount(), instanceCount, firstIndex(), vertexOffset(),
                     firstInstance);
}
