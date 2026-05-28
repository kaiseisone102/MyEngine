// include/MyEngine/renderer/overlay_pass.h
#pragma once
// =============================================================================
// overlay_pass.h — PART4 4a-2: HUD + ImGui as a post-scene rendering scope
// =============================================================================
//
// Responsibility:
//   - Owns the dynamic-rendering scope (vkCmdBeginRendering /
//     vkCmdEndRendering) for HUD + ImGui draws.
//   - Runs AFTER main_pass and BEFORE bloom / post. main_pass leaves HDR in
//     COLOR_ATTACHMENT_OPTIMAL; we keep writing to it with loadOp=LOAD and
//     hand it off to bloom/post in SHADER_READ_ONLY at the end.
//
// Why a separate class (not folded into main_pass):
//   - main_pass attaches the GBuffer (HDR + normal + motion + depth) in its
//     opaque MRT scope. The ImGui debug viewer SAMPLES the GBuffer through
//     ImGui::Image. Mixing those two roles inside one pass forced a
//     mid-pass barrier dance in the original 4a-2 design (and TDR'd on the
//     first launch). Pulling overlay out cleans the lifecycle and removes
//     the feedback-loop hazard entirely.
//
// Why no depth attachment here:
//   - HudPass and ImGui pipelines run with depthTestEnable=FALSE and
//     depthWriteEnable=FALSE. They are built with
//     VkPipelineRenderingCreateInfo::depthAttachmentFormat = VK_FORMAT_UNDEFINED,
//     so the BeginRendering scope is color-only (HDR). Validation: a
//     pipeline that declares no depth slot and an active rendering with no
//     depth attachment is the correct, modern shape.
// =============================================================================

#include <vulkan/vulkan.h>

class VulkanContext;
class HudPass;
class HudDrawList;
class ImGuiLayer;

class OverlayPass {
   public:
    struct InitInfo {
        VulkanContext* ctx = nullptr;
        VkFormat colorFormat = VK_FORMAT_UNDEFINED;  // HDR target format
    };

    struct ExecuteInfo {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkExtent2D extent = {0, 0};
        VkImageView hdrColorView = VK_NULL_HANDLE;
        VkImage hdrColorImage = VK_NULL_HANDLE;
        HudPass* hudPass = nullptr;
        const HudDrawList* hud = nullptr;
        float screenW = 0.f;
        float screenH = 0.f;
        ImGuiLayer* imgui = nullptr;
    };

    void init(const InitInfo& info);
    void shutdown();
    void execute(const ExecuteInfo& info);

    VkFormat colorFormat() const { return colorFormat_; }

   private:
    VulkanContext* ctx_ = nullptr;
    VkFormat colorFormat_ = VK_FORMAT_UNDEFINED;
};
