#pragma once
// =============================================================================
// SkinInstancePool - Phase 2G: per-frame host-mapped SkinInstance[] table.
// =============================================================================
// The batched compute skinning dispatch (skinning.comp) reads ONE SkinInstance[]
// SSBO that describes every skinned submesh to skin this frame. The CPU builds
// it each frame (one entry per skinned SubMesh), so it is a per-frame
// host-mapped + BDA ring, exactly like DrawDataPool / InstanceBufferPool (linear
// allocator, persistently mapped, no descriptors). SkinningPass owns one.
//
// Distinct from SkinnedVertexPool (the device-local OUTPUT streams the compute
// pass writes): this is the small CPU-written INPUT table the compute pass reads.
//
// Capacity is a STARTING size, not a cap (Foundations 8.1 / Work_Protocol 5f):
// push() grows the ring via the DeletionQueue when it overflows.
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
#include "shaders/shared/types.h"

class SkinInstancePool {
   public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = FrameSync::MAX_FRAMES_IN_FLIGHT;
    // Starting instance capacity (skinned submeshes per frame). Grows on demand.
    static constexpr uint32_t INITIAL_CAPACITY = 256;

    void init(VulkanContext* ctx, DeletionQueue* deletionQueue) {
        if (!ctx) throw std::runtime_error("SkinInstancePool::init: invalid ctx");
        if (!deletionQueue)
            throw std::runtime_error("SkinInstancePool::init: deletionQueue is null");
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

    // Append one SkinInstance; returns its index, or UINT32_MAX on invalid
    // frame. Grows automatically.
    uint32_t push(uint32_t frameIndex, const myengine::shared::SkinInstance& si) {
        if (frameIndex >= MAX_FRAMES_IN_FLIGHT) return UINT32_MAX;
        const uint32_t slot = cursor_[frameIndex];
        if (slot >= capacity_) ensureCapacity(slot + 1u);
        uint8_t* dst = static_cast<uint8_t*>(buffers_[frameIndex].mapped()) +
                       static_cast<size_t>(slot) * sizeof(myengine::shared::SkinInstance);
        std::memcpy(dst, &si, sizeof(myengine::shared::SkinInstance));
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
                            static_cast<size_t>(cursor_[i]) * sizeof(myengine::shared::SkinInstance));
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
    uint32_t count(uint32_t frameIndex) const {
        return (frameIndex < MAX_FRAMES_IN_FLIGHT) ? cursor_[frameIndex] : 0;
    }
    uint32_t capacity() const noexcept { return capacity_; }

   private:
    void createBuffers(uint32_t capacity) {
        capacity_ = capacity;
        const VkDeviceSize sz =
            static_cast<VkDeviceSize>(capacity) * sizeof(myengine::shared::SkinInstance);
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
