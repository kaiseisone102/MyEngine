// =============================================================================
// main_pass.cpp — Phase 1C: opaque/transparent + Water + DebugLine + Particle
//                            + HUD + ImGui
// =============================================================================
#include "renderer/main_pass.h"
#include <glm/gtc/matrix_transform.hpp>

#include <cstddef>
#include <iostream>
#include <stdexcept>

#include "renderer/barrier.h"
#include "renderer/debug_line_pass.h"
#include "renderer/depth_layouts.h"
#include "renderer/debug_line_renderer.h"
#include "renderer/hud_draw_list.h"
#include "renderer/hud_pass.h"
#include "renderer/imgui_layer.h"
#include "renderer/material.h"
#include "renderer/mesh.h"
#include "renderer/model.h"
#include "renderer/static_draw.h"
#include "renderer/static_cull_build.h"
#include "renderer/draw_data_pool.h"
#include "renderer/indirect_exec.h"
#include "renderer/particle_pass.h"
#include "renderer/shader_util.h"
#include "renderer/swapchain.h"
#include "renderer/terrain_mesh.h"
#include "renderer/vulkan_context.h"
#include "renderer/water_pass.h"

namespace {

void drawSkinnedList(VkCommandBuffer cmd, VkPipelineLayout layout,
                          VkDeviceAddress skinAddress,
                          const std::vector<SkinnedDrawItem>& list) {
    if (list.empty()) return;
    const Model* curModel = nullptr;
    const std::vector<Material>* curMaterials = nullptr;
    for (const SkinnedDrawItem& item : list) {
        if (!item.sourceModel) continue;
        if (item.sourceModel != curModel) {
            curModel = item.sourceModel;
            curMaterials = &curModel->materials();
        }
        MainPass::SkinnedPushConstants pc{};
        pc.model = item.model;
        pc.skinOffset = item.skinOffset;
        pc.skinBuffer = skinAddress;
        pc.alpha = item.alpha;
        for (const SubMesh& sm : curModel->subMeshes()) {
            if (sm.indexCount == 0) continue;
            // S4-d: per-submesh material id into the SSBO
            pc.materialId = (curMaterials && sm.materialIndex < curMaterials->size())
                ? (*curMaterials)[sm.materialIndex].materialId()
                : 0u;
            vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                sizeof(MainPass::SkinnedPushConstants), &pc);
            sm.bindAndDraw(cmd);
        }
    }
}

}  // namespace

void MainPass::init(const InitInfo& info) {
    if (!info.ctx || !info.swapchain) throw std::runtime_error("MainPass::init: invalid info");
    ctx_ = info.ctx;
    swapchain_ = info.swapchain;
    hdrColorView_ = info.hdrColorView;  // Phase 1H-2
    hdrColorImage_ = info.hdrColorImage;  // PART4 4a-1
    hdrColorFormat_ = info.hdrColorFormat;
    // PART4 4a-1: cache the depth format that pipelines are compatible with.
    // Dynamic rendering takes the format directly (no VkRenderPass) so all
    // child passes must use the same format when building their pipelines.
    depthFormat_ = swapchain_->depthFormat();
    // PART4 4a-2: GBuffer attachments (opaque MRT).
    normalView_ = info.normalView;
    normalImage_ = info.normalImage;
    normalFormat_ = info.normalFormat;
    motionView_ = info.motionView;
    motionImage_ = info.motionImage;
    motionFormat_ = info.motionFormat;

    createStaticLayout(info.frameSetLayout, info.bindlessSetLayout);  // S4-c: set=1 is now the bindless array
    {
        PipelineBuildArgs argsOp{staticLayout_.get(), "triangle_vert.spv", "triangle_frag.spv", false};
        staticPipelineOpaque_ = VkUnique<VkPipeline>(ctx_->device(), buildPipeline(argsOp, info.shaderDir));
        PipelineBuildArgs argsTr{staticLayout_.get(), "triangle_vert.spv", "triangle_frag.spv", true};
        staticPipelineTransparent_ = VkUnique<VkPipeline>(ctx_->device(), buildPipeline(argsTr, info.shaderDir));
    }

    createSkinnedLayout(info.frameSetLayout, info.bindlessSetLayout);  // S5: set=1 bindless
    {
        PipelineBuildArgs argsOp{skinnedLayout_.get(), "triangle_skinned_vert.spv", "triangle_skinned_frag.spv", false, true};
        skinnedPipelineOpaque_ = VkUnique<VkPipeline>(ctx_->device(), buildPipeline(argsOp, info.shaderDir));
        PipelineBuildArgs argsTr{skinnedLayout_.get(), "triangle_skinned_vert.spv", "triangle_skinned_frag.spv", true, true};
        skinnedPipelineTransparent_ = VkUnique<VkPipeline>(ctx_->device(), buildPipeline(argsTr, info.shaderDir));
    }

    // === Phase 1D: bindless pipeline (opaque only, test cube) ===
    if (info.bindlessSetLayout != VK_NULL_HANDLE) {
        createBindlessLayout(info.frameSetLayout, info.bindlessSetLayout);
        PipelineBuildArgs argsBl{bindlessLayout_.get(), "triangle_bindless_vert.spv",
                                  "triangle_bindless_frag.spv", false};
        bindlessPipelineOpaque_ = VkUnique<VkPipeline>(ctx_->device(), buildPipeline(argsBl, info.shaderDir));
        std::cout << "[MainPass] bindless pipeline created\n";
        // Phase 1F: grass pipeline (alpha-tested, bindless texture, no cull)
        createGrassLayout(info.frameSetLayout, info.bindlessSetLayout);
        PipelineBuildArgs argsGrass{grassLayout_.get(), "grass_instanced_vert.spv",
                                   "grass_instanced_frag.spv", false};
        argsGrass.noCull = true;
        grassPipeline_ = VkUnique<VkPipeline>(ctx_->device(), buildPipeline(argsGrass, info.shaderDir));
        std::cout << "[MainPass] grass pipeline created\n";
    }

}

