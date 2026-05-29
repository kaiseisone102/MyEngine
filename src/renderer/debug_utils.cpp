// src/renderer/debug_utils.cpp
#include "renderer/debug_utils.h"

#include "renderer/vulkan_context.h"

namespace dbg {
namespace {

// Lazy-loaded function pointers. The first call to each helper queries the
// instance for the proc address and caches it; subsequent calls go straight
// through the function pointer. If VK_EXT_debug_utils is not enabled, the
// proc address comes back null and every helper becomes a no-op.
PFN_vkSetDebugUtilsObjectNameEXT g_pfnSetObjectName = nullptr;
PFN_vkCmdBeginDebugUtilsLabelEXT g_pfnBeginLabel = nullptr;
PFN_vkCmdEndDebugUtilsLabelEXT   g_pfnEndLabel = nullptr;
PFN_vkCmdInsertDebugUtilsLabelEXT g_pfnInsertLabel = nullptr;
bool g_loaded = false;

void loadFromInstance(VkInstance instance) {
    if (g_loaded || instance == VK_NULL_HANDLE) return;
    g_pfnSetObjectName = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
        vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT"));
    g_pfnBeginLabel = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT"));
    g_pfnEndLabel = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT"));
    g_pfnInsertLabel = reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(instance, "vkCmdInsertDebugUtilsLabelEXT"));
    g_loaded = true;
}

}  // namespace

void setObjectName(const VulkanContext& ctx, VkObjectType type, uint64_t handle,
                   const char* name) {
    if (handle == 0 || name == nullptr) return;
    loadFromInstance(ctx.instance());
    if (!g_pfnSetObjectName) return;

    VkDebugUtilsObjectNameInfoEXT info{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    info.objectType = type;
    info.objectHandle = handle;
    info.pObjectName = name;
    g_pfnSetObjectName(ctx.device(), &info);
}

void beginLabel(VkCommandBuffer cmd, const char* name, const float color[4]) {
    if (cmd == VK_NULL_HANDLE || name == nullptr || !g_pfnBeginLabel) return;
    VkDebugUtilsLabelEXT label{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
    label.pLabelName = name;
    if (color) {
        label.color[0] = color[0];
        label.color[1] = color[1];
        label.color[2] = color[2];
        label.color[3] = color[3];
    }
    g_pfnBeginLabel(cmd, &label);
}

void endLabel(VkCommandBuffer cmd) {
    if (cmd == VK_NULL_HANDLE || !g_pfnEndLabel) return;
    g_pfnEndLabel(cmd);
}

void insertLabel(VkCommandBuffer cmd, const char* name, const float color[4]) {
    if (cmd == VK_NULL_HANDLE || name == nullptr || !g_pfnInsertLabel) return;
    VkDebugUtilsLabelEXT label{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
    label.pLabelName = name;
    if (color) {
        label.color[0] = color[0];
        label.color[1] = color[1];
        label.color[2] = color[2];
        label.color[3] = color[3];
    }
    g_pfnInsertLabel(cmd, &label);
}

}  // namespace dbg
