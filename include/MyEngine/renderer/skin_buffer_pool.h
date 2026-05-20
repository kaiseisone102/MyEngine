// include/MyEngine/renderer/skin_buffer_pool.h
#pragma once
// =============================================================================
// skin_buffer_pool.h - Phase 1B-4c: BDA-only, descriptor-free
// =============================================================================
// One large SSBO per frame, holding bone matrices for up to MAX_ENTITIES.
// Each entity gets a fixed slot of MAX_BONES_PER_ENTITY mat4s.
//
// Phase 1B-4c (this version):
//   - Descriptor set / pool / layout COMPLETELY REMOVED.
//   - Shaders access this buffer purely via BDA (buffer_reference).
//   - bufferAddress(frameIndex) is the sole way to expose the buffer.
//   - The buffer itself is created via VMA with SHADER_DEVICE_ADDRESS_BIT.
//
// This is the modern Vulkan style: buffers go through BDA pointers,
// descriptors are reserved for textures (and other image-like resources).
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

class SkinBufferPool {
   public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = FrameSync::MAX_FRAMES_IN_FLIGHT;
    static constexpr uint32_t MAX_BONES_PER_ENTITY = 128;
    static constexpr uint32_t MAX_ENTITIES = 128;
    static constexpr uint32_t TOTAL_BONES = MAX_BONES_PER_ENTITY * MAX_ENTITIES;

    struct Slot {
        uint32_t boneOffset = 0;
        uint32_t boneCapacity = 0;
        bool valid() const { return boneCapacity > 0; }
        static Slot invalid() { return Slot{0, 0}; }
    };

    void init(VulkanContext* ctx, ResourceFactory* resources);
    void shutdown();

    Slot allocate();
    void release(Slot slot);
    void update(uint32_t frameIndex, const Slot& slot, const std::vector<glm::mat4>& matrices);

    // BDA: shaders cast this address to a typed pointer.
    VkDeviceAddress bufferAddress(uint32_t frameIndex) const {
        return (frameIndex < MAX_FRAMES_IN_FLIGHT) ? addresses_[frameIndex] : 0;
    }

    uint32_t allocatedCount() const { return allocatedCount_; }

   private:
    VulkanContext* ctx_ = nullptr;
    VkDeviceSize bufferSize_ = 0;

    // VMA-managed buffers with BDA support
    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> buffers_{};
    std::array<VmaAllocation, MAX_FRAMES_IN_FLIGHT> allocations_{};
    std::array<VkDeviceAddress, MAX_FRAMES_IN_FLIGHT> addresses_{};
    std::array<void*, MAX_FRAMES_IN_FLIGHT> mapped_{};

    std::vector<uint32_t> freeSlots_;
    uint32_t allocatedCount_ = 0;

    void createBuffers(ResourceFactory* resources);
    void initFreeList();
};
