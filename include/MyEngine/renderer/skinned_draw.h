#pragma once
// =============================================================================
// skinned_draw.h - Phase 2G: prepared compute-skinned draw record.
// =============================================================================
// pass_chain builds ONE list of PreparedSkinnedDraw per frame (in the 2G
// skinning block, alongside SkinInstance + SkinnedDrawData), and shadow / main
// / reflection all iterate the SAME list. The slot baked into each record is
// the SkinnedDrawData index (== firstInstance), so the three passes never
// re-derive slots by re-walking the scene list -- that order-coupling was the
// PART3b cursor pitfall (Work_Protocol §5d). One build, shared by all.
//
// The draw binds the ORIGINAL GeometryBuffer block (for the skin-invariant
// uv/color vertex attributes) and pulls the skinned position + normal from the
// SkinnedVertexPool streams via BDA in the passthrough vertex shader.
// =============================================================================
#include <cstdint>

namespace skinned {

struct PreparedSkinnedDraw {
    uint32_t blockIndex = 0;    // GeometryBuffer block to bind (original vertices)
    uint32_t indexCount = 0;
    uint32_t firstIndex = 0;
    int32_t  vertexOffset = 0;  // also baked into SkinnedDrawData.srcVertexBase
    uint32_t slot = 0;          // SkinnedDrawData index == firstInstance
};

}  // namespace skinned
