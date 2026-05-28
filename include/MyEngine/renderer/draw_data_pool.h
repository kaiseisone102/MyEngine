// include/MyEngine/renderer/draw_data_pool.h
#pragma once
// =============================================================================
// draw_data_pool.h - Phase 2B PART3b / PART4 4-前-3: per-draw SSBO pool.
// =============================================================================
// One large SSBO per in-flight frame holding DrawData (model + materialId +
// alpha) for non-skinned static draws (cube mesh / static models / terrain).
// Mirrors InstanceBufferPool (linear allocator, persistently mapped, BDA, no
// descriptors); the only differences are the element type (DrawData) and that
// draws push ONE element at a time and use the returned slot as gl_InstanceIndex
// via vkCmdDrawIndexed's firstInstance.
//
// PART4 4-前-3 persistent-capacity layout:
//   * INITIAL_CAPACITY is a STARTING size, not a hard cap. ensureCapacity()
//     grows every in-flight ring slot and hands the old buffers to the
//     DeletionQueue so they are freed safely after MAX_FRAMES_IN_FLIGHT frames.
//   * The buffers are still **per-frame host-mapped ring**, NOT a single
//     device-local + staging pattern. The reason is access shape:
//     reflection_pass and main_pass are BOTH same-frame GPU consumers reading
//     DrawData via BDA, and reflection's draw commands are recorded BEFORE the
//     static-cull build finishes pushing main's data. A device-local +
//     vkCmdCopyBuffer flush would have to flush BEFORE reflection records, but
//     main's data is not pushed yet at that point; flushing twice or splitting
//     addresses is messy. Host-mapped with per-frame slot fences out cross-frame
//     races without that complexity, and is the pattern modern engines use for
//     transient per-frame data.
//   * The persistent / forward-compat win is that pushOne returns ABSOLUTE
//     slots (cursor across reflection + main is a single sequence) and grow
//     decouples scene scale from a fixed cap.
//
// Header-only: it uses only VmaBuffer's public API.
// =============================================================================
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#include "renderer/deletion_queue.h"
#include "renderer/frame_sync.h"
#include "renderer/vma_buffer.h"
#include "renderer/vulkan_context.h"
#include "renderer/culling_pass.h"   // INITIAL_CAPACITY (shared per-frame draw cap)
#include "shaders/shared/types.h"

class DrawDataPool {
   public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = FrameSync::MAX_FRAMES_IN_FLIGHT;
    static constexpr uint32_t INITIAL_CAPACITY = CullingPass::INITIAL_CAPACITY;

    void init(VulkanContext* ctx, DeletionQueue* deletionQueue) {
        if (!ctx) throw std::runtime_error("DrawDataPool::init: invalid ctx");
        if (!deletionQueue)
            throw std::runtime_error("DrawDataPool::init: deletionQueue is null (PART4 4-前-3)");
        ctx_ = ctx;
        dq_  = deletionQueue;
        createBuffers(INITIAL_CAPACITY);
    }

    void shutdown() {
        if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            buffers_[i].reset();
            cursor_[i] = 0;
        }
        capacity_ = 0;
        ctx_ = nullptr;
        dq_ = nullptr;
    }

    void beginFrame(uint32_t frameIndex) {
        if (frameIndex >= MAX_FRAMES_IN_FLIGHT) return;
        cursor_[frameIndex] = 0;
    }

    // Append one DrawData; returns its slot index (use as firstInstance), or
    // UINT32_MAX if the frameIndex is invalid. The pool grows automatically if
    // the cursor would exceed capacity.
    uint32_t pushOne(uint32_t frameIndex, const myengine::shared::DrawData& d) {
        if (frameIndex >= MAX_FRAMES_IN_FLIGHT) return UINT32_MAX;
        const uint32_t slot = cursor_[frameIndex];
        if (slot >= capacity_) ensureCapacity(slot + 1u);
        uint8_t* dst = static_cast<uint8_t*>(buffers_[frameIndex].mapped()) +
                       static_cast<size_t>(slot) * sizeof(myengine::shared::DrawData);
        std::memcpy(dst, &d, sizeof(myengine::shared::DrawData));
        cursor_[frameIndex] = slot + 1;
        return slot;
    }

    // PART4 4-前-3: grow every in-flight ring slot to at least `need`. Old
    // buffers are deferred-freed via the DeletionQueue, BUT any data already
    // written this frame is first memcpy'd into the new slot so a mid-frame
    // pushOne overflow doesn't lose reflection's already-pushed entries.
    // (reflection records its draws referencing the OLD bufferAddress; main
    // records using the NEW; both buffers stay alive through the DeletionQueue
    // until they're safely past in-flight reads, so the OLD address remains
    // valid for reflection's commands while the NEW one carries the full
    // updated dataset for main.)
    void ensureCapacity(uint32_t need) {
        if (need <= capacity_) return;
        const uint32_t newCap = std::max(need, capacity_ * 2u);
        std::array<VmaBuffer, MAX_FRAMES_IN_FLIGHT> oldBufs;
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
            oldBufs[i] = std::move(buffers_[i]);

        createBuffers(newCap);  // new empty buffers in buffers_[]

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (oldBufs[i] && cursor_[i] > 0 && oldBufs[i].mapped() && buffers_[i].mapped()) {
                std::memcpy(buffers_[i].mapped(), oldBufs[i].mapped(),
                            static_cast<size_t>(cursor_[i]) * sizeof(myengine::shared::DrawData));
            }
            if (oldBufs[i]) {
                dq_->enqueueBuffer(oldBufs[i].buffer(), oldBufs[i].allocation());
                oldBufs[i].release();
            }
        }
    }

    VkDeviceAddress bufferAddress(uint32_t frameIndex) const {
        return (frameIndex < MAX_FRAMES_IN_FLIGHT) ? buffers_[frameIndex].deviceAddress() : 0;
    }

    uint32_t usedThisFrame(uint32_t frameIndex) const {
        return (frameIndex < MAX_FRAMES_IN_FLIGHT) ? cursor_[frameIndex] : 0;
    }

    uint32_t capacity() const noexcept { return capacity_; }

   private:
    void createBuffers(uint32_t capacity) {
        capacity_ = capacity;
        const VkDeviceSize sz =
            static_cast<VkDeviceSize>(capacity) * sizeof(myengine::shared::DrawData);
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            buffers_[i] = VmaBuffer::createMappedStorageBDA(ctx_, sz);
            cursor_[i] = 0;
        }
    }

    VulkanContext* ctx_ = nullptr;
    DeletionQueue* dq_ = nullptr;
    uint32_t capacity_ = 0;
    std::array<VmaBuffer, MAX_FRAMES_IN_FLIGHT> buffers_{};
    std::array<uint32_t, MAX_FRAMES_IN_FLIGHT> cursor_{};
};
