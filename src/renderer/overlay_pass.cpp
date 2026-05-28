// src/renderer/overlay_pass.cpp
// =============================================================================
// PART4 4a-2: OverlayPass — HUD + ImGui in their own dynamic-rendering scope.
// =============================================================================
#include "renderer/overlay_pass.h"

#include <stdexcept>

#include "renderer/barrier.h"
#include "renderer/hud_pass.h"
#include "renderer/imgui_layer.h"
#include "renderer/vulkan_context.h"

void OverlayPass::init(const InitInfo& info) {
    if (!info.ctx) throw std::runtime_error("OverlayPass::init: ctx is null");
    if (info.colorFormat == VK_FORMAT_UNDEFINED)
        throw std::runtime_error("OverlayPass::init: colorFormat is UNDEFINED");
    ctx_ = info.ctx;
    colorFormat_ = info.colorFormat;
}

void OverlayPass::shutdown() {
    ctx_ = nullptr;
    colorFormat_ = VK_FORMAT_UNDEFINED;
}

void OverlayPass::execute(const ExecuteInfo& info) {
    if (info.cmd == VK_NULL_HANDLE)
        throw std::runtime_error("OverlayPass::execute: invalid cmd");
    if (info.hdrColorView == VK_NULL_HANDLE || info.hdrColorImage == VK_NULL_HANDLE)
        throw std::runtime_error("OverlayPass::execute: HDR target not bound");

    VkRenderingAttachmentInfo colorAtt{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAtt.imageView = info.hdrColorView;
    colorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
    ri.renderArea = {{0, 0}, info.extent};
    ri.layerCount = 1;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachments = &colorAtt;
    // No pDepthAttachment: HUD + ImGui pipelines are built with
    // depthAttachmentFormat = VK_FORMAT_UNDEFINED so depth is not in scope.

    vkCmdBeginRendering(info.cmd, &ri);

    if (info.hudPass && info.hud) {
        HudPass::ExecuteInfo hi{};
        hi.cmd = info.cmd;
        hi.drawList = info.hud;
        hi.screenW = info.screenW;
        hi.screenH = info.screenH;
        info.hudPass->execute(hi);
    }
    if (info.imgui) {
        info.imgui->recordDrawCommands(info.cmd);
    }

    vkCmdEndRendering(info.cmd);

    // Hand HDR off to BloomPass / PostPass in SHADER_READ_ONLY_OPTIMAL. The
    // legacy renderpass setup did this implicitly via finalLayout; dynamic
    // rendering needs the explicit barrier.
    barrier::recordImage(*ctx_, info.cmd, barrier::ImageBarrier{
        .image = info.hdrColorImage,
        .range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcStage  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStage  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .dstAccess = VK_ACCESS_2_SHADER_READ_BIT,
    });
}
