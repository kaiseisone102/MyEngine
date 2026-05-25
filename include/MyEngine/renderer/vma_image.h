#pragma once
// =============================================================================
// vma_image.h - Move-only RAII wrapper for a VMA image (VkImage + VmaAllocation)
// =============================================================================
// The image analogue of VmaBuffer. VkUnique<VkImage> can own the handle but not
// the VMA memory; VmaImage owns the (VkImage + VmaAllocation) pair and frees it
// with vmaDestroyImage(). View/Sampler stay with the owner (RenderTarget etc.)
// as VkUnique, exactly mirroring how VmaBuffer owns only buffer+allocation.
//
// Render targets and depth attachments are created with a dedicated allocation
// (VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT): they are large and recreated on
// swapchain resize, the case VMA recommends dedicated memory for.
//
// As with vma_buffer.h, the full VMA header is only included in the .cpp; here
// the VMA handles are forward-declared.
// =============================================================================
#include <vulkan/vulkan.h>
#include <cstdint>

// VMA forward declarations (same style as vma_buffer.h)
VK_DEFINE_HANDLE(VmaAllocation)
VK_DEFINE_HANDLE(VmaAllocator)

class VulkanContext;

class VmaImage {
   public:
    VmaImage() noexcept = default;
    ~VmaImage() { reset(); }

    VmaImage(const VmaImage&) = delete;
    VmaImage& operator=(const VmaImage&) = delete;

    VmaImage(VmaImage&& o) noexcept { moveFrom(o); }
    VmaImage& operator=(VmaImage&& o) noexcept {
        if (this != &o) {
            reset();
            moveFrom(o);
        }
        return *this;
    }

    // Create a 2D, single-mip, single-layer, OPTIMAL-tiling color/depth
    // attachment image. usage is taken as-is. A dedicated allocation is used
    // (recommended for render targets that are recreated on resolution change).
    // Throws std::runtime_error if vmaCreateImage fails.
    static VmaImage createAttachment(VulkanContext* ctx, uint32_t width, uint32_t height,
                                     VkFormat format, VkImageUsageFlags usage);

    // Low-level: create from a fully-specified VkImageCreateInfo (for images that
    // need mips / array layers / other usage, e.g. textures). When dedicated is
    // true, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT is requested.
    // Throws std::runtime_error if vmaCreateImage fails.
    static VmaImage create(VulkanContext* ctx, const VkImageCreateInfo& ci, bool dedicated);

    void reset() noexcept;  // vmaDestroyImage (no-op if empty)

    VkImage image() const noexcept { return image_; }
    VmaAllocation allocation() const noexcept { return allocation_; }
    VkExtent2D extent() const noexcept { return {width_, height_}; }
    VkFormat format() const noexcept { return format_; }
    explicit operator bool() const noexcept { return image_ != VK_NULL_HANDLE; }

   private:
    void moveFrom(VmaImage& o) noexcept {
        allocator_ = o.allocator_;
        image_ = o.image_;
        allocation_ = o.allocation_;
        width_ = o.width_;
        height_ = o.height_;
        format_ = o.format_;
        o.allocator_ = VK_NULL_HANDLE;
        o.image_ = VK_NULL_HANDLE;
        o.allocation_ = VK_NULL_HANDLE;
        o.width_ = 0;
        o.height_ = 0;
        o.format_ = VK_FORMAT_UNDEFINED;
    }

    VmaAllocator allocator_ = VK_NULL_HANDLE;  // deleter context (not owned)
    VkImage image_ = VK_NULL_HANDLE;
    VmaAllocation allocation_ = VK_NULL_HANDLE;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    VkFormat format_ = VK_FORMAT_UNDEFINED;
};