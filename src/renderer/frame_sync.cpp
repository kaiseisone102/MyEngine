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

void FrameSync::init(VulkanContext* ctx, uint32_t swapchainImageCount) {
    ctx_ = ctx;
    createCommandPool();
    createCommandBuffers();
    createSyncObjects(swapchainImageCount);
}

void FrameSync::shutdown() {
    if (!ctx_) return;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        imageAvailableSemaphores_[i].reset();
    }
    // Per-image present-wait semaphores (sized to swapchain image count).
    // VkUnique elements free their semaphores on clear.
    renderFinishedSemaphores_.clear();
    frameTimeline_.reset();
    nextSignalValue_ = 0;

    // commandBuffers are freed with the pool.
    commandPool_.reset();
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
    VkCommandPool pool = VK_NULL_HANDLE;
    if (vkCreateCommandPool(ctx_->device(), &ci, nullptr, &pool) != VK_SUCCESS) {
        throw std::runtime_error("FrameSync: vkCreateCommandPool failed");
    }
    commandPool_ = VkUnique<VkCommandPool>(ctx_->device(), pool);
}

// =============================================================================
// createCommandBuffers
// =============================================================================

void FrameSync::createCommandBuffers() {
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = commandPool_.get();
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    if (vkAllocateCommandBuffers(ctx_->device(), &ai, commandBuffers_.data()) != VK_SUCCESS) {
        throw std::runtime_error("FrameSync: vkAllocateCommandBuffers failed");
    }
}

// =============================================================================
// createSyncObjects
// =============================================================================

