// include/MyEngine/renderer/deletion_queue.h
#pragma once
// =============================================================================
// deletion_queue.h - frameIndex-ringed deferred destruction.
// =============================================================================
// GPU-driven engines constantly swap resources (streaming chunk load/unload,
// growing megabuffers, swapchain rebuilds). A resource that is still referenced
// by an in-flight command buffer MUST NOT be destroyed immediately. This queue
// defers destruction until the frame that could still be using the resource has
// completed on the GPU.
//
// HOW IT IS SAFE (ties into FrameSync):
//   FrameSync has MAX_FRAMES_IN_FLIGHT (=2) per-frame fences. acquireNextImage
//   waits on the fence for the frame it is about to record, which means every
//   GPU command previously submitted under that SAME frameIndex has completed.
//   So: enqueue() puts the deleter into the bucket of the CURRENT frameIndex;
//   when that same frameIndex comes around again and its fence has been waited
//   on, collectFrame(frameIndex) is called (right after acquire) and the bucket
//   is safe to flush. Worst-case lifetime extension = MAX_FRAMES_IN_FLIGHT frames.
//
// USAGE (owned by VulkanRenderer):
//   - drawFrame, right after a successful acquireNextImage:
//         deletionQueue_.collectFrame(acq.frameIndex);   // free last cycle's bucket
//   - anywhere a resource is retired this frame:
//         deletionQueue_.enqueueBuffer(buf, alloc);       // VMA buffer
//         deletionQueue_.enqueue([=]{ ... });             // arbitrary cleanup
//   - after vkDeviceWaitIdle (shutdown / recreateSwapchain): flushAll().
//
// This queue does NOT own the device/allocator; it borrows them from the
// VulkanContext passed at init and only calls vmaDestroy*/vkDestroy* on flush.
// =============================================================================
#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

// VMA forward decls (same style as the rest of the renderer headers).
VK_DEFINE_HANDLE(VmaAllocation)
VK_DEFINE_HANDLE(VmaAllocator)

class VulkanContext;

class DeletionQueue {
   public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;  // must match FrameSync

    void init(VulkanContext* ctx);
    void shutdown();  // flushes everything (call after vkDeviceWaitIdle)

    // Schedule an arbitrary cleanup to run once the current frame's GPU work has
    // completed (i.e. when this frameIndex's bucket is next collected).
    void enqueue(std::function<void()> deleter);

    // Convenience wrappers for the two most common resources.
    void enqueueBuffer(VkBuffer buffer, VmaAllocation allocation);
    void enqueueImage(VkImage image, VmaAllocation allocation);

    // Run (and clear) the bucket for this frameIndex. Call right after the frame
    // fence has been waited on (acquireNextImage), so the bucket's resources are
    // guaranteed no longer in use by the GPU.
    void collectFrame(uint32_t frameIndex);

    // Run (and clear) ALL buckets immediately. Only valid when the GPU is idle
    // (after vkDeviceWaitIdle): used by shutdown and swapchain recreation.
    void flushAll();

    // Which frameIndex new enqueues are charged to. VulkanRenderer sets this to
    // the frame it is currently recording so deleters land in the right bucket.
    void setCurrentFrame(uint32_t frameIndex) {
        if (frameIndex < MAX_FRAMES_IN_FLIGHT) current_ = frameIndex;
    }

   private:
    VulkanContext* ctx_ = nullptr;
    VmaAllocator allocator_ = VK_NULL_HANDLE;
    uint32_t current_ = 0;
    std::array<std::vector<std::function<void()>>, MAX_FRAMES_IN_FLIGHT> buckets_{};

    void flushBucket(uint32_t frameIndex);
};