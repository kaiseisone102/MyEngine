// src/renderer/hzb_debug_widget.cpp - PART4 4b Hi-Z pyramid viewer
#include "renderer/hzb_debug_widget.h"

#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <stdexcept>
#include <string>

#include "renderer/hiz_pass.h"
#include "renderer/vulkan_context.h"

void HzbDebugWidget::init(VulkanContext* ctx) {
    if (!ctx) throw std::runtime_error("HzbDebugWidget::init: ctx is null");
    ctx_ = ctx;

    // NEAREST so the viewer shows exact texel values without filtering. The
    // pyramid is RG32F; ImGui's fragment shader samples it as a normal
    // texture (we see .r in red, .g in green, .ba clamped to 0/1 = no blue,
    // alpha=1).
    VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter = VK_FILTER_NEAREST;
    sci.minFilter = VK_FILTER_NEAREST;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sci.addressModeU = sci.addressModeV = sci.addressModeW =
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VkSampler s = VK_NULL_HANDLE;
    if (vkCreateSampler(ctx_->device(), &sci, nullptr, &s) != VK_SUCCESS)
        throw std::runtime_error("HzbDebugWidget::init: vkCreateSampler failed");
    sampler_ = VkUnique<VkSampler>(ctx_->device(), s);
}

void HzbDebugWidget::releaseImGuiDescriptors() {
    for (auto& mips : perFrameMipDescs_) {
        for (VkDescriptorSet& id : mips) {
            if (id != VK_NULL_HANDLE) {
                ImGui_ImplVulkan_RemoveTexture(id);
                id = VK_NULL_HANDLE;
            }
        }
    }
    perFrameMipDescs_.clear();
}

void HzbDebugWidget::shutdown() {
    // Release ImGui-backed descriptors BEFORE the ImGui Vulkan backend tears
    // down its pool (PassChain destruction order: widget -> imgui).
    releaseImGuiDescriptors();
    sampler_.reset();
    ctx_ = nullptr;
}

void HzbDebugWidget::setPyramid(const HiZPass* hizPass) {
    perFrameMipViews_.clear();
    if (!hizPass) {
        dirty_ = true;
        return;
    }
    const uint32_t mips = hizPass->mipCount();
    perFrameMipViews_.assign(HiZPass::kMaxFramesInFlight, {});
    for (uint32_t f = 0; f < HiZPass::kMaxFramesInFlight; ++f) {
        perFrameMipViews_[f].reserve(mips);
        for (uint32_t m = 0; m < mips; ++m) {
            perFrameMipViews_[f].push_back(hizPass->pyramidMipView(f, m));
        }
    }
    if (selectedMip_ >= mips) selectedMip_ = 0;
    dirty_ = true;
}

void HzbDebugWidget::draw() {
    if (!ctx_ || !sampler_) return;
    if (perFrameMipViews_.empty() || perFrameMipViews_[0].empty()) return;

    if (dirty_) {
        releaseImGuiDescriptors();
        const VkSampler smp = sampler_.get();
        perFrameMipDescs_.assign(perFrameMipViews_.size(), {});
        for (uint32_t f = 0; f < perFrameMipViews_.size(); ++f) {
            perFrameMipDescs_[f].assign(perFrameMipViews_[f].size(), VK_NULL_HANDLE);
            for (uint32_t m = 0; m < perFrameMipViews_[f].size(); ++m) {
                VkImageView v = perFrameMipViews_[f][m];
                if (v == VK_NULL_HANDLE) continue;
                // HiZPass keeps the pyramid in GENERAL; ImGui can sample it
                // from that layout (Vulkan allows GENERAL for sampled reads).
                perFrameMipDescs_[f][m] = ImGui_ImplVulkan_AddTexture(
                    smp, v, VK_IMAGE_LAYOUT_GENERAL);
            }
        }
        dirty_ = false;
    }

    // Dock to the lower-right on first launch (gbuffer widget is upper-right).
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 viewerSize(280, 360);
    const ImVec2 viewerPos(vp->WorkPos.x + vp->WorkSize.x - viewerSize.x - 8.0f,
                            vp->WorkPos.y + vp->WorkSize.y - viewerSize.y - 8.0f);
    ImGui::SetNextWindowPos(viewerPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(viewerSize, ImGuiCond_FirstUseEver);

    if (ImGui::Begin("HZB (4b)", &open_)) {
        const uint32_t frames = static_cast<uint32_t>(perFrameMipViews_.size());
        const uint32_t mips = static_cast<uint32_t>(perFrameMipViews_[0].size());

        int frameIdx = static_cast<int>(selectedFrame_);
        int mipIdx = static_cast<int>(selectedMip_);
        ImGui::SliderInt("frame",  &frameIdx, 0, static_cast<int>(frames - 1));
        ImGui::SliderInt("mip",    &mipIdx,   0, static_cast<int>(mips - 1));
        selectedFrame_ = static_cast<uint32_t>(frameIdx);
        selectedMip_   = static_cast<uint32_t>(mipIdx);

        ImGui::TextDisabled("RG32F: R=min G=max (reverse-Z: 0 far, 1 near)");

        VkDescriptorSet id = perFrameMipDescs_[selectedFrame_][selectedMip_];
        if (id) {
            const ImVec2 size(256, 144);
            ImGui::Image(reinterpret_cast<ImTextureID>(id), size);
        } else {
            ImGui::Text("(no view bound yet)");
        }
    }
    ImGui::End();
}
