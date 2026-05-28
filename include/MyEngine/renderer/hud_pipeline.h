#pragma once
// =============================================================================
// hud_pipeline.h — + kFlagBarFlat (BarFillSegmented でグラデ無効化)
// =============================================================================

#include <vulkan/vulkan.h>

#include "renderer/vk_unique.h"

#include <string>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

class VulkanContext;

class HudPipeline {
   public:
    enum ShapeMode : int32_t {
        Rect              = 0,
        Circle            = 1,
        Ring              = 2,
        RotatedRect       = 3,
        CircleSegment     = 4,
        GradientRect      = 5,
        BeveledRect       = 6,
        MetalFrame        = 7,
        Rivet             = 8,
        BarFillSegmented  = 9,
    };

    static constexpr int32_t kFlagGloss   = 1 << 0;
    // BarFillSegmented で立体感 (グラデ + ハイライト) を無効化、 ベタ塗りにする。
    // 鮮やかな色をそのまま表示したい場合に使う (例: ダメージ赤バー)。
    static constexpr int32_t kFlagBarFlat = 1 << 1;

    struct PushConstants {
        alignas(8)  glm::vec2 screenSize;
        alignas(8)  glm::vec2 rectMin;
        alignas(8)  glm::vec2 rectSize;
        alignas(16) glm::vec4 color;
        alignas(4)  int32_t shapeMode = 0;
        alignas(4)  int32_t flags = 0;
        alignas(8)  glm::vec2 _pad{0.f};
        alignas(16) glm::vec4 extraParams{0.f};
    };

    // PART4 4a-1 / 4a-2: dynamic rendering, color-only (no depth slot - HUD
    // is depth-less by design).
    void init(VulkanContext* ctx, VkFormat colorFormat, const std::string& shaderDir);
    void shutdown();

    VkPipeline pipeline() const { return pipeline_.get(); }
    VkPipelineLayout layout() const { return layout_.get(); }

   private:
    VulkanContext* ctx_ = nullptr;
    VkFormat colorFormat_ = VK_FORMAT_UNDEFINED;
    VkUnique<VkPipelineLayout> layout_;
    VkUnique<VkPipeline> pipeline_;

    void createLayout();
    void createPipeline(const std::string& shaderDir);
};
