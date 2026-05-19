#pragma once
// =============================================================================
// reflection_target.h — 反射描画用の color + depth ターゲット
// =============================================================================
// 反射シーンを描く先のテクスチャ。 ShadowPass の depth-only RenderTarget と違って
// color + depth 両方が必要。
//
// color image: shader から sample されるので createSampler=true、 usage に SAMPLED 含む
// depth image: 描画中の depth test 用、 sample は不要
//
// 解像度は init 時に指定。 品質変更で再作成する場合は shutdown → init を呼ぶ。
// =============================================================================

#include <vulkan/vulkan.h>

#include "renderer/render_target.h"

class VulkanContext;
class ResourceFactory;

class ReflectionTarget {
   public:
    void init(VulkanContext* ctx, ResourceFactory* resources, uint32_t width, uint32_t height,
              VkFormat colorFormat, VkFormat depthFormat);
    void shutdown();

    const RenderTarget& color() const { return color_; }
    const RenderTarget& depth() const { return depth_; }

    VkExtent2D extent() const { return extent_; }
    bool valid() const { return color_.image() != VK_NULL_HANDLE; }

   private:
    RenderTarget color_;
    RenderTarget depth_;
    VkExtent2D extent_{};
};
