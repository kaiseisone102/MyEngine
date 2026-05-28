// \MyEngine\src\renderer\resource_factory.cpp
// =============================================================================
// resource_factory.cpp
// =============================================================================
// 実装内容:
//   VkBuffer / VkImage の生成・メモリ確保・転送・レイアウト遷移のヘルパー。
//   ワンタイムコマンド用に専用の TRANSIENT コマンドプールを持つ。
//
// 注意:
//   - ctx_ は所有しない。init〜shutdown の間、 呼び出し側が寿命を保証する。
//   - createBuffer/createImage は AllocateMemory 失敗時に先に作った
//     buffer/image を破棄してから throw する (リーク防止)。
//   - transitionImageLayout は color aspect 限定。 サポート済み遷移:
//       UNDEFINED          → TRANSFER_DST_OPTIMAL    (テクスチャアップロード前)
//       TRANSFER_DST       → SHADER_READ_ONLY        (テクスチャアップロード後)
//       UNDEFINED          → SHADER_READ_ONLY        (空テクスチャの初期化)
//       未サポート遷移は generic fallback で実行 (パフォーマンス悪いが安全)。
// =============================================================================
#include "renderer/resource_factory.h"

#include <vk_mem_alloc.h>

#include <stdexcept>

#include "renderer/barrier.h"
#include "renderer/vulkan_context.h"

// =============================================================================
// init / shutdown
// =============================================================================

void ResourceFactory::init(const VulkanContext* ctx) {
    ctx_ = ctx;

    // 転送用コマンドプール: TRANSIENT_BIT をつけると
    // 「短命なコマンドバッファを頻繁に割当/解放する」とドライバに伝えられ、
    // メモリ確保戦略を最適化してくれる。
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
// findMemoryType
// =============================================================================

uint32_t ResourceFactory::findMemoryType(uint32_t typeFilter,
                                         VkMemoryPropertyFlags properties) const {
    const auto& memProps = ctx_->memoryProperties();
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("ResourceFactory::findMemoryType: no suitable memory type");
}

// =============================================================================
// createBuffer
// =============================================================================

void ResourceFactory::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                   VkMemoryPropertyFlags properties, VkBuffer& buffer,
                                   VkDeviceMemory& bufferMemory) const {
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(ctx_->device(), &bi, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("ResourceFactory::createBuffer: vkCreateBuffer failed");
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(ctx_->device(), buffer, &req);

    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = findMemoryType(req.memoryTypeBits, properties);
    if (vkAllocateMemory(ctx_->device(), &ai, nullptr, &bufferMemory) != VK_SUCCESS) {
        // リーク防止: 先に作った buffer を破棄してから throw
        vkDestroyBuffer(ctx_->device(), buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        throw std::runtime_error("ResourceFactory::createBuffer: vkAllocateMemory failed");
    }
    vkBindBufferMemory(ctx_->device(), buffer, bufferMemory, 0);
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
// transitionImageLayout (color aspect 限定)
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
        b.srcStage  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
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
        b.srcStage  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
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

// =============================================================================
// VMA-based helpers (Phase 1B-3+)
// =============================================================================
// These use the VMA allocator owned by VulkanContext, which was initialized
// with VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT.

void ResourceFactory::createBufferVMA(VkDeviceSize size, VkBufferUsageFlags usage,
                                       VmaMemoryUsage memoryUsage,
                                       VmaAllocationCreateFlags flags,
                                       VkBuffer& buffer, VmaAllocation& allocation) const {
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo ai{};
    ai.usage = memoryUsage;
    ai.flags = flags;

    if (vmaCreateBuffer(ctx_->allocator(), &bi, &ai, &buffer, &allocation, nullptr) !=
        VK_SUCCESS) {
        throw std::runtime_error("ResourceFactory::createBufferVMA: vmaCreateBuffer failed");
    }
}

