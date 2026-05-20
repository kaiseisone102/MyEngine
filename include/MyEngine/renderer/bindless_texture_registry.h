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

#include <cstdint>

class VulkanContext;

class BindlessTextureRegistry {
   public:
    static constexpr uint32_t MAX_TEXTURES = 1024;
    static constexpr uint32_t BINDING_SLOT = 0;

    void init(VulkanContext* ctx);
    void shutdown();

    /// Register a texture, returns its bindless index.
    /// Returns UINT32_MAX on failure (e.g., out of slots).
    uint32_t registerTexture(VkImageView view, VkSampler sampler);

    /// Overwrite the descriptor at an existing index (e.g., for streaming).
    void updateTexture(uint32_t index, VkImageView view, VkSampler sampler);

    VkDescriptorSetLayout layout() const { return layout_; }
    VkDescriptorSet descriptorSet() const { return set_; }

    uint32_t count() const { return nextIndex_; }
    bool initialized() const { return set_ != VK_NULL_HANDLE; }

   private:
    VulkanContext* ctx_ = nullptr;
    VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
    VkDescriptorPool pool_ = VK_NULL_HANDLE;
    VkDescriptorSet set_ = VK_NULL_HANDLE;
    uint32_t nextIndex_ = 0;

    void writeDescriptor(uint32_t index, VkImageView view, VkSampler sampler);
};
