// src/renderer/post_pass.cpp
// =============================================================================
// post_pass.cpp - Phase 1H-3: HDR -> swapchain tonemap (PART4 4d: dynamic
// rendering migration; VkRenderPass + VkFramebuffer removed)
// =============================================================================
#include "renderer/post_pass.h"

#include <array>
#include <stdexcept>

#include "renderer/barrier.h"
#include "renderer/shader_util.h"
#include "renderer/swapchain.h"
#include "renderer/vulkan_context.h"

void PostPass::init(const InitInfo& info) {
    if (!info.ctx) throw std::runtime_error("PostPass::init: ctx is null");
    if (!info.swapchain) throw std::runtime_error("PostPass::init: swapchain is null");
    if (info.hdrColorView == VK_NULL_HANDLE) throw std::runtime_error("PostPass::init: hdrColorView is null");
    if (info.hdrColorSampler == VK_NULL_HANDLE) throw std::runtime_error("PostPass::init: hdrColorSampler is null");

    ctx_ = info.ctx;
    swapchain_ = info.swapchain;
    hdrColorView_ = info.hdrColorView;
    hdrColorSampler_ = info.hdrColorSampler;
    bloomColorView_ = info.bloomColorView;
    bloomColorSampler_ = info.bloomColorSampler;
    shaderDir_ = info.shaderDir;

    createDescriptorSetLayout();
    createDescriptorPool();
    allocateAndUpdateDescriptorSet();
    createPipelineLayout();
    createPipeline(shaderDir_);
}

void PostPass::shutdown() {
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;

    destroyPipeline();

    pipelineLayout_.reset();
    descPool_.reset();
    descSet_ = VK_NULL_HANDLE;  // freed with pool
    descSetLayout_.reset();

    ctx_ = nullptr;
    swapchain_ = nullptr;
    hdrColorView_ = VK_NULL_HANDLE;
    hdrColorSampler_ = VK_NULL_HANDLE;
}

void PostPass::onSwapchainResized(VkImageView hdrColorView, VkSampler hdrColorSampler,
                                  VkImageView bloomColorView, VkSampler bloomColorSampler) {
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;

    // Update cached HDR resources (may have new view after HDR target recreate)
    if (hdrColorView != VK_NULL_HANDLE) hdrColorView_ = hdrColorView;
    if (hdrColorSampler != VK_NULL_HANDLE) hdrColorSampler_ = hdrColorSampler;
    if (bloomColorView != VK_NULL_HANDLE) bloomColorView_ = bloomColorView;
    if (bloomColorSampler != VK_NULL_HANDLE) bloomColorSampler_ = bloomColorSampler;

    // Re-record descriptor set with new HDR view. PART4 4d: no framebuffers
    // to re-create now that we're on dynamic rendering - the per-image
    // colorView is fetched fresh in execute() from the swapchain.
    allocateAndUpdateDescriptorSet();
}

void PostPass::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding b[2]{};
    b[0].binding = 0;  // hdrColor
    b[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b[0].descriptorCount = 1;
    b[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    b[1].binding = 1;  // bloomColor (Phase 1I)
    b[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b[1].descriptorCount = 1;
    b[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    ci.bindingCount = 2;
    ci.pBindings = b;

    VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(ctx_->device(), &ci, nullptr, &dsl) != VK_SUCCESS)
        throw std::runtime_error("PostPass: vkCreateDescriptorSetLayout failed");
    descSetLayout_ = VkUnique<VkDescriptorSetLayout>(ctx_->device(), dsl);
}

void PostPass::createDescriptorPool() {
    VkDescriptorPoolSize sz{};
    sz.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sz.descriptorCount = 2;  // hdr + bloom

    VkDescriptorPoolCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    ci.poolSizeCount = 1;
    ci.pPoolSizes = &sz;
    ci.maxSets = 1;
    // FREE_DESCRIPTOR_SET so we can re-allocate on resize
    ci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    VkDescriptorPool pool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(ctx_->device(), &ci, nullptr, &pool) != VK_SUCCESS)
        throw std::runtime_error("PostPass: vkCreateDescriptorPool failed");
    descPool_ = VkUnique<VkDescriptorPool>(ctx_->device(), pool);
}

void PostPass::allocateAndUpdateDescriptorSet() {
    // Free previous set if any (resize case)
    if (descSet_ != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(ctx_->device(), descPool_.get(), 1, &descSet_);
        descSet_ = VK_NULL_HANDLE;
    }

    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = descPool_.get();
    ai.descriptorSetCount = 1;
    VkDescriptorSetLayout dslHandle = descSetLayout_.get();
    ai.pSetLayouts = &dslHandle;
    if (vkAllocateDescriptorSets(ctx_->device(), &ai, &descSet_) != VK_SUCCESS)
        throw std::runtime_error("PostPass: vkAllocateDescriptorSets failed");

    VkDescriptorImageInfo hdrInfo{};
    hdrInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    hdrInfo.imageView = hdrColorView_;
    hdrInfo.sampler = hdrColorSampler_;
    VkDescriptorImageInfo bloomInfo{};
    bloomInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    bloomInfo.imageView = bloomColorView_;
    bloomInfo.sampler = bloomColorSampler_;

    VkWriteDescriptorSet writes[2]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = descSet_;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo = &hdrInfo;
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = descSet_;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = &bloomInfo;

    vkUpdateDescriptorSets(ctx_->device(), 2, writes, 0, nullptr);
}

void PostPass::createPipelineLayout() {
    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(int) + sizeof(float);

    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    li.setLayoutCount = 1;
    VkDescriptorSetLayout liLayout = descSetLayout_.get();
    li.pSetLayouts = &liLayout;
    li.pushConstantRangeCount = 1;
    li.pPushConstantRanges = &pcRange;

    VkPipelineLayout playout = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(ctx_->device(), &li, nullptr, &playout) != VK_SUCCESS)
        throw std::runtime_error("PostPass: vkCreatePipelineLayout failed");
    pipelineLayout_ = VkUnique<VkPipelineLayout>(ctx_->device(), playout);
}

