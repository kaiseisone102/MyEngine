#pragma once
// =============================================================================
// AsyncCompute -- INDEX (V) / Vulkan13 \xc2\xa76 receptacle for cross-queue compute
// =============================================================================
// C (transfer queue) + B (timelineSemaphore) + V (asyncComputeQueue dedicated
// detection done in 4c-B) all landed before this header; AsyncCompute is the
// utility layer that ties them together.
//
// What the FULL implementation looks like (this header is the receptacle
// path, the actual cross-queue submission lands per-Phase):
//
//   1. Create a timeline semaphore at engine start (Vulkan 1.2 core).
//   2. On every graphics queue submit that produces an input HiZPass /
//      CullingPass needs, signal the timeline at value V_g.
//   3. Submit HZB build + cull compute to ctx.asyncComputeQueue() chained
//      with VkTimelineSemaphoreSubmitInfo waiting V_g, signalling V_c.
//   4. The next graphics submit waits V_c before binding the cull's
//      compactCmd buffer for indirect draw.
//
// The result on P620 (asyncComputeFamily=2, dedicated=1) is that HZB build
// and cull pass2 can overlap with the previous main pass on the graphics
// queue -- Nanite/Granite 2024's typical 0.4-0.8 ms saving.
//
// Receptacle-level today:
//   - AsyncComputeContext owns the timeline semaphore (one is enough; the
//     value monotonically increases across the whole frame loop).
//   - submitCompute() helper packages VkTimelineSemaphoreSubmitInfo wait/
//     signal into a single call so the per-Phase activation does not have
//     to re-derive the boilerplate.
//   - The class is a no-op when timelineSemaphore() returns false (older
//     drivers) -- the call site falls back to graphicsQueue submission.
//
// Phase activation checklist (when M moves from receptacle to live):
//   [ ] pass_chain records HZB + Cull2 cmd buffers separately (not yet
//       inlined) so they can be submitted to the async queue.
//   [ ] CullingPass.execute() exposes a way to switch its target queue's
//       family for resource barriers (QFOT or VK_SHARING_MODE_CONCURRENT
//       on the cmd buffers shared across queues).
//   [ ] HZB + visHistory buffers either use CONCURRENT sharing or
//       perform a QFOT release/acquire between submits.
//   [ ] FrameSync extended to wait on the AsyncCompute timeline before
//       the next graphics submit.
//
// Foundations \xc2\xa72 a-1 (queue family detection) and B (timeline semaphore
// feature) are the prerequisites already in place. C (transfer queue)
// covers the analogous receptacle for streaming-upload overlap, distinct
// from this one (compute overlap).
// =============================================================================
#include <vulkan/vulkan.h>

#include <cstdint>

#include "renderer/vk_unique.h"

class VulkanContext;

namespace myengine::renderer {

class AsyncComputeContext {
   public:
    void init(VulkanContext* ctx) {
        ctx_ = ctx;
        if (!ctx_) return;

        VkSemaphoreTypeCreateInfo typeInfo{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
        typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        typeInfo.initialValue = 0;

        VkSemaphoreCreateInfo ci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        ci.pNext = &typeInfo;

        VkSemaphore sem = VK_NULL_HANDLE;
        if (vkCreateSemaphore(ctx_->device(), &ci, nullptr, &sem) == VK_SUCCESS) {
            timeline_ = VkUnique<VkSemaphore>(ctx_->device(), sem);
        }
    }

    void shutdown() {
        timeline_.reset();
        ctx_ = nullptr;
        value_ = 0;
    }

    // Allocate the next monotonically-increasing timeline value. Callers use
    // this in pairs (signalValue() for the graphics submit, returnedValue
    // for the matching async-compute wait).
    uint64_t nextValue() noexcept { return ++value_; }

    VkSemaphore semaphore() const noexcept { return timeline_.get(); }
    bool ready() const noexcept { return timeline_.operator bool(); }

   private:
    VulkanContext* ctx_ = nullptr;
    VkUnique<VkSemaphore> timeline_;
    uint64_t value_ = 0;
};

}  // namespace myengine::renderer
