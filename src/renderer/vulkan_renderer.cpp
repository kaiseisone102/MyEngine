// src/renderer/vulkan_renderer.cpp
// =============================================================================
// + ParticleSystem 連携 (C 案: setCurrentParticles で保持して drawFrame で使う)
// + Phase 1C: scene_.toLightingData() 廃止 → currentLighting_ を直接使う
//            recordFrame に normalLighting / waterTime / reflectShadows を渡す
// =============================================================================

#include "renderer/vulkan_renderer.h"
#include <iostream>
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
    createHdrTarget();
    createBloomTargets();
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
        info.hdrColorView = hdrTarget_.view();
        info.hdrColorSampler = hdrTarget_.sampler();  // Phase 1H-3  // Phase 1H-2
        info.hdrColorFormat = hdrTarget_.format();
        info.shaderDir = shaderDir_;
        info.bloomViewA = bloomTargetA_.view();
        info.bloomSamplerA = bloomTargetA_.sampler();
        info.bloomViewB = bloomTargetB_.view();
        info.bloomSamplerB = bloomTargetB_.sampler();
        info.bloomFormat = bloomTargetA_.format();
        info.bloomWidth = bloomTargetA_.extent().width;
        info.bloomHeight = bloomTargetA_.extent().height;
        passChain_.init(info);
    }
}

void VulkanRenderer::recreateSwapchain() {
    swapchain_.recreate();
    // Phase 1H-2: HDR target must be recreated BEFORE MainPass framebuffers (which use its view)
    hdrTarget_.shutdown();
    createHdrTarget();
    // Phase 1I: bloom targets follow swapchain size too
    bloomTargetA_.shutdown();
    bloomTargetB_.shutdown();
    createBloomTargets();
    passChain_.onSwapchainResized(hdrTarget_.view(), hdrTarget_.sampler(),
                                  bloomTargetA_.view(), bloomTargetA_.sampler(),
                                  bloomTargetB_.view(), bloomTargetB_.sampler(),
                                  bloomTargetA_.extent().width, bloomTargetA_.extent().height);
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
    lighting.shadowParams.y = float(shadowQuality_);  // PCF quality
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
        info.normalLighting.shadowParams.y = float(shadowQuality_);
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
    hdrTarget_.shutdown();
    bindlessTextures_.shutdown();
    skinBufferPool_.shutdown();
    frameUniforms_.shutdown();
    assets_.shutdown();
    frameSync_.shutdown();
    swapchain_.shutdown();
    resources_.shutdown();
    ctx_.shutdown();
}

// === Phase 1H-1: HDR render target helper ===
void VulkanRenderer::createHdrTarget() {
    RenderTarget::Desc hdrDesc{};
    hdrDesc.width = swapchain_.extent().width;
    hdrDesc.height = swapchain_.extent().height;
    hdrDesc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    hdrDesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    hdrDesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    hdrDesc.createSampler = true;
    hdrDesc.samplerFilter = VK_FILTER_LINEAR;
    hdrTarget_.init(&ctx_, &resources_, hdrDesc);
    std::cout << "[VulkanRenderer] HDR target created (" << hdrDesc.width << "x" << hdrDesc.height << ")\n";
}

void VulkanRenderer::createBloomTargets() {
    // Half-resolution ping-pong targets, same HDR float format.
    RenderTarget::Desc d{};
    d.width  = swapchain_.extent().width  / 2;
    d.height = swapchain_.extent().height / 2;
    if (d.width  == 0) d.width  = 1;
    if (d.height == 0) d.height = 1;
    d.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    d.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    d.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    d.createSampler = true;
    d.samplerFilter = VK_FILTER_LINEAR;
    bloomTargetA_.init(&ctx_, &resources_, d);
    bloomTargetB_.init(&ctx_, &resources_, d);
    std::cout << "[VulkanRenderer] Bloom targets created (" << d.width << "x" << d.height << ")\n";
}
