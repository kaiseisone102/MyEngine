#pragma once
// =============================================================================
// DecalPass -- Y: screen-space projected decal receptacle
// =============================================================================
// Decals (bullet holes, footprints, blood splatter, posters, signs) are an
// open-world staple. Modern engines render them as screen-space projected
// boxes that test against the depth buffer and stamp the decal texture
// where the box volume intersects the depth surface.
//
// Receptacle shape:
//   - DecalInstance { vec4 worldPos; vec4 size; uvec2 albedoIdx; ... }
//     fits in a per-frame SSBO that the decal compute / fragment pass
//     iterates.
//   - The decal pass runs AFTER the opaque main pass (depth + MRT GBuffer
//     are stable) and BEFORE the transparency pass / postprocess. It
//     samples MRT depth + normal (4a-2 already exposes both) plus the
//     decal's albedo from the bindless texture array.
//   - With dynamic_rendering_local_read (4d M3 receptacle, P620 supports
//     it), the decal pass can read MRT.depth / MRT.normal without an
//     intermediate barrier -- the modern tile-friendly form.
//
// Variants the receptacle covers (all per-Phase):
//   - Static decals (poster, sign)             : permanent SSBO entry.
//   - Dynamic decals (bullet hit)              : freelist + ring buffer,
//                                                evict oldest.
//   - Deferred decals (modify GBuffer normal)  : same compositor but
//                                                writes back into the
//                                                normal RT for downstream
//                                                lighting.
//   - Mesh decals (large, projected on terrain): separate Phase, requires
//                                                terrain bucket (2F).
//
// Receptacle-level today: an empty SSBO + DecalInstance struct that the
// fragment shader will iterate per-pixel once the pass is activated.
// =============================================================================
#include "renderer/vma_buffer.h"

#include <cstdint>

#include <glm/glm.hpp>

class VulkanContext;
class DeletionQueue;

namespace myengine::renderer {

struct DecalInstance {
    glm::vec4 worldPos;          // .w = orientation packed
    glm::vec4 halfSize;          // .w = lifetime / fade
    glm::uvec4 albedoNormalIdx;  // bindless texture indices
};

class DecalPass {
   public:
    void init(VulkanContext* /*ctx*/, DeletionQueue* /*dq*/) {}
    void shutdown() {
        decals_.reset();
    }

    uint32_t spawn(const DecalInstance& /*decal*/) {
        // Phase: append to live decals_, return a handle the caller can
        // release later. For now this is a no-op.
        return 0;
    }
    void release(uint32_t /*handle*/) {}

   private:
    VmaBuffer decals_;
    uint32_t count_ = 0;
};

}  // namespace myengine::renderer
