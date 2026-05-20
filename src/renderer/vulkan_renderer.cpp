// src/renderer/vulkan_renderer.cpp
// =============================================================================
// + ParticleSystem 連携 (C 案: setCurrentParticles で保持して drawFrame で使う)
// + Phase 1C: scene_.toLightingData() 廃止 → currentLighting_ を直接使う
//            recordFrame に normalLighting / waterTime / reflectShadows を渡す
// =============================================================================

#include "renderer/vulkan_renderer.h"
#include <cmath>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <stdexcept>

void VulkanRenderer::init(SDL_Window* window) {
    window_ = window;
    const char* base = SDL_GetBasePath();
    if (!base) throw std::runtime_error("SDL_GetBasePath failed");
    shaderDir_ = std::string(base) + "shaders/";
    assetDir_ = std::string(base) + "assets/";

    ctx_.init(window);
    resources_.init(&ctx_);
    swapchain_.init(&ctx_, &resources_, window, ctx_.findDepthFormat());

    frameSync_.init(&ctx_);

    // Phase 1D: bindless must be initialized BEFORE assets so textures can be registered
    bindlessTextures_.init(&ctx_);
    assets_.init(&ctx_, &resources_, assetDir_, &bindlessTextures_);

    frameUniforms_.init(&ctx_, &resources_);

    skinBufferPool_.init(&ctx_, &resources_);

    {
        PassChain::InitInfo info{};
        info.window = window_;
        info.ctx = &ctx_;
        info.resources = &resources_;
        info.swapchain = &swapchain_;
        info.frameUniforms = &frameUniforms_;
        info.assets = &assets_;
        info.bindlessSetLayout = bindlessTextures_.layout();
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

    // Phase 1C: 旧 scene_.toLightingData() を廃止。 camera_system が
    // setLighting() で渡した currentLighting_ をそのまま使う。
    // === Phase 1C: time tracking ===
    const uint64_t nowTicks = SDL_GetTicks();
    if (startTickMs_ == 0) startTickMs_ = nowTicks;
    const float dt = (lastTickMs_ > 0) ? static_cast<float>(nowTicks - lastTickMs_) / 1000.f : 0.f;
    lastTickMs_ = nowTicks;
    elapsedTime_ = static_cast<float>(nowTicks - startTickMs_) / 1000.f;
    ++frameNumber_;

    // === Phase 1C: extend LightingUBO with time / screenSize / cameraParams ===
    auto lighting = currentLighting_;
    const VkExtent2D extP1C = swapchain_.extent();
    const float fw = static_cast<float>(extP1C.width);
    const float fh = static_cast<float>(extP1C.height);
    const float invW = (fw > 0.f) ? 1.f / fw : 0.f;
    const float invH = (fh > 0.f) ? 1.f / fh : 0.f;
    lighting.time = glm::vec4(elapsedTime_, dt, static_cast<float>(frameNumber_), std::sin(elapsedTime_));
    lighting.screenSize = glm::vec4(fw, fh, invW, invH);
    lighting.jitter = glm::vec4(0.f);
    lighting.cameraParams = glm::vec4(0.1f, 200.f, glm::radians(45.f), (fh > 0.f) ? fw / fh : 1.f);

    frameUniforms_.update(acq.frameIndex, lighting);

    passChain_.beginUI();
    if (uiCallback) uiCallback();
    passChain_.endUI();

    {
        PassChain::RecordInfo info{};
        info.cmd = acq.cmd;
        info.imageIndex = acq.imageIndex;
        info.frameIndex = acq.frameIndex;
        info.scene = &scene_;
        info.assets = &assets_;
        info.frameUniforms = &frameUniforms_;
        info.skinAddress = skinBufferPool_.bufferAddress(acq.frameIndex);
        info.bindlessSet = bindlessTextures_.descriptorSet();
        info.debugLines = &debugLines_;
        info.particles = currentParticles_;
        info.hud = &hud_;
        const VkExtent2D ext = swapchain_.extent();
        info.screenW = static_cast<float>(ext.width);
        info.screenH = static_cast<float>(ext.height);

        // Phase 1C: 反射 VP の生成と shadowStrength 調整に必要な追加情報。
        info.normalLighting = currentLighting_;
        info.waterTime = waterTime_;
        info.reflectShadows = reflectShadows_;

        passChain_.recordFrame(info);
    }

    // フレーム終わったら particles ポインタはクリア。
    // 次フレームの buildScene で setter が再度呼ばれる想定。
    // Layer が遷移したり Title 画面のように setter を呼ばない場合に
    // 古い (もう無効な) ポインタを使わないための安全策。
    currentParticles_ = nullptr;

    if (frameSync_.submitAndPresent(swapchain_.handle(), acq.imageIndex)) {
        recreateSwapchain();
    }
}

void VulkanRenderer::shutdown() {
    if (ctx_.device() == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(ctx_.device());

    passChain_.shutdown();
    bindlessTextures_.shutdown();
    skinBufferPool_.shutdown();
    frameUniforms_.shutdown();
    assets_.shutdown();
    frameSync_.shutdown();
    swapchain_.shutdown();
    resources_.shutdown();
    ctx_.shutdown();
}
