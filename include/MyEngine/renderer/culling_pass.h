// include/MyEngine/renderer/culling_pass.h
#pragma once
// =============================================================================
// culling_pass.h - Phase 2B PART2: GPU-driven frustum culling (compute pass).
//
// Second compute pass in the engine (after BloomPass). Unlike BloomPass it owns
// NO descriptor sets: both the CullObject input and the VkDrawIndexedIndirect-
// Command output are addressed via Buffer Device Address (BDA), the same modern
// style as InstanceBufferPool / SkinBufferPool / MaterialRegistry. The frustum
// planes (extracted CPU-side by Frustum) and the two buffer addresses + object
// count are pushed as push constants. cull.comp writes instanceCount (0/1) per
// command; the GPU then skips instanceCount==0 draws in the indirect draw.
//
// Per-frame double buffering: cullBuf_/cmdBuf_ are sized MAX_FRAMES_IN_FLIGHT so
// the CPU can write frame N+1 while the GPU still reads frame N. Both are
// persistently mapped (createMappedStorageBDA); cmdBuf_ also carries
// INDIRECT_BUFFER usage so PART3 can vkCmdDrawIndexedIndirect straight from it.
//
// PART2 scope: the pass computes visibility and writes it; MainPass still draws
// with the CPU loop (unchanged). gpuVisibleCount() reads back the previous
// frame's command buffer (host-visible) purely for the [Cull2B] debug log.
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

class CullingPass {
   public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = FrameSync::MAX_FRAMES_IN_FLIGHT;
    // Upper bound on cullable opaque-static draws per frame (grows later).
    static constexpr uint32_t MAX_DRAWS = 4096;

    struct InitInfo {
        VulkanContext* ctx = nullptr;
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
        const std::vector<DrawTemplate>* drawTemplates = nullptr;  // size == cullObjects (PART2: may be empty)
        glm::mat4 viewProj{1.0f};  // CPU extracts frustum planes from this
        glm::vec3 viewPos{0.0f};   // PART4 4-前-2: world camera position for the cone test
    };

    void init(const InitInfo& info);
    void shutdown();
    void execute(const ExecuteInfo& info);

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
    // frame, whose GPU work the frame fence has already waited on). This is the
    // value that is actually safe to read on the CPU.
    uint32_t lastGpuVisible(uint32_t frameIndex) const {
        return (frameIndex < MAX_FRAMES_IN_FLIGHT) ? lastVisible_[frameIndex] : 0;
    }
    // CPU-side expected visible count using the same Frustum test, for the most
    // recent execute (lets us confirm the GPU result matches the CPU oracle).
    uint32_t lastCpuVisible() const { return lastCpuVisible_; }

   private:
    // Push constant block: must match cull.comp's PC exactly.
    // PART4 4-前-2: viewPos slot for the meshlet-ready cone test. 132 bytes
    // total; well within typical maxPushConstantsSize (256 on NVIDIA Pascal,
    // 128 spec-guaranteed minimum). If a future Hi-Z addition (PART4 4c)
    // overflows 128, the doc says move payload to a small UBO or extra BDA.
    struct PushConstants {
        glm::vec4 planes[6];   //  0 .. 95
        glm::vec4 viewPos;     // 96 ..111  (xyz = camera world position; w reserved)
        glm::uvec2 cullAddr;   //112 ..119
        glm::uvec2 cmdAddr;    //120 ..127
        uint32_t objectCount;  //128 ..131
    };

    void createBuffers();
    void createPipeline(const std::string& shaderDir);

    VulkanContext* ctx_ = nullptr;
    VkDeviceSize cullBufSize_ = 0;
    VkDeviceSize cmdBufSize_ = 0;

    std::array<VmaBuffer, MAX_FRAMES_IN_FLIGHT> cullBuf_{};  // CullObject[] (BDA)
    std::array<VmaBuffer, MAX_FRAMES_IN_FLIGHT> cmdBuf_{};   // VkDrawIndexedIndirectCommand[] (BDA + INDIRECT)
    std::array<uint32_t, MAX_FRAMES_IN_FLIGHT> lastCount_{}; // draws submitted this frame
    std::array<uint32_t, MAX_FRAMES_IN_FLIGHT> lastVisible_{}; // PART2 debug: prev-dispatch visible
    uint32_t lastCpuVisible_ = 0;                              // PART2 debug: CPU oracle

    VkUnique<VkPipelineLayout> pipelineLayout_;
    VkUnique<VkPipeline> pipe_;
};