#pragma once
// =============================================================================
// PersistentObjectBuffer -- H: replace per-frame CPU rebuild of CullObject /
// DrawData with delta updates (Foundations \xc2\xa74)
// =============================================================================
// static_cull_build.h's build() walks every prop every frame, fills a CPU
// vector of CullObject + DrawData, and uploads them to the GPU. At the
// current ~67 props this is invisible; at the open-world target (tens of
// thousands of static props per chunk, multiple chunks resident) it makes
// the CPU the bottleneck of GPU-driven rendering -- ironic.
//
// The fix is the modern open-world standard: keep CullObject + DrawData on
// the GPU persistently, and only upload deltas (chunk load/unload, object
// transform changes for animated props). The GPU side then walks the live
// list every frame for cull / draw without the CPU rebuild step.
//
// Receptacle shape (Phase activation lands when the prop count justifies
// the complexity; today the CPU rebuild is still cheap):
//   - objects_   : VmaBuffer (device-local, grows by F-pattern) holding
//                  CullObject[]. One slot per drawable object that lives
//                  across frames.
//   - freeSlots_ : the same release/reuse free-list pattern as G.
//   - dirtyRanges_ : per-frame list of (slotStart, slotEnd) ranges that
//                  changed since the last upload. The upload step copies
//                  only those ranges via vkCmdCopyBuffer (transfer queue
//                  for overlap with graphics -- C).
//   - The cull compute reads objects_ directly; visBuf and compactCmd are
//     unchanged from PART4 4-前-3.
//
// Per-Phase activation order (typically right before Phase 2F when chunk
// streaming makes the rebuild noticeable):
//   1. Move CullObject + DrawData backing store from per-frame staging to
//      this persistent buffer.
//   2. Wire static_cull_build callers (asset_registry, stage_registry) to
//      registerObject() / releaseObject() on chunk load/unload.
//   3. Replace per-frame build() with an update-dirty pass.
//   4. (Optional) spatial subdivision -- bucket by chunk grid so cull's
//      input is "only the chunks intersecting the camera frustum" rather
//      than every loaded object. Foundations \xc2\xa74 cited Life is Feudal's
//      quadtree as the reference design here.
//
// Today this header documents the shape; the buffer stays unallocated and
// every caller goes through the existing per-frame builder.
// =============================================================================
#include "renderer/vma_buffer.h"

#include <cstdint>
#include <vector>

class VulkanContext;
class DeletionQueue;

namespace myengine::renderer {

class PersistentObjectBuffer {
   public:
    void init(VulkanContext* /*ctx*/, DeletionQueue* /*dq*/, uint32_t /*initialCapacity*/) {}
    void shutdown() {
        objects_.reset();
        freeSlots_.clear();
        dirtyRanges_.clear();
    }

    // Reserve a permanent slot for a new object. The caller is expected to
    // write the CullObject via slotPointer(idx) and add a dirty range.
    uint32_t reserve() {
        if (!freeSlots_.empty()) {
            uint32_t s = freeSlots_.back();
            freeSlots_.pop_back();
            return s;
        }
        return nextIndex_++;
    }
    void release(uint32_t slot) { freeSlots_.push_back(slot); }
    void markDirty(uint32_t slotStart, uint32_t slotEnd) {
        dirtyRanges_.push_back({slotStart, slotEnd});
    }

    VkBuffer buffer() const noexcept { return objects_.buffer(); }
    VkDeviceAddress deviceAddress() const noexcept { return objects_.deviceAddress(); }

   private:
    struct DirtyRange { uint32_t first; uint32_t last; };

    VmaBuffer objects_;
    std::vector<uint32_t> freeSlots_;
    std::vector<DirtyRange> dirtyRanges_;
    uint32_t nextIndex_ = 0;
};

}  // namespace myengine::renderer
