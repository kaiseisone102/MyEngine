// include/MyEngine/renderer/vulkan_renderer.h
#pragma once
// =============================================================================
// + setReflectionQuality (反射品質変更時のフック、 orchestrator が呼ぶ)
// + setLighting / setWaterTime (Phase 1C: 旧 scene.setLightingParams/setViewProjection
//   を廃止して、 LightingUBO を VulkanRenderer 側で保持して drawFrame で使う)
// =============================================================================
#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "core/game_settings.h"  // ReflectionQuality
#include "core/particle.h"
#include "frame_sync.h"
#include "frame_uniforms.h"
#include "renderer/asset_registry.h"
#include "renderer/debug_line_renderer.h"
#include "renderer/hud_draw_list.h"
#include "renderer/pass_chain.h"
#include "renderer/skin_buffer_pool.h"
#include "resource_factory.h"
#include "scene/scene_data.h"
#include "swapchain.h"
#include "vulkan_context.h"

class VulkanRenderer {
   public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = FrameSync::MAX_FRAMES_IN_FLIGHT;

    void init(SDL_Window* window);
    void drawFrame(std::function<void()> uiCallback = {});
    void onResize();
    void shutdown();

    void processEvent(const SDL_Event& e) { passChain_.processEvent(e); }

    // 反射品質を変更する。 内部で vkDeviceWaitIdle を呼んで安全に再構築。
    void setReflectionQuality(ReflectionQuality q) {
        vkDeviceWaitIdle(ctx_.device());
        passChain_.onReflectionQualityChanged(q);
    }

    SceneData& scene() { return scene_; }
    const SceneData& scene() const { return scene_; }

    AssetRegistry& assets() { return assets_; }
    const AssetRegistry& assets() const { return assets_; }

    VulkanContext& context() { return ctx_; }
    const VulkanContext& context() const { return ctx_; }
    ResourceFactory& resources() { return resources_; }
    const ResourceFactory& resources() const { return resources_; }

    SkinBufferPool& skinBufferPool() { return skinBufferPool_; }
    const SkinBufferPool& skinBufferPool() const { return skinBufferPool_; }

    DebugLineRenderer& debugLines() { return debugLines_; }
    const DebugLineRenderer& debugLines() const { return debugLines_; }

    HudDrawList& hud() { return hud_; }
    const HudDrawList& hud() const { return hud_; }

    // 現フレームのライティング情報。 camera_system が毎フレーム書き込む。
    // 通常 view + lighting + shadow camera (lightVP) + shadowParams を含む。
    void setLighting(const FrameUniforms::LightingUBO& lighting) {
        currentLighting_ = lighting;
    }

    // 現フレームの水面アニメーション時間 (累積秒)。 game_loop_orchestrator が更新。
    void setWaterTime(float t) { waterTime_ = t; }

    // 反射に影を含めるか (反射 shader 内で shadowStrength を 0 にするだけの軽量フラグ)。
    void setReflectShadows(bool b) { reflectShadows_ = b; }

    // パーティクル参照を設定する (Layer の buildScene 内で呼ぶ想定、 nullptr 可)。
    void setCurrentParticles(const std::vector<particle::Particle>* particles) {
        currentParticles_ = particles;
    }

   private:
    SDL_Window* window_ = nullptr;
    std::string shaderDir_;
    std::string assetDir_;

    VulkanContext ctx_;
    ResourceFactory resources_;
    Swapchain swapchain_;
    FrameSync frameSync_;
    FrameUniforms frameUniforms_;

    AssetRegistry assets_;
    SkinBufferPool skinBufferPool_;
    PassChain passChain_;
    SceneData scene_;

    DebugLineRenderer debugLines_;
    HudDrawList hud_;

    const std::vector<particle::Particle>* currentParticles_ = nullptr;

    // Phase 1C 追加: 旧 scene.setLightingParams 廃止に伴う移植
    FrameUniforms::LightingUBO currentLighting_{};
    float waterTime_ = 0.f;
    bool reflectShadows_ = true;

    void recreateSwapchain();
};
