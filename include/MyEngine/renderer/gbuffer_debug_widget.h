// include/MyEngine/renderer/gbuffer_debug_widget.h
#pragma once
// =============================================================================
// gbuffer_debug_widget.h — PART4 4a-2 debug viewer for the GBuffer attachments
// =============================================================================
//
// Responsibility:
//   - Wraps the ImGui Vulkan backend's "draw a Vulkan image into an ImGui
//     window" affordance for the 4a-2 prepass attachments (normal / motion
//     / depth). HDR is intentionally NOT previewed: it is bound as the
//     OverlayPass color attachment at the same time ImGui is drawing into
//     it, so sampling it from within OverlayPass is a feedback loop
//     (would require VK_EXT_attachment_feedback_loop_layout). HDR is also
//     visible on-screen, which makes the viewer redundant for it.
//
// Lifecycle:
//   init()            — owns a shared linear sampler.
//   setAttachments()  — called on init and after every swapchain resize;
//                       re-registers ImGui descriptor sets against the new
//                       VkImageView handles.
//   draw()            — call between ImGui::NewFrame and ImGui::Render;
//                       opens a window in the top-right corner with one
//                       ImGui::Image per attachment.
//   shutdown()        — releases ImGui descriptors (before the ImGui
//                       backend tears down its pool) and the sampler.
//
// Why a separate class:
//   - Aligns with the rest of the renderer (HudPass / MainPass / ShadowPass
//     / OverlayPass are all classes).
//   - Confines the lazy / dirty-rebuild plumbing to one place.
//   - Lets PassChain just forward views + drive draw(), without the viewer
//     state polluting the orchestrator.
// =============================================================================

#include <vulkan/vulkan.h>

#include "renderer/vk_unique.h"

class VulkanContext;

class GBufferDebugWidget {
   public:
    void init(VulkanContext* ctx);
    void shutdown();

    // Re-register the underlying ImGui texture descriptors against new views.
    // Call from PassChain::init and onSwapchainResized; the descriptors are
    // rebuilt lazily on the next draw() so callers don't have to worry about
    // in-flight frames.
    void setAttachments(VkImageView normalView, VkImageView motionView,
                         VkImageView depthView);

    // Records the ImGui draw list for the GBuffer viewer window. Must run
    // between ImGui::NewFrame and ImGui::Render; the actual GPU sampling
    // happens later, inside OverlayPass, by which time the underlying
    // images are in the read-only layouts the widget expects.
    void draw();

    void setOpen(bool on) { open_ = on; }
    bool isOpen() const { return open_; }

   private:
    VulkanContext* ctx_ = nullptr;
    VkUnique<VkSampler> sampler_;

    VkImageView normalView_ = VK_NULL_HANDLE;
    VkImageView motionView_ = VK_NULL_HANDLE;
    VkImageView depthView_ = VK_NULL_HANDLE;

    VkDescriptorSet normalId_ = VK_NULL_HANDLE;
    VkDescriptorSet motionId_ = VK_NULL_HANDLE;
    VkDescriptorSet depthId_ = VK_NULL_HANDLE;

    bool dirty_ = true;
    bool open_ = true;
};
