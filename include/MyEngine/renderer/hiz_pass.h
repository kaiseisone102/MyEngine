// include/MyEngine/renderer/hiz_pass.h
#pragma once
// =============================================================================
// hiz_pass.h — PART4 4b: Hi-Z pyramid generation (SPD-style, single dispatch)
// =============================================================================
//
// Builds a min+max depth pyramid (RG32F, .r = min, .g = max) from the
// main_pass depth attachment in ONE vkCmdDispatch.
//
// Algorithm (AMD FidelityFX SPD-style, hand-rolled):
//   - Workgroup = 256 threads, covers a 64x64 source tile.
//   - Each group writes its tile's mip0..mip5 using LDS for inter-mip storage.
//   - A globally-coherent atomic counter elects the last finishing group.
//   - The last group reads back the per-tile mip5 outputs and continues
//     reducing to produce the remaining mips (mip6..mipN).
//   - mip0 of the OUTPUT is half-resolution of the input depth
//     (= 2x2 source -> 1 output).
//
// Why per-frame 2 pyramids:
//   The swapchain depth is a single image (Swapchain::depthImage_), not
//   per-frame. Frame N's main_pass overwrites N-1's depth. Two-pass occlusion
//   (4c) needs the PREVIOUS frame's pyramid as input, so we own two pyramids
//   (one per MAX_FRAMES_IN_FLIGHT slot) and produce them from this frame's
//   depth at execute time. 4c will read pyramid(prev) and write visibility.
//
// Reverse-Z note:
//   We compute both min and max so cull.comp (4c) can pick the correct
//   channel depending on the depth convention. Under reverse-Z (this engine,
//   4-前-0), occlusion uses the "farthest" stored value, which is .r (min)
//   because depth=0 is far in reverse-Z. .g (max) is reserved for future
//   reflection / particle-sort use (Design §3.3-N).
//
// Maintenance:
//   - Pipeline + descriptor layout are extent-independent; only the pyramids,
//     per-mip storage views, descriptor pool, and sets rebuild on
//     onSwapchainResized().
//   - HiZPass owns the input depth sampler (NEAREST so we read exact depth
//     values for min/max reduction; LINEAR would alias).
//   - The pyramid descriptor binding has descriptorCount = kMaxMips (12); any
//     mip slots beyond mipCount_ are written with the highest real mip view
//     as a safe no-op write target. This keeps the descriptor layout
//     extent-independent.
// =============================================================================

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <vector>

#include "renderer/frame_sync.h"
#include "renderer/vk_unique.h"
#include "renderer/vma_buffer.h"
#include "renderer/vma_image.h"

class VulkanContext;
class ResourceFactory;

class HiZPass {
   public:
    // SPD descriptor-array upper bound. 12 mips covers up to 4096x4096 input.
    // Matches the AMD SPD spec limit and the SPD_MAX_MIP_LEVELS constant in
    // ffx_spd.h. Used for the storage-image array size in the descriptor
    // layout (extent-independent).
    static constexpr uint32_t kMaxMips = 12;
    // Follow the engine-wide frame-in-flight count (FrameSync). The pool of
    // pyramids + atomic counters + descriptor sets is sized by this; if
    // FrameSync changes, every other pass picks it up automatically and
    // HiZPass must too.
    static constexpr uint32_t kMaxFramesInFlight = FrameSync::MAX_FRAMES_IN_FLIGHT;

    struct InitInfo {
        VulkanContext* ctx = nullptr;
        ResourceFactory* resources = nullptr;
        // Input: main_pass depth after the post-pass barrier moves it to
        // DEPTH_READ_ONLY_OPTIMAL (see main_pass.cpp post barrier). The view's
        // aspect is depth-only and stays valid across frames; we only rebuild
        // on swapchain resize.
        VkImageView depthView = VK_NULL_HANDLE;
        VkFormat depthFormat = VK_FORMAT_UNDEFINED;  // for sanity print only
        uint32_t baseWidth = 0;   // input depth width (= swapchain extent)
        uint32_t baseHeight = 0;  // input depth height
        std::string shaderDir;
    };

