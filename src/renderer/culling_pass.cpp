// src/renderer/culling_pass.cpp
// =============================================================================
// culling_pass.cpp - Phase 2B PART2 / PART4: GPU-driven cull + scan compaction.
// =============================================================================
// PART4 4-前-4 dispatch sequence per frame:
//   1. memcpy CullObjects -> cullStaging_[frame] (host), CPU writes cmdBuf
//      template (host).
//   2. vkCmdCopyBuffer staging -> cullBuf_ + barrier (TRANSFER -> COMPUTE).
//   3. dispatch cull.comp (writes visBuf bits).
//   4. for each BlockRange:
//        - scan_local.comp   (Pass A): per-wg local scan, totals -> wgTotals.
//        - scan_globals.comp (Pass B): in-place scan of wgTotals[block slice],
//                                       write per-block count -> countBuf1.
//        - scan_scatter.comp (Pass C): recompute local scan, add wg prefix,
//                                       scatter visible templates -> compactCmd1.
//      Barriers (sync2) sequence these correctly.
//   5. final barrier: compactCmd / countBuf -> DRAW_INDIRECT read for main_pass.
//
// Spec-formal cross-frame ordering of cullBuf writes and visBuf reads relies
// on single-queue submission order today; Vulkan13 §U timeline semaphores
// make it spec-formal in 4d.
// =============================================================================
#include "renderer/culling_pass.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

#include "renderer/barrier.h"
#include "renderer/deletion_queue.h"
#include "renderer/frustum.h"
#include "renderer/shader_util.h"
#include "renderer/static_cull_build.h"  // full static_cull::BlockRange
#include "renderer/vulkan_context.h"

namespace {
constexpr uint32_t kLocalSize = 64;
inline uint32_t groups(uint32_t n) { return (n + kLocalSize - 1) / kLocalSize; }

inline glm::uvec2 packAddr(VkDeviceAddress a) {
    return glm::uvec2(static_cast<uint32_t>(a & 0xFFFFFFFFu),
                      static_cast<uint32_t>(a >> 32));
}
}  // namespace

void CullingPass::init(const InitInfo& info) {
    if (!info.ctx) throw std::runtime_error("CullingPass::init: ctx is null");
    if (!info.deletionQueue)
        throw std::runtime_error("CullingPass::init: deletionQueue is null");
    ctx_ = info.ctx;
    dq_ = info.deletionQueue;
    createBuffers(INITIAL_CAPACITY, INITIAL_BLOCKS);
    createPipelines(info.shaderDir);
}

