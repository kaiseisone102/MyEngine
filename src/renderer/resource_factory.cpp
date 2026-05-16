// src/renderer/resource_factory.cpp

#include "renderer/resource_factory.h"

#include <stdexcept>

void ResourceFactory::init(const VulkanContext* ctx) {
    ctx_ = ctx;

    // 転送用コマンドプール: TRANSIENT_BIT をつけると
    // 「短命なコマンドバッファを頻繁に割当/解放する」とドライバに伝えられ、
    // メモリ確保戦略を最適化してくれる。
    VkCommandPoolCreateInfo ci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    ci.queueFamilyIndex = ctx_->graphicsFamily();
    if (vkCreateCommandPool(ctx_->device(), &ci, nullptr, &transientPool_) != VK_SUCCESS)
        throw std::runtime_error("ResourceFactory: vkCreateCommandPool failed");
}

void ResourceFactory::shutdown() {
    if (ctx_ && transientPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(ctx_->device(), transientPool_, nullptr);
        transientPool_ = VK_NULL_HANDLE;
    }
    ctx_ = nullptr;
}

uint32_t ResourceFactory::findMemoryType(uint32_t typeFilter,
                                         VkMemoryPropertyFlags properties) const {
    const auto& memProps = ctx_->memoryProperties();
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    throw std::runtime_error("ResourceFactory::findMemoryType: no suitable memory type");
}

void ResourceFactory::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                   VkMemoryPropertyFlags properties, VkBuffer& buffer,
                                   VkDeviceMemory& bufferMemory) const {
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(ctx_->device(), &bi, nullptr, &buffer) != VK_SUCCESS)
        throw std::runtime_error("ResourceFactory::createBuffer: vkCreateBuffer failed");

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(ctx_->device(), buffer, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = findMemoryType(req.memoryTypeBits, properties);
    if (vkAllocateMemory(ctx_->device(), &ai, nullptr, &bufferMemory) != VK_SUCCESS)
        throw std::runtime_error("ResourceFactory::createBuffer: vkAllocateMemory failed");

    vkBindBufferMemory(ctx_->device(), buffer, bufferMemory, 0);
}

void ResourceFactory::createImage(uint32_t width, uint32_t height, VkFormat format,
                                  VkImageTiling tiling, VkImageUsageFlags usage,
                                  VkMemoryPropertyFlags properties, VkImage& image,
                                  VkDeviceMemory& imageMemory) const {
    VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = format;
    ci.extent = {width, height, 1};
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = tiling;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(ctx_->device(), &ci, nullptr, &image) != VK_SUCCESS)
        throw std::runtime_error("ResourceFactory::createImage: vkCreateImage failed");

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(ctx_->device(), image, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = findMemoryType(req.memoryTypeBits, properties);
    if (vkAllocateMemory(ctx_->device(), &ai, nullptr, &imageMemory) != VK_SUCCESS)
        throw std::runtime_error("ResourceFactory::createImage: vkAllocateMemory failed");

    vkBindImageMemory(ctx_->device(), image, imageMemory, 0);
}

VkCommandBuffer ResourceFactory::beginOneTimeCommands() const {
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = transientPool_;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cmd{};
    if (vkAllocateCommandBuffers(ctx_->device(), &ai, &cmd) != VK_SUCCESS)
        throw std::runtime_error("ResourceFactory: beginOneTimeCommands alloc failed");
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
    if (vkQueueSubmit(ctx_->graphicsQueue(), 1, &si, VK_NULL_HANDLE) != VK_SUCCESS)
        throw std::runtime_error("ResourceFactory: endOneTimeCommands submit failed");
    vkQueueWaitIdle(ctx_->graphicsQueue());
    vkFreeCommandBuffers(ctx_->device(), transientPool_, 1, &cmd);
}

void ResourceFactory::copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) const {
    VkCommandBuffer cmd = beginOneTimeCommands();
    VkBufferCopy region{};
    region.size = size;
    vkCmdCopyBuffer(cmd, src, dst, 1, &region);
    endOneTimeCommands(cmd);
}

void ResourceFactory::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width,
                                        uint32_t height) const {
    VkCommandBuffer cmd = beginOneTimeCommands();
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    endOneTimeCommands(cmd);
}

void ResourceFactory::transitionImageLayout(VkImage image, VkImageLayout oldLayout,
                                            VkImageLayout newLayout) const {
    VkCommandBuffer cmd = beginOneTimeCommands();
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkPipelineStageFlags srcStage{}, dstStage{};
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::runtime_error("ResourceFactory::transitionImageLayout: unsupported transition");
    }
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    endOneTimeCommands(cmd);
}