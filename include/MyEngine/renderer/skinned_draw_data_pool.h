#pragma once
// =============================================================================
// skinned_draw_data_pool.h - Phase 2G: per-frame SkinnedDrawData SSBO pool.
// =============================================================================
// Mirrors DrawDataPool (prop / PART3b) but for compute-skinned submeshes: one
// SkinnedDrawData per skinned submesh per frame (model / materialId / alpha +
// the dst/src vertex bases the passthrough vertex shader needs to pull skinned
// vertices). Per-frame host-mapped ring + BDA, no descriptors.
//
// Unlike the static DrawDataPool (whose cursor is shared reflection->main in a
// single sequence), the skinned data is built ONCE per frame in pass_chain's
// 2G block and read by shadow + main + reflection alike ("skin once"): all
// three passes index the same SkinnedDrawData[] by the slot baked into each
// PreparedSkinnedDraw, so there is no cross-pass cursor coupling.
//
// A separate typed pool (not folded into DrawData) keeps slot spaces
// independent and avoids overloading DrawData's padding with skinned-only
// fields. The unified GPUScene-style object buffer that merges DrawDataPool +
// SkinnedDrawDataPool + InstanceBufferPool is the Phase 2F persistent object
// buffer (H receptacle); doing it here would be redone there.
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
#include "renderer/culling_pass.h"  // INITIAL_CAPACITY (shared per-frame draw cap)
#include "shaders/shared/types.h"

class SkinnedDrawDataPool {
   public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = FrameSync::MAX_FRAMES_IN_FLIGHT;
    static constexpr uint32_t INITIAL_CAPACITY = CullingPass::INITIAL_CAPACITY;

    void init(VulkanContext* ctx, DeletionQueue* deletionQueue) {
        if (!ctx) throw std::runtime_error("SkinnedDrawDataPool::init: invalid ctx");
        if (!deletionQueue)
            throw std::runtime_error("SkinnedDrawDataPool::init: deletionQueue is null");
        ctx_ = ctx;
        dq_ = deletionQueue;
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

    // Append one SkinnedDrawData; returns its slot (use as firstInstance), or
    // UINT32_MAX on invalid frame. Grows automatically.
    uint32_t pushOne(uint32_t frameIndex, const myengine::shared::SkinnedDrawData& d) {
        if (frameIndex >= MAX_FRAMES_IN_FLIGHT) return UINT32_MAX;
        const uint32_t slot = cursor_[frameIndex];
        if (slot >= capacity_) ensureCapacity(slot + 1u);
        uint8_t* dst = static_cast<uint8_t*>(buffers_[frameIndex].mapped()) +
                       static_cast<size_t>(slot) * sizeof(myengine::shared::SkinnedDrawData);
        std::memcpy(dst, &d, sizeof(myengine::shared::SkinnedDrawData));
        cursor_[frameIndex] = slot + 1;
        return slot;
    }

    void ensureCapacity(uint32_t need) {
        if (need <= capacity_) return;
        const uint32_t newCap = std::max(need, capacity_ * 2u);
        std::array<VmaBuffer, MAX_FRAMES_IN_FLIGHT> oldBufs;
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
            oldBufs[i] = std::move(buffers_[i]);
        createBuffers(newCap);
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (oldBufs[i] && cursor_[i] > 0 && oldBufs[i].mapped() && buffers_[i].mapped()) {
                std::memcpy(buffers_[i].mapped(), oldBufs[i].mapped(),
                            static_cast<size_t>(cursor_[i]) *
                                sizeof(myengine::shared::SkinnedDrawData));
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
            static_cast<VkDeviceSize>(capacity) * sizeof(myengine::shared::SkinnedDrawData);
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
