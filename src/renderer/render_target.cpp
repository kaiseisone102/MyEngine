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

    VkImage img = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;
    resources->createImage(desc.width, desc.height, desc.format, VK_IMAGE_TILING_OPTIMAL,
                           desc.usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, img, mem);
    image_ = VkUnique<VkImage>(ctx_->device(), img);
    memory_ = VkUnique<VkDeviceMemory>(ctx_->device(), mem);

    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = image_.get();
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = desc.format;
    vi.subresourceRange = {desc.aspect, 0, 1, 0, 1};
    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(ctx_->device(), &vi, nullptr, &view) != VK_SUCCESS)
        throw std::runtime_error("RenderTarget: vkCreateImageView failed");
    view_ = VkUnique<VkImageView>(ctx_->device(), view);

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
        VkSampler samp = VK_NULL_HANDLE;
        if (vkCreateSampler(ctx_->device(), &si, nullptr, &samp) != VK_SUCCESS)
            throw std::runtime_error("RenderTarget: vkCreateSampler failed");
        sampler_ = VkUnique<VkSampler>(ctx_->device(), samp);
    }
}

void RenderTarget::shutdown() {
    // VkUnique frees each handle (no-op if empty). Reset in reverse dependency
    // order; the auto destructor would do the same. shutdown() is kept so the
    // swapchain-resize re-create flow (shutdown -> init) is unchanged.
    sampler_.reset();
    view_.reset();
    image_.reset();
    memory_.reset();
    ctx_ = nullptr;
}
