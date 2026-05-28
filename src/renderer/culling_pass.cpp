// src/renderer/culling_pass.cpp
// =============================================================================
// culling_pass.cpp - Phase 2B PART2 / PART4: GPU-driven frustum culling.
// =============================================================================
// PART4 4-前-3 layout (see header for full rationale):
//   * cullBuf_  : single device-local CullObject[], grown via ensureCapacity().
//                 CPU writes go through per-frame host-mapped staging +
//                 vkCmdCopyBuffer + TRANSFER->COMPUTE barrier (modern
//                 GPU-driven upload).
//   * visBuf_   : single device-local uint32[] bit-packed (32 objects/word).
//                 cull.comp writes per-drawId bit via atomicOr/atomicAnd.
//                 4c two-pass occlusion will start reading the bits.
//   * cmdBuf_[] : per-frame host-mapped (unchanged); 4d will modernise.
// =============================================================================
#include "renderer/culling_pass.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

#include "renderer/barrier.h"
#include "renderer/deletion_queue.h"
#include "renderer/frustum.h"
#include "renderer/shader_util.h"
#include "renderer/vulkan_context.h"

namespace {
constexpr uint32_t kLocalSize = 64;
inline uint32_t groups(uint32_t n) { return (n + kLocalSize - 1) / kLocalSize; }
}  // namespace

void CullingPass::init(const InitInfo& info) {
    if (!info.ctx) throw std::runtime_error("CullingPass::init: ctx is null");
    if (!info.deletionQueue)
        throw std::runtime_error("CullingPass::init: deletionQueue is null (PART4 4-前-3)");
    ctx_ = info.ctx;
    dq_ = info.deletionQueue;
    createBuffers(INITIAL_CAPACITY);
    createPipeline(info.shaderDir);
}

void CullingPass::createBuffers(uint32_t capacity) {
    capacity_ = capacity;

    // Single device-local CullObject[]: read by cull.comp via BDA. Storage +
    // BDA + TRANSFER_DST so vkCmdCopyBuffer from staging works.
    cullBuf_ = VmaBuffer::createDeviceLocal(
        ctx_, static_cast<VkDeviceSize>(capacity) * cullStride(),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    // Single device-local bit-packed visibility (4 bytes per 32 objects).
    visBuf_ = VmaBuffer::createDeviceLocal(
        ctx_, static_cast<VkDeviceSize>(visWordsFor(capacity)) * sizeof(uint32_t),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    // Per-frame upload staging ring (host-mapped). One staging slot per
    // in-flight frame so CPU writes for frame N don't race with frame N-1's
    // GPU copy.
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        cullStaging_[i] = VmaBuffer::createMappedHostVisible(
            ctx_, static_cast<VkDeviceSize>(capacity) * cullStride(),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

        // cmdBuf is still host-mapped per-frame (PART2/3 layout). 4d will
        // move this to device-local + a separate readback.
        cmdBuf_[i] = VmaBuffer::createMappedStorageBDA(
            ctx_, static_cast<VkDeviceSize>(capacity) * cmdStride(),
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        lastCount_[i] = 0;
    }
}

void CullingPass::destroyBuffersToDeletionQueue() {
    // Hand the still-in-use Vulkan buffers to the DeletionQueue: they will be
    // freed MAX_FRAMES_IN_FLIGHT frames from now, after any in-flight GPU work
    // that may still reference them has finished. release() clears the
    // VmaBuffer's ownership so its destructor is a no-op.
    dq_->enqueueBuffer(cullBuf_.buffer(), cullBuf_.allocation());
    cullBuf_.release();

    dq_->enqueueBuffer(visBuf_.buffer(), visBuf_.allocation());
    visBuf_.release();

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        dq_->enqueueBuffer(cullStaging_[i].buffer(), cullStaging_[i].allocation());
        cullStaging_[i].release();
        dq_->enqueueBuffer(cmdBuf_[i].buffer(), cmdBuf_[i].allocation());
        cmdBuf_[i].release();
        lastCount_[i] = 0;
        lastVisible_[i] = 0;
    }
}

void CullingPass::ensureCapacity(uint32_t need) {
    if (need <= capacity_) return;
    const uint32_t newCap = std::max(need, capacity_ * 2u);
    destroyBuffersToDeletionQueue();
    createBuffers(newCap);
    // Note: visBuf_ now contains uninitialised contents. cull.comp writes
    // every drawId's bit each frame (set when visible, clear when culled),
    // so the next dispatch fully overwrites the bits we care about. Bits
    // beyond objectCount are never read.
}

void CullingPass::createPipeline(const std::string& shaderDir) {
    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pc.offset = 0;
    pc.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    li.setLayoutCount = 0;          // fully BDA: no descriptor sets
    li.pSetLayouts = nullptr;
    li.pushConstantRangeCount = 1;
    li.pPushConstantRanges = &pc;
    VkPipelineLayout pl = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(ctx_->device(), &li, nullptr, &pl) != VK_SUCCESS)
        throw std::runtime_error("CullingPass: vkCreatePipelineLayout failed");
    pipelineLayout_ = VkUnique<VkPipelineLayout>(ctx_->device(), pl);

    VkShaderModule mod = shader_util::loadShaderModule(ctx_->device(), shaderDir + "/cull_comp.spv");
    VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = mod;
    stage.pName = "main";
    VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    ci.stage = stage;
    ci.layout = pipelineLayout_.get();
    VkPipeline p = VK_NULL_HANDLE;
    VkResult r = vkCreateComputePipelines(ctx_->device(), VK_NULL_HANDLE, 1, &ci, nullptr, &p);
    vkDestroyShaderModule(ctx_->device(), mod, nullptr);
    if (r != VK_SUCCESS) throw std::runtime_error("CullingPass: vkCreateComputePipelines failed");
    pipe_ = VkUnique<VkPipeline>(ctx_->device(), p);
}

void CullingPass::shutdown() {
    if (!ctx_) return;
    pipe_.reset();
    pipelineLayout_.reset();
    cullBuf_.reset();
    visBuf_.reset();
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        cullStaging_[i].reset();
        cmdBuf_[i].reset();
        lastCount_[i] = 0;
    }
    capacity_ = 0;
    ctx_ = nullptr;
    dq_ = nullptr;
}

