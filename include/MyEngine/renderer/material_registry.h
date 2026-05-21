// include/MyEngine/renderer/material_registry.h
#pragma once
// =============================================================================
// material_registry.h - Phase 1K-2: unified PBR material storage (SSBO + BDA)
// =============================================================================
// Holds an array of GpuMaterial in a single SSBO, accessed by the shaders via
// a buffer device address (BDA) and indexed by materialId. Mirrors the modern
// BDA style of InstanceBufferPool / SkinBufferPool:
//   - No descriptor set / pool / layout. Pure buffer_reference access.
//   - VMA buffer with SHADER_DEVICE_ADDRESS_BIT, persistently mapped.
//   - bufferAddress() is the sole way to expose the buffer.
//
// Unlike InstanceBufferPool (rebuilt every frame), materials change rarely, so
// this is a SINGLE buffer (not per-frame) uploaded only when dirty. add() hands
// out a stable materialId; getId(name) resolves a name to its id (0 = default).
// =============================================================================
#include <vulkan/vulkan.h>

// VMA forward declaration
VK_DEFINE_HANDLE(VmaAllocation)

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "shaders/shared/types.h"

class VulkanContext;
class ResourceFactory;

class MaterialRegistry {
   public:
    static constexpr uint32_t MAX_MATERIALS = 256;
    static constexpr uint32_t kDefaultMaterialId = 0;

    void init(VulkanContext* ctx, ResourceFactory* resources);
    void shutdown();

    // Register a material under a name, returning its stable id. If the name
    // already exists, updates it in place and returns the existing id.
    uint32_t add(const std::string& name, const myengine::shared::GpuMaterial& m);

    // Resolve a name to its id; returns kDefaultMaterialId if not found.
    uint32_t getId(const std::string& name) const;

    // Upload the CPU-side array to the GPU buffer (only does work if dirty).
    void upload();

    // BDA: shaders cast this address to a typed pointer (GpuMaterial[]).
    VkDeviceAddress bufferAddress() const { return address_; }

    uint32_t count() const { return static_cast<uint32_t>(cpuMaterials_.size()); }

   private:
    VulkanContext* ctx_ = nullptr;
    VkDeviceSize bufferSize_ = 0;

    std::vector<myengine::shared::GpuMaterial> cpuMaterials_;
    std::unordered_map<std::string, uint32_t> nameToId_;
    bool dirty_ = false;

    VkBuffer buffer_ = VK_NULL_HANDLE;
    VmaAllocation allocation_ = nullptr;
    VkDeviceAddress address_ = 0;
    void* mapped_ = nullptr;

    void createBuffer(ResourceFactory* resources);
};
