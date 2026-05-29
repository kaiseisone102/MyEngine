// include/MyEngine/renderer/resource_factory.h
#pragma once
// =============================================================================
// ResourceFactory - transfer + layout-transition helpers.
// =============================================================================
// Owns a TRANSIENT command pool for one-time submits used by buffer copies and
// image layout transitions. Memory allocation lives in VmaBuffer / VmaImage;
// this class no longer creates buffers or looks up memory types directly.
// =============================================================================
#include <vulkan/vulkan.h>

#include "vulkan_context.h"

class ResourceFactory {
   public:
    void init(const VulkanContext* ctx);
    void shutdown();

    // Transfer / layout helpers (one-time submit via the transient pool).
    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) const;
    // Copy a sub-range into dst at a byte offset (for packing into a megabuffer).
    void copyBufferRegion(VkBuffer src, VkBuffer dst, VkDeviceSize srcOffset,
                          VkDeviceSize dstOffset, VkDeviceSize size) const;
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) const;
    void transitionImageLayout(VkImage image, VkImageLayout oldLayout,
                               VkImageLayout newLayout) const;

   private:
    const VulkanContext* ctx_ = nullptr;
    VkCommandPool transientPool_ = VK_NULL_HANDLE;

    VkCommandBuffer beginOneTimeCommands() const;
    void endOneTimeCommands(VkCommandBuffer cmd) const;
};
