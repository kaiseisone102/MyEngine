// include/MyEngine/renderer/barrier.h
#pragma once
// =============================================================================
// barrier.h - VK_KHR_synchronization2 barrier helper (header-only)
// =============================================================================
// Vulkan13 §1 (W). One place to record pipeline barriers in the engine.
//
// Usage:
//   barrier::recordImage(ctx, cmd, barrier::ImageBarrier{
//       .image = mips_[i].image(),
//       .range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
//       .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
//       .newLayout = VK_IMAGE_LAYOUT_GENERAL,
//       .srcStage  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
//       .srcAccess = VK_ACCESS_2_SHADER_WRITE_BIT,
//       .dstStage  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
//       .dstAccess = VK_ACCESS_2_SHADER_READ_BIT,
//   });
//
// Internal capability branching:
//   * synchronization2 (Vulkan 1.3 core) enabled -> VkBufferMemoryBarrier2 /
//     VkImageMemoryBarrier2 packed into a single VkDependencyInfo, one call
//     to vkCmdPipelineBarrier2 per recordBatch (best practice).
//   * Not enabled -> legacy vkCmdPipelineBarrier with one call per call site.
//     The 64-bit stage / access flags are truncated to 32-bit; this is safe
//     for all bits that exist in both APIs (overlap design). NONE / new sync2
//     bits without a legacy equivalent must not be used on devices that lack
//     synchronization2.
//
// Forward-compat coverage (open-world predictable additions):
//   * Streaming (Phase 2F): queue family ownership transfer via srcQueueFamily
//     / dstQueueFamily (default VK_QUEUE_FAMILY_IGNORED).
//   * Async compute (Foundations §2 / PART4 §0.4-V): same QFOT path.
//   * HZB mip chain (PART4 4b SPD): recordBatch packs many image barriers
//     into one vkCmdPipelineBarrier2.
//   * Multi-RT prepass -> SS effects (Phase 3 1J / TAA / SSGI / SSR): same
//     batched form.
//   * Global memory barrier (rare but possible): MemoryBarrier struct.
//
// References:
//   - https://docs.vulkan.org/guide/latest/extensions/VK_KHR_synchronization2.html
// =============================================================================

#include <vulkan/vulkan.h>
#include <cstdint>
#include <span>

#include "renderer/vulkan_context.h"

