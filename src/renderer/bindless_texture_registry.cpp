// src/renderer/bindless_texture_registry.cpp
#include "renderer/bindless_texture_registry.h"

#include <iostream>
#include <stdexcept>

#include "renderer/vulkan_context.h"

void BindlessTextureRegistry::init(VulkanContext* ctx) {
    if (!ctx) throw std::runtime_error("BindlessTextureRegistry::init: ctx is null");
    ctx_ = ctx;

    // ─── Descriptor Set Layout (bindless) ─────────────────────────────────
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = BINDING_SLOT;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = MAX_TEXTURES;  // upper bound; not all need to be populated
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
    binding.pImmutableSamplers = nullptr;

    // Per-binding flags. PARTIALLY_BOUND lets us leave slots empty.
    // UPDATE_AFTER_BIND_BIT lets us write descriptors after vkCmdBindDescriptorSets.
    // UPDATE_UNUSED_WHILE_PENDING_BIT lets us overwrite a slot that's not used
    // by an in-flight draw.
    const VkDescriptorBindingFlags bindingFlags =
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
    bindingFlagsInfo.bindingCount = 1;
    bindingFlagsInfo.pBindingFlags = &bindingFlags;

    VkDescriptorSetLayoutCreateInfo layoutCi{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    // UPDATE_AFTER_BIND_POOL_BIT is required since the binding uses UPDATE_AFTER_BIND.
    layoutCi.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layoutCi.bindingCount = 1;
    layoutCi.pBindings = &binding;
    layoutCi.pNext = &bindingFlagsInfo;

    if (vkCreateDescriptorSetLayout(ctx_->device(), &layoutCi, nullptr, &layout_) != VK_SUCCESS) {
        throw std::runtime_error(
            "BindlessTextureRegistry: vkCreateDescriptorSetLayout failed");
    }

    // ─── Descriptor Pool ──────────────────────────────────────────────────
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = MAX_TEXTURES;

    VkDescriptorPoolCreateInfo poolCi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolCi.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    poolCi.maxSets = 1;
    poolCi.poolSizeCount = 1;
    poolCi.pPoolSizes = &poolSize;

    if (vkCreateDescriptorPool(ctx_->device(), &poolCi, nullptr, &pool_) != VK_SUCCESS) {
        throw std::runtime_error("BindlessTextureRegistry: vkCreateDescriptorPool failed");
    }

    // ─── Allocate the single bindless descriptor set ──────────────────────
    // Variable count is optional but supported; we use the full MAX_TEXTURES upper bound.
    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = pool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout_;

    if (vkAllocateDescriptorSets(ctx_->device(), &allocInfo, &set_) != VK_SUCCESS) {
        throw std::runtime_error("BindlessTextureRegistry: vkAllocateDescriptorSets failed");
    }

    std::cout << "[BindlessTextureRegistry] init: capacity=" << MAX_TEXTURES
              << " textures, descriptor indexing enabled\n";
}

uint32_t BindlessTextureRegistry::registerTexture(VkImageView view, VkSampler sampler) {
    if (nextIndex_ >= MAX_TEXTURES) {
        std::cerr << "[BindlessTextureRegistry] WARNING: out of texture slots (max="
                  << MAX_TEXTURES << ")\n";
        return UINT32_MAX;
    }
    if (view == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE) {
        std::cerr << "[BindlessTextureRegistry] WARNING: cannot register null view/sampler\n";
        return UINT32_MAX;
    }
    const uint32_t idx = nextIndex_++;
    writeDescriptor(idx, view, sampler);
    return idx;
}

void BindlessTextureRegistry::updateTexture(uint32_t index, VkImageView view, VkSampler sampler) {
    if (index >= MAX_TEXTURES) {
        std::cerr << "[BindlessTextureRegistry] WARNING: index out of range " << index << "\n";
        return;
    }
    writeDescriptor(index, view, sampler);
}

void BindlessTextureRegistry::writeDescriptor(uint32_t index, VkImageView view,
                                              VkSampler sampler) {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = view;
    imageInfo.sampler = sampler;

    VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    w.dstSet = set_;
    w.dstBinding = BINDING_SLOT;
    w.dstArrayElement = index;          // slot within the bindless array
    w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(ctx_->device(), 1, &w, 0, nullptr);
}

void BindlessTextureRegistry::shutdown() {
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;

    if (pool_ != VK_NULL_HANDLE) {
        // Sets are freed automatically with the pool.
        vkDestroyDescriptorPool(ctx_->device(), pool_, nullptr);
        pool_ = VK_NULL_HANDLE;
        set_ = VK_NULL_HANDLE;
    }
    if (layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(ctx_->device(), layout_, nullptr);
        layout_ = VK_NULL_HANDLE;
    }
    nextIndex_ = 0;
    ctx_ = nullptr;
}
