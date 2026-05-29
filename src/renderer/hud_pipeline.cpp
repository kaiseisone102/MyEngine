// =============================================================================
// hud_pipeline.cpp — HUD 矩形描画用 Vulkan pipeline
// =============================================================================
#include "renderer/hud_pipeline.h"

#include <stdexcept>

#include "renderer/shader_util.h"
#include "renderer/vulkan_context.h"

void HudPipeline::init(VulkanContext* ctx, VkFormat colorFormat,
                       const std::string& shaderDir) {
    ctx_ = ctx;
    colorFormat_ = colorFormat;
    createLayout();
    createPipeline(shaderDir);
}

void HudPipeline::shutdown() {
    // VkUnique frees each handle (no-op if empty). pipeline before layout.
    pipeline_.reset();
    layout_.reset();
    ctx_ = nullptr;
}

void HudPipeline::createLayout() {
    // PushConstants 40 bytes、 vert + frag 共通
    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    ci.setLayoutCount = 0;
    ci.pSetLayouts = nullptr;
    ci.pushConstantRangeCount = 1;
    ci.pPushConstantRanges = &pcRange;

    VkPipelineLayout lay = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(ctx_->device(), &ci, nullptr, &lay) != VK_SUCCESS) {
        throw std::runtime_error("HudPipeline: vkCreatePipelineLayout failed");
    }
    layout_ = VkUnique<VkPipelineLayout>(ctx_->device(), lay);
}

void HudPipeline::createPipeline(const std::string& shaderDir) {
    VkShaderModule vert = shader_util::loadShaderModule(ctx_->device(), shaderDir + "hud_vert.spv");
    VkShaderModule frag = shader_util::loadShaderModule(ctx_->device(), shaderDir + "hud_frag.spv");

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    // 頂点入力なし (gl_VertexIndex で 4 頂点生成)
    VkPipelineVertexInputStateCreateInfo vertexInput{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    VkPipelineInputAssemblyStateCreateInfo inputAsm{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAsm.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineViewportStateCreateInfo viewport{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rast{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rast.polygonMode = VK_POLYGON_MODE_FILL;
    rast.cullMode = VK_CULL_MODE_NONE;
    rast.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rast.lineWidth = 1.f;

    VkPipelineMultisampleStateCreateInfo ms{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // depth: テスト・書き込みとも無効 (HUD は常に手前)
    VkPipelineDepthStencilStateCreateInfo depth{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depth.depthTestEnable = VK_FALSE;
    depth.depthWriteEnable = VK_FALSE;

    // アルファブレンド有効
    VkPipelineColorBlendAttachmentState att{};
    att.blendEnable = VK_TRUE;
    att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    att.colorBlendOp = VK_BLEND_OP_ADD;
    att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    att.alphaBlendOp = VK_BLEND_OP_ADD;
    att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo blend{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend.attachmentCount = 1;
    blend.pAttachments = &att;

    VkDynamicState dynStates[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynStates;

    // PART4 4a-1 / 4a-2: dynamic rendering, color-only. HUD draws after
    // main_pass into the HDR target (OverlayPass scope). depthTestEnable is
    // already FALSE here so we declare no depth slot - the BeginRendering
    // scope is color-only, which is the modern Vulkan idiom for UI overlays.
    VkFormat colorFormats[1] = {colorFormat_};
    VkPipelineRenderingCreateInfo rci{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rci.colorAttachmentCount = 1;
    rci.pColorAttachmentFormats = colorFormats;
    rci.depthAttachmentFormat = VK_FORMAT_UNDEFINED;

    VkGraphicsPipelineCreateInfo pi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pi.pNext = &rci;
    pi.stageCount = 2;
    pi.pStages = stages;
    pi.pVertexInputState = &vertexInput;
    pi.pInputAssemblyState = &inputAsm;
    pi.pViewportState = &viewport;
    pi.pRasterizationState = &rast;
    pi.pMultisampleState = &ms;
    pi.pDepthStencilState = &depth;
    pi.pColorBlendState = &blend;
    pi.pDynamicState = &dyn;
    pi.layout = layout_.get();
    pi.renderPass = VK_NULL_HANDLE;

    VkPipeline pipe = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(ctx_->device(), ctx_->pipelineCache(), 1, &pi, nullptr, &pipe) !=
        VK_SUCCESS) {
        throw std::runtime_error("HudPipeline: vkCreateGraphicsPipelines failed");
    }
    pipeline_ = VkUnique<VkPipeline>(ctx_->device(), pipe);

    vkDestroyShaderModule(ctx_->device(), vert, nullptr);
    vkDestroyShaderModule(ctx_->device(), frag, nullptr);
}
