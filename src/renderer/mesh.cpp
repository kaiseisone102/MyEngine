// src/renderer/mesh.cpp

#include "renderer/mesh.h"

#include <tiny_obj_loader.h>

#include <cstring>
#include <stdexcept>
#include <unordered_map>

#include "renderer/resource_factory.h"
#include "renderer/vulkan_context.h"

void Mesh::loadFromObj(const VulkanContext* ctx, const ResourceFactory* resources,
                       const std::string& path) {
    ctx_ = ctx;

    // 1) OBJ をパースして CPU 側データを組み立てる
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str())) {
        throw std::runtime_error("Mesh::loadFromObj: " + warn + err);
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::unordered_map<Vertex, uint32_t> uniqueVertices;

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex v{};
            v.pos = {attrib.vertices[3 * index.vertex_index + 0],
                     attrib.vertices[3 * index.vertex_index + 1],
                     attrib.vertices[3 * index.vertex_index + 2]};

            // OBJ は V 軸が下→上、Vulkan は上→下なので反転
            if (index.texcoord_index >= 0) {
                v.texCoord = {attrib.texcoords[2 * index.texcoord_index + 0],
                              1.0f - attrib.texcoords[2 * index.texcoord_index + 1]};
            }

            if (index.normal_index >= 0) {
                v.normal = {attrib.normals[3 * size_t(index.normal_index) + 0],
                            attrib.normals[3 * size_t(index.normal_index) + 1],
                            attrib.normals[3 * size_t(index.normal_index) + 2]};
            } else {
                v.normal = {0.f, 1.f, 0.f};
            }

            v.color = {1.f, 1.f, 1.f};

            if (uniqueVertices.count(v) == 0) {
                uniqueVertices[v] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(v);
            }
            indices.push_back(uniqueVertices[v]);
        }
    }

    indexCount_ = static_cast<uint32_t>(indices.size());

    // 2) GPU バッファに転送
    uploadBuffer(resources, vertices.data(), sizeof(Vertex) * vertices.size(),
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBuffer_, vertexBufferMemory_);
    uploadBuffer(resources, indices.data(), sizeof(uint32_t) * indices.size(),
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffer_, indexBufferMemory_);
}

// =============================================================================
// createCube — 足元基準の 1x1x1 cube をコードで生成
// =============================================================================
// 頂点座標:
//   X: [-0.5, +0.5]
//   Y: [ 0  ,  1  ]   ← 足元基準
//   Z: [-0.5, +0.5]
//
// scale 適用後の AABB は AABB::fromBottomCenter(pos, scale) と完全に一致:
//   X: [pos.x - scale.x*0.5, pos.x + scale.x*0.5]
//   Y: [pos.y, pos.y + scale.y]
//   Z: [pos.z - scale.z*0.5, pos.z + scale.z*0.5]
//
// 6 面 × 4 頂点 = 24 頂点 (面ごとに別頂点。 法線と UV が面ごとに異なるため共有不可)。
// 6 面 × 2 三角形 = 12 三角形 = 36 インデックス。
//
// 巻き方向:
//   Vulkan の規約 (front face = CCW、 cullMode=BACK) に合わせて、
//   外側から見て CCW になるように頂点順を選ぶ。
// =============================================================================
void Mesh::createCube(const VulkanContext* ctx, const ResourceFactory* resources) {
    ctx_ = ctx;

    // 立方体の 8 隅 (足元基準: Y は [0, 1])
    //   y=0 (底面)              y=1 (上面)
    //     (-x,0,+z) (+x,0,+z)     (-x,1,+z) (+x,1,+z)
    //     (-x,0,-z) (+x,0,-z)     (-x,1,-z) (+x,1,-z)
    const float h = 0.5f;  // 水平方向のハーフサイズ

    // 各面 (法線、 4 頂点の座標) を定義し、 UV と triangulation を共通処理。
    struct Face {
        glm::vec3 normal;
        glm::vec3 corners[4];  // 反時計回り (外から見て)
    };

    const Face faces[6] = {
        // +Y (上面)
        {{0.f, 1.f, 0.f}, {{-h, 1.f, -h}, {-h, 1.f, +h}, {+h, 1.f, +h}, {+h, 1.f, -h}}},
        // -Y (底面)
        {{0.f, -1.f, 0.f}, {{-h, 0.f, +h}, {-h, 0.f, -h}, {+h, 0.f, -h}, {+h, 0.f, +h}}},
        // +X (右面)
        {{1.f, 0.f, 0.f}, {{+h, 0.f, -h}, {+h, 1.f, -h}, {+h, 1.f, +h}, {+h, 0.f, +h}}},
        // -X (左面)
        {{-1.f, 0.f, 0.f}, {{-h, 0.f, +h}, {-h, 1.f, +h}, {-h, 1.f, -h}, {-h, 0.f, -h}}},
        // +Z (奥面)
        {{0.f, 0.f, 1.f}, {{+h, 0.f, +h}, {+h, 1.f, +h}, {-h, 1.f, +h}, {-h, 0.f, +h}}},
        // -Z (前面)
        {{0.f, 0.f, -1.f}, {{-h, 0.f, -h}, {-h, 1.f, -h}, {+h, 1.f, -h}, {+h, 0.f, -h}}},
    };

    // UV: 各面で (0,0)→(0,1)→(1,1)→(1,0) (左下→左上→右上→右下)。
    // V 反転は loadFromObj と異なり、 ここでは画像座標と直接対応させる。
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
            // jointIndices / jointWeights は default 初期化で 0
            vertices.push_back(v);
        }
        // 4 頂点を 2 三角形に: (0,1,2) と (0,2,3) (反時計回り = 外向き)
        indices.push_back(baseIdx + 0);
        indices.push_back(baseIdx + 1);
        indices.push_back(baseIdx + 2);
        indices.push_back(baseIdx + 0);
        indices.push_back(baseIdx + 2);
        indices.push_back(baseIdx + 3);
    }

    indexCount_ = static_cast<uint32_t>(indices.size());

    uploadBuffer(resources, vertices.data(), sizeof(Vertex) * vertices.size(),
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBuffer_, vertexBufferMemory_);
    uploadBuffer(resources, indices.data(), sizeof(uint32_t) * indices.size(),
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffer_, indexBufferMemory_);
}

