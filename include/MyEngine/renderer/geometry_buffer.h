// include/MyEngine/renderer/geometry_buffer.h
#pragma once
// =============================================================================
// geometry_buffer.h - Phase 2B PART3a: consolidated geometry megabuffer.
// =============================================================================
// THE foundation for GPU-driven rendering. Instead of one vertex/index buffer
// per Mesh/SubMesh (which makes vkCmdDrawIndexedIndirect impossible), ALL static
// geometry lives in two big device-local buffers. A draw is then just a
// (firstIndex, vertexOffset, indexCount) range into these shared buffers, which
// is exactly what VkDrawIndexedIndirectCommand needs. One bind for the whole
// scene; the GPU picks ranges via indirect commands (PART3c).
//
// DYNAMIC-SCENE READY (no rework for streaming): allocation is a first-fit
// sub-allocator over the megabuffers with a free list, NOT an append-only bump
// pointer. alloc()/free() can run at runtime as chunks stream in/out. free()
// routes the range back to the free list THROUGH the DeletionQueue, so a region
// is only reused after the GPU has finished any in-flight frame that referenced
// it (no GPU hazard, no TODO fallback).
//
// GROWTH: the megabuffers are fixed-capacity for now (set at init). Growing them
// (allocate larger + copy + retire old via DeletionQueue) is a later add when a
// real scene exceeds capacity; the free-list/handle API does not change when it
// is added, so this is not rework — just a capacity policy bolt-on.
//
// Vertex format is the engine-wide `Vertex` (mesh.h), shared by cube/grass/Model
// SubMeshes alike, so one megabuffer serves every static mesh. Indices are uint32.
// =============================================================================
#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

#include "renderer/vma_buffer.h"
#include "renderer/mesh.h"   // Vertex

class VulkanContext;
class ResourceFactory;
class DeletionQueue;

// A sub-range into the shared megabuffers. Everything a draw needs.
struct MeshHandle {
    uint32_t firstIndex = 0;     // index-buffer offset (in indices)
    int32_t  vertexOffset = 0;   // vertex-buffer offset (in vertices), added to index values
    uint32_t indexCount = 0;     // indices to draw
    uint32_t vertexCount = 0;    // vertices owned (for free-list bookkeeping)
    bool valid() const { return indexCount > 0; }
};

class GeometryBuffer {
   public:
    // Default capacities (Vertex is ~64B): 1M vertices (~64MB) + 4M indices (~16MB).
    // Comfortably covers the current scene; grow policy is a later bolt-on.
    static constexpr uint32_t DEFAULT_VERTEX_CAPACITY = 1u << 20;  // 1,048,576 vertices
    static constexpr uint32_t DEFAULT_INDEX_CAPACITY  = 1u << 22;  // 4,194,304 indices

    void init(VulkanContext* ctx, ResourceFactory* resources, DeletionQueue* deletionQueue,
              uint32_t vertexCapacity = DEFAULT_VERTEX_CAPACITY,
              uint32_t indexCapacity = DEFAULT_INDEX_CAPACITY);
    void shutdown();

    // Upload one mesh's vertices+indices into the megabuffers via staging.
    // Returns an invalid handle (indexCount==0) if capacity is exhausted.
    MeshHandle alloc(const std::vector<Vertex>& vertices,
                     const std::vector<uint32_t>& indices);

    // Retire a handle. The freed ranges return to the free list THROUGH the
    // DeletionQueue, so they are only reusable after in-flight frames complete.
    void free(const MeshHandle& handle);

    // Bind the shared vertex+index buffers once for the whole scene.
    void bind(VkCommandBuffer cmd) const;

    VkBuffer vertexBuffer() const { return vertexBuf_.buffer(); }
    VkBuffer indexBuffer() const { return indexBuf_.buffer(); }
    VkDeviceAddress vertexAddress() const { return vertexBuf_.deviceAddress(); }

   private:
    // A free span in element units (vertices or indices).
    struct Span {
        uint32_t offset = 0;
        uint32_t count = 0;
    };

    // First-fit allocate `count` elements from a free list; returns offset or
    // UINT32_MAX if no span is large enough. Splits the span it takes from.
    static uint32_t allocSpan(std::vector<Span>& freeList, uint32_t count);
    // Return a span to the free list and coalesce with neighbours.
    static void freeSpan(std::vector<Span>& freeList, uint32_t offset, uint32_t count);

    VulkanContext* ctx_ = nullptr;
    ResourceFactory* resources_ = nullptr;
    DeletionQueue* deletionQueue_ = nullptr;

    VmaBuffer vertexBuf_;  // device-local, VERTEX|TRANSFER_DST|SHADER_DEVICE_ADDRESS
    VmaBuffer indexBuf_;   // device-local, INDEX|TRANSFER_DST

    uint32_t vertexCapacity_ = 0;  // in vertices
    uint32_t indexCapacity_ = 0;   // in indices

    std::vector<Span> vertexFree_;  // free vertex spans (element units)
    std::vector<Span> indexFree_;   // free index spans (element units)
};