void MainPass::createStaticLayout(VkDescriptorSetLayout frameSetLayout,
                                     VkDescriptorSetLayout bindlessSetLayout) {
    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;  // PART3b: only the DrawData SSBO address
    pc.offset = 0;
    pc.size = sizeof(myengine::shared::StaticDrawPushConstants);

    // S6-c: set=1 is the bindless texture array (call site already passes bindlessSetLayout)
    VkDescriptorSetLayout setLayouts[2] = {frameSetLayout, bindlessSetLayout};
    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    lci.setLayoutCount = 2;
    lci.pSetLayouts = setLayouts;
    lci.pushConstantRangeCount = 1;
    lci.pPushConstantRanges = &pc;
    VkPipelineLayout slay = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(ctx_->device(), &lci, nullptr, &slay) != VK_SUCCESS) {
        throw std::runtime_error("MainPass: static layout failed");
    }
    staticLayout_ = VkUnique<VkPipelineLayout>(ctx_->device(), slay);
}

void MainPass::createGrassLayout(VkDescriptorSetLayout frameSetLayout,
                                 VkDescriptorSetLayout bindlessSetLayout) {
    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pc.offset = 0;
    pc.size = sizeof(InstancedPushConstants);

    // set=0 frame UBO, set=1 bindless texture array (for grass texture).
    VkDescriptorSetLayout setLayouts[2] = {frameSetLayout, bindlessSetLayout};
    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    lci.setLayoutCount = 2;
    lci.pSetLayouts = setLayouts;
    lci.pushConstantRangeCount = 1;
    lci.pPushConstantRanges = &pc;
    VkPipelineLayout glay = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(ctx_->device(), &lci, nullptr, &glay) != VK_SUCCESS) {
        throw std::runtime_error("MainPass: grass layout failed");
    }
    grassLayout_ = VkUnique<VkPipelineLayout>(ctx_->device(), glay);
}

void MainPass::createSkinnedLayout(VkDescriptorSetLayout frameSetLayout,
                                       VkDescriptorSetLayout bindlessSetLayout) {
    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;  // S5: frag reads materialId
    pc.offset = 0;
    pc.size = sizeof(SkinnedPushConstants);

    VkDescriptorSetLayout setLayouts[2] = {frameSetLayout, bindlessSetLayout};
    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    lci.setLayoutCount = 2;
    lci.pSetLayouts = setLayouts;
    lci.pushConstantRangeCount = 1;
    lci.pPushConstantRanges = &pc;
    VkPipelineLayout klay = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(ctx_->device(), &lci, nullptr, &klay) != VK_SUCCESS) {
        throw std::runtime_error("MainPass: skinned layout failed");
    }
    skinnedLayout_ = VkUnique<VkPipelineLayout>(ctx_->device(), klay);
}

