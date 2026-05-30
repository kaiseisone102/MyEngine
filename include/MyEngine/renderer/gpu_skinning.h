#pragma once
// =============================================================================
// SkinnedVertexPool -- Phase 2G: per-instance compute-skinned vertex pool.
// =============================================================================
// Phase 2G replaces vertex-shader skinning (re-run per pass = 3-4x) with a
// single compute pre-pass (skinning.comp) that writes skinned vertices once;
// shadow / main / reflection then read them as a static mesh.
//
// DESIGN (Roadmap Phase 2G, audited 2026-05-30 -- no compromises):
//   - PER-INSTANCE, not per-mesh. The old gpu_skinning.h receptacle held one
//     skinnedVB per mesh, which breaks the moment two entities share a Model
//     with different poses (multiple enemies). reserve() hands out a distinct
//     vertex range per skinned INSTANCE per frame.
//   - DEINTERLEAVED streams. Skinning only changes position + normal, so only
//     those are written here; uv / color / material are skin-invariant and stay
//     in the original GeometryBuffer block stream (NOT copied -- saves VRAM on
//     the 2 GB P620). Output streams are tightly packed:
//       * position : 3 x fp32 per vertex (12 B). Full precision for skinning +
//                    motion-vector delta accuracy.
//       * normal   : octahedral 2x SNORM16 packed into one uint per vertex
//                    (4 B vs 12 B). skinning.comp does the pack; the passthrough
//                    vertex shader unpacks (2G-2).
//   - POSITION PING-PONG for motion vectors. With MAX_FRAMES_IN_FLIGHT == 2 the
//     per-frame position buffers double as current (frameIndex) and previous
//     (the other index): skinned motion vectors come for free by reading the
//     other buffer (2G-3 wires this into the main pass; the layout is ready
//     now). normal only needs the current frame.
//
// Buffers are device-local STORAGE | VERTEX_BUFFER | BDA | TRANSFER_DST so the
// compute pass writes them (STORAGE/BDA) and the draw reads them either as a
// bound vertex buffer or via BDA in the passthrough vertex shader (2G-2 picks).
//
// 2G-1 scope: allocate + expose addresses + reserve(); the SkinningPass fills
// them, but the draw path still uses legacy skinning, so the streams are
// produced and unused (a true no-op for rendering). 2G-2 switches the draws.
//
// Capacity is a STARTING size, not a cap (Foundations 8.1 / Work_Protocol 5f):
// reserve() grows every ring buffer via the DeletionQueue when it overflows.
// =============================================================================
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>

#include "renderer/deletion_queue.h"
#include "renderer/frame_sync.h"
#include "renderer/vma_buffer.h"
#include "renderer/vulkan_context.h"

namespace myengine::renderer {

class SkinnedVertexPool {
   public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = FrameSync::MAX_FRAMES_IN_FLIGHT;
    // Starting vertex capacity across all skinned instances in one frame.
    // Grows on demand. ~ a few skinned characters * a few thousand verts.
    static constexpr uint32_t INITIAL_CAPACITY = 65536;

    void init(VulkanContext* ctx, DeletionQueue* deletionQueue) {
        if (!ctx) throw std::runtime_error("SkinnedVertexPool::init: invalid ctx");
        if (!deletionQueue)
            throw std::runtime_error("SkinnedVertexPool::init: deletionQueue is null");
        ctx_ = ctx;
        dq_ = deletionQueue;
        createBuffers(INITIAL_CAPACITY);
    }

    void shutdown() {
        if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            posBuffers_[i].reset();
            normalBuffers_[i].reset();
            cursor_[i] = 0;
        }
        capacity_ = 0;
        ctx_ = nullptr;
        dq_ = nullptr;
    }

    // Reset this frame's allocation cursor before the SkinningPass fills it.
    void beginFrame(uint32_t frameIndex) {
        if (frameIndex >= MAX_FRAMES_IN_FLIGHT) return;
        cursor_[frameIndex] = 0;
    }

