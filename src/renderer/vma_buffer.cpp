// src/renderer/vma_buffer.cpp
#include "renderer/vma_buffer.h"

#include <stdexcept>

#include <vk_mem_alloc.h>

#include "renderer/vulkan_context.h"

VmaBuffer VmaBuffer::createMappedStorageBDA(VulkanContext* ctx, VkDeviceSize size,
                                            VkBufferUsageFlags extraUsage) {
    if (!ctx) throw std::runtime_error("VmaBuffer::createMappedStorageBDA: null ctx");

    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
               extraUsage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo ai{};
    ai.usage = VMA_MEMORY_USAGE_AUTO;
    ai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
               VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaBuffer out;
    out.allocator_ = ctx->allocator();
    out.size_ = size;

    VmaAllocationInfo allocInfo{};
    if (vmaCreateBuffer(out.allocator_, &bi, &ai, &out.buffer_, &out.allocation_, &allocInfo) !=
        VK_SUCCESS) {
        throw std::runtime_error("VmaBuffer::createMappedStorageBDA: vmaCreateBuffer failed");
    }

    out.mapped_ = allocInfo.pMappedData;
    if (!out.mapped_) {
        out.reset();
        throw std::runtime_error("VmaBuffer::createMappedStorageBDA: not persistently mapped");
    }

    VkBufferDeviceAddressInfo bai{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    bai.buffer = out.buffer_;
    out.address_ = vkGetBufferDeviceAddress(ctx->device(), &bai);
    if (out.address_ == 0) {
        out.reset();
        throw std::runtime_error("VmaBuffer::createMappedStorageBDA: device address is 0");
    }

    return out;
}

VmaBuffer VmaBuffer::createMappedHostVisible(VulkanContext* ctx, VkDeviceSize size,
                                             VkBufferUsageFlags usage) {
    if (!ctx) throw std::runtime_error("VmaBuffer::createMappedHostVisible: null ctx");

    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo ai{};
    ai.usage = VMA_MEMORY_USAGE_AUTO;
    ai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
               VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaBuffer out;
    out.allocator_ = ctx->allocator();
    out.size_ = size;

    VmaAllocationInfo allocInfo{};
    if (vmaCreateBuffer(out.allocator_, &bi, &ai, &out.buffer_, &out.allocation_, &allocInfo) !=
        VK_SUCCESS) {
        throw std::runtime_error("VmaBuffer::createMappedHostVisible: vmaCreateBuffer failed");
    }

    out.mapped_ = allocInfo.pMappedData;
    if (!out.mapped_) {
        out.reset();
        throw std::runtime_error("VmaBuffer::createMappedHostVisible: not persistently mapped");
    }
    // No buffer device address requested; address_ stays 0.
    return out;
}

VmaBuffer VmaBuffer::createDeviceLocal(VulkanContext* ctx, VkDeviceSize size,
                                       VkBufferUsageFlags usage) {
    if (!ctx) throw std::runtime_error("VmaBuffer::createDeviceLocal: null ctx");

    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // Device-local, no host access, no persistent mapping. VMA_MEMORY_USAGE_AUTO
    // with no HOST_ACCESS flag selects DEVICE_LOCAL on a discrete/typical GPU.
    VmaAllocationCreateInfo ai{};
    ai.usage = VMA_MEMORY_USAGE_AUTO;
    ai.flags = 0;

    VmaBuffer out;
    out.allocator_ = ctx->allocator();
    out.size_ = size;

    if (vmaCreateBuffer(out.allocator_, &bi, &ai, &out.buffer_, &out.allocation_, nullptr) !=
        VK_SUCCESS) {
        throw std::runtime_error("VmaBuffer::createDeviceLocal: vmaCreateBuffer failed");
    }

    // mapped_ stays nullptr (device-local). Populate address only if requested.
    if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        VkBufferDeviceAddressInfo bai{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
        bai.buffer = out.buffer_;
        out.address_ = vkGetBufferDeviceAddress(ctx->device(), &bai);
        if (out.address_ == 0) {
            out.reset();
            throw std::runtime_error("VmaBuffer::createDeviceLocal: device address is 0");
        }
    }
    return out;
}

void VmaBuffer::reset() noexcept {
    if (buffer_ != VK_NULL_HANDLE && allocator_ != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator_, buffer_, allocation_);
    }
    allocator_ = VK_NULL_HANDLE;
    buffer_ = VK_NULL_HANDLE;
    allocation_ = VK_NULL_HANDLE;
    mapped_ = nullptr;
    address_ = 0;
    size_ = 0;
}