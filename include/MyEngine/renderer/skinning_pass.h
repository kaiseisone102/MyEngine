#pragma once
// =============================================================================
// skinning_pass.h - Phase 2G: batched compute linear-blend skinning pre-pass.
// =============================================================================
// Engine compute pass that skins EVERY skinned vertex of EVERY skinned instance
// this frame in ONE dispatch (skinning.comp). Fully BDA-driven (no descriptor
// sets), same recipe as CullingPass. Reads the SkinInstance[] table
// (SkinInstancePool) + each instance's original block vertices + bone matrices
// (all via BDA), writes the deinterleaved skinned position + normal streams
// (SkinnedVertexPool).
//
// 2G-1: dispatched and producing the streams, but the draw path still uses the
// legacy vertex-shader skinning, so this is a rendering no-op. 2G-2 switches the
// draws to a passthrough vertex shader that reads these streams.
// =============================================================================
#include <vulkan/vulkan.h>

#include <string>

#include "renderer/vk_unique.h"

class VulkanContext;
class SkinInstancePool;

namespace myengine::renderer {

class SkinnedVertexPool;

class SkinningPass {
   public:
    struct InitInfo {
        VulkanContext* ctx = nullptr;
        std::string shaderDir;
    };

    void init(const InitInfo& info);
    void shutdown();

    // One batched dispatch: skins `totalVertexCount` vertices across all
    // instances in `instances`, writing into `verts`. Records a
    // COMPUTE -> VERTEX/SHADER_READ buffer barrier on the output streams so the
    // draw passes that follow see the skinned data.
    void execute(VkCommandBuffer cmd, uint32_t frameIndex,
                 const SkinnedVertexPool& verts, const SkinInstancePool& instances,
                 uint32_t totalVertexCount);

   private:
    VulkanContext* ctx_ = nullptr;
    VkUnique<VkPipelineLayout> layout_;
    VkUnique<VkPipeline> pipeline_;
};

}  // namespace myengine::renderer
