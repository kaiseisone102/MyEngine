// src/renderer/indirect_exec.cpp
// =============================================================================
// indirect_exec.cpp - PART4 4-前-4: capability-driven indirect draw wrapper.
// =============================================================================
// See indirect_exec.h for the contract. The point of this file is that
// CullingPass / main_pass do not learn about which Vulkan entry point is
// actually used; they hand over the call info, this file picks.
//
// DGC code path: Today this file falls through to IndirectCount on every
// device. PART4 4d will fill in vkCmdExecuteGeneratedCommandsEXT here,
// driven by a VkIndirectCommandsLayoutEXT + VkIndirectExecutionSetEXT built
// at engine init.
// =============================================================================
#include "renderer/indirect_exec.h"

#include "renderer/vulkan_context.h"

namespace indirect_exec {

Mode preferredMode(const VulkanContext& ctx) {
    if (ctx.deviceGeneratedCommands()) return Mode::DGC;
    if (ctx.drawIndirectCount())       return Mode::IndirectCount;
    return Mode::Legacy;
}

void recordDrawIndexedIndirectCount(const VulkanContext& ctx, VkCommandBuffer cmd,
                                     const DrawIndexedIndirectCountInfo& info) {
    if (info.maxCount == 0 || info.commandBuffer == VK_NULL_HANDLE) return;

    switch (preferredMode(ctx)) {
        case Mode::DGC:
            // PART4 4d: VK_EXT_device_generated_commands wiring lands here.
            // The DGC path would build a VkGeneratedCommandsInfoEXT referring
            // to a prebuilt indirectCommandsLayout + executionSet, then call
            // vkCmdExecuteGeneratedCommandsEXT. Today we drop through to
            // IndirectCount so devices that advertise DGC still draw correctly.
            [[fallthrough]];

        case Mode::IndirectCount:
            if (info.countBuffer != VK_NULL_HANDLE) {
                vkCmdDrawIndexedIndirectCount(cmd, info.commandBuffer, info.commandOffset,
                                               info.countBuffer, info.countOffset,
                                               info.maxCount, info.stride);
                return;
            }
            // No countBuffer supplied -> fall through to legacy.
            [[fallthrough]];

        case Mode::Legacy:
            // Walk every slot; instanceCount==0 entries are GPU-skipped.
            // multiDrawIndirect lets a single call iterate maxCount slots; if
            // the device lacks it, issue maxCount single-draw calls.
            if (ctx.multiDrawIndirect()) {
                vkCmdDrawIndexedIndirect(cmd, info.commandBuffer, info.commandOffset,
                                          info.maxCount, info.stride);
            } else {
                for (uint32_t i = 0; i < info.maxCount; ++i) {
                    vkCmdDrawIndexedIndirect(cmd, info.commandBuffer,
                                              info.commandOffset +
                                                  static_cast<VkDeviceSize>(i) * info.stride,
                                              1, info.stride);
                }
            }
            return;
    }
}

}  // namespace indirect_exec
