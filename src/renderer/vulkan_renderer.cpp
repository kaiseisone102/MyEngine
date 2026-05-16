// src/renderer/vulkan_renderer.cpp
// =============================================================================
// vulkan_renderer.cpp — リファクタ Step 10 (案C 徹底分離後)
//
// drawFrame は acquire → frameUniforms.update → passChain.recordFrame
// → submitAndPresent の 4 ステップのみ。
// 描画ロジック・行列計算・Pass 順序はすべて SceneData / PassChain に委譲済み。
// =============================================================================

#include "renderer/vulkan_renderer.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <stdexcept>

void VulkanRenderer::init(SDL_Window* window) {
    window_ = window;
    const char* base = SDL_GetBasePath();
    if (!base) throw std::runtime_error("SDL_GetBasePath failed");
    shaderDir_ = std::string(base) + "shaders/";
    assetDir_ = std::string(base) + "assets/";

    // [Step 1-3] コア
    ctx_.init(window);
    resources_.init(&ctx_);
    swapchain_.init(&ctx_, &resources_, window, ctx_.findDepthFormat());

    // [Step 5] FrameSync
    frameSync_.init(&ctx_);

    // [Step 4 → AssetRegistry] Mesh / Texture
    assets_.init(&ctx_, &resources_, assetDir_);

    // [Step 6] FrameUniforms (Layout を先に確保。PassChain が layout を要求するため)
    frameUniforms_.init(&ctx_, &resources_);

    // [Step 8-9, 7] PassChain (ShadowPass + MainPass + ImGuiLayer + 配線)
    {
        PassChain::InitInfo info{};
        info.window = window_;
        info.ctx = &ctx_;
        info.resources = &resources_;
        info.swapchain = &swapchain_;
        info.frameUniforms = &frameUniforms_;
        info.defaultTexture = &assets_.defaultTexture();
        info.shaderDir = shaderDir_;
        passChain_.init(info);
    }
}

void VulkanRenderer::recreateSwapchain() {
    swapchain_.recreate();
    passChain_.onSwapchainResized();
}

void VulkanRenderer::onResize() {
    if (ctx_.device() != VK_NULL_HANDLE) recreateSwapchain();
}

void VulkanRenderer::drawFrame(std::function<void()> uiCallback) {
    auto acq = frameSync_.acquireNextImage(swapchain_.handle());
    if (acq.needsRecreate) {
        recreateSwapchain();
        return;
    }

    // 1) UBO 更新 (シーン情報 → LightingData は SceneData が組む)
    frameUniforms_.update(acq.frameIndex, scene_.toLightingData());

    // 2) ImGui フレーム + ユーザコールバック
    passChain_.beginUI();
    if (uiCallback) uiCallback();
    passChain_.endUI();

    // 3) コマンド記録 (ShadowPass → MainPass を内部で順に発行)
    {
        PassChain::RecordInfo info{};
        info.cmd = acq.cmd;
        info.imageIndex = acq.imageIndex;
        info.frameIndex = acq.frameIndex;
        info.scene = &scene_;
        info.assets = &assets_;
        info.frameUniforms = &frameUniforms_;
        passChain_.recordFrame(info);
    }

    // 4) 提出 + Present
    if (frameSync_.submitAndPresent(swapchain_.handle(), acq.imageIndex)) {
        recreateSwapchain();
    }
}

void VulkanRenderer::shutdown() {
    if (ctx_.device() == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(ctx_.device());

    // init と逆順:
    passChain_.shutdown();      // [Step 7,8,9]
    frameUniforms_.shutdown();  // [Step 6]
    assets_.shutdown();         // [Step 4]
    frameSync_.shutdown();      // [Step 5]
    swapchain_.shutdown();      // [Step 3]
    resources_.shutdown();      // [Step 2]
    ctx_.shutdown();            // [Step 1]
}
