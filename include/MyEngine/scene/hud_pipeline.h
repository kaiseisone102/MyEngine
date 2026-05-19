#pragma once
// =============================================================================
// hud_pipeline.h — HUD 用 graphics pipeline (rect/circle/ring/segment)
// =============================================================================

#include <vulkan/vulkan.h>

#include <string>

class VulkanContext;

class HudPipeline {
   public:
    struct InitInfo {
        VulkanContext* ctx = nullptr;
        VkRenderPass renderPass = VK_NULL_HANDLE;
        std::string shaderDir;
    };

    void init(const InitInfo& info);
    void shutdown();

    VkPipelineLayout layout() const { return layout_; }
    VkPipeline pipeline() const { return pipeline_; }

   private:
    VulkanContext* ctx_ = nullptr;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
};
