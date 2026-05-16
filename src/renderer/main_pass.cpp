// src/renderer/main_pass.cpp
#include "renderer/main_pass.h"

#include <cstddef>
#include <stdexcept>

#include "renderer/imgui_layer.h"
#include "renderer/material.h"
#include "renderer/mesh.h"
#include "renderer/model.h"
#include "renderer/shader_util.h"
#include "renderer/swapchain.h"
#include "renderer/vulkan_context.h"

void MainPass::init(const InitInfo& info) {
    if (!info.ctx || !info.swapchain) throw std::runtime_error("MainPass::init: invalid info");
    ctx_ = info.ctx;
    swapchain_ = info.swapchain;

    createRenderPass();
    createPipeline(info.frameSetLayout, info.materialSetLayout, info.shaderDir);
    createFramebuffers();
}

void MainPass::createRenderPass() {
    VkAttachmentDescription color{};
    color.format = swapchain_->colorFormat();
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depth{};
    depth.format = swapchain_->depthFormat();
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &colorRef;
    sub.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[] = {color, depth};
    VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    ci.attachmentCount = 2;
    ci.pAttachments = attachments;
    ci.subpassCount = 1;
    ci.pSubpasses = &sub;
    ci.dependencyCount = 1;
    ci.pDependencies = &dep;
    if (vkCreateRenderPass(ctx_->device(), &ci, nullptr, &renderPass_) != VK_SUCCESS)
        throw std::runtime_error("MainPass: vkCreateRenderPass failed");
}

