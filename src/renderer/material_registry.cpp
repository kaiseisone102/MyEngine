// src/renderer/material_registry.cpp
#include "renderer/material_registry.h"

#include <cstring>
#include <iostream>
#include <stdexcept>

#include "renderer/resource_factory.h"
#include "renderer/vulkan_context.h"
#include "vk_mem_alloc.h"

using myengine::shared::GpuMaterial;

void MaterialRegistry::init(VulkanContext* ctx, ResourceFactory* resources) {
    if (!ctx || !resources) throw std::runtime_error("MaterialRegistry::init: invalid args");
    ctx_ = ctx;

    bufferSize_ = static_cast<VkDeviceSize>(MAX_MATERIALS) * sizeof(GpuMaterial);
    createBuffer(resources);

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

    std::cout << "[MaterialRegistry] init: capacity " << MAX_MATERIALS << " materials, "
              << (bufferSize_ / 1024) << " KB (BDA-only, no descriptors)\n";
}

void MaterialRegistry::createBuffer(ResourceFactory* resources) {
    resources->createBufferVMA(
        bufferSize_,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        buffer_, allocation_);

    VmaAllocationInfo allocInfo{};
    vmaGetAllocationInfo(ctx_->allocator(), allocation_, &allocInfo);
    mapped_ = allocInfo.pMappedData;
    if (!mapped_) {
        throw std::runtime_error("MaterialRegistry: VMA allocation is not persistently mapped");
    }

    VkBufferDeviceAddressInfo bai{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    bai.buffer = buffer_;
    address_ = vkGetBufferDeviceAddress(ctx_->device(), &bai);
    if (address_ == 0) {
        throw std::runtime_error("MaterialRegistry: vkGetBufferDeviceAddress returned 0");
    }
}

uint32_t MaterialRegistry::add(const std::string& name, const GpuMaterial& m) {
    auto it = nameToId_.find(name);
    if (it != nameToId_.end()) {
        cpuMaterials_[it->second] = m;  // update in place
        dirty_ = true;
        return it->second;
    }
    if (cpuMaterials_.size() >= MAX_MATERIALS) {
        std::cerr << "[MaterialRegistry] capacity (" << MAX_MATERIALS
                  << ") exceeded; returning default id for '" << name << "'\n";
        return kDefaultMaterialId;
    }
    uint32_t id = static_cast<uint32_t>(cpuMaterials_.size());
    cpuMaterials_.push_back(m);
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
    std::memcpy(mapped_, cpuMaterials_.data(), cpuMaterials_.size() * sizeof(GpuMaterial));
    dirty_ = false;
}

void MaterialRegistry::shutdown() {
    if (ctx_ && buffer_ != VK_NULL_HANDLE) {
        vmaDestroyBuffer(ctx_->allocator(), buffer_, allocation_);
        buffer_ = VK_NULL_HANDLE;
        allocation_ = nullptr;
    }
    mapped_ = nullptr;
    address_ = 0;
    cpuMaterials_.clear();
    nameToId_.clear();
}
