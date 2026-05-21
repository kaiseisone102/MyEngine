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
#include "renderer/particle_pass.h"
#include "renderer/shader_util.h"
#include "renderer/swapchain.h"
#include "renderer/terrain_mesh.h"
#include "renderer/vulkan_context.h"
#include "renderer/water_pass.h"

namespace {

void drawMeshList(VkCommandBuffer cmd, VkPipelineLayout layout, VkDescriptorSet defaultMatSet,
                    const Mesh* mesh, const std::vector<MeshDrawItem>& list) {
    if (!mesh || list.empty()) return;
    mesh->bind(cmd);
    VkDescriptorSet lastMatSet = VK_NULL_HANDLE;
    for (const MeshDrawItem& item : list) {
        VkDescriptorSet matSet = defaultMatSet;
        if (item.material && item.material->descriptorSet() != VK_NULL_HANDLE) {
            matSet = item.material->descriptorSet();
        }
        if (matSet != lastMatSet) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1, &matSet,
                                      0, nullptr);
            lastMatSet = matSet;
        }
        MainPass::StaticPushConstants pc{};
        pc.model = item.model;
        pc.alpha = item.alpha;
        vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                            sizeof(MainPass::StaticPushConstants), &pc);
        vkCmdDrawIndexed(cmd, mesh->indexCount(), 1, 0, 0, 0);
    }
}

void drawStaticModelList(VkCommandBuffer cmd, VkPipelineLayout layout,
                            VkDescriptorSet defaultMatSet,
                            const std::vector<StaticModelDrawItem>& list) {
    if (list.empty()) return;
    const Model* curModel = nullptr;
    const std::vector<Material>* curMaterials = nullptr;

    for (const StaticModelDrawItem& item : list) {
        if (!item.sourceModel) continue;
        if (item.sourceModel != curModel) {
            curModel = item.sourceModel;
            curMaterials = &curModel->materials();
        }

        MainPass::StaticPushConstants pc{};
        pc.model = item.model;
        pc.alpha = item.alpha;
        vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                            sizeof(MainPass::StaticPushConstants), &pc);

        for (const SubMesh& sm : curModel->subMeshes()) {
            if (sm.indexCount == 0) continue;
            VkDescriptorSet matSet = defaultMatSet;
            if (curMaterials && sm.materialIndex < curMaterials->size()) {
                VkDescriptorSet ms = (*curMaterials)[sm.materialIndex].descriptorSet();
                if (ms != VK_NULL_HANDLE) matSet = ms;
            }
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1, &matSet,
                                      0, nullptr);
            sm.bind(cmd);
            vkCmdDrawIndexed(cmd, sm.indexCount, 1, 0, 0, 0);
        }
    }
}

void drawTerrainList(VkCommandBuffer cmd, VkPipelineLayout layout, VkDescriptorSet defaultMatSet,
                       const std::vector<TerrainDrawItem>& list) {
    if (list.empty()) return;
    VkDescriptorSet lastMatSet = VK_NULL_HANDLE;
    for (const TerrainDrawItem& item : list) {
        if (!item.terrain) continue;
        VkDescriptorSet matSet = defaultMatSet;
        if (item.material && item.material->descriptorSet() != VK_NULL_HANDLE) {
            matSet = item.material->descriptorSet();
        }
        if (matSet != lastMatSet) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1, &matSet,
                                      0, nullptr);
            lastMatSet = matSet;
        }
        MainPass::StaticPushConstants pc{};
        pc.model = item.model;
        pc.alpha = item.alpha;
        vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                            sizeof(MainPass::StaticPushConstants), &pc);
        item.terrain->bind(cmd);
        vkCmdDrawIndexed(cmd, item.terrain->indexCount(), 1, 0, 0, 0);
    }
}

