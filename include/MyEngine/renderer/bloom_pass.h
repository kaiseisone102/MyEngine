// include/MyEngine/renderer/bloom_pass.h
#pragma once
// =============================================================================
// bloom_pass.h - Phase 1I: HDR bloom (minimal: bright extract + separable blur)
//
// Inputs:
//   - HDR scene color (R16G16B16A16_SFLOAT, sampled image) from MainPass
//   - Two half-resolution ping-pong bloom targets (owned by VulkanRenderer)
//
// Output:
//   - Bloom texture in bloomTargetA (the result PostPass samples & composites)
//
// Passes (all fullscreen-triangle, no vertex buffer):
//   1. bright : HDR        -> targetA   (extract bright regions, soft-knee)
//   2. blur H : targetA    -> targetB   (horizontal 9-tap Gaussian)
//   3. blur V : targetB    -> targetA   (vertical   9-tap Gaussian)
//
// The two bloom targets share one render pass (same format / load-store).
// Two pipelines: bright (bloom_bright.frag) and blur (bloom_blur.frag).
// Three descriptor sets, each binding the source texture of that draw.
// =============================================================================
#include <vulkan/vulkan.h>
#include <string>

class VulkanContext;

class BloomPass {
   public:
    struct InitInfo {
        VulkanContext* ctx = nullptr;
        // HDR scene color (source for the bright pass).
        VkImageView hdrColorView = VK_NULL_HANDLE;
        VkSampler hdrColorSampler = VK_NULL_HANDLE;
        // Two ping-pong bloom targets (half-res), owned by VulkanRenderer.
        VkImageView targetAView = VK_NULL_HANDLE;
        VkSampler targetASampler = VK_NULL_HANDLE;
        VkImageView targetBView = VK_NULL_HANDLE;
        VkSampler targetBSampler = VK_NULL_HANDLE;
        VkFormat bloomFormat = VK_FORMAT_UNDEFINED;
        uint32_t width = 0;   // bloom target extent (half of swapchain)
        uint32_t height = 0;
        std::string shaderDir;
    };
    struct ExecuteInfo {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
    };

    void init(const InitInfo& info);
    void shutdown();

    // Called when swapchain is recreated; pass the new views + new extent.
    void onSwapchainResized(const InitInfo& info);

    // Records the 3 bloom draws into cmd. After this, targetA holds the bloom.
    void execute(const ExecuteInfo& info);

    // Tuning (pushed as constants each draw).
    void setThreshold(float t) { threshold_ = t; }
    void setSoftKnee(float k) { softKnee_ = k; }
    float threshold() const { return threshold_; }
    float softKnee() const { return softKnee_; }

   private:
    void createRenderPass();
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void allocateDescriptorSets();
    void updateDescriptorSets();
    void createPipelineLayout();
    void createPipelines(const std::string& shaderDir);
    void createFramebuffers();
    void destroyFramebuffers();
    void destroyPipelines();

    // push constant block matching bloom_bright/bloom_blur.frag
    struct PushConstants {
        float threshold;
        float softKnee;
        float intensity;
        float texelDir;  // 0 = horizontal, 1 = vertical
    };

    void recordDraw(VkCommandBuffer cmd, VkFramebuffer fb, VkPipeline pipe,
                    VkDescriptorSet set, const PushConstants& pc);

    VulkanContext* ctx_ = nullptr;

    // Cached resources (not owned).
    VkImageView hdrColorView_ = VK_NULL_HANDLE;
    VkSampler hdrColorSampler_ = VK_NULL_HANDLE;
    VkImageView targetAView_ = VK_NULL_HANDLE;
    VkSampler targetASampler_ = VK_NULL_HANDLE;
    VkImageView targetBView_ = VK_NULL_HANDLE;
    VkSampler targetBSampler_ = VK_NULL_HANDLE;
    VkFormat bloomFormat_ = VK_FORMAT_UNDEFINED;
    uint32_t width_ = 0;
    uint32_t height_ = 0;

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descPool_ = VK_NULL_HANDLE;
    // 0 = sample HDR, 1 = sample targetA, 2 = sample targetB
    VkDescriptorSet descHdr_ = VK_NULL_HANDLE;
    VkDescriptorSet descA_ = VK_NULL_HANDLE;
    VkDescriptorSet descB_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipelineBright_ = VK_NULL_HANDLE;
    VkPipeline pipelineBlur_ = VK_NULL_HANDLE;
    VkFramebuffer fbA_ = VK_NULL_HANDLE;  // renders into targetA
    VkFramebuffer fbB_ = VK_NULL_HANDLE;  // renders into targetB

    std::string shaderDir_;
    float threshold_ = 1.0f;
    float softKnee_ = 0.5f;
};
