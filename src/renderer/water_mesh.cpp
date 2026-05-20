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
    resources->createBuffer(vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            vertexBuffer_, vertexMemory_);
    void* vbData = nullptr;
    vkMapMemory(ctx_->device(), vertexMemory_, 0, vbSize, 0, &vbData);
    std::memcpy(vbData, vertices.data(), static_cast<size_t>(vbSize));
    vkUnmapMemory(ctx_->device(), vertexMemory_);

    const VkDeviceSize ibSize = sizeof(uint32_t) * indices.size();
    resources->createBuffer(ibSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            indexBuffer_, indexMemory_);
    void* ibData = nullptr;
    vkMapMemory(ctx_->device(), indexMemory_, 0, ibSize, 0, &ibData);
    std::memcpy(ibData, indices.data(), static_cast<size_t>(ibSize));
    vkUnmapMemory(ctx_->device(), indexMemory_);
}

void WaterMesh::destroy() {
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;
    if (indexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx_->device(), indexBuffer_, nullptr);
        indexBuffer_ = VK_NULL_HANDLE;
    }
    if (indexMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(ctx_->device(), indexMemory_, nullptr);
        indexMemory_ = VK_NULL_HANDLE;
    }
    if (vertexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx_->device(), vertexBuffer_, nullptr);
        vertexBuffer_ = VK_NULL_HANDLE;
    }
    if (vertexMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(ctx_->device(), vertexMemory_, nullptr);
        vertexMemory_ = VK_NULL_HANDLE;
    }
    indexCount_ = 0;
    ctx_ = nullptr;
}

void WaterMesh::bind(VkCommandBuffer cmd) const {
    if (!vertexBuffer_ || !indexBuffer_) return;
    VkBuffer vbs[] = {vertexBuffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer_, 0, VK_INDEX_TYPE_UINT32);
}
