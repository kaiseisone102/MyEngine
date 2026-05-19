// \MyEngine\include\MyEngine\renderer\vulkan_context.h

#pragma once
// =============================================================================
// vulkan_context.h
// =============================================================================
// 担当:
//   Vulkan の「土台」となるオブジェクトを一括管理するクラス。
//     - VkInstance        : Vulkan API のエントリポイント
//     - VkSurfaceKHR      : SDL ウィンドウと Vulkan をつなぐサーフェス
//     - VkPhysicalDevice  : 物理 GPU（デバイススコアリングで自動選択）
//     - VkDevice          : 論理デバイス
//     - VkQueue × 2       : graphics / present キュー
//     - Queue family index, memory properties
//     - VkDebugUtilsMessengerEXT : デバッグビルド専用のバリデーション出力
//
// 方針:
//   - このクラスは「状態」を持つが「描画」は行わない
//   - Instance/Device が欲しい他クラス（Swapchain, ResourceFactory など）が
//     VulkanContext& を参照として受け取って使う
//
// 依存:
//   - SDL3  : SDL_Vulkan_GetInstanceExtensions / SDL_Vulkan_CreateSurface
//   - なし（Vulkan 本体以外）
// =============================================================================

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

// VMA (Vulkan Memory Allocator) forward declaration to avoid including vma in headers
VK_DEFINE_HANDLE(VmaAllocator)

#include <cstdint>

class VulkanContext {
   public:
    // ─── ライフサイクル ────────────────────────────────────────────
    // 順序:
    //   1. createInstance           Instance + Debug extension 有効化
    //   2. setupDebugMessenger      (#ifndef NDEBUG のみ)
    //   3. createSurface            window を元にサーフェスを作成
    //   4. pickPhysicalDevice       GPU スコアリング
    //   5. createDevice             論理デバイスとキューを作成
    void init(SDL_Window* window);
    void shutdown();

    // ─── アクセサ ──────────────────────────────────────────────────
    VkInstance instance() const { return instance_; }
    VkSurfaceKHR surface() const { return surface_; }
    VkPhysicalDevice physicalDevice() const { return physical_; }
    VkDevice device() const { return device_; }
    VkQueue graphicsQueue() const { return graphicsQueue_; }
    VkQueue presentQueue() const { return presentQueue_; }
    uint32_t graphicsFamily() const { return graphicsFamily_; }
    uint32_t presentFamily() const { return presentFamily_; }

    const VkPhysicalDeviceMemoryProperties& memoryProperties() const { return memoryProperties_; }
    VmaAllocator allocator() const { return allocator_; }

    // ─── ユーティリティ ────────────────────────────────────────────
    // GPU がサポートする最適な深度フォーマットを返す（D32_SFLOAT 優先）
    VkFormat findDepthFormat() const;

   private:
    SDL_Window* window_ = nullptr;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;

    uint32_t graphicsFamily_ = VK_QUEUE_FAMILY_IGNORED;
    uint32_t presentFamily_ = VK_QUEUE_FAMILY_IGNORED;

    VkPhysicalDeviceMemoryProperties memoryProperties_{};
    VmaAllocator allocator_ = VK_NULL_HANDLE;

    // デバッグビルドのみ有効。Release では VK_NULL_HANDLE のまま。
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;

    void createInstance();
    void setupDebugMessenger();
    void destroyDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createDevice();
};
