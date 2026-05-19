// =============================================================================
// debug_line_pass.cpp — デバッグ線描画 Pass の実装
// =============================================================================
// MainPass::buildPipeline の構造を踏襲しつつ、 デバッグ用の特殊設定を加える:
//   - vertex input: DebugLineVertex (pos + color)
//   - topology は線/三角形を切り替えるため buildPipeline の引数で受ける
//   - cullMode = NONE, depthWrite = FALSE
//   - アルファブレンド有効
// =============================================================================
#include "renderer/debug_line_pass.h"

#include <cstddef>
#include <cstring>
#include <stdexcept>

#include "renderer/resource_factory.h"
#include "renderer/shader_util.h"
#include "renderer/swapchain.h"
#include "renderer/vulkan_context.h"

void DebugLinePass::init(const InitInfo& info) {
    if (!info.ctx || !info.resources || !info.swapchain)
        throw std::runtime_error("DebugLinePass::init: invalid info");
    if (info.mainRenderPass == VK_NULL_HANDLE || info.frameSetLayout == VK_NULL_HANDLE)
        throw std::runtime_error("DebugLinePass::init: missing renderPass/frameSetLayout");

    ctx_ = info.ctx;
    resources_ = info.resources;
    swapchain_ = info.swapchain;

    createLayout(info.frameSetLayout);
    createPipelines(info.mainRenderPass, info.shaderDir);
    createVertexBuffers();
}

void DebugLinePass::createLayout(VkDescriptorSetLayout frameSetLayout) {
    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    lci.setLayoutCount = 1;
    lci.pSetLayouts = &frameSetLayout;
    lci.pushConstantRangeCount = 0;
    lci.pPushConstantRanges = nullptr;
    if (vkCreatePipelineLayout(ctx_->device(), &lci, nullptr, &layout_) != VK_SUCCESS) {
        throw std::runtime_error("DebugLinePass: vkCreatePipelineLayout failed");
    }
}

