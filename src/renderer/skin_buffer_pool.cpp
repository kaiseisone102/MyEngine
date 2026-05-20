// src/renderer/skin_buffer_pool.cpp - Phase 1B-4: BDA-enabled
#include "renderer/skin_buffer_pool.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include <vk_mem_alloc.h>

#include "renderer/resource_factory.h"
#include "renderer/vulkan_context.h"

void SkinBufferPool::init(VulkanContext* ctx, ResourceFactory* resources) {
    if (!ctx || !resources) throw std::runtime_error("SkinBufferPool::init: invalid args");
    ctx_ = ctx;

    bufferSize_ = static_cast<VkDeviceSize>(TOTAL_BONES) * sizeof(glm::mat4);

    createLayout();
    createPool();
    createBuffers(resources);
    allocateAndWriteSets();
    initFreeList();

    std::cout << "[SkinBufferPool] init: " << MAX_ENTITIES << " slots x "
              << MAX_BONES_PER_ENTITY << " bones, total " << (bufferSize_ / 1024)
              << " KB / frame (BDA enabled)\n";
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        std::cout << "  frame[" << i << "] buffer address = 0x" << std::hex
                  << addresses_[i] << std::dec << "\n";
    }
}

void SkinBufferPool::createLayout() {
    // Legacy descriptor path - kept during 1B-4a transition.
    VkDescriptorSetLayoutBinding bind{};
    bind.binding = 0;
    bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bind.descriptorCount = 1;
    bind.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    ci.bindingCount = 1;
    ci.pBindings = &bind;

    if (vkCreateDescriptorSetLayout(ctx_->device(), &ci, nullptr, &layout_) != VK_SUCCESS) {
        throw std::runtime_error("SkinBufferPool: vkCreateDescriptorSetLayout failed");
    }
}

void SkinBufferPool::createPool() {
    VkDescriptorPoolSize sz{};
    sz.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    sz.descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    ci.poolSizeCount = 1;
    ci.pPoolSizes = &sz;
    ci.maxSets = MAX_FRAMES_IN_FLIGHT;

    if (vkCreateDescriptorPool(ctx_->device(), &ci, nullptr, &pool_) != VK_SUCCESS) {
        throw std::runtime_error("SkinBufferPool: vkCreateDescriptorPool failed");
    }
}

void SkinBufferPool::createBuffers(ResourceFactory* resources) {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        // VMA-based buffer creation with BDA support.
        // VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT enables vkGetBufferDeviceAddress.
        // VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT means HOST_VISIBLE.
        // VMA_ALLOCATION_CREATE_MAPPED_BIT keeps the buffer persistently mapped.
        resources->createBufferVMA(
            bufferSize_,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT,
            buffers_[i], allocations_[i]);

        // Retrieve persistently mapped pointer.
        VmaAllocationInfo allocInfo{};
        vmaGetAllocationInfo(ctx_->allocator(), allocations_[i], &allocInfo);
        mapped_[i] = allocInfo.pMappedData;
        if (!mapped_[i]) {
            throw std::runtime_error(
                "SkinBufferPool: VMA allocation is not persistently mapped");
        }

        // Get the GPU device address for this buffer.
        VkBufferDeviceAddressInfo bai{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
        bai.buffer = buffers_[i];
        addresses_[i] = vkGetBufferDeviceAddress(ctx_->device(), &bai);
        if (addresses_[i] == 0) {
            throw std::runtime_error(
                "SkinBufferPool: vkGetBufferDeviceAddress returned 0");
        }

        // Initialize whole region with identity matrices (safe default before
        // any animator writes).
        glm::mat4 ident(1.f);
        for (uint32_t b = 0; b < TOTAL_BONES; ++b) {
            std::memcpy(static_cast<uint8_t*>(mapped_[i]) + b * sizeof(glm::mat4), &ident,
                        sizeof(glm::mat4));
        }
    }
}

void SkinBufferPool::allocateAndWriteSets() {
    // Legacy descriptor allocation - kept during 1B-4a transition so shaders
    // that still bind set=2 keep working.
    std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts{};
    for (auto& l : layouts) l = layout_;

    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = pool_;
    ai.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    ai.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(ctx_->device(), &ai, sets_.data()) != VK_SUCCESS) {
        throw std::runtime_error("SkinBufferPool: vkAllocateDescriptorSets failed");
    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkDescriptorBufferInfo bi{};
        bi.buffer = buffers_[i];
        bi.offset = 0;
        bi.range = bufferSize_;

        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet = sets_[i];
        w.dstBinding = 0;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.pBufferInfo = &bi;
        vkUpdateDescriptorSets(ctx_->device(), 1, &w, 0, nullptr);
    }
}

void SkinBufferPool::initFreeList() {
    freeSlots_.clear();
    freeSlots_.reserve(MAX_ENTITIES);
    for (uint32_t i = 0; i < MAX_ENTITIES; ++i) {
        freeSlots_.push_back(MAX_ENTITIES - 1 - i);
    }
    allocatedCount_ = 0;
}

SkinBufferPool::Slot SkinBufferPool::allocate() {
    if (freeSlots_.empty()) {
        std::cerr << "[SkinBufferPool] WARNING: no free slots (max=" << MAX_ENTITIES << ")\n";
        return Slot::invalid();
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
    if (slotIndex >= MAX_ENTITIES) {
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
    uint8_t* dst = static_cast<uint8_t*>(mapped_[frameIndex]) +
                   slot.boneOffset * sizeof(glm::mat4);
    std::memcpy(dst, matrices.data(), n * sizeof(glm::mat4));
}

void SkinBufferPool::shutdown() {
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (buffers_[i] != VK_NULL_HANDLE) {
            // VMA destroys both the buffer and the allocation; persistent
            // mapping is automatically unmapped.
            vmaDestroyBuffer(ctx_->allocator(), buffers_[i], allocations_[i]);
            buffers_[i] = VK_NULL_HANDLE;
            allocations_[i] = VK_NULL_HANDLE;
            mapped_[i] = nullptr;
            addresses_[i] = 0;
        }
    }
    if (pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(ctx_->device(), pool_, nullptr);
        pool_ = VK_NULL_HANDLE;
    }
    if (layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(ctx_->device(), layout_, nullptr);
        layout_ = VK_NULL_HANDLE;
    }
    freeSlots_.clear();
    allocatedCount_ = 0;
    ctx_ = nullptr;
}
