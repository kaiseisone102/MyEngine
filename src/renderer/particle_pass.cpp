// =============================================================================
// particle_pass.cpp — GPU Instancing + 距離フェード
// =============================================================================
#include "renderer/particle_pass.h"

#include <cstddef>
#include <cstring>
#include <stdexcept>

#include <glm/glm.hpp>

#include "renderer/resource_factory.h"
#include "renderer/shader_util.h"
#include "renderer/swapchain.h"
#include "renderer/vulkan_context.h"

namespace {

struct QuadVertex {
    glm::vec2 corner;
};

constexpr QuadVertex kQuadVertices[4] = {
    {{-0.5f, -0.5f}},
    {{+0.5f, -0.5f}},
    {{-0.5f, +0.5f}},
    {{+0.5f, +0.5f}},
};

constexpr uint16_t kQuadIndices[6] = {
    0, 1, 2,
    2, 1, 3,
};

}  // namespace

void ParticlePass::init(const InitInfo& info) {
    if (!info.ctx || !info.resources || !info.swapchain)
        throw std::runtime_error("ParticlePass::init: invalid info");
    if (info.mainRenderPass == VK_NULL_HANDLE || info.frameSetLayout == VK_NULL_HANDLE)
        throw std::runtime_error("ParticlePass::init: missing renderPass/frameSetLayout");

    ctx_ = info.ctx;
    resources_ = info.resources;
    swapchain_ = info.swapchain;

    createLayout(info.frameSetLayout);
    createPipeline(info.mainRenderPass, info.shaderDir);
    createQuadBuffers();
    createInstanceBuffers();
}

void ParticlePass::createLayout(VkDescriptorSetLayout frameSetLayout) {
    // push constant: vertex stage で受け取る
    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(ParticlePC);

    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    lci.setLayoutCount = 1;
    lci.pSetLayouts = &frameSetLayout;
    lci.pushConstantRangeCount = 1;
    lci.pPushConstantRanges = &pcRange;
    if (vkCreatePipelineLayout(ctx_->device(), &lci, nullptr, &layout_) != VK_SUCCESS) {
        throw std::runtime_error("ParticlePass: vkCreatePipelineLayout failed");
    }
}

void ParticlePass::createPipeline(VkRenderPass renderPass, const std::string& shaderDir) {
    VkShaderModule vert =
        shader_util::loadShaderModule(ctx_->device(), shaderDir + "particle_vert.spv");
    VkShaderModule frag =
        shader_util::loadShaderModule(ctx_->device(), shaderDir + "particle_frag.spv");

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    VkVertexInputBindingDescription bindings[2]{};
    bindings[0] = {0, sizeof(QuadVertex), VK_VERTEX_INPUT_RATE_VERTEX};
    bindings[1] = {1, sizeof(particle::ParticleInstance), VK_VERTEX_INPUT_RATE_INSTANCE};

    VkVertexInputAttributeDescription attrs[5]{};
    attrs[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT, 0};
    attrs[1] = {1, 1, VK_FORMAT_R32G32B32_SFLOAT,
                static_cast<uint32_t>(offsetof(particle::ParticleInstance, pos))};
    attrs[2] = {2, 1, VK_FORMAT_R32_SFLOAT,
                static_cast<uint32_t>(offsetof(particle::ParticleInstance, size))};
    attrs[3] = {3, 1, VK_FORMAT_R32G32B32A32_SFLOAT,
                static_cast<uint32_t>(offsetof(particle::ParticleInstance, color))};
    attrs[4] = {4, 1, VK_FORMAT_R32_SFLOAT,
                static_cast<uint32_t>(offsetof(particle::ParticleInstance, age01))};

    VkPipelineVertexInputStateCreateInfo vi{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 2;
    vi.pVertexBindingDescriptions = bindings;
    vi.vertexAttributeDescriptionCount = 5;
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

    VkPipelineColorBlendAttachmentState blendAtt{};
    blendAtt.blendEnable = VK_TRUE;
    blendAtt.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAtt.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
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

    const VkResult r =
        vkCreateGraphicsPipelines(ctx_->device(), VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline_);

    vkDestroyShaderModule(ctx_->device(), vert, nullptr);
    vkDestroyShaderModule(ctx_->device(), frag, nullptr);

    if (r != VK_SUCCESS) {
        throw std::runtime_error("ParticlePass: vkCreateGraphicsPipelines failed");
    }
}

void ParticlePass::createQuadBuffers() {
    {
        const VkDeviceSize sz = sizeof(kQuadVertices);
        resources_->createBuffer(
            sz, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            quadVB_, quadVBMem_);
        void* mapped = nullptr;
        if (vkMapMemory(ctx_->device(), quadVBMem_, 0, sz, 0, &mapped) != VK_SUCCESS) {
            throw std::runtime_error("ParticlePass: quad VB map failed");
        }
        std::memcpy(mapped, kQuadVertices, sz);
        vkUnmapMemory(ctx_->device(), quadVBMem_);
    }

    {
        const VkDeviceSize sz = sizeof(kQuadIndices);
        resources_->createBuffer(
            sz, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            quadIB_, quadIBMem_);
        void* mapped = nullptr;
        if (vkMapMemory(ctx_->device(), quadIBMem_, 0, sz, 0, &mapped) != VK_SUCCESS) {
            throw std::runtime_error("ParticlePass: quad IB map failed");
        }
        std::memcpy(mapped, kQuadIndices, sz);
        vkUnmapMemory(ctx_->device(), quadIBMem_);
    }
}

void ParticlePass::createInstanceBuffers() {
    const VkDeviceSize bufSize = sizeof(particle::ParticleInstance) * kMaxParticlesPerFrame;

    for (uint32_t i = 0; i < FrameSync::MAX_FRAMES_IN_FLIGHT; ++i) {
        resources_->createBuffer(
            bufSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            instanceVBs_[i], instanceVBMems_[i]);
        if (vkMapMemory(ctx_->device(), instanceVBMems_[i], 0, bufSize, 0,
                        &instanceVBMapped_[i]) != VK_SUCCESS) {
            throw std::runtime_error("ParticlePass: instance VB map failed");
        }
    }
}

void ParticlePass::destroyBuffers() {
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;

    for (uint32_t i = 0; i < FrameSync::MAX_FRAMES_IN_FLIGHT; ++i) {
        if (instanceVBMapped_[i]) {
            vkUnmapMemory(ctx_->device(), instanceVBMems_[i]);
            instanceVBMapped_[i] = nullptr;
        }
        if (instanceVBs_[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(ctx_->device(), instanceVBs_[i], nullptr);
            instanceVBs_[i] = VK_NULL_HANDLE;
        }
        if (instanceVBMems_[i] != VK_NULL_HANDLE) {
            vkFreeMemory(ctx_->device(), instanceVBMems_[i], nullptr);
            instanceVBMems_[i] = VK_NULL_HANDLE;
        }
    }

    if (quadVB_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx_->device(), quadVB_, nullptr);
        quadVB_ = VK_NULL_HANDLE;
    }
    if (quadVBMem_ != VK_NULL_HANDLE) {
        vkFreeMemory(ctx_->device(), quadVBMem_, nullptr);
        quadVBMem_ = VK_NULL_HANDLE;
    }
    if (quadIB_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx_->device(), quadIB_, nullptr);
        quadIB_ = VK_NULL_HANDLE;
    }
    if (quadIBMem_ != VK_NULL_HANDLE) {
        vkFreeMemory(ctx_->device(), quadIBMem_, nullptr);
        quadIBMem_ = VK_NULL_HANDLE;
    }
}

