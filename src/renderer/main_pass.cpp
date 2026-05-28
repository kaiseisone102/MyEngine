// =============================================================================
// main_pass.cpp — Phase 1C: opaque/transparent + Water + DebugLine + Particle
//                            + HUD + ImGui
// =============================================================================
#include "renderer/main_pass.h"
#include <glm/gtc/matrix_transform.hpp>

#include <cstddef>
#include <iostream>
#include <stdexcept>

#include "renderer/debug_line_pass.h"
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
    hdrColorFormat_ = info.hdrColorFormat;

    createRenderPass();

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

    createFramebuffers();
}

void MainPass::createRenderPass() {
    VkAttachmentDescription color{};
    color.format = hdrColorFormat_;  // Phase 1H-2 (was swapchain colorFormat)
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  // Phase 1H-2: PostPass will sample this

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
    VkRenderPass rp = VK_NULL_HANDLE;
    if (vkCreateRenderPass(ctx_->device(), &ci, nullptr, &rp) != VK_SUCCESS)
        throw std::runtime_error("MainPass: vkCreateRenderPass failed");
    renderPass_ = VkUnique<VkRenderPass>(ctx_->device(), rp);
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

    VkPipelineColorBlendAttachmentState blendAtt{};
    blendAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (args.transparent) {
        blendAtt.blendEnable = VK_TRUE;
        blendAtt.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAtt.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAtt.colorBlendOp = VK_BLEND_OP_ADD;
        blendAtt.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAtt.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blendAtt.alphaBlendOp = VK_BLEND_OP_ADD;
    } else {
        blendAtt.blendEnable = VK_FALSE;
    }

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
    pci.layout = args.layout;
    pci.renderPass = renderPass_.get();
    pci.subpass = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    const VkResult r =
        vkCreateGraphicsPipelines(ctx_->device(), VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline);

    vkDestroyShaderModule(ctx_->device(), vert, nullptr);
    vkDestroyShaderModule(ctx_->device(), frag, nullptr);

    if (r != VK_SUCCESS) throw std::runtime_error("MainPass: vkCreateGraphicsPipelines failed");
    return pipeline;
}

void MainPass::createFramebuffers() {
    // Phase 1H-2: write into the HDR target (single framebuffer, not per-swapchain-image)
    const VkExtent2D extent = swapchain_->extent();
    framebuffers_.clear();
    VkImageView attachments[] = {hdrColorView_, swapchain_->depthView()};
    VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    ci.renderPass = renderPass_.get();
    ci.attachmentCount = 2;
    ci.pAttachments = attachments;
    ci.width = extent.width;
    ci.height = extent.height;
    ci.layers = 1;
    VkFramebuffer fb = VK_NULL_HANDLE;
    if (vkCreateFramebuffer(ctx_->device(), &ci, nullptr, &fb) != VK_SUCCESS)
        throw std::runtime_error("MainPass: vkCreateFramebuffer failed");
    framebuffers_.emplace_back(ctx_->device(), fb);
}

void MainPass::destroyFramebuffers() {
    if (!ctx_) return;
    // VkUnique elements free their framebuffers on clear.
    framebuffers_.clear();
}

void MainPass::onSwapchainResized() {
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;
    destroyFramebuffers();
    createFramebuffers();
}

