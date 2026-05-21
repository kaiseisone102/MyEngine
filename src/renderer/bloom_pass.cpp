// src/renderer/bloom_pass.cpp
// =============================================================================
// bloom_pass.cpp - Phase 1I: HDR bloom (bright extract + separable Gaussian)
// =============================================================================
#include "renderer/bloom_pass.h"

#include <array>
#include <stdexcept>

#include "renderer/shader_util.h"
#include "renderer/vulkan_context.h"

void BloomPass::init(const InitInfo& info) {
    if (!info.ctx) throw std::runtime_error("BloomPass::init: ctx is null");
    if (info.hdrColorView == VK_NULL_HANDLE) throw std::runtime_error("BloomPass::init: hdrColorView is null");
    if (info.targetAView == VK_NULL_HANDLE || info.targetBView == VK_NULL_HANDLE)
        throw std::runtime_error("BloomPass::init: bloom target views are null");
    if (info.bloomFormat == VK_FORMAT_UNDEFINED) throw std::runtime_error("BloomPass::init: bloomFormat undefined");
    if (info.width == 0 || info.height == 0) throw std::runtime_error("BloomPass::init: zero extent");

    ctx_ = info.ctx;
    hdrColorView_ = info.hdrColorView;
    hdrColorSampler_ = info.hdrColorSampler;
    targetAView_ = info.targetAView;
    targetASampler_ = info.targetASampler;
    targetBView_ = info.targetBView;
    targetBSampler_ = info.targetBSampler;
    bloomFormat_ = info.bloomFormat;
    width_ = info.width;
    height_ = info.height;
    shaderDir_ = info.shaderDir;

    createRenderPass();
    createDescriptorSetLayout();
    createDescriptorPool();
    allocateDescriptorSets();
    updateDescriptorSets();
    createPipelineLayout();
    createPipelines(shaderDir_);
    createFramebuffers();
}

void BloomPass::shutdown() {
    if (!ctx_) return;
    VkDevice dev = ctx_->device();
    destroyFramebuffers();
    destroyPipelines();
    if (pipelineLayout_ != VK_NULL_HANDLE) { vkDestroyPipelineLayout(dev, pipelineLayout_, nullptr); pipelineLayout_ = VK_NULL_HANDLE; }
    if (descPool_ != VK_NULL_HANDLE) { vkDestroyDescriptorPool(dev, descPool_, nullptr); descPool_ = VK_NULL_HANDLE; }
    if (descSetLayout_ != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(dev, descSetLayout_, nullptr); descSetLayout_ = VK_NULL_HANDLE; }
    if (renderPass_ != VK_NULL_HANDLE) { vkDestroyRenderPass(dev, renderPass_, nullptr); renderPass_ = VK_NULL_HANDLE; }
    descHdr_ = descA_ = descB_ = VK_NULL_HANDLE;
}

void BloomPass::onSwapchainResized(const InitInfo& info) {
    // Resources were recreated by the renderer; refresh views + extent and
    // rebuild framebuffers + descriptor sets. Render pass / pipelines / layout
    // are format-only dependent and can be kept (format unchanged).
    hdrColorView_ = info.hdrColorView;
    hdrColorSampler_ = info.hdrColorSampler;
    targetAView_ = info.targetAView;
    targetASampler_ = info.targetASampler;
    targetBView_ = info.targetBView;
    targetBSampler_ = info.targetBSampler;
    width_ = info.width;
    height_ = info.height;

    destroyFramebuffers();
    updateDescriptorSets();
    createFramebuffers();
}

