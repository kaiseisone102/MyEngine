// include/MyEngine/renderer/post_pass.h
#pragma once
// =============================================================================
// post_pass.h - Phase 1H-3: HDR -> swapchain tonemap post-process pass
//
// Inputs:
//   - HDR render target (R16G16B16A16_SFLOAT, sampled image)
//
// Output:
//   - Swapchain image (B8G8R8A8_SRGB)
//
// Pipeline:
//   - Fullscreen triangle (3 vertices, no vertex buffer, gl_VertexIndex trick)
//   - ACES Filmic tonemap (Narkowicz 2016 approximation)
//   - sRGB swapchain handles linear -> sRGB encoding automatically
// =============================================================================
#include <vulkan/vulkan.h>

#include "renderer/vk_unique.h"

#include <string>

class VulkanContext;
class Swapchain;

class PostPass {
   public:
    struct InitInfo {
        VulkanContext* ctx = nullptr;
        Swapchain* swapchain = nullptr;
        VkImageView hdrColorView = VK_NULL_HANDLE;
        VkSampler hdrColorSampler = VK_NULL_HANDLE;
        VkImageView bloomColorView = VK_NULL_HANDLE;   // Phase 1I
        VkSampler bloomColorSampler = VK_NULL_HANDLE;
        std::string shaderDir;
    };

    struct ExecuteInfo {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        uint32_t imageIndex = 0;
    };

    void init(const InitInfo& info);
    void shutdown();

    // Called when swapchain is recreated. Pass the new HDR view + sampler.
    void onSwapchainResized(VkImageView hdrColorView, VkSampler hdrColorSampler,
                            VkImageView bloomColorView, VkSampler bloomColorSampler);

    void execute(const ExecuteInfo& info);

    // Phase 1H-4: tonemapper selection
    void setTonemapMode(int mode) { tonemapMode_ = mode; }
    void setExposure(float e) { exposure_ = e; }
    int  tonemapMode() const { return tonemapMode_; }
    float exposure() const { return exposure_; }

   private:
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void allocateAndUpdateDescriptorSet();
    void createPipelineLayout();
    void createPipeline(const std::string& shaderDir);
    void destroyPipeline();

    VulkanContext* ctx_ = nullptr;
    Swapchain* swapchain_ = nullptr;
    VkImageView hdrColorView_ = VK_NULL_HANDLE;
    VkSampler hdrColorSampler_ = VK_NULL_HANDLE;
    VkImageView bloomColorView_ = VK_NULL_HANDLE;   // Phase 1I
    VkSampler bloomColorSampler_ = VK_NULL_HANDLE;

    // PART4 4d: dynamic rendering migration. VkRenderPass + VkFramebuffer
    // removed; pipeline uses VkPipelineRenderingCreateInfo and execute()
    // uses vkCmdBeginRendering with the swapchain's per-image colorView.
    VkUnique<VkDescriptorSetLayout> descSetLayout_;
    VkUnique<VkDescriptorPool> descPool_;
    VkDescriptorSet descSet_ = VK_NULL_HANDLE;
    VkUnique<VkPipelineLayout> pipelineLayout_;
    VkUnique<VkPipeline> pipeline_;

    std::string shaderDir_;
    int   tonemapMode_ = 0;   // Phase 1H-4
    float exposure_ = 1.0f;
};
