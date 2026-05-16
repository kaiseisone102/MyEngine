// include/MyEngine/renderer/vulkan_renderer.h
#pragma once
// =============================================================================
// vulkan_renderer.h — リファクタ Step 10 (案C: 徹底分離後)
// =============================================================================
// 切り出し済み:
//   [Step 1] VulkanContext    : Instance/Surface/PhysicalDevice/Device/Queues
//   [Step 2] ResourceFactory  : Buffer/Image/Memory/ワンタイムコマンド
//   [Step 3] Swapchain        : Swapchain/Views/Depth
//   [Step 4] Mesh / Texture   : OBJ ロード / テクスチャアップロード
//   [Step 5] FrameSync        : Sync prims / CommandPool / CommandBuffers
//   [Step 6] FrameUniforms    : per-frame UBO + DescriptorSet
//   [Step 7] ImGuiLayer       : Dear ImGui ライフサイクル (PassChain 所有)
//   [Step 8] RenderTarget     : Image+Memory+View+Sampler の RAII
//            ShadowPass       : シャドウマップ描画 (PassChain 所有)
//            shader_util      : SPIR-V ロード
//   [Step 9] MainPass         : メインカラー描画 (PassChain 所有)
//   [Step10] SceneData        : シーン状態 + LightingData 派生
//            AssetRegistry    : Mesh / Texture の所有
//            PassChain        : Pass 群と ImGui の配線・記録
//
// VulkanRenderer の責務は以下のみ:
//   - 各コンポーネントの init / shutdown を順序通りに呼ぶ
//   - drawFrame: acquire → frameUniforms.update → passChain.recordFrame
//                → submitAndPresent
//   - シーン/アセットへのアクセサ提供
//
// 描画ロジック・行列の組み立て・Pass の順序はすべて他クラスに委譲済み。
// =============================================================================

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <functional>
#include <string>

#include "frame_sync.h"
#include "frame_uniforms.h"
#include "renderer/asset_registry.h"
#include "renderer/pass_chain.h"
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

    // ─── シーンへのアクセス ───────────────────────────────────────
    // 直接 SceneData を露出する。setter ラッパーを増やすより、
    // SceneData の API を呼んでもらう方が薄い。
    SceneData& scene() { return scene_; }
    const SceneData& scene() const { return scene_; }

    // ─── アセットへのアクセス ─────────────────────────────────────
    AssetRegistry& assets() { return assets_; }
    const AssetRegistry& assets() const { return assets_; }

   private:
    SDL_Window* window_ = nullptr;
    std::string shaderDir_;
    std::string assetDir_;

    // ─── コア / 共通リソース ─────────────────────────────────────
    VulkanContext ctx_;
    ResourceFactory resources_;
    Swapchain swapchain_;
    FrameSync frameSync_;
    FrameUniforms frameUniforms_;

    // ─── 上位コンポーネント ──────────────────────────────────────
    AssetRegistry assets_;
    PassChain passChain_;
    SceneData scene_;

    void recreateSwapchain();
};
