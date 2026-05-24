#pragma once
// =============================================================================
// vk_unique.h - Move-only RAII wrapper for a single Vulkan handle
// =============================================================================
// VkUnique<Handle> is the "unique_ptr of Vulkan handles": it owns one handle,
// frees it in the destructor via the matching vkDestroy*/vkFree*, and is
// move-only (copy deleted, move noexcept). A class whose members are all
// VkUnique/value types then needs NO hand-written destructor / move / copy
// (Rule of Zero): the compiler-generated special members are correct, and
// adding a member can no longer be silently forgotten in a hand-written
// moveFrom(). Move being noexcept also lets std::vector<T> move (not copy) on
// reallocation.
//
// Each VkUnique stores the VkDevice next to the handle, because vkDestroy*
// needs the device (mirrors vk::raii, which carries its dispatcher). 16 bytes
// per handle on x64.
//
// To teach VkUnique a new handle type, add one MYENGINE_DEFINE_VK_DELETER line.
// A handle type without a deleter specialization is a compile error by design.
//
// NOTE: VMA-allocated resources (VkImage/VkBuffer + VmaAllocation) are NOT here
// -- they must be freed with vmaDestroyImage/vmaDestroyBuffer (which also needs
// the VmaAllocation). Those get dedicated VmaImage/VmaBuffer wrappers when the
// VMA-using classes (buffer pools, VMA images) are migrated.
// =============================================================================
#include <vulkan/vulkan.h>
#include <utility>

// Primary template intentionally left undefined; specialized per handle type.
template <typename Handle>
struct VkHandleDeleter;

#define MYENGINE_DEFINE_VK_DELETER(HandleType, destroyExpr) \
    template <> struct VkHandleDeleter<HandleType> { \
        void operator()(VkDevice d, HandleType h) const { destroyExpr(d, h, nullptr); } \
    }

// Standard vkDestroyX(device, x, pAllocator) handles.
MYENGINE_DEFINE_VK_DELETER(VkImage, vkDestroyImage);
MYENGINE_DEFINE_VK_DELETER(VkImageView, vkDestroyImageView);
MYENGINE_DEFINE_VK_DELETER(VkSampler, vkDestroySampler);
MYENGINE_DEFINE_VK_DELETER(VkBuffer, vkDestroyBuffer);
MYENGINE_DEFINE_VK_DELETER(VkBufferView, vkDestroyBufferView);
MYENGINE_DEFINE_VK_DELETER(VkPipeline, vkDestroyPipeline);
MYENGINE_DEFINE_VK_DELETER(VkPipelineLayout, vkDestroyPipelineLayout);
MYENGINE_DEFINE_VK_DELETER(VkPipelineCache, vkDestroyPipelineCache);
MYENGINE_DEFINE_VK_DELETER(VkRenderPass, vkDestroyRenderPass);
MYENGINE_DEFINE_VK_DELETER(VkFramebuffer, vkDestroyFramebuffer);
MYENGINE_DEFINE_VK_DELETER(VkDescriptorSetLayout, vkDestroyDescriptorSetLayout);
MYENGINE_DEFINE_VK_DELETER(VkDescriptorPool, vkDestroyDescriptorPool);
MYENGINE_DEFINE_VK_DELETER(VkShaderModule, vkDestroyShaderModule);
MYENGINE_DEFINE_VK_DELETER(VkSemaphore, vkDestroySemaphore);
MYENGINE_DEFINE_VK_DELETER(VkFence, vkDestroyFence);
MYENGINE_DEFINE_VK_DELETER(VkEvent, vkDestroyEvent);
MYENGINE_DEFINE_VK_DELETER(VkQueryPool, vkDestroyQueryPool);
MYENGINE_DEFINE_VK_DELETER(VkCommandPool, vkDestroyCommandPool);
MYENGINE_DEFINE_VK_DELETER(VkSwapchainKHR, vkDestroySwapchainKHR);
// Device memory is freed, not destroyed.
MYENGINE_DEFINE_VK_DELETER(VkDeviceMemory, vkFreeMemory);

template <typename Handle>
class VkUnique {
   public:
    VkUnique() noexcept = default;
    VkUnique(VkDevice device, Handle handle) noexcept : device_(device), handle_(handle) {}
    ~VkUnique() { reset(); }

    VkUnique(const VkUnique&) = delete;
    VkUnique& operator=(const VkUnique&) = delete;

    VkUnique(VkUnique&& o) noexcept : device_(o.device_), handle_(o.handle_) {
        o.device_ = VK_NULL_HANDLE;
        o.handle_ = VK_NULL_HANDLE;
    }
    VkUnique& operator=(VkUnique&& o) noexcept {
        if (this != &o) {
            reset();
            device_ = o.device_;
            handle_ = o.handle_;
            o.device_ = VK_NULL_HANDLE;
            o.handle_ = VK_NULL_HANDLE;
        }
        return *this;
    }

    Handle get() const noexcept { return handle_; }
    explicit operator bool() const noexcept { return handle_ != VK_NULL_HANDLE; }

    // Free the owned handle now (no-op if empty). Idempotent.
    void reset() noexcept {
        if (handle_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
            VkHandleDeleter<Handle>{}(device_, handle_);
        }
        handle_ = VK_NULL_HANDLE;
        device_ = VK_NULL_HANDLE;
    }

   private:
    VkDevice device_ = VK_NULL_HANDLE;
    Handle handle_ = VK_NULL_HANDLE;
};