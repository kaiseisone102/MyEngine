// include/MyEngine/renderer/hzb_debug_widget.h
#pragma once
// =============================================================================
// hzb_debug_widget.h - PART4 4b debug viewer for the Hi-Z pyramid
// =============================================================================
//
// Companion to gbuffer_debug_widget. Where that one shows the depth/normal/
// motion prepass outputs, this one shows the SPD pyramid that HiZPass
// produces from depth. A slider picks the mip level; the two RG32F channels
// (.r = min, .g = max) render side by side via ImGui::Image so the user can
// visually verify the reduction is correct (mip(n+1) should be the
// conservative reduction of mip(n)).
//
// Lifecycle is identical to GBufferDebugWidget:
//   init()            - owns a NEAREST sampler (we want to see exact texels).
//   setPyramid()      - call after HiZPass::init / onSwapchainResized; the
//                       widget caches per-mip views and lazily re-registers
//                       ImGui descriptors.
//   draw()            - between ImGui::NewFrame and ImGui::Render.
//   shutdown()        - releases the ImGui descriptors first, then sampler.
//
// Image layout note:
//   HiZPass leaves the pyramid in VK_IMAGE_LAYOUT_GENERAL (compute-friendly,
//   no transition between frames). ImGui's fragment shader can sample from
//   GENERAL just fine; we pass it as the AddTexture layout argument.
// =============================================================================

#include <vulkan/vulkan.h>

#include <vector>

#include "renderer/vk_unique.h"

class VulkanContext;
class HiZPass;

class HzbDebugWidget {
   public:
    void init(VulkanContext* ctx);
    void shutdown();

    // Re-cache the per-mip storage views and mip count. The widget holds
    // pointers to the views; HiZPass owns their lifetime, so we must call
    // this after HiZPass::init() and again after onSwapchainResized().
    void setPyramid(const HiZPass* hizPass);

    void draw();

    void setOpen(bool on) { open_ = on; }
    bool isOpen() const { return open_; }

   private:
    void releaseImGuiDescriptors();

    VulkanContext* ctx_ = nullptr;
    VkUnique<VkSampler> sampler_;  // NEAREST, no mipmap (we sample explicit mip views)

    // Cached per-frame, per-mip storage image views (HiZPass owns).
    // [frameIndex][mipIndex] -> view
    std::vector<std::vector<VkImageView>> perFrameMipViews_;
    std::vector<std::vector<VkDescriptorSet>> perFrameMipDescs_;

    uint32_t selectedFrame_ = 0;
    uint32_t selectedMip_ = 0;
    bool dirty_ = true;
    bool open_ = true;
};
