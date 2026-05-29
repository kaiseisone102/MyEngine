#pragma once
// =============================================================================
// GpuSkinning -- S: compute-shader skinning + persistent skinned VB receptacle
// =============================================================================
// Today: Animator::computeSkinMatrices on the CPU writes per-bone matrices,
// SkinBufferPool uploads them to a per-frame BDA SSBO, vertex shaders apply
// the skinning per vertex. At ~1 skeletal entity in Stage 1-1 the CPU cost
// is negligible.
//
// At scale (1000+ animated characters, large open-world fight scenes), the
// per-vertex skinning math runs 3x per skeleton: shadow, main pass, and
// reflection all transform the same vertices independently. Modern engines
// solve this by running a single compute pass that writes a persistent
// "skinned vertex buffer" once per frame; every downstream draw binds the
// post-skinned buffer with no skinning math in the vertex shader.
//
// Receptacle shape:
//   - skinnedVB_[FIF] : per-frame VmaBuffer at vertexCount * sizeof(Vertex)
//     for each skinned mesh. The compute pass writes here.
//   - dispatch(meshHandle, animatorOutput) : kicks one workgroup per 64
//     vertices on the async-compute queue (V receptacle integrates with
//     M's AsyncComputeContext for overlap with the previous main pass).
//   - downstream draws bind skinnedVB_ instead of the original Mesh VB.
//
// Per-Phase activation order:
//   1. Add skinning.comp -- writes Vertex.pos / Vertex.normal from BDA
//      pointers to original vertex buffer + bone matrices.
//   2. Allocate skinnedVB_ at model load (size = mesh vertexCount).
//   3. Insert a compute dispatch into pass_chain BEFORE shadow / main /
//      reflection so all three see the post-skinned buffer.
//   4. Switch triangle_skinned.vert to a non-skinning variant when reading
//      from the skinnedVB_ (just a passthrough).
//
// Foundations \xc2\xa78.1 dynamic-capacity composes here: SkinBufferPool already
// grows (F3); per-mesh skinnedVB_ can grow with the same VmaBuffer + dq_
// pattern when meshes get larger.
// =============================================================================
#include "renderer/vma_buffer.h"

#include <array>
#include <cstdint>

#include "renderer/frame_sync.h"

class VulkanContext;

namespace myengine::renderer {

class GpuSkinningTarget {
   public:
    // Allocate a per-frame skinnedVB_ pair sized for `vertexCount` post-
    // skinning vertices.
    void init(VulkanContext* /*ctx*/, uint32_t /*vertexCount*/) {}
    void shutdown() {
        for (VmaBuffer& vb : skinnedVB_) vb.reset();
    }

    VkBuffer buffer(uint32_t frameIndex) const {
        return frameIndex < skinnedVB_.size() ? skinnedVB_[frameIndex].buffer()
                                              : VK_NULL_HANDLE;
    }

   private:
    std::array<VmaBuffer, FrameSync::MAX_FRAMES_IN_FLIGHT> skinnedVB_{};
};

}  // namespace myengine::renderer
