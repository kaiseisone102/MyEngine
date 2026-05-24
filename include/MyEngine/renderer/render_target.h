// include/MyEngine/renderer/render_target.h
#pragma once
// =============================================================================
// render_target.h — リファクタ Step 8
//   描画の中間出力 (Image + Memory + View + 任意の Sampler) を束ねた RAII。
//   モダンエンジンでは Pass の出力を「型として扱う」のが標準で、
//   それにより Pass 間の接続が明示的になり、Bloom / G-Buffer / SSAO 等の
//   追加 Pass を導入してもインターフェースが揺らがない。
//
//   所有関係:
//     - Image / DeviceMemory / ImageView: 常に所有
//     - Sampler: 設定で要求された場合のみ所有
//     - VkExtent2D / VkFormat / VkImageAspectFlags: 値として保持 (バインド時に使う)
// =============================================================================

#include <vulkan/vulkan.h>
#include "renderer/vk_unique.h"

class VulkanContext;
class ResourceFactory;

class RenderTarget {
   public:
    struct Desc {
        uint32_t width = 0;
        uint32_t height = 0;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkImageUsageFlags usage = 0;
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        bool createSampler = false;

        // Sampler 設定 (createSampler=true のときのみ参照)。
        VkFilter samplerFilter = VK_FILTER_LINEAR;
        VkSamplerAddressMode samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VkBorderColor samplerBorderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    };

    void init(VulkanContext* ctx, ResourceFactory* resources, const Desc& desc);
    void shutdown();

    VkImage image() const { return image_.get(); }
    VkImageView view() const { return view_.get(); }
    VkSampler sampler() const { return sampler_.get(); }
    VkFormat format() const { return desc_.format; }
    VkExtent2D extent() const { return {desc_.width, desc_.height}; }
    VkImageAspectFlags aspect() const { return desc_.aspect; }

   private:
    VulkanContext* ctx_ = nullptr;
    Desc desc_{};

    VkUnique<VkImage> image_;
    VkUnique<VkDeviceMemory> memory_;
    VkUnique<VkImageView> view_;
    VkUnique<VkSampler> sampler_;
};