VkPipeline MainPass::buildPipeline(const PipelineBuildArgs& args, const std::string& shaderDir) {
    VkShaderModule vert = shader_util::loadShaderModule(ctx_->device(), shaderDir + args.vertSpv);
    VkShaderModule frag = shader_util::loadShaderModule(ctx_->device(), shaderDir + args.fragSpv);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    VkVertexInputBindingDescription bind{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[6]{};
    attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, pos))};
    attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, color))};
    attrs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, texCoord))};
    attrs[3] = {3, 0, VK_FORMAT_R32G32B32_SFLOAT,
                  static_cast<uint32_t>(offsetof(Vertex, normal))};
    attrs[4] = {4, 0, VK_FORMAT_R32G32B32A32_SINT,
                  static_cast<uint32_t>(offsetof(Vertex, jointIndices))};
    attrs[5] = {5, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                  static_cast<uint32_t>(offsetof(Vertex, jointWeights))};

    VkPipelineVertexInputStateCreateInfo vi{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &bind;
    vi.vertexAttributeDescriptionCount = args.isSkinned ? 6 : 4;  // Phase 1B-6
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
    rs.cullMode = args.noCull ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo ms{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = args.transparent ? VK_FALSE : VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_GREATER;  // reverse-Z (see renderer/projection.h)

    // PART4 4a-2: opaque pipelines render into the 3-attachment MRT (HDR +
    // normal + motion); transparent pipelines stay 1-attachment and run in
    // the non-opaque BeginRendering. Blend state mirrors the attachment
    // count so VUID-06195 (pipeline colorAttachmentCount == active rendering
    // colorAttachmentCount) is satisfied.
    VkPipelineColorBlendAttachmentState opaqueBlendAtts[3]{};
    for (int i = 0; i < 3; ++i) {
        opaqueBlendAtts[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        opaqueBlendAtts[i].blendEnable = VK_FALSE;
    }

    VkPipelineColorBlendAttachmentState transparentBlendAtt{};
    transparentBlendAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    transparentBlendAtt.blendEnable = VK_TRUE;
    transparentBlendAtt.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    transparentBlendAtt.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    transparentBlendAtt.colorBlendOp = VK_BLEND_OP_ADD;
    transparentBlendAtt.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    transparentBlendAtt.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    transparentBlendAtt.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo cb{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = args.transparent ? 1u : 3u;
    cb.pAttachments = args.transparent ? &transparentBlendAtt : opaqueBlendAtts;

    VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynStates;

    // PART4 4a-1/4a-2: dynamic rendering. Chain VkPipelineRenderingCreateInfo
    // describing the attachment formats; renderPass = VK_NULL_HANDLE.
    // Opaque draws use the 3-attachment GBuffer (HDR + normal + motion);
    // transparent uses 1-attachment HDR.
    VkFormat colorFormatsMrt[3] = {hdrColorFormat_, normalFormat_, motionFormat_};
    VkFormat colorFormatsOne[1] = {hdrColorFormat_};
    VkPipelineRenderingCreateInfo rci{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rci.colorAttachmentCount = args.transparent ? 1u : 3u;
    rci.pColorAttachmentFormats = args.transparent ? colorFormatsOne : colorFormatsMrt;
    rci.depthAttachmentFormat = depthFormat_;

    VkGraphicsPipelineCreateInfo pci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pci.pNext = &rci;
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
    pci.layout = args.layout;
    pci.renderPass = VK_NULL_HANDLE;

    VkPipeline pipeline = VK_NULL_HANDLE;
    const VkResult r =
        vkCreateGraphicsPipelines(ctx_->device(), VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline);

    vkDestroyShaderModule(ctx_->device(), vert, nullptr);
    vkDestroyShaderModule(ctx_->device(), frag, nullptr);

    if (r != VK_SUCCESS) throw std::runtime_error("MainPass: vkCreateGraphicsPipelines failed");
    return pipeline;
}

// PART4 4a-1: no framebuffer / renderpass to recreate on resize. The HDR
// color view is replaced via setHdrColorView() from the swapchain owner and
// dynamic rendering picks it up at the next execute().
void MainPass::onSwapchainResized() {
}

void MainPass::execute(const ExecuteInfo& info) {
    if (!info.cmd) throw std::runtime_error("MainPass::execute: invalid cmd");
    if (hdrColorView_ == VK_NULL_HANDLE)
        throw std::runtime_error("MainPass::execute: hdrColorView not set");

    const VkExtent2D extent = swapchain_->extent();

    // PART4 4a-2: opaque draws now write a 3-attachment MRT (HDR color,
    // GBuffer normal, motion vector) while non-opaque draws (water /
    // transparent / particle / debug_line / hud / imgui) stay 1-attachment
    // HDR. Vulkan 1.3 dynamic rendering requires the bound pipeline's
    // colorAttachmentCount to match the active vkCmdBeginRendering set, so
    // we split into two beginRender / endRender blocks instead of using
    // colorWriteMask=0 (legacy workaround). See
    // PART4 design §3.4-S and the 2026-05 MRT survey.

    // PART4 4a-2: dynamic rendering does not auto-transition image layouts
    // (VkRenderPass used initial/final layouts; vkCmdBeginRendering does
    // not). Every attachment must be in the layout declared by
    // VkRenderingAttachmentInfo on entry. We discard previous contents on
    // all four attachments (oldLayout=UNDEFINED, srcAccess=0): the first
    // frame's images really are UNDEFINED, and on every subsequent frame
    // PostPass / OverlayPass / this pass's post-barrier have left them in
    // *_READ_ONLY layouts that we'd overwrite with loadOp=CLEAR anyway.
    // sync2 NONE for the source stage matches "no prior work to sync."
    // sync2 modern idiom: VK_PIPELINE_STAGE_2_NONE for the producer side of
    // an UNDEFINED -> X transition (no prior work to synchronize against).
    // Depth uses VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL (Vulkan 1.2 separate
    // depth/stencil layouts) - D32_SFLOAT has no stencil so this is the
    // precise layout; the combined DEPTH_STENCIL_*_OPTIMAL form is the legacy
    // shape kept for stencil-bearing formats.
    barrier::ImageBarrier toAttach[4] = {
        {
            .image = hdrColorImage_,
            .range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .srcStage = VK_PIPELINE_STAGE_2_NONE,
            .srcAccess = 0,
            .dstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        },
        {
            .image = normalImage_,
            .range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .srcStage = VK_PIPELINE_STAGE_2_NONE,
            .srcAccess = 0,
            .dstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        },
        {
            .image = motionImage_,
            .range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .srcStage = VK_PIPELINE_STAGE_2_NONE,
            .srcAccess = 0,
            .dstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        },
        {
            .image = swapchain_->depthImage(),
            .range = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1},
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = depth_layouts::attachment(*ctx_),
            .srcStage = VK_PIPELINE_STAGE_2_NONE,
            .srcAccess = 0,
            .dstStage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
            .dstAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        },
    };
    barrier::recordBatch(*ctx_, info.cmd, {}, {}, toAttach);

    // ─── BeginRendering #1: opaque MRT (HDR + normal + motion + depth) ─
    VkRenderingAttachmentInfo opaqueColorAtts[3]{};
    opaqueColorAtts[0].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    opaqueColorAtts[0].imageView = hdrColorView_;
    opaqueColorAtts[0].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    opaqueColorAtts[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    opaqueColorAtts[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    opaqueColorAtts[0].clearValue.color = {
        {info.clearColor.r, info.clearColor.g, info.clearColor.b, info.clearColor.a}};
    opaqueColorAtts[1].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    opaqueColorAtts[1].imageView = normalView_;
    opaqueColorAtts[1].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    opaqueColorAtts[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    opaqueColorAtts[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    opaqueColorAtts[1].clearValue.color = {{0.5f, 0.5f, 1.0f, 0.0f}};  // octahedral 0 = up
    opaqueColorAtts[2].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    opaqueColorAtts[2].imageView = motionView_;
    opaqueColorAtts[2].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    opaqueColorAtts[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    opaqueColorAtts[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    opaqueColorAtts[2].clearValue.color = {{0.0f, 0.0f, 0.0f, 0.0f}};

    VkRenderingAttachmentInfo depthAtt{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    depthAtt.imageView = swapchain_->depthView();
    depthAtt.imageLayout = depth_layouts::attachment(*ctx_);
    depthAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;  // 4a-2: HZB / HUD will sample
    depthAtt.clearValue.depthStencil = {0.0f, 0};  // reverse-Z: clear to far (= 0.0)

    VkRenderingInfo rp{VK_STRUCTURE_TYPE_RENDERING_INFO};
    rp.renderArea = {{0, 0}, extent};
    rp.layerCount = 1;
    rp.colorAttachmentCount = 3;
    rp.pColorAttachments = opaqueColorAtts;
    rp.pDepthAttachment = &depthAtt;

    vkCmdBeginRendering(info.cmd, &rp);

    VkViewport viewport{0.f, 0.f, static_cast<float>(extent.width),
                          static_cast<float>(extent.height), 0.f, 1.f};
    VkRect2D scissor{{0, 0}, extent};

    // ============================================================
    // PHASE 1: 不透明 (opaque)
    // ============================================================
    const auto* meshOp = info.meshDrawListOpaque;
    const auto* staticOp = info.staticModelDrawListOpaque;
    const auto* terrainOp = info.terrainDrawListOpaque;
    const auto* modelOp = info.modelDrawListOpaque;

    const bool hasOpaqueStatic =
        (info.mesh && meshOp && !meshOp->empty()) ||
        (staticOp && !staticOp->empty()) ||
        (terrainOp && !terrainOp->empty());
    if (hasOpaqueStatic) {
        vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, staticPipelineOpaque_.get());
        vkCmdSetViewport(info.cmd, 0, 1, &viewport);
        vkCmdSetScissor(info.cmd, 0, 1, &scissor);
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, staticLayout_.get(), 0, 1,
                                  &info.frameSet, 0, nullptr);
        // S4-c: set=1 is the bindless texture array (bound once for all static draws)
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, staticLayout_.get(), 1, 1,
                                  &info.bindlessSet, 0, nullptr);
        // PART3b: push the DrawData SSBO address once (per-draw data via gl_InstanceIndex)
        {
            StaticDrawPC dpc{};
            dpc.drawBuffer = info.drawBufferAddress;
            vkCmdPushConstants(info.cmd, staticLayout_.get(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                               sizeof(StaticDrawPC), &dpc);
        }

        // PART3c-2: opaque static is GPU-driven. CullingPass wrote instanceCount
        // (0/1) into its per-frame indirect command buffer, built parallel to
        // preparedOpaque (command index == drawId). We bind each GeometryBuffer
        // block and issue an indirect draw over that block's CONTIGUOUS run of
        // commands; the GPU skips instanceCount==0 (culled) draws. Capability path
        // (Roadmap §3): multiDrawIndirect -> one MDI per run; else per-draw indirect
        // loop. If drawIndirectFirstInstance is unsupported, a non-zero firstInstance
        // (our DrawData slot) is illegal in an indirect command, so we fall back to
        // the direct CPU draw loop (firstInstance is unrestricted for direct draws).
        if (info.preparedOpaque && !info.preparedOpaque->empty() && info.geometry) {
            const std::vector<static_cull::PreparedDraw>& draws = *info.preparedOpaque;
            // PART4 4-前-4: prefer the compaction path (compactCmd + countBuf
            // written by scan_compact). indirect_exec picks DGC / IndirectCount
            // / Legacy per device capability. Pre-4-prep-4 indirectCommandBuffer
            // remains as a deeper fallback when compactCmd is unavailable, and
            // the direct CPU draw stays as the final fallback when no indirect
            // path can be used.
            const bool useCompact =
                info.compactCommandBuffer != VK_NULL_HANDLE && ctx_->drawIndirectFirstInstance();
            const bool useLegacyIndirect =
                !useCompact && info.indirectCommandBuffer != VK_NULL_HANDLE &&
                ctx_->drawIndirectFirstInstance();
            if (useCompact && info.preparedOpaqueRanges) {
                const uint32_t stride = static_cast<uint32_t>(sizeof(VkDrawIndexedIndirectCommand));
                uint32_t blockIdx = 0;
                for (const static_cull::BlockRange& range : *info.preparedOpaqueRanges) {
                    if (range.drawCount == 0) { ++blockIdx; continue; }
                    info.geometry->bindBlock(info.cmd, range.blockIndex);
                    indirect_exec::DrawIndexedIndirectCountInfo dii{};
                    dii.commandBuffer = info.compactCommandBuffer;
                    dii.commandOffset = static_cast<VkDeviceSize>(range.firstDraw) * stride;
                    dii.countBuffer   = info.indirectCountBuffer;
                    dii.countOffset   = static_cast<VkDeviceSize>(blockIdx) * sizeof(uint32_t);
                    dii.maxCount      = range.drawCount;
                    dii.stride        = stride;
                    indirect_exec::recordDrawIndexedIndirectCount(*ctx_, info.cmd, dii);
                    ++blockIdx;
                }
            } else if (useLegacyIndirect && info.preparedOpaqueRanges) {
                // PART4 4-前-1/3 legacy path: instanceCount=0 skipping over the
                // full block range. Used when 4-前-4's compactCmd path is not
                // wired (e.g. integration test fallback).
                const uint32_t stride = static_cast<uint32_t>(sizeof(VkDrawIndexedIndirectCommand));
                for (const static_cull::BlockRange& range : *info.preparedOpaqueRanges) {
                    if (range.drawCount == 0) continue;
                    info.geometry->bindBlock(info.cmd, range.blockIndex);
                    const VkDeviceSize off = static_cast<VkDeviceSize>(range.firstDraw) * stride;
                    if (ctx_->multiDrawIndirect()) {
                        vkCmdDrawIndexedIndirect(info.cmd, info.indirectCommandBuffer,
                                                  off, range.drawCount, stride);
                    } else {
                        for (uint32_t k = 0; k < range.drawCount; ++k)
                            vkCmdDrawIndexedIndirect(info.cmd, info.indirectCommandBuffer,
                                                     off + static_cast<VkDeviceSize>(k) * stride, 1, stride);
                    }
                }
            } else {
                // Direct CPU draw fallback (firstInstance unrestricted).
                uint32_t boundBlock = UINT32_MAX;
                for (const static_cull::PreparedDraw& pd : draws) {
                    if (pd.blockIndex != boundBlock) {
                        info.geometry->bindBlock(info.cmd, pd.blockIndex);
                        boundBlock = pd.blockIndex;
                    }
                    vkCmdDrawIndexed(info.cmd, pd.indexCount, 1, pd.firstIndex, pd.vertexOffset, pd.drawSlot);
                }
            }
        }

        // PART3c scope: terrain is a separate bucket (own GeometryBuffer + cull +
        // splat material, built in the streaming Phase). For now it stays on the
        // legacy CPU loop, drawn after the GPU-driven props.
        if (terrainOp)
            static_draw::drawTerrainList(info.cmd, *info.drawDataPool, info.frameIndex, *terrainOp, true);
    }

    // === Phase 1F: instanced grass (alpha-tested, bindless texture) ===
    if (info.grassDrawList && !info.grassDrawList->empty()
        && info.instanceBufferAddress != 0 && grassPipeline_
        && info.bindlessSet != VK_NULL_HANDLE) {
        vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, grassPipeline_.get());
        vkCmdSetViewport(info.cmd, 0, 1, &viewport);
        vkCmdSetScissor(info.cmd, 0, 1, &scissor);
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, grassLayout_.get(), 0, 1,
                                &info.frameSet, 0, nullptr);
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, grassLayout_.get(), 1, 1,
                                &info.bindlessSet, 0, nullptr);
        for (const InstancedMeshDrawItem& item : *info.grassDrawList) {
            if (!item.mesh || item.instances.empty()) continue;
            InstancedPushConstants pc{};
            pc.instanceBuffer = info.instanceBufferAddress;
            pc.materialId = item.material ? item.material->materialId() : 0u;  // S6-b: unified material path
            pc.alpha = item.alpha;
            vkCmdPushConstants(info.cmd, grassLayout_.get(),
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(InstancedPushConstants), &pc);
            const uint32_t count = static_cast<uint32_t>(item.instances.size());
            item.mesh->bindAndDraw(info.cmd, count, item.instanceOffset);
        }
    }

    if (modelOp && !modelOp->empty()) {
        vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skinnedPipelineOpaque_.get());
        vkCmdSetViewport(info.cmd, 0, 1, &viewport);
        vkCmdSetScissor(info.cmd, 0, 1, &scissor);
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skinnedLayout_.get(), 0, 1,
                                  &info.frameSet, 0, nullptr);
        // S5: set=1 bindless texture array
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skinnedLayout_.get(), 1, 1,
                                  &info.bindlessSet, 0, nullptr);
        drawSkinnedList(info.cmd, skinnedLayout_.get(), info.skinAddress, *modelOp);
    }

    // ============================================================
    // Phase 1D-2d: bindless test cube (opaque demo, moved into the opaque MRT
    // pass by PART4 4a-2 so it writes to all three attachments and is no
    // longer drawn on top of transparent geometry).
    // ============================================================
    if (bindlessPipelineOpaque_ && info.bindlessSet != VK_NULL_HANDLE && info.mesh) {
        vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bindlessPipelineOpaque_.get());
        vkCmdSetViewport(info.cmd, 0, 1, &viewport);
        vkCmdSetScissor(info.cmd, 0, 1, &scissor);

        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bindlessLayout_.get(), 0, 1,
                                &info.frameSet, 0, nullptr);
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bindlessLayout_.get(), 1, 1,
                                &info.bindlessSet, 0, nullptr);

        info.mesh->bind(info.cmd);

        StaticBindlessPushConstants pc{};
        pc.model = glm::translate(glm::mat4(1.f), glm::vec3(3.f, 5.f, 3.f));
        pc.alpha = 1.0f;
        pc.albedoIdx = 5;  // grass_field

        vkCmdPushConstants(info.cmd, bindlessLayout_.get(),
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(pc), &pc);
        vkCmdDrawIndexed(info.cmd, info.mesh->indexCount(), 1, 0, 0, 0);
    }

    // PART4 4a-2 redesign: end the opaque MRT and begin a 1-attachment
    // non-opaque rendering for water + transparent + debug_line + particle.
    // No mid-pass barriers are needed: with HUD and ImGui pulled out into
    // PassChain's OverlayPass step, nothing in this scope samples the
    // GBuffer attachments. HDR + depth carry through via loadOp=LOAD.
    vkCmdEndRendering(info.cmd);

    VkRenderingAttachmentInfo nonOpaqueColorAtt{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    nonOpaqueColorAtt.imageView = hdrColorView_;
    nonOpaqueColorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    nonOpaqueColorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    nonOpaqueColorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingAttachmentInfo nonOpaqueDepthAtt{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    nonOpaqueDepthAtt.imageView = swapchain_->depthView();
    nonOpaqueDepthAtt.imageLayout = depth_layouts::attachment(*ctx_);
    nonOpaqueDepthAtt.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    nonOpaqueDepthAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo rp2{VK_STRUCTURE_TYPE_RENDERING_INFO};
    rp2.renderArea = {{0, 0}, extent};
    rp2.layerCount = 1;
    rp2.colorAttachmentCount = 1;
    rp2.pColorAttachments = &nonOpaqueColorAtt;
    rp2.pDepthAttachment = &nonOpaqueDepthAtt;

    vkCmdBeginRendering(info.cmd, &rp2);

    // ============================================================
    // PHASE 1.5: 水面 (opaque と transparent の間)
    // ============================================================
    if (info.waterPass && info.waterDrawList && !info.waterDrawList->empty()) {
        vkCmdSetViewport(info.cmd, 0, 1, &viewport);
        vkCmdSetScissor(info.cmd, 0, 1, &scissor);

        WaterPass::ExecuteInfo wi{};
        wi.cmd = info.cmd;
        wi.frameSet = info.frameSet;
        wi.waterDrawList = info.waterDrawList;
        wi.time = info.waterTime;
        wi.frameIndex = info.frameIndex;
        wi.useReflection = info.waterUseReflection;
        wi.reflectVP = info.waterReflectVP;
        info.waterPass->execute(wi);
    }

    // ============================================================
    // PHASE 2: 半透明 (transparent) — 奥→手前ソート済み想定
    // ============================================================
    const auto* meshTr = info.meshDrawListTransparent;
    const auto* staticTr = info.staticModelDrawListTransparent;
    const auto* terrainTr = info.terrainDrawListTransparent;
    const auto* modelTr = info.modelDrawListTransparent;

    const bool hasTransparentStatic =
        (info.mesh && meshTr && !meshTr->empty()) ||
        (staticTr && !staticTr->empty()) ||
        (terrainTr && !terrainTr->empty());

    if (hasTransparentStatic) {
        vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, staticPipelineTransparent_.get());
        vkCmdSetViewport(info.cmd, 0, 1, &viewport);
        vkCmdSetScissor(info.cmd, 0, 1, &scissor);
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, staticLayout_.get(), 0, 1,
                                  &info.frameSet, 0, nullptr);
        // S4-c: set=1 bindless texture array
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, staticLayout_.get(), 1, 1,
                                  &info.bindlessSet, 0, nullptr);
        // PART3b: push the DrawData SSBO address once
        {
            StaticDrawPC dpc{};
            dpc.drawBuffer = info.drawBufferAddress;
            vkCmdPushConstants(info.cmd, staticLayout_.get(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                               sizeof(StaticDrawPC), &dpc);
        }

        if (info.mesh && meshTr)
            static_draw::drawMeshList(info.cmd, *info.drawDataPool, info.frameIndex, info.mesh, *meshTr, true);
        if (staticTr)
            static_draw::drawStaticModelList(info.cmd, *info.drawDataPool, info.frameIndex, *staticTr, true);
        if (terrainTr)
            static_draw::drawTerrainList(info.cmd, *info.drawDataPool, info.frameIndex, *terrainTr, true);
    }

    if (modelTr && !modelTr->empty()) {
        vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skinnedPipelineTransparent_.get());
        vkCmdSetViewport(info.cmd, 0, 1, &viewport);
        vkCmdSetScissor(info.cmd, 0, 1, &scissor);
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skinnedLayout_.get(), 0, 1,
                                  &info.frameSet, 0, nullptr);
        // S5: set=1 bindless texture array
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skinnedLayout_.get(), 1, 1,
                                  &info.bindlessSet, 0, nullptr);
        drawSkinnedList(info.cmd, skinnedLayout_.get(), info.skinAddress, *modelTr);
    }

    // ============================================================
    // PHASE 3: Overlay (debug, particle, hud, imgui)
    // ============================================================
    if (info.debugLinePass && info.debugLines) {
        DebugLinePass::ExecuteInfo dbg{};
        dbg.cmd = info.cmd;
        dbg.frameSet = info.frameSet;
        dbg.frameIndex = info.frameIndex;
        dbg.lineVertices = &info.debugLines->lineVertices();
        dbg.triVertices = &info.debugLines->triVertices();
        info.debugLinePass->execute(dbg);
    }

    if (info.particlePass && info.particles) {
        ParticlePass::ExecuteInfo pi{};
        pi.cmd = info.cmd;
        pi.frameSet = info.frameSet;
        pi.frameIndex = info.frameIndex;
        pi.particles = info.particles;
        pi.cullingDistance = info.particleCullingDistance;
        info.particlePass->execute(pi);
    }

    // HUD + ImGui moved to PassChain::recordOverlay (4a-2 redesign).

    vkCmdEndRendering(info.cmd);

    // PART4 4a-2 redesign: clean handoff to the overlay step. HDR stays in
    // COLOR_ATTACHMENT_OPTIMAL so OverlayPass can keep writing (loadOp=LOAD).
    // GBuffer attachments (normal / motion / depth) transition to read-only
    // here so OverlayPass's debug viewer (and any future SS effect) samples
    // them safely. depth uses DEPTH_STENCIL_READ_ONLY_OPTIMAL so the
    // overlay's read-only depth attachment / sampled-image read both work.
    barrier::ImageBarrier toRead[3] = {
        {
            .image = normalImage_,
            .range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
            .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcStage  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            .dstStage  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            .dstAccess = VK_ACCESS_2_SHADER_READ_BIT,
        },
        {
            .image = motionImage_,
            .range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
            .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcStage  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            .dstStage  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            .dstAccess = VK_ACCESS_2_SHADER_READ_BIT,
        },
        {
            .image = swapchain_->depthImage(),
            .range = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1},
            .oldLayout = depth_layouts::attachment(*ctx_),
            .newLayout = depth_layouts::readOnly(*ctx_),
            .srcStage  = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            .srcAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstStage  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                         VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
            .dstAccess = VK_ACCESS_2_SHADER_READ_BIT |
                         VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
        },
    };
    barrier::recordBatch(*ctx_, info.cmd, {}, {}, toRead);
}

