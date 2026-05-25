// Force rebuild marker 678365246
// include/MyEngine/renderer/resource_factory.h
#pragma once
// =============================================================================
// ResourceFactory - Phase 1B-3: VMA-based helpers added alongside legacy API.
// =============================================================================
// New: createBufferVMA uses the VMA allocator with BDA support.
// Legacy createBuffer remains for backward compatibility (buffer only).
// (Legacy createImage / createImageVMA removed; all images now use VmaImage.)
//
// Migration path (per call site):
//   Old: VkBuffer buf; VkDeviceMemory mem;
//        rf.createBuffer(size, usage, props, buf, mem);
//        // ... use ...
//        vkDestroyBuffer(dev, buf, nullptr);
//        vkFreeMemory(dev, mem, nullptr);
//
//   New: VkBuffer buf; VmaAllocation alloc;
//        rf.createBufferVMA(size, usage, VMA_MEMORY_USAGE_AUTO, 0, buf, alloc);
//        // ... use ...
//        vmaDestroyBuffer(rf.allocator(), buf, alloc);
// =============================================================================
#include <vulkan/vulkan.h>

#include "vulkan_context.h"

// VMA forward declarations (avoid including full VMA header in this .h)
VK_DEFINE_HANDLE(VmaAllocation)
enum VmaMemoryUsage : int;
typedef VkFlags VmaAllocationCreateFlags;

class ResourceFactory {
   public:
    void init(const VulkanContext* ctx);
    void shutdown();

    // ┌─ Legacy buffer creation (raw VkDeviceMemory). DO NOT use for new code.
    [[deprecated("Use createBufferVMA instead. Will be removed after Phase 1B migration.")]]
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& bufferMemory) const;


    // ┌─ Memory type lookup (legacy path only).
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    // ─── VMA-based creation (Phase 1B-3+) ─────────────────────────────────
    // Use VMA for new buffer/image creation. BDA is automatically supported
    // when usage includes VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT.

    /// Create buffer via VMA. memoryUsage = VMA_MEMORY_USAGE_AUTO is recommended.
    /// flags can be VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
    /// VMA_ALLOCATION_CREATE_MAPPED_BIT, etc.
    void createBufferVMA(VkDeviceSize size, VkBufferUsageFlags usage,
                         VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags flags,
                         VkBuffer& buffer, VmaAllocation& allocation) const;


    // ─── Transfer helpers (unchanged) ─────────────────────────────────────
    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) const;
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) const;
    void transitionImageLayout(VkImage image, VkImageLayout oldLayout,
                               VkImageLayout newLayout) const;

   private:
    const VulkanContext* ctx_ = nullptr;
    VkCommandPool transientPool_ = VK_NULL_HANDLE;

    VkCommandBuffer beginOneTimeCommands() const;
    void endOneTimeCommands(VkCommandBuffer cmd) const;
};
