// include/MyEngine/renderer/bloom_pass.h
#pragma once
// =============================================================================
// bloom_pass.h - Phase 1I: compute-based mip-chain bloom (Jimenez / CoD:AW).
//
// First compute pass in the engine. No render pass, no framebuffer. Each mip is
// a storage+sampled VmaImage; down/upsampling is done with vkCmdDispatch and
// imageLoad/imageStore. The renderer only feeds the HDR scene color in and reads
// bloomView()/Sampler() (= mip0) back out for PostPass compositing.
//
// Per-frame flow (execute), all compute dispatches:
//   bright    : HDR(sampled)  -> mips[0]              soft-knee extract
//   downsample: mips[i]       -> mips[i+1]  i=0..n-2  (13-tap; Karis on i==0)
//   upsample  : mips[i]       -> mips[i-1]  i=n-1..1  (3x3 tent, += additive)
//   result    : mips[0] holds the final bloom.
// Between every dispatch: COMPUTE->COMPUTE barrier (write->read) on the target.
//
// Maintenance contract (kept deliberately un-scattered):
//   - The mip chain is OWNED here; nothing outside indexes individual mips.
//   - Mip count + each mip extent are computed in ONE place (computeMipSizes()).
//   - execute() binds pre-built sets named by direction (setDown_/setUp_) so the
//     i->i+1 / i->i-1 indexing cannot be misread.
// =============================================================================
#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <vector>

#include "renderer/vma_image.h"
#include "renderer/vk_unique.h"

class VulkanContext;
class ResourceFactory;

class BloomPass {
   public:
    struct InitInfo {
        VulkanContext* ctx = nullptr;
        ResourceFactory* resources = nullptr;  // for one-time layout transitions
        VkImageView hdrColorView = VK_NULL_HANDLE;     // bright-pass input (sampled)
        VkSampler hdrColorSampler = VK_NULL_HANDLE;
        VkFormat bloomFormat = VK_FORMAT_R16G16B16A16_SFLOAT;  // storage-capable HDR fmt
        uint32_t baseWidth = 0;    // mip0 extent (half swapchain recommended)
        uint32_t baseHeight = 0;
        uint32_t maxMips = 6;      // quality knob; clamped to what extent allows
        std::string shaderDir;
    };
    struct ExecuteInfo {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
    };

    void init(const InitInfo& info);
    void shutdown();
    void onSwapchainResized(const InitInfo& info);  // rebuild mips + descriptors
    void execute(const ExecuteInfo& info);

    // Final bloom output (= mip0) for PostPass.
    VkImageView bloomView() const { return mipViews_.empty() ? VK_NULL_HANDLE : mipViews_[0].get(); }
    VkSampler bloomSampler() const { return sampler_.get(); }

    // Tuning (push constants).
    void setThreshold(float t) { threshold_ = t; }
    void setSoftKnee(float k) { softKnee_ = k; }
    void setFilterRadius(float r) { filterRadius_ = r; }

   private:
    struct MipSize { uint32_t w; uint32_t h; };

    struct PushConstants {
        float threshold;
        float softKnee;
        float intensity;
        float param;  // bright: unused; downsample: karis flag; upsample: radius
    };

    std::vector<MipSize> computeMipSizes() const;  // THE single place mip math lives
    void createMipsAndViews();                     // mips_ + mipViews_ + sampler_
    void createDescriptorInfra();                  // set layout + pool
    void allocateAndWriteSets();                   // bright set + per-edge down/up sets
    void createPipelines(const std::string& shaderDir);  // bright/down/up compute
    void destroyMipsAndSets();

    void recordDispatch(VkCommandBuffer cmd, VkPipeline pipe, VkDescriptorSet set,
                        uint32_t dstW, uint32_t dstH, const PushConstants& pc);
    void barrierWriteToRead(VkCommandBuffer cmd, VkImage img);  // COMPUTE->COMPUTE
    void transitionMip0(VkCommandBuffer cmd, VkImageLayout oldL, VkImageLayout newL,
                        VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                        VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);


    VulkanContext* ctx_ = nullptr;
    ResourceFactory* resources_ = nullptr;
    VkImageView hdrColorView_ = VK_NULL_HANDLE;
    VkSampler hdrColorSampler_ = VK_NULL_HANDLE;
    VkFormat bloomFormat_ = VK_FORMAT_R16G16B16A16_SFLOAT;
    uint32_t baseWidth_ = 0;
    uint32_t baseHeight_ = 0;
    uint32_t maxMips_ = 6;
    std::string shaderDir_;

    // Owned mip chain (mip0 = base extent, halving). Each is STORAGE | SAMPLED.
    std::vector<VmaImage> mips_;
    std::vector<VkUnique<VkImageView>> mipViews_;
    VkUnique<VkSampler> sampler_;  // linear clamp, shared by all sampled reads

    // Layout: binding0 = sampled input (sampler2D), binding1 = storage out (image2D).
    VkUnique<VkDescriptorSetLayout> descSetLayout_;
    VkUnique<VkDescriptorPool> descPool_;
    VkDescriptorSet setBright_ = VK_NULL_HANDLE;   // HDR -> mip0
    std::vector<VkDescriptorSet> setDown_;         // setDown_[i]: mip[i]  -> mip[i+1]
    std::vector<VkDescriptorSet> setUp_;           // setUp_[i]:   mip[i+1]-> mip[i] (read i+1, write i)

    VkUnique<VkPipelineLayout> pipelineLayout_;
    VkUnique<VkPipeline> pipeBright_;
    VkUnique<VkPipeline> pipeDownsample_;
    VkUnique<VkPipeline> pipeUpsample_;

    float threshold_ = 1.0f;
    float softKnee_ = 0.5f;
    float filterRadius_ = 1.0f;
};