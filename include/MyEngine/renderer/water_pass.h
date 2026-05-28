#pragma once
// =============================================================================
// water_pass.h — 2 pipeline 切替 + reflection set (sampler + reflectVP UBO)
// =============================================================================
// set=1 layout:
//   binding=0: sampler2D reflectionTex
//   binding=1: UBO { mat4 reflectVP }  ← world→反射 NDC で UV
//
// useReflection=true: withReflection pipeline + set=1 bind
// useReflection=false: fakeOnly pipeline
// =============================================================================

#include <vulkan/vulkan.h>

#include <array>
#include <string>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "renderer/vk_unique.h"
#include "renderer/vma_buffer.h"
#include "renderer/water_pipeline.h"
#include "scene/scene_data.h"

class VulkanContext;
class ResourceFactory;

class WaterPass {
   public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    struct InitInfo {
        VulkanContext* ctx = nullptr;
        ResourceFactory* resources = nullptr;
        // PART4 4a-1: dynamic rendering — formats instead of VkRenderPass.
        VkFormat colorFormat = VK_FORMAT_UNDEFINED;
        VkFormat depthFormat = VK_FORMAT_UNDEFINED;
        VkDescriptorSetLayout frameSetLayout = VK_NULL_HANDLE;
        std::string shaderDir;
    };

    struct ExecuteInfo {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkDescriptorSet frameSet = VK_NULL_HANDLE;
        const std::vector<WaterDrawItem>* waterDrawList = nullptr;
        float time = 0.f;
        bool useReflection = false;
        uint32_t frameIndex = 0;
        glm::mat4 reflectVP{1.0f};  // world→反射 NDC の VP
    };

    void init(const InitInfo& info);
    void shutdown();
    void execute(const ExecuteInfo& info);

    // ReflectionPass 出力 color を set=1 binding=0 に bind (rebuild 後など)
    void bindReflectionTexture(VkImageView view, VkSampler sampler);

   private:
    VulkanContext* ctx_ = nullptr;

    WaterPipeline pipeline_;

    // set=1 layout (sampler + UBO)
    VkUnique<VkDescriptorSetLayout> reflectionLayout_;
    VkUnique<VkDescriptorPool> reflectionPool_;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> reflectionSets_{};

    // reflection VP UBO (frame in flight 分)
    std::array<VmaBuffer, MAX_FRAMES_IN_FLIGHT> reflectVpBuffers_{};

    // 現在 bind されてる reflection texture
    VkImageView currentReflectView_ = VK_NULL_HANDLE;
    VkSampler currentReflectSampler_ = VK_NULL_HANDLE;

    void createReflectionLayoutAndPool();
    void createReflectionUbo(ResourceFactory* resources);
    void allocateReflectionSets();
    void writeReflectVp(uint32_t frameIndex, const glm::mat4& reflectVP);
    void writeSamplerToSets();
};
