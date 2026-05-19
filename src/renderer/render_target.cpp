// src/renderer/render_target.cpp
#include "renderer/render_target.h"

#include <stdexcept>

#include "renderer/resource_factory.h"
#include "renderer/vulkan_context.h"

void RenderTarget::init(VulkanContext* ctx, ResourceFactory* resources, const Desc& desc) {
    ctx_ = ctx;
    desc_ = desc;

    if (desc.width == 0 || desc.height == 0) throw std::runtime_error("RenderTarget: zero extent");
    if (desc.format == VK_FORMAT_UNDEFINED) throw std::runtime_error("RenderTarget: no format");

    resources->createImage(desc.width, desc.height, desc.format, VK_IMAGE_TILING_OPTIMAL,
                           desc.usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image_, memory_);

    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = image_;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = desc.format;
    vi.subresourceRange = {desc.aspect, 0, 1, 0, 1};
    if (vkCreateImageView(ctx_->device(), &vi, nullptr, &view_) != VK_SUCCESS)
        throw std::runtime_error("RenderTarget: vkCreateImageView failed");

    if (desc.createSampler) {
        VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        si.magFilter = desc.samplerFilter;
        si.minFilter = desc.samplerFilter;
        si.addressModeU = desc.samplerAddressMode;
        si.addressModeV = desc.samplerAddressMode;
        si.addressModeW = desc.samplerAddressMode;
        si.borderColor = desc.samplerBorderColor;
        si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        si.minLod = 0.f;
        si.maxLod = 1.f;
        if (vkCreateSampler(ctx_->device(), &si, nullptr, &sampler_) != VK_SUCCESS)
            throw std::runtime_error("RenderTarget: vkCreateSampler failed");
    }
}

void RenderTarget::shutdown() {
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;
    if (sampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(ctx_->device(), sampler_, nullptr);
        sampler_ = VK_NULL_HANDLE;
    }
    if (view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(ctx_->device(), view_, nullptr);
        view_ = VK_NULL_HANDLE;
    }
    if (image_ != VK_NULL_HANDLE) {
        vkDestroyImage(ctx_->device(), image_, nullptr);
        image_ = VK_NULL_HANDLE;
    }
    if (memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(ctx_->device(), memory_, nullptr);
        memory_ = VK_NULL_HANDLE;
    }
    ctx_ = nullptr;
}