    struct ExecuteInfo {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        uint32_t frameIndex = 0;
    };

    void init(const InitInfo& info);
    void shutdown();
    void onSwapchainResized(const InitInfo& info);
    void execute(const ExecuteInfo& info);

    // Accessors for downstream consumers (4c cull.comp, 4b debug widget).
    // The sampled view covers the full mip chain; samplers should use a
    // LINEAR_MIPMAP_NEAREST or NEAREST sampler depending on cull semantics.
    VkImageView pyramidView(uint32_t frameIndex) const {
        return (frameIndex < kMaxFramesInFlight) ? frames_[frameIndex].sampledView.get()
                                                  : VK_NULL_HANDLE;
    }
    // Per-mip storage view for the debug widget (ImGui::Image samples one
    // mip at a time). Returns the highest real mip view if mip is
    // out-of-range.
    VkImageView pyramidMipView(uint32_t frameIndex, uint32_t mip) const {
        if (frameIndex >= kMaxFramesInFlight) return VK_NULL_HANDLE;
        const auto& mips = frames_[frameIndex].mipViews;
        if (mips.empty()) return VK_NULL_HANDLE;
        return mips[mip < mips.size() ? mip : mips.size() - 1].get();
    }
    VkSampler pyramidSampler() const { return pyramidSampler_.get(); }
    uint32_t mipCount() const { return mipCount_; }
    uint32_t baseWidth() const { return mip0Width_; }   // mip0 of output (half input)
    uint32_t baseHeight() const { return mip0Height_; }

   private:
    struct PerFrame {
        VmaImage pyramid;                                // RG32F mip chain
        std::vector<VkUnique<VkImageView>> mipViews;     // one storage view per mip
        VkUnique<VkImageView> sampledView;               // full chain for downstream sample
        VmaBuffer atomicCounter;                          // 1 uint, coherent storage buffer
        VkDescriptorSet set = VK_NULL_HANDLE;
        bool pyramidInited = false;  // first execute does UNDEFINED -> GENERAL
    };

    struct PushConstants {
        uint32_t mipsToGenerate;     // == mipCount_
        uint32_t numWorkGroupsTotal; // groupsX * groupsY
        uint32_t inputWidth;         // depth width
        uint32_t inputHeight;        // depth height
    };

    void computeMipLayout();      // mipCount_ + mip0Width_ + mip0Height_
    void createPyramids();        // per-frame VmaImage + views + counter
    void destroyPyramids();
    void createDescriptorInfra(); // set layout + pool (extent-independent)
    void allocateAndWriteSets();
    void createPipeline(const std::string& shaderDir); // picks wave / lds variant
    void destroyPipeline();
    void initialTransitionToGeneral(VkCommandBuffer cmd, uint32_t frameIndex);

    VulkanContext* ctx_ = nullptr;
    ResourceFactory* resources_ = nullptr;
    VkImageView depthView_ = VK_NULL_HANDLE;
    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
    uint32_t inputWidth_ = 0;
    uint32_t inputHeight_ = 0;
    uint32_t mip0Width_ = 0;
    uint32_t mip0Height_ = 0;
    uint32_t mipCount_ = 0;
    uint32_t numWorkGroupsTotal_ = 0;
    std::string shaderDir_;
    bool useWavePath_ = false;

    PerFrame frames_[kMaxFramesInFlight];

    // depth sampler (NEAREST, CLAMP): exact texel reads for min/max reduction.
    VkUnique<VkSampler> depthSampler_;
    // sampler for downstream consumers (4c cull / debug widget).
    VkUnique<VkSampler> pyramidSampler_;

    VkUnique<VkDescriptorSetLayout> setLayout_;
    VkUnique<VkDescriptorPool> descPool_;
    VkUnique<VkPipelineLayout> pipelineLayout_;
    VkUnique<VkPipeline> pipeline_;
};
