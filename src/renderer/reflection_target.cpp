// =============================================================================
// reflection_target.cpp
// =============================================================================
#include "renderer/reflection_target.h"

void ReflectionTarget::init(VulkanContext* ctx, ResourceFactory* resources, uint32_t width,
                              uint32_t height, VkFormat colorFormat, VkFormat depthFormat) {
    extent_ = {width, height};

    // ─── Color (sampled、 後で water shader でサンプル) ────
    RenderTarget::Desc colorDesc{};
    colorDesc.width = width;
    colorDesc.height = height;
    colorDesc.format = colorFormat;
    colorDesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    colorDesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    colorDesc.createSampler = true;
    colorDesc.samplerFilter = VK_FILTER_LINEAR;
    colorDesc.samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    color_.init(ctx, resources, colorDesc);

    // ─── Depth (描画中の depth test 用、 sample 不要) ────
    RenderTarget::Desc depthDesc{};
    depthDesc.width = width;
    depthDesc.height = height;
    depthDesc.format = depthFormat;
    depthDesc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthDesc.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthDesc.createSampler = false;
    depth_.init(ctx, resources, depthDesc);
}

void ReflectionTarget::shutdown() {
    depth_.shutdown();
    color_.shutdown();
    extent_ = {};
}
