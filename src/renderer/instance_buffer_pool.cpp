// src/renderer/instance_buffer_pool.cpp - Phase 1E: BDA-only instance matrices
#include "renderer/instance_buffer_pool.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include <vk_mem_alloc.h>

#include "renderer/resource_factory.h"
#include "renderer/vulkan_context.h"

void InstanceBufferPool::init(VulkanContext* ctx, ResourceFactory* resources) {
    if (!ctx || !resources) throw std::runtime_error("InstanceBufferPool::init: invalid args");
    ctx_ = ctx;
    bufferSize_ = static_cast<VkDeviceSize>(MAX_INSTANCES) * sizeof(glm::mat4);
    createBuffers(resources);

    std::cout << "[InstanceBufferPool] init: " << MAX_INSTANCES << " instances, "
              << (bufferSize_ / 1024) << " KB / frame (BDA-only, no descriptors)\n";
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        std::cout << "  frame[" << i << "] buffer address = 0x" << std::hex
                  << addresses_[i] << std::dec << "\n";
    }
}

void InstanceBufferPool::createBuffers(ResourceFactory* resources) {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        resources->createBufferVMA(
            bufferSize_,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT,
            buffers_[i], allocations_[i]);

        VmaAllocationInfo allocInfo{};
        vmaGetAllocationInfo(ctx_->allocator(), allocations_[i], &allocInfo);
        mapped_[i] = allocInfo.pMappedData;
        if (!mapped_[i]) {
            throw std::runtime_error(
                "InstanceBufferPool: VMA allocation is not persistently mapped");
        }

        VkBufferDeviceAddressInfo bai{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
        bai.buffer = buffers_[i];
        addresses_[i] = vkGetBufferDeviceAddress(ctx_->device(), &bai);
        if (addresses_[i] == 0) {
            throw std::runtime_error(
                "InstanceBufferPool: vkGetBufferDeviceAddress returned 0");
        }

        cursor_[i] = 0;
    }
}

void InstanceBufferPool::beginFrame(uint32_t frameIndex) {
    if (frameIndex >= MAX_FRAMES_IN_FLIGHT) return;
    cursor_[frameIndex] = 0;
}

uint32_t InstanceBufferPool::push(uint32_t frameIndex,
                                  const std::vector<glm::mat4>& matrices) {
    if (frameIndex >= MAX_FRAMES_IN_FLIGHT) return UINT32_MAX;
    if (matrices.empty()) return UINT32_MAX;

    const uint32_t count = static_cast<uint32_t>(matrices.size());
    const uint32_t offset = cursor_[frameIndex];

    if (offset + count > MAX_INSTANCES) {
        std::cerr << "[InstanceBufferPool] WARNING: pool full (need " << (offset + count)
                  << ", max " << MAX_INSTANCES << ")\n";
        return UINT32_MAX;
    }

    uint8_t* dst = static_cast<uint8_t*>(mapped_[frameIndex]) +
                   static_cast<size_t>(offset) * sizeof(glm::mat4);
    std::memcpy(dst, matrices.data(), static_cast<size_t>(count) * sizeof(glm::mat4));

    cursor_[frameIndex] = offset + count;
    return offset;
}

void InstanceBufferPool::shutdown() {
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (buffers_[i] != VK_NULL_HANDLE) {
            vmaDestroyBuffer(ctx_->allocator(), buffers_[i], allocations_[i]);
            buffers_[i] = VK_NULL_HANDLE;
            allocations_[i] = VK_NULL_HANDLE;
            mapped_[i] = nullptr;
            addresses_[i] = 0;
            cursor_[i] = 0;
        }
    }
    ctx_ = nullptr;
}