void DebugLinePass::createPipelines(VkRenderPass renderPass, const std::string& shaderDir) {
    linePipeline_ = buildPipeline(renderPass, shaderDir, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
    triPipeline_  = buildPipeline(renderPass, shaderDir, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
}

VkPipeline DebugLinePass::buildPipeline(VkRenderPass renderPass, const std::string& shaderDir,
                                          VkPrimitiveTopology topology) {
    VkShaderModule vert =
        shader_util::loadShaderModule(ctx_->device(), shaderDir + "debug_line_vert.spv");
    VkShaderModule frag =
        shader_util::loadShaderModule(ctx_->device(), shaderDir + "debug_line_frag.spv");

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    VkVertexInputBindingDescription bind{0, sizeof(DebugLineVertex),
                                          VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[2]{};
    attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT,
                static_cast<uint32_t>(offsetof(DebugLineVertex, pos))};
    attrs[1] = {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                static_cast<uint32_t>(offsetof(DebugLineVertex, color))};

    VkPipelineVertexInputStateCreateInfo vi{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &bind;
    vi.vertexAttributeDescriptionCount = 2;
    vi.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ia{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = topology;

    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.lineWidth = 2.f;  // 太めの線で見やすく
    rs.cullMode = VK_CULL_MODE_NONE;  // 両面 (扇形は裏側から見ても見える)
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo ms{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_FALSE;  // デバッグなので depth を上書きしない
    ds.depthCompareOp = VK_COMPARE_OP_LESS;

    // アルファブレンド有効 (扇形の半透明塗りつぶし用)
    VkPipelineColorBlendAttachmentState blendAtt{};
    blendAtt.blendEnable = VK_TRUE;
    blendAtt.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAtt.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAtt.colorBlendOp = VK_BLEND_OP_ADD;
    blendAtt.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAtt.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendAtt.alphaBlendOp = VK_BLEND_OP_ADD;
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
    pci.layout = layout_;
    pci.renderPass = renderPass;
    pci.subpass = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    const VkResult r =
        vkCreateGraphicsPipelines(ctx_->device(), VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline);

    vkDestroyShaderModule(ctx_->device(), vert, nullptr);
    vkDestroyShaderModule(ctx_->device(), frag, nullptr);

    if (r != VK_SUCCESS) {
        throw std::runtime_error("DebugLinePass: vkCreateGraphicsPipelines failed");
    }
    return pipeline;
}

void DebugLinePass::createVertexBuffers() {
    const VkDeviceSize bufSize = sizeof(DebugLineVertex) * kMaxVerticesPerFrame;

    for (uint32_t i = 0; i < FrameSync::MAX_FRAMES_IN_FLIGHT; ++i) {
        // line VB
        resources_->createBuffer(
            bufSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            lineVBs_[i], lineVBMems_[i]);
        if (vkMapMemory(ctx_->device(), lineVBMems_[i], 0, bufSize, 0, &lineVBMapped_[i]) !=
            VK_SUCCESS) {
            throw std::runtime_error("DebugLinePass: vkMapMemory (line) failed");
        }
        // tri VB
        resources_->createBuffer(
            bufSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            triVBs_[i], triVBMems_[i]);
        if (vkMapMemory(ctx_->device(), triVBMems_[i], 0, bufSize, 0, &triVBMapped_[i]) !=
            VK_SUCCESS) {
            throw std::runtime_error("DebugLinePass: vkMapMemory (tri) failed");
        }
    }
}

void DebugLinePass::destroyVertexBuffers() {
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;
    for (uint32_t i = 0; i < FrameSync::MAX_FRAMES_IN_FLIGHT; ++i) {
        if (lineVBMapped_[i]) {
            vkUnmapMemory(ctx_->device(), lineVBMems_[i]);
            lineVBMapped_[i] = nullptr;
        }
        if (lineVBs_[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(ctx_->device(), lineVBs_[i], nullptr);
            lineVBs_[i] = VK_NULL_HANDLE;
        }
        if (lineVBMems_[i] != VK_NULL_HANDLE) {
            vkFreeMemory(ctx_->device(), lineVBMems_[i], nullptr);
            lineVBMems_[i] = VK_NULL_HANDLE;
        }
        if (triVBMapped_[i]) {
            vkUnmapMemory(ctx_->device(), triVBMems_[i]);
            triVBMapped_[i] = nullptr;
        }
        if (triVBs_[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(ctx_->device(), triVBs_[i], nullptr);
            triVBs_[i] = VK_NULL_HANDLE;
        }
        if (triVBMems_[i] != VK_NULL_HANDLE) {
            vkFreeMemory(ctx_->device(), triVBMems_[i], nullptr);
            triVBMems_[i] = VK_NULL_HANDLE;
        }
    }
}

void DebugLinePass::shutdown() {
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;
    destroyVertexBuffers();
    if (linePipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(ctx_->device(), linePipeline_, nullptr);
        linePipeline_ = VK_NULL_HANDLE;
    }
    if (triPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(ctx_->device(), triPipeline_, nullptr);
        triPipeline_ = VK_NULL_HANDLE;
    }
    if (layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(ctx_->device(), layout_, nullptr);
        layout_ = VK_NULL_HANDLE;
    }
    ctx_ = nullptr;
    resources_ = nullptr;
    swapchain_ = nullptr;
}

void DebugLinePass::execute(const ExecuteInfo& info) {
    if (info.cmd == VK_NULL_HANDLE) return;
    if (info.frameIndex >= FrameSync::MAX_FRAMES_IN_FLIGHT) return;

    const size_t lineCount = info.lineVertices ? info.lineVertices->size() : 0;
    const size_t triCount  = info.triVertices  ? info.triVertices->size()  : 0;

    // 早期 return: 描画するものがなければ何もしない (パイプライン bind すら不要)
    if (lineCount == 0 && triCount == 0) return;

    // 上限チェック (超過した場合は超えた分を捨てる)
    const size_t lineUsed = (lineCount < kMaxVerticesPerFrame) ? lineCount : kMaxVerticesPerFrame;
    const size_t triUsed  = (triCount  < kMaxVerticesPerFrame) ? triCount  : kMaxVerticesPerFrame;

    // 頂点バッファに書き込む (vkMapMemory は init 時に永続マップ済み)
    if (lineUsed > 0) {
        std::memcpy(lineVBMapped_[info.frameIndex], info.lineVertices->data(),
                    sizeof(DebugLineVertex) * lineUsed);
    }
    if (triUsed > 0) {
        std::memcpy(triVBMapped_[info.frameIndex], info.triVertices->data(),
                    sizeof(DebugLineVertex) * triUsed);
    }

    // viewport/scissor は MainPass が既にセット済み (動的ステートは command buffer に残る)
    // が、 念のためここでも設定する (パイプライン切替で reset される可能性に備える)
    const VkExtent2D extent = swapchain_->extent();
    VkViewport viewport{
        0.f, 0.f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.f, 1.f};
    VkRect2D scissor{{0, 0}, extent};

    // frame set bind は 1 回で OK (どちらの pipeline でも layout は同じ)
    vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_, 0, 1,
                            &info.frameSet, 0, nullptr);

    VkDeviceSize zero = 0;

    // ── triangle pass (扇形の塗りつぶし、 先に描いて線で上書きされるように) ──
    if (triUsed > 0) {
        vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, triPipeline_);
        vkCmdSetViewport(info.cmd, 0, 1, &viewport);
        vkCmdSetScissor(info.cmd, 0, 1, &scissor);
        vkCmdBindVertexBuffers(info.cmd, 0, 1, &triVBs_[info.frameIndex], &zero);
        vkCmdDraw(info.cmd, static_cast<uint32_t>(triUsed), 1, 0, 0);
    }

    // ── line pass (縁取り、 刃の線、 円) ──
    if (lineUsed > 0) {
        vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline_);
        vkCmdSetViewport(info.cmd, 0, 1, &viewport);
        vkCmdSetScissor(info.cmd, 0, 1, &scissor);
        vkCmdBindVertexBuffers(info.cmd, 0, 1, &lineVBs_[info.frameIndex], &zero);
        vkCmdDraw(info.cmd, static_cast<uint32_t>(lineUsed), 1, 0, 0);
    }
}