void BloomPass::createRenderPass() {
    // One color attachment in the bloom format. We CLEAR on load (we fully
    // overwrite each target) and leave it SHADER_READ_ONLY so the next pass
    // (or PostPass) can sample it.
    VkAttachmentDescription color{};
    color.format = bloomFormat_;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &colorRef;

    // Two dependencies: wait for prior sampling to finish before we write,
    // and make our writes visible to the next fragment-shader sampling.
    std::array<VkSubpassDependency, 2> deps{};
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;
    deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    ci.attachmentCount = 1;
    ci.pAttachments = &color;
    ci.subpassCount = 1;
    ci.pSubpasses = &sub;
    ci.dependencyCount = static_cast<uint32_t>(deps.size());
    ci.pDependencies = deps.data();

    if (vkCreateRenderPass(ctx_->device(), &ci, nullptr, &renderPass_) != VK_SUCCESS)
        throw std::runtime_error("BloomPass: vkCreateRenderPass failed");
}

void BloomPass::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding b{};
    b.binding = 0;
    b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b.descriptorCount = 1;
    b.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    ci.bindingCount = 1;
    ci.pBindings = &b;

    if (vkCreateDescriptorSetLayout(ctx_->device(), &ci, nullptr, &descSetLayout_) != VK_SUCCESS)
        throw std::runtime_error("BloomPass: vkCreateDescriptorSetLayout failed");
}

void BloomPass::createDescriptorPool() {
    VkDescriptorPoolSize ps{};
    ps.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ps.descriptorCount = 3;  // HDR, A, B

    VkDescriptorPoolCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    ci.maxSets = 3;
    ci.poolSizeCount = 1;
    ci.pPoolSizes = &ps;

    if (vkCreateDescriptorPool(ctx_->device(), &ci, nullptr, &descPool_) != VK_SUCCESS)
        throw std::runtime_error("BloomPass: vkCreateDescriptorPool failed");
}

void BloomPass::allocateDescriptorSets() {
    std::array<VkDescriptorSetLayout, 3> layouts{descSetLayout_, descSetLayout_, descSetLayout_};
    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = descPool_;
    ai.descriptorSetCount = 3;
    ai.pSetLayouts = layouts.data();

    std::array<VkDescriptorSet, 3> sets{};
    if (vkAllocateDescriptorSets(ctx_->device(), &ai, sets.data()) != VK_SUCCESS)
        throw std::runtime_error("BloomPass: vkAllocateDescriptorSets failed");
    descHdr_ = sets[0];
    descA_ = sets[1];
    descB_ = sets[2];
}

void BloomPass::updateDescriptorSets() {
    auto writeOne = [&](VkDescriptorSet set, VkImageView view, VkSampler samp) {
        VkDescriptorImageInfo ii{};
        ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        ii.imageView = view;
        ii.sampler = samp;
        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet = set;
        w.dstBinding = 0;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.pImageInfo = &ii;
        vkUpdateDescriptorSets(ctx_->device(), 1, &w, 0, nullptr);
    };
    writeOne(descHdr_, hdrColorView_, hdrColorSampler_);
    writeOne(descA_, targetAView_, targetASampler_);
    writeOne(descB_, targetBView_, targetBSampler_);
}

void BloomPass::createPipelineLayout() {
    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    li.setLayoutCount = 1;
    li.pSetLayouts = &descSetLayout_;
    li.pushConstantRangeCount = 1;
    li.pPushConstantRanges = &pcRange;

    if (vkCreatePipelineLayout(ctx_->device(), &li, nullptr, &pipelineLayout_) != VK_SUCCESS)
        throw std::runtime_error("BloomPass: vkCreatePipelineLayout failed");
}

