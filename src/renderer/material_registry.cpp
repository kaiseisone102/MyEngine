// src/renderer/material_registry.cpp
#include "renderer/material_registry.h"

#include <cstring>
#include <iostream>
#include <stdexcept>

#include "renderer/deletion_queue.h"
#include "renderer/resource_factory.h"
#include "renderer/vulkan_context.h"
#include "vk_mem_alloc.h"

using myengine::shared::GpuMaterial;

void MaterialRegistry::init(VulkanContext* ctx, ResourceFactory* resources, DeletionQueue* dq) {
    if (!ctx || !resources) throw std::runtime_error("MaterialRegistry::init: invalid args");
    if (!dq) throw std::runtime_error("MaterialRegistry::init: deletion queue is null");
    ctx_ = ctx;
    dq_ = dq;

    capacity_ = INITIAL_CAPACITY;
    buffer_ = VmaBuffer::createMappedStorageBDA(
        ctx_, static_cast<VkDeviceSize>(capacity_) * sizeof(GpuMaterial));

    // Slot 0 is always a sane default material (mid-grey dielectric).
    GpuMaterial def{};
    def.baseColorFactor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
    def.metallic = 0.0f;
    def.roughness = 0.5f;
    def.emissiveStrength = 0.0f;
    def.albedoIdx = -1;
    def.normalIdx = -1;
    def.mrIdx = -1;
    def.aoIdx = -1;
    def.emissiveIdx = -1;
    add("__default", def);  // -> id 0
    upload();

    std::cout << "[MaterialRegistry] init: capacity " << capacity_ << " materials, "
              << (static_cast<VkDeviceSize>(capacity_) * sizeof(GpuMaterial) / 1024)
              << " KB (BDA-only, no descriptors)\n";
}

void MaterialRegistry::growTo(uint32_t requiredCount) {
    uint32_t newCapacity = capacity_;
    while (newCapacity < requiredCount) newCapacity *= 2;
    if (newCapacity == capacity_) return;

    const VkDeviceSize newSize =
        static_cast<VkDeviceSize>(newCapacity) * sizeof(GpuMaterial);
    VmaBuffer newBuffer = VmaBuffer::createMappedStorageBDA(ctx_, newSize);

    // Carry over the materials that have already been recorded on the CPU.
    // (upload() will be called by the caller after the add() that triggered
    // this growth, so the GPU copy is implicit -- but copying eagerly here
    // means a stray reader that calls bufferAddress() between grow and
    // upload sees a consistent view too.)
    if (!cpuMaterials_.empty()) {
        std::memcpy(newBuffer.mapped(), cpuMaterials_.data(),
                    cpuMaterials_.size() * sizeof(GpuMaterial));
    }

    // Old buffer must outlive every in-flight frame that already had its BDA
    // baked into a command buffer. DeletionQueue extends its lifetime by
    // MAX_FRAMES_IN_FLIGHT frames, which matches FrameSync.
    VkBuffer oldBuf = buffer_.buffer();
    VmaAllocation oldAlloc = buffer_.allocation();
    if (oldBuf != VK_NULL_HANDLE) {
        dq_->enqueueBuffer(oldBuf, oldAlloc);
        buffer_.release();  // give up ownership without destroying
    }

    buffer_ = std::move(newBuffer);
    capacity_ = newCapacity;

    std::cout << "[MaterialRegistry] grew capacity to " << capacity_ << " materials, "
              << (newSize / 1024) << " KB\n";
}

uint32_t MaterialRegistry::add(const std::string& name, const GpuMaterial& material) {
    auto it = nameToId_.find(name);
    if (it != nameToId_.end()) {
        cpuMaterials_[it->second] = material;  // update in place
        dirty_ = true;
        return it->second;
    }
    const uint32_t newCount = static_cast<uint32_t>(cpuMaterials_.size()) + 1;
    if (newCount > capacity_) {
        growTo(newCount);
    }
    uint32_t id = static_cast<uint32_t>(cpuMaterials_.size());
    cpuMaterials_.push_back(material);
    nameToId_[name] = id;
    dirty_ = true;
    return id;
}

uint32_t MaterialRegistry::getId(const std::string& name) const {
    auto it = nameToId_.find(name);
    return (it != nameToId_.end()) ? it->second : kDefaultMaterialId;
}

void MaterialRegistry::upload() {
    if (!dirty_ || cpuMaterials_.empty()) return;
    std::memcpy(buffer_.mapped(), cpuMaterials_.data(), cpuMaterials_.size() * sizeof(GpuMaterial));
    dirty_ = false;
}

void MaterialRegistry::shutdown() {
    buffer_.reset();
    cpuMaterials_.clear();
    nameToId_.clear();
    capacity_ = 0;
    dq_ = nullptr;
}
