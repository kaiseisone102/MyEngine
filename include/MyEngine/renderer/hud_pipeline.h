#pragma once
// =============================================================================
// hud_pipeline.h — HUD 矩形描画用 Vulkan pipeline
// =============================================================================
// 設計:
//   - 頂点バッファ・入力レイアウト なし (gl_VertexIndex から quad 生成)
//   - DescriptorSet なし
//   - Push constants のみ (screenSize, rectMin, rectSize, color = 計 48 bytes)
//   - TRIANGLE_STRIP で 4 頂点 = 2 三角形 = 1 quad
//   - アルファブレンド有効 (半透明背景など)
//   - depth テスト無効 (HUD は常に手前)
//   - cullMode = NONE (UI なので両面描画)
//
// MainPass の renderPass を共有 (同じ render pass 内で描画される)。
// =============================================================================

#include <vulkan/vulkan.h>

#include <string>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

class VulkanContext;

class HudPipeline {
   public:
    // Push constants の構造 (vert/frag で共通参照)。
    //
    // GLSL std430 (push_constant block) のアラインメント則:
    //   - vec2 は 8-byte align
    //   - vec4 は 16-byte align ★
    //
    // 単純な float 並びだと vec4 color の前にパディングが入って
    // C++ 側とオフセットがズレ、 GLSL 側で color の中身がゴミになる。
    // glm 型 + alignas(16) で揃えて C++ ↔ GLSL のオフセットを一致させる。
    //
    //   offset  0: screenSize (vec2)
    //   offset  8: rectMin    (vec2)
    //   offset 16: rectSize   (vec2)
    //   offset 32: color      (vec4) ← 8 bytes padding が入る
    //   total: 48 bytes
    struct PushConstants {
        glm::vec2 screenSize;
        glm::vec2 rectMin;
        glm::vec2 rectSize;
        alignas(16) glm::vec4 color;
    };
    static_assert(sizeof(PushConstants) == 48,
                  "HudPipeline::PushConstants size mismatch (expected 48 bytes)");

    void init(VulkanContext* ctx, VkRenderPass renderPass, const std::string& shaderDir);
    void shutdown();

    VkPipeline pipeline() const { return pipeline_; }
    VkPipelineLayout layout() const { return layout_; }

   private:
    VulkanContext* ctx_ = nullptr;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;

    void createLayout();
    void createPipeline(VkRenderPass renderPass, const std::string& shaderDir);
};
