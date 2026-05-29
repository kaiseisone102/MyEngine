// src/renderer/vma_image.cpp
#include "renderer/vma_image.h"

#include <stdexcept>

#include <vk_mem_alloc.h>

#include "renderer/vulkan_context.h"

VmaImage VmaImage::create(VulkanContext* ctx, const VkImageCreateInfo& ci, bool dedicated) {
    if (!ctx) throw std::runtime_error("VmaImage::create: null ctx");

    VmaAllocationCreateInfo ai{};
    ai.usage = VMA_MEMORY_USAGE_AUTO;
    // N: render targets / textures are HIGH priority (kept resident under
    // VRAM pressure -- evicting a frame-critical attachment causes a stall
    // on next sample). Honored now that the allocator carries
    // VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT (was ignored before N).
    ai.priority = 0.75f;
    if (dedicated) {
        ai.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    VmaImage out;
    out.allocator_ = ctx->allocator();
    out.width_ = ci.extent.width;
    out.height_ = ci.extent.height;
    out.format_ = ci.format;

    if (vmaCreateImage(out.allocator_, &ci, &ai, &out.image_, &out.allocation_, nullptr) !=
        VK_SUCCESS) {
        throw std::runtime_error("VmaImage::create: vmaCreateImage failed");
    }
    return out;
}

VmaImage VmaImage::createAttachment(VulkanContext* ctx, uint32_t width, uint32_t height,
                                    VkFormat format, VkImageUsageFlags usage) {
    if (width == 0 || height == 0) throw std::runtime_error("VmaImage::createAttachment: zero extent");
    if (format == VK_FORMAT_UNDEFINED) throw std::runtime_error("VmaImage::createAttachment: no format");

    VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = format;
    ci.extent = {width, height, 1};
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    return create(ctx, ci, /*dedicated=*/true);
}

void VmaImage::reset() noexcept {
    if (image_ != VK_NULL_HANDLE && allocator_ != VK_NULL_HANDLE) {
        vmaDestroyImage(allocator_, image_, allocation_);
    }
    allocator_ = VK_NULL_HANDLE;
    image_ = VK_NULL_HANDLE;
    allocation_ = VK_NULL_HANDLE;
    width_ = 0;
    height_ = 0;
    format_ = VK_FORMAT_UNDEFINED;
}