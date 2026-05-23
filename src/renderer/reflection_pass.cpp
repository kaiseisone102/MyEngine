// =============================================================================
// reflection_pass.cpp — 2B-3: execute 実装
// =============================================================================
#include "renderer/reflection_pass.h"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <stdexcept>

#include "renderer/main_pass.h"
#include "renderer/material.h"
#include "renderer/mesh.h"
#include "renderer/model.h"
#include "renderer/resource_factory.h"
#include "renderer/shader_util.h"
#include "renderer/terrain_mesh.h"
#include "renderer/vulkan_context.h"

void ReflectionPass::init(const InitInfo& info) {
    if (!info.ctx || !info.resources) {
        throw std::runtime_error("ReflectionPass::init: invalid info");
    }
    if (info.frameSetLayout == VK_NULL_HANDLE || info.bindlessSetLayout == VK_NULL_HANDLE) {
        throw std::runtime_error("ReflectionPass::init: layouts missing");
    }
    ctx_ = info.ctx;
    resources_ = info.resources;
    colorFormat_ = info.colorFormat;
    depthFormat_ = info.depthFormat;
    quality_ = info.quality;

    createRenderPass();
    createStaticLayout(info.frameSetLayout, info.bindlessSetLayout);  // S4-c: static uses bindless
    createSkinnedLayout(info.frameSetLayout, info.bindlessSetLayout);  // S5: set=1 bindless

    {
        const std::string vert = "triangle_vert.spv";
        const std::string frag = "triangle_frag.spv";
        PipelineBuildArgs args{staticLayout_, vert, frag, /*skinned=*/false};
        staticPipeline_ = buildPipeline(args, info.shaderDir);
    }
    {
        const std::string vert = "triangle_skinned_vert.spv";
        const std::string frag = "triangle_skinned_frag.spv";
        PipelineBuildArgs args{skinnedLayout_, vert, frag, /*skinned=*/true};
        skinnedPipeline_ = buildPipeline(args, info.shaderDir);
    }

    rebuild(info.quality, info.baseWidth, info.baseHeight);
}

void ReflectionPass::createRenderPass() {
    VkAttachmentDescription color{};
    color.format = colorFormat_;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription depth{};
    depth.format = depthFormat_;
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

    VkSubpassDependency depBegin{};
    depBegin.srcSubpass = VK_SUBPASS_EXTERNAL;
    depBegin.dstSubpass = 0;
    depBegin.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    depBegin.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    depBegin.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    depBegin.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkSubpassDependency depEnd{};
    depEnd.srcSubpass = 0;
    depEnd.dstSubpass = VK_SUBPASS_EXTERNAL;
    depEnd.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    depEnd.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    depEnd.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    depEnd.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkSubpassDependency deps[] = {depBegin, depEnd};
    VkAttachmentDescription attachments[] = {color, depth};

    VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    ci.attachmentCount = 2;
    ci.pAttachments = attachments;
    ci.subpassCount = 1;
    ci.pSubpasses = &sub;
    ci.dependencyCount = 2;
    ci.pDependencies = deps;
    if (vkCreateRenderPass(ctx_->device(), &ci, nullptr, &renderPass_) != VK_SUCCESS) {
        throw std::runtime_error("ReflectionPass: vkCreateRenderPass failed");
    }
}

void ReflectionPass::createStaticLayout(VkDescriptorSetLayout frameSetLayout,
                                          VkDescriptorSetLayout bindlessSetLayout) {
    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;  // S4-c: frag reads materialId
    pc.offset = 0;
    pc.size = sizeof(MainPass::StaticPushConstants);

    VkDescriptorSetLayout setLayouts[2] = {frameSetLayout, bindlessSetLayout};
    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    lci.setLayoutCount = 2;
    lci.pSetLayouts = setLayouts;
    lci.pushConstantRangeCount = 1;
    lci.pPushConstantRanges = &pc;
    if (vkCreatePipelineLayout(ctx_->device(), &lci, nullptr, &staticLayout_) != VK_SUCCESS) {
        throw std::runtime_error("ReflectionPass: static layout failed");
    }
}

