// src/renderer/instance_buffer_pool.cpp - Phase 1E: BDA-only instance matrices
#include "renderer/instance_buffer_pool.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include <vk_mem_alloc.h>

#include "renderer/deletion_queue.h"
#include "renderer/resource_factory.h"
#include "renderer/vulkan_context.h"

using myengine::shared::InstanceData;

void InstanceBufferPool::init(VulkanContext* ctx, ResourceFactory* resources,
                              DeletionQueue* dq) {
    if (!ctx || !resources) throw std::runtime_error("InstanceBufferPool::init: invalid args");
    if (!dq) throw std::runtime_error("InstanceBufferPool::init: deletion queue is null");
    ctx_ = ctx;
    dq_ = dq;
    capacity_ = INITIAL_CAPACITY;
    allocateBuffers();

    std::cout << "[InstanceBufferPool] init: " << capacity_ << " instances, "
              << (static_cast<VkDeviceSize>(capacity_) * sizeof(InstanceData) / 1024)
              << " KB / frame (BDA-only, no descriptors)\n";
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        std::cout << "  frame[" << i << "] buffer address = 0x" << std::hex
                  << buffers_[i].deviceAddress() << std::dec << "\n";
    }
}

void InstanceBufferPool::allocateBuffers() {
    const VkDeviceSize size =
        static_cast<VkDeviceSize>(capacity_) * sizeof(InstanceData);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        buffers_[i] = VmaBuffer::createMappedStorageBDA(ctx_, size);
        cursor_[i] = 0;
    }
}

void InstanceBufferPool::growToFitPeak() {
    uint32_t newCapacity = capacity_;
    while (newCapacity < peakRequested_) newCapacity *= 2;
    if (newCapacity == capacity_) return;

    // Hand the current buffer pair to the DeletionQueue: any in-flight cmd
    // buffer that already baked their BDA stays valid for the
    // MAX_FRAMES_IN_FLIGHT window. After release() each VmaBuffer is empty
    // so the destructor will not double-free.
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkBuffer oldBuf = buffers_[i].buffer();
        VmaAllocation oldAlloc = buffers_[i].allocation();
        if (oldBuf != VK_NULL_HANDLE) {
            dq_->enqueueBuffer(oldBuf, oldAlloc);
            buffers_[i].release();
        }
    }
    capacity_ = newCapacity;
    allocateBuffers();
    peakRequested_ = 0;

    std::cout << "[InstanceBufferPool] grew capacity to " << capacity_ << " instances, "
              << (static_cast<VkDeviceSize>(capacity_) * sizeof(InstanceData) / 1024)
              << " KB / frame\n";
}

void InstanceBufferPool::beginFrame(uint32_t frameIndex) {
    if (frameIndex >= MAX_FRAMES_IN_FLIGHT) return;
    // First, settle any outstanding grow request from the previous frame's
    // overflow. BDA changes propagate to consumers via FrameUBO rebuild this
    // frame.
    if (peakRequested_ > capacity_) {
        growToFitPeak();
    }
    cursor_[frameIndex] = 0;
}

uint32_t InstanceBufferPool::push(uint32_t frameIndex,
                                  const std::vector<InstanceData>& data) {
    if (frameIndex >= MAX_FRAMES_IN_FLIGHT) return UINT32_MAX;
    if (data.empty()) return UINT32_MAX;

    const uint32_t count = static_cast<uint32_t>(data.size());
    const uint32_t offset = cursor_[frameIndex];
    const uint32_t needed = offset + count;

    // Remember the largest request even when it overflows; next beginFrame
    // grows to fit.
    if (needed > peakRequested_) peakRequested_ = needed;

    if (needed > capacity_) {
        std::cerr << "[InstanceBufferPool] WARNING: pool full this frame (need "
                  << needed << ", capacity " << capacity_
                  << ") -- growing on next beginFrame()\n";
        return UINT32_MAX;
    }

    uint8_t* dst = static_cast<uint8_t*>(buffers_[frameIndex].mapped()) +
                   static_cast<size_t>(offset) * sizeof(InstanceData);
    std::memcpy(dst, data.data(), static_cast<size_t>(count) * sizeof(InstanceData));

    cursor_[frameIndex] = needed;
    return offset;
}

void InstanceBufferPool::shutdown() {
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        buffers_[i].reset();
        cursor_[i] = 0;
    }
    capacity_ = 0;
    peakRequested_ = 0;
    dq_ = nullptr;
    ctx_ = nullptr;
}