    // Reserve `vertexCount` contiguous skinned-vertex slots for one instance;
    // returns the base slot (= dstVertexBase for skinning.comp), or UINT32_MAX
    // on an invalid frame. Grows the pool if needed.
    uint32_t reserve(uint32_t frameIndex, uint32_t vertexCount) {
        if (frameIndex >= MAX_FRAMES_IN_FLIGHT) return UINT32_MAX;
        const uint32_t base = cursor_[frameIndex];
        const uint32_t need = base + vertexCount;
        if (need > capacity_) ensureCapacity(need);
        cursor_[frameIndex] = need;
        return base;
    }

    // Grow every ring buffer to at least `need` vertices. Old buffers go to the
    // DeletionQueue (freed after in-flight frames). No data copy: skinned
    // vertices are rewritten every frame by the compute pass.
    void ensureCapacity(uint32_t need) {
        if (need <= capacity_) return;
        const uint32_t newCap = std::max(need, capacity_ * 2u);
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (posBuffers_[i]) {
                dq_->enqueueBuffer(posBuffers_[i].buffer(), posBuffers_[i].allocation());
                posBuffers_[i].release();
            }
            if (normalBuffers_[i]) {
                dq_->enqueueBuffer(normalBuffers_[i].buffer(), normalBuffers_[i].allocation());
                normalBuffers_[i].release();
            }
        }
        createBuffers(newCap);
    }

    // Current-frame skinned streams (compute writes, draw reads).
    VkDeviceAddress posAddress(uint32_t frameIndex) const {
        return (frameIndex < MAX_FRAMES_IN_FLIGHT) ? posBuffers_[frameIndex].deviceAddress() : 0;
    }
    VkDeviceAddress normalAddress(uint32_t frameIndex) const {
        return (frameIndex < MAX_FRAMES_IN_FLIGHT) ? normalBuffers_[frameIndex].deviceAddress() : 0;
    }
    VkBuffer posBuffer(uint32_t frameIndex) const {
        return (frameIndex < MAX_FRAMES_IN_FLIGHT) ? posBuffers_[frameIndex].buffer() : VK_NULL_HANDLE;
    }
    VkBuffer normalBuffer(uint32_t frameIndex) const {
        return (frameIndex < MAX_FRAMES_IN_FLIGHT) ? normalBuffers_[frameIndex].buffer() : VK_NULL_HANDLE;
    }

    // Previous-frame position stream for motion vectors (2G-3). With FIF == 2
    // this is simply the other ring slot.
    VkDeviceAddress prevPosAddress(uint32_t frameIndex) const {
        if (frameIndex >= MAX_FRAMES_IN_FLIGHT) return 0;
        const uint32_t prev = (frameIndex + MAX_FRAMES_IN_FLIGHT - 1u) % MAX_FRAMES_IN_FLIGHT;
        return posBuffers_[prev].deviceAddress();
    }

    uint32_t capacity() const noexcept { return capacity_; }
    uint32_t usedThisFrame(uint32_t frameIndex) const {
        return (frameIndex < MAX_FRAMES_IN_FLIGHT) ? cursor_[frameIndex] : 0;
    }

   private:
    void createBuffers(uint32_t capacity) {
        capacity_ = capacity;
        // position: 3 x fp32 / vertex; normal: 1 x uint (oct16) / vertex.
        const VkDeviceSize posSz = static_cast<VkDeviceSize>(capacity) * sizeof(float) * 3u;
        const VkDeviceSize nrmSz = static_cast<VkDeviceSize>(capacity) * sizeof(uint32_t);
        const VkBufferUsageFlags usage =
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            posBuffers_[i] = VmaBuffer::createDeviceLocal(ctx_, posSz, usage);
            normalBuffers_[i] = VmaBuffer::createDeviceLocal(ctx_, nrmSz, usage);
            cursor_[i] = 0;
        }
    }

    VulkanContext* ctx_ = nullptr;
    DeletionQueue* dq_ = nullptr;
    uint32_t capacity_ = 0;
    std::array<VmaBuffer, MAX_FRAMES_IN_FLIGHT> posBuffers_{};
    std::array<VmaBuffer, MAX_FRAMES_IN_FLIGHT> normalBuffers_{};
    std::array<uint32_t, MAX_FRAMES_IN_FLIGHT> cursor_{};
};

}  // namespace myengine::renderer
