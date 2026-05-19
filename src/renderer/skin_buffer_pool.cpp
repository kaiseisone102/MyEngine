// src/renderer/skin_buffer_pool.cpp
#include "renderer/skin_buffer_pool.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>

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

    std::cout << "[SkinBufferPool] init: " << MAX_ENTITIES << " slots × "
              << MAX_BONES_PER_ENTITY << " bones, total " << (bufferSize_ / 1024) << " KB / frame\n";
}

void SkinBufferPool::createLayout() {
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
        resources->createBuffer(
            bufferSize_, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            buffers_[i], memories_[i]);
        if (vkMapMemory(ctx_->device(), memories_[i], 0, bufferSize_, 0, &mapped_[i]) !=
            VK_SUCCESS) {
            throw std::runtime_error("SkinBufferPool: vkMapMemory failed");
        }
        // 初期化: 全領域を単位行列で埋める (バインドポーズ相当の安全な初期状態)
        glm::mat4 ident(1.f);
        for (uint32_t b = 0; b < TOTAL_BONES; ++b) {
            std::memcpy(static_cast<uint8_t*>(mapped_[i]) + b * sizeof(glm::mat4), &ident,
                        sizeof(glm::mat4));
        }
    }
}

void SkinBufferPool::allocateAndWriteSets() {
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
    // 後ろから push (= LIFO で前から取り出される)
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
        if (mapped_[i]) {
            vkUnmapMemory(ctx_->device(), memories_[i]);
            mapped_[i] = nullptr;
        }
        if (buffers_[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(ctx_->device(), buffers_[i], nullptr);
            buffers_[i] = VK_NULL_HANDLE;
        }
        if (memories_[i] != VK_NULL_HANDLE) {
            vkFreeMemory(ctx_->device(), memories_[i], nullptr);
            memories_[i] = VK_NULL_HANDLE;
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
