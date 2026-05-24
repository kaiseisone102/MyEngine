// =============================================================================
// water_pass.cpp — 2 pipeline + reflection set (sampler + reflectVP UBO)
// =============================================================================
#include "renderer/water_pass.h"

#include <cstring>
#include <stdexcept>

#include "core/water.h"
#include "renderer/resource_factory.h"
#include "renderer/vulkan_context.h"
#include "renderer/water_mesh.h"

namespace {
struct ReflectVpUboData {
    glm::mat4 reflectVP;
};
}  // namespace

void WaterPass::init(const InitInfo& info) {
    if (!info.ctx || !info.resources) throw std::runtime_error("WaterPass::init: invalid info");
    ctx_ = info.ctx;

    createReflectionLayoutAndPool();
    createReflectionUbo(info.resources);
    allocateReflectionSets();

    WaterPipeline::InitInfo pi{};
    pi.ctx = info.ctx;
    pi.renderPass = info.mainRenderPass;
    pi.frameSetLayout = info.frameSetLayout;
    pi.reflectionSetLayout = reflectionLayout_.get();
    pi.shaderDir = info.shaderDir;
    pipeline_.init(pi);
}

void WaterPass::createReflectionLayoutAndPool() {
    VkDescriptorSetLayoutBinding bindings[2]{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    ci.bindingCount = 2;
    ci.pBindings = bindings;
    VkDescriptorSetLayout lay = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(ctx_->device(), &ci, nullptr, &lay) != VK_SUCCESS)
        throw std::runtime_error("WaterPass: reflection layout failed");
    reflectionLayout_ = VkUnique<VkDescriptorSetLayout>(ctx_->device(), lay);

    VkDescriptorPoolSize sizes[2]{};
    sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;
    sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pi.maxSets = MAX_FRAMES_IN_FLIGHT;
    pi.poolSizeCount = 2;
    pi.pPoolSizes = sizes;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(ctx_->device(), &pi, nullptr, &pool) != VK_SUCCESS)
        throw std::runtime_error("WaterPass: reflection pool failed");
    reflectionPool_ = VkUnique<VkDescriptorPool>(ctx_->device(), pool);
}

void WaterPass::createReflectionUbo(ResourceFactory* resources) {
    const VkDeviceSize size = sizeof(ReflectVpUboData);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        reflectVpBuffers_[i] =
            VmaBuffer::createMappedHostVisible(ctx_, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        ReflectVpUboData init{};
        init.reflectVP = glm::mat4(1.0f);
        std::memcpy(reflectVpBuffers_[i].mapped(), &init, sizeof(init));
    }
}

void WaterPass::allocateReflectionSets() {
    std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts;
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) layouts[i] = reflectionLayout_.get();

    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = reflectionPool_.get();
    ai.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    ai.pSetLayouts = layouts.data();
    if (vkAllocateDescriptorSets(ctx_->device(), &ai, reflectionSets_.data()) != VK_SUCCESS)
        throw std::runtime_error("WaterPass: descriptor set alloc failed");

    // UBO 部分だけ最初に書き込み (sampler は bindReflectionTexture で後で)
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkDescriptorBufferInfo bi{};
        bi.buffer = reflectVpBuffers_[i].buffer();
        bi.offset = 0;
        bi.range = sizeof(ReflectVpUboData);

        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet = reflectionSets_[i];
        w.dstBinding = 1;
        w.dstArrayElement = 0;
        w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.descriptorCount = 1;
        w.pBufferInfo = &bi;
        vkUpdateDescriptorSets(ctx_->device(), 1, &w, 0, nullptr);
    }
}

void WaterPass::bindReflectionTexture(VkImageView view, VkSampler sampler) {
    currentReflectView_ = view;
    currentReflectSampler_ = sampler;
    writeSamplerToSets();
}

void WaterPass::writeSamplerToSets() {
    if (currentReflectView_ == VK_NULL_HANDLE || currentReflectSampler_ == VK_NULL_HANDLE) return;
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkDescriptorImageInfo ii{};
        ii.sampler = currentReflectSampler_;
        ii.imageView = currentReflectView_;
        ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet = reflectionSets_[i];
        w.dstBinding = 0;
        w.dstArrayElement = 0;
        w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.descriptorCount = 1;
        w.pImageInfo = &ii;
        vkUpdateDescriptorSets(ctx_->device(), 1, &w, 0, nullptr);
    }
}

void WaterPass::writeReflectVp(uint32_t frameIndex, const glm::mat4& reflectVP) {
    const uint32_t idx = frameIndex % MAX_FRAMES_IN_FLIGHT;
    ReflectVpUboData data{};
    data.reflectVP = reflectVP;
    std::memcpy(reflectVpBuffers_[idx].mapped(), &data, sizeof(data));
}

void WaterPass::execute(const ExecuteInfo& info) {
    if (!info.cmd || !info.waterDrawList || info.waterDrawList->empty()) return;

    const bool useRefl = info.useReflection && currentReflectView_ != VK_NULL_HANDLE;
    const VkPipeline pipeline =
        useRefl ? pipeline_.pipelineWithReflection() : pipeline_.pipelineFakeOnly();
    const VkPipelineLayout layout =
        useRefl ? pipeline_.layoutWithReflection() : pipeline_.layoutFakeOnly();
    if (pipeline == VK_NULL_HANDLE || layout == VK_NULL_HANDLE) return;

    vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1,
                            &info.frameSet, 0, nullptr);

    if (useRefl) {
        writeReflectVp(info.frameIndex, info.reflectVP);
        const uint32_t idx = info.frameIndex % MAX_FRAMES_IN_FLIGHT;
        vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1,
                                &reflectionSets_[idx], 0, nullptr);
    }

    for (const WaterDrawItem& item : *info.waterDrawList) {
        if (!item.mesh) continue;

        WaterPipeline::PushConstants pc{};
        pc.model = glm::mat4(1.0f);  // WaterMesh bakes world coords into vertices
        pc.time = info.time;
        pc.waveAmp = item.drawParams.waveAmp;
        pc.waveSpeed = item.drawParams.waveSpeed;
        pc.waveWavelength = item.drawParams.waveWavelength;
        pc.shallowColor = glm::vec4(item.drawParams.shallowColor, item.drawParams.baseAlpha);
        pc.deepColor = glm::vec4(item.drawParams.deepColor, item.drawParams.baseAlpha);

        vkCmdPushConstants(info.cmd, layout,
                            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                            sizeof(pc), &pc);

        item.mesh->bind(info.cmd);
        vkCmdDrawIndexed(info.cmd, item.mesh->indexCount(), 1, 0, 0, 0);
    }
}

void WaterPass::shutdown() {
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;
    pipeline_.shutdown();

    // VmaBuffer frees buffer + allocation (and unmaps) in reset (no-op if empty).
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        reflectVpBuffers_[i].reset();
    }
    // Descriptor sets are freed implicitly when the pool is destroyed.
    reflectionPool_.reset();
    reflectionLayout_.reset();
    ctx_ = nullptr;
}
