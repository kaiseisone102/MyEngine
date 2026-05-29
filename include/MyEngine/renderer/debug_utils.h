#pragma once
// =============================================================================
// debug_utils.h - VK_EXT_debug_utils helpers (object names + cmd labels)
// =============================================================================
// Modern Vulkan profilers (RenderDoc, NVIDIA Nsight Graphics, AMD Radeon GPU
// Profiler) read VK_EXT_debug_utils object names and command-buffer labels
// to organise their capture timeline. Without these, every draw/dispatch
// shows up as "unnamed N" and the capture is hard to read.
//
// Usage:
//   dbg::setName(ctx, buffer, "MaterialRegistry SSBO");
//   dbg::setName(ctx, image,  "HDR target");
//
//   // Region label around a pass (recommended via the RAII helper below):
//   {
//       DBG_LABEL(cmd, "HiZ build");
//       hizPass_.execute(...);
//   }
//
//   // One-shot inline marker:
//   dbg::insertLabel(cmd, "frame end");
//
// All calls are no-ops if VK_EXT_debug_utils is not enabled (it is enabled
// in debug builds together with the validation layer); the proc-address
// loads happen lazily on first call and cache the result in a static.
// =============================================================================
#include <vulkan/vulkan.h>

#include <cstdint>

class VulkanContext;

namespace dbg {

// Set a human-readable name on a Vulkan handle. Profilers display this name.
void setObjectName(const VulkanContext& ctx, VkObjectType type, uint64_t handle,
                   const char* name);

// Convenience overloads for the common handle types.
inline void setName(const VulkanContext& ctx, VkBuffer h, const char* name) {
    setObjectName(ctx, VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(h), name);
}
inline void setName(const VulkanContext& ctx, VkImage h, const char* name) {
    setObjectName(ctx, VK_OBJECT_TYPE_IMAGE, reinterpret_cast<uint64_t>(h), name);
}
inline void setName(const VulkanContext& ctx, VkImageView h, const char* name) {
    setObjectName(ctx, VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<uint64_t>(h), name);
}
inline void setName(const VulkanContext& ctx, VkSampler h, const char* name) {
    setObjectName(ctx, VK_OBJECT_TYPE_SAMPLER, reinterpret_cast<uint64_t>(h), name);
}
inline void setName(const VulkanContext& ctx, VkPipeline h, const char* name) {
    setObjectName(ctx, VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<uint64_t>(h), name);
}
inline void setName(const VulkanContext& ctx, VkPipelineLayout h, const char* name) {
    setObjectName(ctx, VK_OBJECT_TYPE_PIPELINE_LAYOUT, reinterpret_cast<uint64_t>(h), name);
}
inline void setName(const VulkanContext& ctx, VkDescriptorSetLayout h, const char* name) {
    setObjectName(ctx, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                  reinterpret_cast<uint64_t>(h), name);
}
inline void setName(const VulkanContext& ctx, VkDescriptorSet h, const char* name) {
    setObjectName(ctx, VK_OBJECT_TYPE_DESCRIPTOR_SET, reinterpret_cast<uint64_t>(h),
                  name);
}
inline void setName(const VulkanContext& ctx, VkSemaphore h, const char* name) {
    setObjectName(ctx, VK_OBJECT_TYPE_SEMAPHORE, reinterpret_cast<uint64_t>(h), name);
}
inline void setName(const VulkanContext& ctx, VkFence h, const char* name) {
    setObjectName(ctx, VK_OBJECT_TYPE_FENCE, reinterpret_cast<uint64_t>(h), name);
}
inline void setName(const VulkanContext& ctx, VkCommandBuffer h, const char* name) {
    setObjectName(ctx, VK_OBJECT_TYPE_COMMAND_BUFFER, reinterpret_cast<uint64_t>(h),
                  name);
}

// Push a named region into the cmd buffer. Profilers render this as a folder
// containing every draw/dispatch between begin and end. Pair with endLabel().
void beginLabel(VkCommandBuffer cmd, const char* name, const float color[4] = nullptr);
void endLabel(VkCommandBuffer cmd);

// One-shot label that does not nest (a tick mark on the timeline).
void insertLabel(VkCommandBuffer cmd, const char* name,
                 const float color[4] = nullptr);

// RAII helper: pairs begin/end on scope entry/exit. Use through DBG_LABEL.
class ScopedLabel {
   public:
    ScopedLabel(VkCommandBuffer cmd, const char* name) : cmd_(cmd) {
        beginLabel(cmd_, name);
    }
    ~ScopedLabel() { endLabel(cmd_); }
    ScopedLabel(const ScopedLabel&) = delete;
    ScopedLabel& operator=(const ScopedLabel&) = delete;

   private:
    VkCommandBuffer cmd_;
};

}  // namespace dbg

// Stamp a label region into the current scope.
#define DBG_LABEL_CONCAT_IMPL(a, b) a##b
#define DBG_LABEL_CONCAT(a, b) DBG_LABEL_CONCAT_IMPL(a, b)
#define DBG_LABEL(cmd, name) \
    ::dbg::ScopedLabel DBG_LABEL_CONCAT(_dbg_scope_, __LINE__){cmd, name}
