// include/MyEngine/renderer/skin_buffer_pool.h
#pragma once
// =============================================================================
// skin_buffer_pool.h - Phase 1B-4: BDA-enabled skin matrix pool
// =============================================================================
// One large SSBO per frame, holding bone matrices for up to MAX_ENTITIES.
// Each entity gets a fixed slot of MAX_BONES_PER_ENTITY mat4s.
//
// BDA migration (Phase 1B-4):
//   The buffer is now created with VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
//   and vmaCreateBuffer(VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT).
//   bufferAddress(frameIndex) returns a VkDeviceAddress for the frame's
//   buffer, which is then placed into the push constant (SkinnedPushConstants
//   ::skinBuffer). Shaders dereference this address directly via
//   GL_EXT_buffer_reference - no descriptor set required.
//
//   The legacy descriptor set path (layout / descriptorSet / pool) is kept
//   during Step 1B-4a so the build stays green while shaders still bind
//   set=2. It will be removed in Step 1B-4c once shaders switch over.
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

    // ── Legacy descriptor path (Phase 1B-4a: still present, removed in 1B-4c) ──
    VkDescriptorSetLayout layout() const { return layout_; }
    VkDescriptorSet descriptorSet(uint32_t frameIndex) const { return sets_[frameIndex]; }

    // ── New BDA path (Phase 1B-4a+): pass this into SkinnedPushConstants.skinBuffer ──
    VkDeviceAddress bufferAddress(uint32_t frameIndex) const {
        return (frameIndex < MAX_FRAMES_IN_FLIGHT) ? addresses_[frameIndex] : 0;
    }

    uint32_t allocatedCount() const { return allocatedCount_; }

   private:
    VulkanContext* ctx_ = nullptr;
    VkDeviceSize bufferSize_ = 0;

    // Legacy descriptor path
    VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
    VkDescriptorPool pool_ = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> sets_{};

    // Buffers (VMA-managed, BDA-enabled)
    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> buffers_{};
    std::array<VmaAllocation, MAX_FRAMES_IN_FLIGHT> allocations_{};
    std::array<VkDeviceAddress, MAX_FRAMES_IN_FLIGHT> addresses_{};
    std::array<void*, MAX_FRAMES_IN_FLIGHT> mapped_{};

    std::vector<uint32_t> freeSlots_;
    uint32_t allocatedCount_ = 0;

    void createLayout();
    void createPool();
    void createBuffers(ResourceFactory* resources);
    void allocateAndWriteSets();
    void initFreeList();
};
