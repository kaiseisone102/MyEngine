#pragma once
// =============================================================================
// frame_uniforms.h — 通常用 + 反射用の UBO / descriptor set
// =============================================================================
// レイアウトは共通の UBO とシャドウマップを使うため、 共用レイアウトに対応する。
// pool は 2 倍 (通常 MAX_FRAMES_IN_FLIGHT 個 + 反射 MAX_FRAMES_IN_FLIGHT 個)。
// =============================================================================

#include <vulkan/vulkan.h>

#include "renderer/vk_unique.h"
#include "renderer/vma_buffer.h"

#include <array>
#include <cstdint>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include "shaders/shared/types.h"

class VulkanContext;
class ResourceFactory;

class FrameUniforms {
   public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    using LightingUBO = myengine::shared::FrameUBO;

    void init(VulkanContext* ctx, ResourceFactory* resources);
    void shutdown();

    // shadow map (set=0 binding=1) を bind
    void bindShadowMap(VkImageView view, VkSampler sampler);
    void rebuildDescriptorSets();

    // 通常描画の UBO 更新
    void update(uint32_t frameIndex, const LightingUBO& data);
    VkDescriptorSet descriptorSet(uint32_t frameIndex) const;

    // 反射描画の UBO 更新
    void updateReflection(uint32_t frameIndex, const LightingUBO& data);
    VkDescriptorSet descriptorSetReflection(uint32_t frameIndex) const;

    VkDescriptorSetLayout layout() const { return layout_.get(); }

   private:
    VulkanContext* ctx_ = nullptr;

    VkUnique<VkDescriptorSetLayout> layout_;
    VkUnique<VkDescriptorPool> pool_;

    std::array<VmaBuffer, MAX_FRAMES_IN_FLIGHT> buffers_{};
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> sets_{};

    std::array<VmaBuffer, MAX_FRAMES_IN_FLIGHT> buffersReflection_{};
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> setsReflection_{};

    VkImageView shadowView_ = VK_NULL_HANDLE;
    VkSampler shadowSampler_ = VK_NULL_HANDLE;

    void createLayoutAndPool();
    void createUbos(ResourceFactory* resources);
    void allocateSets();
};