void drawSkinnedList(VkCommandBuffer cmd, VkPipelineLayout layout, VkDescriptorSet defaultMatSet,
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
        vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                            sizeof(MainPass::SkinnedPushConstants), &pc);
        for (const SubMesh& sm : curModel->subMeshes()) {
            if (sm.indexCount == 0) continue;
            VkDescriptorSet matSet = defaultMatSet;
            if (curMaterials && sm.materialIndex < curMaterials->size()) {
                VkDescriptorSet ms = (*curMaterials)[sm.materialIndex].descriptorSet();
                if (ms != VK_NULL_HANDLE) matSet = ms;
            }
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1, &matSet,
                                      0, nullptr);
            sm.bind(cmd);
            vkCmdDrawIndexed(cmd, sm.indexCount, 1, 0, 0, 0);
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

    createStaticLayout(info.frameSetLayout, info.materialSetLayout);
    {
        PipelineBuildArgs argsOp{staticLayout_, "triangle_vert.spv", "triangle_frag.spv", false};
        staticPipelineOpaque_ = buildPipeline(argsOp, info.shaderDir);
        PipelineBuildArgs argsTr{staticLayout_, "triangle_vert.spv", "triangle_frag.spv", true};
        staticPipelineTransparent_ = buildPipeline(argsTr, info.shaderDir);
    }

    createSkinnedLayout(info.frameSetLayout, info.materialSetLayout);
    {
        PipelineBuildArgs argsOp{skinnedLayout_, "triangle_skinned_vert.spv", "triangle_skinned_frag.spv", false, true};
        skinnedPipelineOpaque_ = buildPipeline(argsOp, info.shaderDir);
        PipelineBuildArgs argsTr{skinnedLayout_, "triangle_skinned_vert.spv", "triangle_skinned_frag.spv", true, true};
        skinnedPipelineTransparent_ = buildPipeline(argsTr, info.shaderDir);
    }

    // === Phase 1E: instanced pipeline (opaque) ===
    createInstancedLayout(info.frameSetLayout);
    {
        PipelineBuildArgs argsInst{instancedLayout_, "triangle_instanced_vert.spv", "triangle_instanced_frag.spv", false};
        instancedPipelineOpaque_ = buildPipeline(argsInst, info.shaderDir);
        std::cout << "[MainPass] instanced pipeline created\n";
    }

    // === Phase 1D: bindless pipeline (opaque only, test cube) ===
    if (info.bindlessSetLayout != VK_NULL_HANDLE) {
        createBindlessLayout(info.frameSetLayout, info.bindlessSetLayout);
        PipelineBuildArgs argsBl{bindlessLayout_, "triangle_bindless_vert.spv",
                                  "triangle_bindless_frag.spv", false};
        bindlessPipelineOpaque_ = buildPipeline(argsBl, info.shaderDir);
        std::cout << "[MainPass] bindless pipeline created\n";
        // Phase 1F: grass pipeline (alpha-tested, bindless texture, no cull)
        createGrassLayout(info.frameSetLayout, info.bindlessSetLayout);
        PipelineBuildArgs argsGrass{grassLayout_, "grass_instanced_vert.spv",
                                   "grass_instanced_frag.spv", false};
        argsGrass.noCull = true;
        grassPipeline_ = buildPipeline(argsGrass, info.shaderDir);
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
    if (vkCreateRenderPass(ctx_->device(), &ci, nullptr, &renderPass_) != VK_SUCCESS)
        throw std::runtime_error("MainPass: vkCreateRenderPass failed");
}

void MainPass::createStaticLayout(VkDescriptorSetLayout frameSetLayout,
                                     VkDescriptorSetLayout materialSetLayout) {
    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pc.offset = 0;
    pc.size = sizeof(StaticPushConstants);

    VkDescriptorSetLayout setLayouts[2] = {frameSetLayout, materialSetLayout};
    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    lci.setLayoutCount = 2;
    lci.pSetLayouts = setLayouts;
    lci.pushConstantRangeCount = 1;
    lci.pPushConstantRanges = &pc;
    if (vkCreatePipelineLayout(ctx_->device(), &lci, nullptr, &staticLayout_) != VK_SUCCESS) {
        throw std::runtime_error("MainPass: static layout failed");
    }
}

