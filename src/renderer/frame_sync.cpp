// \MyEngine\src\renderer\frame_sync.cpp
// =============================================================================
// frame_sync.cpp
// =============================================================================
// 実装内容:
//   フレーム同期 + コマンドバッファ管理。
//   2 フレーム同時進行 (MAX_FRAMES_IN_FLIGHT = 2) を許す前提で、
//   各フレームに 1 つずつ:
//     - commandBuffer
//     - imageAvailableSemaphore (acquire 結果を submit が待つ)
//     - renderFinishedSemaphore (submit 結果を present が待つ)
//     - inFlightFence (CPU 側でフレーム完了を待つ)
//   を持つ。
//
// 呼び出し側 (VulkanRenderer) の責務:
//   - acquireNextImage で取得した cmd に対して、 vkBeginCommandBuffer →
//     ... 描画コマンド記録 ... → vkEndCommandBuffer を行ってから
//     submitAndPresent を呼ぶ。
//   - needsRecreate=true が返ったら swapchain を作り直して early-return。
// =============================================================================
#include "renderer/frame_sync.h"

#include "renderer/vulkan_context.h"

#include <stdexcept>

// =============================================================================
// init / shutdown
// =============================================================================

void FrameSync::init(VulkanContext* ctx) {
    ctx_ = ctx;
    createCommandPool();
    createCommandBuffers();
    createSyncObjects();
}

void FrameSync::shutdown() {
    if (!ctx_) return;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (inFlightFences_[i] != VK_NULL_HANDLE) {
            vkDestroyFence(ctx_->device(), inFlightFences_[i], nullptr);
            inFlightFences_[i] = VK_NULL_HANDLE;
        }
        if (renderFinishedSemaphores_[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(ctx_->device(), renderFinishedSemaphores_[i], nullptr);
            renderFinishedSemaphores_[i] = VK_NULL_HANDLE;
        }
        if (imageAvailableSemaphores_[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(ctx_->device(), imageAvailableSemaphores_[i], nullptr);
            imageAvailableSemaphores_[i] = VK_NULL_HANDLE;
        }
    }

    if (commandPool_ != VK_NULL_HANDLE) {
        // commandBuffers はプール破棄で一緒に解放される
        vkDestroyCommandPool(ctx_->device(), commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
    }
    for (auto& cb : commandBuffers_) cb = VK_NULL_HANDLE;

    currentFrame_ = 0;
    ctx_ = nullptr;
}

// =============================================================================
// createCommandPool
// =============================================================================

void FrameSync::createCommandPool() {
    VkCommandPoolCreateInfo ci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    ci.queueFamilyIndex = ctx_->graphicsFamily();
    // RESET_COMMAND_BUFFER_BIT: 個別 cmd を vkResetCommandBuffer で reset 可能
    ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(ctx_->device(), &ci, nullptr, &commandPool_) != VK_SUCCESS) {
        throw std::runtime_error("FrameSync: vkCreateCommandPool failed");
    }
}

// =============================================================================
// createCommandBuffers
// =============================================================================

void FrameSync::createCommandBuffers() {
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = commandPool_;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    if (vkAllocateCommandBuffers(ctx_->device(), &ai, commandBuffers_.data()) != VK_SUCCESS) {
        throw std::runtime_error("FrameSync: vkAllocateCommandBuffers failed");
    }
}

// =============================================================================
// createSyncObjects
// =============================================================================

void FrameSync::createSyncObjects() {
    VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // 最初の fence wait が即通過するように

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(ctx_->device(), &si, nullptr, &imageAvailableSemaphores_[i]) !=
                VK_SUCCESS ||
            vkCreateSemaphore(ctx_->device(), &si, nullptr, &renderFinishedSemaphores_[i]) !=
                VK_SUCCESS ||
            vkCreateFence(ctx_->device(), &fi, nullptr, &inFlightFences_[i]) != VK_SUCCESS) {
            throw std::runtime_error("FrameSync: sync object creation failed");
        }
    }
}

// =============================================================================
// acquireNextImage
// =============================================================================

FrameSync::AcquireResult FrameSync::acquireNextImage(VkSwapchainKHR swapchain) {
    AcquireResult r{};
    r.frameIndex = currentFrame_;

    // 前フレームの GPU 完了を待つ
    vkWaitForFences(ctx_->device(), 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex = 0;
    VkResult res = vkAcquireNextImageKHR(ctx_->device(), swapchain, UINT64_MAX,
                                         imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE,
                                         &imageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        // swapchain 再生成が必要。 fence は reset しない (次フレームでも再利用)。
        r.needsRecreate = true;
        return r;
    }
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("FrameSync: vkAcquireNextImageKHR failed");
    }
    // SUBOPTIMAL は描画自体は可能なので続行 (submit 後にやり直す)

    // ここまで来たら確実にこのフレームを処理する → fence reset
    vkResetFences(ctx_->device(), 1, &inFlightFences_[currentFrame_]);

    // 既に書き込んだ可能性のあるコマンドを reset
    vkResetCommandBuffer(commandBuffers_[currentFrame_], 0);

    r.needsRecreate = false;
    r.imageIndex = imageIndex;
    r.cmd = commandBuffers_[currentFrame_];
    return r;
}

// =============================================================================
// submitAndPresent
// =============================================================================

bool FrameSync::submitAndPresent(VkSwapchainKHR swapchain, uint32_t imageIndex) {
    // ─── Submit: imageAvailable を待ち、 renderFinished を signal、 fence で完了通知 ───
    VkSemaphore waitSems[] = {imageAvailableSemaphores_[currentFrame_]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signalSems[] = {renderFinishedSemaphores_[currentFrame_]};

    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = waitSems;
    si.pWaitDstStageMask = waitStages;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &commandBuffers_[currentFrame_];
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = signalSems;

    if (vkQueueSubmit(ctx_->graphicsQueue(), 1, &si, inFlightFences_[currentFrame_]) !=
        VK_SUCCESS) {
        throw std::runtime_error("FrameSync: vkQueueSubmit failed");
    }

    // ─── Present: renderFinished を待ってから表示 ───
    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = signalSems;
    pi.swapchainCount = 1;
    pi.pSwapchains = &swapchain;
    pi.pImageIndices = &imageIndex;

    VkResult res = vkQueuePresentKHR(ctx_->presentQueue(), &pi);

    // 次フレームへ
    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;

    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        return true;  // swapchain 再生成が必要
    }
    if (res != VK_SUCCESS) {
        throw std::runtime_error("FrameSync: vkQueuePresentKHR failed");
    }
    return false;
}

// =============================================================================
// waitIdle
// =============================================================================

void FrameSync::waitIdle() {
    if (ctx_ && ctx_->device() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(ctx_->device());
    }
}
