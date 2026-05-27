// include/MyEngine/renderer/draw_data_pool.h
#pragma once
// =============================================================================
// draw_data_pool.h - Phase 2B PART3b: BDA-only per-draw data pool (static draws)
// =============================================================================
// One large SSBO per frame holding DrawData (model + materialId + alpha) for
// non-skinned static draws (cube mesh / static models / terrain). Mirrors
// InstanceBufferPool (linear allocator, persistently mapped, BDA, no
// descriptors); the only differences are the element type (DrawData) and that
// draws push ONE element at a time and use the returned slot as gl_InstanceIndex
// via vkCmdDrawIndexed's firstInstance.
//
// Header-only: it uses only VmaBuffer's public API (no direct VMA calls), so
// there is no separate translation unit / CMakeLists entry to add.
//
// Capacity note (Work_Protocol 5b): MAX_DRAWS is a FIXED cap matching
// CullingPass::MAX_DRAWS (one DrawData per indirect command slot). Per the
// development-first principle this whole per-frame draw-count cap (cull cmds +
// this pool) should grow dynamically; tracked as shared debt, not done here.
// =============================================================================
#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#include "renderer/vma_buffer.h"
#include "renderer/vulkan_context.h"
#include "renderer/frame_sync.h"
#include "renderer/culling_pass.h"   // MAX_DRAWS (shared per-frame draw cap)
#include "shaders/shared/types.h"

class DrawDataPool {
   public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = FrameSync::MAX_FRAMES_IN_FLIGHT;
    static constexpr uint32_t MAX_DRAWS = CullingPass::MAX_DRAWS;

    void init(VulkanContext* ctx) {
        if (!ctx) throw std::runtime_error("DrawDataPool::init: invalid ctx");
        ctx_ = ctx;
        const VkDeviceSize sz =
            static_cast<VkDeviceSize>(MAX_DRAWS) * sizeof(myengine::shared::DrawData);
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            buffers_[i] = VmaBuffer::createMappedStorageBDA(ctx_, sz);
            cursor_[i] = 0;
        }
    }

    void shutdown() {
        if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            buffers_[i].reset();
            cursor_[i] = 0;
        }
        ctx_ = nullptr;
    }

    void beginFrame(uint32_t frameIndex) {
        if (frameIndex >= MAX_FRAMES_IN_FLIGHT) return;
        cursor_[frameIndex] = 0;
    }

    // Append one DrawData; returns its slot index (use as firstInstance), or
    // UINT32_MAX if the pool is full.
    uint32_t pushOne(uint32_t frameIndex, const myengine::shared::DrawData& d) {
        if (frameIndex >= MAX_FRAMES_IN_FLIGHT) return UINT32_MAX;
        const uint32_t slot = cursor_[frameIndex];
        if (slot >= MAX_DRAWS) return UINT32_MAX;
        uint8_t* dst = static_cast<uint8_t*>(buffers_[frameIndex].mapped()) +
                       static_cast<size_t>(slot) * sizeof(myengine::shared::DrawData);
        std::memcpy(dst, &d, sizeof(myengine::shared::DrawData));
        cursor_[frameIndex] = slot + 1;
        return slot;
    }

    VkDeviceAddress bufferAddress(uint32_t frameIndex) const {
        return (frameIndex < MAX_FRAMES_IN_FLIGHT) ? buffers_[frameIndex].deviceAddress() : 0;
    }

    uint32_t usedThisFrame(uint32_t frameIndex) const {
        return (frameIndex < MAX_FRAMES_IN_FLIGHT) ? cursor_[frameIndex] : 0;
    }

   private:
    VulkanContext* ctx_ = nullptr;
    std::array<VmaBuffer, MAX_FRAMES_IN_FLIGHT> buffers_{};
    std::array<uint32_t, MAX_FRAMES_IN_FLIGHT> cursor_{};
};