void MainPass::createInstancedLayout(VkDescriptorSetLayout frameSetLayout) {
    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pc.offset = 0;
    pc.size = sizeof(InstancedPushConstants);

    // Only set=0 (frame UBO). No material set in this minimal instanced path.
    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    lci.setLayoutCount = 1;
    lci.pSetLayouts = &frameSetLayout;
    lci.pushConstantRangeCount = 1;
    lci.pPushConstantRanges = &pc;
    if (vkCreatePipelineLayout(ctx_->device(), &lci, nullptr, &instancedLayout_) != VK_SUCCESS) {
        throw std::runtime_error("MainPass: instanced layout failed");
    }
}

void MainPass::createGrassLayout(VkDescriptorSetLayout frameSetLayout,
                                 VkDescriptorSetLayout bindlessSetLayout) {
    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pc.offset = 0;
    pc.size = sizeof(InstancedPushConstants);

    // set=0 frame UBO, set=1 bindless texture array (for grass texture).
    VkDescriptorSetLayout setLayouts[2] = {frameSetLayout, bindlessSetLayout};
    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    lci.setLayoutCount = 2;
    lci.pSetLayouts = setLayouts;
    lci.pushConstantRangeCount = 1;
    lci.pPushConstantRanges = &pc;
    if (vkCreatePipelineLayout(ctx_->device(), &lci, nullptr, &grassLayout_) != VK_SUCCESS) {
        throw std::runtime_error("MainPass: grass layout failed");
    }
}

