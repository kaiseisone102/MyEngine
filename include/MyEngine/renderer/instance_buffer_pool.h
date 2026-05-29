// include/MyEngine/renderer/instance_buffer_pool.h
#pragma once
// =============================================================================
// instance_buffer_pool.h - Phase 1E: BDA-only instance matrix pool
// =============================================================================
// One large SSBO per frame, holding instance model matrices.
// Unlike SkinBufferPool (which uses fixed per-entity slots), this is a simple
// LINEAR allocator: each frame, call beginFrame() to reset the write cursor,
// then push() a block of matrices to get its starting offset. The instanced
// draw passes the buffer address (BDA) + offset to the vertex shader, which
// reads matrices[gl_InstanceIndex + offset].
//
// This mirrors the modern BDA style established by SkinBufferPool:
//   - No descriptor set / pool / layout. Pure buffer_reference access.
//   - VMA buffer with SHADER_DEVICE_ADDRESS_BIT, persistently mapped.
//   - bufferAddress(frameIndex) is the sole way to expose the buffer.
// =============================================================================
#include <vulkan/vulkan.h>

#include "renderer/vma_buffer.h"

#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include "shaders/shared/types.h"
#include <vector>

#include "frame_sync.h"

class VulkanContext;
class ResourceFactory;
class DeletionQueue;

class InstanceBufferPool {
   public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = FrameSync::MAX_FRAMES_IN_FLIGHT;
    // Starting capacity (Foundations \xc2\xa78.1 dynamic-growth pattern): when a
    // push() requests more than capacity_ instances, peakRequested_ records the
    // need and the next beginFrame() doubles capacity_, recreates both
    // per-frame buffers at the new size, and hands the old pair to the
    // DeletionQueue so any in-flight cmd buffer that baked their BDA stays
    // valid. Mid-frame overflows are still lost (returning UINT32_MAX) -- we
    // cannot grow a buffer the GPU is currently reading from -- but the next
    // frame fits.
    static constexpr uint32_t INITIAL_CAPACITY = 8192;

    void init(VulkanContext* ctx, ResourceFactory* resources, DeletionQueue* dq);
    void shutdown();

    // Reset the linear write cursor for a new frame.
    void beginFrame(uint32_t frameIndex);

    // Append a block of matrices to the current frame's buffer.
    // Returns the starting instance offset (use as base for gl_InstanceIndex),
    // or UINT32_MAX if the pool is full.
    uint32_t push(uint32_t frameIndex, const std::vector<myengine::shared::InstanceData>& data);

    // BDA: shaders cast this address to a typed pointer.
    VkDeviceAddress bufferAddress(uint32_t frameIndex) const {
        return (frameIndex < MAX_FRAMES_IN_FLIGHT) ? buffers_[frameIndex].deviceAddress() : 0;
    }

    uint32_t usedThisFrame(uint32_t frameIndex) const {
        return (frameIndex < MAX_FRAMES_IN_FLIGHT) ? cursor_[frameIndex] : 0;
    }
    uint32_t capacity() const { return capacity_; }

   private:
    VulkanContext* ctx_ = nullptr;
    DeletionQueue* dq_ = nullptr;
    uint32_t capacity_ = 0;
    uint32_t peakRequested_ = 0;  // tracks the largest needed amount; triggers grow

    std::array<VmaBuffer, MAX_FRAMES_IN_FLIGHT> buffers_{};
    // Linear write cursor (in matrix units), reset each frame.
    std::array<uint32_t, MAX_FRAMES_IN_FLIGHT> cursor_{};

    void allocateBuffers();
    void growToFitPeak();
};
