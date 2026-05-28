// include/MyEngine/renderer/swapchain.h

#pragma once
// =============================================================================
// Swapchain — Step 3 で VulkanRenderer から切り出したクラス
// =============================================================================
// 責務:
//   - VkSwapchainKHR 本体の生成/破棄
//   - カラー用 VkImage / VkImageView（プレゼント対象）
//   - デプスバッファ（VkImage/Memory/View、サイズは swap と一致）
//   - リサイズ時の再生成（recreate）
//
// 責務外（意図的に含めない）:
//   - VkFramebuffer  … RenderPass に依存するため MainPass の責務
//   - カメラ行列/ビューポート状態  … Renderer が管理
//
// 設計メモ:
//   - VulkanContext / ResourceFactory はポインタで保持（所有しない）
//   - recreate() は内部で vkDeviceWaitIdle を呼ぶ。呼び出し側は描画中でないことを保証すること
//   - depthFormat は init 時に決定し、recreate でも維持する
// =============================================================================

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

#include "renderer/vk_unique.h"
#include "renderer/vma_image.h"

#include <cstdint>
#include <vector>

class VulkanContext;
class ResourceFactory;

class Swapchain {
   public:
    void init(const VulkanContext* ctx, const ResourceFactory* resources, SDL_Window* window,
              VkFormat depthFormat);
    void shutdown();

    // ウィンドウサイズが変わったとき呼ぶ。0x0 の間はブロックして待つ。
    void recreate();

    // ─── アクセサ ─────────────────────────────────────────────
    VkSwapchainKHR handle() const { return swapchain_.get(); }
    VkFormat colorFormat() const { return colorFormat_; }
    VkFormat depthFormat() const { return depthFormat_; }
    VkExtent2D extent() const { return extent_; }
    uint32_t imageCount() const { return static_cast<uint32_t>(images_.size()); }
    VkImageView colorView(uint32_t i) const { return views_[i].get(); }
    VkImageView depthView() const { return depthView_.get(); }
    // PART4 4a-2: HZB compute (4b) and the 4a-2 debug HUD need the raw VkImage
    // to issue the post-main_pass DEPTH_STENCIL_ATTACHMENT_OPTIMAL ->
    // DEPTH_STENCIL_READ_ONLY_OPTIMAL transition.
    VkImage depthImage() const { return depthImage_.image(); }

   private:
    const VulkanContext* ctx_ = nullptr;
    const ResourceFactory* resources_ = nullptr;
    SDL_Window* window_ = nullptr;

    VkUnique<VkSwapchainKHR> swapchain_;
    VkFormat colorFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D extent_ = {};
    std::vector<VkImage> images_;  // swapchain が所有（破棄不要）
    std::vector<VkUnique<VkImageView>> views_;

    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
    VmaImage depthImage_;  // VkImage + VmaAllocation (memory is now VMA-managed)
    VkUnique<VkImageView> depthView_;

    // ─── 内部ヘルパ ───────────────────────────────────────────
    void createSwapchain(VkSwapchainKHR oldSwapchain);
    void createImageViews();
    void createDepthResources();
    void destroyImageViewsAndSwapchain();
    void destroyDepthResources();
};