void MainPass::createSkinnedLayout(VkDescriptorSetLayout frameSetLayout,
                                       VkDescriptorSetLayout materialSetLayout) {
    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pc.offset = 0;
    pc.size = sizeof(SkinnedPushConstants);

    VkDescriptorSetLayout setLayouts[2] = {frameSetLayout, materialSetLayout};
    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    lci.setLayoutCount = 2;
    lci.pSetLayouts = setLayouts;
    lci.pushConstantRangeCount = 1;
    lci.pPushConstantRanges = &pc;
    if (vkCreatePipelineLayout(ctx_->device(), &lci, nullptr, &skinnedLayout_) != VK_SUCCESS) {
        throw std::runtime_error("MainPass: skinned layout failed");
    }
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
    ds.depthCompareOp = VK_COMPARE_OP_LESS;

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
    pci.renderPass = renderPass_;
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
    framebuffers_.resize(1);
    VkImageView attachments[] = {hdrColorView_, swapchain_->depthView()};
    VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    ci.renderPass = renderPass_;
    ci.attachmentCount = 2;
    ci.pAttachments = attachments;
    ci.width = extent.width;
    ci.height = extent.height;
    ci.layers = 1;
    if (vkCreateFramebuffer(ctx_->device(), &ci, nullptr, &framebuffers_[0]) != VK_SUCCESS)
        throw std::runtime_error("MainPass: vkCreateFramebuffer failed");
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
    if (framebuffers_.empty())
        throw std::runtime_error("MainPass::execute: framebuffers not created");
    if (info.defaultMaterialSet == VK_NULL_HANDLE)
        throw std::runtime_error("MainPass::execute: defaultMaterialSet missing");

    const VkExtent2D extent = swapchain_->extent();

    VkClearValue clearValues[2]{};
    clearValues[0].color = {
        {info.clearColor.r, info.clearColor.g, info.clearColor.b, info.clearColor.a}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp.renderPass = renderPass_;
    rp.framebuffer = framebuffers_[0];  // Phase 1H-2 (single HDR framebuffer)
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
        vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, staticPipelineOpaque_);
        vkCmdSetViewport(info.cmd, 0, 1, &viewport);
        vkCmdSetScissor(info.cmd, 0, 1, &scissor);
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, staticLayout_, 0, 1,
                                  &info.frameSet, 0, nullptr);

        if (info.mesh && meshOp)
            drawMeshList(info.cmd, staticLayout_, info.defaultMaterialSet, info.mesh, *meshOp);
        if (staticOp)
            drawStaticModelList(info.cmd, staticLayout_, info.defaultMaterialSet, *staticOp);
        if (terrainOp)
            drawTerrainList(info.cmd, staticLayout_, info.defaultMaterialSet, *terrainOp);
    }

    // === Phase 1E: instanced opaque meshes ===
    if (info.instancedMeshDrawListOpaque && !info.instancedMeshDrawListOpaque->empty()
        && info.instanceBufferAddress != 0) {
        vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, instancedPipelineOpaque_);
        vkCmdSetViewport(info.cmd, 0, 1, &viewport);
        vkCmdSetScissor(info.cmd, 0, 1, &scissor);
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, instancedLayout_, 0, 1,
                                &info.frameSet, 0, nullptr);
        for (const InstancedMeshDrawItem& item : *info.instancedMeshDrawListOpaque) {
            if (!item.mesh || item.instances.empty()) continue;
            item.mesh->bind(info.cmd);
            InstancedPushConstants pc{};
            pc.instanceBuffer = info.instanceBufferAddress;
            pc.albedoIdx = -1;
            pc.alpha = item.alpha;
            vkCmdPushConstants(info.cmd, instancedLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                               sizeof(InstancedPushConstants), &pc);
            const uint32_t count = static_cast<uint32_t>(item.instances.size());
            // firstInstance = item.instanceOffset -> gl_InstanceIndex starts there
            vkCmdDrawIndexed(info.cmd, item.mesh->indexCount(), count, 0, 0, item.instanceOffset);
        }
    }

    // === Phase 1F: instanced grass (alpha-tested, bindless texture) ===
    if (info.grassDrawList && !info.grassDrawList->empty()
        && info.instanceBufferAddress != 0 && grassPipeline_ != VK_NULL_HANDLE
        && info.bindlessSet != VK_NULL_HANDLE) {
        vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, grassPipeline_);
        vkCmdSetViewport(info.cmd, 0, 1, &viewport);
        vkCmdSetScissor(info.cmd, 0, 1, &scissor);
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, grassLayout_, 0, 1,
                                &info.frameSet, 0, nullptr);
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, grassLayout_, 1, 1,
                                &info.bindlessSet, 0, nullptr);
        for (const InstancedMeshDrawItem& item : *info.grassDrawList) {
            if (!item.mesh || item.instances.empty()) continue;
            item.mesh->bind(info.cmd);
            InstancedPushConstants pc{};
            pc.instanceBuffer = info.instanceBufferAddress;
            pc.albedoIdx = item.material ? 0 : 0;  // grass texture bindless idx (0)
            pc.alpha = item.alpha;
            vkCmdPushConstants(info.cmd, grassLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                               sizeof(InstancedPushConstants), &pc);
            const uint32_t count = static_cast<uint32_t>(item.instances.size());
            vkCmdDrawIndexed(info.cmd, item.mesh->indexCount(), count, 0, 0, item.instanceOffset);
        }
    }

    if (modelOp && !modelOp->empty()) {
        vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skinnedPipelineOpaque_);
        vkCmdSetViewport(info.cmd, 0, 1, &viewport);
        vkCmdSetScissor(info.cmd, 0, 1, &scissor);
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skinnedLayout_, 0, 1,
                                  &info.frameSet, 0, nullptr);
        drawSkinnedList(info.cmd, skinnedLayout_, info.defaultMaterialSet, info.skinAddress, *modelOp);
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
        vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, staticPipelineTransparent_);
        vkCmdSetViewport(info.cmd, 0, 1, &viewport);
        vkCmdSetScissor(info.cmd, 0, 1, &scissor);
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, staticLayout_, 0, 1,
                                  &info.frameSet, 0, nullptr);

        if (info.mesh && meshTr)
            drawMeshList(info.cmd, staticLayout_, info.defaultMaterialSet, info.mesh, *meshTr);
        if (staticTr)
            drawStaticModelList(info.cmd, staticLayout_, info.defaultMaterialSet, *staticTr);
        if (terrainTr)
            drawTerrainList(info.cmd, staticLayout_, info.defaultMaterialSet, *terrainTr);
    }

    if (modelTr && !modelTr->empty()) {
        vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skinnedPipelineTransparent_);
        vkCmdSetViewport(info.cmd, 0, 1, &viewport);
        vkCmdSetScissor(info.cmd, 0, 1, &scissor);
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skinnedLayout_, 0, 1,
                                  &info.frameSet, 0, nullptr);
        drawSkinnedList(info.cmd, skinnedLayout_, info.defaultMaterialSet, info.skinAddress, *modelTr);
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
    if (bindlessPipelineOpaque_ != VK_NULL_HANDLE && info.bindlessSet != VK_NULL_HANDLE &&
        info.mesh) {
        vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bindlessPipelineOpaque_);

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
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bindlessLayout_, 0, 1,
                                &info.frameSet, 0, nullptr);
        // set=1 bindless texture array (1024 capacity)
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bindlessLayout_, 1, 1,
                                &info.bindlessSet, 0, nullptr);

        // Cube geometry (already created in AssetRegistry::defaultMesh_)
        info.mesh->bind(info.cmd);

        // Push constant: position + texture index
        StaticBindlessPushConstants pc{};
        pc.model = glm::translate(glm::mat4(1.f), glm::vec3(3.f, 5.f, 3.f));
        pc.alpha = 1.0f;
        pc.albedoIdx = 5;  // grass_field

        vkCmdPushConstants(info.cmd, bindlessLayout_,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(pc), &pc);

        // Use the default mesh (cube) index count.
        // Note: info.mesh is a Mesh* whose indexCount() returns the cube's index count.
        vkCmdDrawIndexed(info.cmd, info.mesh->indexCount(), 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(info.cmd);
}

