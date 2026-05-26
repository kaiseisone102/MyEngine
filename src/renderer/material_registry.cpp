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
    buffer_ = VmaBuffer::createMappedStorageBDA(ctx_, bufferSize_);
}

uint32_t MaterialRegistry::add(const std::string& name, const GpuMaterial& material) {
    auto it = nameToId_.find(name);
    if (it != nameToId_.end()) {
        cpuMaterials_[it->second] = material;  // update in place
        dirty_ = true;
        return it->second;
    }
    if (cpuMaterials_.size() >= MAX_MATERIALS) {
        std::cerr << "[MaterialRegistry] capacity (" << MAX_MATERIALS
                  << ") exceeded; returning default id for '" << name << "'\n";
        return kDefaultMaterialId;
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
}
