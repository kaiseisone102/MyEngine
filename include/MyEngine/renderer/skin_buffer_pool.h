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

#include "renderer/vma_buffer.h"

#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

#include "frame_sync.h"

class VulkanContext;
class ResourceFactory;
class DeletionQueue;

class SkinBufferPool {
   public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = FrameSync::MAX_FRAMES_IN_FLIGHT;
    static constexpr uint32_t MAX_BONES_PER_ENTITY = 128;
    // Initial entity capacity. Foundations \xc2\xa78.1 dynamic-growth pattern: when
    // allocate() empties the free list, capacity_ doubles; new per-frame
    // buffers are created at the larger size and the old pair is handed to
    // the DeletionQueue. Existing Slots stay valid -- their boneOffset (in
    // mat4 units) is < oldCapacity * MAX_BONES_PER_ENTITY, which is still
    // inside the new buffer's first oldCapacity slots, so callers never see
    // an invalidated handle. Bone matrices are rewritten every frame via
    // update(), so no data copy is needed across the grow.
    static constexpr uint32_t INITIAL_CAPACITY = 128;

    struct Slot {
        uint32_t boneOffset = 0;
        uint32_t boneCapacity = 0;
        bool valid() const { return boneCapacity > 0; }
        static Slot invalid() { return Slot{0, 0}; }
    };

    void init(VulkanContext* ctx, ResourceFactory* resources, DeletionQueue* dq);
    void shutdown();

    Slot allocate();
    void release(Slot slot);
    void update(uint32_t frameIndex, const Slot& slot, const std::vector<glm::mat4>& matrices);

    // BDA: shaders cast this address to a typed pointer.
    VkDeviceAddress bufferAddress(uint32_t frameIndex) const {
        return (frameIndex < MAX_FRAMES_IN_FLIGHT) ? buffers_[frameIndex].deviceAddress() : 0;
    }

    uint32_t allocatedCount() const { return allocatedCount_; }

    uint32_t capacity() const { return capacity_; }

   private:
    VulkanContext* ctx_ = nullptr;
    DeletionQueue* dq_ = nullptr;
    uint32_t capacity_ = 0;

    // VMA-managed buffers with BDA support
    std::array<VmaBuffer, MAX_FRAMES_IN_FLIGHT> buffers_{};

    std::vector<uint32_t> freeSlots_;
    uint32_t allocatedCount_ = 0;

    void allocateBuffers();
    void growToDouble();  // capacity_ *= 2, recreate buffers, extend freeSlots_
    void appendFreeSlots(uint32_t fromIdx, uint32_t toIdx);
};
