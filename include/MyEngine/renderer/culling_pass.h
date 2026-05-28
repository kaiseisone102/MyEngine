// include/MyEngine/renderer/culling_pass.h
#pragma once
// =============================================================================
// culling_pass.h - Phase 2B PART2 / PART4: GPU-driven frustum culling (compute).
//
// Second compute pass in the engine (after BloomPass). Fully BDA-driven: both
// CullObject input and the VkDrawIndexedIndirectCommand output (cmdBuf_) and
// the visibility bitmap (visBuf_) are accessed via Buffer Device Address. The
// frustum planes (CPU-extracted by Frustum) plus the camera position (for the
// meshlet-ready cone test, PART4 4-前-2) plus the three BDA pointers plus the
// object count are passed as push constants. cull.comp writes
// cmds[drawId].instanceCount = 0/1 and visBuf_[drawId] = visible-bit; the GPU
// then skips instanceCount==0 draws in the indirect draw.
//
// PART4 4-前-3 persistent layout (this pass owns the buffers for the engine's
// lifetime, sized initially to INITIAL_CAPACITY and grown lazily via
// ensureCapacity()):
//   * cullBuf_:  single DEVICE-LOCAL CullObject[]      <- read by cull.comp.
//                CPU writes go through per-frame host-mapped staging +
//                vkCmdCopyBuffer with a TRANSFER->COMPUTE barrier so the
//                shader sees the new contents.
//   * cullStaging_[FIF]: per-frame host-mapped staging ring.
//   * visBuf_:   single DEVICE-LOCAL bit-packed visibility (uint32 per 32
//                objects). cull.comp atomicOr/atomicAnd per drawId. Only
//                written today; PART4 4c two-pass occlusion will read the
//                previous frame's bits (cross-frame read needs Vulkan13 §U
//                timeline semaphore sync to be spec-formal; in practice on a
//                single graphics queue submission order serialises it).
//   * cmdBuf_[FIF]: per-frame host-mapped (unchanged from PART2/3). 4d's
//                純 GPU-driven 化仕上げ step will move this to device-local
//                + a separate readback buffer.
//
// ensureCapacity(need, dq): if need > capacity_, doubles capacity, allocates
// new buffers, and hands the old (VkBuffer + VmaAllocation) pairs to the
// DeletionQueue so they are freed MAX_FRAMES_IN_FLIGHT frames later (§5c).
// INITIAL_CAPACITY=4096 is now a starting size, NOT a hard cap.
// =============================================================================
#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "renderer/vma_buffer.h"
#include "renderer/vk_unique.h"
#include "renderer/frame_sync.h"
#include "shaders/shared/types.h"

class VulkanContext;
class DeletionQueue;

class CullingPass {
   public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = FrameSync::MAX_FRAMES_IN_FLIGHT;
    // PART4 4-前-3: INITIAL capacity. The buffers grow on demand via
    // ensureCapacity(); this constant is NOT a hard cap. Grow path verified
    // by temporarily setting this to 32 and observing 77 draws still rendered
    // correctly (allocations doubled to 32 -> 64 -> 128).
    static constexpr uint32_t INITIAL_CAPACITY = 4096;

    struct InitInfo {
        VulkanContext* ctx = nullptr;
        DeletionQueue* deletionQueue = nullptr;  // PART4 4-前-3: for grow path
        std::string shaderDir;
    };

    // A single draw command template (everything except instanceCount, which the
    // compute shader fills). Matches VkDrawIndexedIndirectCommand layout.
    struct DrawTemplate {
        uint32_t indexCount = 0;
        uint32_t firstIndex = 0;
        int32_t  vertexOffset = 0;
        uint32_t firstInstance = 0;
    };

    struct ExecuteInfo {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        uint32_t frameIndex = 0;
        const std::vector<myengine::shared::CullObject>* cullObjects = nullptr;
        const std::vector<DrawTemplate>* drawTemplates = nullptr;  // size == cullObjects
        glm::mat4 viewProj{1.0f};  // CPU extracts frustum planes from this
        glm::vec3 viewPos{0.0f};   // PART4 4-前-2: world camera position for the cone test
    };

    void init(const InitInfo& info);
    void shutdown();
    void execute(const ExecuteInfo& info);

    // PART4 4-前-3: grow cullBuf / visBuf / cullStaging / cmdBuf if needed.
    // Old buffers are handed to the DeletionQueue (set at init) to be freed
    // MAX_FRAMES_IN_FLIGHT frames later, after any in-flight GPU work is done.
    void ensureCapacity(uint32_t need);