void CullingPass::execute(const ExecuteInfo& info) {
    if (info.cmd == VK_NULL_HANDLE) throw std::runtime_error("CullingPass::execute: invalid cmd");
    const uint32_t frame = info.frameIndex;
    if (frame >= MAX_FRAMES_IN_FLIGHT) return;
    if (!info.cullObjects || info.cullObjects->empty()) {
        lastCount_[frame] = 0;
        return;
    }

    const uint32_t count = static_cast<uint32_t>(info.cullObjects->size());

    // PART4 4-前-3: grow if the scene outgrew our buffers. INITIAL_CAPACITY is
    // a starting size, not a hard cap.
    ensureCapacity(count);

    // Debug: read the previous dispatch's instanceCount counts (host-mapped
    // cmdBuf is safe after frame fence) and the current frame's CPU oracle.
    lastVisible_[frame] = gpuVisibleCount(frame);
    {
        Frustum frDbg;
        frDbg.extract(info.viewProj);
        uint32_t cpuVis = 0;
        for (const auto& o : *info.cullObjects)
            if (frDbg.sphereVisible(glm::vec3(o.centerRadius), o.centerRadius.w)) ++cpuVis;
        lastCpuVisible_ = cpuVis;
    }

    lastCount_[frame] = count;

    // 1) CPU memcpy CullObjects into this frame's host-visible staging slot.
    std::memcpy(cullStaging_[frame].mapped(), info.cullObjects->data(),
                static_cast<size_t>(count) * cullStride());

    // 2) Prepare the indirect command template in cmdBuf (still host-mapped).
    //    instanceCount = 0 (cull.comp overwrites it per visibility).
    auto* cmds = static_cast<VkDrawIndexedIndirectCommand*>(cmdBuf_[frame].mapped());
    std::memset(cmds, 0, static_cast<size_t>(count) * cmdStride());
    if (info.drawTemplates) {
        const uint32_t tn = static_cast<uint32_t>(info.drawTemplates->size());
        for (uint32_t i = 0; i < count && i < tn; ++i) {
            const DrawTemplate& t = (*info.drawTemplates)[i];
            cmds[i].indexCount = t.indexCount;
            cmds[i].firstIndex = t.firstIndex;
            cmds[i].vertexOffset = t.vertexOffset;
            cmds[i].firstInstance = t.firstInstance;
        }
    }

    // 3) Stage -> device-local copy + barrier (TRANSFER_WRITE -> SHADER_READ).
    //    Modern GPU-driven upload pattern (vkguide / Granite / AnKi): keeps
    //    the SSBO the shader reads in device-local memory while the CPU writes
    //    through a host-visible staging ring. The barrier is intra-frame; the
    //    cross-frame ordering of frame N+1's copy vs frame N's dispatch relies
    //    on single-queue submission order today (Vulkan13 §U timeline
    //    semaphores will make it spec-formal in 4d).
    VkBufferCopy region{};
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size = static_cast<VkDeviceSize>(count) * cullStride();
    vkCmdCopyBuffer(info.cmd, cullStaging_[frame].buffer(), cullBuf_.buffer(), 1, &region);
    barrier::recordBuffer(*ctx_, info.cmd, barrier::BufferBarrier{
        .buffer = cullBuf_.buffer(),
        .srcStage  = VK_PIPELINE_STAGE_2_COPY_BIT,
        .srcAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStage  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccess = VK_ACCESS_2_SHADER_READ_BIT,
    });

    // 4) Push constants: frustum planes + camera position + 3 buffer addresses
    //    + object count.
    Frustum fr;
    fr.extract(info.viewProj);
    PushConstants pcs{};
    for (int i = 0; i < 6; ++i) pcs.planes[i] = fr.planes[i];
    pcs.viewPos = glm::vec4(info.viewPos, 0.0f);
    const VkDeviceAddress cullAddr = cullBuf_.deviceAddress();
    const VkDeviceAddress cmdAddr  = cmdBuf_[frame].deviceAddress();
    const VkDeviceAddress visAddr  = visBuf_.deviceAddress();
    pcs.cullAddr = glm::uvec2(static_cast<uint32_t>(cullAddr & 0xFFFFFFFFu),
                               static_cast<uint32_t>(cullAddr >> 32));
    pcs.cmdAddr  = glm::uvec2(static_cast<uint32_t>(cmdAddr  & 0xFFFFFFFFu),
                               static_cast<uint32_t>(cmdAddr  >> 32));
    pcs.visAddr  = glm::uvec2(static_cast<uint32_t>(visAddr  & 0xFFFFFFFFu),
                               static_cast<uint32_t>(visAddr  >> 32));
    pcs.objectCount = count;

    // 5) Dispatch the cull.
    vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe_.get());
    vkCmdPushConstants(info.cmd, pipelineLayout_.get(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(PushConstants), &pcs);
    vkCmdDispatch(info.cmd, groups(count), 1, 1);

    // 6) Make the compute writes visible to the indirect-draw stage and to
    //    the next frame's reads of visBuf (the visBuf write barrier is
    //    intra-pass for now; 4c two-pass will tighten cross-pass deps).
    const barrier::BufferBarrier batched[2] = {
        {
            .buffer = cmdBuf_[frame].buffer(),
            .srcStage  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .srcAccess = VK_ACCESS_2_SHADER_WRITE_BIT,
            .dstStage  = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
            .dstAccess = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
        },
        {
            .buffer = visBuf_.buffer(),
            .srcStage  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .srcAccess = VK_ACCESS_2_SHADER_WRITE_BIT,
            .dstStage  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .dstAccess = VK_ACCESS_2_SHADER_READ_BIT,
        },
    };
    barrier::recordBatch(*ctx_, info.cmd, {}, std::span<const barrier::BufferBarrier>{batched, 2}, {});
}

uint32_t CullingPass::gpuVisibleCount(uint32_t frameIndex) const {
    if (frameIndex >= MAX_FRAMES_IN_FLIGHT) return 0;
    if (!cmdBuf_[frameIndex].mapped()) return 0;
    const auto* cmds = static_cast<const VkDrawIndexedIndirectCommand*>(cmdBuf_[frameIndex].mapped());
    const uint32_t n = lastCount_[frameIndex];
    uint32_t visible = 0;
    for (uint32_t i = 0; i < n; ++i)
        if (cmds[i].instanceCount == 1u) ++visible;
    return visible;
}
