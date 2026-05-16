// include/MyEngine/renderer/frame_uniforms.h
#pragma once
// =============================================================================
// frame_uniforms.h — Phase 1-D 段階2-a
//   per-frame の UBO + ShadowMap の DescriptorSet を所有。
//
//   段階2-a 変更:
//     - texture (binding=1) を削除し、Material set (set=1) へ移動。
//     - shadowMap は binding=2 -> binding=1 へ移動。
//
//   set=0 (frame) の binding 構成:
//     binding=0: UBO (LightingUBO)
//     binding=1: shadowMap
// =============================================================================

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <glm/glm.hpp>

#include "frame_sync.h"

class VulkanContext;
class ResourceFactory;

class FrameUniforms {
   public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = FrameSync::MAX_FRAMES_IN_FLIGHT;

    struct LightingData {
        glm::mat4 vp{1.f};
        glm::mat4 lightVP{1.f};
        glm::vec3 lightPos{0.f};
        glm::vec3 lightColor{1.f};
        glm::vec3 viewPos{0.f};
        float ambient = 0.15f;
        float specular = 0.5f;
        float shadowStrength = 0.6f;
        float shadowBias = 0.003f;
    };

    void init(VulkanContext* ctx, ResourceFactory* resources);
    void shutdown();

    // shadowMap の弱参照を差し替える (init 後にシャドウマップが用意されたら呼ぶ)。
    void bindShadowMap(VkImageView view, VkSampler sampler);

    // bindShadowMap の内容を全 frame の DescriptorSet に書き込む。
    void rebuildDescriptorSets();

    // 毎フレーム呼ぶ。CPU データを mapped メモリへコピー。
    void update(uint32_t frameIndex, const LightingData& data);

    VkDescriptorSetLayout layout() const { return layout_; }
    VkDescriptorSet descriptorSet(uint32_t frameIndex) const { return sets_[frameIndex]; }

   private:
    struct LightingUBO {
        glm::mat4 vp;
        glm::mat4 lightVP;
        alignas(16) glm::vec3 lightPos;
        float _p0;
        alignas(16) glm::vec3 lightColor;
        float _p1;
        alignas(16) glm::vec3 viewPos;
        float _p2;
        float ambient;
        float specular;
        float shadowStrength;
        float shadowBias;
    };

    VulkanContext* ctx_ = nullptr;
    ResourceFactory* resources_ = nullptr;

    VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
    VkDescriptorPool pool_ = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> sets_{};

    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> buffers_{};
    std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> memories_{};
    std::array<void*, MAX_FRAMES_IN_FLIGHT> mapped_{};
    VkDeviceSize bufferSize_ = 0;

    // shadowMap の弱参照 (Material set へ移動した texture はここでは持たない)
    VkImageView shadowView_ = VK_NULL_HANDLE;
    VkSampler shadowSampler_ = VK_NULL_HANDLE;

    void createLayout();
    void createPool();
    void createBuffers();
    void allocateSets();
};