void ParticlePass::shutdown() {
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;
    destroyBuffers();
    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(ctx_->device(), pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(ctx_->device(), layout_, nullptr);
        layout_ = VK_NULL_HANDLE;
    }
    ctx_ = nullptr;
    resources_ = nullptr;
    swapchain_ = nullptr;
}

void ParticlePass::execute(const ExecuteInfo& info) {
    if (info.cmd == VK_NULL_HANDLE) return;
    if (info.frameIndex >= FrameSync::MAX_FRAMES_IN_FLIGHT) return;
    if (!info.particles || info.particles->empty()) return;

    particle::ParticleInstance* dst =
        reinterpret_cast<particle::ParticleInstance*>(instanceVBMapped_[info.frameIndex]);
    uint32_t aliveN = 0;
    for (const auto& p : *info.particles) {
        if (!p.alive) continue;
        if (aliveN >= kMaxParticlesPerFrame) break;

        const float t = p.age01();
        const float invT = 1.f - t;

        dst[aliveN].pos = p.pos;
        dst[aliveN].size = p.sizeStart * invT + p.sizeEnd * t;
        dst[aliveN].color = p.colorStart * invT + p.colorEnd * t;
        dst[aliveN].age01 = t;
        aliveN++;
    }

    if (aliveN == 0) return;

    const VkExtent2D extent = swapchain_->extent();
    VkViewport viewport{
        0.f, 0.f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.f, 1.f};
    VkRect2D scissor{{0, 0}, extent};

    vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdSetViewport(info.cmd, 0, 1, &viewport);
    vkCmdSetScissor(info.cmd, 0, 1, &scissor);

    vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_, 0, 1,
                            &info.frameSet, 0, nullptr);

    // ─── push constant: フェード範囲を送る ─────────────────
    ParticlePC pc{};
    if (info.cullingDistance > 0.f) {
        pc.fadeStart = info.cullingDistance * kFadeStartRatio;
        pc.fadeEnd = info.cullingDistance;
    } else {
        // フェード無効: fadeEnd <= fadeStart にして shader 側で 1.0 確定
        pc.fadeStart = 0.f;
        pc.fadeEnd = 0.f;
    }
    vkCmdPushConstants(info.cmd, layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);

    VkBuffer buffers[2] = {quadVB_, instanceVBs_[info.frameIndex]};
    VkDeviceSize offsets[2] = {0, 0};
    vkCmdBindVertexBuffers(info.cmd, 0, 2, buffers, offsets);

    vkCmdBindIndexBuffer(info.cmd, quadIB_, 0, VK_INDEX_TYPE_UINT16);

    vkCmdDrawIndexed(info.cmd, 6, aliveN, 0, 0, 0);
}
