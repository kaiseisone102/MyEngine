// src/renderer/shadow_pass.cpp
// =============================================================================
// Phase 5-B 段階B-A: 静的 Model (装備品) の影描画を追加
// =============================================================================
#include "renderer/shadow_pass.h"
#include <iostream>

#include <cstddef>
#include <stdexcept>

#include "renderer/barrier.h"
#include "renderer/geometry_buffer.h"
#include "renderer/indirect_exec.h"
#include "renderer/mesh.h"
#include "renderer/model.h"
#include "renderer/resource_factory.h"
#include "renderer/shader_util.h"
#include "renderer/static_cull_build.h"
#include "world/engine_origin.h"  // E: toEngineRelative for skinned shadow push
#include "renderer/vulkan_context.h"

void ShadowPass::init(const InitInfo& info) {
    if (!info.ctx || !info.resources) throw std::runtime_error("ShadowPass::init: invalid info");
    ctx_ = info.ctx;
    extent_ = info.extent;
    depthFormat_ = info.depthFormat;

    // PART4 4d: dynamic rendering migration. No VkRenderPass / VkFramebuffer.
    createTarget(info.resources);
    createStaticPipeline(info.frameSetLayout, info.shaderDir);
    createSkinnedPipeline(info.frameSetLayout, info.shaderDir);
}

void ShadowPass::createTarget(ResourceFactory* resources) {
    RenderTarget::Desc desc{};
    desc.width = extent_.width;
    desc.height = extent_.height;
    desc.format = depthFormat_;
    desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    desc.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    desc.createSampler = true;
    desc.samplerFilter = VK_FILTER_LINEAR;
    desc.samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    desc.samplerBorderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    target_.init(ctx_, resources, desc);
}

namespace {

struct ShadowPipelineParams {
    VkDevice device;
    VkPipelineCache cache;  // PART4 4d M1: persistent pipeline cache from VulkanContext
    VkFormat depthFormat;
    VkPipelineLayout layout;
    VkShaderModule vert;
    bool useSkinningAttrs;
};

VkPipeline createShadowPipeline(const ShadowPipelineParams& p) {
    VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage.module = p.vert;
    stage.pName = "main";

    VkVertexInputBindingDescription bind{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};

    VkVertexInputAttributeDescription posAttr{
        0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, pos))};
    VkVertexInputAttributeDescription jointIdxAttr{
        4, 0, VK_FORMAT_R32G32B32A32_SINT,
        static_cast<uint32_t>(offsetof(Vertex, jointIndices))};
    VkVertexInputAttributeDescription jointWtAttr{
        5, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
        static_cast<uint32_t>(offsetof(Vertex, jointWeights))};

    VkVertexInputAttributeDescription staticAttrs[1] = {posAttr};
    VkVertexInputAttributeDescription skinnedAttrs[3] = {posAttr, jointIdxAttr, jointWtAttr};

    VkPipelineVertexInputStateCreateInfo vi{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &bind;
    vi.vertexAttributeDescriptionCount = p.useSkinningAttrs ? 3 : 1;
    vi.pVertexAttributeDescriptions = p.useSkinningAttrs ? skinnedAttrs : staticAttrs;

    VkPipelineInputAssemblyStateCreateInfo ia{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.lineWidth = 1.f;
    rs.cullMode = VK_CULL_MODE_FRONT_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.depthBiasEnable = VK_TRUE;
    rs.depthBiasConstantFactor = 1.25f;
    rs.depthBiasSlopeFactor = 1.75f;

    VkPipelineMultisampleStateCreateInfo ms{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynStates;

    // PART4 4d: dynamic rendering. Depth-only pass; no color attachments.
    VkPipelineRenderingCreateInfo rci{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rci.colorAttachmentCount = 0;
    rci.pColorAttachmentFormats = nullptr;
    rci.depthAttachmentFormat = p.depthFormat;
    rci.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    VkGraphicsPipelineCreateInfo pci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pci.pNext = &rci;
    pci.stageCount = 1;
    pci.pStages = &stage;
    pci.pVertexInputState = &vi;
    pci.pInputAssemblyState = &ia;
    pci.pViewportState = &vp;
    pci.pRasterizationState = &rs;
    pci.pMultisampleState = &ms;
    pci.pDepthStencilState = &ds;
    pci.pDynamicState = &dyn;
    pci.layout = p.layout;
    pci.renderPass = VK_NULL_HANDLE;  // PART4 4d: dynamic rendering
    pci.subpass = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(p.device, p.cache, 1, &pci, nullptr, &pipeline) !=
        VK_SUCCESS) {
        throw std::runtime_error("ShadowPass: vkCreateGraphicsPipelines failed");
    }
    return pipeline;
}

}  // namespace

void ShadowPass::createStaticPipeline(VkDescriptorSetLayout frameSetLayout,
                                      const std::string& shaderDir) {
    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pc.offset = 0;
    // PART4 4-前-5: shadow.vert now reads DrawData via BDA (DrawDataPool),
    // so the push constant is just the BDA pointer (8 bytes) instead of the
    // 64-byte per-draw model matrix. Same layout as StaticDrawPushConstants.
    pc.size = sizeof(myengine::shared::ShadowDrawPushConstants);

    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    lci.setLayoutCount = 1;
    lci.pSetLayouts = &frameSetLayout;
    lci.pushConstantRangeCount = 1;
    lci.pPushConstantRanges = &pc;
    VkPipelineLayout slay = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(ctx_->device(), &lci, nullptr, &slay) != VK_SUCCESS) {
        throw std::runtime_error("ShadowPass: static layout failed");
    }
    staticLayout_ = VkUnique<VkPipelineLayout>(ctx_->device(), slay);

    VkShaderModule vert =
        shader_util::loadShaderModule(ctx_->device(), shaderDir + "shadow_vert.spv");
    try {
        ShadowPipelineParams p{ctx_->device(), ctx_->pipelineCache(), depthFormat_, staticLayout_.get(), vert, false};
        staticPipeline_ = VkUnique<VkPipeline>(ctx_->device(), createShadowPipeline(p));
    } catch (...) {
        vkDestroyShaderModule(ctx_->device(), vert, nullptr);
        throw;
    }
    vkDestroyShaderModule(ctx_->device(), vert, nullptr);
}

