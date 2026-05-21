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

// VMA forward declarations
VK_DEFINE_HANDLE(VmaAllocation)

#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

#include "frame_sync.h"

class VulkanContext;
class ResourceFactory;

class InstanceBufferPool {
   public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = FrameSync::MAX_FRAMES_IN_FLIGHT;
    // Total instance matrices per frame across all instanced draws.
    static constexpr uint32_t MAX_INSTANCES = 8192;

    void init(VulkanContext* ctx, ResourceFactory* resources);
    void shutdown();

    // Reset the linear write cursor for a new frame.
    void beginFrame(uint32_t frameIndex);

    // Append a block of matrices to the current frame's buffer.
    // Returns the starting instance offset (use as base for gl_InstanceIndex),
    // or UINT32_MAX if the pool is full.
    uint32_t push(uint32_t frameIndex, const std::vector<glm::mat4>& matrices);

    // BDA: shaders cast this address to a typed pointer.
    VkDeviceAddress bufferAddress(uint32_t frameIndex) const {
        return (frameIndex < MAX_FRAMES_IN_FLIGHT) ? addresses_[frameIndex] : 0;
    }

    uint32_t usedThisFrame(uint32_t frameIndex) const {
        return (frameIndex < MAX_FRAMES_IN_FLIGHT) ? cursor_[frameIndex] : 0;
    }

   private:
    VulkanContext* ctx_ = nullptr;
    VkDeviceSize bufferSize_ = 0;

    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> buffers_{};
    std::array<VmaAllocation, MAX_FRAMES_IN_FLIGHT> allocations_{};
    std::array<VkDeviceAddress, MAX_FRAMES_IN_FLIGHT> addresses_{};
    std::array<void*, MAX_FRAMES_IN_FLIGHT> mapped_{};
    // Linear write cursor (in matrix units), reset each frame.
    std::array<uint32_t, MAX_FRAMES_IN_FLIGHT> cursor_{};

    void createBuffers(ResourceFactory* resources);
};
