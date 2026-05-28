// =============================================================================
// hud_pass.cpp — HUD 描画パス本体 (shape mode 対応)
// =============================================================================
#include "renderer/hud_pass.h"

#include "renderer/hud_draw_list.h"
#include "renderer/swapchain.h"
#include "renderer/vulkan_context.h"

void HudPass::init(const InitInfo& info) {
    ctx_ = info.ctx;
    swapchain_ = info.swapchain;
    pipeline_.init(ctx_, info.colorFormat, info.depthFormat, info.shaderDir);
}

void HudPass::shutdown() {
    pipeline_.shutdown();
    ctx_ = nullptr;
    swapchain_ = nullptr;
}

void HudPass::execute(const ExecuteInfo& info) {
    if (!info.drawList || info.drawList->empty()) return;
    if (info.screenW <= 0.f || info.screenH <= 0.f) return;

    VkViewport viewport{};
    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.width = info.screenW;
    viewport.height = info.screenH;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(info.cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {static_cast<uint32_t>(info.screenW), static_cast<uint32_t>(info.screenH)};
    vkCmdSetScissor(info.cmd, 0, 1, &scissor);

    vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.pipeline());

    for (const auto& s : info.drawList->shapes()) {
        HudPipeline::PushConstants pc{};
        pc.screenSize = {info.screenW, info.screenH};
        pc.rectMin = s.min;
        pc.rectSize = s.size;
        pc.color = s.color;
        pc.shapeMode = static_cast<int32_t>(s.mode);
        pc.flags = s.flags;
        pc.extraParams = s.extraParams;
        vkCmdPushConstants(info.cmd, pipeline_.layout(),
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc),
                           &pc);
        vkCmdDraw(info.cmd, 4, 1, 0, 0);
    }
}
