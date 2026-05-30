// src/renderer/skinning_pass.cpp
// =============================================================================
// skinning_pass.cpp - Phase 2G: batched compute LBS pre-pass. See header.
// =============================================================================
#include "renderer/skinning_pass.h"

#include <span>
#include <stdexcept>

#include "renderer/barrier.h"
#include "renderer/gpu_skinning.h"
#include "renderer/shader_util.h"
#include "renderer/skin_instance_pool.h"
#include "renderer/vulkan_context.h"
#include "shaders/shared/types.h"

namespace myengine::renderer {

namespace {
constexpr uint32_t kLocalSize = 64;
inline uint32_t groups(uint32_t n) { return (n + kLocalSize - 1) / kLocalSize; }
}  // namespace

void SkinningPass::init(const InitInfo& info) {
    if (!info.ctx) throw std::runtime_error("SkinningPass::init: ctx is null");
    ctx_ = info.ctx;

    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pc.offset = 0;
    pc.size = sizeof(myengine::shared::SkinningPushConstants);

    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    li.setLayoutCount = 0;
    li.pSetLayouts = nullptr;
    li.pushConstantRangeCount = 1;
    li.pPushConstantRanges = &pc;
    VkPipelineLayout pl = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(ctx_->device(), &li, nullptr, &pl) != VK_SUCCESS)
        throw std::runtime_error("SkinningPass: vkCreatePipelineLayout failed");
    layout_ = VkUnique<VkPipelineLayout>(ctx_->device(), pl);

    VkShaderModule mod =
        shader_util::loadShaderModule(ctx_->device(), info.shaderDir + "/skinning_comp.spv");
    VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = mod;
    stage.pName = "main";
    VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    ci.stage = stage;
    ci.layout = layout_.get();
    VkPipeline p = VK_NULL_HANDLE;
    VkResult r =
        vkCreateComputePipelines(ctx_->device(), ctx_->pipelineCache(), 1, &ci, nullptr, &p);
    vkDestroyShaderModule(ctx_->device(), mod, nullptr);
    if (r != VK_SUCCESS) throw std::runtime_error("SkinningPass: vkCreateComputePipelines failed");
    pipeline_ = VkUnique<VkPipeline>(ctx_->device(), p);
}

void SkinningPass::shutdown() {
    pipeline_.reset();
    layout_.reset();
    ctx_ = nullptr;
}

void SkinningPass::execute(VkCommandBuffer cmd, uint32_t frameIndex,
                           const SkinnedVertexPool& verts, const SkinInstancePool& instances,
                           uint32_t totalVertexCount) {
    const uint32_t instanceCount = instances.count(frameIndex);
    if (instanceCount == 0 || totalVertexCount == 0) return;

    myengine::shared::SkinningPushConstants pc{};
    pc.instanceBuffer = instances.bufferAddress(frameIndex);
    pc.instanceCount = instanceCount;
    pc.totalVertexCount = totalVertexCount;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_.get());
    vkCmdPushConstants(cmd, layout_.get(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
    vkCmdDispatch(cmd, groups(totalVertexCount), 1, 1);

    // COMPUTE write -> the draw passes that read the skinned streams. Cover both
    // a future BDA read in the vertex shader (VERTEX_SHADER / SHADER_READ) and a
    // vertex-buffer bind (VERTEX_ATTRIBUTE_INPUT / VERTEX_ATTRIBUTE_READ); 2G-2
    // picks one. In 2G-1 nothing reads these yet, so the barrier is harmless.
    const VkPipelineStageFlags2 dstStage =
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
    const VkAccessFlags2 dstAccess =
        VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
    const barrier::BufferBarrier bufBarriers[2] = {
        barrier::BufferBarrier{
            .buffer = verts.posBuffer(frameIndex),
            .srcStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .srcAccess = VK_ACCESS_2_SHADER_WRITE_BIT,
            .dstStage = dstStage,
            .dstAccess = dstAccess},
        barrier::BufferBarrier{
            .buffer = verts.normalBuffer(frameIndex),
            .srcStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .srcAccess = VK_ACCESS_2_SHADER_WRITE_BIT,
            .dstStage = dstStage,
            .dstAccess = dstAccess},
    };
    barrier::recordBatch(*ctx_, cmd, {}, std::span<const barrier::BufferBarrier>{bufBarriers, 2}, {});
}

}  // namespace myengine::renderer
