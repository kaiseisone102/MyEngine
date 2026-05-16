// =============================================================================
// hud_pass.cpp — HUD 描画パス本体
// =============================================================================
#include "renderer/hud_pass.h"

#include "renderer/hud_draw_list.h"
#include "renderer/swapchain.h"
#include "renderer/vulkan_context.h"

void HudPass::init(const InitInfo& info) {
    ctx_ = info.ctx;
    swapchain_ = info.swapchain;
    pipeline_.init(ctx_, info.mainRenderPass, info.shaderDir);
}

void HudPass::shutdown() {
    pipeline_.shutdown();
    ctx_ = nullptr;
    swapchain_ = nullptr;
}

void HudPass::execute(const ExecuteInfo& info) {
    if (!info.drawList || info.drawList->empty()) return;
    if (info.screenW <= 0.f || info.screenH <= 0.f) return;

    // viewport / scissor は MainPass 終了時点で設定されているが、 念のため再設定。
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

    for (const auto& r : info.drawList->rects()) {
        HudPipeline::PushConstants pc{};
        pc.screenSize = {info.screenW, info.screenH};
        pc.rectMin = r.min;
        pc.rectSize = r.size;
        pc.color = r.color;
        vkCmdPushConstants(info.cmd, pipeline_.layout(),
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc),
                           &pc);
        vkCmdDraw(info.cmd, 4, 1, 0, 0);
    }
}
