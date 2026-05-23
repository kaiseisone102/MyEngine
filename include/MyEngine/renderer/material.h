#pragma once
// =============================================================================
// material.h — Material リソース (テクスチャ + descriptor set)
// =============================================================================
// Material は 1 枚のテクスチャに対応する descriptor set を管理する。
// init() で descriptor pool から 1 set 確保し、 渡された texture を
// set=1, binding=0 として書き込む。
// =============================================================================

#include <vulkan/vulkan.h>
#include <cstdint>

class VulkanContext;
class Texture;

class Material {
   public:
    void init(const VulkanContext* ctx,
              VkDescriptorPool pool,
              VkDescriptorSetLayout layout,
              const Texture* texture);

    // descriptor set は pool 破棄で自動解放されるため、 ここでは何もしない。
    // ただし、 asset_registry/model からの呼び出し互換性のため公開しておく。
    void destroy() { set_ = VK_NULL_HANDLE; }

    VkDescriptorSet descriptorSet() const { return set_; }

    // === Phase 1D: bindless texture index ===
    uint32_t bindlessIndex() const { return bindlessIndex_; }
    void setBindlessIndex(uint32_t idx) { bindlessIndex_ = idx; }
    // Phase 1K-2 S4-d: slot in the global MaterialRegistry SSBO
    uint32_t materialId() const { return materialId_; }
    void setMaterialId(uint32_t id) { materialId_ = id; }

   private:
    VkDescriptorSet set_ = VK_NULL_HANDLE;
    uint32_t bindlessIndex_ = UINT32_MAX;
    uint32_t materialId_ = 0;  // 0 = default material until registered
};
