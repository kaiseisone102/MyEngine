#pragma once
// =============================================================================
// water_pipeline.h - Phase 1A2: PushConstants now uses shared::WaterPushConstants
// + 2 pipelines (fakeOnly + withReflection)
// =============================================================================
// fakeOnly_:        water.vert + water.frag         (no reflection texture)
// withReflection_:  water.vert + water_reflect.frag (set=1 reflection texture + UBO)
//
// PushConstants is now 112 bytes (includes mat4 model). This matches the GLSL
// layout in shared/types.h, fixing the previous CPU/GPU mismatch.
// =============================================================================
#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "shaders/shared/types.h"

class VulkanContext;

class WaterPipeline {
   public:
    using PushConstants = myengine::shared::WaterPushConstants;

    struct InitInfo {
        VulkanContext* ctx = nullptr;
        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkDescriptorSetLayout frameSetLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout reflectionSetLayout = VK_NULL_HANDLE;
        std::string shaderDir;
    };

    void init(const InitInfo& info);
    void shutdown();

    VkPipelineLayout layoutFakeOnly() const { return layoutFakeOnly_; }
    VkPipeline pipelineFakeOnly() const { return pipelineFakeOnly_; }
    VkPipelineLayout layoutWithReflection() const { return layoutWithReflection_; }
    VkPipeline pipelineWithReflection() const { return pipelineWithReflection_; }

   private:
    VulkanContext* ctx_ = nullptr;
    VkPipelineLayout layoutFakeOnly_ = VK_NULL_HANDLE;
    VkPipeline pipelineFakeOnly_ = VK_NULL_HANDLE;
    VkPipelineLayout layoutWithReflection_ = VK_NULL_HANDLE;
    VkPipeline pipelineWithReflection_ = VK_NULL_HANDLE;

    VkPipeline buildPipeline(VkRenderPass renderPass, VkPipelineLayout layout,
                              const std::string& shaderDir, const std::string& fragName);
};
