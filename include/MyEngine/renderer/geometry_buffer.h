// include/MyEngine/renderer/geometry_buffer.h
#pragma once
// =============================================================================
// geometry_buffer.h - Phase 2B PART3a: consolidated geometry, multi-block.
// =============================================================================
// THE foundation for GPU-driven rendering. Instead of one vertex/index buffer
// per Mesh/SubMesh (which makes vkCmdDrawIndexedIndirect impossible), static
// geometry is consolidated into large device-local buffers. A draw becomes a
// (firstIndex, vertexOffset, indexCount) range -> exactly what indirect needs.
//
// UNBOUNDED / DEVELOPMENT-FIRST (Work_Protocol 5b/5c): capacity is NOT a fixed
// guess sized to the current scene. We hold a VECTOR OF BLOCKS; when every block
// is full, a NEW block is added (existing blocks are left in place -- no copy, no
// handle invalidation, no VRAM doubling). Assets can keep growing during dev and
// it never overflows. (A single 1M-vertex megabuffer was tried first and armor
// etc. overflowed at load -- see Work_Protocol 5b failure note.)
//
// SUB-ALLOCATION uses VMA's official virtual allocator (VmaVirtualBlock), not a
// hand-rolled free list. Each block owns a vtx + idx VmaVirtualBlock that track
// used/free ranges (in bytes) inside that block's buffers.
//
// ONE MESH STAYS IN ONE BLOCK: alloc() places a mesh's vertices AND indices in
// the same block, so a draw binds exactly one block (handle.blockIndex). vertex
// and index virtual blocks are grown together (a new block is added if either
// the vtx or idx allocation fails in all existing blocks).
//
// free() returns the ranges via the DeletionQueue (reused only after in-flight
// frames finish -- real free, no TODO fallback).
//
// Vertex format is the engine-wide `Vertex` (mesh.h); indices are uint32.
// =============================================================================
#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

#include "renderer/vma_buffer.h"
#include "renderer/mesh.h"   // Vertex

// VMA virtual-allocator forward decls (avoid including vma in this header).
VK_DEFINE_HANDLE(VmaVirtualBlock)
struct VmaVirtualAllocation_T;
typedef struct VmaVirtualAllocation_T* VmaVirtualAllocation;

class VulkanContext;
class ResourceFactory;
class DeletionQueue;

// A sub-range into one block's shared buffers. Everything a draw needs.
struct MeshHandle {
    uint32_t blockIndex = 0;     // which block (bind this block before drawing)
    uint32_t firstIndex = 0;     // index-buffer offset (in indices) within the block
    int32_t  vertexOffset = 0;   // vertex-buffer offset (in vertices), added to indices
    uint32_t indexCount = 0;     // indices to draw
    uint32_t vertexCount = 0;    // vertices owned (bookkeeping)
    // VMA virtual allocations, needed to free the ranges later.
    VmaVirtualAllocation vtxAlloc = nullptr;
    VmaVirtualAllocation idxAlloc = nullptr;
    bool valid() const { return indexCount > 0; }
};

class GeometryBuffer {
   public:
    // Per-block granularity (NOT an upper bound -- blocks are added on demand).
    // Vertex is ~76B: 512K verts ~= 39MB, 2M indices ~= 8MB per block.
    static constexpr uint32_t DEFAULT_BLOCK_VERTICES = 512u * 1024u;
    static constexpr uint32_t DEFAULT_BLOCK_INDICES  = 2u * 1024u * 1024u;

    void init(VulkanContext* ctx, ResourceFactory* resources, DeletionQueue* deletionQueue,
              uint32_t blockVertices = DEFAULT_BLOCK_VERTICES,
              uint32_t blockIndices = DEFAULT_BLOCK_INDICES);
    void shutdown();

    // Upload one mesh into some block (adding a block if needed). Returns an
    // invalid handle (indexCount==0) only on a hard allocation failure.
    MeshHandle alloc(const std::vector<Vertex>& vertices,
                     const std::vector<uint32_t>& indices);

    // Retire a handle: frees its ranges via the DeletionQueue.
    void free(const MeshHandle& handle);

    // Bind the vertex+index buffers of a specific block (called from Mesh/SubMesh
    // bind() using their handle's blockIndex). Draw then uses firstIndex/vertexOffset.
    void bindBlock(VkCommandBuffer cmd, uint32_t blockIndex) const;

    uint32_t blockCount() const { return static_cast<uint32_t>(blocks_.size()); }

   private:
    struct Block {
        VmaBuffer vbuf;                       // device-local VERTEX|TRANSFER_DST|BDA
        VmaBuffer ibuf;                       // device-local INDEX|TRANSFER_DST
        VmaVirtualBlock vtxVirt = nullptr;    // sub-alloc within vbuf (bytes)
        VmaVirtualBlock idxVirt = nullptr;    // sub-alloc within ibuf (bytes)
        uint32_t vtxCapacity = 0;             // in vertices
        uint32_t idxCapacity = 0;             // in indices
    };

    // Append a new block sized max(default, need). Returns its index.
    uint32_t addBlock(uint32_t needVertices, uint32_t needIndices);

    VulkanContext* ctx_ = nullptr;
    ResourceFactory* resources_ = nullptr;
    DeletionQueue* deletionQueue_ = nullptr;

    uint32_t blockVertices_ = 0;  // default per-block vertex granularity
    uint32_t blockIndices_ = 0;   // default per-block index granularity

    std::vector<Block> blocks_;
};