// src/renderer/vulkan_renderer.cpp
// =============================================================================
// + ParticleSystem 連携 (C 案: setCurrentParticles で保持して drawFrame で使う)
// + Phase 1C: scene_.toLightingData() 廃止 → currentLighting_ を直接使う
//            recordFrame に normalLighting / waterTime / reflectShadows を渡す
// =============================================================================

#include "renderer/vulkan_renderer.h"
#include <iostream>
#include <cmath>
#include <limits>

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

    frameSync_.init(&ctx_, swapchain_.imageCount());
    deletionQueue_.init(&ctx_);

    // Phase 1D: bindless must be initialized BEFORE assets so textures can be registered
    bindlessTextures_.init(&ctx_);
    createHdrTarget();
    assets_.init(&ctx_, &resources_, assetDir_, &bindlessTextures_, &deletionQueue_);

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
        info.deletionQueue = &deletionQueue_;  // PART4 4-前-3: CullingPass / DrawDataPool grow path
        info.bindlessSetLayout = bindlessTextures_.layout();
        info.hdrColorView = hdrTarget_.view();
        info.hdrColorSampler = hdrTarget_.sampler();  // Phase 1H-3  // Phase 1H-2
        info.hdrColorFormat = hdrTarget_.format();
        info.shaderDir = shaderDir_;
        info.bloomFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        info.bloomBaseWidth = swapchain_.extent().width / 2;
        info.bloomBaseHeight = swapchain_.extent().height / 2;
        info.bloomMaxMips = 6;
        passChain_.init(info);
    }
}

void VulkanRenderer::destroyRenderTargets() {
    // Single source of truth for tearing down swapchain-sized targets.
    hdrTarget_.shutdown();
}

void VulkanRenderer::recreateSwapchain() {
    // Pending deletions may reference resources tied to the old swapchain; the
    // GPU must be idle before we rebuild, so flush everything here.
    vkDeviceWaitIdle(ctx_.device());
    deletionQueue_.flushAll();
    swapchain_.recreate();
    // HDR target must be recreated BEFORE MainPass framebuffers (which use its view).
    // Bloom mip chain is owned by BloomPass and rebuilt inside onSwapchainResized.
    destroyRenderTargets();
    createHdrTarget();
    passChain_.onSwapchainResized(hdrTarget_.view(), hdrTarget_.sampler(),
                                  swapchain_.extent().width / 2, swapchain_.extent().height / 2);
}

void VulkanRenderer::onResize() {
    if (ctx_.device() != VK_NULL_HANDLE) recreateSwapchain();
}

FrameUniforms::LightingUBO VulkanRenderer::buildCompleteFrameUBO() const {
    // Start from what the camera/lighting system gave us via setLighting(),
    // then fill every per-frame field here so the result is complete in one place.
    FrameUniforms::LightingUBO ubo = currentLighting_;
    ubo.shadowParams.y = float(shadowQuality_);  // PCF quality
    ubo.shadowParams.z = normalMapping_ ? 1.f : 0.f;  // Phase 1K-5: normal-map toggle
    ubo.shadowParams.w = mrMapping_ ? 1.f : 0.f;      // Phase 1K-4: metallic-roughness toggle

    const VkExtent2D ext = swapchain_.extent();
    const float fw = static_cast<float>(ext.width);
    const float fh = static_cast<float>(ext.height);
    const float invW = (fw > 0.f) ? 1.f / fw : 0.f;
    const float invH = (fh > 0.f) ? 1.f / fh : 0.f;
    ubo.time = glm::vec4(elapsedTime_, lastDt_, static_cast<float>(frameNumber_),
                         std::sin(elapsedTime_));
    ubo.screenSize = glm::vec4(fw, fh, invW, invH);
    ubo.jitter = glm::vec4(0.f);
    // farZ = +INFINITY: reverse-Z infinite-far perspective (see renderer/projection.h).
    // Shaders currently do not read cameraParams.y; if a future depth-linearization
    // pass needs a usable far, derive it from near and ndc_z instead.
    ubo.cameraParams = glm::vec4(0.1f, std::numeric_limits<float>::infinity(),
                                  glm::radians(45.f), (fh > 0.f) ? fw / fh : 1.f);

    // Material SSBO address (BDA) for the shaders' buffer_reference.
    ubo.materialBuffer = assets_.materialRegistry().bufferAddressPacked();
    return ubo;
}

void VulkanRenderer::drawFrame(std::function<void()> uiCallback) {
    auto acq = frameSync_.acquireNextImage(swapchain_.handle());
    if (acq.needsRecreate) {
        recreateSwapchain();
        return;
    }
    // The fence for acq.frameIndex was just waited on, so anything enqueued under
    // this frameIndex one cycle ago is no longer in use by the GPU: free it now.
    // Then charge new enqueues this frame to the same frameIndex bucket.
    deletionQueue_.collectFrame(acq.frameIndex);
    deletionQueue_.setCurrentFrame(acq.frameIndex);

    // Phase 1C: 旧 scene_.toLightingData() を廃止。 camera_system が
    // setLighting() で渡した currentLighting_ をそのまま使う。
    // === Phase 1C: time tracking ===
    const uint64_t nowTicks = SDL_GetTicks();
    if (startTickMs_ == 0) startTickMs_ = nowTicks;
    const float dt = (lastTickMs_ > 0) ? static_cast<float>(nowTicks - lastTickMs_) / 1000.f : 0.f;
    lastDt_ = dt;
    lastTickMs_ = nowTicks;
    elapsedTime_ = static_cast<float>(nowTicks - startTickMs_) / 1000.f;
    ++frameNumber_;

    // Flush pending material edits to the GPU (no-op unless dirty). This is the
    // asset->GPU transfer step, kept separate from UBO assembly; it is the
    // natural hook for a future loading screen.
    assets_.materialRegistry().upload();

    // One fully-populated UBO, used by both the main and reflection passes.
    const auto lighting = buildCompleteFrameUBO();
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

        // Reflection uses the same fully-populated UBO as the main pass, so it
        // also gets time / screenSize / cameraParams / materialBuffer (previously
        // this copied the half-filled currentLighting_ and silently dropped them).
        info.normalLighting = lighting;
        info.waterTime = waterTime_;
        info.reflectShadows = reflectShadows_;

        passChain_.recordFrame(info);
    }

    // フレーム終わったら particles ポインタはクリア。
    // 次フレームの buildScene で setter が再度呼ばれる想定。
    // Layer が遷移したり Title 画面のように setter を呼ばない場合に
    // 古い (もう無効な) ポインタを使わないための安全策。
    currentParticles_ = nullptr;
    // Clear the HUD at the frame boundary so each layer only has to ADD its own
    // shapes (a layer that draws no HUD, like the title, then shows nothing
    // instead of leaking the previous layer's bars).
    hud_.clear();

    if (frameSync_.submitAndPresent(swapchain_.handle(), acq.imageIndex)) {
        recreateSwapchain();
    }
}

void VulkanRenderer::shutdown() {
    if (ctx_.device() == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(ctx_.device());
    deletionQueue_.flushAll();  // GPU idle: safe to free everything still pending
    deletionQueue_.shutdown();

    passChain_.shutdown();
    destroyRenderTargets();  // HDR + bloom A/B (was leaking bloom targets)
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
