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
    bufferSize_ = static_cast<VkDeviceSize>(MAX_INSTANCES) * sizeof(myengine::shared::InstanceData);
    createBuffers(resources);

    std::cout << "[InstanceBufferPool] init: " << MAX_INSTANCES << " instances, "
              << (bufferSize_ / 1024) << " KB / frame (BDA-only, no descriptors)\n";
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        std::cout << "  frame[" << i << "] buffer address = 0x" << std::hex
                  << buffers_[i].deviceAddress() << std::dec << "\n";
    }
}

void InstanceBufferPool::createBuffers(ResourceFactory* resources) {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        buffers_[i] = VmaBuffer::createMappedStorageBDA(ctx_, bufferSize_);
        cursor_[i] = 0;
    }
}

void InstanceBufferPool::beginFrame(uint32_t frameIndex) {
    if (frameIndex >= MAX_FRAMES_IN_FLIGHT) return;
    cursor_[frameIndex] = 0;
}

uint32_t InstanceBufferPool::push(uint32_t frameIndex,
                                  const std::vector<myengine::shared::InstanceData>& data) {
    if (frameIndex >= MAX_FRAMES_IN_FLIGHT) return UINT32_MAX;
    if (data.empty()) return UINT32_MAX;

    const uint32_t count = static_cast<uint32_t>(data.size());
    const uint32_t offset = cursor_[frameIndex];

    if (offset + count > MAX_INSTANCES) {
        std::cerr << "[InstanceBufferPool] WARNING: pool full (need " << (offset + count)
                  << ", max " << MAX_INSTANCES << ")\n";
        return UINT32_MAX;
    }

    uint8_t* dst = static_cast<uint8_t*>(buffers_[frameIndex].mapped()) +
                   static_cast<size_t>(offset) * sizeof(myengine::shared::InstanceData);
    std::memcpy(dst, data.data(), static_cast<size_t>(count) * sizeof(myengine::shared::InstanceData));

    cursor_[frameIndex] = offset + count;
    return offset;
}

void InstanceBufferPool::shutdown() {
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        buffers_[i].reset();
        cursor_[i] = 0;
    }
    ctx_ = nullptr;
}
