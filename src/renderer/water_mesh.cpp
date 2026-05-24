// =============================================================================
// water_mesh.cpp
// =============================================================================
#include "renderer/water_mesh.h"

#include <stdexcept>
#include <vector>

#include "renderer/resource_factory.h"
#include "renderer/vulkan_context.h"

void WaterMesh::init(VulkanContext* ctx, ResourceFactory* resources, glm::vec3 center,
                     glm::vec2 sizeXZ, int resolution) {
    if (!ctx || !resources) throw std::runtime_error("WaterMesh::init: invalid context");
    if (resolution < 1) resolution = 1;

    ctx_ = ctx;
    center_ = center;
    sizeXZ_ = sizeXZ;

    const float halfX = sizeXZ.x * 0.5f;
    const float halfZ = sizeXZ.y * 0.5f;
    const int N = resolution;
    const int verticesPerSide = N + 1;

    std::vector<WaterVertex> vertices;
    vertices.reserve(static_cast<size_t>(verticesPerSide) * verticesPerSide);
    for (int iz = 0; iz <= N; ++iz) {
        const float tz = static_cast<float>(iz) / static_cast<float>(N);
        const float worldZ = center.z - halfZ + tz * sizeXZ.y;
        for (int ix = 0; ix <= N; ++ix) {
            const float tx = static_cast<float>(ix) / static_cast<float>(N);
            const float worldX = center.x - halfX + tx * sizeXZ.x;
            WaterVertex v{};
            v.pos = glm::vec3{worldX, center.y, worldZ};
            v.texCoord = glm::vec2(tx, tz);  // Phase 1B-6
            vertices.push_back(v);
        }
    }

    std::vector<uint32_t> indices;
    indices.reserve(static_cast<size_t>(N) * N * 6);
    for (int iz = 0; iz < N; ++iz) {
        for (int ix = 0; ix < N; ++ix) {
            const uint32_t i0 = static_cast<uint32_t>(iz * verticesPerSide + ix);
            const uint32_t i1 = i0 + 1;
            const uint32_t i2 = i0 + verticesPerSide;
            const uint32_t i3 = i2 + 1;
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    indexCount_ = static_cast<uint32_t>(indices.size());

    const VkDeviceSize vbSize = sizeof(WaterVertex) * vertices.size();
    vertexBuffer_ = VmaBuffer::createMappedHostVisible(ctx_, vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    std::memcpy(vertexBuffer_.mapped(), vertices.data(), static_cast<size_t>(vbSize));

    const VkDeviceSize ibSize = sizeof(uint32_t) * indices.size();
    indexBuffer_ = VmaBuffer::createMappedHostVisible(ctx_, ibSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    std::memcpy(indexBuffer_.mapped(), indices.data(), static_cast<size_t>(ibSize));
}

void WaterMesh::destroy() {
    // VkUnique frees each handle (no-op if empty). The auto destructor would do
    // the same if destroy() were never called.
    indexBuffer_.reset();
    vertexBuffer_.reset();
    indexCount_ = 0;
    ctx_ = nullptr;
}

void WaterMesh::bind(VkCommandBuffer cmd) const {
    if (!vertexBuffer_ || !indexBuffer_) return;
    VkBuffer vbs[] = {vertexBuffer_.buffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer_.buffer(), 0, VK_INDEX_TYPE_UINT32);
}
