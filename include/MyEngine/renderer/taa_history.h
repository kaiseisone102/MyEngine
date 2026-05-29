#pragma once
// =============================================================================
// TaaHistory -- R: TAA / TSR / DLSS / FSR history buffer receptacle
// =============================================================================
// PART4 4a-2 (commit ed0d80e) added the motion vector RT to the MainPass MRT
// layout; that closes one half of every temporal-AA family the engine might
// land. The other half is a per-frame history buffer of the previous-frame
// HDR target that the temporal pass reprojects with the motion RT and
// blends against the current frame.
//
// Receptacle shape:
//   - history_[2] : VmaImage ping-pong (HDR format, swapchain extent)
//   - readIndex() / writeIndex() : alternates each frame so the previous
//     frame's write is this frame's read.
//   - swap() called once per drawFrame to advance the cycle.
//
// All temporal techniques share this exact ping-pong primitive:
//   - TAA  (Phase 2D)               : jitter VP, reproject + blend (Karis 14)
//   - TSR / FSR / DLSS              : same shape, fancier reproject filter
//   - SSGI / SSR temporal denoise   : reproject the screen-space probe
//   - SSAO temporal smoothing       : same again
//
// Bookkeeping today is intentionally tiny so it can be activated by any of
// the above without re-architecting the renderer. The Phase that turns
// on jittering ("Phase 2D") fills in the format from VulkanRenderer's HDR
// target and recreates on swapchain resize.
// =============================================================================
#include "renderer/vma_image.h"

#include <array>
#include <cstdint>

class VulkanContext;

namespace myengine::renderer {

class TaaHistory {
   public:
    void init(VulkanContext* ctx, uint32_t width, uint32_t height, VkFormat hdrFormat) {
        // Phase: allocate history_[0/1] via VmaImage::createAttachment.
        (void)ctx; (void)width; (void)height; (void)hdrFormat;
    }
    void shutdown() {
        history_[0].reset();
        history_[1].reset();
    }
    void onSwapchainResized(uint32_t /*width*/, uint32_t /*height*/) {}

    uint32_t readIndex() const noexcept { return frame_ & 1u; }
    uint32_t writeIndex() const noexcept { return (frame_ + 1u) & 1u; }
    void swap() noexcept { ++frame_; }

   private:
    std::array<VmaImage, 2> history_{};
    uint32_t frame_ = 0;
};

}  // namespace myengine::renderer
