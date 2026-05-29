// include/MyEngine/renderer/bindless_texture_registry.h
#pragma once
// =============================================================================
// BindlessTextureRegistry - Phase 1D: descriptor indexing for textures
// =============================================================================
// One large descriptor set holding up to MAX_TEXTURES combined image samplers.
// Shaders access textures by integer index instead of binding a specific
// descriptor set per material. The index is passed via push constant.
//
// Design follows the modern Vulkan "bindless" pattern with these features
// (all already enabled in VulkanContext device creation):
//   - VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
//     -> not every slot has to be filled
//   - VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
//     -> can register new textures even after binding the set
//   - VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT
//     -> can overwrite a slot while another (unused) draw is in flight
//   - shaderSampledImageArrayNonUniformIndexing
//     -> shader can use nonuniformEXT(idx) for divergent indexing
//
// Coexistence with legacy material sets:
//   - Existing AssetRegistry materialPool/materialSetLayout stay intact.
//   - This registry adds a SECOND descriptor set (set index typically = 3)
//     used by NEW bindless-aware shaders.
//   - Old shaders keep using set=1 material sets, unchanged.
//
// Usage:
//   uint32_t idx = bindless.registerTexture(tex.view(), tex.sampler());
//   // Pass idx via push constant. In shader:
//   //   layout(set=3, binding=0) uniform sampler2D bindless[];
//   //   vec4 c = texture(bindless[nonuniformEXT(push.albedoIdx)], uv);
// =============================================================================
#include <vulkan/vulkan.h>

#include "renderer/vk_unique.h"

#include <cstdint>
#include <vector>

class VulkanContext;

class BindlessTextureRegistry {
   public:
    // Bindless descriptor array size. The VkDescriptorPool's descriptorCount
    // is fixed at create time, so this acts as a hard ceiling on live
    // textures -- but releaseTexture()/free-list reuse keeps the live count
    // bounded by the working set (loaded textures) rather than the lifetime
    // count (textures ever loaded). Growth past MAX_TEXTURES requires
    // recreating pool + set + descriptor writes; that's a separate Phase
    // (G+: pool grow), tracked in Foundations \xc2\xa78.1.
    static constexpr uint32_t MAX_TEXTURES = 1024;
    static constexpr uint32_t BINDING_SLOT = 0;

    void init(VulkanContext* ctx);
    void shutdown();

    /// Register a texture, returns its bindless index. Reuses an index from
    /// the free-list when one is available; otherwise advances nextIndex_.
    /// Returns UINT32_MAX on failure (out of slots and free-list empty).
    uint32_t registerTexture(VkImageView view, VkSampler sampler);

    /// Free a bindless index for reuse. The descriptor stays bound (the
    /// shader must not sample from it once the underlying view/sampler is
    /// destroyed); the next registerTexture call may overwrite the slot.
    /// PARTIALLY_BOUND_BIT lets the empty-but-allocated state remain valid.
    void releaseTexture(uint32_t index);

    /// Overwrite the descriptor at an existing index (e.g., for streaming).
    void updateTexture(uint32_t index, VkImageView view, VkSampler sampler);

    VkDescriptorSetLayout layout() const { return layout_.get(); }
    VkDescriptorSet descriptorSet() const { return set_; }

    /// Total textures alive right now (nextIndex_ - freeSlots_.size()).
    uint32_t count() const {
        return nextIndex_ - static_cast<uint32_t>(freeSlots_.size());
    }
    uint32_t capacity() const { return MAX_TEXTURES; }
    bool initialized() const { return set_ != VK_NULL_HANDLE; }

   private:
    VulkanContext* ctx_ = nullptr;
    VkUnique<VkDescriptorSetLayout> layout_;
    VkUnique<VkDescriptorPool> pool_;
    VkDescriptorSet set_ = VK_NULL_HANDLE;
    uint32_t nextIndex_ = 0;
    std::vector<uint32_t> freeSlots_;  // G: released indices, reused before nextIndex_

    void writeDescriptor(uint32_t index, VkImageView view, VkSampler sampler);
};