void ShadowPass::createSkinnedPipeline(VkDescriptorSetLayout frameSetLayout,
                                            const std::string& shaderDir) {
    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pc.offset = 0;
    pc.size = sizeof(SkinnedPushConstants);

    VkDescriptorSetLayout setLayouts[1] = {frameSetLayout};
    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    lci.setLayoutCount = 1;
    lci.pSetLayouts = setLayouts;
    lci.pushConstantRangeCount = 1;
    lci.pPushConstantRanges = &pc;
    VkPipelineLayout klay = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(ctx_->device(), &lci, nullptr, &klay) != VK_SUCCESS) {
        throw std::runtime_error("ShadowPass: skinned layout failed");
    }
    skinnedLayout_ = VkUnique<VkPipelineLayout>(ctx_->device(), klay);

    VkShaderModule vert =
        shader_util::loadShaderModule(ctx_->device(), shaderDir + "shadow_skinned_vert.spv");
    try {
        ShadowPipelineParams p{ctx_->device(), ctx_->pipelineCache(), depthFormat_, skinnedLayout_.get(), vert, true};
        skinnedPipeline_ = VkUnique<VkPipeline>(ctx_->device(), createShadowPipeline(p));
    } catch (...) {
        vkDestroyShaderModule(ctx_->device(), vert, nullptr);
        throw;
    }
    vkDestroyShaderModule(ctx_->device(), vert, nullptr);
}

