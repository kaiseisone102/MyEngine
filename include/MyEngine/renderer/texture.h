// include/MyEngine/renderer/texture.h

#pragma once
// =============================================================================
// Texture — Step 4 で VulkanRenderer から切り出したクラス
// =============================================================================
// 責務:
//   - VkImage / VkDeviceMemory / VkImageView / VkSampler 一式の管理
//   - stb_image による PNG ロード
//   - ロード失敗時のフォールバック（チェッカーボード生成）
//   - ステージング経由の GPU アップロード
//
// 設計メモ:
//   - フォーマットは VK_FORMAT_R8G8B8A8_SRGB 固定（将来拡張時に引数化）
//   - サンプラ設定もデフォルト固定（REPEAT / LINEAR / アニソ無効）
// =============================================================================

#include <vulkan/vulkan.h>

#include <string>

class VulkanContext;
class ResourceFactory;

class Texture {
   public:
    // path のファイルが存在すれば読み込み、なければチェッカーボードを生成する。
    void loadFromFileOrCheckerboard(const VulkanContext* ctx, const ResourceFactory* resources,
                                    const std::string& path);
    void destroy();

    VkImageView view() const { return view_; }
    VkSampler sampler() const { return sampler_; }

   private:
    const VulkanContext* ctx_ = nullptr;

    VkImage image_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    VkImageView view_ = VK_NULL_HANDLE;
    VkSampler sampler_ = VK_NULL_HANDLE;

    void createImageAndView(const ResourceFactory* resources, const uint8_t* pixels, int width,
                            int height);
    void createSampler();
};