    uint32_t capacity() const noexcept { return capacity_; }

    // BDA of the indirect command buffer for a frame (PART3 draws from this).
    VkDeviceAddress commandAddress(uint32_t frameIndex) const {
        return (frameIndex < MAX_FRAMES_IN_FLIGHT) ? cmdBuf_[frameIndex].deviceAddress() : 0;
    }
    VkBuffer commandBuffer(uint32_t frameIndex) const {
        return (frameIndex < MAX_FRAMES_IN_FLIGHT) ? cmdBuf_[frameIndex].buffer() : VK_NULL_HANDLE;
    }
    uint32_t drawCount(uint32_t frameIndex) const {
        return (frameIndex < MAX_FRAMES_IN_FLIGHT) ? lastCount_[frameIndex] : 0;
    }

    // PART2 debug: count instanceCount==1 in a frame's (host-visible) command
    // buffer. Valid to read once that frame's GPU work has completed.
    uint32_t gpuVisibleCount(uint32_t frameIndex) const;

    // PART2 debug: GPU visible count captured at the START of the latest execute
    // for this frameIndex (i.e. the result of the PREVIOUS dispatch on the same
    // frame, whose GPU work the frame fence has already waited on).
    uint32_t lastGpuVisible(uint32_t frameIndex) const {
        return (frameIndex < MAX_FRAMES_IN_FLIGHT) ? lastVisible_[frameIndex] : 0;
    }
    // CPU-side expected visible count using the same Frustum test.
    uint32_t lastCpuVisible() const { return lastCpuVisible_; }

   private:
    // Push constant block: must match cull.comp's PC exactly.
    // PART4 4-前-3: + visAddr (bit-packed visibility bitmap BDA) -> 140 bytes.
    // P620 maxPushConstantsSize is 256 in practice (128 spec-guaranteed); a
    // future Hi-Z addition (PART4 4c) that pushes past 128 should move payload
    // to a small UBO or extra BDA per HiZ_PART4_Design §7.
    struct PushConstants {
        glm::vec4  planes[6];   //  0 .. 95
        glm::vec4  viewPos;     // 96 ..111
        glm::uvec2 cullAddr;    //112 ..119  CullObject[] (device-local)
        glm::uvec2 cmdAddr;     //120 ..127  VkDrawIndexedIndirectCommand[] (per-frame)
        glm::uvec2 visAddr;     //128 ..135  uint32[] bit-packed visibility (device-local)
        uint32_t   objectCount; //136 ..139
    };

    void createBuffers(uint32_t capacity);
    void destroyBuffersToDeletionQueue();  // releases ownership + enqueues old buffers
    void createPipeline(const std::string& shaderDir);

    static constexpr VkDeviceSize cullStride() { return sizeof(myengine::shared::CullObject); }
    static constexpr VkDeviceSize cmdStride()  { return sizeof(VkDrawIndexedIndirectCommand); }
    static constexpr uint32_t     visWordsFor(uint32_t cap) { return (cap + 31u) >> 5; }

    VulkanContext* ctx_ = nullptr;
    DeletionQueue* dq_ = nullptr;
    uint32_t capacity_ = 0;

    // PART4 4-前-3: single persistent device-local CullObject[] + per-frame
    // host-mapped staging (modern GPU-driven upload pattern: CPU writes to
    // staging, cmdCopyBuffer + barrier copies to the device buffer that the
    // compute shader reads).
    VmaBuffer cullBuf_;                                       // device-local
    std::array<VmaBuffer, MAX_FRAMES_IN_FLIGHT> cullStaging_; // host-mapped

    // Per-frame indirect command ring (host-mapped). Still per-frame because
    // CPU writes the template into it and GPU writes instanceCount; readback
    // is host-mapped for the debug HUD. 4d's GPU-driven 仕上げ moves this to
    // device-local with a separate readback buffer.
    std::array<VmaBuffer, MAX_FRAMES_IN_FLIGHT> cmdBuf_;

    // PART4 4-前-3: bit-packed visibility (uint32 per 32 objects). One bit per
    // drawId set by cull.comp via atomicOr/atomicAnd. Single device-local
    // buffer; 4c two-pass occlusion reads the previous frame's bits.
    VmaBuffer visBuf_;

    std::array<uint32_t, MAX_FRAMES_IN_FLIGHT> lastCount_{};
    std::array<uint32_t, MAX_FRAMES_IN_FLIGHT> lastVisible_{};
    uint32_t lastCpuVisible_ = 0;

    VkUnique<VkPipelineLayout> pipelineLayout_;
    VkUnique<VkPipeline> pipe_;
};
