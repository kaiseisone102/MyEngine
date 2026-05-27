// src/renderer/shadow_pass.cpp
// =============================================================================
// Phase 5-B 段階B-A: 静的 Model (装備品) の影描画を追加
// =============================================================================
#include "renderer/shadow_pass.h"
#include <iostream>

#include <cstddef>
#include <stdexcept>

#include "renderer/mesh.h"
#include "renderer/model.h"
#include "renderer/resource_factory.h"
#include "renderer/shader_util.h"
#include "renderer/vulkan_context.h"

void ShadowPass::init(const InitInfo& info) {
    if (!info.ctx || !info.resources) throw std::runtime_error("ShadowPass::init: invalid info");
    ctx_ = info.ctx;
    extent_ = info.extent;
    depthFormat_ = info.depthFormat;

    createRenderPass();
    createTarget(info.resources);
    createFramebuffer();
    createStaticPipeline(info.frameSetLayout, info.shaderDir);
    createSkinnedPipeline(info.frameSetLayout, info.shaderDir);
}

void ShadowPass::createRenderPass() {
    VkAttachmentDescription depth{};
    depth.format = depthFormat_;
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthRef{0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dep.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    ci.attachmentCount = 1;
    ci.pAttachments = &depth;
    ci.subpassCount = 1;
    ci.pSubpasses = &sub;
    ci.dependencyCount = 1;
    ci.pDependencies = &dep;
    VkRenderPass rp = VK_NULL_HANDLE;
    if (vkCreateRenderPass(ctx_->device(), &ci, nullptr, &rp) != VK_SUCCESS)
        throw std::runtime_error("ShadowPass: vkCreateRenderPass failed");
    renderPass_ = VkUnique<VkRenderPass>(ctx_->device(), rp);
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

void ShadowPass::createFramebuffer() {
    VkImageView view = target_.view();
    VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fi.renderPass = renderPass_.get();
    fi.attachmentCount = 1;
    fi.pAttachments = &view;
    fi.width = extent_.width;
    fi.height = extent_.height;
    fi.layers = 1;
    VkFramebuffer fb = VK_NULL_HANDLE;
    if (vkCreateFramebuffer(ctx_->device(), &fi, nullptr, &fb) != VK_SUCCESS)
        throw std::runtime_error("ShadowPass: vkCreateFramebuffer failed");
    framebuffer_ = VkUnique<VkFramebuffer>(ctx_->device(), fb);
}

namespace {

struct ShadowPipelineParams {
    VkDevice device;
    VkRenderPass renderPass;
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

    VkGraphicsPipelineCreateInfo pci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
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
    pci.renderPass = p.renderPass;
    pci.subpass = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(p.device, VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline) !=
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
    pc.size = sizeof(glm::mat4);

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
        ShadowPipelineParams p{ctx_->device(), renderPass_.get(), staticLayout_.get(), vert, false};
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
        ShadowPipelineParams p{ctx_->device(), renderPass_.get(), skinnedLayout_.get(), vert, true};
        skinnedPipeline_ = VkUnique<VkPipeline>(ctx_->device(), createShadowPipeline(p));
    } catch (...) {
        vkDestroyShaderModule(ctx_->device(), vert, nullptr);
        throw;
    }
    vkDestroyShaderModule(ctx_->device(), vert, nullptr);
}

void ShadowPass::execute(const ExecuteInfo& info) {
    if (!info.cmd) throw std::runtime_error("ShadowPass::execute: invalid cmd");

    VkClearValue clear{};
    clear.depthStencil = {1.0f, 0};
    VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp.renderPass = renderPass_.get();
    rp.framebuffer = framebuffer_.get();
    rp.renderArea = {{0, 0}, extent_};
    rp.clearValueCount = 1;
    rp.pClearValues = &clear;
    vkCmdBeginRenderPass(info.cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{0.f,
                        0.f,
                        static_cast<float>(extent_.width),
                        static_cast<float>(extent_.height),
                        0.f,
                        1.f};
    VkRect2D scissor{{0, 0}, extent_};

    // ─── Static pipeline 共通セットアップ (Mesh + Phase 5-B Static Models) ─
    const bool hasMesh = info.mesh && info.meshDrawList && !info.meshDrawList->empty();
    const bool hasStaticModels =
        info.staticModelDrawList && !info.staticModelDrawList->empty();

    if (hasMesh || hasStaticModels) {
        vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, staticPipeline_.get());
        vkCmdSetViewport(info.cmd, 0, 1, &viewport);
        vkCmdSetScissor(info.cmd, 0, 1, &scissor);
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, staticLayout_.get(), 0, 1,
                                &info.frameSet, 0, nullptr);

        // ── Mesh (cube) 影 ────────────────────────────────────
        if (hasMesh) {
            info.mesh->bind(info.cmd);
            for (const MeshDrawItem& item : *info.meshDrawList) {
                vkCmdPushConstants(info.cmd, staticLayout_.get(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                                   sizeof(glm::mat4), &item.model);
                vkCmdDrawIndexed(info.cmd, info.mesh->indexCount(), 1, 0, 0, 0);
            }
        }

        // ── Phase 5-B: Static Model (装備品) 影 ────────────────
        if (hasStaticModels) {
            const Model* curModel = nullptr;
            for (const StaticModelDrawItem& item : *info.staticModelDrawList) {
                if (!item.sourceModel) continue;
                if (item.sourceModel != curModel) {
                    curModel = item.sourceModel;
                }
                vkCmdPushConstants(info.cmd, staticLayout_.get(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                                   sizeof(glm::mat4), &item.model);
                for (const SubMesh& sm : curModel->subMeshes()) {
                    if (sm.indexCount == 0) continue;
                    sm.bindAndDraw(info.cmd);
                }
            }
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
            pc.model = item.model;
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

    vkCmdEndRenderPass(info.cmd);

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = target_.image();
    barrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(info.cmd, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);
}

void ShadowPass::shutdown() {
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;
    framebuffer_.reset();
    skinnedPipeline_.reset();
    skinnedLayout_.reset();
    staticPipeline_.reset();
    staticLayout_.reset();
    target_.shutdown();
    renderPass_.reset();
    ctx_ = nullptr;
}
