// =============================================================================
// frame_uniforms.cpp — Phase 1C: 通常 + 反射用 UBO/set 管理
// =============================================================================
#include "renderer/frame_uniforms.h"

#include <cstring>
#include <stdexcept>

#include "renderer/resource_factory.h"
#include "renderer/vulkan_context.h"

void FrameUniforms::init(VulkanContext* ctx, ResourceFactory* resources) {
    ctx_ = ctx;
    createLayoutAndPool();
    createUbos(resources);
    allocateSets();
}

void FrameUniforms::createLayoutAndPool() {
    VkDescriptorSetLayoutBinding bindings[2]{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    ci.bindingCount = 2;
    ci.pBindings = bindings;
    VkDescriptorSetLayout lay = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(ctx_->device(), &ci, nullptr, &lay) != VK_SUCCESS)
        throw std::runtime_error("FrameUniforms: layout failed");
    layout_ = VkUnique<VkDescriptorSetLayout>(ctx_->device(), lay);

    // 通常 N + 反射 N の 2 倍
    VkDescriptorPoolSize sizes[2]{};
    sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT * 2;
    sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT * 2;

    VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pi.maxSets = MAX_FRAMES_IN_FLIGHT * 2;
    pi.poolSizeCount = 2;
    pi.pPoolSizes = sizes;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(ctx_->device(), &pi, nullptr, &pool) != VK_SUCCESS)
        throw std::runtime_error("FrameUniforms: pool failed");
    pool_ = VkUnique<VkDescriptorPool>(ctx_->device(), pool);
}

void FrameUniforms::createUbos(ResourceFactory* resources) {
    const VkDeviceSize size = sizeof(LightingUBO);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        buffers_[i] =
            VmaBuffer::createMappedHostVisible(ctx_, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        buffersReflection_[i] =
            VmaBuffer::createMappedHostVisible(ctx_, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    }
}

void FrameUniforms::allocateSets() {
    std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts;
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) layouts[i] = layout_.get();

    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = pool_.get();
    ai.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    ai.pSetLayouts = layouts.data();
    if (vkAllocateDescriptorSets(ctx_->device(), &ai, sets_.data()) != VK_SUCCESS)
        throw std::runtime_error("FrameUniforms: alloc sets failed");
    if (vkAllocateDescriptorSets(ctx_->device(), &ai, setsReflection_.data()) != VK_SUCCESS)
        throw std::runtime_error("FrameUniforms: alloc reflection sets failed");

    // UBO binding (両方の set 共通の手順)
    auto writeUbo = [&](VkDescriptorSet set, VkBuffer buffer) {
        VkDescriptorBufferInfo bi{};
        bi.buffer = buffer;
        bi.offset = 0;
        bi.range = sizeof(LightingUBO);
        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet = set;
        w.dstBinding = 0;
        w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.descriptorCount = 1;
        w.pBufferInfo = &bi;
        vkUpdateDescriptorSets(ctx_->device(), 1, &w, 0, nullptr);
    };
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        writeUbo(sets_[i], buffers_[i].buffer());
        writeUbo(setsReflection_[i], buffersReflection_[i].buffer());
    }
}

void FrameUniforms::bindShadowMap(VkImageView view, VkSampler sampler) {
    shadowView_ = view;
    shadowSampler_ = sampler;
}

void FrameUniforms::rebuildDescriptorSets() {
    if (shadowView_ == VK_NULL_HANDLE || shadowSampler_ == VK_NULL_HANDLE) return;

    auto writeSampler = [&](VkDescriptorSet set) {
        VkDescriptorImageInfo ii{};
        ii.sampler = shadowSampler_;
        ii.imageView = shadowView_;
        ii.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet = set;
        w.dstBinding = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.descriptorCount = 1;
        w.pImageInfo = &ii;
        vkUpdateDescriptorSets(ctx_->device(), 1, &w, 0, nullptr);
    };
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        writeSampler(sets_[i]);
        writeSampler(setsReflection_[i]);
    }
}

void FrameUniforms::update(uint32_t frameIndex, const LightingUBO& data) {
    const uint32_t idx = frameIndex % MAX_FRAMES_IN_FLIGHT;
    std::memcpy(buffers_[idx].mapped(), &data, sizeof(data));
}

void FrameUniforms::updateReflection(uint32_t frameIndex, const LightingUBO& data) {
    const uint32_t idx = frameIndex % MAX_FRAMES_IN_FLIGHT;
    std::memcpy(buffersReflection_[idx].mapped(), &data, sizeof(data));
}

VkDescriptorSet FrameUniforms::descriptorSet(uint32_t frameIndex) const {
    return sets_[frameIndex % MAX_FRAMES_IN_FLIGHT];
}

VkDescriptorSet FrameUniforms::descriptorSetReflection(uint32_t frameIndex) const {
    return setsReflection_[frameIndex % MAX_FRAMES_IN_FLIGHT];
}

void FrameUniforms::shutdown() {
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;
    // VmaBuffer frees buffer + allocation (and unmaps) in reset (no-op if empty).
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        buffers_[i].reset();
        buffersReflection_[i].reset();
    }
    // Descriptor sets are freed implicitly when the pool is destroyed.
    pool_.reset();
    layout_.reset();
    ctx_ = nullptr;
}