void CullingPass::createBuffers(uint32_t capacity, uint32_t blockCount) {
    capacity_ = capacity;
    blockCount_ = blockCount;

    cullBuf_ = VmaBuffer::createDeviceLocal(
        ctx_, static_cast<VkDeviceSize>(capacity) * cullStride(),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    // PART4 4-前-4 + 4-前-5: per-CullSet output (visBuf + compactCmd1/2 +
    // countBuf1/2 + workgroupTotals). Pipelines are shared; only output
    // buffers differ per cull consumer (camera, shadow, future cascades).
    const VkDeviceSize cmdBufBytes = static_cast<VkDeviceSize>(capacity) * cmdStride();
    const VkBufferUsageFlags compactUsage =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    const VkDeviceSize countBufBytes = static_cast<VkDeviceSize>(blockCount) * sizeof(uint32_t);
    const VkDeviceSize wgTotalsBytes =
        static_cast<VkDeviceSize>(scanWgsFor(capacity)) * sizeof(uint32_t);
    for (size_t s = 0; s < kNumCullSets; ++s) {
        CullOutputs& o = cullOutputs_[s];
        o.visBuf = VmaBuffer::createDeviceLocal(
            ctx_, static_cast<VkDeviceSize>(visWordsFor(capacity)) * sizeof(uint32_t),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        o.compactCmd1 = VmaBuffer::createDeviceLocal(ctx_, cmdBufBytes, compactUsage);
        o.compactCmd2 = VmaBuffer::createDeviceLocal(ctx_, cmdBufBytes, compactUsage);
        o.countBuf1   = VmaBuffer::createDeviceLocal(ctx_, countBufBytes, compactUsage);
        o.countBuf2   = VmaBuffer::createDeviceLocal(ctx_, countBufBytes, compactUsage);
        o.workgroupTotals = VmaBuffer::createDeviceLocal(
            ctx_, wgTotalsBytes,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    }

    // Per-frame staging + per-frame template (4-前-3 layout, shared across sets).
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        cullStaging_[i] = VmaBuffer::createMappedHostVisible(
            ctx_, static_cast<VkDeviceSize>(capacity) * cullStride(),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        cmdBuf_[i] = VmaBuffer::createMappedStorageBDA(
            ctx_, cmdBufBytes,
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        // PART4 4c-B: HizParams[2] per frame (pass1 + pass2 slots). 96B each =
        // 192B. createMappedStorageBDA gives host-mapped + STORAGE + BDA; no
        // extra usage. Receptacle today.
        hizParamsBuf_[i] = VmaBuffer::createMappedStorageBDA(
            ctx_, static_cast<VkDeviceSize>(2) * sizeof(myengine::shared::HizParams));
        lastCount_[i] = 0;
    }
}

void CullingPass::destroyBuffersToDeletionQueue() {
    auto enqueue = [&](VmaBuffer& b) {
        if (b) {
            dq_->enqueueBuffer(b.buffer(), b.allocation());
            b.release();
        }
    };
    enqueue(cullBuf_);
    for (CullOutputs& o : cullOutputs_) {
        enqueue(o.visBuf);
        enqueue(o.compactCmd1);
        enqueue(o.compactCmd2);
        enqueue(o.countBuf1);
        enqueue(o.countBuf2);
        enqueue(o.workgroupTotals);
    }
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        enqueue(cullStaging_[i]);
        enqueue(cmdBuf_[i]);
        enqueue(hizParamsBuf_[i]);  // PART4 4c-B
        lastCount_[i] = 0;
        lastVisible_[i] = 0;
    }
}

void CullingPass::ensureCapacity(uint32_t need) {
    if (need <= capacity_) return;
    const uint32_t newCap = std::max(need, capacity_ * 2u);
    const uint32_t keepBlocks = blockCount_;
    destroyBuffersToDeletionQueue();
    createBuffers(newCap, keepBlocks);
}

void CullingPass::ensureBlockCount(uint32_t need) {
    if (need <= blockCount_) return;
    const uint32_t newBlocks = std::max(need, blockCount_ * 2u);
    const uint32_t keepCap = capacity_;
    destroyBuffersToDeletionQueue();
    createBuffers(keepCap, newBlocks);
}

template <typename PC>
VkUnique<VkPipelineLayout> CullingPass::createComputePipelineLayout() {
    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pc.offset = 0;
    pc.size = sizeof(PC);

    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    li.setLayoutCount = 0;
    li.pSetLayouts = nullptr;
    li.pushConstantRangeCount = 1;
    li.pPushConstantRanges = &pc;
    VkPipelineLayout pl = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(ctx_->device(), &li, nullptr, &pl) != VK_SUCCESS)
        throw std::runtime_error("CullingPass: vkCreatePipelineLayout failed");
    return VkUnique<VkPipelineLayout>(ctx_->device(), pl);
}

VkUnique<VkPipeline> CullingPass::createComputePipeline(const std::string& spvPath,
                                                         VkPipelineLayout layout) {
    VkShaderModule mod = shader_util::loadShaderModule(ctx_->device(), spvPath);
    VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = mod;
    stage.pName = "main";
    VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    ci.stage = stage;
    ci.layout = layout;
    VkPipeline p = VK_NULL_HANDLE;
    VkResult r = vkCreateComputePipelines(ctx_->device(), VK_NULL_HANDLE, 1, &ci, nullptr, &p);
    vkDestroyShaderModule(ctx_->device(), mod, nullptr);
    if (r != VK_SUCCESS) throw std::runtime_error("CullingPass: vkCreateComputePipelines failed");
    return VkUnique<VkPipeline>(ctx_->device(), p);
}

void CullingPass::createPipelines(const std::string& shaderDir) {
    cullLayout_        = createComputePipelineLayout<CullPC>();
    scanLocalLayout_   = createComputePipelineLayout<ScanLocalPC>();
    scanGlobalsLayout_ = createComputePipelineLayout<ScanGlobalsPC>();
    scanScatterLayout_ = createComputePipelineLayout<ScanScatterPC>();

    cullPipe_        = createComputePipeline(shaderDir + "/cull_comp.spv",         cullLayout_.get());
    scanLocalPipe_   = createComputePipeline(shaderDir + "/scan_local_comp.spv",   scanLocalLayout_.get());
    scanGlobalsPipe_ = createComputePipeline(shaderDir + "/scan_globals_comp.spv", scanGlobalsLayout_.get());
    scanScatterPipe_ = createComputePipeline(shaderDir + "/scan_scatter_comp.spv", scanScatterLayout_.get());
}

void CullingPass::shutdown() {
    if (!ctx_) return;
    scanScatterPipe_.reset();
    scanScatterLayout_.reset();
    scanGlobalsPipe_.reset();
    scanGlobalsLayout_.reset();
    scanLocalPipe_.reset();
    scanLocalLayout_.reset();
    cullPipe_.reset();
    cullLayout_.reset();
    cullBuf_.reset();
    for (CullOutputs& o : cullOutputs_) {
        o.visBuf.reset();
        o.compactCmd1.reset();
        o.compactCmd2.reset();
        o.countBuf1.reset();
        o.countBuf2.reset();
        o.workgroupTotals.reset();
    }
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        cullStaging_[i].reset();
        cmdBuf_[i].reset();
        hizParamsBuf_[i].reset();  // PART4 4c-B
        lastCount_[i] = 0;
    }
    capacity_ = 0;
    blockCount_ = 0;
    ctx_ = nullptr;
    dq_ = nullptr;
}

// PART4 4c-B: HizParams BDA + writer (receptacle today). The per-frame buffer
// stores 2 HizParams back-to-back (slot 0 = pass1, slot 1 = pass2); the
// address returned is base + (passIndex - 1) * sizeof(HizParams). Returning 0
// for an out-of-range frame/pass lets cull.comp's gate-off receptacle behave
// safely (the dead-code pass2 branch would just sample a null pointer that it
// never reaches).
VkDeviceAddress CullingPass::hizParamsAddress(uint32_t frameIndex, uint32_t passIndex) const {
    if (frameIndex >= MAX_FRAMES_IN_FLIGHT) return 0;
    if (passIndex < 1 || passIndex > 2) return 0;
    const VkDeviceAddress base = hizParamsBuf_[frameIndex].deviceAddress();
    if (base == 0) return 0;
    return base + static_cast<VkDeviceSize>(passIndex - 1) * sizeof(myengine::shared::HizParams);
}

void CullingPass::writeHizParams(uint32_t frameIndex, uint32_t passIndex,
                                  const myengine::shared::HizParams& params) {
    if (frameIndex >= MAX_FRAMES_IN_FLIGHT) return;
    if (passIndex < 1 || passIndex > 2) return;
    auto* dst = static_cast<myengine::shared::HizParams*>(hizParamsBuf_[frameIndex].mapped());
    if (!dst) return;
    dst[passIndex - 1] = params;
}

void CullingPass::execute(const ExecuteInfo& info) {
    if (info.cmd == VK_NULL_HANDLE) throw std::runtime_error("CullingPass::execute: invalid cmd");
    const uint32_t frame = info.frameIndex;
    if (frame >= MAX_FRAMES_IN_FLIGHT) return;
    if (!info.cullObjects || info.cullObjects->empty() ||
        !info.blockRanges || info.blockRangeCount == 0) {
        lastCount_[frame] = 0;
        return;
    }

    const uint32_t count = static_cast<uint32_t>(info.cullObjects->size());

    ensureCapacity(count);
    ensureBlockCount(info.blockRangeCount);

    // Debug: previous-dispatch instanceCount counts via the host-mapped
    // template's compacted result is no longer trivially countable (we'd need
    // to read compactCmd1Buf_, which is device-local). For now lastVisible_
    // is left as the prior-frame value (not updated here); 4d's HUD cleanup
    // re-routes this via a small readback buffer.
    {
        Frustum frDbg;
        frDbg.extract(info.viewProj);
        uint32_t cpuVis = 0;
        for (const auto& o : *info.cullObjects)
            if (frDbg.sphereVisible(glm::vec3(o.centerRadius), o.centerRadius.w)) ++cpuVis;
        lastCpuVisible_ = cpuVis;
    }

    lastCount_[frame] = count;

    CullOutputs& outs = cullOutputs_[size_t(info.set)];

    // 1) CPU writes to shared staging + cmd template - only when this is the
    //    first cull set this frame. Shadow sets reuse the data Camera uploaded.
    if (!info.inputAlreadyUploaded) {
        std::memcpy(cullStaging_[frame].mapped(), info.cullObjects->data(),
                    static_cast<size_t>(count) * cullStride());

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
    }

    // 2) Stage -> device-local copy of the shared CullObject input (skipped
    //    when an earlier cull set this frame already did it). Zero-fill THIS
    //    set's compactCmd1 so the Legacy indirect_exec fallback gets
    //    instanceCount = 0 on slots scan_scatter does not populate.
    {
        barrier::BufferBarrier transferToCompute[2];
        uint32_t nb = 0;
        if (!info.inputAlreadyUploaded) {
            VkBufferCopy region{};
            region.srcOffset = 0;
            region.dstOffset = 0;
            region.size = static_cast<VkDeviceSize>(count) * cullStride();
            vkCmdCopyBuffer(info.cmd, cullStaging_[frame].buffer(), cullBuf_.buffer(), 1, &region);
            transferToCompute[nb++] = barrier::BufferBarrier{
                .buffer = cullBuf_.buffer(),
                .srcStage  = VK_PIPELINE_STAGE_2_COPY_BIT,
                .srcAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStage  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccess = VK_ACCESS_2_SHADER_READ_BIT,
            };
        }
        vkCmdFillBuffer(info.cmd, outs.compactCmd1.buffer(), 0, VK_WHOLE_SIZE, 0);
        transferToCompute[nb++] = barrier::BufferBarrier{
            .buffer = outs.compactCmd1.buffer(),
            .srcStage  = VK_PIPELINE_STAGE_2_CLEAR_BIT,
            .srcAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStage  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .dstAccess = VK_ACCESS_2_SHADER_WRITE_BIT,
        };
        barrier::recordBatch(*ctx_, info.cmd, {},
                              std::span<const barrier::BufferBarrier>{transferToCompute, nb}, {});
    }

    // 3) Dispatch cull.comp: write THIS set's visBuf bit per drawId.
    {
        CullPC pcs{};  // value-init: _pad = 0, hizParamsAddr = (0, 0) by default
        Frustum fr;
        fr.extract(info.viewProj);
        for (int i = 0; i < 6; ++i) pcs.planes[i] = fr.planes[i];
        pcs.viewPos    = glm::vec4(info.viewPos, 0.0f);
        pcs.cullAddr   = packAddr(cullBuf_.deviceAddress());
        pcs.visAddr    = packAddr(outs.visBuf.deviceAddress());
        pcs.objectCount = count;
        // PART4 4c-B receptacle: hizParamsAddr stays at (0, 0) so cull.comp's
        // pass2 / HZB branch (dead code today) is never entered. 4c-C will set
        // this from CullingPass::hizParamsAddress(frame, passIndex) when
        // pass_chain dispatches the two-pass orchestration.
        pcs.hizParamsAddr = glm::uvec2(0u, 0u);
        vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cullPipe_.get());
        vkCmdPushConstants(info.cmd, cullLayout_.get(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                            sizeof(CullPC), &pcs);
        vkCmdDispatch(info.cmd, groups(count), 1, 1);
        barrier::recordBuffer(*ctx_, info.cmd, barrier::BufferBarrier{
            .buffer = outs.visBuf.buffer(),
            .srcStage  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .srcAccess = VK_ACCESS_2_SHADER_WRITE_BIT,
            .dstStage  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .dstAccess = VK_ACCESS_2_SHADER_READ_BIT,
        });
    }

    // 4) Per-block 3-pass scan compaction using THIS set's output buffers.
    const VkDeviceAddress visAddr     = outs.visBuf.deviceAddress();
    const VkDeviceAddress wgTotAddr   = outs.workgroupTotals.deviceAddress();
    const VkDeviceAddress cmdTplAddr  = cmdBuf_[frame].deviceAddress();
    const VkDeviceAddress compactAddr = outs.compactCmd1.deviceAddress();
    const VkDeviceAddress countAddr   = outs.countBuf1.deviceAddress();

    uint32_t wgCursor = 0;  // running offset into workgroupTotalsBuf_
    for (uint32_t i = 0; i < info.blockRangeCount; ++i) {
        const static_cull::BlockRange& range = info.blockRanges[i];
        if (range.drawCount == 0) continue;
        const uint32_t numWgs = (range.drawCount + SCAN_WG_SIZE - 1u) / SCAN_WG_SIZE;

        // Pass A: scan_local
        {
            ScanLocalPC pcs{};
            pcs.visAddr             = packAddr(visAddr);
            pcs.workgroupTotalsAddr = packAddr(wgTotAddr);
            pcs.blockFirstDraw      = range.firstDraw;
            pcs.blockDrawCount      = range.drawCount;
            pcs.wgFirstInBlock      = wgCursor;
            vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, scanLocalPipe_.get());
            vkCmdPushConstants(info.cmd, scanLocalLayout_.get(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                sizeof(ScanLocalPC), &pcs);
            vkCmdDispatch(info.cmd, numWgs, 1, 1);
            barrier::recordBuffer(*ctx_, info.cmd, barrier::BufferBarrier{
                .buffer = outs.workgroupTotals.buffer(),
                .srcStage  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccess = VK_ACCESS_2_SHADER_WRITE_BIT,
                .dstStage  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccess = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
            });
        }

        // Pass B: scan_globals (single workgroup scans this block's slice)
        {
            ScanGlobalsPC pcs{};
            pcs.workgroupTotalsAddr = packAddr(wgTotAddr);
            pcs.countBufAddr        = packAddr(countAddr);
            pcs.wgFirstInBlock      = wgCursor;
            pcs.numWgsInBlock       = numWgs;
            pcs.blockIdx            = i;
            vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, scanGlobalsPipe_.get());
            vkCmdPushConstants(info.cmd, scanGlobalsLayout_.get(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                sizeof(ScanGlobalsPC), &pcs);
            vkCmdDispatch(info.cmd, 1, 1, 1);
            barrier::recordBuffer(*ctx_, info.cmd, barrier::BufferBarrier{
                .buffer = outs.workgroupTotals.buffer(),
                .srcStage  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccess = VK_ACCESS_2_SHADER_WRITE_BIT,
                .dstStage  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccess = VK_ACCESS_2_SHADER_READ_BIT,
            });
        }

        // Pass C: scan_scatter
        {
            ScanScatterPC pcs{};
            pcs.visAddr             = packAddr(visAddr);
            pcs.workgroupTotalsAddr = packAddr(wgTotAddr);
            pcs.cmdTemplateAddr     = packAddr(cmdTplAddr);
            pcs.compactCmdAddr      = packAddr(compactAddr);
            pcs.blockFirstDraw      = range.firstDraw;
            pcs.blockDrawCount      = range.drawCount;
            pcs.wgFirstInBlock      = wgCursor;
            vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, scanScatterPipe_.get());
            vkCmdPushConstants(info.cmd, scanScatterLayout_.get(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                sizeof(ScanScatterPC), &pcs);
            vkCmdDispatch(info.cmd, numWgs, 1, 1);
        }

        wgCursor += numWgs;
    }

    // 5) Final barrier: compactCmd + countBuf -> DRAW_INDIRECT read.
    const barrier::BufferBarrier batched[2] = {
        {
            .buffer = outs.compactCmd1.buffer(),
            .srcStage  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .srcAccess = VK_ACCESS_2_SHADER_WRITE_BIT,
            .dstStage  = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
            .dstAccess = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
        },
        {
            .buffer = outs.countBuf1.buffer(),
            .srcStage  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .srcAccess = VK_ACCESS_2_SHADER_WRITE_BIT,
            .dstStage  = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
            .dstAccess = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
        },
    };
    barrier::recordBatch(*ctx_, info.cmd, {}, std::span<const barrier::BufferBarrier>{batched, 2}, {});
}
