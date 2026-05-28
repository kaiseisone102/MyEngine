// src/renderer/gbuffer_debug_widget.cpp
#include "renderer/gbuffer_debug_widget.h"

#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <stdexcept>

#include "renderer/depth_layouts.h"
#include "renderer/vulkan_context.h"

void GBufferDebugWidget::init(VulkanContext* ctx) {
    if (!ctx) throw std::runtime_error("GBufferDebugWidget::init: ctx is null");
    ctx_ = ctx;

    VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    VkSampler s = VK_NULL_HANDLE;
    if (vkCreateSampler(ctx_->device(), &sci, nullptr, &s) != VK_SUCCESS)
        throw std::runtime_error("GBufferDebugWidget::init: vkCreateSampler failed");
    sampler_ = VkUnique<VkSampler>(ctx_->device(), s);
}

void GBufferDebugWidget::shutdown() {
    // The ImGui descriptor sets live in the ImGui Vulkan backend's pool, so
    // they must be released before the backend itself is torn down.
    if (normalId_) { ImGui_ImplVulkan_RemoveTexture(normalId_); normalId_ = VK_NULL_HANDLE; }
    if (motionId_) { ImGui_ImplVulkan_RemoveTexture(motionId_); motionId_ = VK_NULL_HANDLE; }
    if (depthId_)  { ImGui_ImplVulkan_RemoveTexture(depthId_);  depthId_  = VK_NULL_HANDLE; }
    sampler_.reset();
    ctx_ = nullptr;
}

void GBufferDebugWidget::setAttachments(VkImageView normalView, VkImageView motionView,
                                         VkImageView depthView) {
    normalView_ = normalView;
    motionView_ = motionView;
    depthView_  = depthView;
    dirty_ = true;
}

void GBufferDebugWidget::draw() {
    if (!ctx_ || !sampler_) return;
    if (normalView_ == VK_NULL_HANDLE) return;  // attachments not bound yet

    if (dirty_) {
        if (normalId_) { ImGui_ImplVulkan_RemoveTexture(normalId_); normalId_ = VK_NULL_HANDLE; }
        if (motionId_) { ImGui_ImplVulkan_RemoveTexture(motionId_); motionId_ = VK_NULL_HANDLE; }
        if (depthId_)  { ImGui_ImplVulkan_RemoveTexture(depthId_);  depthId_  = VK_NULL_HANDLE; }

        const VkSampler smp = sampler_.get();
        normalId_ = ImGui_ImplVulkan_AddTexture(
            smp, normalView_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        motionId_ = ImGui_ImplVulkan_AddTexture(
            smp, motionView_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        // Matches main_pass's post-pass barrier on the depth image (same
        // helper). See depth_layouts.h.
        depthId_ = ImGui_ImplVulkan_AddTexture(
            smp, depthView_, depth_layouts::readOnly(*ctx_));
        dirty_ = false;
    }

    // Dock to the upper-right on first launch; the user can drag/resize
    // freely afterwards (FirstUseEver preserves that state).
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 viewerSize(280, 540);
    const ImVec2 viewerPos(vp->WorkPos.x + vp->WorkSize.x - viewerSize.x - 8.0f,
                            vp->WorkPos.y + 8.0f);
    ImGui::SetNextWindowPos(viewerPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(viewerSize, ImGuiCond_FirstUseEver);

    if (ImGui::Begin("GBuffer (4a-2)", &open_)) {
        const ImVec2 size(256, 144);
        ImGui::TextDisabled("HDR color: on-screen (feedback-loop hazard inside OverlayPass)");
        if (normalId_) {
            ImGui::Text("Normal (octahedral, R=x G=y)");
            ImGui::Image(reinterpret_cast<ImTextureID>(normalId_), size);
        }
        if (motionId_) {
            ImGui::Text("Motion (NDC delta, RG16F)");
            ImGui::Image(reinterpret_cast<ImTextureID>(motionId_), size);
        }
        if (depthId_) {
            ImGui::Text("Depth (reverse-Z, near=1 far=0)");
            ImGui::Image(reinterpret_cast<ImTextureID>(depthId_), size);
        }
    }
    ImGui::End();
}
