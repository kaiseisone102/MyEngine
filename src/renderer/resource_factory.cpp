// src/renderer/resource_factory.cpp
// =============================================================================
// resource_factory.cpp
// =============================================================================
// Owns a TRANSIENT command pool used by the one-time submit helpers below
// (buffer copies + image layout transitions). All memory allocation lives in
// VmaBuffer / VmaImage; this file deliberately has no vkCreateBuffer /
// vkAllocateMemory / vkBindBufferMemory call.
//
// transitionImageLayout supports a small set of color-aspect transitions used
// by texture upload; anything else falls through to a generic ALL_COMMANDS
// barrier (slow but safe).
// =============================================================================
#include "renderer/resource_factory.h"

#include <stdexcept>

#include "renderer/barrier.h"
#include "renderer/vulkan_context.h"

// =============================================================================
// init / shutdown
// =============================================================================

void ResourceFactory::init(const VulkanContext* ctx) {
    ctx_ = ctx;

    // TRANSIENT pool: short-lived, frequently allocated/freed command buffers.
    VkCommandPoolCreateInfo ci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    ci.queueFamilyIndex = ctx_->graphicsFamily();
    if (vkCreateCommandPool(ctx_->device(), &ci, nullptr, &transientPool_) != VK_SUCCESS) {
        throw std::runtime_error("ResourceFactory: vkCreateCommandPool failed");
    }
}

void ResourceFactory::shutdown() {
    if (ctx_ && transientPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(ctx_->device(), transientPool_, nullptr);
        transientPool_ = VK_NULL_HANDLE;
    }
    ctx_ = nullptr;
}

// =============================================================================
// One-time commands
// =============================================================================

VkCommandBuffer ResourceFactory::beginOneTimeCommands() const {
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = transientPool_;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cmd{};
    if (vkAllocateCommandBuffers(ctx_->device(), &ai, &cmd) != VK_SUCCESS) {
        throw std::runtime_error("ResourceFactory: beginOneTimeCommands alloc failed");
    }
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    return cmd;
}

void ResourceFactory::endOneTimeCommands(VkCommandBuffer cmd) const {
    vkEndCommandBuffer(cmd);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    if (vkQueueSubmit(ctx_->graphicsQueue(), 1, &si, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("ResourceFactory: endOneTimeCommands submit failed");
    }
    vkQueueWaitIdle(ctx_->graphicsQueue());
    vkFreeCommandBuffers(ctx_->device(), transientPool_, 1, &cmd);
}

// =============================================================================
// copyBuffer
// =============================================================================

void ResourceFactory::copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) const {
    VkCommandBuffer cmd = beginOneTimeCommands();
    VkBufferCopy region{};
    region.size = size;
    vkCmdCopyBuffer(cmd, src, dst, 1, &region);
    endOneTimeCommands(cmd);
}

void ResourceFactory::copyBufferRegion(VkBuffer src, VkBuffer dst, VkDeviceSize srcOffset,
                                       VkDeviceSize dstOffset, VkDeviceSize size) const {
    VkCommandBuffer cmd = beginOneTimeCommands();
    VkBufferCopy region{};
    region.srcOffset = srcOffset;
    region.dstOffset = dstOffset;
    region.size = size;
    vkCmdCopyBuffer(cmd, src, dst, 1, &region);
    endOneTimeCommands(cmd);
}

// =============================================================================
// copyBufferToImage
// =============================================================================

void ResourceFactory::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width,
                                        uint32_t height) const {
    VkCommandBuffer cmd = beginOneTimeCommands();
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    endOneTimeCommands(cmd);
}

// =============================================================================
// transitionImageLayout (color aspect only)
// =============================================================================

void ResourceFactory::transitionImageLayout(VkImage image, VkImageLayout oldLayout,
                                            VkImageLayout newLayout) const {
    VkCommandBuffer cmd = beginOneTimeCommands();

    barrier::ImageBarrier b{
        .image = image,
        .range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        .oldLayout = oldLayout,
        .newLayout = newLayout,
    };

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        b.srcStage  = VK_PIPELINE_STAGE_2_NONE;
        b.srcAccess = 0;
        b.dstStage  = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        b.dstAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        b.srcStage  = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        b.srcAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        b.dstStage  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        b.dstAccess = VK_ACCESS_2_SHADER_READ_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        // Used to initialise empty textures.
        b.srcStage  = VK_PIPELINE_STAGE_2_NONE;
        b.srcAccess = 0;
        b.dstStage  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        b.dstAccess = VK_ACCESS_2_SHADER_READ_BIT;
    } else {
        // Generic fallback for unhandled transitions (slow but safe).
        b.srcStage  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        b.srcAccess = VK_ACCESS_2_MEMORY_WRITE_BIT;
        b.dstStage  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        b.dstAccess = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    }

    barrier::recordImage(*ctx_, cmd, b);
    endOneTimeCommands(cmd);
}