void MainPass::createPipeline(VkDescriptorSetLayout frameSetLayout,
                              VkDescriptorSetLayout materialSetLayout,
                              const std::string& shaderDir) {
    if (frameSetLayout == VK_NULL_HANDLE || materialSetLayout == VK_NULL_HANDLE)
        throw std::runtime_error("MainPass::createPipeline: set layout missing");

    VkShaderModule vert =
        shader_util::loadShaderModule(ctx_->device(), shaderDir + "triangle_vert.spv");
    VkShaderModule frag =
        shader_util::loadShaderModule(ctx_->device(), shaderDir + "triangle_frag.spv");

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    // ─── 頂点入力 ────────────────────────────────────────────────
    // Phase 2 段階A: jointIndices (location=4) と jointWeights (location=5) を追加。
    // シェーダー側は段階F まで読み取らないが、attribute は今のうちに通しておく。
    VkVertexInputBindingDescription bind{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[6]{};
    attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, pos))};
    attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, color))};
    attrs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, texCoord))};
    attrs[3] = {3, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, normal))};
    attrs[4] = {4, 0, VK_FORMAT_R32G32B32A32_SINT,
                static_cast<uint32_t>(offsetof(Vertex, jointIndices))};
    attrs[5] = {5, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                static_cast<uint32_t>(offsetof(Vertex, jointWeights))};

    VkPipelineVertexInputStateCreateInfo vi{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &bind;
    vi.vertexAttributeDescriptionCount = 6;
    vi.pVertexAttributeDescriptions = attrs;

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
    rs.cullMode = VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo ms{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState blendAtt{};
    blendAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments = &blendAtt;

    VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynStates;

    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pc.offset = 0;
    pc.size = sizeof(glm::mat4);

    VkDescriptorSetLayout setLayouts[2] = {frameSetLayout, materialSetLayout};
    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    lci.setLayoutCount = 2;
    lci.pSetLayouts = setLayouts;
    lci.pushConstantRangeCount = 1;
    lci.pPushConstantRanges = &pc;
    if (vkCreatePipelineLayout(ctx_->device(), &lci, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        vkDestroyShaderModule(ctx_->device(), vert, nullptr);
        vkDestroyShaderModule(ctx_->device(), frag, nullptr);
        throw std::runtime_error("MainPass: vkCreatePipelineLayout failed");
    }

    VkGraphicsPipelineCreateInfo pci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pci.stageCount = 2;
    pci.pStages = stages;
    pci.pVertexInputState = &vi;
    pci.pInputAssemblyState = &ia;
    pci.pViewportState = &vp;
    pci.pRasterizationState = &rs;
    pci.pMultisampleState = &ms;
    pci.pDepthStencilState = &ds;
    pci.pColorBlendState = &cb;
    pci.pDynamicState = &dyn;
    pci.layout = pipelineLayout_;
    pci.renderPass = renderPass_;
    pci.subpass = 0;
    if (vkCreateGraphicsPipelines(ctx_->device(), VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline_) !=
        VK_SUCCESS) {
        vkDestroyShaderModule(ctx_->device(), vert, nullptr);
        vkDestroyShaderModule(ctx_->device(), frag, nullptr);
        throw std::runtime_error("MainPass: vkCreateGraphicsPipelines failed");
    }
    vkDestroyShaderModule(ctx_->device(), vert, nullptr);
    vkDestroyShaderModule(ctx_->device(), frag, nullptr);
}

void MainPass::createFramebuffers() {
    const uint32_t count = swapchain_->imageCount();
    const VkExtent2D extent = swapchain_->extent();
    framebuffers_.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        VkImageView attachments[] = {swapchain_->colorView(i), swapchain_->depthView()};
        VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        ci.renderPass = renderPass_;
        ci.attachmentCount = 2;
        ci.pAttachments = attachments;
        ci.width = extent.width;
        ci.height = extent.height;
        ci.layers = 1;
        if (vkCreateFramebuffer(ctx_->device(), &ci, nullptr, &framebuffers_[i]) != VK_SUCCESS)
            throw std::runtime_error("MainPass: vkCreateFramebuffer failed");
    }
}

void MainPass::destroyFramebuffers() {
    if (!ctx_) return;
    for (auto fb : framebuffers_) {
        if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(ctx_->device(), fb, nullptr);
    }
    framebuffers_.clear();
}

void MainPass::onSwapchainResized() {
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;
    destroyFramebuffers();
    createFramebuffers();
}

void MainPass::execute(const ExecuteInfo& info) {
    if (!info.cmd) throw std::runtime_error("MainPass::execute: invalid cmd");
    if (info.imageIndex >= framebuffers_.size())
        throw std::runtime_error("MainPass::execute: imageIndex out of range");
    if (info.defaultMaterialSet == VK_NULL_HANDLE)
        throw std::runtime_error("MainPass::execute: defaultMaterialSet missing");

    const VkExtent2D extent = swapchain_->extent();

    VkClearValue clearValues[2]{};
    clearValues[0].color = {
        {info.clearColor.r, info.clearColor.g, info.clearColor.b, info.clearColor.a}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp.renderPass = renderPass_;
    rp.framebuffer = framebuffers_[info.imageIndex];
    rp.renderArea = {{0, 0}, extent};
    rp.clearValueCount = 2;
    rp.pClearValues = clearValues;

    vkCmdBeginRenderPass(info.cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    VkViewport viewport{
        0.f, 0.f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.f, 1.f};
    vkCmdSetViewport(info.cmd, 0, 1, &viewport);
    VkRect2D scissor{{0, 0}, extent};
    vkCmdSetScissor(info.cmd, 0, 1, &scissor);

    vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1,
                            &info.frameSet, 0, nullptr);

    if (info.mesh && info.meshDrawList && !info.meshDrawList->empty()) {
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 1, 1,
                                &info.defaultMaterialSet, 0, nullptr);
        info.mesh->bind(info.cmd);
        for (const glm::mat4& model : *info.meshDrawList) {
            vkCmdPushConstants(info.cmd, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                               sizeof(glm::mat4), &model);
            vkCmdDrawIndexed(info.cmd, info.mesh->indexCount(), 1, 0, 0, 0);
        }
    }

    if (info.model && info.modelDrawList && !info.modelDrawList->empty()) {
        const auto& materials = info.model->materials();
        for (const glm::mat4& model : *info.modelDrawList) {
            vkCmdPushConstants(info.cmd, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                               sizeof(glm::mat4), &model);
            for (const SubMesh& sm : info.model->subMeshes()) {
                if (sm.indexCount == 0) continue;
                VkDescriptorSet matSet = info.defaultMaterialSet;
                if (sm.materialIndex < materials.size()) {
                    VkDescriptorSet ms = materials[sm.materialIndex].descriptorSet();
                    if (ms != VK_NULL_HANDLE) matSet = ms;
                }
                vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_,
                                        1, 1, &matSet, 0, nullptr);
                sm.bind(info.cmd);
                vkCmdDrawIndexed(info.cmd, sm.indexCount, 1, 0, 0, 0);
            }
        }
    }

    if (info.imgui) info.imgui->recordDrawCommands(info.cmd);

    vkCmdEndRenderPass(info.cmd);
}

void MainPass::shutdown() {
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;
    destroyFramebuffers();
    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(ctx_->device(), pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(ctx_->device(), pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
    if (renderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(ctx_->device(), renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }
    ctx_ = nullptr;
    swapchain_ = nullptr;
}
