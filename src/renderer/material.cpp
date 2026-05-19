// src/renderer/material.cpp
#include "renderer/material.h"

#include <stdexcept>

#include "renderer/texture.h"
#include "renderer/vulkan_context.h"

void Material::init(const VulkanContext* ctx, VkDescriptorPool pool, VkDescriptorSetLayout layout,
                   const Texture* texture) {
    if (!ctx || pool == VK_NULL_HANDLE || layout == VK_NULL_HANDLE || !texture) {
        throw std::runtime_error("Material::init: invalid argument");
    }

    // alloc 1 set
    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = pool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &layout;
    if (vkAllocateDescriptorSets(ctx->device(), &ai, &set_) != VK_SUCCESS) {
        throw std::runtime_error("Material: vkAllocateDescriptorSets failed");
    }

    // write texture (set=1, binding=0)
    VkDescriptorImageInfo ti{};
    ti.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ti.imageView = texture->view();
    ti.sampler = texture->sampler();

    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = set_;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &ti;

    vkUpdateDescriptorSets(ctx->device(), 1, &write, 0, nullptr);
}