void ReflectionPass::createSkinnedLayout(VkDescriptorSetLayout frameSetLayout,
                                              VkDescriptorSetLayout bindlessSetLayout) {
    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;  // S5: frag reads materialId
    pc.offset = 0;
    pc.size = sizeof(MainPass::SkinnedPushConstants);

    VkDescriptorSetLayout setLayouts[2] = {frameSetLayout, bindlessSetLayout};
    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    lci.setLayoutCount = 2;
    lci.pSetLayouts = setLayouts;
    lci.pushConstantRangeCount = 1;
    lci.pPushConstantRanges = &pc;
    if (vkCreatePipelineLayout(ctx_->device(), &lci, nullptr, &skinnedLayout_) != VK_SUCCESS) {
        throw std::runtime_error("ReflectionPass: skinned layout failed");
    }
}

VkPipeline ReflectionPass::buildPipeline(const PipelineBuildArgs& args,
                                          const std::string& shaderDir) {
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
    attrs[3] = {3, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, normal))};
    attrs[4] = {4, 0, VK_FORMAT_R32G32B32A32_SINT,
                static_cast<uint32_t>(offsetof(Vertex, jointIndices))};
    attrs[5] = {5, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                static_cast<uint32_t>(offsetof(Vertex, jointWeights))};

    VkPipelineVertexInputStateCreateInfo vi{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &bind;
    vi.vertexAttributeDescriptionCount = args.skinned ? 6 : 4;  // Phase 1B-6
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
    rs.frontFace = VK_FRONT_FACE_CLOCKWISE;

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
    blendAtt.blendEnable = VK_FALSE;

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

    (void)args.skinned;
    if (r != VK_SUCCESS) {
        throw std::runtime_error("ReflectionPass: vkCreateGraphicsPipelines failed");
    }
    return pipeline;
}

void ReflectionPass::createFramebuffer() {
    if (!target_.valid()) return;

    VkImageView views[2] = {target_.color().view(), target_.depth().view()};
    VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fi.renderPass = renderPass_;
    fi.attachmentCount = 2;
    fi.pAttachments = views;
    fi.width = target_.extent().width;
    fi.height = target_.extent().height;
    fi.layers = 1;
    if (vkCreateFramebuffer(ctx_->device(), &fi, nullptr, &framebuffer_) != VK_SUCCESS) {
        throw std::runtime_error("ReflectionPass: vkCreateFramebuffer failed");
    }
}

void ReflectionPass::rebuild(ReflectionQuality quality, uint32_t baseWidth, uint32_t baseHeight) {
    destroyTargetAndFramebuffer();

    quality_ = quality;
    if (quality == ReflectionQuality::Off) {
        std::cout << "[ReflectionPass] disabled (quality=Off)\n";
        return;
    }

    const float scale = reflectionQualityScale(quality);
    const uint32_t w = std::max(1u, static_cast<uint32_t>(baseWidth * scale));
    const uint32_t h = std::max(1u, static_cast<uint32_t>(baseHeight * scale));

    target_.init(ctx_, resources_, w, h, colorFormat_, depthFormat_);
    createFramebuffer();

    std::cout << "[ReflectionPass] rebuilt: quality=" << reflectionQualityName(quality)
              << " (" << w << "x" << h << ")\n";
}

void ReflectionPass::destroyTargetAndFramebuffer() {
    if (framebuffer_ != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(ctx_->device(), framebuffer_, nullptr);
        framebuffer_ = VK_NULL_HANDLE;
    }
    target_.shutdown();
}

namespace {

void drawMeshList(VkCommandBuffer cmd, VkPipelineLayout layout,
                  const Mesh* mesh, const std::vector<MeshDrawItem>& list) {
    if (!mesh || list.empty()) return;
    mesh->bind(cmd);
    for (const MeshDrawItem& item : list) {
        // S4-c: set=1 is the bindless array (bound in execute); no per-material bind
        MainPass::StaticPushConstants pc{};
        pc.model = item.model;
        pc.alpha = item.alpha;
        pc.materialId = 0;  // reflection uses default for now
        vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(MainPass::StaticPushConstants), &pc);
        vkCmdDrawIndexed(cmd, mesh->indexCount(), 1, 0, 0, 0);
    }
}

void drawStaticModelList(VkCommandBuffer cmd, VkPipelineLayout layout,
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
        pc.materialId = 0;  // reflection uses default for now
        vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(MainPass::StaticPushConstants), &pc);

        for (const SubMesh& sm : curModel->subMeshes()) {
            if (sm.indexCount == 0) continue;
            // S4-c: no per-material bind; set=1 is the bindless array
            sm.bind(cmd);
            vkCmdDrawIndexed(cmd, sm.indexCount, 1, 0, 0, 0);
        }
    }
}

