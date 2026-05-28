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

    // Phase 2B PART3c-2: GPU-driven indirect-draw capabilities. Queried at device
    // creation; enabled only if the physical device supports them (else false).
    bool multiDrawIndirect() const { return multiDrawIndirect_; }
    bool drawIndirectFirstInstance() const { return drawIndirectFirstInstance_; }

    // Vulkan13 §1 (W): VK_KHR_synchronization2 (core in Vulkan 1.3). Queried at
    // device creation; the barrier helper (renderer/barrier.h) falls back to the
    // legacy vkCmdPipelineBarrier path when this is false. Future PART4 barriers
    // (HZB mips, two-pass cull, async-compute QFOT) all go through that helper.
    bool synchronization2() const { return synchronization2_; }

    // PART4 4-前-4: VK_KHR_draw_indirect_count (core in Vulkan 1.2). Used by
    // indirect_exec to call vkCmdDrawIndexedIndirectCount instead of walking
    // all command slots. CullingPass's scan_compact writes a real visible-count
    // into countBuf; with this feature the GPU only walks that many entries
    // (the empty-cmd-full-walk problem at scale is gone).
    bool drawIndirectCount() const { return drawIndirectCount_; }

    // PART4 4-前-4: VK_EXT_device_generated_commands. Receptacle today; the
    // indirect_exec wrapper has a DGC code path that 4d will fill in.
    bool deviceGeneratedCommands() const { return deviceGeneratedCommands_; }

    // PART4 4a-1: VK_KHR_dynamic_rendering (core in Vulkan 1.3). Required by
    // the MRT prepass (4a-2) and is the modern replacement for VkRenderPass +
    // VkFramebuffer. Pascal-and-newer NVIDIA + Mesa NVK all expose Vulkan 1.3,
    // so we expect this to always be true on supported hardware; the bit lives
    // here so the receptacle (4a-1) is symmetric with sync2 / drawIndirectCount.
    bool dynamicRendering() const { return dynamicRendering_; }

    // PART4 4a-2: Vulkan 1.2 separate depth/stencil layouts. Lets D32_SFLOAT
    // use the precise VK_IMAGE_LAYOUT_DEPTH_*_OPTIMAL forms instead of the
    // legacy combined DEPTH_STENCIL_*_OPTIMAL. Querying here so device
    // creation only enables it when present.
    bool separateDepthStencilLayouts() const { return separateDepthStencilLayouts_; }

    // PART4 4b: GL_KHR_shader_subgroup_basic + GL_KHR_shader_subgroup_shuffle
    // availability for the SPD-style Hi-Z compute pass. true = wave-ops fast
    // path enabled (uses subgroupShuffleXor for Phase C of hiz_spd_wave.comp),
    // false = LDS-only fallback (hiz_spd.comp) selected at pipeline creation.
    // Queried via VkPhysicalDeviceSubgroupProperties (Vulkan 1.1 core).
    // Pascal+ NVIDIA, GCN+ AMD, Skylake+ Intel all expose basic + shuffle in
    // COMPUTE; mobile drivers may not. Pair with subgroupSize() >= 32 before
    // selecting the wave path (see HiZPass::createPipeline).
    bool subgroupOps() const { return subgroupOps_; }
    uint32_t subgroupSize() const { return subgroupSize_; }

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

    // Phase 2B PART3c-2: queried indirect-draw capability flags.
    bool multiDrawIndirect_ = false;
    bool drawIndirectFirstInstance_ = false;

    // Vulkan13 §1 (W): queried sync2 capability flag.
    bool synchronization2_ = false;

    // PART4 4-前-4: queried indirect-count / DGC capability flags.
    bool drawIndirectCount_ = false;
    bool deviceGeneratedCommands_ = false;

    // PART4 4a-1: queried VK_KHR_dynamic_rendering capability flag.
    bool dynamicRendering_ = false;

    // PART4 4a-2: queried Vulkan 1.2 separate depth/stencil layouts feature.
    // Some Pascal/Maxwell drivers expose Vulkan 1.2 without this optional
    // feature, so enabling it unconditionally on the device CreateInfo would
    // make vkCreateDevice return VK_ERROR_FEATURE_NOT_PRESENT.
    bool separateDepthStencilLayouts_ = false;

    // PART4 4b: queried subgroup-ops capability (basic + shuffle in COMPUTE)
    // and the subgroup size. HiZPass picks the wave-ops pipeline variant
    // when subgroupOps_ is true AND subgroupSize_ >= 32, and the LDS-only
    // variant otherwise.
    bool subgroupOps_ = false;
    uint32_t subgroupSize_ = 0;

    // デバッグビルドのみ有効。Release では VK_NULL_HANDLE のまま。
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;

    void createInstance();
    void setupDebugMessenger();
    void destroyDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createDevice();
};
