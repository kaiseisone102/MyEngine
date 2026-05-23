// include/MyEngine/renderer/frame_sync.h
#pragma once
// =============================================================================
// frame_sync.h — リファクタ Step 5
//   フレーム同期 + コマンドバッファ管理を VulkanRenderer から分離。
//
//   将来 Step 8/9 で各 Pass を切り出した際に、FrameContext を介して
//   Pass にフレーム情報を渡す土台となる。今は VulkanRenderer から
//   acquire/submit を呼ぶだけのシンプルな API。
// =============================================================================

#include <vulkan/vulkan.h>

#include <array>
#include <vector>
#include <cstdint>

class VulkanContext;

class FrameSync {
   public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    // フレーム取得の結果。
    //   needsRecreate=true のとき、他のフィールドは未定義。
    //   呼び出し側で swapchain を作り直して early-return すること。
    struct AcquireResult {
        bool needsRecreate;
        uint32_t imageIndex;
        VkCommandBuffer cmd;      // このフレーム用 (まだ Begin されていない)
        uint32_t frameIndex;      // 0..MAX_FRAMES_IN_FLIGHT-1
    };

    void init(VulkanContext* ctx, uint32_t swapchainImageCount);
    void shutdown();

    // fence 待ち -> vkAcquireNextImageKHR -> fence reset -> vkResetCommandBuffer。
    // OUT_OF_DATE の場合は needsRecreate=true を返し、fence は reset しない
    // (= 次フレームの待ちで再利用される)。
    AcquireResult acquireNextImage(VkSwapchainKHR swapchain);

    // submit (image acquire を待ち、render finished をシグナル) -> present。
    // 戻り値が true のときは swapchain の作り直しが必要 (OUT_OF_DATE / SUBOPTIMAL)。
    // submit/present 後、currentFrame_ をインクリメント。
    bool submitAndPresent(VkSwapchainKHR swapchain, uint32_t imageIndex);

    // shutdown 前に vkDeviceWaitIdle 相当を呼びたいときに使う薄いラッパー。
    void waitIdle();

    uint32_t currentFrameIndex() const { return currentFrame_; }

    // ImGui_ImplVulkan_InitInfo.ImageCount などで使うわけではないが、
    // 将来 Pass から「現在のフレームの cmd」が欲しい時に使えるようにしておく。
    VkCommandBuffer currentCommandBuffer() const { return commandBuffers_[currentFrame_]; }

   private:
    VulkanContext* ctx_ = nullptr;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;

    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers_{};
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores_{};
    // Per-SWAPCHAIN-IMAGE (not per-frame): a present-wait semaphore signaled by
    // submit must not be reused until that image is re-acquired. Indexed by the
    // acquired image index. See Vulkan-Guide swapchain_semaphore_reuse.
    std::vector<VkSemaphore> renderFinishedSemaphores_{};
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences_{};
    uint32_t currentFrame_ = 0;

    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects(uint32_t swapchainImageCount);
};