namespace barrier {

struct MemoryBarrier {  // resource-agnostic / global
    VkPipelineStageFlags2 srcStage  = VK_PIPELINE_STAGE_2_NONE;
    VkAccessFlags2        srcAccess = 0;
    VkPipelineStageFlags2 dstStage  = VK_PIPELINE_STAGE_2_NONE;
    VkAccessFlags2        dstAccess = 0;
};

struct BufferBarrier {
    VkBuffer     buffer = VK_NULL_HANDLE;
    VkDeviceSize offset = 0;
    VkDeviceSize size   = VK_WHOLE_SIZE;
    VkPipelineStageFlags2 srcStage  = VK_PIPELINE_STAGE_2_NONE;
    VkAccessFlags2        srcAccess = 0;
    VkPipelineStageFlags2 dstStage  = VK_PIPELINE_STAGE_2_NONE;
    VkAccessFlags2        dstAccess = 0;
    uint32_t srcQueueFamily = VK_QUEUE_FAMILY_IGNORED;  // QFOT (streaming / async compute)
    uint32_t dstQueueFamily = VK_QUEUE_FAMILY_IGNORED;
};

struct ImageBarrier {
    VkImage                 image = VK_NULL_HANDLE;
    VkImageSubresourceRange range{};
    VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkPipelineStageFlags2 srcStage  = VK_PIPELINE_STAGE_2_NONE;
    VkAccessFlags2        srcAccess = 0;
    VkPipelineStageFlags2 dstStage  = VK_PIPELINE_STAGE_2_NONE;
    VkAccessFlags2        dstAccess = 0;
    uint32_t srcQueueFamily = VK_QUEUE_FAMILY_IGNORED;
    uint32_t dstQueueFamily = VK_QUEUE_FAMILY_IGNORED;
};

// Batched form. Use this when many dependencies fire together (HZB mips, prepass MRTs).
inline void recordBatch(const VulkanContext& ctx, VkCommandBuffer cmd,
                         std::span<const MemoryBarrier> memory,
                         std::span<const BufferBarrier> buffers,
                         std::span<const ImageBarrier>  images) {
    if (ctx.synchronization2()) {
        // Pack everything into one VkDependencyInfo.
        VkMemoryBarrier2 mem2[16];
        VkBufferMemoryBarrier2 buf2[16];
        VkImageMemoryBarrier2 img2[16];
        // The fixed buffers above are sized for current MyEngine call sites.
        // If a future batch exceeds them, switch to a std::pmr::vector or split.
        const uint32_t memN = static_cast<uint32_t>(memory.size());
        const uint32_t bufN = static_cast<uint32_t>(buffers.size());
        const uint32_t imgN = static_cast<uint32_t>(images.size());
        for (uint32_t i = 0; i < memN; ++i) {
            mem2[i] = VkMemoryBarrier2{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
            mem2[i].srcStageMask  = memory[i].srcStage;
            mem2[i].srcAccessMask = memory[i].srcAccess;
            mem2[i].dstStageMask  = memory[i].dstStage;
            mem2[i].dstAccessMask = memory[i].dstAccess;
        }
        for (uint32_t i = 0; i < bufN; ++i) {
            buf2[i] = VkBufferMemoryBarrier2{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
            buf2[i].srcStageMask        = buffers[i].srcStage;
            buf2[i].srcAccessMask       = buffers[i].srcAccess;
            buf2[i].dstStageMask        = buffers[i].dstStage;
            buf2[i].dstAccessMask       = buffers[i].dstAccess;
            buf2[i].srcQueueFamilyIndex = buffers[i].srcQueueFamily;
            buf2[i].dstQueueFamilyIndex = buffers[i].dstQueueFamily;
            buf2[i].buffer              = buffers[i].buffer;
            buf2[i].offset              = buffers[i].offset;
            buf2[i].size                = buffers[i].size;
        }
        for (uint32_t i = 0; i < imgN; ++i) {
            img2[i] = VkImageMemoryBarrier2{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
            img2[i].srcStageMask        = images[i].srcStage;
            img2[i].srcAccessMask       = images[i].srcAccess;
            img2[i].dstStageMask        = images[i].dstStage;
            img2[i].dstAccessMask       = images[i].dstAccess;
            img2[i].oldLayout           = images[i].oldLayout;
            img2[i].newLayout           = images[i].newLayout;
            img2[i].srcQueueFamilyIndex = images[i].srcQueueFamily;
            img2[i].dstQueueFamilyIndex = images[i].dstQueueFamily;
            img2[i].image               = images[i].image;
            img2[i].subresourceRange    = images[i].range;
        }
        VkDependencyInfo di{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        di.memoryBarrierCount       = memN;
        di.pMemoryBarriers          = memN ? mem2 : nullptr;
        di.bufferMemoryBarrierCount = bufN;
        di.pBufferMemoryBarriers    = bufN ? buf2 : nullptr;
        di.imageMemoryBarrierCount  = imgN;
        di.pImageMemoryBarriers     = imgN ? img2 : nullptr;
        vkCmdPipelineBarrier2(cmd, &di);
        return;
    }

    // Legacy fallback: pack into one vkCmdPipelineBarrier call.
    // 64-bit stage / access values are truncated to 32-bit; bits unique to
    // sync2 (e.g., NONE = 0 alone) must not appear here. All current call
    // sites use stages/accesses that exist in both APIs.
    VkMemoryBarrier        mem1[16];
    VkBufferMemoryBarrier  buf1[16];
    VkImageMemoryBarrier   img1[16];
    VkPipelineStageFlags srcStageAgg = 0;
    VkPipelineStageFlags dstStageAgg = 0;
    const uint32_t memN = static_cast<uint32_t>(memory.size());
    const uint32_t bufN = static_cast<uint32_t>(buffers.size());
    const uint32_t imgN = static_cast<uint32_t>(images.size());
    for (uint32_t i = 0; i < memN; ++i) {
        mem1[i] = VkMemoryBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        mem1[i].srcAccessMask = static_cast<VkAccessFlags>(memory[i].srcAccess);
        mem1[i].dstAccessMask = static_cast<VkAccessFlags>(memory[i].dstAccess);
        srcStageAgg |= static_cast<VkPipelineStageFlags>(memory[i].srcStage);
        dstStageAgg |= static_cast<VkPipelineStageFlags>(memory[i].dstStage);
    }
    for (uint32_t i = 0; i < bufN; ++i) {
        buf1[i] = VkBufferMemoryBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        buf1[i].srcAccessMask       = static_cast<VkAccessFlags>(buffers[i].srcAccess);
        buf1[i].dstAccessMask       = static_cast<VkAccessFlags>(buffers[i].dstAccess);
        buf1[i].srcQueueFamilyIndex = buffers[i].srcQueueFamily;
        buf1[i].dstQueueFamilyIndex = buffers[i].dstQueueFamily;
        buf1[i].buffer              = buffers[i].buffer;
        buf1[i].offset              = buffers[i].offset;
        buf1[i].size                = buffers[i].size;
        srcStageAgg |= static_cast<VkPipelineStageFlags>(buffers[i].srcStage);
        dstStageAgg |= static_cast<VkPipelineStageFlags>(buffers[i].dstStage);
    }
    for (uint32_t i = 0; i < imgN; ++i) {
        img1[i] = VkImageMemoryBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        img1[i].srcAccessMask       = static_cast<VkAccessFlags>(images[i].srcAccess);
        img1[i].dstAccessMask       = static_cast<VkAccessFlags>(images[i].dstAccess);
        img1[i].oldLayout           = images[i].oldLayout;
        img1[i].newLayout           = images[i].newLayout;
        img1[i].srcQueueFamilyIndex = images[i].srcQueueFamily;
        img1[i].dstQueueFamilyIndex = images[i].dstQueueFamily;
        img1[i].image               = images[i].image;
        img1[i].subresourceRange    = images[i].range;
        srcStageAgg |= static_cast<VkPipelineStageFlags>(images[i].srcStage);
        dstStageAgg |= static_cast<VkPipelineStageFlags>(images[i].dstStage);
    }
    // vkCmdPipelineBarrier requires non-zero stage masks.
    if (srcStageAgg == 0) srcStageAgg = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    if (dstStageAgg == 0) dstStageAgg = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    vkCmdPipelineBarrier(cmd, srcStageAgg, dstStageAgg, 0,
                          memN, memN ? mem1 : nullptr,
                          bufN, bufN ? buf1 : nullptr,
                          imgN, imgN ? img1 : nullptr);
}

// Single-shot convenience wrappers around recordBatch.
inline void recordBuffer(const VulkanContext& ctx, VkCommandBuffer cmd, const BufferBarrier& b) {
    recordBatch(ctx, cmd, {}, std::span<const BufferBarrier>{&b, 1}, {});
}

inline void recordImage(const VulkanContext& ctx, VkCommandBuffer cmd, const ImageBarrier& b) {
    recordBatch(ctx, cmd, {}, {}, std::span<const ImageBarrier>{&b, 1});
}

}  // namespace barrier
