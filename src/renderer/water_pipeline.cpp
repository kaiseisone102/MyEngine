// =============================================================================
// water_pipeline.cpp
// =============================================================================
#include "renderer/water_pipeline.h"

#include <cstddef>
#include <stdexcept>

#include "renderer/shader_util.h"
#include "renderer/vulkan_context.h"
#include "renderer/water_mesh.h"

void WaterPipeline::init(const InitInfo& info) {
    if (!info.ctx) throw std::runtime_error("WaterPipeline::init: ctx is null");
    if (info.renderPass == VK_NULL_HANDLE)
        throw std::runtime_error("WaterPipeline::init: renderPass missing");
    if (info.frameSetLayout == VK_NULL_HANDLE)
        throw std::runtime_error("WaterPipeline::init: frameSetLayout missing");

    ctx_ = info.ctx;

    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(PushConstants);

    // ─── fakeOnly レイアウト (set=0 のみ) ────
    {
        VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        lci.setLayoutCount = 1;
        lci.pSetLayouts = &info.frameSetLayout;
        lci.pushConstantRangeCount = 1;
        lci.pPushConstantRanges = &pcRange;
        VkPipelineLayout lay = VK_NULL_HANDLE;
        if (vkCreatePipelineLayout(ctx_->device(), &lci, nullptr, &lay) != VK_SUCCESS)
            throw std::runtime_error("WaterPipeline: layoutFakeOnly failed");
        layoutFakeOnly_ = VkUnique<VkPipelineLayout>(ctx_->device(), lay);
        pipelineFakeOnly_ = VkUnique<VkPipeline>(
            ctx_->device(),
            buildPipeline(info.renderPass, layoutFakeOnly_.get(), info.shaderDir, "water_frag.spv"));
    }

    // ─── withReflection レイアウト (set=0, set=1) ────
    if (info.reflectionSetLayout != VK_NULL_HANDLE) {
        VkDescriptorSetLayout setLayouts[2] = {info.frameSetLayout, info.reflectionSetLayout};
        VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        lci.setLayoutCount = 2;
        lci.pSetLayouts = setLayouts;
        lci.pushConstantRangeCount = 1;
        lci.pPushConstantRanges = &pcRange;
        VkPipelineLayout lay = VK_NULL_HANDLE;
        if (vkCreatePipelineLayout(ctx_->device(), &lci, nullptr, &lay) != VK_SUCCESS)
            throw std::runtime_error("WaterPipeline: layoutWithReflection failed");
        layoutWithReflection_ = VkUnique<VkPipelineLayout>(ctx_->device(), lay);
        pipelineWithReflection_ = VkUnique<VkPipeline>(
            ctx_->device(), buildPipeline(info.renderPass, layoutWithReflection_.get(),
                                          info.shaderDir, "water_reflect_frag.spv"));
    }
}

VkPipeline WaterPipeline::buildPipeline(VkRenderPass renderPass, VkPipelineLayout layout,
                                          const std::string& shaderDir,
                                          const std::string& fragName) {
    VkShaderModule vert = shader_util::loadShaderModule(ctx_->device(), shaderDir + "water_vert.spv");
    VkShaderModule frag = shader_util::loadShaderModule(ctx_->device(), shaderDir + fragName);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    VkVertexInputBindingDescription bind{0, sizeof(WaterMesh::WaterVertex),
                                          VK_VERTEX_INPUT_RATE_VERTEX};
    // Phase 1B-6: water now has pos + texCoord
    VkVertexInputAttributeDescription attrs[2]{};
    attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(WaterMesh::WaterVertex, pos))};
    attrs[1] = {2, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(WaterMesh::WaterVertex, texCoord))};

    VkPipelineVertexInputStateCreateInfo vi{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &bind;
    vi.vertexAttributeDescriptionCount = 2;
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
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo ms{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_FALSE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo cb{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments = &blend;

    VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynStates;

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
    pci.layout = layout;
    pci.renderPass = renderPass;
    pci.subpass = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    const VkResult r =
        vkCreateGraphicsPipelines(ctx_->device(), VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline);

    vkDestroyShaderModule(ctx_->device(), vert, nullptr);
    vkDestroyShaderModule(ctx_->device(), frag, nullptr);

    if (r != VK_SUCCESS)
        throw std::runtime_error("WaterPipeline: vkCreateGraphicsPipelines failed");
    return pipeline;
}

void WaterPipeline::shutdown() {
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;
    // VkUnique frees each handle (no-op if empty); pipeline before its layout.
    pipelineWithReflection_.reset();
    layoutWithReflection_.reset();
    pipelineFakeOnly_.reset();
    layoutFakeOnly_.reset();
    ctx_ = nullptr;
}