void FrameSync::createSyncObjects(uint32_t swapchainImageCount) {
    VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    // B: single timeline semaphore replaces the per-frame VkFence array.
    // initialValue = MAX_FRAMES_IN_FLIGHT so the first MAX_FRAMES_IN_FLIGHT
    // frames pass their CPU wait immediately (matching VK_FENCE_CREATE_
    // SIGNALED_BIT's effect on the old per-frame fence). nextSignalValue_
    // starts at MAX_FRAMES_IN_FLIGHT + 1 so frame 0 signals
    // MAX_FRAMES_IN_FLIGHT + 1 and the wait at frame 2 is for value
    // (nextSignalValue - MAX_FRAMES_IN_FLIGHT - 1) = 1, etc.
    {
        VkSemaphoreTypeCreateInfo typeInfo{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
        typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        typeInfo.initialValue = MAX_FRAMES_IN_FLIGHT;

        VkSemaphoreCreateInfo timelineSi{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        timelineSi.pNext = &typeInfo;

        VkSemaphore timelineSem = VK_NULL_HANDLE;
        if (vkCreateSemaphore(ctx_->device(), &timelineSi, nullptr, &timelineSem) !=
            VK_SUCCESS) {
            throw std::runtime_error("FrameSync: timeline semaphore creation failed");
        }
        frameTimeline_ = VkUnique<VkSemaphore>(ctx_->device(), timelineSem);
        nextSignalValue_ = MAX_FRAMES_IN_FLIGHT + 1;
    }

    // image-available semaphore stays binary -- vkAcquireNextImageKHR has no
    // timeline variant in core Vulkan.
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkSemaphore sem = VK_NULL_HANDLE;
        if (vkCreateSemaphore(ctx_->device(), &si, nullptr, &sem) != VK_SUCCESS) {
            throw std::runtime_error("FrameSync: imageAvailable semaphore creation failed");
        }
        imageAvailableSemaphores_[i] = VkUnique<VkSemaphore>(ctx_->device(), sem);
    }

    // render-finished (present-wait) semaphore is PER SWAPCHAIN IMAGE: it must not
    // be reused until its image is re-acquired (Vulkan-Guide swapchain_semaphore_reuse).
    // vkQueuePresentKHR also has no timeline variant, so this stays binary.
    renderFinishedSemaphores_.clear();
    renderFinishedSemaphores_.reserve(swapchainImageCount);
    for (uint32_t i = 0; i < swapchainImageCount; ++i) {
        VkSemaphore sem = VK_NULL_HANDLE;
        if (vkCreateSemaphore(ctx_->device(), &si, nullptr, &sem) != VK_SUCCESS) {
            throw std::runtime_error("FrameSync: render-finished semaphore creation failed");
        }
        renderFinishedSemaphores_.emplace_back(ctx_->device(), sem);
    }
}

// =============================================================================
// acquireNextImage
// =============================================================================

FrameSync::AcquireResult FrameSync::acquireNextImage(VkSwapchainKHR swapchain) {
    AcquireResult r{};
    r.frameIndex = currentFrame_;

    // B: wait on the timeline value MAX_FRAMES_IN_FLIGHT frames in the past
    // -- the same constraint the per-frame VkFence array used to enforce
    // (a frame's commandBuffer cannot be reset while the GPU is still
    // executing it). For frame N this is value (MAX_FRAMES_IN_FLIGHT + 1 + N - MAX_FRAMES_IN_FLIGHT)
    // = nextSignalValue_ - MAX_FRAMES_IN_FLIGHT, which is exactly the value
    // that the frame using this slot last signaled. For the first
    // MAX_FRAMES_IN_FLIGHT frames the waitValue is <= initial timeline
    // value (MAX_FRAMES_IN_FLIGHT) so the wait returns immediately.
    const uint64_t waitValue = nextSignalValue_ - MAX_FRAMES_IN_FLIGHT;
    VkSemaphore waitSem = frameTimeline_.get();
    VkSemaphoreWaitInfo wi{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
    wi.semaphoreCount = 1;
    wi.pSemaphores = &waitSem;
    wi.pValues = &waitValue;
    vkWaitSemaphores(ctx_->device(), &wi, UINT64_MAX);

    uint32_t imageIndex = 0;
    VkResult res = vkAcquireNextImageKHR(ctx_->device(), swapchain, UINT64_MAX,
                                         imageAvailableSemaphores_[currentFrame_].get(), VK_NULL_HANDLE,
                                         &imageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        r.needsRecreate = true;
        return r;
    }
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("FrameSync: vkAcquireNextImageKHR failed");
    }
    // SUBOPTIMAL は描画自体は可能なので続行 (submit 後にやり直す)

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
    // ─── Submit: imageAvailable を待ち、 renderFinished + timeline を signal ───
    VkSemaphore waitSems[] = {imageAvailableSemaphores_[currentFrame_].get()};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    // Index by the ACQUIRED IMAGE, not the in-flight frame, so a present-wait
    // semaphore is never reused while a previous present of the same image
    // is still pending.
    // B: two signal semaphores -- binary renderFinished[imageIndex] for the
    // present, plus timeline frameTimeline_ at value nextSignalValue_ so the
    // next acquireNextImage's CPU wait knows when this frame is done.
    VkSemaphore signalSems[] = {
        renderFinishedSemaphores_[imageIndex].get(),
        frameTimeline_.get(),
    };
    const uint64_t signalValue = nextSignalValue_;

    // VkTimelineSemaphoreSubmitInfo: per-semaphore values (binary semaphores
    // accept 0). waitSemaphoreValues must match waitSemaphoreCount even when
    // every wait semaphore is binary.
    const uint64_t waitValues[] = {0};
    const uint64_t signalValues[] = {0, signalValue};
    VkTimelineSemaphoreSubmitInfo ti{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    ti.waitSemaphoreValueCount = 1;
    ti.pWaitSemaphoreValues = waitValues;
    ti.signalSemaphoreValueCount = 2;
    ti.pSignalSemaphoreValues = signalValues;

    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.pNext = &ti;
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = waitSems;
    si.pWaitDstStageMask = waitStages;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &commandBuffers_[currentFrame_];
    si.signalSemaphoreCount = 2;
    si.pSignalSemaphores = signalSems;

    // B: no fence at the queue level -- the timeline signal in the submit
    // info covers what the old VkFence used to.
    if (vkQueueSubmit(ctx_->graphicsQueue(), 1, &si, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("FrameSync: vkQueueSubmit failed");
    }

    // ─── Present: renderFinished を待ってから表示 ───
    // Only the binary render-finished semaphore is part of the present wait;
    // the timeline value is for CPU consumption.
    VkSemaphore presentWaitSems[] = {renderFinishedSemaphores_[imageIndex].get()};
    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = presentWaitSems;
    pi.swapchainCount = 1;
    pi.pSwapchains = &swapchain;
    pi.pImageIndices = &imageIndex;

    VkResult res = vkQueuePresentKHR(ctx_->presentQueue(), &pi);

    // 次フレームへ + timeline 値を進める
    ++nextSignalValue_;
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
