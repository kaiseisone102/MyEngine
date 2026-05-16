// include/MyEngine/renderer/resource_factory.h

#pragma once
// =============================================================================
// ResourceFactory — Step 2 で VulkanRenderer から切り出したクラス
// =============================================================================
// 責務:
//   - VkBuffer / VkImage の作成とメモリ確保
//   - VkDeviceMemory の適切な memoryTypeIndex 選定（findMemoryType）
//   - ワンタイムコマンド発行（転送・レイアウト遷移）
//
// 設計メモ:
//   - 自前の「転送用コマンドプール」(TRANSIENT_BIT) を持つため、
//     Renderer 側の描画用コマンドプールとは独立している。
//   - VulkanContext をポインタで持つ（所有しない）。寿命は呼び出し側で管理。
//   - init() / shutdown() は必ずペアで呼ぶこと。
// =============================================================================

#include <vulkan/vulkan.h>

#include "vulkan_context.h"

class ResourceFactory {
   public:
    // ctx は init〜shutdown の間、有効であること
    void init(const VulkanContext* ctx);
    void shutdown();

    // ─── バッファ作成 ─────────────────────────────────────────────
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& bufferMemory) const;

    // ─── 画像作成 ─────────────────────────────────────────────────
    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                     VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image,
                     VkDeviceMemory& imageMemory) const;

    // ─── メモリタイプ検索 ────────────────────────────────────────
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    // ─── 転送系 ──────────────────────────────────────────────────
    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) const;
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) const;
    // color aspect 限定。深度画像の遷移が必要になったら aspect 引数を追加する。
    void transitionImageLayout(VkImage image, VkImageLayout oldLayout,
                               VkImageLayout newLayout) const;

   private:
    const VulkanContext* ctx_ = nullptr;
    VkCommandPool transientPool_ = VK_NULL_HANDLE;

    VkCommandBuffer beginOneTimeCommands() const;
    void endOneTimeCommands(VkCommandBuffer cmd) const;
};