void MainPass::shutdown() {
    grassPipeline_.reset();
    grassLayout_.reset();
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;

    skinnedPipelineTransparent_.reset();
    skinnedPipelineOpaque_.reset();
    skinnedLayout_.reset();
    // Phase 1D: bindless pipeline cleanup
    bindlessPipelineOpaque_.reset();
    bindlessLayout_.reset();
    staticPipelineTransparent_.reset();
    staticPipelineOpaque_.reset();
    staticLayout_.reset();
    // PART4 4a-1: no renderPass_ / framebuffers_ to release (dynamic rendering).
    ctx_ = nullptr;
    swapchain_ = nullptr;
}

// =============================================================================
// Phase 1D: createBindlessLayout
// =============================================================================
void MainPass::createBindlessLayout(VkDescriptorSetLayout frameSetLayout,
                                    VkDescriptorSetLayout bindlessSetLayout) {
    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pc.offset = 0;
    pc.size = sizeof(StaticBindlessPushConstants);

    VkDescriptorSetLayout setLayouts[2] = {frameSetLayout, bindlessSetLayout};
    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    lci.setLayoutCount = 2;
    lci.pSetLayouts = setLayouts;
    lci.pushConstantRangeCount = 1;
    lci.pPushConstantRanges = &pc;

    VkPipelineLayout blay = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(ctx_->device(), &lci, nullptr, &blay) != VK_SUCCESS) {
        throw std::runtime_error("MainPass::createBindlessLayout failed");
    }
    bindlessLayout_ = VkUnique<VkPipelineLayout>(ctx_->device(), blay);
}
