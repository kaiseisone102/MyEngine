// include/MyEngine/renderer/indirect_exec.h
#pragma once
// =============================================================================
// indirect_exec.h - PART4 4-前-4: indirect draw execution wrapper
// =============================================================================
// One place to record a per-block indirect draw, with capability-based picking
// across the three implementation strategies modern Vulkan offers:
//
//   * DGC          (VK_EXT_device_generated_commands)
//                  GPU builds and executes the entire command stream including
//                  pipeline / push constant / shader-object switches. The most
//                  scalable Vulkan equivalent of D3D12 GPU Work Graphs.
//                  Implementation lands in PART4 4d; today the path falls
//                  through to IndirectCount on every device (P620 lacks DGC).
//
//   * IndirectCount (VK_KHR_draw_indirect_count, Vulkan 1.2 core)
//                  GPU walks only `*countBuffer` entries instead of the full
//                  `maxCount` slots, so empty-cmd full-walk goes away once the
//                  scene scales. CullingPass.scan_compact writes the per-block
//                  visible count, this path reads it.
//
//   * Legacy       (vkCmdDrawIndexedIndirect, every device)
//                  PART4 4-前-3 behaviour: walk every slot and let
//                  instanceCount==0 GPU-skip the empty draws. Slower at scale
//                  but the safety net when neither modern feature is enabled.
//
// main_pass records its per-BlockRange draws by handing recordDrawIndexedIndi-
// rectCount() this info struct; the wrapper picks the path. Adding the real
// DGC code inside the wrapper later is a contained change - main_pass and
// CullingPass do not learn about it.
// =============================================================================
#include <vulkan/vulkan.h>

#include <cstdint>

class VulkanContext;

namespace indirect_exec {

enum class Mode {
    DGC,            // VK_EXT_device_generated_commands (preferred when supported + wired)
    IndirectCount,  // vkCmdDrawIndexedIndirectCount (Vulkan 1.2 core feature)
    Legacy,         // vkCmdDrawIndexedIndirect with maxCount loop fallback
};

// Pick the preferred mode for `ctx`. Today DGC falls through to IndirectCount
// because the DGC wiring lands in PART4 4d; this function is still the single
// place for the decision so callers don't sprinkle capability checks.
Mode preferredMode(const VulkanContext& ctx);

// Input to a single per-block indirect-count draw. Match
// vkCmdDrawIndexedIndirectCount's parameter list so the IndirectCount path is
// a direct passthrough. `countBuffer` holds the visible count written by
// scan_compact (Pass B); `maxCount` is the per-block compact range capacity.
struct DrawIndexedIndirectCountInfo {
    VkBuffer     commandBuffer  = VK_NULL_HANDLE;
    VkDeviceSize commandOffset  = 0;
    VkBuffer     countBuffer    = VK_NULL_HANDLE;
    VkDeviceSize countOffset    = 0;
    uint32_t     maxCount       = 0;
    uint32_t     stride         = 0;
};

// Record one per-block indirect-count draw. cmd must already have the
// pipeline / descriptor set / vertex+index buffer bind for this block.
void recordDrawIndexedIndirectCount(const VulkanContext& ctx, VkCommandBuffer cmd,
                                     const DrawIndexedIndirectCountInfo& info);

}  // namespace indirect_exec