void PostPass::createPipeline(const std::string& shaderDir) {
    VkShaderModule vert = shader_util::loadShaderModule(ctx_->device(), shaderDir + "/post_fullscreen_vert.spv");
    VkShaderModule frag = shader_util::loadShaderModule(ctx_->device(), shaderDir + "/post_tonemap_frag.spv");

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    // No vertex input (fullscreen triangle uses gl_VertexIndex)
    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Dynamic viewport / scissor
    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cba.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;

    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;

    VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynStates;

    // PART4 4d: dynamic rendering. Declare the attachment format(s) the
    // pipeline writes to via VkPipelineRenderingCreateInfo chained into the
    // GraphicsPipelineCreateInfo's pNext, instead of the old VkRenderPass +
    // subpass pointers.
    const VkFormat swapchainFormat = swapchain_->colorFormat();
    VkPipelineRenderingCreateInfo rci{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rci.colorAttachmentCount = 1;
    rci.pColorAttachmentFormats = &swapchainFormat;
    rci.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    rci.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    VkGraphicsPipelineCreateInfo pci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pci.pNext = &rci;
    pci.stageCount = 2;
    pci.pStages = stages;
    pci.pVertexInputState = &vi;
    pci.pInputAssemblyState = &ia;
    pci.pViewportState = &vp;
    pci.pRasterizationState = &rs;
    pci.pMultisampleState = &ms;
    pci.pColorBlendState = &cb;
    pci.pDepthStencilState = &ds;
    pci.pDynamicState = &dyn;
    pci.layout = pipelineLayout_.get();
    pci.renderPass = VK_NULL_HANDLE;  // PART4 4d: dynamic rendering, no VkRenderPass
    pci.subpass = 0;

    VkPipeline pipe = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(ctx_->device(), VK_NULL_HANDLE, 1, &pci, nullptr, &pipe) != VK_SUCCESS) {
        vkDestroyShaderModule(ctx_->device(), frag, nullptr);
        vkDestroyShaderModule(ctx_->device(), vert, nullptr);
        throw std::runtime_error("PostPass: vkCreateGraphicsPipelines failed");
    }

    vkDestroyShaderModule(ctx_->device(), frag, nullptr);
    vkDestroyShaderModule(ctx_->device(), vert, nullptr);
    pipeline_ = VkUnique<VkPipeline>(ctx_->device(), pipe);
}

void PostPass::destroyPipeline() {
    pipeline_.reset();
}

void PostPass::execute(const ExecuteInfo& info) {
    if (!info.cmd) throw std::runtime_error("PostPass::execute: invalid cmd");
    if (info.imageIndex >= swapchain_->imageCount())
        throw std::runtime_error("PostPass::execute: imageIndex out of range");

    const VkExtent2D extent = swapchain_->extent();

    // PART4 4d: dynamic rendering. The swapchain image must be in
    // COLOR_ATTACHMENT_OPTIMAL on entry to vkCmdBeginRendering; the legacy
    // VkRenderPass did this via initialLayout=UNDEFINED -> COLOR_ATTACHMENT.
    // We discard previous contents (oldLayout = UNDEFINED) since loadOp =
    // CLEAR will overwrite the whole image anyway.
    barrier::recordImage(*ctx_, info.cmd, barrier::ImageBarrier{
        .image = swapchain_->colorImage(info.imageIndex),
        .range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcStage  = VK_PIPELINE_STAGE_2_NONE,
        .srcAccess = VK_ACCESS_2_NONE,
        .dstStage  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    });

    VkRenderingAttachmentInfo colorAtt{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAtt.imageView = swapchain_->colorView(info.imageIndex);
    colorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderingInfo rp{VK_STRUCTURE_TYPE_RENDERING_INFO};
    rp.renderArea = {{0, 0}, extent};
    rp.layerCount = 1;
    rp.colorAttachmentCount = 1;
    rp.pColorAttachments = &colorAtt;

    vkCmdBeginRendering(info.cmd, &rp);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(info.cmd, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, extent};
    vkCmdSetScissor(info.cmd, 0, 1, &scissor);

    vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.get());
    vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_.get(), 0, 1, &descSet_, 0, nullptr);

    // Phase 1H-4: push tonemapper mode + exposure
    struct { int mode; float exposure; } pc{tonemapMode_, exposure_};
    vkCmdPushConstants(info.cmd, pipelineLayout_.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

    // Fullscreen triangle (3 vertices, no vertex buffer)
    vkCmdDraw(info.cmd, 3, 1, 0, 0);

    vkCmdEndRendering(info.cmd);

    // PART4 4d: swapchain -> PRESENT_SRC_KHR for vkQueuePresentKHR. Legacy
    // VkRenderPass did this via finalLayout=PRESENT_SRC_KHR.
    barrier::recordImage(*ctx_, info.cmd, barrier::ImageBarrier{
        .image = swapchain_->colorImage(info.imageIndex),
        .range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcStage  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStage  = VK_PIPELINE_STAGE_2_NONE,
        .dstAccess = VK_ACCESS_2_NONE,
    });
}
