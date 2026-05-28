// include/MyEngine/renderer/depth_layouts.h
#pragma once
// =============================================================================
// depth_layouts.h — pick the right VK_IMAGE_LAYOUT_DEPTH_* layout
// =============================================================================
//
// PART4 4a-2: D32_SFLOAT has no stencil, so the precise Vulkan 1.2 separate
// depth/stencil layouts (VK_IMAGE_LAYOUT_DEPTH_*_OPTIMAL) describe the depth
// image exactly. Those layouts require the device feature
// separateDepthStencilLayouts; when the feature isn't supported we use the
// legacy combined VK_IMAGE_LAYOUT_DEPTH_STENCIL_*_OPTIMAL form, which works
// on every Vulkan version.
//
// The choice is read at every call site (main_pass barriers, OverlayPass
// depth attachment, GBuffer viewer descriptors). Centralising it here keeps
// those sites consistent with each other and with vulkan_context's queried
// capability bit - changing depth-aspect handling later only touches one
// file.
// =============================================================================

#include <vulkan/vulkan.h>

#include "renderer/vulkan_context.h"

namespace depth_layouts {

inline VkImageLayout attachment(const VulkanContext& ctx) {
    return ctx.separateDepthStencilLayouts()
               ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
               : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
}

inline VkImageLayout readOnly(const VulkanContext& ctx) {
    return ctx.separateDepthStencilLayouts()
               ? VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL
               : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
}

}  // namespace depth_layouts
