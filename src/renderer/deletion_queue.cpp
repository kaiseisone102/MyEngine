// src/renderer/deletion_queue.cpp
#include "renderer/deletion_queue.h"

#include <vk_mem_alloc.h>

#include "renderer/vulkan_context.h"

void DeletionQueue::init(VulkanContext* ctx) {
    ctx_ = ctx;
    allocator_ = ctx ? ctx->allocator() : VK_NULL_HANDLE;
    current_ = 0;
    for (auto& b : buckets_) b.clear();
}

void DeletionQueue::shutdown() {
    // Caller is responsible for vkDeviceWaitIdle before this. Flush every bucket.
    flushAll();
    ctx_ = nullptr;
    allocator_ = VK_NULL_HANDLE;
}

void DeletionQueue::enqueue(std::function<void()> deleter) {
    if (!deleter) return;
    buckets_[current_].push_back(std::move(deleter));
}

void DeletionQueue::enqueueBuffer(VkBuffer buffer, VmaAllocation allocation) {
    if (buffer == VK_NULL_HANDLE) return;
    VmaAllocator alloc = allocator_;
    buckets_[current_].push_back([alloc, buffer, allocation]() {
        if (alloc != VK_NULL_HANDLE) vmaDestroyBuffer(alloc, buffer, allocation);
    });
}

void DeletionQueue::enqueueImage(VkImage image, VmaAllocation allocation) {
    if (image == VK_NULL_HANDLE) return;
    VmaAllocator alloc = allocator_;
    buckets_[current_].push_back([alloc, image, allocation]() {
        if (alloc != VK_NULL_HANDLE) vmaDestroyImage(alloc, image, allocation);
    });
}

void DeletionQueue::flushBucket(uint32_t frameIndex) {
    if (frameIndex >= MAX_FRAMES_IN_FLIGHT) return;
    auto& bucket = buckets_[frameIndex];
    // Run in reverse insertion order (destroy dependents before dependencies),
    // mirroring the usual "deletion stack" convention.
    for (auto it = bucket.rbegin(); it != bucket.rend(); ++it) {
        if (*it) (*it)();
    }
    bucket.clear();
}

void DeletionQueue::collectFrame(uint32_t frameIndex) {
    flushBucket(frameIndex);
}

void DeletionQueue::flushAll() {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) flushBucket(i);
}