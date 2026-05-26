#pragma once
// =============================================================================
// vma_buffer.h - Move-only RAII wrapper for a VMA buffer (BDA + mapped)
// =============================================================================
// VkUnique<Handle> cannot own VMA resources: a VMA buffer is a (VkBuffer +
// VmaAllocation) pair that must be freed together with vmaDestroyBuffer(), and
// the allocator (not the device) is the deleter. VmaBuffer is the VMA analogue
// of VkUnique: it owns the pair, frees it in the destructor, and is move-only.
//
// Every VMA buffer in this engine (instance/skin/material pools) follows the
// same "BDA + persistently-mapped storage SSBO" recipe, so VmaBuffer::create()
// bakes that recipe in: VMA_ALLOCATION_CREATE_MAPPED_BIT, then grab the mapped
// pointer (allocInfo.pMappedData) and the buffer device address. Callers get a
// ready-to-write buffer with mapped() and deviceAddress() already populated,
// removing the ~20 lines of identical create/query code duplicated across the
// three pools.
//
// This header avoids including the full VMA header (to match the rest of the
// renderer headers, which only forward-declare the VMA handles); all VMA calls
// live in vma_buffer.cpp.
// =============================================================================
#include <vulkan/vulkan.h>
#include <cstdint>

// VMA forward declarations (same style as instance_buffer_pool.h etc.)
VK_DEFINE_HANDLE(VmaAllocation)
VK_DEFINE_HANDLE(VmaAllocator)

class VulkanContext;

class VmaBuffer {
   public:
    VmaBuffer() noexcept = default;
    ~VmaBuffer() { reset(); }

    VmaBuffer(const VmaBuffer&) = delete;
    VmaBuffer& operator=(const VmaBuffer&) = delete;

    VmaBuffer(VmaBuffer&& o) noexcept { moveFrom(o); }
    VmaBuffer& operator=(VmaBuffer&& o) noexcept {
        if (this != &o) {
            reset();
            moveFrom(o);
        }
        return *this;
    }

    // Create a persistently-mapped, BDA-enabled storage buffer (the recipe used
    // by every pool here). usage is STORAGE_BUFFER | SHADER_DEVICE_ADDRESS, OR'd
    // with extraUsage (e.g. INDIRECT_BUFFER for a GPU-written draw command buffer);
    // vmaFlags is OR'd with HOST_ACCESS_SEQUENTIAL_WRITE | MAPPED.
    // Throws std::runtime_error if mapping or the device address fails.
    static VmaBuffer createMappedStorageBDA(VulkanContext* ctx, VkDeviceSize size,
                                            VkBufferUsageFlags extraUsage = 0);

    // Create a persistently-mapped host-visible buffer with arbitrary usage and
    // no buffer device address (for vertex/index/uniform data the CPU writes each
    // frame and the GPU reads). Maps once via VMA; no manual vkMapMemory. usage is
    // taken as-is. Throws std::runtime_error if mapping fails. deviceAddress()
    // stays 0.
    static VmaBuffer createMappedHostVisible(VulkanContext* ctx, VkDeviceSize size,
                                             VkBufferUsageFlags usage);

    // Create a DEVICE-LOCAL buffer with arbitrary usage and NO persistent mapping
    // (for large GPU-resident geometry/storage written via staging copies, e.g.
    // the GeometryBuffer megabuffers). mapped() stays nullptr. If usage includes
    // VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, deviceAddress() is populated;
    // otherwise it stays 0. Throws std::runtime_error on failure.
    static VmaBuffer createDeviceLocal(VulkanContext* ctx, VkDeviceSize size,
                                       VkBufferUsageFlags usage);
    void reset() noexcept;  // vmaDestroyBuffer (no-op if empty)

    VkBuffer buffer() const noexcept { return buffer_; }
    VmaAllocation allocation() const noexcept { return allocation_; }
    void* mapped() const noexcept { return mapped_; }
    VkDeviceAddress deviceAddress() const noexcept { return address_; }
    VkDeviceSize size() const noexcept { return size_; }
    explicit operator bool() const noexcept { return buffer_ != VK_NULL_HANDLE; }

   private:
    void moveFrom(VmaBuffer& o) noexcept {
        allocator_ = o.allocator_;
        buffer_ = o.buffer_;
        allocation_ = o.allocation_;
        mapped_ = o.mapped_;
        address_ = o.address_;
        size_ = o.size_;
        o.allocator_ = VK_NULL_HANDLE;
        o.buffer_ = VK_NULL_HANDLE;
        o.allocation_ = VK_NULL_HANDLE;
        o.mapped_ = nullptr;
        o.address_ = 0;
        o.size_ = 0;
    }

    VmaAllocator allocator_ = VK_NULL_HANDLE;  // deleter context (not owned)
    VkBuffer buffer_ = VK_NULL_HANDLE;
    VmaAllocation allocation_ = VK_NULL_HANDLE;
    void* mapped_ = nullptr;
    VkDeviceAddress address_ = 0;
    VkDeviceSize size_ = 0;
};