void ShadowPass::execute(const ExecuteInfo& info) {
    if (!info.cmd) throw std::runtime_error("ShadowPass::execute: invalid cmd");

    // PART4 4d: dynamic rendering. shadow target sits at READ_ONLY between
    // frames (the post-rendering barrier at the end of execute() puts it
    // there, and on the very first frame the image is in UNDEFINED). Move
    // to attachment-write layout for the BeginRendering scope; UNDEFINED
    // oldLayout discards prior content - the depth loadOp = CLEAR resets
    // every texel anyway.
    barrier::recordImage(*ctx_, info.cmd, barrier::ImageBarrier{
        .image = target_.image(),
        .range = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1},
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .srcStage  = VK_PIPELINE_STAGE_2_NONE,
        .srcAccess = VK_ACCESS_2_NONE,
        .dstStage  = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                     VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        .dstAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    });

    VkRenderingAttachmentInfo depthAtt{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    depthAtt.imageView = target_.view();
    depthAtt.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    depthAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAtt.clearValue.depthStencil = {1.0f, 0};

    VkRenderingInfo rp{VK_STRUCTURE_TYPE_RENDERING_INFO};
    rp.renderArea = {{0, 0}, extent_};
    rp.layerCount = 1;
    rp.colorAttachmentCount = 0;
    rp.pDepthAttachment = &depthAtt;

    vkCmdBeginRendering(info.cmd, &rp);

    VkViewport viewport{0.f,
                        0.f,
                        static_cast<float>(extent_.width),
                        static_cast<float>(extent_.height),
                        0.f,
                        1.f};
    VkRect2D scissor{{0, 0}, extent_};

    // ─── PART4 4-前-5: GPU-driven static-mesh shadow ──────────────────────
    // Cube mesh + static model draws now flow through indirect_exec backed by
    // CullingPass's shadow-set compactCmd / countBuf. The shadow vertex
    // shader (shadow.vert) reads DrawData[gl_InstanceIndex].model from the
    // DrawDataPool BDA - same source main_pass uses - so the per-block
    // ranges and template are identical; only the cull frustum / visBuf
    // differ between camera and shadow.
    const bool hasGpuDrivenStatic =
        info.geometry &&
        info.blockRanges && info.blockRangeCount > 0 &&
        info.compactCommandBuffer != VK_NULL_HANDLE &&
        info.indirectCountBuffer != VK_NULL_HANDLE &&
        info.drawBufferAddress != 0;

    if (hasGpuDrivenStatic) {
        vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, staticPipeline_.get());
        vkCmdSetViewport(info.cmd, 0, 1, &viewport);
        vkCmdSetScissor(info.cmd, 0, 1, &scissor);
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, staticLayout_.get(), 0, 1,
                                &info.frameSet, 0, nullptr);

        myengine::shared::ShadowDrawPushConstants spc{};
        spc.drawBuffer = info.drawBufferAddress;
        vkCmdPushConstants(info.cmd, staticLayout_.get(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                            sizeof(spc), &spc);

        const uint32_t stride = static_cast<uint32_t>(sizeof(VkDrawIndexedIndirectCommand));
        for (uint32_t i = 0; i < info.blockRangeCount; ++i) {
            const static_cull::BlockRange& range = info.blockRanges[i];
            if (range.drawCount == 0) continue;
            info.geometry->bindBlock(info.cmd, range.blockIndex);
            indirect_exec::DrawIndexedIndirectCountInfo dii{};
            dii.commandBuffer = info.compactCommandBuffer;
            dii.commandOffset = static_cast<VkDeviceSize>(range.firstDraw) * stride;
            dii.countBuffer   = info.indirectCountBuffer;
            dii.countOffset   = static_cast<VkDeviceSize>(i) * sizeof(uint32_t);
            dii.maxCount      = range.drawCount;
            dii.stride        = stride;
            indirect_exec::recordDrawIndexedIndirectCount(*ctx_, info.cmd, dii);
        }
    }

    // ─── Skinned (Model) 影 ──────────────────────────────────
    if (info.modelDrawList && !info.modelDrawList->empty()) {
        vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skinnedPipeline_.get());
        vkCmdSetViewport(info.cmd, 0, 1, &viewport);
        vkCmdSetScissor(info.cmd, 0, 1, &scissor);
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skinnedLayout_.get(), 0, 1,
                                &info.frameSet, 0, nullptr);

        const Model* curModel = nullptr;
        for (const SkinnedDrawItem& item : *info.modelDrawList) {
            if (!item.sourceModel) continue;
            if (item.sourceModel != curModel) {
                curModel = item.sourceModel;
            }

            SkinnedPushConstants pc{};
            // E: skinned shadow draws compose with the engine-relative
            // light view (camera_system.cpp ships lightVP shifted by
            // -origin), so the model matrix needs the same shift.
            pc.model = myengine::world::toEngineRelative(item.model);
            pc.skinOffset = item.skinOffset;
            pc.skinBuffer = info.skinAddress;
            vkCmdPushConstants(info.cmd, skinnedLayout_.get(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                               sizeof(SkinnedPushConstants), &pc);
            for (const SubMesh& sm : curModel->subMeshes()) {
                if (sm.indexCount == 0) continue;
                sm.bindAndDraw(info.cmd);
            }
        }
    }

    vkCmdEndRendering(info.cmd);

    // PART4 4d: dynamic rendering needs the explicit attachment -> readOnly
    // transition that VkRenderPass used to do via finalLayout. main_pass /
    // bindless shaders later sample target_.view() through their frame
    // descriptor set so we must land in the matching readOnly layout
    // (separate or combined, picked by depth_layouts::readOnly).
    barrier::recordImage(*ctx_, info.cmd, barrier::ImageBarrier{
        .image = target_.image(),
        .range = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1},
        .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        .srcStage  = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        .srcAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstStage  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .dstAccess = VK_ACCESS_2_SHADER_READ_BIT,
    });
}

void ShadowPass::shutdown() {
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;
    skinnedPipeline_.reset();
    skinnedLayout_.reset();
    staticPipeline_.reset();
    staticLayout_.reset();
    target_.shutdown();
    ctx_ = nullptr;
}
