#pragma once
// =============================================================================
// material.h — Material リソース (テクスチャ + descriptor set)
// =============================================================================
// Material は 1 枚のテクスチャに対応する descriptor set を管理する。
// init() で descriptor pool から 1 set 確保し、 渡された texture を
// set=1, binding=0 として書き込む。
// =============================================================================

#include <vulkan/vulkan.h>

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

   private:
    VkDescriptorSet set_ = VK_NULL_HANDLE;
};