void MainPass::execute(const ExecuteInfo& info) {
    if (!info.cmd) throw std::runtime_error("MainPass::execute: invalid cmd");
    if (framebuffers_.empty())
        throw std::runtime_error("MainPass::execute: framebuffers not created");

    const VkExtent2D extent = swapchain_->extent();

    VkClearValue clearValues[2]{};
    clearValues[0].color = {
        {info.clearColor.r, info.clearColor.g, info.clearColor.b, info.clearColor.a}};
    clearValues[1].depthStencil = {0.0f, 0};  // reverse-Z: clear to far (= 0.0)

    VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp.renderPass = renderPass_.get();
    rp.framebuffer = framebuffers_[0].get();  // Phase 1H-2 (single HDR framebuffer)
    rp.renderArea = {{0, 0}, extent};
    rp.clearValueCount = 2;
    rp.pClearValues = clearValues;

    vkCmdBeginRenderPass(info.cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

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
            const bool useIndirect =
                info.indirectCommandBuffer != VK_NULL_HANDLE && ctx_->drawIndirectFirstInstance();
            if (useIndirect) {
                const uint32_t stride = static_cast<uint32_t>(sizeof(VkDrawIndexedIndirectCommand));
                const uint32_t drawCount = static_cast<uint32_t>(draws.size());
                uint32_t runStart = 0;
                while (runStart < drawCount) {
                    const uint32_t block = draws[runStart].blockIndex;
                    uint32_t runEnd = runStart + 1;
                    while (runEnd < drawCount && draws[runEnd].blockIndex == block) ++runEnd;
                    info.geometry->bindBlock(info.cmd, block);
                    const uint32_t runLen = runEnd - runStart;
                    const VkDeviceSize off = static_cast<VkDeviceSize>(runStart) * stride;
                    if (ctx_->multiDrawIndirect()) {
                        vkCmdDrawIndexedIndirect(info.cmd, info.indirectCommandBuffer, off, runLen, stride);
                    } else {
                        for (uint32_t k = 0; k < runLen; ++k)
                            vkCmdDrawIndexedIndirect(info.cmd, info.indirectCommandBuffer,
                                                     off + static_cast<VkDeviceSize>(k) * stride, 1, stride);
                    }
                    runStart = runEnd;
                }
            } else {
                uint32_t boundBlock = UINT32_MAX;
                for (const static_cull::PreparedDraw& pd : draws) {
                    if (pd.blockIndex != boundBlock) { info.geometry->bindBlock(info.cmd, pd.blockIndex); boundBlock = pd.blockIndex; }
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

    if (info.hudPass && info.hud) {
        HudPass::ExecuteInfo hi{};
        hi.cmd = info.cmd;
        hi.drawList = info.hud;
        hi.screenW = info.screenW;
        hi.screenH = info.screenH;
        info.hudPass->execute(hi);
    }

    if (info.imgui) info.imgui->recordDrawCommands(info.cmd);

    // ============================================================
    // Phase 1D-2d: bindless test cube (floating above world origin)
    //   Demonstrates that a SINGLE draw using the bindless texture array
    //   can pick any texture by index, without any per-material descriptor
    //   set binding. Here we use index 5 (grass_field) on a cube.
    // ============================================================
    if (bindlessPipelineOpaque_ && info.bindlessSet != VK_NULL_HANDLE &&
        info.mesh) {
        vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bindlessPipelineOpaque_.get());

        VkViewport vp{};
        vp.x = 0.f;
        vp.y = 0.f;
        vp.width = static_cast<float>(extent.width);
        vp.height = static_cast<float>(extent.height);
        vp.minDepth = 0.f;
        vp.maxDepth = 1.f;
        VkRect2D sc{{0, 0}, extent};
        vkCmdSetViewport(info.cmd, 0, 1, &vp);
        vkCmdSetScissor(info.cmd, 0, 1, &sc);

        // set=0 frame uniforms
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bindlessLayout_.get(), 0, 1,
                                &info.frameSet, 0, nullptr);
        // set=1 bindless texture array (1024 capacity)
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bindlessLayout_.get(), 1, 1,
                                &info.bindlessSet, 0, nullptr);

        // Cube geometry (already created in AssetRegistry::defaultMesh_)
        info.mesh->bind(info.cmd);

        // Push constant: position + texture index
        StaticBindlessPushConstants pc{};
        pc.model = glm::translate(glm::mat4(1.f), glm::vec3(3.f, 5.f, 3.f));
        pc.alpha = 1.0f;
        pc.albedoIdx = 5;  // grass_field

        vkCmdPushConstants(info.cmd, bindlessLayout_.get(),
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(pc), &pc);

        // Use the default mesh (cube) index count.
        // Note: info.mesh is a Mesh* whose indexCount() returns the cube's index count.
        vkCmdDrawIndexed(info.cmd, info.mesh->indexCount(), 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(info.cmd);
}

void MainPass::shutdown() {
    grassPipeline_.reset();
    grassLayout_.reset();
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;
    destroyFramebuffers();

    skinnedPipelineTransparent_.reset();
    skinnedPipelineOpaque_.reset();
    skinnedLayout_.reset();
    // Phase 1D: bindless pipeline cleanup
    bindlessPipelineOpaque_.reset();
    bindlessLayout_.reset();
    staticPipelineTransparent_.reset();
    staticPipelineOpaque_.reset();
    staticLayout_.reset();
    renderPass_.reset();
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