void BloomPass::createPipelines(const std::string& shaderDir) {
    VkShaderModule vert = shader_util::loadShaderModule(ctx_->device(), shaderDir + "/post_fullscreen_vert.spv");
    VkShaderModule brightFrag = shader_util::loadShaderModule(ctx_->device(), shaderDir + "/bloom_bright_frag.spv");
    VkShaderModule blurFrag = shader_util::loadShaderModule(ctx_->device(), shaderDir + "/bloom_blur_frag.spv");

    // Shared fixed-function state.
    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

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

    auto buildPipeline = [&](VkShaderModule frag, VkPipeline& outPipe) {
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vert;
        stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = frag;
        stages[1].pName = "main";

        VkGraphicsPipelineCreateInfo pci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
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
        pci.layout = pipelineLayout_;
        pci.renderPass = renderPass_;
        pci.subpass = 0;

        if (vkCreateGraphicsPipelines(ctx_->device(), VK_NULL_HANDLE, 1, &pci, nullptr, &outPipe) != VK_SUCCESS)
            throw std::runtime_error("BloomPass: vkCreateGraphicsPipelines failed");
    };

    buildPipeline(brightFrag, pipelineBright_);
    buildPipeline(blurFrag, pipelineBlur_);

    vkDestroyShaderModule(ctx_->device(), blurFrag, nullptr);
    vkDestroyShaderModule(ctx_->device(), brightFrag, nullptr);
    vkDestroyShaderModule(ctx_->device(), vert, nullptr);
}

void BloomPass::createFramebuffers() {
    auto makeFb = [&](VkImageView view, VkFramebuffer& outFb) {
        VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fi.renderPass = renderPass_;
        fi.attachmentCount = 1;
        fi.pAttachments = &view;
        fi.width = width_;
        fi.height = height_;
        fi.layers = 1;
        if (vkCreateFramebuffer(ctx_->device(), &fi, nullptr, &outFb) != VK_SUCCESS)
            throw std::runtime_error("BloomPass: vkCreateFramebuffer failed");
    };
    makeFb(targetAView_, fbA_);
    makeFb(targetBView_, fbB_);
}

void BloomPass::destroyFramebuffers() {
    VkDevice dev = ctx_->device();
    if (fbA_ != VK_NULL_HANDLE) { vkDestroyFramebuffer(dev, fbA_, nullptr); fbA_ = VK_NULL_HANDLE; }
    if (fbB_ != VK_NULL_HANDLE) { vkDestroyFramebuffer(dev, fbB_, nullptr); fbB_ = VK_NULL_HANDLE; }
}

void BloomPass::destroyPipelines() {
    VkDevice dev = ctx_->device();
    if (pipelineBright_ != VK_NULL_HANDLE) { vkDestroyPipeline(dev, pipelineBright_, nullptr); pipelineBright_ = VK_NULL_HANDLE; }
    if (pipelineBlur_ != VK_NULL_HANDLE) { vkDestroyPipeline(dev, pipelineBlur_, nullptr); pipelineBlur_ = VK_NULL_HANDLE; }
}

void BloomPass::recordDraw(VkCommandBuffer cmd, VkFramebuffer fb, VkPipeline pipe,
                           VkDescriptorSet set, const PushConstants& pc) {
    VkClearValue clear{};
    clear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp.renderPass = renderPass_;
    rp.framebuffer = fb;
    rp.renderArea = {{0, 0}, {width_, height_}};
    rp.clearValueCount = 1;
    rp.pClearValues = &clear;

    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(width_);
    viewport.height = static_cast<float>(height_);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, {width_, height_}};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &set, 0, nullptr);
    vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pc);

    vkCmdDraw(cmd, 3, 1, 0, 0);  // fullscreen triangle
    vkCmdEndRenderPass(cmd);
}

void BloomPass::execute(const ExecuteInfo& info) {
    if (!info.cmd) throw std::runtime_error("BloomPass::execute: invalid cmd");

    PushConstants pc{};
    pc.threshold = threshold_;
    pc.softKnee = softKnee_;
    pc.intensity = 1.0f;

    // 1) bright extract: HDR -> targetA
    pc.texelDir = 0.0f;
    recordDraw(info.cmd, fbA_, pipelineBright_, descHdr_, pc);

    // 2) horizontal blur: targetA -> targetB
    pc.texelDir = 0.0f;
    recordDraw(info.cmd, fbB_, pipelineBlur_, descA_, pc);

    // 3) vertical blur: targetB -> targetA  (result = bloom)
    pc.texelDir = 1.0f;
    recordDraw(info.cmd, fbA_, pipelineBlur_, descB_, pc);
}
