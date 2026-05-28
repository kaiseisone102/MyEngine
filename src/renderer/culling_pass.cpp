// src/renderer/culling_pass.cpp
// =============================================================================
// culling_pass.cpp - Phase 2B PART2: GPU-driven frustum culling (compute).
// Engine's second compute pass. Fully BDA / no descriptor sets. See the header
// for the contract. Follows the compute conventions established by BloomPass
// (single COMPUTE stage pipeline, push-constant driven), minus all the mip /
// image / descriptor machinery (this pass only touches BDA storage buffers).
// =============================================================================
#include "renderer/culling_pass.h"

#include <cstring>
#include <stdexcept>

#include "renderer/barrier.h"
#include "renderer/frustum.h"
#include "renderer/shader_util.h"
#include "renderer/vulkan_context.h"

namespace {
constexpr uint32_t kLocalSize = 64;
inline uint32_t groups(uint32_t n) { return (n + kLocalSize - 1) / kLocalSize; }
}  // namespace

void CullingPass::init(const InitInfo& info) {
    if (!info.ctx) throw std::runtime_error("CullingPass::init: ctx is null");
    ctx_ = info.ctx;
    cullBufSize_ = static_cast<VkDeviceSize>(MAX_DRAWS) * sizeof(myengine::shared::CullObject);
    cmdBufSize_ = static_cast<VkDeviceSize>(MAX_DRAWS) * sizeof(VkDrawIndexedIndirectCommand);
    createBuffers();
    createPipeline(info.shaderDir);
}

void CullingPass::createBuffers() {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        cullBuf_[i] = VmaBuffer::createMappedStorageBDA(ctx_, cullBufSize_);
        cmdBuf_[i] = VmaBuffer::createMappedStorageBDA(ctx_, cmdBufSize_,
                                                       VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
        lastCount_[i] = 0;
    }
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
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        cullBuf_[i].reset();
        cmdBuf_[i].reset();
        lastCount_[i] = 0;
    }
    ctx_ = nullptr;
}

void CullingPass::execute(const ExecuteInfo& info) {
    if (info.cmd == VK_NULL_HANDLE) throw std::runtime_error("CullingPass::execute: invalid cmd");
    const uint32_t frame = info.frameIndex;
    if (frame >= MAX_FRAMES_IN_FLIGHT) return;
    if (!info.cullObjects || info.cullObjects->empty()) {
        lastCount_[frame] = 0;
        return;
    }

    uint32_t count = static_cast<uint32_t>(info.cullObjects->size());
    if (count > MAX_DRAWS) count = MAX_DRAWS;

    // PART2 debug: BEFORE we overwrite this frame's buffers, read the result of
    // the PREVIOUS dispatch on the same frameIndex. The frame fence (waited in
    // acquireNextImage) guarantees that GPU work has completed, so the mapped
    // command buffer holds valid instanceCounts from last time. Also compute the
    // CPU oracle for the CURRENT frustum to compare against next time.
    lastVisible_[frame] = gpuVisibleCount(frame);  // uses lastCount_[frame] from prev execute
    {
        Frustum frDbg;
        frDbg.extract(info.viewProj);
        uint32_t cpuVis = 0;
        for (const auto& o : *info.cullObjects)
            if (frDbg.sphereVisible(glm::vec3(o.centerRadius), o.centerRadius.w)) ++cpuVis;
        lastCpuVisible_ = cpuVis;
    }

    lastCount_[frame] = count;

    // 1) upload CullObjects to this frame's BDA buffer.
    std::memcpy(cullBuf_[frame].mapped(), info.cullObjects->data(),
                static_cast<size_t>(count) * sizeof(myengine::shared::CullObject));

    // 2) prepare the indirect command template. instanceCount = 0 (cull.comp
    //    overwrites it per visibility). Other fields come from drawTemplates if
    //    supplied (PART3); in PART2 they may be absent -> left zeroed.
    auto* cmds = static_cast<VkDrawIndexedIndirectCommand*>(cmdBuf_[frame].mapped());
    std::memset(cmds, 0, static_cast<size_t>(count) * sizeof(VkDrawIndexedIndirectCommand));
    if (info.drawTemplates) {
        const uint32_t tn = static_cast<uint32_t>(info.drawTemplates->size());
        for (uint32_t i = 0; i < count && i < tn; ++i) {
            const DrawTemplate& t = (*info.drawTemplates)[i];
            cmds[i].indexCount = t.indexCount;
            cmds[i].firstIndex = t.firstIndex;
            cmds[i].vertexOffset = t.vertexOffset;
            cmds[i].firstInstance = t.firstInstance;
            // instanceCount stays 0 until the compute shader sets it.
        }
    }

    // 3) push constants: frustum planes (CPU-extracted, same test as the shader)
    //    + buffer addresses + object count.
    Frustum fr;
    fr.extract(info.viewProj);
    PushConstants pcs{};
    for (int i = 0; i < 6; ++i) pcs.planes[i] = fr.planes[i];
    const VkDeviceAddress cullAddr = cullBuf_[frame].deviceAddress();
    const VkDeviceAddress cmdAddr = cmdBuf_[frame].deviceAddress();
    pcs.cullAddr = glm::uvec2(static_cast<uint32_t>(cullAddr & 0xFFFFFFFFu),
                              static_cast<uint32_t>(cullAddr >> 32));
    pcs.cmdAddr = glm::uvec2(static_cast<uint32_t>(cmdAddr & 0xFFFFFFFFu),
                             static_cast<uint32_t>(cmdAddr >> 32));
    pcs.objectCount = count;

    // 4) dispatch the cull.
    vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe_.get());
    vkCmdPushConstants(info.cmd, pipelineLayout_.get(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(PushConstants), &pcs);
    vkCmdDispatch(info.cmd, groups(count), 1, 1);

    // 5) make the compute writes visible to the indirect-draw stage (PART3 reads
    //    instanceCount via vkCmdDrawIndexedIndirect). Confined to this pass.
    barrier::recordBuffer(*ctx_, info.cmd, barrier::BufferBarrier{
        .buffer = cmdBuf_[frame].buffer(),
        .srcStage  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccess = VK_ACCESS_2_SHADER_WRITE_BIT,
        .dstStage  = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        .dstAccess = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
    });
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