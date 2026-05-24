#pragma once
// =============================================================================
// water_mesh.h — 水面用の単純な平面メッシュ
// =============================================================================
// XZ 平面の格子。 center.y が水面 Y 座標。 各頂点は pos のみで OK
// (法線は shader 内で計算、 UV も pos から導出)。
// =============================================================================

#include <vulkan/vulkan.h>
#include "renderer/vk_unique.h"

#include <cstdint>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

class VulkanContext;
class ResourceFactory;

class WaterMesh {
   public:
    struct WaterVertex {
        glm::vec3 pos;
        glm::vec2 texCoord;  // Phase 1B-6: enables fragDepthFactor without descriptor mismatch
    };

    void init(VulkanContext* ctx, ResourceFactory* resources, glm::vec3 center, glm::vec2 sizeXZ,
              int resolution);
    void destroy();

    void bind(VkCommandBuffer cmd) const;
    uint32_t indexCount() const { return indexCount_; }

    glm::vec3 center() const { return center_; }
    glm::vec2 sizeXZ() const { return sizeXZ_; }

   private:
    VulkanContext* ctx_ = nullptr;

    VkUnique<VkBuffer> vertexBuffer_;
    VkUnique<VkDeviceMemory> vertexMemory_;
    VkUnique<VkBuffer> indexBuffer_;
    VkUnique<VkDeviceMemory> indexMemory_;
    uint32_t indexCount_ = 0;

    glm::vec3 center_{};
    glm::vec2 sizeXZ_{};
};