void drawTerrainList(VkCommandBuffer cmd, VkPipelineLayout layout,
                      const std::vector<TerrainDrawItem>& list) {
    if (list.empty()) return;
    for (const TerrainDrawItem& item : list) {
        if (!item.terrain) continue;
        // S4-c: no per-material bind; set=1 is the bindless array
        MainPass::StaticPushConstants pc{};
        pc.model = item.model;
        pc.alpha = item.alpha;
        pc.materialId = 0;  // reflection uses default for now
        vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(MainPass::StaticPushConstants), &pc);
        item.terrain->bind(cmd);
        vkCmdDrawIndexed(cmd, item.terrain->indexCount(), 1, 0, 0, 0);
    }
}

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
            sm.bind(cmd);
            vkCmdDrawIndexed(cmd, sm.indexCount, 1, 0, 0, 0);
        }
    }
}

}  // namespace

void ReflectionPass::execute(const ExecuteInfo& info) {
    if (quality_ == ReflectionQuality::Off) return;
    if (framebuffer_ == VK_NULL_HANDLE) return;
    if (info.cmd == VK_NULL_HANDLE || info.frameSet == VK_NULL_HANDLE) return;

    const VkExtent2D extent = target_.extent();

    VkClearValue clears[2]{};
    clears[0].color = {{info.clearColor.r, info.clearColor.g, info.clearColor.b,
                         info.clearColor.a}};
    clears[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp.renderPass = renderPass_;
    rp.framebuffer = framebuffer_;
    rp.renderArea = {{0, 0}, extent};
    rp.clearValueCount = 2;
    rp.pClearValues = clears;

    vkCmdBeginRenderPass(info.cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{
        0.f, 0.f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.f, 1.f};
    VkRect2D scissor{{0, 0}, extent};

    // ── Static (mesh + staticModel + terrain) ──
    const bool hasStatic =
        (info.mesh && info.meshDrawListOpaque && !info.meshDrawListOpaque->empty()) ||
        (info.staticModelDrawListOpaque && !info.staticModelDrawListOpaque->empty()) ||
        (info.terrainDrawListOpaque && !info.terrainDrawListOpaque->empty());

    if (hasStatic) {
        vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, staticPipeline_);
        vkCmdSetViewport(info.cmd, 0, 1, &viewport);
        vkCmdSetScissor(info.cmd, 0, 1, &scissor);
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, staticLayout_, 0, 1,
                                &info.frameSet, 0, nullptr);
        // S4-c: set=1 bindless texture array
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, staticLayout_, 1, 1,
                                &info.bindlessSet, 0, nullptr);

        if (info.mesh && info.meshDrawListOpaque) {
            drawMeshList(info.cmd, staticLayout_, info.mesh,
                         *info.meshDrawListOpaque);
        }
        if (info.staticModelDrawListOpaque) {
            drawStaticModelList(info.cmd, staticLayout_,
                                 *info.staticModelDrawListOpaque);
        }
        if (info.terrainDrawListOpaque) {
            drawTerrainList(info.cmd, staticLayout_,
                            *info.terrainDrawListOpaque);
        }
    }

    // ── Skinned (player + enemies) ──
    if (info.modelDrawListOpaque && !info.modelDrawListOpaque->empty()) {
        vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skinnedPipeline_);
        vkCmdSetViewport(info.cmd, 0, 1, &viewport);
        vkCmdSetScissor(info.cmd, 0, 1, &scissor);
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skinnedLayout_, 0, 1,
                                &info.frameSet, 0, nullptr);
        // S5: set=1 bindless texture array
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skinnedLayout_, 1, 1,
                                &info.bindlessSet, 0, nullptr);
        drawSkinnedList(info.cmd, skinnedLayout_, info.skinAddress, *info.modelDrawListOpaque);
    }

    vkCmdEndRenderPass(info.cmd);
}

void ReflectionPass::shutdown() {
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;
    destroyTargetAndFramebuffer();

    if (skinnedPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(ctx_->device(), skinnedPipeline_, nullptr);
        skinnedPipeline_ = VK_NULL_HANDLE;
    }
    if (skinnedLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(ctx_->device(), skinnedLayout_, nullptr);
        skinnedLayout_ = VK_NULL_HANDLE;
    }
    if (staticPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(ctx_->device(), staticPipeline_, nullptr);
        staticPipeline_ = VK_NULL_HANDLE;
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
    resources_ = nullptr;
}