// =============================================================================
// createCrossQuad - Phase 1F: 2 vertical quads crossed at 90 deg, for grass
// =============================================================================
void Mesh::createCrossQuad(const VulkanContext* ctx, const ResourceFactory* resources) {
    ctx_ = ctx;
    const float h = 0.5f;  // half-width in X/Z

    // Two flat quads crossed at 90 deg. Each quad is split into 2 vertical
    // segments (3 rows) so a future wind shader can bend only the upper part.
    // The blade SHAPE (multiple thin leaves) comes from the texture, not the mesh.
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
    indexCount_ = static_cast<uint32_t>(indices.size());
    uploadBuffer(resources, vertices.data(), sizeof(Vertex) * vertices.size(),
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBuffer_, vertexBufferMemory_);
    uploadBuffer(resources, indices.data(), sizeof(uint32_t) * indices.size(),
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffer_, indexBufferMemory_);
}

void Mesh::uploadBuffer(const ResourceFactory* resources, const void* src, VkDeviceSize size,
                        VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& memory) const {
    // staging(HOST_VISIBLE)→ device-local (高速)
    VkBuffer staging{};
    VkDeviceMemory stagingMem{};
    resources->createBuffer(
        size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging,
        stagingMem);

    void* data = nullptr;
    vkMapMemory(ctx_->device(), stagingMem, 0, size, 0, &data);
    std::memcpy(data, src, static_cast<size_t>(size));
    vkUnmapMemory(ctx_->device(), stagingMem);

    resources->createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, memory);
    resources->copyBuffer(staging, buffer, size);

    vkDestroyBuffer(ctx_->device(), staging, nullptr);
    vkFreeMemory(ctx_->device(), stagingMem, nullptr);
}

void Mesh::bind(VkCommandBuffer cmd) const {
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer_, &offset);
    vkCmdBindIndexBuffer(cmd, indexBuffer_, 0, VK_INDEX_TYPE_UINT32);
}

void Mesh::destroy() {
    if (!ctx_) return;
    if (indexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx_->device(), indexBuffer_, nullptr);
        indexBuffer_ = VK_NULL_HANDLE;
    }
    if (indexBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(ctx_->device(), indexBufferMemory_, nullptr);
        indexBufferMemory_ = VK_NULL_HANDLE;
    }
    if (vertexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx_->device(), vertexBuffer_, nullptr);
        vertexBuffer_ = VK_NULL_HANDLE;
    }
    if (vertexBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(ctx_->device(), vertexBufferMemory_, nullptr);
        vertexBufferMemory_ = VK_NULL_HANDLE;
    }
    indexCount_ = 0;
    ctx_ = nullptr;
}