void MainPass::shutdown() {
    if (grassPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(ctx_->device(), grassPipeline_, nullptr);
        grassPipeline_ = VK_NULL_HANDLE;
    }
    if (grassLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(ctx_->device(), grassLayout_, nullptr);
        grassLayout_ = VK_NULL_HANDLE;
    }
    if (instancedPipelineOpaque_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(ctx_->device(), instancedPipelineOpaque_, nullptr);
        instancedPipelineOpaque_ = VK_NULL_HANDLE;
    }
    if (instancedLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(ctx_->device(), instancedLayout_, nullptr);
        instancedLayout_ = VK_NULL_HANDLE;
    }
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;
    destroyFramebuffers();

    if (skinnedPipelineTransparent_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(ctx_->device(), skinnedPipelineTransparent_, nullptr);
        skinnedPipelineTransparent_ = VK_NULL_HANDLE;
    }
    if (skinnedPipelineOpaque_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(ctx_->device(), skinnedPipelineOpaque_, nullptr);
        skinnedPipelineOpaque_ = VK_NULL_HANDLE;
    }
    if (skinnedLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(ctx_->device(), skinnedLayout_, nullptr);
        skinnedLayout_ = VK_NULL_HANDLE;
    }
    // Phase 1D: bindless pipeline cleanup
    if (bindlessPipelineOpaque_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(ctx_->device(), bindlessPipelineOpaque_, nullptr);
        bindlessPipelineOpaque_ = VK_NULL_HANDLE;
    }
    if (bindlessLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(ctx_->device(), bindlessLayout_, nullptr);
        bindlessLayout_ = VK_NULL_HANDLE;
    }
    if (staticPipelineTransparent_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(ctx_->device(), staticPipelineTransparent_, nullptr);
        staticPipelineTransparent_ = VK_NULL_HANDLE;
    }
    if (staticPipelineOpaque_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(ctx_->device(), staticPipelineOpaque_, nullptr);
        staticPipelineOpaque_ = VK_NULL_HANDLE;
    }
    if (staticLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(ctx_->device(), staticLayout_, nullptr);
        staticLayout_ = VK_NULL_HANDLE;
    }
    if (renderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(ctx_->device(), renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }
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

    if (vkCreatePipelineLayout(ctx_->device(), &lci, nullptr, &bindlessLayout_) != VK_SUCCESS) {
        throw std::runtime_error("MainPass::createBindlessLayout failed");
    }
}
