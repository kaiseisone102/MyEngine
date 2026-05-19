#pragma once
// =============================================================================
// hud_pass.h — HUD 描画パス
// =============================================================================
// MainPass の renderPass 内で execute される (DebugLinePass と同じパターン)。
// HudDrawList の全矩形を 1 個ずつ push constants で描画する。
//
// HudPipeline を内部で所有 (init で一緒に作る、 shutdown で破棄)。
// =============================================================================

#include <vulkan/vulkan.h>

#include <string>

#include "renderer/hud_pipeline.h"

class VulkanContext;
class Swapchain;
class HudDrawList;

class HudPass {
   public:
    struct InitInfo {
        VulkanContext* ctx = nullptr;
        Swapchain* swapchain = nullptr;
        VkRenderPass mainRenderPass = VK_NULL_HANDLE;
        std::string shaderDir;
    };

    struct ExecuteInfo {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        const HudDrawList* drawList = nullptr;
        // ウィンドウ現在の解像度 (NDC 変換に使う)
        float screenW = 0.f;
        float screenH = 0.f;
    };

    void init(const InitInfo& info);
    void shutdown();

    // MainPass の renderPass 内で呼ぶ。
    void execute(const ExecuteInfo& info);

   private:
    VulkanContext* ctx_ = nullptr;
    Swapchain* swapchain_ = nullptr;
    HudPipeline pipeline_;
};
