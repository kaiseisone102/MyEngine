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
    if (vkCreateDescriptorSetLayout(ctx_->device(), &ci, nullptr, &layout_) != VK_SUCCESS)
        throw std::runtime_error("FrameUniforms: layout failed");

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
    if (vkCreateDescriptorPool(ctx_->device(), &pi, nullptr, &pool_) != VK_SUCCESS)
        throw std::runtime_error("FrameUniforms: pool failed");
}

void FrameUniforms::createUbos(ResourceFactory* resources) {
    const VkDeviceSize size = sizeof(LightingUBO);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        resources->createBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  buffers_[i], memories_[i]);
        vkMapMemory(ctx_->device(), memories_[i], 0, size, 0, &mapped_[i]);

        resources->createBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  buffersReflection_[i], memoriesReflection_[i]);
        vkMapMemory(ctx_->device(), memoriesReflection_[i], 0, size, 0, &mappedReflection_[i]);
    }
}

void FrameUniforms::allocateSets() {
    std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts;
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) layouts[i] = layout_;

    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = pool_;
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
        writeUbo(sets_[i], buffers_[i]);
        writeUbo(setsReflection_[i], buffersReflection_[i]);
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
    std::memcpy(mapped_[idx], &data, sizeof(data));
}

void FrameUniforms::updateReflection(uint32_t frameIndex, const LightingUBO& data) {
    const uint32_t idx = frameIndex % MAX_FRAMES_IN_FLIGHT;
    std::memcpy(mappedReflection_[idx], &data, sizeof(data));
}

VkDescriptorSet FrameUniforms::descriptorSet(uint32_t frameIndex) const {
    return sets_[frameIndex % MAX_FRAMES_IN_FLIGHT];
}

VkDescriptorSet FrameUniforms::descriptorSetReflection(uint32_t frameIndex) const {
    return setsReflection_[frameIndex % MAX_FRAMES_IN_FLIGHT];
}

void FrameUniforms::shutdown() {
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (memories_[i] != VK_NULL_HANDLE) {
            vkUnmapMemory(ctx_->device(), memories_[i]);
            vkFreeMemory(ctx_->device(), memories_[i], nullptr);
            memories_[i] = VK_NULL_HANDLE;
        }
        if (buffers_[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(ctx_->device(), buffers_[i], nullptr);
            buffers_[i] = VK_NULL_HANDLE;
        }
        mapped_[i] = nullptr;

        if (memoriesReflection_[i] != VK_NULL_HANDLE) {
            vkUnmapMemory(ctx_->device(), memoriesReflection_[i]);
            vkFreeMemory(ctx_->device(), memoriesReflection_[i], nullptr);
            memoriesReflection_[i] = VK_NULL_HANDLE;
        }
        if (buffersReflection_[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(ctx_->device(), buffersReflection_[i], nullptr);
            buffersReflection_[i] = VK_NULL_HANDLE;
        }
        mappedReflection_[i] = nullptr;
    }
    if (pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(ctx_->device(), pool_, nullptr);
        pool_ = VK_NULL_HANDLE;
    }
    if (layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(ctx_->device(), layout_, nullptr);
        layout_ = VK_NULL_HANDLE;
    }
    ctx_ = nullptr;
}
