// src/renderer/skin_buffer_pool.cpp - Phase 1B-4c: BDA-only
#include "renderer/skin_buffer_pool.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include <vk_mem_alloc.h>

#include "renderer/deletion_queue.h"
#include "renderer/resource_factory.h"
#include "renderer/vulkan_context.h"

void SkinBufferPool::init(VulkanContext* ctx, ResourceFactory* resources,
                          DeletionQueue* dq) {
    if (!ctx || !resources) throw std::runtime_error("SkinBufferPool::init: invalid args");
    if (!dq) throw std::runtime_error("SkinBufferPool::init: deletion queue is null");
    ctx_ = ctx;
    dq_ = dq;
    capacity_ = INITIAL_CAPACITY;

    allocateBuffers();
    freeSlots_.reserve(capacity_);
    appendFreeSlots(0, capacity_);
    allocatedCount_ = 0;

    std::cout << "[SkinBufferPool] init: " << capacity_ << " slots x "
              << MAX_BONES_PER_ENTITY << " bones, total "
              << (static_cast<VkDeviceSize>(capacity_) * MAX_BONES_PER_ENTITY *
                  sizeof(glm::mat4) / 1024)
              << " KB / frame (BDA-only, no descriptors)\n";
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        std::cout << "  frame[" << i << "] buffer address = 0x" << std::hex
                  << buffers_[i].deviceAddress() << std::dec << "\n";
    }
}

void SkinBufferPool::allocateBuffers() {
    const VkDeviceSize size = static_cast<VkDeviceSize>(capacity_) *
                              MAX_BONES_PER_ENTITY * sizeof(glm::mat4);
    const uint32_t totalBones = capacity_ * MAX_BONES_PER_ENTITY;
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        buffers_[i] = VmaBuffer::createMappedStorageBDA(ctx_, size);
        // Initialise to identity so unallocated slots render as a no-op skin
        // (their entities also pass an empty skin pointer, so this is a
        // defence-in-depth -- a stray bind would still show the rest pose).
        glm::mat4 ident(1.f);
        uint8_t* dst = static_cast<uint8_t*>(buffers_[i].mapped());
        for (uint32_t b = 0; b < totalBones; ++b) {
            std::memcpy(dst + b * sizeof(glm::mat4), &ident, sizeof(glm::mat4));
        }
    }
}

void SkinBufferPool::appendFreeSlots(uint32_t fromIdx, uint32_t toIdx) {
    // Push (toIdx-1, toIdx-2, ..., fromIdx) so allocate() (pop_back) hands out
    // slot fromIdx first. Matches the original initFreeList ordering.
    for (uint32_t i = 0; i < toIdx - fromIdx; ++i) {
        freeSlots_.push_back(toIdx - 1 - i);
    }
}

void SkinBufferPool::growToDouble() {
    const uint32_t oldCapacity = capacity_;
    capacity_ = oldCapacity * 2;

    // Hand the old buffer pair to the DeletionQueue so any in-flight cmd
    // buffer that baked their BDA stays valid for the FIF window.
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkBuffer oldBuf = buffers_[i].buffer();
        VmaAllocation oldAlloc = buffers_[i].allocation();
        if (oldBuf != VK_NULL_HANDLE) {
            dq_->enqueueBuffer(oldBuf, oldAlloc);
            buffers_[i].release();
        }
    }

    allocateBuffers();

    // Existing Slots stay valid because their boneOffset is < oldCapacity *
    // MAX_BONES_PER_ENTITY which is < new capacity * MAX_BONES_PER_ENTITY.
    // Bone matrices are rewritten every frame via update(), so we don't have
    // to copy the old per-frame contents -- the next animation pass fills
    // every active slot. Append the new free slot range.
    appendFreeSlots(oldCapacity, capacity_);

    std::cout << "[SkinBufferPool] grew capacity to " << capacity_ << " slots, "
              << (static_cast<VkDeviceSize>(capacity_) * MAX_BONES_PER_ENTITY *
                  sizeof(glm::mat4) / 1024)
              << " KB / frame\n";
}

SkinBufferPool::Slot SkinBufferPool::allocate() {
    if (freeSlots_.empty()) {
        growToDouble();
    }
    const uint32_t slotIndex = freeSlots_.back();
    freeSlots_.pop_back();
    allocatedCount_++;

    Slot s;
    s.boneOffset = slotIndex * MAX_BONES_PER_ENTITY;
    s.boneCapacity = MAX_BONES_PER_ENTITY;
    return s;
}

void SkinBufferPool::release(Slot slot) {
    if (!slot.valid()) return;
    if (slot.boneOffset % MAX_BONES_PER_ENTITY != 0) {
        std::cerr << "[SkinBufferPool] WARNING: invalid slot release (offset="
                  << slot.boneOffset << ")\n";
        return;
    }
    const uint32_t slotIndex = slot.boneOffset / MAX_BONES_PER_ENTITY;
    if (slotIndex >= capacity_) {
        std::cerr << "[SkinBufferPool] WARNING: slot index out of range\n";
        return;
    }
    freeSlots_.push_back(slotIndex);
    if (allocatedCount_ > 0) allocatedCount_--;
}

void SkinBufferPool::update(uint32_t frameIndex, const Slot& slot,
                            const std::vector<glm::mat4>& matrices) {
    if (frameIndex >= MAX_FRAMES_IN_FLIGHT) return;
    if (!slot.valid()) return;
    if (matrices.empty()) return;

    const size_t n = std::min(matrices.size(), static_cast<size_t>(slot.boneCapacity));
    uint8_t* dst = static_cast<uint8_t*>(buffers_[frameIndex].mapped()) +
                   slot.boneOffset * sizeof(glm::mat4);
    std::memcpy(dst, matrices.data(), n * sizeof(glm::mat4));
}

void SkinBufferPool::shutdown() {
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        buffers_[i].reset();
    }
    freeSlots_.clear();
    allocatedCount_ = 0;
    capacity_ = 0;
    dq_ = nullptr;
    ctx_ = nullptr;
}
